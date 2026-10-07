#include "chips/floppy.h"
#include "core/log.h"
#include "core/util.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// name, size, tracks, heads, sectors, n, mfm, kbps, rpm
static const FloppyGeom k_8sssd = { "8\" SS SD", 256256, 77, 1, 26, 0, false, 250, 360 };
static const FloppyGeom k_35hd = { "3.5\" DS HD", 1474560, 80, 2, 18, 2, true, 500, 300 };
static const FloppyGeom k_525hd = { "5.25\" DS HD", 1228800, 80, 2, 15, 2, true, 500, 360 };
static const FloppyGeom k_dd80 = { "3.5\"/5.25\" DS DD 80 tr.", 737280, 80, 2, 9, 2, true, 250, 300 };
static const FloppyGeom k_dd40 = { "5.25\" DS DD 40 tr.", 368640, 40, 2, 9, 2, true, 250, 300 };
// SAPI-1.5 (MFC-1A, CBIOS V050690): 5.25" DS DD, 16 sectors of 256 B, R = 1..16 on both sides
static const FloppyGeom k_sp2 = { "5.25\" DS DD SAPI-1.5 (16x256)", 327680, 40, 2, 16, 1, true, 250, 300 };
static const FloppyGeom* const k_geoms[] = { &k_8sssd, &k_35hd, &k_525hd, &k_dd80, &k_dd40, &k_sp2, nullptr };

const FloppyGeom* const* floppy_geoms(void)
{
    return k_geoms;
}

const FloppyGeom* floppy_geom_for_size(size_t size)
{
    for (int i = 0; k_geoms[i]; i++)
        if ((size_t)k_geoms[i]->size == size) return k_geoms[i];
    return nullptr;
}

