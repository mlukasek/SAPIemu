#include "chips/i8272.h"
#include "chips/floppy.h"
#include "core/log.h"
#include "core/state.h"

#include <string.h>

// RQM is low for a while after each command or result byte (the chip's microcode)
static const uint32_t RQM_US = 12;
static const uint64_t NEVER = ~(uint64_t)0;

// ST0 interrupt codes
static const uint8_t IC_NORMAL = 0x00, IC_ABNORMAL = 0x40, IC_INVALID = 0x80, IC_READY = 0xC0;
static const uint8_t ST0_SE = 0x20, ST0_EC = 0x10, ST0_NR = 0x08;
static const uint8_t ST1_EN = 0x80, ST1_DE = 0x20, ST1_OR = 0x10, ST1_ND = 0x04, ST1_NW = 0x02, ST1_MA = 0x01;
static const uint8_t ST2_CM = 0x40, ST2_DD = 0x20, ST2_WC = 0x10, ST2_BC = 0x02, ST2_MD = 0x01;

static int command_length(uint8_t c)
{
    switch (c & 0x1F) {
    case 0x02: case 0x05: case 0x06: case 0x09: case 0x0C: case 0x11: case 0x19: case 0x1D: return 9;
    case 0x03: case 0x0F: return 3;
    case 0x04: case 0x07: case 0x0A: return 2;
    case 0x0D: return 6;
    default: return 1;   // SENSE INTERRUPT STATUS, invalid
    }
}

FloppyTrack* I8272::trk(void)
{
    FloppyDrive* d = drive(unit);
    return d && d->present() ? d->track_at(d->track, head) : nullptr;
}

uint64_t I8272::byte_cyc(void)
{
    int bu;
    if (phase == PH_EXEC && op == OP_FORMAT) {
        bu = 8000 / (fmt_kbps > 0 ? fmt_kbps : 250);   // FORMAT writes with the controller's rate
    } else if (FloppyTrack* t = trk()) {
        bu = t->byte_us;                                // the track's own byte time
    } else {
        int k = host ? host->fdc_kbps(unit, mfm) : 250;   // no track: index pulses only
        bu = 8000 / (k > 0 ? k : 250);
    }
    uint64_t c = ((uint64_t)hz * (uint64_t)bu + 500000) / 1000000;
    return c ? c : 1;
}

void I8272::lay(int& track_len, int& first, int& sector_len, int& id_mark)
{
    FloppyDrive* d = drive(unit);
    FloppyTrack t;
    if (phase == PH_EXEC && op == OP_FORMAT) {   // the track being written
        t.mfm = mfm;
        t.kbps = fmt_kbps;
        t.size = n_ <= 6 ? 128 << n_ : 8192;
        t.sec.resize(eot ? eot : 1);
        if (d) d->layout(t);
    } else if (FloppyTrack* p = trk()) {
        t = *p;
    } else {
        t.kbps = host ? host->fdc_kbps(unit, mfm) : 250;
        t.mfm = mfm;
        if (d) d->layout(t);
    }
    track_len = t.track_len;
    first = t.first_sector;
    sector_len = t.sector_len;
    id_mark = t.id_mark;
}

void I8272::reset(uint64_t now)
{
    phase = PH_CMD;
    cmd_n = cmd_len = 0;
    res_n = res_pos = 0;
    int_result = int_byte = false;
    hd_loaded = false;
    for (int u = 0; u < 4; u++) {
        seeking[u] = recal[u] = false;
        last_ready[u] = false;   // the polling reports the ready drives (ready changed)
        pcn[u] = 0;
    }
    seek_busy = 0;
    for (int u = 0; u < 4; u++) pend[u] = false;
    ex = X_NONE;
    nd_full = false;
    tc = overrun = false;
    rqm_at = now + us(RQM_US);
}

bool I8272::has_index(void)
{
    FloppyDrive* d = drive(unit);
    return d && d->present();
}

