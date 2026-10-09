#include "cards/rmp1.h"
#include "core/bus.h"
#include "core/log.h"
#include "core/state.h"
#include "core/util.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>

using nlohmann::json;

// Drive timing (SM 5300, no manual found, docs/desky/RMP-1.md): the read head 0.15" behind the write head
// (Pertec drives of the same kind: read after write within the 32 ms RMP.MAC waits), rewind about 50 ips.
static const int64_t HEAD_GAP = MagTape::UNIT * 15 / 100;
static const int64_t REWIND_STEP = 4;          // units per PHI2 clock: 4 x 12.5 ips
// Start and stop: full speed in 65536 PHI2 clocks (32.8 ms), so the tape travels 0.2" while it starts or
// stops (ECMA-62 asks 0.19" for 800 bpi). RMP.MAC reads 20 ms after the start and wants no data then: after
// a backspace it stops only 9 ms past the block, the drive coasting on makes the gap.
static const int32_t FULL = 65536, ACCEL = 1;
static const int64_t MS = 2000;                // tape units per millisecond at 12.5 ips
// Where a loaded tape has its records: as RMP.MAC writes them (the gap before a block 33 ms, before a tape
// mark 368 ms, at BOT 570 / 855 ms, plus the 5 ms the tape runs on after a block)
static const int64_t GAP_BLOCK = (5 + 33) * MS, GAP_MARK = (5 + 368) * MS;
static const int64_t BOT_BLOCK = 570 * MS, BOT_MARK = 855 * MS;

static int parity9(uint16_t v)
{
    int n = 0;
    for (; v; v &= (uint16_t)(v - 1)) n++;
    return n & 1;
}

static uint16_t odd_row(uint8_t b)   // a data row with odd parity on 9 tracks
{
    return (uint16_t)(b | (parity9(b) ? 0 : 0x100));
}

// --- tape -------------------------------------------------------------------------------------------------

// comp.crc of RMP.MAC (ČSN 36 9464 art. 28 ba-be): the 9-bit CRC register, c1 in `d`, c2..c9 in `e`
static void crc_step(uint8_t& d, uint8_t& e, uint8_t data, int p)
{
    int c9 = e & 1, c1 = d & 1;
    uint8_t rot = (uint8_t)((c1 << 7) | (e >> 1));
    uint8_t mask = c9 ? 0x3C : 0;
    e = (uint8_t)(data ^ rot ^ mask);
    d = (uint8_t)(p ^ c9);
}

uint16_t MagTape::crc(const uint8_t* data, size_t len)
{
    uint8_t d = 0, e = 0;
    for (size_t i = 0; i < len; i++) crc_step(d, e, data[i], parity9(data[i]) ? 0 : 1);
    crc_step(d, e, 0, 0);   // bd)
    e ^= 0xD7;              // be): c2, c3, c5, c7..c9 inverted
    d ^= 1;                 // and c1
    return (uint16_t)(e | ((d & 1) << 8));
}

void MagTape::put_record(std::vector<Row>& rows, int64_t pos, const uint8_t* data, size_t len)
{
    uint16_t lrc = 0;
    for (size_t i = 0; i < len; i++) {
        uint16_t r = odd_row(data[i]);
        rows.push_back({ pos + (int64_t)i * ROW, r });
        lrc ^= r;
    }
    int64_t last = pos + (int64_t)(len ? len - 1 : 0) * ROW;
    uint16_t c = crc(data, len);
    if (c) rows.push_back({ last + 4 * ROW, c });   // an all-zero row is no row
    lrc ^= c;
    if (lrc) rows.push_back({ last + 8 * ROW, lrc });
}

void MagTape::put_mark(std::vector<Row>& rows, int64_t pos)
{
    rows.push_back({ pos, odd_row(0x13) });
    rows.push_back({ pos + 8 * ROW, odd_row(0x13) });
}

void MagTape::erase_from(int64_t pos)
{
    size_t n = rows.size();
    while (n > 0 && rows[n - 1].pos >= pos) n--;
    if (n != rows.size()) {
        rows.resize(n);
        dirty = true;
    }
}

