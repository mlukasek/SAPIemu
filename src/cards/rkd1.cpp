#include "cards/rkd1.h"
#include "core/bus.h"
#include "core/log.h"
#include "core/state.h"
#include "core/util.h"

#include <stdio.h>
#include <string.h>

using nlohmann::json;

// Drive timing (CM 5403; no drive manual found, docs/desky/RKD-1.md): 2400 rpm, sector pulse 40 us, bit
// 400 ns, the write starts DELAY_WRITE after the pulse. Seek: like the DEC RK05 of the same class (10 ms
// track to track, about 85 ms over the whole disk), an estimate.
static const double TURN_US = 25000.0;
static const double PULSE_US = 40.0;
static const double BYTE_US = 3.2;
static const double WRITE_LEAD_US = 3.0;   // where the stream starts after the pulse (DELAY_WRITE of the driver)
static const char MAGIC[8] = { 'S', 'A', 'P', 'I', 'R', 'K', 'D', '1' };
static const size_t HEADER = 16;

// CM 5400 on the cable SM 5400 - RKD-FRB: the controller track N is cylinder 2N (+1 with T00). FORMRKD of
// J. Biba formats tracks 0-203 on it, so the drive takes cylinders up to 407 (an assumption: 203 cylinders
// would end at track 101; docs/desky/RKD-1.md). The seek over the whole disk as long as on the CM 5403.
static const int CYL_5400 = 408;

const char* const Rkd1::SYSTEM_SOURCE = "media/rkd-1/tesla_cpm_system.rkd";

// `from`, `to` in cylinders of the drive
static double seek_us(int drive, int from, int to)
{
    int d = from > to ? from - to : to - from;
    if (d == 0) return 1000.0;
    return 10000.0 + (drive == Rkd1::CM5400 ? 185.0 : 370.0) * d;
}

// --- platter ------------------------------------------------------------------------------------------

bool RkdPlatter::create(const std::string& file, int nsectors, std::string& err)
{
    if (nsectors != 12 && nsectors != 24) {
        err = "a cartridge has 12 or 24 sectors";
        return false;
    }
    unload();
    sectors = nsectors;
    data.assign((size_t)SURFACES * TRACKS * sectors * (2 + cap()), 0);
    path = file;
    loaded = true;
    dirty = true;   // written at the next flush
    return true;
}

bool RkdPlatter::load(const std::string& file, bool work_copy, std::string& err)
{
    std::string p;
    if (!media_path(file, work_copy, p, err)) return false;
    std::vector<uint8_t> d;
    if (!file_read_all(p, d)) {
        err = "cannot read " + p;
        return false;
    }
    if (d.size() < HEADER || memcmp(d.data(), MAGIC, sizeof(MAGIC)) != 0 || (d[8] != 12 && d[8] != 24) ||
        d[9] != TRACKS || d[10] != SURFACES) {
        err = p + ": not an RKD-1 cartridge image (SAPIRKD1, 12 or 24 sectors, 204 tracks, 2 surfaces)";
        return false;
    }
    unload();
    sectors = d[8];
    size_t size = (size_t)SURFACES * TRACKS * sectors * (2 + cap());
    if (d.size() != HEADER + size) {
        err = p + ": wrong size of an RKD-1 image";
        return false;
    }
    data.assign(d.begin() + HEADER, d.end());
    path = p;
    loaded = true;
    dirty = false;
    return true;
}

bool RkdPlatter::save(std::string& err)
{
    if (!loaded || !dirty) return true;
    std::vector<uint8_t> d(HEADER, 0);
    memcpy(d.data(), MAGIC, sizeof(MAGIC));
    d[8] = (uint8_t)sectors;
    d[9] = TRACKS;
    d[10] = SURFACES;
    d.insert(d.end(), data.begin(), data.end());
    make_dirs(path_dir(path));
    if (!file_write_all(path, d.data(), d.size())) {
        err = "cannot write " + path;
        return false;
    }
    dirty = false;
    return true;
}

void RkdPlatter::unload(void)
{
    data.clear();
    path.clear();
    loaded = false;
    dirty = false;
}

// --- card ---------------------------------------------------------------------------------------------

Rkd1::~Rkd1()
{
    flush();
}

