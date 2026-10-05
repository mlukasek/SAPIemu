#include "chips/floppy.h"
#include "core/log.h"
#include "core/util.h"

#include <stdio.h>

uint16_t floppy_crc(uint16_t crc, uint8_t b)
{
    crc ^= (uint16_t)(b << 8);
    for (int i = 0; i < 8; i++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    return crc;
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
    if (d.size() != (size_t)IMAGE_SIZE) {
        char b[160];
        snprintf(b, sizeof(b), "%s: %u bytes, an 8\" SS SD image has %d (77 x 26 x 128)", img.c_str(),
                 (unsigned)d.size(), IMAGE_SIZE);
        err = b;
        return false;
    }
    image = img;
    path = p;
    data.swap(d);
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
}

uint8_t* FloppyDrive::sector(int trk, int r)
{
    if (data.empty() || trk < 0 || trk >= TRACKS || r < 1 || r > SECTORS) return nullptr;
    return &data[(size_t)(trk * SECTORS + r - 1) * SECTOR_SIZE];
}