std::vector<MagTape::Record> MagTape::records(void) const
{
    std::vector<Record> out;
    size_t i = 0;
    while (i < rows.size()) {
        size_t j = i + 1;   // the rows of one block: no gap longer than 16 rows
        while (j < rows.size() && rows[j].pos - rows[j - 1].pos <= 16 * ROW) j++;
        Record r;
        r.pos = rows[i].pos;
        r.mark = false;
        r.bad = false;
        if (j - i == 2 && (rows[i].ch & 0xFF) == 0x13 && (rows[i + 1].ch & 0xFF) == 0x13 &&
            rows[i + 1].pos - rows[i].pos > 6 * ROW) {
            r.mark = true;
        } else {
            size_t k = i;   // the data: rows one ROW apart (half a row of slack)
            r.data.push_back((uint8_t)rows[k].ch);
            while (k + 1 < j && rows[k + 1].pos - rows[k].pos < ROW * 3 / 2) {
                k++;
                r.data.push_back((uint8_t)rows[k].ch);
            }
            uint16_t lrc = 0;
            for (size_t m = i; m < j; m++) lrc ^= rows[m].ch;
            for (size_t m = i; m <= k; m++)
                if (!parity9(rows[m].ch)) r.bad = true;
            uint16_t c = crc(r.data.data(), r.data.size());
            uint16_t got = 0;
            for (size_t m = k + 1; m < j; m++)
                if (rows[m].pos - rows[k].pos < 6 * ROW) got = rows[m].ch;
            if (got != c || lrc != 0) r.bad = true;
        }
        out.push_back(r);
        i = j;
    }
    return out;
}

static void put32(std::vector<uint8_t>& v, uint32_t x)
{
    for (int i = 0; i < 4; i++) v.push_back((uint8_t)(x >> (8 * i)));
}

bool MagTape::save(std::string& err)
{
    if (!loaded || !dirty) return true;
    std::vector<uint8_t> f;
    for (const Record& r : records()) {
        if (r.mark) {
            put32(f, 0);
            continue;
        }
        uint32_t len = (uint32_t)r.data.size() | (r.bad ? 0x80000000u : 0);
        put32(f, len);
        f.insert(f.end(), r.data.begin(), r.data.end());
        if (r.data.size() & 1) f.push_back(0);   // SIMH: records padded to an even length
        put32(f, len);
    }
    make_dirs(path_dir(path));
    if (!file_write_all(path, f.data(), f.size())) {
        err = "cannot write " + path;
        return false;
    }
    dirty = false;
    return true;
}

bool MagTape::load(const std::string& file, bool work_copy, std::string& err)
{
    unload();
    std::string norm = file;
    for (char& c : norm) if (c == '\\') c = '/';
    if (norm.rfind("media/", 0) != 0 && !file_exists(data_path(file))) {   // a blank tape
        path = data_path(file);
        loaded = true;
        dirty = true;
        return true;
    }
    std::string p;
    if (!media_path(file, work_copy, p, err)) return false;
    std::vector<uint8_t> d;
    if (!file_read_all(p, d)) {
        err = "cannot read " + p;
        return false;
    }
    int64_t pos = 0;
    bool first = true;
    size_t i = 0;
    while (i + 4 <= d.size()) {
        uint32_t len = (uint32_t)(d[i] | d[i + 1] << 8 | d[i + 2] << 16 | (uint32_t)d[i + 3] << 24);
        i += 4;
        if (len == 0xFFFFFFFFu) break;      // end of medium
        if (len == 0xFFFFFFFEu || (len & 0xFFFF0000u) == 0xFFFF0000u) continue;   // erase gaps
        if (len == 0) {
            pos += first ? BOT_MARK + HEAD_GAP : GAP_MARK;
            put_mark(rows, pos);
            pos = rows.back().pos;
            first = false;
            continue;
        }
        size_t n = len & 0x00FFFFFF;
        if (i + n > d.size()) {
            err = p + ": a record goes past the end of the file";
            return false;
        }
        pos += first ? BOT_BLOCK + HEAD_GAP : GAP_BLOCK;
        put_record(rows, pos, &d[i], n);
        pos = rows.back().pos;
        first = false;
        i += n + (n & 1);
        i += 4;   // the length after the record
    }
    path = p;
    loaded = true;
    dirty = false;
    return true;
}

