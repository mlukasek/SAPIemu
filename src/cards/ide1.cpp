#include "cards/ide1.h"
#include "core/state.h"
#include "core/bus.h"
#include "core/log.h"
#include "core/util.h"

#include <stdio.h>
#include <string.h>
#include <vector>

using nlohmann::json;

// Port C of the IDE 8255 (through 7406/7407): PC0-2 DA0-2, PC3 /CS1FX, PC4 /CS3FX,
// PC5 /DIOW, PC6 /DIOR, PC7 /RESET; a 1 asserts the (active low) IDE signal.
enum { PC_CS1 = 0x08, PC_CS3 = 0x10, PC_DIOW = 0x20, PC_DIOR = 0x40, PC_RESET = 0x80 };

Ide1::Ide1() : Card("IDE-1")
{
    ppi_ide.dev = this;
    pmd.attach(&ppi_pmd);
}

Ide1::~Ide1()
{
    flush();
}

bool ide_create_image(const std::string& path, int mb, std::string& err)
{
    make_dirs(path_dir(path));
    FILE* f = file_open(path, "wb");
    if (!f) { err = "cannot create " + path; return false; }
    std::vector<uint8_t> chunk(1 << 20, 0xE5);
    for (int i = 0; i < mb; i++) {
        if (fwrite(chunk.data(), 1, chunk.size(), f) != chunk.size()) {
            fclose(f);
            err = "cannot write " + path;
            return false;
        }
    }
    fclose(f);
    LOGI("created IDE image %s (%d MB, E5h)", path.c_str(), mb);
    return true;
}

bool Ide1::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_number(cfg, "ide_base", v, 0x58, err)) return false;
    base_ide = (uint8_t)(v & 0xFC);
    if (!cfg_number(cfg, "pmd_base", v, 0x5C, err)) return false;
    base_pmd = (uint8_t)(v & 0xFC);

    // IDE disk
    ide_enabled = cfg.contains("ide") && !cfg["ide"].is_null();
    if (ide_enabled) {
        const json& d = cfg["ide"];
        std::string image, path;
        bool ro, work;
        long long create_mb;
        if (!cfg_string(d, "image", image, "", err)) return false;
        if (!cfg_bool(d, "readonly", ro, false, err)) return false;
        if (!cfg_bool(d, "working_copy", work, true, err)) return false;
        if (!cfg_number(d, "create_mb", create_mb, 0, err)) return false;
        if (!image.empty() && !probe) {
            if (!media_path(image, work, path, err)) {
                if (create_mb <= 0) return false;
                err.clear();
                path = data_path(image);
            }
            if (!file_exists(path) && create_mb > 0 && !ide_create_image(path, (int)create_mb, err)) return false;
            if (!ide.open(path, ro, err)) return false;
            LOGI("%s: IDE disk %s, %u sectors (%u MB)%s", name.c_str(), path.c_str(), ide.sector_count(),
                 ide.sector_count() / 2048, ro ? ", read only" : "");
        }
    }

    // PMD32-SD
    pmd_enabled = cfg.contains("pmd32sd") && !cfg["pmd32sd"].is_null();
    if (pmd_enabled) {
        const json& p = cfg["pmd32sd"];
        std::string sd, path;
        bool work;
        long long startup;
        if (!cfg_string(p, "sd_card", sd, "media/pmd32sd", err)) return false;
        if (!cfg_bool(p, "working_copy", work, true, err)) return false;
        if (!cfg_bool(p, "card_locked", pmd.card_locked, false, err)) return false;
        if (!cfg_number(p, "startup_ms", startup, 0, err)) return false;
        pmd.startup_ms = (uint32_t)startup;
        if (probe) return true;   // only the decoding is wanted: no SD card copy
        if (!media_path(sd, work, path, err)) return false;
        if (!pmd.init(path, err)) return false;
        if (p.contains("drives")) {
            const json& drv = p["drives"];
            if (!drv.is_array() || drv.size() > Pmd32Sd::DRIVES) {
                err = "pmd32sd.drives must be an array of up to 4 image names (A-D)";
                return false;
            }
            for (size_t i = 0; i < drv.size(); i++) {
                if (drv[i].is_null()) continue;
                bool wp = false;
                std::string fn;
                if (drv[i].is_string()) {
                    fn = drv[i].get<std::string>();
                } else if (drv[i].is_object()) {
                    fn = drv[i].value("image", std::string());
                    wp = drv[i].value("wp", false);
                }
                if (fn.empty()) continue;
                int rv = pmd.mount((int)i, fn, wp);
                if (rv != 0) {
                    err = "PMD32-SD drive " + std::string(1, (char)('A' + i)) + ": cannot insert '" + fn +
                          "' (error " + std::to_string(rv) + ")";
                    return false;
                }
            }
        }
        pmd.power_on(bus->now);
        LOGI("%s: PMD32-SD card %s", name.c_str(), path.c_str());
    }
    return true;
}