bool I8272::ids_readable(void)
{
    FloppyTrack* t = trk();
    return t && !t->sec.empty() && t->mfm == mfm && host->fdc_kbps(unit, mfm) == t->kbps;
}

uint8_t I8272::msr(uint64_t now) const
{
    uint8_t v = (uint8_t)(seek_busy & 0x0F);
    if (phase != PH_CMD) v |= 0x10;
    if (now < rqm_at) return v;
    switch (phase) {
    case PH_CMD: v |= 0x80; break;
    case PH_RESULT: v |= 0xC0; break;
    case PH_EXEC:
        if (nd && ex == X_DATA) {
            v |= 0x20;
            if (op == OP_READ || op == OP_READ_TRACK) {
                if (nd_full) v |= 0xC0;
            } else if (!nd_full) {
                v |= 0x80;
            }
        } else if (nd && ex == X_FMT_ID) {
            v |= 0x20;
            if (!nd_full) v |= 0x80;
        }
        break;
    }
    return v;
}

uint8_t I8272::read(int a0, uint64_t now)
{
    advance(now);
    if (!a0) return msr(now);
    if (phase == PH_RESULT) {
        uint8_t v = res[res_pos++];
        int_result = false;
        if (res_pos >= res_n) phase = PH_CMD;
        rqm_at = now + us(RQM_US);
        return v;
    }
    if (phase == PH_EXEC && nd) {
        nd_full = false;
        int_byte = false;
        return nd_byte;
    }
    return 0xFF;
}

void I8272::write(int a0, uint8_t value, uint64_t now)
{
    advance(now);
    if (!a0) return;
    if (phase == PH_EXEC && nd) {   // a data byte for WRITE DATA / FORMAT in non-DMA mode
        nd_byte = value;
        nd_full = true;
        int_byte = false;
        return;
    }
    if (phase != PH_CMD) return;
    if (cmd_n == 0) cmd_len = command_length(value);
    cmd[cmd_n++] = value;
    rqm_at = now + us(RQM_US);
    if (cmd_n >= cmd_len) {
        cmd_n = 0;
        command(now);
    }
}

void I8272::result(const uint8_t* r, int n, bool irq, uint64_t now)
{
    memcpy(res, r, (size_t)n);
    res_n = n;
    res_pos = 0;
    phase = PH_RESULT;
    int_result = irq;
    ex = X_NONE;
    rqm_at = now + us(RQM_US);
}

void I8272::set_int(uint8_t st0, uint8_t p)
{
    int u = st0 & 3;
    pend[u] = true;
    pend_st0[u] = st0;
    pend_pcn[u] = p;
}

void I8272::poll_ready(void)
{
    for (int u = 0; u < 4; u++) {
        bool r = ready(u);
        if (r == last_ready[u]) continue;
        last_ready[u] = r;
        set_int((uint8_t)(IC_READY | (r ? 0 : ST0_NR) | u), pcn[u]);
    }
}

