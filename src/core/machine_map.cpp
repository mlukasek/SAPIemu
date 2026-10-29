#include "core/machine_map.h"
#include "cards/catalog.h"
#include "cards/placeholder.h"
#include "core/bus.h"
#include "core/card.h"
#include "core/log.h"
#include "core/machine.h"
#include "core/util.h"

#include <stdio.h>
#include <memory>

using nlohmann::json;

namespace {

struct Probe {
    std::unique_ptr<Card> card;
    int declared_base = -1;   // not emulated card with a port set in the configuration
};

// Close the current range and start a new one when the users change.
void add_point(std::vector<MapRange>& out, int addr, const std::vector<MapUser>& users)
{
    if (!out.empty() && out.back().hi == addr - 1 && out.back().users == users) {
        out.back().hi = addr;
        return;
    }
    if (users.empty()) return;
    MapRange r;
    r.lo = r.hi = addr;
    r.users = users;
    out.push_back(r);
}

void classify(std::vector<MapRange>& ranges)
{
    for (MapRange& r : ranges) {
        int readers = 0, writers = 0;
        for (const MapUser& u : r.users) {
            if (u.flags & DEC_ONCARD) continue;   // the CPU card's own EPROM is not on the bus
            if (u.flags & DEC_READ) readers++;
            if (u.flags & DEC_WRITE) writers++;
        }
        r.conflict = readers > 1;
        r.shared_write = !r.conflict && writers > 1;
    }
}

// Shared writes of two cards the catalog knows as meant so (card_shared_write): information, not a problem.
void mark_by_design(std::vector<MapRange>& ranges, const std::vector<MapCard>& cards)
{
    for (MapRange& r : ranges) {
        if (!r.shared_write) continue;
        const MapUser* w[2] = { nullptr, nullptr };
        int n = 0;
        for (const MapUser& u : r.users) {   // the writers on the bus, as classify counts them
            if ((u.flags & DEC_ONCARD) || !(u.flags & DEC_WRITE)) continue;
            if (n < 2) w[n] = &u;
            n++;
        }
        if (n != 2) continue;
        r.by_design = card_shared_write(cards[(size_t)w[0]->card].type, w[0]->what, cards[(size_t)w[1]->card].type,
                                        w[1]->what);
    }
}

std::string hex_range(int lo, int hi, int digits)
{
    char buf[32];
    if (lo == hi) snprintf(buf, sizeof(buf), "%0*X", digits, lo);
    else snprintf(buf, sizeof(buf), "%0*X-%0*X", digits, lo, digits, hi);
    return buf;
}

} // namespace

void machine_map_build(const json& cfg, MachineMap& out)
{
    out = MachineMap();
    std::unique_ptr<Bus> bus(new Bus());
    std::vector<Probe> probes;
    if (cfg.contains("cards") && cfg["cards"].is_array()) {
        for (const json& cc : cfg["cards"]) {
            std::string type = cc.value("type", std::string());
            const CardInfo* ci = card_info(type);
            MapCard mc;
            mc.name = card_display_name(cc);
            mc.type = type;
            mc.slot = cc.value("slot", 0);
            Probe p;
            p.card.reset(card_create(type));
            if (!p.card) {
                mc.known = false;
                p.card.reset(new PlaceholderCard(type));
                // a serial card set up in the configurator: its four ports like DMR-1
                std::string port = card_port_label(cc);
                if (ci && !port.empty()) {
                    long long v;
                    if (parse_number(port.substr(1) + "h", v)) {
                        p.declared_base = (int)v;
                        mc.declared = true;
                    }
                }
            } else {
                p.card->bus = bus.get();
                p.card->name = mc.name;
                p.card->slot = mc.slot;
                p.card->probe = true;
                std::string err;
                if (!p.card->configure(cc, err)) {
                    mc.known = false;
                    mc.error = err;
                }
            }
            out.cards.push_back(mc);
            probes.push_back(std::move(p));
        }
    }

    // I/O ports
    std::vector<MapUser> users;
    for (int port = 0; port < 256; port++) {
        users.clear();
        for (size_t i = 0; i < probes.size(); i++) {
            const char* what = "";
            int f = 0;
            if (out.cards[i].known) {
                f = probes[i].card->decode_io((uint8_t)port, &what);
            } else if (probes[i].declared_base >= 0 && (port & 0xFC) == probes[i].declared_base) {
                f = DEC_READ | DEC_WRITE;
                what = "(not emulated)";
            }
            if (f) users.push_back({ (int)i, f, what });
        }
        add_point(out.io, port, users);
    }
    classify(out.io);
    mark_by_design(out.io, out.cards);

    // memory, for every state of MAP1/MAP2
    for (int map = 0; map < 4; map++) {
        for (int addr = 0; addr < 65536; addr++) {
            users.clear();
            for (size_t i = 0; i < probes.size(); i++) {
                if (!out.cards[i].known) continue;
                const char* what = "";
                int f = probes[i].card->decode_mem((uint16_t)addr, map, &what);
                if (f) users.push_back({ (int)i, f, what });
            }
            add_point(out.mem[map], addr, users);
        }
        classify(out.mem[map]);
        mark_by_design(out.mem[map], out.cards);
    }
    for (int map = 1; map < 4; map++) {
        const std::vector<MapRange>& a = out.mem[0];
        const std::vector<MapRange>& b = out.mem[map];
        bool same = a.size() == b.size();
        for (size_t i = 0; same && i < a.size(); i++)
            same = a[i].lo == b[i].lo && a[i].hi == b[i].hi && a[i].users == b[i].users;
        if (!same) out.map_matters = true;
    }
}