// Load an image, or make an unformatted one when the file is not there yet (not under media/).
static bool open_platter(RkdPlatter& p, const std::string& image, int nsectors, bool work_copy, std::string& err)
{
    std::string norm = image;
    for (char& c : norm) if (c == '\\') c = '/';
    if (norm.rfind("media/", 0) != 0 && !file_exists(data_path(image))) {
        if (!p.create(data_path(image), nsectors, err)) return false;
        LOGI("RKD-1: new unformatted platter %s (%d sectors)", p.path.c_str(), nsectors);
        return true;
    }
    return p.load(image, work_copy, err);
}

bool Rkd1::configure(const json& cfg, std::string& err)
{
    if (!cfg_bool(cfg, "working_copy", work_copy, true, err)) return false;
    long long v;
    if (!cfg_number(cfg, "spinup_s", v, 0, err)) return false;
    if (v < 0 || v > 120) { err = "spinup_s must be 0..120"; return false; }
    spinup_s = (double)v;
    static const char* const keys[UNITS] = { "unit0", "unit1", "unit2", "unit3" };
    for (int i = 0; i < UNITS; i++) {   // the drives first: the format of new platters depends on them
        units[i].drive = CM5403;
        if (!cfg.contains(keys[i]) || !cfg[keys[i]].is_object()) continue;
        std::string dr;
        if (!cfg_string(cfg[keys[i]], "drive", dr, "CM 5403", err)) return false;
        if (dr == "CM 5400") units[i].drive = CM5400;
        else if (dr != "CM 5403") { err = std::string(keys[i]) + ".drive must be \"CM 5403\" or \"CM 5400\""; return false; }
    }
    // new cartridges: 24 x 256 B for the CP/M of Tesla on the CM 5403, 12 x 512 B (SMEP) on the CM 5400
    if (!cfg_number(cfg, "new_sectors", v, units[0].drive == CM5400 ? 12 : 24, err)) return false;
    if (v != 12 && v != 24) { err = "new_sectors must be 12 or 24"; return false; }
    new_sectors = (int)v;
    for (int i = 0; i < UNITS; i++) {
        Unit& u = units[i];
        u.fitted = i == 0;
        u.cyl = u.track = 0;
        if (!cfg.contains(keys[i])) continue;
        const json& d = cfg[keys[i]];
        if (d.is_boolean()) {
            u.fitted = d.get<bool>();
            continue;
        }
        if (!d.is_object()) { err = std::string(keys[i]) + " must be an object"; return false; }
        if (!cfg_bool(d, "fitted", u.fitted, true, err)) return false;
        int fixed_sectors = u.drive == CM5400 ? 12 : new_sectors;   // the fixed platter of a CM 5400: SMEP
        std::string cart, fixed;
        bool wp;
        if (!cfg_string(d, "cartridge", cart, "", err)) return false;
        if (!cfg_bool(d, "write_protect", wp, false, err)) return false;
        char def[64];
        snprintf(def, sizeof(def), "work/rkd-1/unit%d_fixed.rkd", i);
        if (!cfg_string(d, "fixed", fixed, def, err)) return false;
        u.disk[CARTRIDGE].image = cart;
        u.disk[CARTRIDGE].write_protect = wp;
        u.disk[FIXED].image = fixed;
        if (probe || !u.fitted) continue;
        if (!cart.empty() && !insert(i, cart, wp, err)) return false;
        if (!open_platter(u.disk[FIXED], fixed, fixed_sectors, work_copy, err)) return false;
    }
    if (!probe && units[0].fitted && !units[0].disk[FIXED].loaded) {   // unit0 not described: its fixed disk
        units[0].disk[FIXED].image = "work/rkd-1/unit0_fixed.rkd";
        if (!open_platter(units[0].disk[FIXED], units[0].disk[FIXED].image, new_sectors, work_copy, err)) return false;
    }
    ppi.dev = this;
    ppi.reset();
    pit.reset();
    power_on_fill(ram, sizeof(ram), 0);
    return true;
}

