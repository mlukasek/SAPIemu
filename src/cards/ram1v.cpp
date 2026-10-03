#include "cards/ram1v.h"
#include "core/bus.h"

#include <stdio.h>
#include <time.h>

using nlohmann::json;

static bool parse_cond(const json& cfg, const char* key, int& out, const char* def, std::string& err)
{
    std::string s;
    if (!cfg_string(cfg, key, s, def, err)) return false;
    if (s == "ignore") out = 0;
    else if (s == "h") out = 1;
    else if (s == "l") out = 2;
    else if (s == "off") out = 3;
    else { err = std::string(key) + " must be ignore, h, l or off"; return false; }
    return true;
}

bool Ram1v::configure(const json& cfg, std::string& err)
{
    if (!cfg_bool(cfg, "sram_a", fitted[0], true, err)) return false;
    if (!cfg_bool(cfg, "sram_b", fitted[1], true, err)) return false;
    std::string c;
    if (!cfg_string(cfg, "sram_c", c, "none", err)) return false;
    if (c != "none" && c != "sram" && c != "m48t35") { err = "sram_c must be none, sram or m48t35"; return false; }
    fitted[2] = c != "none";
    c_rtc = c == "m48t35";
    std::string s4;
    if (!cfg_string(cfg, "c_select", s4, "never", err)) return false;
    if (s4 == "never") c_select = 0;
    else if (s4 == "map1") c_select = 1;
    else if (s4 == "map2") c_select = 2;
    else { err = "c_select must be never, map1 or map2 (S4 CONF)"; return false; }

    long long v;
    if (!parse_cond(cfg, "hole16_map1", hole16_cond1, "h", err)) return false;
    if (!parse_cond(cfg, "hole16_map2", hole16_cond2, "ignore", err)) return false;
    if (!cfg_number(cfg, "hole16_base", v, 0xC000, err)) return false;
    if (v < 0 || v > 0xC000 || (v & 0x3FFF)) { err = "hole16_base must be 0000, 4000, 8000 or C000 (S5)"; return false; }
    hole16_base = (uint16_t)v;
    if (!parse_cond(cfg, "hole2_map1", hole2_cond1, "l", err)) return false;
    if (!cfg_number(cfg, "hole2_base", v, 0xF800, err)) return false;
    if (v < 0 || v > 0xF800 || (v & 0x7FF)) { err = "hole2_base must be a multiple of 800h (S6)"; return false; }
    hole2_base = (uint16_t)v;

    if (!cfg_number(cfg, "map_port", v, 0x63, err)) return false;
    if (v < 0 || v > 0xFF) { err = "map_port must be 00h-FFh (S3)"; return false; }
    map_port = (uint8_t)v;
    static const char* const en[2] = { "map1_enable", "map2_enable" };
    static const char* const bit[2] = { "map1_bit", "map2_bit" };
    static const int defbit[2] = { 6, 7 };   // like ZRMD-1: 63h bit 6 MAP1, bit 7 MAP2
    for (int i = 0; i < 2; i++) {
        if (!cfg_bool(cfg, en[i], map_en[i], true, err)) return false;
        if (!cfg_number(cfg, bit[i], v, defbit[i], err)) return false;
        if (v < 0 || v > 7) { err = std::string(bit[i]) + " must be 0-7 (B1/B2/B4)"; return false; }
        map_bit[i] = (int)v;
    }
    for (int i = 0; i < 3; i++) mem[i].assign(fitted[i] ? 0x8000 : 0, 0x00);
    return true;
}

void Ram1v::reset()
{
    map_out = 0;   // /RES clears both flip-flops
}

int Ram1v::chip(uint16_t a, int map) const
{
    bool m1 = (map & MAP_1) != 0, m2 = (map & MAP_2) != 0;
    // 16 KB hole: both MAP conditions; "off" disables the hole
    if (hole16_cond1 != C_OFF && hole16_cond2 != C_OFF && (a & 0xC000) == hole16_base &&
        cond(hole16_cond1, m1) && cond(hole16_cond2, m2))
        return -1;
    if (hole2_cond1 != C_OFF && (a & 0xF800) == hole2_base && cond(hole2_cond1, m1)) return -1;
    if (a & 0x8000) return fitted[1] ? 1 : -1;
    bool use_c = (c_select == 1 && m1) || (c_select == 2 && m2);
    if (use_c) return fitted[2] ? 2 : -1;
    return fitted[0] ? 0 : -1;
}

// M48T35: 7FF8 control, 7FF9 seconds, 7FFA minutes, 7FFB hours, 7FFC day,
// 7FFD date, 7FFE month, 7FFF year (BCD), from the host clock
uint8_t Ram1v::rtc_read(unsigned reg)
{
    time_t t = time(nullptr);
    struct tm lt;
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    auto bcd = [](int x) { return (uint8_t)((x / 10) << 4 | (x % 10)); };
    switch (reg) {
    case 0: return rtc_ctrl;
    case 1: return bcd(lt.tm_sec);
    case 2: return bcd(lt.tm_min);
    case 3: return bcd(lt.tm_hour);
    case 4: return bcd(lt.tm_wday ? lt.tm_wday : 7);
    case 5: return bcd(lt.tm_mday);
    case 6: return bcd(lt.tm_mon + 1);
    default: return bcd(lt.tm_year % 100);
    }
}

bool Ram1v::mem_read(uint16_t a, uint8_t& data)
{
    int c = chip(a, bus->map);
    if (c < 0) return false;
    if (c == 2 && c_rtc && (a & 0x7FFF) >= 0x7FF8) data = rtc_read(a & 7u);
    else data = mem[c][a & 0x7FFF];
    return true;
}

bool Ram1v::mem_write(uint16_t a, uint8_t data)
{
    int c = chip(a, bus->map);
    if (c < 0) return false;
    if (c == 2 && c_rtc && (a & 0x7FFF) >= 0x7FF8) {
        if ((a & 7u) == 0) rtc_ctrl = data & 0xC0;   // W/R bits; the clock follows the host
        return true;
    }
    mem[c][a & 0x7FFF] = data;
    return true;
}

bool Ram1v::mem_peek(uint16_t a, uint8_t& data)
{
    int c = chip(a, bus->map);
    if (c < 0) return false;
    data = mem[c][a & 0x7FFF];
    return true;
}

bool Ram1v::io_write(uint8_t port, uint8_t data)
{
    if (port != map_port) return false;
    map_out = ((data >> map_bit[0]) & 1 ? MAP_1 : 0) | ((data >> map_bit[1]) & 1 ? MAP_2 : 0);
    return true;
}

int Ram1v::map_lines()
{
    return (map_en[0] ? (map_out & MAP_1) : 0) | (map_en[1] ? (map_out & MAP_2) : 0);
}

int Ram1v::decode_mem(uint16_t a, int map, const char** what)
{
    int c = chip(a, map);
    if (c < 0) return 0;
    *what = c == 0 ? "SRAM A" : c == 1 ? "SRAM B" : (c_rtc ? "SRAM C (M48T35)" : "SRAM C");
    return DEC_READ | DEC_WRITE;
}

int Ram1v::decode_io(uint8_t port, const char** what)
{
    if (port != map_port) return 0;
    *what = "MAP register";
    return DEC_WRITE;
}

void Ram1v::describe(json& out)
{
    char b[8];
    snprintf(b, sizeof(b), "%02Xh", map_port);
    out["map_port"] = b;
    out["map1"] = (map_out & MAP_1) != 0;
    out["map2"] = (map_out & MAP_2) != 0;
    out["sram"] = { fitted[0], fitted[1], fitted[2] };
}