std::string map_range_users(const MachineMap& m, const MapRange& r, MapWhatText what)
{
    std::string s;
    for (const MapUser& u : r.users) {
        if (!s.empty()) s += ", ";
        s += m.cards[(size_t)u.card].name;
        if (!u.what.empty()) s += std::string(" ") + (what ? what(u.what.c_str()) : u.what.c_str());
    }
    return s;
}

std::vector<CfgProblem> machine_map_problems(const MachineMap& m, MapWhatText cs_what)
{
    std::vector<CfgProblem> out;
    auto add = [&](int slot, bool error, const std::string& en, const std::string& cs) {
        for (const CfgProblem& p : out)
            if (p.en == en) return;
        out.push_back({ slot, error, en, cs });
    };
    for (const MapRange& r : m.io) {
        if (!r.conflict && (!r.shared_write || r.by_design)) continue;
        std::string who = map_range_users(m, r), who_cs = map_range_users(m, r, cs_what);
        int slot = m.cards[(size_t)r.users.back().card].slot;
        if (r.conflict)
            add(slot, true, "ports " + hex_range(r.lo, r.hi, 2) + "h answered by two cards: " + who,
                "porty " + hex_range(r.lo, r.hi, 2) + "h obsluhují dvě karty: " + who_cs);
        else
            add(slot, false, "writes to ports " + hex_range(r.lo, r.hi, 2) + "h go to two cards: " + who,
                "zápis na porty " + hex_range(r.lo, r.hi, 2) + "h jde do dvou karet: " + who_cs);
    }
    for (int map = 0; map < 4; map++) {
        char st[48] = "";
        if (m.map_matters) snprintf(st, sizeof(st), " (MAP1=%d MAP2=%d)", map & 1, (map >> 1) & 1);
        for (const MapRange& r : m.mem[map]) {
            if (!r.conflict && (!r.shared_write || r.by_design)) continue;
            std::string who = map_range_users(m, r), who_cs = map_range_users(m, r, cs_what);
            int slot = m.cards[(size_t)r.users.back().card].slot;
            if (r.conflict)
                add(slot, true, "memory " + hex_range(r.lo, r.hi, 4) + "h answered by two cards" + st + ": " + who,
                    "paměť " + hex_range(r.lo, r.hi, 4) + "h obsluhují dvě karty" + st + ": " + who_cs);
            else
                add(slot, false, "writes to memory " + hex_range(r.lo, r.hi, 4) + "h go to two cards" + st + ": " + who,
                    "zápis do paměti " + hex_range(r.lo, r.hi, 4) + "h jde do dvou karet" + st + ": " + who_cs);
        }
    }
    for (const MapCard& c : m.cards)
        if (!c.error.empty())
            add(c.slot, true, c.name + ": " + c.error, c.name + ": " + c.error);
    return out;
}

std::vector<CfgProblem> machine_all_problems(const json& cfg)
{
    std::vector<CfgProblem> out = machine_cfg_problems(cfg);
    MachineMap m;
    machine_map_build(cfg, m);
    std::vector<CfgProblem> mp = machine_map_problems(m);
    out.insert(out.end(), mp.begin(), mp.end());
    return out;
}