void MagTape::unload(void)
{
    rows.clear();
    path.clear();
    loaded = false;
    dirty = false;
}

// --- card -------------------------------------------------------------------------------------------------

Rmp1::~Rmp1()
{
    flush();
}

bool Rmp1::configure(const json& cfg, std::string& err)
{
    if (!cfg_bool(cfg, "working_copy", work_copy, true, err)) return false;
    if (!cfg_bool(cfg, "ready_at_start", ready_at_start, true, err)) return false;
    static const char* const keys[UNITS] = { "unit0", "unit1", "unit2" };
    for (int i = 0; i < UNITS; i++) {
        Unit& u = units[i];
        u.fitted = i == 0;
        std::string tape = i == 0 ? "work/rmp-1/paska0.tap" : "";
        u.ring = true;
        if (cfg.contains(keys[i])) {
            const json& d = cfg[keys[i]];
            if (d.is_boolean()) u.fitted = d.get<bool>();
            else if (d.is_object()) {
                if (!cfg_bool(d, "fitted", u.fitted, true, err)) return false;
                if (!cfg_string(d, "tape", tape, tape.c_str(), err)) return false;
                if (!cfg_bool(d, "write_ring", u.ring, true, err)) return false;
            } else {
                err = std::string(keys[i]) + " must be an object";
                return false;
            }
        }
        u.tape.image = tape;
        if (probe || !u.fitted || tape.empty()) continue;
        if (!insert(i, tape, err)) return false;
        if (ready_at_start) {   // the operator threaded it and pressed LOAD and ON-LINE
            u.loaded = true;
            u.online = true;
        }
    }
    ppi_ctl.dev = this;
    reset();
    return true;
}

bool Rmp1::insert(int i, const std::string& image, std::string& err)
{
    if (i < 0 || i >= UNITS || !units[i].fitted) {
        err = "no such unit";
        return false;
    }
    eject(i);
    Unit& u = units[i];
    if (!u.tape.load(image, work_copy, err)) return false;
    u.tape.image = image;
    u.pos = 0;
    u.next = 0;
    u.loaded = false;   // on the drive, not threaded yet: LOAD
    u.online = false;
    LOGI("%s: unit %d tape %s (%d records)", name.c_str(), i, u.tape.path.c_str(), (int)u.tape.records().size());
    return true;
}

void Rmp1::eject(int i)
{
    if (i < 0 || i >= UNITS) return;
    Unit& u = units[i];
    if (!u.tape.loaded) return;
    std::string err;
    if (battery_files_enabled() && !u.tape.save(err)) LOGW("%s: %s", name.c_str(), err.c_str());
    u.tape.unload();
    u.tape.image.clear();
    u.loaded = u.online = u.rewinding = u.writing = false;
    u.pos = 0;
    u.next = 0;
}

void Rmp1::flush()
{
    if (!battery_files_enabled()) return;   // tests: work/ stays untouched
    for (Unit& u : units) {
        std::string err;
        if (!u.tape.save(err)) LOGW("%s: %s", name.c_str(), err.c_str());
    }
}

void Rmp1::press_load(int i)
{
    Unit& u = units[i];
    if (!u.tape.loaded || u.loaded) return;
    u.loaded = true;   // the tension arms, the reel to the BOT marker
    u.pos = 0;
    u.next = 0;
}

void Rmp1::press_online(int i)
{
    Unit& u = units[i];
    u.online = u.loaded && !u.online;
}

void Rmp1::press_rewind(int i)
{
    Unit& u = units[i];
    if (u.loaded && !u.online && u.pos > 0) u.rewinding = true;
}

void Rmp1::press_unload(int i)
{
    Unit& u = units[i];
    if (!u.loaded || u.online) return;
    if (u.pos > 0) {
        u.rewinding = true;
        u.unload_after = true;
    } else u.loaded = false;
}

void Rmp1::reset()
{
    ppi_data.reset();
    ppi_ctl.reset();
    pon = true;   // D7A: no motion commands until RPON
    gate1_hold = 0;
    pit.set_gate(1, false);
    for (Unit& u : units) {
        u.writing = false;
        u.dir = 0;
        u.vel = 0;
        u.acc = 0;
    }
}

