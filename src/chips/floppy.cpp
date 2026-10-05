#include "chips/floppy.h"
#include "core/log.h"
#include "core/util.h"

#include <stdio.h>

// name, size, tracks, heads, sectors, n, mfm, kbps, rpm, byte_us, track_len, first_sector, sector_len,
// id_mark, data_mark (MFM: sector = 60 + data + CRC 2 + gap 3)
static const FloppyGeom k_8sssd = { "8\" SS SD", 256256, 77, 1, 26, 0, false, 250, 360, 32, 5208, 73, 188, 6, 30 };
static const FloppyGeom k_35hd = { "3.5\" DS HD", 1474560, 80, 2, 18, 2, true, 500, 300, 16, 12500, 146,
                                   60 + 512 + 2 + 108, 15, 59 };
static const FloppyGeom k_525hd = { "5.25\" DS HD", 1228800, 80, 2, 15, 2, true, 500, 360, 16, 10416, 146,
                                    60 + 512 + 2 + 84, 15, 59 };
static const FloppyGeom k_dd80 = { "3.5\"/5.25\" DS DD 80 tr.", 737280, 80, 2, 9, 2, true, 250, 300, 32, 6250, 146,
                                   60 + 512 + 2 + 80, 15, 59 };
static const FloppyGeom k_dd40 = { "5.25\" DS DD 40 tr.", 368640, 40, 2, 9, 2, true, 250, 300, 32, 6250, 146,
                                   60 + 512 + 2 + 80, 15, 59 };
static const FloppyGeom* const k_geoms[] = { &k_8sssd, &k_35hd, &k_525hd, &k_dd80, &k_dd40, nullptr };

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

FloppyDrive::FloppyDrive() : geom(&k_8sssd) {}

bool FloppyDrive::load(const std::string& img, bool work_copy, std::string& err)
{
    std::string p;
    if (!media_path(img, work_copy, p, err)) return false;
    std::vector<uint8_t> d;
    if (!file_read_all(p, d)) {
        err = "cannot read " + p;
        return false;
    }
    const FloppyGeom* g = floppy_geom_for_size(d.size());
    if (!g) {
        char b[256];
        snprintf(b, sizeof(b), "%s: %u bytes; a disk image has 256256 (8\" SS SD), 1474560 (3.5\" HD), "
                 "1228800 (5.25\" HD), 737280 or 368640 (DD)", img.c_str(), (unsigned)d.size());
        err = b;
        return false;
    }
    image = img;
    path = p;
    data.swap(d);
    geom = g;
    dirty = false;
    return true;
}

bool FloppyDrive::save(std::string& err)
{
    if (!dirty || data.empty()) return true;
    if (!file_write_all(path, data.data(), data.size())) {
        err = "cannot write " + path;
        return false;
    }
    dirty = false;
    return true;
}

void FloppyDrive::unload(const char* who)
{
    std::string err;
    if (!save(err)) LOGE("%s: %s", who, err.c_str());
    data.clear();
    path.clear();
    geom = &k_8sssd;
}

uint8_t* FloppyDrive::sector(int trk, int head, int r)
{
    const FloppyGeom& g = *geom;
    if (data.empty() || trk < 0 || trk >= g.tracks || head < 0 || head >= g.heads || r < 1 || r > g.sectors)
        return nullptr;
    return &data[((size_t)(trk * g.heads + head) * g.sectors + (size_t)(r - 1)) * (size_t)g.sector_size()];
}