bool Rkd1::insert(int i, const std::string& image, bool wp, std::string& err)
{
    if (i < 0 || i >= UNITS || !units[i].fitted) { err = "no such unit"; return false; }
    eject(i);
    RkdPlatter& p = units[i].disk[CARTRIDGE];
    if (!open_platter(p, image, new_sectors, work_copy, err)) return false;
    p.image = image;
    p.write_protect = wp;
    ready_at[i] = bus ? bus->now + us(spinup_s * 1e6) : 0;
    LOGI("%s: unit %d cartridge %s (%d sectors)%s", name.c_str(), i, p.path.c_str(), p.sectors,
         wp ? ", write protected" : "");
    return true;
}

void Rkd1::eject(int i)
{
    if (i < 0 || i >= UNITS) return;
    RkdPlatter& p = units[i].disk[CARTRIDGE];
    if (!p.loaded) return;
    std::string err;
    if (battery_files_enabled() && !p.save(err)) LOGW("%s: %s", name.c_str(), err.c_str());
    p.unload();
    p.image.clear();
}

void Rkd1::flush()
{
    if (!battery_files_enabled()) return;   // tests: work/ stays untouched
    for (Unit& u : units)
        for (RkdPlatter& p : u.disk) {
            std::string err;
            if (!p.save(err)) LOGW("%s: %s", name.c_str(), err.c_str());
        }
}

void Rkd1::reset()
{
    // /RES: the 8255 ports float (PC0 high at the inverter: T00 cleared), the RUN flip-flop, the sector
    // register (74LS174) and the RAM counters are cleared. The drives are not reset.
    ppi.reset();
    busy = false;
    t00 = false;
    sector_reg = 0;
    ptr = 0;
}

uint64_t Rkd1::us(double t) const
{
    return (uint64_t)(t * (double)bus->cpu_hz / 1e6);
}

double Rkd1::now_us(void) const
{
    return (double)bus->now * 1e6 / (double)bus->cpu_hz;
}

bool Rkd1::seeking(const Unit& u) const
{
    return bus->now < u.seek_end;
}

bool Rkd1::ready(const Unit& u) const
{
    int i = (int)(&u - units);
    return u.fitted && u.disk[CARTRIDGE].loaded && bus->now >= ready_at[i];
}

// STB: the selected drive takes the address on the lines. CM 5403: the track number (PC0, the line drivers
// on) or a restore (T00). CM 5400 on the cable SM 5400: cylinder 2N + T00 (the two exclude each other,
// PC0 clears T00), no restore: RK_HOME goes to cylinder 1.
void Rkd1::strobe(void)
{
    Unit& u = unit();
    if (!ready(u)) return;
    int n = (ppi.out_c & 0x01) ? ppi.out_a : 0;   // no address on the lines: 0
    int cyl, limit;
    if (u.drive == CM5400) {
        cyl = 2 * n + (t00 ? 1 : 0);
        limit = CYL_5400;
    } else {
        cyl = n;
        limit = RkdPlatter::TRACKS;
        if (t00 && !(ppi.out_c & 0x01)) u.err = false;   // a restore clears the errors of the drive
    }
    if (cyl >= limit) {   // NSA
        u.apt = false;
        return;
    }
    u.apt = true;
    uint64_t from = bus->now > u.seek_end ? bus->now : u.seek_end;
    u.seek_end = from + us(seek_us(u.drive, u.cyl, cyl));
    u.cyl = cyl;
    // the data of track N lie on cylinder 2N of a CM 5400; an odd cylinder (only after RK_HOME, the driver
    // always seeks before a transfer) is taken as the track below it
    u.track = u.drive == CM5400 ? cyl / 2 : cyl;
}

