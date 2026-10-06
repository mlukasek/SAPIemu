#include "cards/rpd1.h"
#include "core/state.h"
#include "core/bus.h"
#include "core/log.h"
#include "core/util.h"

#include <stdio.h>
#include <string.h>

using nlohmann::json;

// control register (25H write)
static const uint8_t C_RE = 0x01, C_WE = 0x02, C_RDM = 0x04, C_RAM = 0x08;

static uint8_t reverse8(uint8_t v)
{
    v = (uint8_t)((v & 0xF0) >> 4 | (v & 0x0F) << 4);
    v = (uint8_t)((v & 0xCC) >> 2 | (v & 0x33) << 2);
    return (uint8_t)((v & 0xAA) >> 1 | (v & 0x55) << 1);
}

// CRC-CCITT as on IBM 3740 disks (and in the EBIOS)
static uint16_t crc_update(uint16_t crc, uint8_t b)
{
    return floppy_crc(crc, b);
}

Rpd1::~Rpd1()
{
    flush();
}

bool Rpd1::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_bool(cfg, "working_copy", work_copy, true, err)) return false;
    if (!cfg_number(cfg, "step_ms", v, 8, err)) return false;
    if (v < 1 || v > 50) { err = "step_ms must be 1..50"; return false; }
    step_ms = (int)v;
    if (!cfg_number(cfg, "settle_ms", v, 35, err)) return false;
    if (v < 0 || v > 500) { err = "settle_ms must be 0..500"; return false; }
    settle_ms = (int)v;
    static const char* const keys[DRIVES] = { "drive_a", "drive_b" };
    for (int i = 0; i < DRIVES; i++) {
        std::string image;
        bool wp = false;
        if (cfg.contains(keys[i]) && cfg[keys[i]].is_object()) {
            const json& d = cfg[keys[i]];
            if (!cfg_string(d, "image", image, "", err)) return false;
            if (!cfg_bool(d, "write_protect", wp, false, err)) return false;
            if (!cfg_letter(d, drives[i].cpm_cfg, err)) return false;
        }
        drives[i].image = image;
        drives[i].write_protect = wp;
        if (!image.empty() && !probe && !insert(i, image, wp, err)) return false;
    }
    return true;
}

bool Rpd1::insert(int i, const std::string& image, bool wp, std::string& err)
{
    if (i < 0 || i >= DRIVES) { err = "no such drive"; return false; }
    eject(i);
    Drive& d = drives[i];
    if (!d.load(image, work_copy, err)) return false;
    if (!d.is_8sssd()) {   // the RPD-1 data separator and its PROM know FM 8" disks only
        err = image + ": RPD-1 reads 8\" SS SD disks only (this one is " + d.format + ")";
        d.unload(name.c_str());
        return false;
    }
    d.write_protect = wp;
    raw_valid = false;
    LOGI("%s: drive %c: %s%s", name.c_str(), 'A' + i, d.path.c_str(), wp ? " (write protected)" : "");
    return true;
}

void Rpd1::eject(int i)
{
    if (i < 0 || i >= DRIVES) return;
    drives[i].unload(name.c_str());
    raw_valid = false;
}

void Rpd1::flush()
{
    for (int i = 0; i < DRIVES; i++) {
        std::string err;
        if (!drives[i].save(err)) LOGE("%s: %s", name.c_str(), err.c_str());
    }
}

void Rpd1::reset()
{
    for (Drive& d : drives) d.cpm_seen = 0;   // another system may come
    ctrl = 0;
    drive_ctrl = 0;
    sel = 0;
    head_loaded = false;
    wf = false;
    for (Counter& c : cnt) c = Counter();
    cnt0_armed = cnt1_armed = false;
    ovfl = false;
    tout_dam = false;
    streaming = search_dam = false;
    writing = false;
    transfers = 0;
    raw_valid = false;
}

uint64_t Rpd1::byte_cycles(void) const
{
    uint64_t c = ((uint64_t)bus->cpu_hz * 32 + 500000) / 1000000;   // 32 us per byte (8" FM)
    return c ? c : 1;
}