void I8272::command(uint64_t now)
{
    uint8_t c0 = cmd[0];
    int u = cmd[1] & 3;
    switch (c0 & 0x1F) {
    case 0x03:   // SPECIFY
        srt = (uint8_t)(cmd[1] >> 4);
        hut = (uint8_t)(cmd[1] & 15);
        hlt = (uint8_t)(cmd[2] >> 1);
        nd = (cmd[2] & 1) != 0;
        return;
    case 0x04: {   // SENSE DRIVE STATUS
        FloppyDrive* d = drive(u);
        uint8_t st3 = (uint8_t)(cmd[1] & 7);
        if (ready(u)) st3 |= 0x20;
        if (d && d->track == 0) st3 |= 0x10;
        if (d && d->present() && d->write_protect) st3 |= 0x40;
        if (d && d->present() && d->heads > 1) st3 |= 0x08;   // TWO SIDE
        result(&st3, 1, false, now);
        return;
    }
    case 0x07:   // RECALIBRATE
        seek_start(u, 0, true, now);
        return;
    case 0x0F:   // SEEK
        seek_start(u, cmd[2], false, now);
        return;
    case 0x08: {   // SENSE INTERRUPT STATUS
        int k = 0;
        while (k < 4 && !pend[k]) k++;   // the lowest unit first
        if (k == 4) {
            uint8_t r = IC_INVALID;
            result(&r, 1, false, now);
            return;
        }
        pend[k] = false;
        uint8_t r[2] = { pend_st0[k], pend_pcn[k] };
        if ((r[0] & 0xC0) != IC_READY) seek_busy &= (uint8_t)~(1 << k);
        result(r, 2, false, now);
        return;
    }
    case 0x02: case 0x05: case 0x06: case 0x09: case 0x0A: case 0x0C: case 0x0D:
        start_exec(now);
        return;
    default: {
        if ((c0 & 0x1F) == 0x11 || (c0 & 0x1F) == 0x19 || (c0 & 0x1F) == 0x1D)
            LOGW("8272: SCAN commands are not emulated (answered as invalid)");
        uint8_t r = IC_INVALID;
        result(&r, 1, false, now);
        return;
    }
    }
}

void I8272::seek_start(int u, int tgt, bool rc, uint64_t now)
{
    seek_busy |= (uint8_t)(1 << u);
    seek_hd[u] = (uint8_t)(cmd[1] & 4);
    pend[u] = false;   // the unit's old status (ready changed after RESET) gives way to the seek
    if (!ready(u)) {
        set_int((uint8_t)(IC_ABNORMAL | ST0_SE | ST0_NR | seek_hd[u] | u), pcn[u]);
        return;
    }
    seeking[u] = true;
    recal[u] = rc;
    target[u] = tgt;
    steps_left[u] = 77;   // RECALIBRATE gives up after 77 steps
    next_step[u] = now;
}

void I8272::seek_step(int u)
{
    FloppyDrive* d = drive(u);
    uint64_t period = us(1000u * (uint32_t)(16 - srt));   // SRT: 16 - n ms at 8 MHz
    if (recal[u]) {
        if (d && d->track == 0) {
            seeking[u] = false;
            pcn[u] = 0;
            set_int((uint8_t)(IC_NORMAL | ST0_SE | seek_hd[u] | u), 0);
            return;
        }
        if (steps_left[u] == 0) {   // no TRACK 0 signal: equipment check
            seeking[u] = false;
            pcn[u] = 0;
            set_int((uint8_t)(IC_ABNORMAL | ST0_SE | ST0_EC | seek_hd[u] | u), 0);
            return;
        }
        steps_left[u]--;
        if (d && d->track > 0) d->track--;
    } else {
        if (pcn[u] == target[u]) {
            seeking[u] = false;
            set_int((uint8_t)(IC_NORMAL | ST0_SE | seek_hd[u] | u), pcn[u]);
            return;
        }
        int dir = target[u] > pcn[u] ? 1 : -1;
        pcn[u] = (uint8_t)(pcn[u] + dir);
        if (d) {
            int t = d->track + dir;
            if (t >= 0 && t <= FloppyDrive::LAST_TRACK) d->track = t;
        }
    }
    next_step[u] += period ? period : 1;
}

void I8272::advance(uint64_t now)
{
    for (int u = 0; u < 4; u++)
        while (seeking[u] && next_step[u] <= now) seek_step(u);
    if (hd_loaded && phase == PH_CMD && unload_at <= now) hd_loaded = false;
    while (phase == PH_EXEC && ev_t <= now) exec_event();
    if (phase == PH_CMD) poll_ready();
}