void Ide1::reset()
{
    ppi_ide.reset();
    ppi_pmd.reset();
    last_pc = 0;
    data_lo = data_hi = 0xFF;
    // /RES does not reach the PMD32-SD: it keeps running (power on in configure())
}

void Ide1::advance(uint64_t now)
{
    if (pmd_enabled) pmd.advance(now, bus->cpu_hz);
}

void Ide1::outputs_changed(int port, uint8_t value)
{
    if (port != 2) return;
    // port C as outputs only when the lower and upper halves are outputs (CW 80h/92h)
    uint8_t pc = (ppi_ide.control() & 0x09) == 0 ? value : 0;
    uint8_t rising = (uint8_t)(pc & ~last_pc), falling = (uint8_t)(last_pc & ~pc);
    if (ide_enabled) {
        if (rising & PC_RESET) ide.reset();
        bool cs1 = (pc & PC_CS1) != 0, cs3 = (pc & PC_CS3) != 0;
        int reg = pc & 7;
        if ((rising & PC_DIOR) && (cs1 || cs3)) {
            uint16_t w = ide.read_reg(cs1, reg);
            data_lo = (uint8_t)w;
            data_hi = (reg == 0 && cs1) ? (uint8_t)(w >> 8) : 0xFF;   // 8-bit registers: D8-D15 float
        }
        // the device latches written data at the end of /DIOW
        bool cs1w = (last_pc & PC_CS1) != 0, cs3w = (last_pc & PC_CS3) != 0;
        if ((falling & PC_DIOW) && (cs1w || cs3w)) {
            uint16_t w = (uint16_t)(ppi_ide.out_a | (ppi_ide.out_b << 8));
            ide.write_reg(cs1w, last_pc & 7, w);
        }
    }
    last_pc = pc;
}

int Ide1::decode_io(uint8_t port, const char** what)
{
    // both 8255 are on the card: they answer whether a disk / PMD32-SD is connected or not
    if ((port & 0xFC) == base_ide) {
        *what = ide_enabled ? "IDE (8255)" : "IDE (8255, no disk)";
        return DEC_READ | DEC_WRITE;
    }
    if ((port & 0xFC) == base_pmd) {
        *what = pmd_enabled ? "PMD32-SD (8255)" : "PMD32-SD (8255, not connected)";
        return DEC_READ | DEC_WRITE;
    }
    return 0;
}

unsigned Ide1::panel_leds()
{
    return accessed && bus && bus->now - access_at < bus->cpu_hz / 20 ? 1u : 0u;
}

bool Ide1::io_read(uint8_t port, uint8_t& data)
{
    if ((port & 0xFC) == base_ide) {
        data = ppi_ide.read(port & 3);
        accessed = true;
        access_at = bus->now;
        return true;
    }
    if ((port & 0xFC) == base_pmd) {
        data = ppi_pmd.read(port & 3);
        accessed = true;
        access_at = bus->now;
        return true;
    }
    return false;
}

bool Ide1::io_write(uint8_t port, uint8_t data)
{
    if ((port & 0xFC) == base_ide) {
        ppi_ide.write(port & 3, data);
        accessed = true;
        access_at = bus->now;
        return true;
    }
    if ((port & 0xFC) == base_pmd) {
        ppi_pmd.write(port & 3, data);
        accessed = true;
        access_at = bus->now;
        if (pmd_enabled) pmd.advance(bus->now, bus->cpu_hz);   // the ATmega reacts within microseconds
        return true;
    }
    return false;
}

void Ide1::flush()
{
    ide.flush();
    pmd.flush();
}

void Ide1::describe(json& out)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02Xh", base_ide);
    out["ide_base"] = buf;
    snprintf(buf, sizeof(buf), "%02Xh", base_pmd);
    out["pmd_base"] = buf;
    if (ide_enabled) {
        out["ide_image"] = ide.path();
        out["ide_mb"] = ide.sector_count() / 2048;
        out["ide_readonly"] = ide.is_readonly();
    }
    if (pmd_enabled) {
        out["sd_card"] = pmd.root();
        json drives = json::array();
        for (int d = 0; d < Pmd32Sd::DRIVES; d++) {
            if (pmd.drive_present(d))
                drives.push_back(json{ { "drive", std::string(1, (char)('A' + d)) }, { "image", pmd.drive_path(d) },
                                       { "wp", pmd.drive_wp(d) }, { "tracks", pmd.drive_tracks(d) },
                                       { "sectors", pmd.drive_sectors(d) } });
            else
                drives.push_back(json{ { "drive", std::string(1, (char)('A' + d)) }, { "image", nullptr } });
        }
        out["pmd32sd"] = drives;
    }
}

void Ide1::state(StateIO& io)
{
    io.v(access_at);
    io.v(accessed);
    ppi_ide.state(io);
    ppi_pmd.state(io);
    io.begin("ide");
    ide.state(io);
    io.end();
    io.begin("pmd32sd");
    pmd.state(io);
    io.end();
    io.v(last_pc);
    io.v(data_lo);
    io.v(data_hi);
}