int Rmp1::selected(void) const
{
    uint8_t s = ppi_ctl.out_c & 7;   // S0-S2, one of them
    if (s == 1) return units[0].fitted ? 0 : -1;
    if (s == 2) return units[1].fitted ? 1 : -1;
    if (s == 4) return units[2].fitted ? 2 : -1;
    return -1;
}

void Rmp1::read_row(Unit& u, uint16_t ch)
{
    pae = !parity9(ch);          // 74180: the rows have odd parity
    if (ch & 0x100) lpp = !lpp;  // D5C: the longitudinal parity of the parity track
    ppi_data.strobe_b((uint8_t)ch);   // D4C: STB of port B
    pit.set_gate(1, true);       // and the data clock of counter 1
    gate1_hold = 2;
}

// One PHI2 clock: the counters (G0 = TE, G1 = the data clock, G2 = OUT1), the write strobe on the rising
// edge of OUT0, the drives.
void Rmp1::step(void)
{
    uint8_t cmd = ppi_ctl.out_a;
    bool te = (cmd & 0x20) != 0, we = (cmd & 0x80) != 0, re = (cmd & 0x40) != 0;
    if (re && !re_prev) lpp = false;
    re_prev = re;
    pit.set_gate(0, te);
    if (gate1_hold > 0 && --gate1_hold == 0) pit.set_gate(1, false);
    pit.tick(0);
    pit.tick(1);
    pit.tick(2);
    pit.set_gate(2, pit.out(1));
    bool out0 = pit.out(0);
    int s = selected();
    if (out0 && !out0_prev && te && we && ppi_data.a_output_full()) {   // D7C: WDS or WARS, then /ACK
        uint8_t d = ppi_data.ack_a();
        if (s >= 0 && units[s].writing) {
            Unit& u = units[s];
            uint16_t ch;
            if (ppi_data.out_c & 0x20) {   // WDSE: WDS with the parity WDP
                ch = (uint16_t)(d | ((ppi_data.out_c & 0x10) ? 0x100 : 0));
                u.wff ^= ch;
            } else {                       // WARS: the write flip-flops to zero = the LRC row
                ch = u.wff;
                u.wff = 0;
            }
            if (ch) {
                u.tape.rows.push_back({ u.pos + HEAD_GAP, ch });
                u.tape.dirty = true;
            }
        }
    }
    out0_prev = out0;
    for (int i = 0; i < UNITS; i++) {
        Unit& u = units[i];
        if (u.rewinding) {
            u.pos -= REWIND_STEP;
            u.dir = -1;
            u.vel = u.acc = 0;   // rewinding: the start and stop are not modelled
            if (u.pos <= 0) {
                u.pos = 0;
                u.next = 0;
                u.rewinding = false;
                u.dir = 0;
                if (u.unload_after) {
                    u.unload_after = false;
                    u.loaded = false;
                }
            }
            continue;
        }
        if (i != s) {
            u.dir = 0;
            u.vel = 0;
            u.acc = 0;
            u.writing = false;
        }
    }
    if (s < 0) return;
    Unit& u = units[s];
    if (u.rewinding) return;
    bool sfc = (cmd & 0x01) != 0, src = (cmd & 0x02) != 0, sws = (cmd & 0x04) != 0;
    bool can = !pon && u.tape.loaded && u.loaded && u.online;
    int dir = 0;
    if (can && sfc && !src && u.pos < MagTape::END_POS) dir = 1;
    else if (can && src && !sfc && u.pos > 0) dir = -1;
    bool writing = dir == 1 && sws && u.ring;
    if (writing && !u.writing) {   // the write current on: the tape from the write head on is erased
        u.tape.erase_from(u.pos + HEAD_GAP);
        u.wff = 0;
        if (u.next > u.tape.rows.size()) u.next = u.tape.rows.size();
    }
    u.writing = writing;
    int32_t target = dir * FULL;
    if (u.vel < target) u.vel += ACCEL;
    else if (u.vel > target) u.vel -= ACCEL;
    u.acc += u.vel;
    int move = 0;
    if (u.acc >= FULL) {
        u.acc -= FULL;
        move = 1;
    } else if (u.acc <= -FULL) {
        u.acc += FULL;
        move = -1;
    }
    if (move < 0 && u.pos <= 0) {   // the drive stops on the BOT marker
        move = 0;
        u.vel = 0;
        u.acc = 0;
    }
    u.dir = u.vel > 0 ? 1 : u.vel < 0 ? -1 : 0;
    std::vector<MagTape::Row>& rows = u.tape.rows;
    if (move > 0) {
        u.pos++;
        while (u.next < rows.size() && rows[u.next].pos <= u.pos) {
            if (re) read_row(u, rows[u.next].ch);
            u.next++;
        }
    } else if (move < 0) {
        u.pos--;
        while (u.next > 0 && rows[u.next - 1].pos > u.pos) {
            u.next--;
            if (re) read_row(u, rows[u.next].ch);
        }
    }
}