void I8272::start_exec(uint64_t now)
{
    uint8_t c0 = cmd[0];
    int opc = c0 & 0x1F;
    unit = cmd[1] & 3;
    head = (cmd[1] >> 2) & 1;
    mt = (c0 & 0x80) != 0;
    mfm = (c0 & 0x40) != 0;
    sk = (c0 & 0x20) != 0;
    deleted = opc == 0x0C || opc == 0x09;
    switch (opc) {
    case 0x05: case 0x09: op = OP_WRITE; break;
    case 0x02: op = OP_READ_TRACK; break;
    case 0x0A: op = OP_READ_ID; break;
    case 0x0D: op = OP_FORMAT; break;
    default: op = OP_READ; break;
    }
    if (op == OP_FORMAT) {
        n_ = cmd[2];
        eot = cmd[3];   // sectors per track
        gpl = cmd[4];
        fill = cmd[5];
        fmt_kbps = host ? host->fdc_kbps(unit, mfm) : 250;
    } else if (op != OP_READ_ID) {
        c_ = cmd[2];
        h_ = cmd[3];
        r_ = cmd[4];
        n_ = cmd[5];
        eot = cmd[6];
        gpl = cmd[7];
        dtl = cmd[8];
    }
    st1 = st2 = 0;
    tc = overrun = false;
    saw_id = false;
    skip_sec = false;
    sectors_done = 0;
    nd_full = false;
    int_byte = false;
    phase = PH_EXEC;
    ev_t = now;
    if (!ready(unit)) {
        finish((uint8_t)(IC_ABNORMAL | ST0_NR), 0, 0, c_, h_, r_, n_);
        rqm_at = now + us(RQM_US);
        return;
    }
    FloppyDrive* d = drive(unit);
    if ((op == OP_WRITE || op == OP_FORMAT) && d && d->present() && d->write_protect) {
        finish(IC_ABNORMAL, ST1_NW, 0, c_, h_, r_, n_);
        rqm_at = now + us(RQM_US);
        return;
    }
    ex = X_LOAD;
    if (!hd_loaded) {
        hd_loaded = true;
        ev_t = now + us(2000u * (uint32_t)(hlt ? hlt : 128));   // HLT x 2 ms
    }
}

void I8272::finish(uint8_t ic, uint8_t s1, uint8_t s2, uint8_t c, uint8_t h, uint8_t r, uint8_t n)
{
    uint8_t out[7] = { (uint8_t)(ic | head << 2 | unit), s1, s2, c, h, r, n };
    result(out, 7, true, ev_t);
    unload_at = ev_t + us(16000u * (uint32_t)(hut ? hut : 16));   // HUT x 16 ms
}

bool I8272::data_byte(uint8_t& b, bool to_memory)
{
    if (!nd) {
        bool t = false;
        if (!host || !host->fdc_dma(to_memory, b, t)) return false;
        if (t) tc = true;
        return true;
    }
    if (to_memory) {
        if (nd_full) return false;   // the CPU did not take the previous byte
        nd_byte = b;
        nd_full = true;
        int_byte = true;
        return true;
    }
    if (!nd_full) return false;      // the CPU did not supply the byte in time
    b = nd_byte;
    nd_full = false;
    int_byte = true;                 // the next one, please
    return true;
}