uint64_t Rpd1::byte_now(void) const
{
    return bus->now / byte_cycles();
}

// The track under the head of the selected drive, IBM 3740 layout (gaps 40/26,
// 6 zeros before every mark, gap 2 = 11, gap 3 = 27 bytes).
void Rpd1::build_track(void)
{
    Drive& d = cur();
    if (raw_valid && raw_drive == sel && raw_track == d.track) return;
    memset(raw, 0xFF, sizeof(raw));
    memset(mark, M_NONE, sizeof(mark));
    memset(sector_at, 0xFF, sizeof(sector_at));
    raw_valid = true;
    raw_drive = sel;
    raw_track = d.track;
    if (d.data.empty() || d.track >= TRACKS) return;   // no disk: no marks at all
    int p = 40;
    for (int i = 0; i < 6; i++) raw[p++] = 0x00;
    raw[p++] = 0xFC;   // index address mark (not used by the software)
    p += 26;
    for (int s = 0; s < SECTORS; s++) {
        for (int i = 0; i < 6; i++) raw[p++] = 0x00;
        uint8_t id[5] = { 0xFE, (uint8_t)d.track, 0, (uint8_t)(s + 1), 0 };
        uint16_t crc = 0xFFFF;
        mark[p] = M_ID;
        sector_at[p] = (uint8_t)s;
        for (uint8_t b : id) {
            raw[p++] = b;
            crc = crc_update(crc, b);
        }
        raw[p++] = (uint8_t)(crc >> 8);
        raw[p++] = (uint8_t)crc;
        p += 11;
        for (int i = 0; i < 6; i++) raw[p++] = 0x00;
        mark[p] = M_DATA;
        raw[p++] = 0xFB;
        crc = crc_update(0xFFFF, 0xFB);
        const uint8_t* sec = &d.data[(size_t)(d.track * SECTORS + s) * SECTOR_SIZE];
        for (int i = 0; i < SECTOR_SIZE; i++) {
            raw[p++] = sec[i];
            crc = crc_update(crc, sec[i]);
        }
        raw[p++] = (uint8_t)(crc >> 8);
        raw[p++] = (uint8_t)crc;
        p += 27;
    }
}

bool Rpd1::tout(void)
{
    uint64_t now = bus->now;
    if ((ctrl & (C_RE | C_WE)) && cnt0_armed && now >= cnt0_expire) return true;
    return tout_dam && cnt1_armed && now >= cnt1_fire;
}

void Rpd1::control(uint8_t v)
{
    uint8_t old = ctrl;
    write_update();
    if ((old & C_WE) && !(v & C_WE) && writing) {
        write_commit();
        writing = false;
    }
    ctrl = v;
    if (!(v & C_RE)) {
        streaming = search_dam = false;
        tout_dam = false;
    }
    if ((v & C_RE) && !(old & C_RE) && !(v & (C_RAM | C_RDM))) {   // plain read of the stream
        streaming = true;
        stream_b = byte_now() + 1;
        consumed_b = stream_b - 1;
    }
    if ((v & C_RE) && (v & C_RDM)) cur().seen_as(bus->cpm_drive);   // a sector read: its CP/M drive
    if ((v & C_RE) && (v & (C_RAM | C_RDM))) {
        build_track();
        uint64_t b0 = byte_now() + 1;
        int want = (v & C_RAM) ? M_ID : M_DATA;
        streaming = false;
        tout_dam = false;
        for (int k = 0; k < TRACK_LEN; k++) {
            uint64_t b = b0 + (uint64_t)k;
            int pos = (int)(b % TRACK_LEN);
            if (mark[pos] != want) continue;
            uint64_t t = b * byte_cycles();
            if (want == M_ID) {   // SYC starts counter 1 (mode 5, 0.5 us clock)
                sync_sector = sector_at[pos];
                LOGD("%s: track %d ID of sector %d in %u bytes", name.c_str(), cur().track, sync_sector + 1,
                     (unsigned)(b - b0));
                uint64_t c1 = ((uint64_t)bus->cpu_hz + 1000000) / 2000000;
                cnt1_fire = t + ((uint64_t)(n1 ? n1 : 65536) + 1) * (c1 ? c1 : 1);
                cnt1_armed = true;
            } else if (cnt1_armed && cnt1_fire < t) {   // the data mark comes too late
                tout_dam = true;
                break;
            }
            streaming = true;
            stream_b = b;
            consumed_b = b - 1;
            break;
        }
    }
    if ((v & C_WE) && !(old & C_WE)) {
        writing = true;
        holding_full = false;
        shift_started = false;
        written.clear();
        shifted_b = byte_now();
        write_sector = sync_sector;
        write_drive = sel;
        write_track = cur().track;
        if (cur().write_protect || cur().data.empty()) wf = true;   // the drive refuses to write
    }
}