void Rmp1::sync(uint64_t now)
{
    if (!bus || now <= synced) return;
    phi2_per_cycle = 2000000.0 / (double)bus->cpu_hz;   // PHI2 of the bus is 2 MHz
    double clocks = (double)(now - synced) * phi2_per_cycle + frac;
    uint64_t n = (uint64_t)clocks;
    frac = clocks - (double)n;
    synced = now;
    // quiet: no tape moves past the heads, no pulse pending, no byte waiting for the write strobe: the
    // counters and the rewinding reels at once
    bool moving = false;
    int s = selected();
    if (s >= 0 && units[s].dir != 0) moving = true;
    uint8_t cmd = ppi_ctl.out_a;
    bool may_move = s >= 0 && (cmd & 0x03) && !pon;
    bool may_write = (cmd & 0xA0) == 0xA0 && ppi_data.a_output_full();
    if (!moving && !may_move && !may_write && gate1_hold == 0 && n > 4) {
        pit.set_gate(0, (cmd & 0x20) != 0);
        pit.ticks(0, n);
        pit.ticks(1, n);
        pit.set_gate(2, pit.out(1));
        pit.ticks(2, n);
        out0_prev = pit.out(0);
        for (Unit& u : units) {
            if (!u.rewinding) continue;
            u.pos -= REWIND_STEP * (int64_t)n;
            u.dir = -1;
            u.vel = u.acc = 0;
            if (u.pos <= 0) {
                u.pos = 0;
                u.next = 0;
                u.rewinding = false;
                u.dir = 0;
                if (u.unload_after) {
                    u.unload_after = false;
                    u.loaded = false;
                }
            }
        }
        phi2 += n;
        return;
    }
    for (uint64_t i = 0; i < n; i++) step();
    phi2 += n;
}

void Rmp1::advance(uint64_t now)
{
    sync(now);
}

uint8_t Rmp1::pins_b()
{
    uint8_t b = pon ? 0x40 : 0;
    int s = selected();
    if (s < 0) return b;   // no unit: the status lines stay inactive
    const Unit& u = units[s];
    if (u.tape.loaded && u.loaded && !u.rewinding) b |= 0x20;   // RDY
    if (u.tape.loaded && !u.ring) b |= 0x10;                    // WPT
    if (bot(u)) b |= 0x08;
    if (u.online) b |= 0x04;
    if (u.rewinding) b |= 0x02;
    if (eot(u)) b |= 0x01;
    return b;
}

uint8_t Rmp1::pins_c()
{
    return (uint8_t)((pit.out(2) ? 0x80 : 0) | (pit.out(0) ? 0x40 : 0) | (lpp ? 0x20 : 0) | (pae ? 0x10 : 0) | 0x0F);
}

void Rmp1::outputs_changed(int port, uint8_t value)
{
    if (port == 2) {
        if (value & 0x08) pon = false;   // RPON
        return;
    }
    if (port != 0) return;
    uint8_t rise = (uint8_t)(value & ~cmd_prev);
    cmd_prev = value;
    int s = selected();
    if (s < 0 || pon) return;
    Unit& u = units[s];
    if ((rise & 0x08) && u.loaded && u.online && u.pos > 0) u.rewinding = true;   // RWC
    if (rise & 0x10) u.online = false;                                            // OFC
}