// Look for the ID of the wanted sector from disk byte `from_b` on, at most until the second
// index pulse; schedules the data (X_DATA), the end of the ID (READ ID) or the failure.
void I8272::search(uint64_t from_b)
{
    ex = X_SEARCH;
    if (!has_index()) {   // no disk turning, no index pulses: the chip waits for ever
        ev_t = NEVER;
        return;
    }
    int len, first, slen, idm;
    lay(len, first, slen, idm);
    uint64_t L = (uint64_t)len;
    search_end_b = (from_b / L + 1) * L + L;
    if (ids_readable()) {
        FloppyDrive* d = drive(unit);
        FloppyTrack* t = trk();
        for (uint64_t rev = from_b / L; rev * L < search_end_b; rev++) {
            for (size_t s = 0; s < t->sec.size(); s++) {
                uint64_t start = rev * L + (uint64_t)first + (uint64_t)s * (uint64_t)slen;
                uint64_t id_b = start + (uint64_t)idm;
                if (id_b < from_b) continue;
                if (id_b + 6 >= search_end_b) break;
                saw_id = true;
                const FloppySector& fs = t->sec[s];
                bool same = fs.c == c_ && fs.h == h_ && fs.r == r_ && fs.n == n_;
                bool match;
                if (op == OP_READ_ID) match = true;
                else if (op == OP_READ_TRACK) match = (int)s == sectors_done;
                else match = same;
                if (op == OP_READ_TRACK && !same) st1 |= ST1_ND;
                if (!match) {
                    if (fs.c != c_) st2 |= c_ == 0xFF ? ST2_BC : ST2_WC;
                    continue;
                }
                sec_found = (int)s;
                sec_flags = fs.flags;
                if (op == OP_READ_ID) {
                    c_ = fs.c, h_ = fs.h, r_ = fs.r, n_ = fs.n;
                    ex = X_SECEND;
                    ev_t = byte_end(id_b + 6);
                    return;
                }
                st2 &= (uint8_t)~(ST2_WC | ST2_BC);
                data_b = start + (uint64_t)t->data_mark + 1;
                data_i = 0;
                int size = t->size <= (int)sizeof(buf) ? t->size : (int)sizeof(buf);
                data_len = (n_ == 0 && dtl < 128) ? dtl : size;
                skip_sec = false;
                if (op != OP_WRITE) {
                    if (fs.flags & FS_NO_DATA) {   // no data mark after the ID
                        ex = X_NODATA;
                        ev_t = byte_end(data_b);
                        return;
                    }
                    bool del = (fs.flags & FS_DELETED) != 0;
                    if (op == OP_READ && del != deleted) {   // the other data mark
                        st2 |= ST2_CM;
                        if (sk) {   // skipped: on to the next sector
                            skip_sec = true;
                            ex = X_SECEND;
                            ev_t = byte_end(data_b + (uint64_t)size + 1);
                            return;
                        }
                    }
                    memcpy(buf, &d->data[fs.off], (size_t)size);
                }
                ex = X_DATA;
                ev_t = byte_end(data_b);
                return;
            }
        }
    }
    ev_t = byte_end(search_end_b);   // X_SEARCH event: not found
}

void I8272::fail_search(void)
{
    st1 |= saw_id ? ST1_ND : ST1_MA;
    finish(IC_ABNORMAL, st1, st2, c_, h_, r_, n_);
}

void I8272::sector_done(void)
{
    FloppyDrive* d = drive(unit);
    if (op == OP_READ_ID) {
        finish(IC_NORMAL, 0, 0, c_, h_, r_, n_);
        return;
    }
    size_t size = (size_t)(128 << n_);
    if (size > sizeof(buf)) size = sizeof(buf);
    if (!skip_sec) {
        if (op == OP_WRITE && d && d->present()) {
            FloppyTrack* t = trk();
            if (t && sec_found < (int)t->sec.size()) {
                FloppySector& fs = t->sec[sec_found];
                uint8_t nf = deleted ? FS_DELETED : 0;   // a new data mark, a good CRC
                uint8_t* p = &d->data[fs.off];
                if (memcmp(p, buf, size) != 0 || fs.flags != nf) {
                    memcpy(p, buf, size);
                    fs.flags = nf;
                    d->dirty = true;
                }
            }
        }
        sectors_done++;
        if (op != OP_WRITE && (sec_flags & FS_DATA_CRC)) {   // data error: the transfer ends here
            finish(IC_ABNORMAL, ST1_DE | st1, ST2_DD | st2, c_, h_, r_, n_);
            return;
        }
    }
    skip_sec = false;
    uint8_t nc = c_, nh = h_, nr = (uint8_t)(r_ + 1);
    if (r_ == eot) {
        if (mt && h_ == 0) nh = 1, nr = 1;
        else nc = (uint8_t)(c_ + 1), nr = 1;
    }
    if (tc || (op == OP_READ && (st2 & ST2_CM) && !sk)) {   // TC, or a sector with the other data mark
        finish(IC_NORMAL, st1, st2, nc, nh, nr, n_);
        return;
    }
    if (op == OP_READ_TRACK ? sectors_done >= eot : r_ == eot) {
        if (op == OP_READ_TRACK) {
            finish(IC_NORMAL, st1, st2, nc, nh, nr, n_);
            return;
        }
        if (mt && h_ == 0) {   // multi-track: on to head 1
            h_ = 1, r_ = 1, head = 1;
            search(data_b + size + 2);
            return;
        }
        finish(IC_ABNORMAL, ST1_EN | st1, st2, nc, nh, nr, n_);
        return;
    }
    r_ = nr;
    search(data_b + size + 2);
}