// RUN: the sector compare waits for the pulse of the sector in 4AH, then the delay of counter 1, then the
// bytes of counter 2 go from or to the RAM. Without the sector in 256 pulses the controller gives up.
void Rkd1::start(void)
{
    if (busy) return;
    bool rd = (ppi.out_c & 0x08) != 0, wr = (ppi.out_c & 0x10) != 0;
    if (!rd && !wr) return;
    busy = true;
    cnt_ok = false;
    op_write = wr;
    op_ok = false;
    op_unit = unit_no();
    Unit& u = units[op_unit];
    op_disk = disk_index(u);
    op_surface = (ppi.out_c & 0x20) ? 1 : 0;
    RkdPlatter& p = u.disk[op_disk];
    if (!ready(u) || !p.loaded) {   // no turning disk, no sector pulses: RUN stays until /RES
        op_end = UINT64_MAX;
        return;
    }
    op_track = u.track;
    op_count = pit.divisor(2);
    double delay = pit.divisor(1);
    double t0 = now_us();
    double settled = (double)u.seek_end * 1e6 / (double)bus->cpu_hz;
    if (settled > t0) t0 = settled;
    double slot = TURN_US / p.sectors;
    uint64_t k0 = (uint64_t)(t0 / slot) + 1;   // the next sector pulse
    double give_up = (double)(k0 + 255) * slot + PULSE_US;
    op_sector = (uint8_t)~sector_reg & 0x1F;
    if (op_sector >= p.sectors) {
        op_end = us(give_up);
        return;
    }
    uint64_t k = k0;
    while ((int)(k % (uint64_t)p.sectors) != op_sector) k++;
    double pulse = (double)k * slot;
    if (op_write) {
        op_ok = true;
        op_end = us(pulse + PULSE_US + delay + op_count * BYTE_US);
        return;
    }
    // reading: the data separator finds the zeros, then 80H
    const uint8_t* rec = &p.data[p.record(op_surface, op_track, op_sector)];
    size_t len = (size_t)(rec[0] | rec[1] << 8);
    size_t i = 0;
    while (i < len && rec[2 + i] == 0) i++;
    if (i >= len || rec[2 + i] != 0x80) {   // nothing written there (or garbage): no data counted
        op_end = us(give_up);
        return;
    }
    op_sync = i + 1;
    op_ok = true;
    op_end = us(pulse + PULSE_US + WRITE_LEAD_US + (op_sync + op_count) * BYTE_US);
}

void Rkd1::finish(void)
{
    busy = false;
    if (!op_ok) return;
    Unit& u = units[op_unit];
    RkdPlatter& p = u.disk[op_disk];
    cnt_ok = true;
    if (!p.loaded) return;
    uint8_t* rec = &p.data[p.record(op_surface, op_track, op_sector)];
    size_t cap = (size_t)p.cap();
    if (op_write) {
        if (p.write_protect) {
            // the drive inhibits its write current (as the RK05 family), the controller counts the bytes
            // anyway: nothing written, no fault. The driver of Tesla does not read WPROTECT, CP/M does not
            // notice. A fault (ERR) here hangs the system: WBOOT and the boot ROM never restore, RK_SEEK
            // returns with BC left on the stack (docs/desky/RKD-1.md).
            ptr = (uint16_t)((ptr + op_count) & 0x3FF);
            return;
        }
        size_t n = op_count < cap ? op_count : cap;
        for (size_t i = 0; i < n; i++) rec[2 + i] = ram[(ptr + i) & 0x3FF];
        memset(rec + 2 + n, 0, cap - n);
        rec[0] = (uint8_t)n;
        rec[1] = (uint8_t)(n >> 8);
        if (op_count > cap) {   // written over the next sector pulse: the next sector is gone
            uint8_t* next = &p.data[p.record(op_surface, op_track, (op_sector + 1) % p.sectors)];
            memset(next, 0, 2 + cap);
        }
        ptr = (uint16_t)((ptr + op_count) & 0x3FF);
        p.dirty = true;
        return;
    }
    size_t len = (size_t)(rec[0] | rec[1] << 8);
    for (uint32_t i = 0; i < op_count; i++) {
        size_t at = op_sync + i;
        ram[ptr] = at < len ? rec[2 + at] : 0;   // past the written bytes: the gap
        ptr = (uint16_t)((ptr + 1) & 0x3FF);
    }
}

static uint16_t sector_header(int disk, int surface, int track, int sector, int len)
{
    return (uint16_t)((len == 512 ? 0x8000 : 0) | (disk ? 0x4000 : 0) | (surface ? 0x2000 : 0) | (track << 5) | sector);
}

int Rkd1::disk_line(int ui, int disk) const
{
    return (disk == FIXED) != (units[ui].drive == CM5400) ? 1 : 0;
}

