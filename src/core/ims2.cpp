#include "core/ims2.h"
#include <stdio.h>
#include "core/state.h"

void Ims2Bus::attach(Ims2Node* n)
{
    for (Ims2Node* x : nodes)
        if (x == n) return;
    n->ims = this;
    nodes.push_back(n);
    settle(t);
}

void Ims2Bus::detach(Ims2Node* n)
{
    for (size_t i = 0; i < nodes.size(); i++)
        if (nodes[i] == n) {
            nodes.erase(nodes.begin() + (long)i);
            n->ims = nullptr;
            n->dio = n->ctl = 0;
            settle(t);
            return;
        }
}

void Ims2Bus::settle(uint64_t at)
{
    if (at > t) t = at;
    if (settling) return;   // a node changed its drives while reacting: the loop below sees it
    settling = true;
    for (int round = 0; round < 32; round++) {
        uint8_t d = 0, c = 0;
        for (Ims2Node* n : nodes) {
            d |= n->dio;
            c |= n->ctl;
        }
        if (d == dio_lines && c == ctl_lines && round > 0) break;
        uint8_t old = ctl_lines;
        dio_lines = d;
        ctl_lines = c;
        record(old);
        for (Ims2Node* n : nodes) n->lines_changed(t);
    }
    settling = false;
}

void Ims2Bus::advance(uint64_t now)
{
    for (int guard = 0; guard < 100000; guard++) {
        Ims2Node* first = nullptr;
        uint64_t when = UINT64_MAX;
        for (Ims2Node* n : nodes) {
            uint64_t e = n->next_event();
            if (e < when) {
                when = e;
                first = n;
            }
        }
        if (!first || when > now) break;
        if (when > t) t = when;
        first->on_event(t);
        settle(t);
    }
    if (now > t) t = now;
}

void Ims2Bus::record(uint8_t old_ctl)
{
    Ims2Event e;
    e.t = t;
    if ((ctl_lines & IMS_DAV) && !(old_ctl & IMS_DAV)) {
        e.byte = dio_lines;
        e.lines = ctl_lines & (IMS_ATN | IMS_EOI);
        for (Ims2Node* n : nodes)
            if (n->ctl & IMS_DAV) e.from = n->node_name();
        transfers++;
    } else if ((ctl_lines ^ old_ctl) & (IMS_IFC | IMS_REN)) {
        e.transfer = false;
        e.lines = ctl_lines & (IMS_IFC | IMS_REN);
        e.byte = (ctl_lines ^ old_ctl) & (IMS_IFC | IMS_REN);   // which changed
    } else {
        return;
    }
    if (log.size() < LOG_SIZE) log.push_back(e);
    else log[log_next] = e;
    log_next = (log_next + 1) % LOG_SIZE;
}

void Ims2Bus::log_copy(std::vector<Ims2Event>& out) const
{
    out.clear();
    if (log.size() < LOG_SIZE) {
        out = log;
        return;
    }
    for (size_t i = 0; i < LOG_SIZE; i++) out.push_back(log[(log_next + i) % LOG_SIZE]);
}

void Ims2Bus::clear_log(void)
{
    log.clear();
    log_next = 0;
}

void Ims2Bus::state(StateIO& io)
{
    io.v(dio_lines);
    io.v(ctl_lines);
    io.v(t);
    io.v(transfers);
    // the monitor log, oldest first
    std::vector<Ims2Event> l;
    if (!io.loading) log_copy(l);
    uint32_t n = (uint32_t)l.size();
    io.v(n);
    if (io.loading) l.resize(n > LOG_SIZE ? LOG_SIZE : n);
    for (Ims2Event& e : l) {
        io.v(e.t);
        io.v(e.byte);
        io.v(e.lines);
        io.v(e.transfer);
        io.v(e.from);
    }
    if (io.loading) {
        log = l;
        log_next = log.size() % LOG_SIZE;
    }
}

std::string ims2_command_name(uint8_t b)
{
    char s[24];
    b &= 0x7F;
    switch (b) {
    case 0x01: return "GTL";
    case 0x04: return "SDC";
    case 0x05: return "PPC";
    case 0x08: return "GET";
    case 0x09: return "TCT";
    case 0x11: return "LLO";
    case 0x14: return "DCL";
    case 0x15: return "PPU";
    case 0x18: return "SPE";
    case 0x19: return "SPD";
    case 0x3F: return "UNL";
    case 0x5F: return "UNT";
    }
    if (b >= 0x20 && b < 0x3F) { snprintf(s, sizeof s, "LAD %d", b - 0x20); return s; }
    if (b >= 0x40 && b < 0x5F) { snprintf(s, sizeof s, "TAD %d", b - 0x40); return s; }
    if (b >= 0x60) { snprintf(s, sizeof s, "SA %d", b - 0x60); return s; }
    snprintf(s, sizeof s, "%02Xh", b);
    return s;
}