void I8272::exec_event(void)
{
    FloppyDrive* d = drive(unit);
    switch (ex) {
    case X_LOAD: {
        uint64_t b = ev_t / byte_cyc();
        int len, first, slen, idm;
        lay(len, first, slen, idm);
        uint64_t L = (uint64_t)len;
        if (op == OP_FORMAT || op == OP_READ_TRACK) {
            if (!has_index()) {
                ex = X_SEARCH;
                ev_t = NEVER;
                return;
            }
            index_b = (b / L + 1) * L;   // both start at the index pulse
            if (op == OP_FORMAT) {
                ex = X_FMT_INDEX;
                ev_t = index_b * byte_cyc();
            } else {
                search(index_b);
            }
            return;
        }
        search(b);
        return;
    }
    case X_SEARCH:
        fail_search();
        return;
    case X_NODATA:
        finish(IC_ABNORMAL, ST1_MA | st1, ST2_MD | st2, c_, h_, r_, n_);
        return;
    case X_DATA: {
        int size = 128 << n_;
        if (size > (int)sizeof(buf)) size = (int)sizeof(buf);
        if (data_i < data_len && !tc) {
            uint8_t b = buf[data_i];
            if (!data_byte(b, op != OP_WRITE)) {
                finish(IC_ABNORMAL, ST1_OR | st1, st2, c_, h_, r_, n_);
                return;
            }
            if (op == OP_WRITE) buf[data_i] = b;
        } else if (op == OP_WRITE) {
            buf[data_i] = 0;   // after TC or DTL the rest of the sector is written with zeros
        }
        data_i++;
        if (data_i >= size) {
            ex = X_SECEND;
            ev_t = byte_end(data_b + (uint64_t)size + 1);   // after the CRC
        } else {
            ev_t = byte_end(data_b + (uint64_t)data_i);
        }
        return;
    }
    case X_SECEND:
        sector_done();
        return;
    case X_FMT_INDEX: {
        int len, first, slen, idm;
        lay(len, first, slen, idm);
        fmt_sec = 0;
        fmt_i = 0;
        ex = X_FMT_ID;
        ev_t = byte_end(index_b + (uint64_t)(first + idm + 1));
        return;
    }
    case X_FMT_ID: {
        int len, first, slen, idm;
        lay(len, first, slen, idm);
        uint8_t b = 0;
        if (!data_byte(b, false)) {
            finish(IC_ABNORMAL, ST1_OR, 0, fmt_id[0], fmt_id[1], fmt_id[2], fmt_id[3]);
            return;
        }
        fmt_id[fmt_i++] = b;
        uint64_t sec_b = index_b + (uint64_t)first + (uint64_t)fmt_sec * (uint64_t)slen;
        if (fmt_i < 4) {
            ev_t = byte_end(sec_b + (uint64_t)idm + 1 + (uint64_t)fmt_i);
            return;
        }
        memcpy(fmt_ids[fmt_sec], fmt_id, 4);
        fmt_sec++;
        fmt_i = 0;
        if (!tc && fmt_sec < eot && fmt_sec < 256) {
            ev_t = byte_end(sec_b + (uint64_t)(slen + idm + 1));
        } else {
            ex = X_FMT_END;
            ev_t = (index_b + (uint64_t)len) * byte_cyc();   // until the next index pulse
        }
        return;
    }
    case X_FMT_END: {
        // the track is written: the image keeps it when it can (ImageDisk always, a plain image
        // in its own format)
        if (d && d->present()) {
            std::vector<FloppySector> ids;
            for (int k = 0; k < fmt_sec; k++) {
                FloppySector s;
                s.c = fmt_ids[k][0], s.h = fmt_ids[k][1], s.r = fmt_ids[k][2], s.n = fmt_ids[k][3];
                ids.push_back(s);
            }
            if (fmt_sec && fmt_ids[0][0] != d->track)
                LOGW("8272: FORMAT writes C=%d on track %d", fmt_ids[0][0], d->track);
            if (n_ > 6 || !d->format_track(d->track, head, mfm, fmt_kbps, n_, ids, fill))
                LOGW("8272: FORMAT %s N=%d x %d head %d does not fit the disk (%s), not kept in the image",
                     mfm ? "MFM" : "FM", n_, fmt_sec, head, d->format.c_str());
        }
        finish(IC_NORMAL, 0, 0, fmt_id[0], fmt_id[1], fmt_id[2], fmt_id[3]);
        return;
    }
    default:
        ev_t = NEVER;
        return;
    }
}