bool Rkd1::write_sector(int ui, int disk, int surface, int track, int sector, const uint8_t* d, int len)
{
    if (ui < 0 || ui >= UNITS || disk < 0 || disk > 1 || surface < 0 || surface > 1 || track < 0 ||
        track >= RkdPlatter::TRACKS)
        return false;
    RkdPlatter& p = units[ui].disk[disk];
    if (!p.loaded || sector < 0 || sector >= p.sectors || 31 + len > p.cap()) return false;
    std::vector<uint8_t> s(26, 0);
    uint16_t h = sector_header(disk_line(ui, disk), surface, track, sector, len);
    s.push_back(0x80);
    s.push_back((uint8_t)(h >> 8));
    s.push_back((uint8_t)h);
    uint16_t sum = 0;
    for (int i = 0; i < len; i++) {
        s.push_back(d[i]);
        sum = (uint16_t)(sum + d[i]);
    }
    s.push_back((uint8_t)sum);
    s.push_back((uint8_t)(sum >> 8));
    uint8_t* rec = &p.data[p.record(surface, track, sector)];
    memset(rec, 0, 2 + (size_t)p.cap());
    memcpy(rec + 2, s.data(), s.size());
    rec[0] = (uint8_t)s.size();
    rec[1] = (uint8_t)(s.size() >> 8);
    p.dirty = true;
    return true;
}

bool Rkd1::read_sector(int ui, int disk, int surface, int track, int sector, uint8_t* d, int len)
{
    if (ui < 0 || ui >= UNITS || disk < 0 || disk > 1 || surface < 0 || surface > 1 || track < 0 ||
        track >= RkdPlatter::TRACKS)
        return false;
    RkdPlatter& p = units[ui].disk[disk];
    if (!p.loaded || sector < 0 || sector >= p.sectors) return false;
    const uint8_t* rec = &p.data[p.record(surface, track, sector)];
    size_t n = (size_t)(rec[0] | rec[1] << 8), i = 0;
    while (i < n && rec[2 + i] == 0) i++;
    if (i >= n || rec[2 + i] != 0x80 || i + 1 + 2 + (size_t)len + 2 > n) return false;
    const uint8_t* s = rec + 2 + i + 1;
    if ((uint16_t)(s[0] << 8 | s[1]) != sector_header(disk_line(ui, disk), surface, track, sector, len)) return false;
    uint16_t sum = 0;
    for (int k = 0; k < len; k++) {
        d[k] = s[2 + k];
        sum = (uint16_t)(sum + d[k]);
    }
    return s[2 + len] == (uint8_t)sum && s[3 + len] == (uint8_t)(sum >> 8);
}

// Every sector E5H, as FORMTRKD (24 x 256 B) and FORMRKD (12 x 512 B) write them through RKD.MAC: the same
// bytes, so the result cannot be told from a platter formatted in CP/M.
bool Rkd1::format_platter(int ui, int disk, std::string& err)
{
    if (ui < 0 || ui >= UNITS || disk < 0 || disk > 1 || !units[ui].disk[disk].loaded) {
        err = "no platter";
        return false;
    }
    RkdPlatter& p = units[ui].disk[disk];
    if (p.write_protect) {
        err = "write protected";
        return false;
    }
    int len = p.sectors == 12 ? 512 : 256;
    std::vector<uint8_t> e5((size_t)len, 0xE5);
    for (int s = 0; s < RkdPlatter::SURFACES; s++)
        for (int t = 0; t < RkdPlatter::TRACKS; t++)
            for (int c = 0; c < p.sectors; c++)
                if (!write_sector(ui, disk, s, t, c, e5.data(), len)) {
                    err = "format failed";
                    return false;
                }
    LOGI("%s: unit %d %s formatted (%d x %d B, E5H)", name.c_str(), ui, disk == FIXED ? "fixed disk" : "cartridge",
         p.sectors, len);
    return true;
}

bool Rkd1::platter_formatted(int ui, int disk)
{
    if (ui < 0 || ui >= UNITS || disk < 0 || disk > 1 || !units[ui].disk[disk].loaded) return false;
    RkdPlatter& p = units[ui].disk[disk];
    int len = p.sectors == 12 ? 512 : 256;
    std::vector<uint8_t> b((size_t)len);
    return read_sector(ui, disk, 0, 2, 0, b.data(), len);
}

