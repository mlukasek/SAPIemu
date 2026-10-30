#include "chips/eprom27.h"
#include "core/util.h"

int Eprom27::type_of_size(size_t n)
{
    if (n == 2048) return T2716;
    if (n == 4096) return T2732;
    return NONE;
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
        err = file + ": an EPROM image has 2048 B (2716) or 4096 B (2732), this one " + std::to_string(d.size()) + " B";
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
        if (p.p21 == EPL_L) return EPM_NO_VPP;
        bool vpp = p.p21 == EPL_VPP;
        if (p.p18 != EPL_L) {   // PGM high: the pulse with /OE high and 25 V, else standby
            return vpp && p.p20 != EPL_L ? EPM_PROGRAM : EPM_STANDBY;
        }
        return p.p20 == EPL_L ? EPM_READ : EPM_OUTPUT_OFF;   // read / program verify, output off / inhibit
    }
    if (type == T2732) {
        if (p.p18 != EPL_L) return EPM_STANDBY;
        if (p.p20 == EPL_VPP) return EPM_PROGRAM;
        return p.p20 == EPL_L ? EPM_READ : EPM_OUTPUT_OFF;
    }
    return EPM_EMPTY;
}

uint16_t Eprom27::cell(const EpromPins& p) const
{
    uint16_t a = p.a & 0x7FF;
    if (type == T2732 && p.p21 != EPL_L) a |= 0x800;
    return a;
}

bool Eprom27::output(const EpromPins& p, uint8_t& d) const
{
    if (mode(p) != EPM_READ) return false;
    d = data[cell(p)];
    return true;
}

int Eprom27::overvoltage(const EpromPins& p) const
{
    if (!loaded()) return 0;
    if (p.p18 == EPL_VPP) return 18;
    if (type == T2716 && p.p20 == EPL_VPP) return 20;
    if (type == T2732 && p.p21 == EPL_VPP) return 21;
    return 0;
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