void I8272::state(StateIO& io)
{
    io.e(phase);
    io.arr(cmd, sizeof(cmd));
    io.e(cmd_n);
    io.e(cmd_len);
    io.arr(res, sizeof(res));
    io.e(res_n);
    io.e(res_pos);
    io.v(rqm_at);
    io.v(int_result);
    io.v(int_byte);
    io.v(srt);
    io.v(hut);
    io.v(hlt);
    io.v(nd);
    io.v(hd_loaded);
    io.v(unload_at);
    io.arr(pcn, sizeof(pcn));
    io.arr(seek_hd, sizeof(seek_hd));
    for (int u = 0; u < 4; u++) {
        io.v(seeking[u]);
        io.v(recal[u]);
        io.e(target[u]);
        io.e(steps_left[u]);
        io.v(next_step[u]);
        io.v(last_ready[u]);
    }
    io.v(seek_busy);
    for (int u = 0; u < 4; u++) io.v(pend[u]);
    io.arr(pend_st0, sizeof(pend_st0));
    io.arr(pend_pcn, sizeof(pend_pcn));
    io.e(ex);
    io.v(ev_t);
    io.e(op);
    io.e(unit);
    io.e(head);
    io.v(mt);
    io.v(mfm);
    io.v(sk);
    io.v(deleted);
    io.v(c_);
    io.v(h_);
    io.v(r_);
    io.v(n_);
    io.v(eot);
    io.v(gpl);
    io.v(dtl);
    io.v(st1);
    io.v(st2);
    io.v(search_end_b);
    io.v(saw_id);
    io.v(data_b);
    io.e(data_i);
    io.e(data_len);
    io.e(sec_found);
    io.v(sec_flags);
    io.v(skip_sec);
    io.v(tc);
    io.v(overrun);
    io.arr(buf, sizeof(buf));
    io.e(sectors_done);
    io.v(nd_full);
    io.v(nd_byte);
    io.v(index_b);
    io.e(fmt_sec);
    io.e(fmt_i);
    io.arr(fmt_id, sizeof(fmt_id));
    io.arr(&fmt_ids[0][0], sizeof(fmt_ids));
    io.e(fmt_kbps);
    io.v(fill);
}