// Shift the holding register to the disk at every byte boundary once the
// write gate (counter 1 after the ID mark) is open; an empty register there
// is data late (WF).
void Rpd1::write_update(void)
{
    if (!writing || !cnt1_armed) return;
    uint64_t bc = byte_cycles();
    uint64_t latest = byte_now();
    uint64_t first = (cnt1_fire + bc - 1) / bc;
    if (shifted_b + 1 > first) first = shifted_b + 1;
    for (uint64_t b = first; b <= latest; b++) {
        if (holding_full) {
            written.push_back(holding);
            holding_full = false;
            shift_started = true;
        } else if (shift_started) {
            wf = true;   // data late
            break;
        }
        if (written.size() > 4096) break;
    }
    if (latest > shifted_b) shifted_b = latest;
}

// The written stream (zeros, data mark, 128 bytes, CRC, FF) replaces the data
// field of the sector whose ID was found last.
void Rpd1::write_commit(void)
{
    size_t i = 0;
    while (i < written.size() && written[i] == 0x00) i++;
    if (i >= written.size()) return;
    uint8_t m = written[i];
    if ((m != 0xFB && m != 0xF8) || written.size() < i + 1 + SECTOR_SIZE) {
        LOGW("%s: write of %u bytes without a data field", name.c_str(), (unsigned)written.size());
        return;
    }
    Drive& d = drives[write_drive];
    if (d.data.empty() || d.write_protect || write_sector < 0 || write_track >= TRACKS) return;
    uint16_t crc = crc_update(0xFFFF, m);
    for (int k = 0; k < SECTOR_SIZE; k++) crc = crc_update(crc, written[i + 1 + k]);
    if (written.size() >= i + 3 + SECTOR_SIZE &&
        (written[i + 1 + SECTOR_SIZE] != (uint8_t)(crc >> 8) || written[i + 2 + SECTOR_SIZE] != (uint8_t)crc))
        LOGW("%s: track %d sector %d written with a bad CRC", name.c_str(), write_track, write_sector + 1);
    memcpy(&d.data[(size_t)(write_track * SECTORS + write_sector) * SECTOR_SIZE], &written[i + 1], SECTOR_SIZE);
    d.dirty = true;
    raw_valid = false;
    written.clear();
}

void Rpd1::transfer(void)
{
    transfers++;
    if (transfers < (unsigned)(n2 ? n2 : 65536) + 1u) return;
    ovfl = true;   // OVFL clears the control bits D3-D0, the table pointer stays
    write_update();
    if (writing) {
        write_commit();
        writing = false;
    }
    ctrl &= 0xE0;
    streaming = search_dam = false;
    tout_dam = false;
}

void Rpd1::counter_loaded(int n)
{
    uint16_t v = cnt[n].value;
    if (n == 0) {
        n0 = v;
        cnt0_armed = true;
        cnt0_expire = bus->now + ((uint64_t)(v ? v : 65536) + 1) * byte_cycles();
    } else if (n == 1) {
        n1 = v;
        cnt1_armed = false;
        tout_dam = false;
    } else {
        n2 = v;
        transfers = 0;
        ovfl = false;
    }
}

