#include "chips/eprom27.h"
#include "core/util.h"

// a logic input: 5 V, 25 V and +12 V are H, 0 V and -5 V L
static bool high(uint8_t lv)
{
    return lv == EPL_H || lv == EPL_VPP || lv == EPL_12;
}

int Eprom27::type_of_size(size_t n)
{
    if (n == 1024) return T2708;
    if (n == 2048) return T2716;
    if (n == 4096) return T2732;
    return NONE;
}

size_t Eprom27::size_of_type(int t)
{
    return t == T2708 ? 1024 : t == T2716 ? 2048 : t == T2732 ? 4096 : 0;
}

bool Eprom27::load(const std::string& file, std::string& err)
{
    std::vector<uint8_t> d;
    if (!file_read_all(file, d)) {
        err = "cannot read " + file;
        return false;
    }
    int t = type_of_size(d.size());
    if (t == NONE) {
        err = file + ": an EPROM image has 1024 B (2708), 2048 B (2716) or 4096 B (2732), this one " +
              std::to_string(d.size()) + " B";
        return false;
    }
    type = t;
    data.swap(d);
    path = file;
    dirty = false;
    return true;
}

bool Eprom27::save(std::string& err)
{
    if (!loaded() || !dirty || path.empty()) return true;
    make_dirs(path_dir(path));
    if (!file_write_safe(path, data.data(), data.size())) {
        err = "cannot write " + path;
        return false;
    }
    dirty = false;
    return true;
}

void Eprom27::unload(void)
{
    type = NONE;
    data.clear();
    path.clear();
    dirty = false;
}

int Eprom27::mode(const EpromPins& p) const
{
    if (type == T2716) {
        if (!high(p.p21)) return EPM_NO_VPP;
        bool vpp = p.p21 == EPL_VPP;
        if (high(p.p18)) {   // PGM high: the pulse with /OE high and 25 V, else standby
            return vpp && high(p.p20) ? EPM_PROGRAM : EPM_STANDBY;
        }
        return high(p.p20) ? EPM_OUTPUT_OFF : EPM_READ;   // output off / inhibit, read / program verify
    }
    if (type == T2732) {
        if (high(p.p18)) return EPM_STANDBY;
        if (p.p20 == EPL_VPP) return EPM_PROGRAM;
        return high(p.p20) ? EPM_OUTPUT_OFF : EPM_READ;
    }
    if (type == T2708) {
        if (p.p19 != EPL_12 || p.p21 != EPL_M5) return EPM_NO_SUPPLY;
        if (p.p20 == EPL_12) return p.p18 == EPL_VPP ? EPM_PROGRAM : EPM_OUTPUT_OFF;   // program / program inhibit
        if (high(p.p20)) return EPM_STANDBY;                                           // deselect
        return p.p18 == EPL_VPP ? EPM_OUTPUT_OFF : EPM_READ;   // 25 V with CS/WE L: not in the sheets (assumption)
    }
    return EPM_EMPTY;
}

uint16_t Eprom27::cell(const EpromPins& p) const
{
    uint16_t a = p.a & 0x3FF;
    if (type == T2708) return a;
    if (high(p.p19)) a |= 0x400;
    if (type == T2732 && high(p.p21)) a |= 0x800;
    return a;
}

bool Eprom27::output(const EpromPins& p, uint8_t& d) const
{
    if (mode(p) != EPM_READ) return false;
    d = data[cell(p)];
    return true;
}

unsigned Eprom27::overvoltage(const EpromPins& p) const
{
    unsigned o = 0;
    if (type == T2716 || type == T2732) {
        if (p.p18 == EPL_VPP) o |= EPO_25V_P18;
        if (p.p19 == EPL_12) o |= EPO_12V_A10;
        if (p.p21 == EPL_M5) o |= EPO_M5V_P21;
    }
    if (type == T2716) {
        if (p.p20 == EPL_VPP) o |= EPO_25V_OE;
        if (p.p20 == EPL_12) o |= EPO_12V_OE;
    }
    if (type == T2732 && p.p21 == EPL_VPP) o |= EPO_25V_A11;
    if (type == T2708) {
        if (p.p21 == EPL_H || p.p21 == EPL_VPP) o |= EPO_VBB_PLUS;
        if (p.p20 == EPL_VPP) o |= EPO_25V_CSWE;
    }
    return o;
}

bool Eprom27::program(uint16_t addr, uint8_t d)
{
    if (!loaded() || addr >= data.size()) return false;
    uint8_t v = data[addr] & d;
    if (v == data[addr]) return false;
    data[addr] = v;
    dirty = true;
    return true;
}