uint16_t floppy_crc(uint16_t crc, uint8_t b)
{
    crc ^= (uint16_t)(b << 8);
    for (int i = 0; i < 8; i++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    return crc;
}

// ImageDisk mode byte <-> recording: 0 FM 500, 1 FM 300, 2 FM 250, 3 MFM 500, 4 MFM 300, 5 MFM 250
// (the PC controller's rate; 300 is a 300 rpm disk in a 360 rpm drive). Here: the disk's own rate.
static void imd_mode_rate(int mode, bool& mfm, int& kbps)
{
    mfm = mode >= 3;
    int r = mode % 3;   // 0 = 500, 1 = 300, 2 = 250
    kbps = r == 0 ? (mfm ? 500 : 250) : (mfm ? 250 : 125);
}

static uint8_t imd_mode_of(bool mfm, int kbps)
{
    if (mfm) return kbps >= 500 ? 3 : 5;
    return kbps >= 250 ? 0 : 2;
}

static int size_code(int size)
{
    int n = 0;
    while ((128 << n) < size && n < 7) n++;
    return n;
}

FloppyDrive::FloppyDrive() : geom(nullptr) {}

bool FloppyDrive::is_8sssd(void) const
{
    return present() && geom == &k_8sssd;
}

void FloppyDrive::layout(FloppyTrack& t) const
{
    static const int fm_gap[8] = { 27, 42, 58, 138, 200, 200, 200, 200 };
    static const int mfm_gap[8] = { 27, 54, 108, 116, 200, 200, 200, 200 };
    t.byte_us = 8000 / (t.kbps > 0 ? t.kbps : 250);
    if (t.byte_us < 1) t.byte_us = 1;
    t.track_len = 60000000 / (rpm > 0 ? rpm : 360) / t.byte_us;
    int n = size_code(t.size);
    int over;
    if (t.mfm) {
        t.first_sector = 146;
        t.id_mark = 15;
        t.data_mark = 59;
        over = 60 + t.size + 2;
    } else {
        t.first_sector = 73;
        t.id_mark = 6;
        t.data_mark = 30;
        over = 31 + t.size + 2;
    }
    int ns = (int)t.sec.size();
    int gap = t.mfm ? mfm_gap[n] : fm_gap[n];
    if (ns > 0) {
        int avail = (t.track_len - t.first_sector) / ns - over;
        if (avail < gap) gap = avail < 1 ? 1 : avail;
    }
    t.sector_len = over + gap;
}

void FloppyDrive::clear(void)
{
    data.clear();
    tracks.clear();
    geom = nullptr;
    format.clear();
    imd = false;
    imd_header.clear();
    cylinders = 0;
    heads = 1;
    rpm = 360;
}

bool FloppyDrive::load(const std::string& img, bool work_copy, std::string& err)
{
    std::string p;
    if (!media_path(img, work_copy, p, err)) return false;
    std::vector<uint8_t> d;
    if (!file_read_all(p, d)) {
        err = "cannot read " + p;
        return false;
    }
    clear();
    bool ok = (d.size() >= 4 && memcmp(d.data(), "IMD ", 4) == 0) ? load_imd(d, err) : load_plain(d, err);
    if (!ok) {
        err = img + ": " + err;
        clear();
        return false;
    }
    image = img;
    path = p;
    dirty = false;
    return true;
}

bool FloppyDrive::load_plain(std::vector<uint8_t>& d, std::string& err)
{
    const FloppyGeom* g = floppy_geom_for_size(d.size());
    if (!g) {
        char b[256];
        snprintf(b, sizeof(b), "%u bytes; a disk image is ImageDisk (.imd) or has 256256 (8\" SS SD), 1474560 "
                 "(3.5\" HD), 1228800 (5.25\" HD), 737280 or 368640 (DD), 327680 (SAPI-1.5 DD)", (unsigned)d.size());
        err = b;
        return false;
    }
    data.swap(d);
    geom = g;
    format = g->name;
    cylinders = g->tracks;
    heads = g->heads;
    rpm = g->rpm;
    tracks.assign((size_t)cylinders * 2, FloppyTrack());
    int size = g->sector_size();
    for (int c = 0; c < cylinders; c++)
        for (int h = 0; h < heads; h++) {
            FloppyTrack& t = tracks[(size_t)c * 2 + h];
            t.formatted = true;
            t.mfm = g->mfm;
            t.kbps = g->kbps;
            t.size = size;
            t.imd_mode = imd_mode_of(g->mfm, g->kbps);
            for (int r = 1; r <= g->sectors; r++) {
                FloppySector s;
                s.c = (uint8_t)c, s.h = (uint8_t)h, s.r = (uint8_t)r, s.n = (uint8_t)g->n;
                s.off = (uint32_t)((((size_t)c * heads + h) * g->sectors + (size_t)(r - 1)) * (size_t)size);
                t.sec.push_back(s);
            }
            layout(t);
        }
    return true;
}

namespace {
struct ImdSector {
    uint8_t c, h, r, flags;
    bool compressed;
    uint8_t fill;
    size_t pos;   // data in the file (not compressed)
};
struct ImdTrack {
    int mode, cyl, head, size;
    std::vector<ImdSector> sec;
};
}

bool FloppyDrive::load_imd(const std::vector<uint8_t>& d, std::string& err)
{
    size_t i = 0;
    while (i < d.size() && d[i] != 0x1A) i++;
    if (i >= d.size()) {
        err = "ImageDisk without the end of its header (1Ah)";
        return false;
    }
    imd_header.assign((const char*)d.data(), i);
    i++;
    std::vector<ImdTrack> trk;
    int max_cyl = -1, max_head = 0;
    while (i < d.size()) {
        if (i + 5 > d.size()) {
            err = "ImageDisk: a truncated track header";
            return false;
        }
        ImdTrack t;
        t.mode = d[i];
        t.cyl = d[i + 1];
        int hb = d[i + 2];
        t.head = hb & 0x3F;
        int ns = d[i + 3];
        int sc = d[i + 4];
        i += 5;
        if (t.mode > 5 || t.head > 1) {
            err = "ImageDisk: unknown mode or head";
            return false;
        }
        if (sc == 0xFF) {
            err = "ImageDisk: per-sector sizes are not supported";
            return false;
        }
        if (sc > 6) {
            err = "ImageDisk: sector size code above 6";
            return false;
        }
        t.size = 128 << sc;
        size_t need = (size_t)ns * (1 + ((hb & 0x80) ? 1 : 0) + ((hb & 0x40) ? 1 : 0));
        if (i + need > d.size()) {
            err = "ImageDisk: truncated sector maps";
            return false;
        }
        const uint8_t* rmap = &d[i];
        i += ns;
        const uint8_t* cmap = nullptr;
        const uint8_t* hmap = nullptr;
        if (hb & 0x80) cmap = &d[i], i += ns;
        if (hb & 0x40) hmap = &d[i], i += ns;
        for (int k = 0; k < ns; k++) {
            if (i >= d.size()) {
                err = "ImageDisk: truncated sector data";
                return false;
            }
            int type = d[i++];
            ImdSector s;
            s.r = rmap[k];
            s.c = cmap ? cmap[k] : (uint8_t)t.cyl;
            s.h = hmap ? hmap[k] : (uint8_t)t.head;
            s.flags = 0;
            s.compressed = false;
            s.fill = 0;
            s.pos = 0;
            if (type > 8) {
                err = "ImageDisk: unknown sector record type";
                return false;
            }
            if (type == 0) {
                s.flags = FS_NO_DATA;
            } else {
                int t1 = type - 1;   // bit 0 compressed, bit 1 deleted, bit 2 data error
                if (t1 & 2) s.flags |= FS_DELETED;
                if (t1 & 4) s.flags |= FS_DATA_CRC;
                if (t1 & 1) {
                    if (i >= d.size()) {
                        err = "ImageDisk: truncated sector data";
                        return false;
                    }
                    s.compressed = true;
                    s.fill = d[i++];
                } else {
                    if (i + (size_t)t.size > d.size()) {
                        err = "ImageDisk: truncated sector data";
                        return false;
                    }
                    s.pos = i;
                    i += (size_t)t.size;
                }
            }
            t.sec.push_back(s);
        }
        if (t.cyl > max_cyl) max_cyl = t.cyl;
        if (t.head > max_head) max_head = t.head;
        trk.push_back(t);
    }
    if (max_cyl < 0) {
        err = "ImageDisk without tracks";
        return false;
    }
    imd = true;
    cylinders = max_cyl + 1;
    heads = max_head + 1;

    // rpm: 360 for disks written at 500 kbit/s (8", 5.25" HD) unless a track needs 300 rpm (3.5" HD)
    rpm = 300;
    for (const ImdTrack& t : trk)
        if (t.mode % 3 == 0 && !t.sec.empty()) rpm = 360;
    if (rpm == 360)
        for (const ImdTrack& t : trk)
            if (t.mode == 3 && 146 + (int)t.sec.size() * (62 + t.size) > 10416) rpm = 300;

    // an 8" SS SD disk keeps its data in logical order (as a plain image), for RPD-1
    bool std8 = cylinders == 77 && heads == 1 && (int)trk.size() == 77;
    for (const ImdTrack& t : trk) {
        if (!std8) break;
        if (t.mode != 0 || t.size != 128 || t.sec.size() != 26 || t.head != 0) std8 = false;
        uint32_t seen = 0;
        for (const ImdSector& s : t.sec) {
            if (s.c != t.cyl || s.h != 0 || s.r < 1 || s.r > 26 || (seen & (1u << s.r))) std8 = false;
            else seen |= 1u << s.r;
        }
    }

    tracks.assign((size_t)cylinders * 2, FloppyTrack());
    if (std8) data.assign((size_t)k_8sssd.size, 0xE5);
    int sizes = 0, mfm_t = 0, fm_t = 0, ns0 = 0, size0 = 0;
    for (const ImdTrack& it : trk) {
        FloppyTrack& t = tracks[(size_t)it.cyl * 2 + it.head];
        if (t.formatted) {
            LOGW("ImageDisk: track %d/%d twice, the later one is used", it.cyl, it.head);
            t.sec.clear();
        }
        t.formatted = true;
        t.imd_mode = (uint8_t)it.mode;
        imd_mode_rate(it.mode, t.mfm, t.kbps);
        t.size = it.size;
        int n = size_code(it.size);
        for (const ImdSector& s : it.sec) {
            FloppySector fs;
            fs.c = s.c, fs.h = s.h, fs.r = s.r, fs.n = (uint8_t)n, fs.flags = s.flags;
            if (std8) {
                fs.off = (uint32_t)(((size_t)it.cyl * 26 + (size_t)(s.r - 1)) * 128);
            } else {
                fs.off = (uint32_t)data.size();
                data.resize(data.size() + (size_t)it.size, 0);
            }
            uint8_t* p = &data[fs.off];
            if (s.flags & FS_NO_DATA) memset(p, 0, (size_t)it.size);
            else if (s.compressed) memset(p, s.fill, (size_t)it.size);
            else memcpy(p, &d[s.pos], (size_t)it.size);
            t.sec.push_back(fs);
        }
        layout(t);
        if (it.sec.empty()) continue;
        if (t.mfm) mfm_t++;
        else fm_t++;
        if (ns0 == 0 || (it.cyl == 1 && it.head == 0)) ns0 = (int)it.sec.size(), size0 = it.size;
        sizes++;
    }
    geom = std8 ? &k_8sssd : nullptr;
    char b[128];
    snprintf(b, sizeof(b), "ImageDisk %d x %d, %s, %d x %d B", cylinders, heads,
             mfm_t && fm_t ? "FM/MFM" : (mfm_t ? "MFM" : "FM"), ns0, size0);
    format = b;
    (void)sizes;
    return true;
}

bool FloppyDrive::save(std::string& err)
{
    if (!dirty || !present()) return true;
    if (imd) return save_imd(err);
    // tracks formatted beyond the image are not in the file
    if (!file_write_all(path, data.data(), geom ? (size_t)geom->size : data.size())) {
        err = "cannot write " + path;
        return false;
    }
    dirty = false;
    return true;
}

bool FloppyDrive::save_imd(std::string& err)
{
    std::vector<uint8_t> o(imd_header.begin(), imd_header.end());
    o.push_back(0x1A);
    for (int c = 0; c < cylinders; c++)
        for (int h = 0; h < heads; h++) {
            const FloppyTrack& t = tracks[(size_t)c * 2 + h];
            if (!t.formatted) continue;
            bool cmap = false, hmap = false;
            for (const FloppySector& s : t.sec) {
                if (s.c != c) cmap = true;
                if (s.h != h) hmap = true;
            }
            int ns = (int)t.sec.size();
            o.push_back(t.imd_mode);
            o.push_back((uint8_t)c);
            o.push_back((uint8_t)(h | (cmap ? 0x80 : 0) | (hmap ? 0x40 : 0)));
            o.push_back((uint8_t)ns);
            o.push_back((uint8_t)size_code(t.size));
            for (const FloppySector& s : t.sec) o.push_back(s.r);
            if (cmap)
                for (const FloppySector& s : t.sec) o.push_back(s.c);
            if (hmap)
                for (const FloppySector& s : t.sec) o.push_back(s.h);
            for (const FloppySector& s : t.sec) {
                if (s.flags & FS_NO_DATA) {
                    o.push_back(0);
                    continue;
                }
                const uint8_t* p = &data[s.off];
                bool same = true;
                for (int k = 1; k < t.size && same; k++) same = p[k] == p[0];
                int type = 1 + ((s.flags & FS_DELETED) ? 2 : 0) + ((s.flags & FS_DATA_CRC) ? 4 : 0) + (same ? 1 : 0);
                o.push_back((uint8_t)type);
                if (same) o.push_back(p[0]);
                else o.insert(o.end(), p, p + t.size);
            }
        }
    if (!file_write_all(path, o.data(), o.size())) {
        err = "cannot write " + path;
        return false;
    }
    dirty = false;
    return true;
}

bool FloppyDrive::create(const std::string& path, const FloppyGeom* g, std::string& err)
{
    std::vector<uint8_t> blank((size_t)g->size, 0xE5);
    size_t dot = path.find_last_of('.');
    std::string ext = dot == std::string::npos ? std::string() : path.substr(dot);
    for (char& c : ext) c = (char)tolower((unsigned char)c);
    if (ext != ".imd") {
        if (file_write_all(path, blank.data(), blank.size())) return true;
        err = "cannot write " + path;
        return false;
    }
    FloppyDrive d;
    if (!d.load_plain(blank, err)) return false;
    time_t now = time(nullptr);
    char h[128];
    strftime(h, sizeof(h), "IMD 1.18: %d/%m/%Y %H:%M:%S\r\n", localtime(&now));
    d.imd_header = std::string(h) + "SAPIemu: " + g->name + "\r\n";
    d.imd = true;
    d.path = path;
    d.dirty = true;
    return d.save(err);
}

void FloppyDrive::unload(const char* who)
{
    std::string err;
    if (!save(err)) LOGE("%s: %s", who, err.c_str());
    clear();
    path.clear();
}

FloppyTrack* FloppyDrive::track_at(int cyl, int head)
{
    if (cyl < 0 || cyl >= cylinders || head < 0 || head > 1) return nullptr;
    FloppyTrack& t = tracks[(size_t)cyl * 2 + head];
    return t.formatted ? &t : nullptr;
}

bool FloppyDrive::format_track(int cyl, int head, bool mfm, int kbps, int n, const std::vector<FloppySector>& ids,
                               uint8_t fill)
{
    int size = 128 << n;
    // a disk in a plain format (or a logical 8" ImageDisk) keeps its layout: the same format only
    if (geom && cyl < geom->tracks && head < geom->heads && mfm == geom->mfm && kbps == geom->kbps &&
        n == geom->n && (int)ids.size() == geom->sectors) {
        uint32_t seen = 0;
        bool ok = true;
        for (const FloppySector& s : ids) {
            if (s.r < 1 || s.r > geom->sectors || (seen & (1u << s.r))) ok = false;
            else seen |= 1u << s.r;
        }
        if (ok) {
            FloppyTrack& t = tracks[(size_t)cyl * 2 + head];
            for (size_t k = 0; k < ids.size(); k++) {
                uint32_t off =
                    (uint32_t)((((size_t)cyl * geom->heads + head) * geom->sectors + (size_t)(ids[k].r - 1)) * size);
                memset(&data[off], fill, (size_t)size);
                t.sec[k] = ids[k];
                t.sec[k].off = off;
                t.sec[k].flags = 0;
            }
            layout(t);
            dirty = true;
            return true;
        }
    }
    // beyond a plain image (a drive reaches a few tracks more): in memory only, the file keeps its size
    bool beyond = geom && cyl >= geom->tracks;
    if (!imd && !beyond) return false;
    // ImageDisk (or a track beyond the image): the track gets the new sectors
    if (cyl >= cylinders) {
        tracks.resize((size_t)(cyl + 1) * 2);
        cylinders = cyl + 1;
    }
    if (imd && head + 1 > heads) heads = head + 1;
    if (!imd) LOGI("%s: track %d head %d beyond the image, kept until the disk is taken out", path.c_str(), cyl, head);
    FloppyTrack& t = tracks[(size_t)cyl * 2 + head];
    t = FloppyTrack();
    t.formatted = true;
    t.mfm = mfm;
    t.kbps = kbps;
    t.size = size;
    t.imd_mode = imd_mode_of(mfm, kbps);
    for (const FloppySector& id : ids) {
        FloppySector s = id;
        s.flags = 0;
        s.off = (uint32_t)data.size();
        data.resize(data.size() + (size_t)size, fill);
        t.sec.push_back(s);
    }
    layout(t);
    if (!beyond) geom = nullptr;   // no longer an image in logical order
    dirty = true;
    return true;
}
