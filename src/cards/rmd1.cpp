#include "cards/rmd1.h"
#include "core/state.h"
#include "core/ihex.h"
#include "core/log.h"
#include "core/util.h"

#include <string.h>

using nlohmann::json;

Rmd1::~Rmd1()
{
    if (!trial) flush();
}

bool Rmd1::configure(const json& cfg, std::string& err)
{
    long long kb;
    if (!cfg_number(cfg, "size_kb", kb, 256, err)) return false;
    if (kb != 256 && kb != 1024) { err = "size_kb must be 256 (4164) or 1024 (41256)"; return false; }
    mask = kb == 256 ? 0x3FFFF : 0xFFFFF;
    if (!cfg_string(cfg, "prom", prom_file, "roms/rmd-1/RMD-1_74S287.bin", err)) return false;
    std::vector<uint8_t> p;
    if (!rom_load(prom_file, p, 256, 0xFF, 0, err)) return false;
    if (p.size() != 256) { err = prom_file + ": the decoder PROM (74S287) has 256 entries"; return false; }
    memcpy(prom, p.data(), 256);
    std::string fill;
    if (!cfg_string(cfg, "power_on", fill, "random", err)) return false;
    if (fill != "random" && fill != "e5") { err = "power_on must be random (like the DRAM) or e5 (empty CP/M disk)"; return false; }
    mem.assign((size_t)mask + 1, 0xE5);
    if (fill == "random") {   // what the DRAM holds after power on: the software has to initialise it
        uint32_t x = 0x12345678u;
        for (uint8_t& b : mem) {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            b = (uint8_t)x;
        }
    }
    if (!cfg_string(cfg, "image", image, "", err)) return false;
    if (!image.empty() && !probe) {   // emulator convenience: keep the content in a file
        image_path = data_path(image);
        std::vector<uint8_t> d;
        if (file_read_all(image_path, d)) {
            if (d.size() == mem.size()) mem.swap(d);
            else LOGW("%s: %s has %u bytes, expected %u; not used", name.c_str(), image.c_str(), (unsigned)d.size(),
                      (unsigned)mem.size());
        }
    }
    return true;
}

void Rmd1::flush()
{
    if (!unsaved()) return;
    if (!file_write_safe(image_path, mem.data(), mem.size())) LOGE("%s: cannot write %s", name.c_str(), image_path.c_str());
    else dirty = false;
}

bool Rmd1::unsaved()
{
    return dirty && !image_path.empty();
}

bool Rmd1::io_read(uint8_t port, uint8_t& data)
{
    if (prom[port] & 0x02) return false;   // only the data port drives the bus on a read
    data = latch;
    latch = mem[full()];
    addr++;
    return true;
}

bool Rmd1::io_write(uint8_t port, uint8_t data)
{
    uint8_t o = prom[port] & 0x0F;
    if (o == 0x0F) return false;
    if (!(o & 0x04)) row = data & 0x0F;
    if (!(o & 0x01)) addr = (uint16_t)(data << 8 | ((port & 0x04) ? 0x80 : 0x00));
    if (!(o & 0x08)) {
        latch = mem[full()];
        addr++;
    }
    if (!(o & 0x02)) {
        mem[full()] = data;
        addr++;
        dirty = true;
        media_written();
    }
    return true;
}

int Rmd1::decode_io(uint8_t port, const char** what)
{
    uint8_t o = prom[port] & 0x0F;
    if (!(o & 0x02)) { *what = "RAM disk data"; return DEC_READ | DEC_WRITE; }
    if (!(o & 0x04)) { *what = "RAM disk row (A16-A19)"; return DEC_WRITE; }
    if (!(o & 0x01)) { *what = "RAM disk address"; return DEC_WRITE; }
    if (!(o & 0x08)) { *what = "RAM disk prepare read"; return DEC_WRITE; }
    return 0;
}

void Rmd1::describe(json& out)
{
    out["size_kb"] = (mask + 1) / 1024;
    out["row"] = row;
    out["address"] = addr;
    out["prom"] = prom_file;
    if (!image.empty()) out["image"] = image;
}

void Rmd1::state(StateIO& io)
{
    std::vector<uint8_t> before;
    if (io.loading) before = mem;
    io.mem(mem);
    io.v(row);
    io.v(addr);
    io.v(latch);
    if (io.loading && mem != before) {   // the content of the state goes to the image file too
        dirty = true;
        media_written();
    }
}