bool Rmp1::io_read(uint8_t port, uint8_t& data)
{
    if ((port & 0xF0) != 0xC0 || (port & 0x0F) > 0x0B) return false;
    sync(bus->now);
    int r = port & 0x0F;
    if (r < 4) data = ppi_data.read(r);
    else if (r < 8) data = ppi_ctl.read(r - 4);
    else data = pit.read(r - 8);
    return true;
}

bool Rmp1::io_write(uint8_t port, uint8_t data)
{
    if ((port & 0xF0) != 0xC0 || (port & 0x0F) > 0x0B) return false;
    sync(bus->now);
    int r = port & 0x0F;
    if (r < 4) ppi_data.write(r, data);
    else if (r < 8) ppi_ctl.write(r - 4, data);
    else pit.write(r - 8, data);
    return true;
}

int Rmp1::decode_io(uint8_t port, const char** what)
{
    if ((port & 0xF0) != 0xC0 || (port & 0x0F) > 0x0B) return 0;
    static const char* const names[12] = {
        "write data (D1B PA)", "read data (D1B PB)", "data status (D1B PC)", "D1B control",
        "drive commands (D2B PA)", "drive status (D2B PB)", "unit select, OUT2 OUT0 LPP PAE (D2B PC)", "D2B control",
        "8253 counter 0", "8253 counter 1", "8253 counter 2", "8253 control",
    };
    *what = names[port & 0x0F];
    return DEC_READ | DEC_WRITE;
}

void Rmp1::describe(json& out)
{
    if (bus) sync(bus->now);
    json us = json::array();
    for (int i = 0; i < UNITS; i++) {
        const Unit& u = units[i];
        if (!u.fitted) continue;
        json j;
        j["unit"] = i;
        j["tape"] = u.tape.loaded ? data_relative(u.tape.path) : "";
        j["loaded"] = u.loaded;
        j["online"] = u.online;
        j["write_ring"] = u.ring;
        j["rewinding"] = u.rewinding;
        j["bot"] = bot(u);
        j["eot"] = eot(u);
        j["position_m"] = (double)u.pos / MagTape::UNIT * 0.0254;
        if (u.tape.loaded) j["records"] = (int)u.tape.records().size();
        us.push_back(j);
    }
    out["units"] = us;
    out["selected_unit"] = selected();
    out["pon"] = pon;
}

std::string Rmp1::live_status()
{
    int s = selected();
    const Unit& u = units[s >= 0 ? s : 0];
    if (!u.fitted) return std::string();
    char b[96];
    if (!u.tape.loaded) snprintf(b, sizeof(b), "RMP-1: bez pásky");
    else snprintf(b, sizeof(b), "RMP-1: %.1f m%s", (double)u.pos / MagTape::UNIT * 0.0254,
                  u.rewinding ? " <<" : u.dir > 0 ? " >" : u.dir < 0 ? " <" : "");
    return b;
}

void Rmp1::state(StateIO& io)
{
    // the tapes are referred to (files), not stored: written to their files first
    if (!io.loading) flush();
    for (int i = 0; i < UNITS; i++) {
        Unit& u = units[i];
        io.media("tap", u.tape.path);
        std::string img = u.tape.image;
        io.v(img);
        if (io.loading && io.ok() && img != u.tape.image) {
            std::string err;
            if (img.empty()) eject(i);
            else if (!insert(i, img, err)) LOGW("state: RMP-1 tape %s: %s", img.c_str(), err.c_str());
        }
        io.v(u.ring);
        io.v(u.online);
        io.v(u.loaded);
        io.v(u.rewinding);
        io.v(u.unload_after);
        io.v(u.pos);
        io.e(u.dir);
        io.v(u.vel);
        io.v(u.acc);
        io.v(u.writing);
        io.v(u.wff);
        if (io.loading) {   // the read head index again
            u.next = 0;
            while (u.next < u.tape.rows.size() && u.tape.rows[u.next].pos <= u.pos) u.next++;
        }
    }
    ppi_data.state(io);
    ppi_ctl.state(io);
    pit.state(io);
    io.v(pon);
    io.v(phi2);
    io.v(synced);
    io.v(frac);
    io.v(out0_prev);
    io.e(gate1_hold);
    io.v(pae);
    io.v(lpp);
    io.v(re_prev);
    io.v(cmd_prev);
}