bool Rpd1::io_read(uint8_t port, uint8_t& data)
{
    if (port >= 0x20 && port <= 0x23) {
        data = 0xFF;   // the driver never reads the counters back
        return true;
    }
    if (port == 0x24) {
        data = 0xFF;
        if (writing) return true;
        if (streaming) {
            uint64_t latest = byte_now();
            if (latest >= stream_b) {
                build_track();
                data = raw[latest % TRACK_LEN];
                consumed_b = latest;
            }
        }
        if (ctrl & C_RE) transfer();
        return true;
    }
    if (port == 0x25) {
        int drn = 1;
        if (writing) {
            write_update();
            drn = holding_full ? 1 : 0;
        } else if (streaming) {
            uint64_t latest = byte_now();
            drn = (latest >= stream_b && latest > consumed_b) ? 0 : 1;
        }
        data = (uint8_t)((ctrl & 0xE0) | (ovfl ? 0x10 : 0) | (tout() ? 0x08 : 0) | (drn << 2));
        return true;
    }
    if (port == 0x3C) {
        Drive& d = cur();
        uint64_t now = bus->now;
        bool rdy = !d.data.empty();
        uint8_t v = 0;
        if (head_loaded && now >= head_ready_at && now < head_unload_at) v |= 0x80;          // HLR (timer)
        if (now >= step_ready_at) v |= 0x40;                                                  // STR
        if (d.track == 0) v |= 0x20;                                                          // T00
        if (rdy && byte_now() % TRACK_LEN < 16) v |= 0x10;                                    // INDX
        if (rdy && d.write_protect) v |= 0x08;                                                // WP
        if (rdy) v |= 0x04;                                                                   // RDY
        if (wf) v |= 0x02;                                                                    // WF
        data = v;
        return true;
    }
    if (port >= 0x3D && port <= 0x3F) {
        data = 0xFF;
        return true;
    }
    return false;
}

bool Rpd1::io_write(uint8_t port, uint8_t data)
{
    uint64_t now = bus->now;
    if (port == 0x23) {
        uint8_t cw = reverse8(data);
        int sc = cw >> 6;
        if (sc == 3) return true;
        cnt[sc].mode = (cw >> 1) & 7;
        cnt[sc].rl = (cw >> 4) & 3;
        cnt[sc].msb_next = false;
        return true;
    }
    if (port >= 0x20 && port <= 0x22) {
        Counter& c = cnt[port - 0x20];
        uint8_t v = reverse8(data);
        if (c.rl == 1) {
            c.value = v;
        } else if (c.rl == 2) {
            c.value = (uint16_t)(v << 8);
        } else if (!c.msb_next) {
            c.lsb = v;
            c.msb_next = true;
            return true;
        } else {
            c.value = (uint16_t)(c.lsb | v << 8);
            c.msb_next = false;
        }
        counter_loaded(port - 0x20);
        return true;
    }
    if (port == 0x24) {
        if (writing) {
            write_update();
            holding = data;
            holding_full = true;
            transfer();
        }
        return true;
    }
    if (port == 0x25) {
        control(data);
        return true;
    }
    if (port == 0x3C) {
        int s = data & 1;
        if (s != sel) {
            sel = s;
            head_loaded = false;
            raw_valid = false;
        }
        if (data & 0x02) wf = false;   // FR
        drive_ctrl = data;
        return true;
    }
    if (port == 0x3D) {   // head load; unloads after 8 revolutions without another 3DH
        uint64_t rev = (uint64_t)TRACK_LEN * byte_cycles();
        if (!head_loaded || now >= head_unload_at) {
            head_loaded = true;
            head_ready_at = now + (uint64_t)bus->cpu_hz * (uint64_t)settle_ms / 1000;
        }
        head_unload_at = now + 8 * rev;
        return true;
    }
    if (port == 0x3E) {   // one step, DIR = 3CH D2 (1 = to higher tracks)
        Drive& d = cur();
        if (drive_ctrl & 0x04) {
            if (d.track < FloppyDrive::LAST_TRACK) d.track++;
        } else if (d.track > 0) {
            d.track--;
        }
        step_ready_at = now + (uint64_t)bus->cpu_hz * (uint64_t)step_ms / 1000;
        LOGD("%s: step %s to track %d", name.c_str(), (drive_ctrl & 0x04) ? "in" : "out", d.track);
        raw_valid = false;
        return true;
    }
    if (port == 0x3F) return true;   // test interrupt (not used by the software)
    return false;
}