// The system of Tesla Elstroj (CCP, BDOS, CBIOS: tracks 0 and 1 of medium 0) copied sector by sector from a
// system cartridge, the bytes as its SYSGEN would have written them. Only the system tracks change.
bool Rkd1::write_system(int ui, const std::string& source, std::string& err)
{
    if (ui < 0 || ui >= UNITS || !units[ui].fitted || !units[ui].disk[CARTRIDGE].loaded) {
        err = "no cartridge in the unit";
        return false;
    }
    Unit& u = units[ui];
    RkdPlatter& p = u.disk[CARTRIDGE];
    if (u.drive != CM5403 || p.sectors != 24) {
        err = "the CP/M of Tesla Elstroj needs a CM 5403 with a 24 x 256 B cartridge";
        return false;
    }
    if (p.write_protect) {
        err = "the cartridge is write protected";
        return false;
    }
    RkdPlatter src;
    if (!src.load(source, false, err)) return false;
    if (src.sectors != 24) {
        err = source + ": not a 24 x 256 B cartridge";
        return false;
    }
    size_t rec = 2 + (size_t)p.cap();
    for (int t = 0; t < 2; t++)
        for (int c = 0; c < 24; c++)
            memcpy(&p.data[p.record(0, t, c)], &src.data[src.record(0, t, c)], rec);
    p.dirty = true;
    LOGI("%s: unit %d: the system written on the cartridge (tracks 0 and 1 of A:) from %s", name.c_str(), ui,
         source.c_str());
    return true;
}

void Rkd1::update(void)
{
    if (busy && bus->now >= op_end) finish();
}

uint8_t Rkd1::pins_b()
{
    update();
    Unit& u = unit();
    bool rdy = ready(u);
    uint8_t b = 0;
    if (busy) b |= 0x01;
    if (rdy && platter().loaded && platter().write_protect) b |= 0x02;
    if (cnt_ok) b |= 0x04;
    if (rdy && u.apt) b |= 0x08;
    if (ppi.out_c & 0x80) {   // status of the selected unit, active low; a missing drive leaves them high
        bool sk = rdy && seeking(u);
        if (!(rdy && !sk)) b |= 0x10;   // /SRDY
        b |= 0x20;                      // /STO: the seek never times out here
        if (!rdy) b |= 0x40;            // /UNR
        if (!(rdy && u.err)) b |= 0x80; // /ERR
    } else {
        for (int i = 0; i < UNITS; i++)   // /ATN: the seek of unit i is over
            if (!(ready(units[i]) && !seeking(units[i]))) b |= (uint8_t)(0x10 << i);
    }
    return b;
}

void Rkd1::outputs_changed(int port, uint8_t value)
{
    if (port == 2 && (value & 0x01)) t00 = false;   // PC0 drives the track number and clears D7CA
}

bool Rkd1::io_read(uint8_t port, uint8_t& data)
{
    if ((port & 0xF0) != 0x40) return false;
    update();
    int r = port & 0x0F;
    if (r < 4) data = pit.read(r);
    else if (r < 8) data = ppi.read(r - 4);
    else if (r == 0x0C) {
        data = ram[ptr];
        ptr = (uint16_t)((ptr + 1) & 0x3FF);
    } else data = 0xFF;   // 48H-4BH, 4DH-4FH: write only or unused
    return true;
}

bool Rkd1::io_write(uint8_t port, uint8_t data)
{
    if ((port & 0xF0) != 0x40) return false;
    update();
    int r = port & 0x0F;
    if (r < 4) {
        pit.write(r, data);
        if (r == 2) cnt_ok = false;   // mode 0: a new count makes OUT2 low
    } else if (r < 8) ppi.write(r - 4, data);
    else if (r == 0x08) strobe();
    else if (r == 0x09) start();
    else if (r == 0x0A) {
        sector_reg = data;
        if (data & 0x80) ptr = 0;
    } else if (r == 0x0B) {
        t00 = (data & 0x01) && !(ppi.out_c & 0x01);
        if ((data & 0x02) && ready(unit()) && platter().loaded) platter().write_protect = true;   // WPP
    } else if (r == 0x0C) {
        ram[ptr] = data;
        ptr = (uint16_t)((ptr + 1) & 0x3FF);
    }
    return true;
}

int Rkd1::decode_io(uint8_t port, const char** what)
{
    if ((port & 0xF0) != 0x40) return 0;
    static const char* const names[16] = {
        "8253 counter 0", "8253 counter 1 (delay)", "8253 counter 2 (bytes)", "8253 control",
        "8255 PA track", "8255 PB status", "8255 PC control", "8255 control",
        "STB", "RUN", "sector, RAM pointer", "T00, WPP", "buffer RAM", "unused", "unused", "unused",
    };
    *what = names[port & 0x0F];
    int r = port & 0x0F;
    if (r < 8 || r == 0x0C) return DEC_READ | DEC_WRITE;
    return DEC_WRITE;
}

void Rkd1::describe(json& out)
{
    update();
    json us_ = json::array();
    for (int i = 0; i < UNITS; i++) {
        const Unit& u = units[i];
        if (!u.fitted) continue;
        json j;
        j["unit"] = i;
        j["drive"] = u.drive == CM5400 ? "CM 5400" : "CM 5403";
        j["ready"] = ready(u);
        j["track"] = u.track;
        if (u.drive == CM5400) j["cylinder"] = u.cyl;
        j["seeking"] = bus && seeking(u);
        j["error"] = u.err;
        static const char* const names[2] = { "cartridge", "fixed" };
        for (int d = 0; d < 2; d++) {
            const RkdPlatter& p = u.disk[d];
            json k;
            k["loaded"] = p.loaded;
            if (p.loaded) {
                k["file"] = data_relative(p.path);
                k["sectors"] = p.sectors;
                k["sector_bytes"] = p.sectors == 12 ? 512 : 256;
                k["write_protect"] = p.write_protect;
            }
            j[names[d]] = k;
        }
        us_.push_back(j);
    }
    out["units"] = us_;
    out["busy"] = busy;
    out["selected_unit"] = unit_no();
    out["selected_disk"] = disk_index(units[unit_no()]) == FIXED ? "fixed" : "cartridge";
    out["surface"] = (ppi.out_c & 0x20) ? 1 : 0;
}

std::string Rkd1::live_status()
{
    const Unit& u = units[0];
    if (!u.fitted) return std::string();
    char b[96];
    if (!u.disk[CARTRIDGE].loaded) snprintf(b, sizeof(b), "RKD-1: bez kazety");
    else snprintf(b, sizeof(b), "RKD-1: stopa %d%s", u.track, busy ? " *" : "");
    return b;
}

void Rkd1::state(StateIO& io)
{
    // the platters are referred to (files), not stored, like the floppies
    for (int i = 0; i < UNITS; i++) {
        Unit& u = units[i];
        RkdPlatter& c = u.disk[CARTRIDGE];
        io.media("rkd", c.path);
        io.media("rkd", u.disk[FIXED].path);
        std::string img = c.image;
        io.v(img);
        bool wp = c.write_protect;
        io.v(wp);
        if (io.loading && io.ok() && img != c.image) {
            std::string err;
            if (img.empty()) eject(i);
            else if (!insert(i, img, wp, err)) LOGW("state: RKD-1 cartridge %s: %s", img.c_str(), err.c_str());
        }
        if (io.loading) c.write_protect = wp;
        bool fwp = u.disk[FIXED].write_protect;
        io.v(fwp);
        u.disk[FIXED].write_protect = fwp;
        io.e(u.track);
        io.e(u.cyl);
        io.v(u.seek_end);
        io.v(u.err);
        io.v(u.apt);
        io.v(ready_at[i]);
    }
    ppi.state(io);
    pit.state(io);
    io.arr(ram, sizeof(ram));
    io.v(ptr);
    io.v(sector_reg);
    io.v(t00);
    io.v(cnt_ok);
    io.v(busy);
    io.v(op_write);
    io.v(op_end);
    io.v(op_ok);
    io.e(op_unit);
    io.e(op_disk);
    io.e(op_surface);
    io.e(op_track);
    io.e(op_sector);
    io.v(op_count);
    uint64_t sync = op_sync;
    io.v(sync);
    op_sync = (size_t)sync;
}