int Rpd1::decode_io(uint8_t port, const char** what)
{
    if (port >= 0x20 && port <= 0x23) { *what = "RPD-1A 8253 timer"; return DEC_READ | DEC_WRITE; }
    if (port == 0x24) { *what = "RPD-1A data"; return DEC_READ | DEC_WRITE; }
    if (port == 0x25) { *what = "RPD-1A control/status"; return DEC_READ | DEC_WRITE; }
    if (port == 0x3C) { *what = "RPD-1B drive control/status"; return DEC_READ | DEC_WRITE; }
    if (port == 0x3D) { *what = "RPD-1B head load"; return DEC_WRITE; }
    if (port == 0x3E) { *what = "RPD-1B step"; return DEC_WRITE; }
    if (port == 0x3F) { *what = "RPD-1B test interrupt"; return DEC_WRITE; }
    return 0;
}

void Rpd1::describe(json& out)
{
    json drv = json::array();
    for (int i = 0; i < DRIVES; i++) {
        const Drive& d = drives[i];
        drv.push_back({ { "unit", i }, { "image", d.image }, { "path", d.path },
                        { "inserted", !d.data.empty() }, { "write_protect", d.write_protect },
                        { "modified", d.dirty }, { "track", d.track },
                        { "cpm", d.cpm_cfg ? std::string(1, d.cpm_cfg) : d.cpm_seen ? std::string(1, d.cpm_seen) : std::string() },
                        { "cpm_seen", d.cpm_seen ? std::string(1, d.cpm_seen) : std::string() } });
    }
    out["drives"] = drv;
    out["selected"] = sel;
}

void Rpd1::state(StateIO& io)
{
    // the floppies are referred to (file, protection, head), not stored
    for (int d = 0; d < DRIVES; d++) {
        Drive& dr = drives[d];
        std::string img = dr.image;
        io.media("floppy", dr.path);
        io.v(img);
        bool wp = dr.write_protect;
        io.v(wp);
        io.e(dr.track);
        if (io.loading && io.ok() && img != dr.image) {
            std::string err;
            if (img.empty()) eject(d);
            else if (!insert(d, img, wp, err)) LOGW("state: floppy %s: %s", img.c_str(), err.c_str());
        }
        if (io.loading) dr.write_protect = wp;
    }
    io.e(sel);
    io.v(drive_ctrl);
    io.v(head_loaded);
    io.v(head_ready_at);
    io.v(head_unload_at);
    io.v(step_ready_at);
    io.v(wf);
    io.arr(raw, sizeof(raw));
    io.arr(mark, sizeof(mark));
    io.arr(sector_at, sizeof(sector_at));
    io.e(raw_drive);
    io.e(raw_track);
    io.v(raw_valid);
    io.v(ctrl);
    for (Counter& k : cnt) {
        io.e(k.mode);
        io.e(k.rl);
        io.v(k.value);
        io.v(k.msb_next);
        io.v(k.lsb);
    }
    io.v(n0);
    io.v(n1);
    io.v(n2);
    io.v(cnt0_armed);
    io.v(cnt0_expire);
    io.v(cnt1_armed);
    io.v(cnt1_fire);
    io.v(transfers);
    io.v(ovfl);
    io.v(tout_dam);
    io.v(streaming);
    io.v(stream_b);
    io.v(consumed_b);
    io.v(search_dam);
    io.e(sync_sector);
    io.v(writing);
    io.v(holding_full);
    io.v(holding);
    io.v(shifted_b);
    io.v(shift_started);
    io.v(written);
    io.e(write_sector);
    io.e(write_drive);
    io.e(write_track);
}
