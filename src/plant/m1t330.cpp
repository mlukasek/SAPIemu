#include "plant/m1t330.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "core/state.h"

// times of the instrument (guessed: a firmware in 3 x 2708 EPROM serves the interface)
static const double ACCEPT_US = 3;       // DAV seen -> NDAC released
static const double PROCESS_US = 40;     // a byte taken -> ready for the next (NRFD released)
static const double T1_US = 2;           // data on the lines -> DAV
static const double BETWEEN_US = 25;     // DAV released -> the next byte of the reply
static const double CONV_MS = 10;        // one conversion: half a mains period
static const double OVERHEAD_MS = 8;     // zero and the arithmetic of a measurement
static const double RANGE_MS = 30;       // a range relay settles
static const double AUTOCAL_MS = 20;     // the zero and reference of the autocalibration
static const double CAL_MS = 200;        // command C
static const double REPEAT_MS = 200;     // START REP: the next measurement

static double full_scale(int r) { return 0.3 * pow(10.0, r); }   // 0.3, 3, 30, 300 V

void M1t330::power_on(uint64_t t)
{
    power = true;
    listener = talker = remote = false;
    ah = 0;
    ah_at = UINT64_MAX;
    sh = 0;
    sh_at = UINT64_MAX;
    out.clear();
    out_pos = 0;
    device_clear();
    autorange = true;
    filter = false;
    repeat = true;
    show_error = false;
    range = 1;
    valid = false;
    overload = false;
    counts = 0;
    dio = 0;
    ctl = 0;
    busy = false;
    meas_at = UINT64_MAX;
    if (ims) {
        prev_dav = ims->asserted(IMS_DAV);
        prev_ren = ims->asserted(IMS_REN);
        prev_ifc = ims->asserted(IMS_IFC);
    }
    start(t, false);
    update_drives();
}

void M1t330::power_off(void)
{
    power = false;
    listener = talker = remote = false;
    ah = 0;
    sh = 0;
    ah_at = sh_at = meas_at = UINT64_MAX;
    busy = false;
    out.clear();
    out_pos = 0;
    dio = 0;
    ctl = 0;
    if (ims) ims->settle(ims->t);
}

void M1t330::device_clear(void)
{
    cmd = 0;
    cmd_num = -1;
    wait_10ms = 0;
    autocal = true;
    reply_error = false;
    single_pending = false;
}

// ---------------------------------------------------------------- interface

void M1t330::update_drives(void)
{
    uint8_t c = 0;
    if (power) {
        if (ah == 1) c |= IMS_NDAC;                       // ready: NRFD false, NDAC true
        else if (ah == 2 || ah == 4) c |= IMS_NRFD | IMS_NDAC;
        else if (ah == 3) c |= IMS_NRFD;                  // accepted: NDAC false
        if (sh == 1 || sh == 2) {
            if (out_pos + 1 >= out.size()) c |= IMS_EOI;  // the last byte of the reply
            if (sh == 2) c |= IMS_DAV;
        }
    }
    ctl = c;
    dio = power && (sh == 1 || sh == 2) && out_pos < out.size() ? (uint8_t)out[out_pos] : 0;
}

void M1t330::lines_changed(uint64_t t)
{
    if (!power || !ims) return;
    bool atn = ims->asserted(IMS_ATN), dav = ims->asserted(IMS_DAV);
    bool ren = ims->asserted(IMS_REN), ifc = ims->asserted(IMS_IFC);
    if (ifc && !prev_ifc) {   // interface clear: unaddressed, the handshakes stop
        listener = talker = false;
        sh = 0;
        sh_at = UINT64_MAX;
    }
    if (!ren && prev_ren && remote) {   // back to local: the panel measures again
        remote = false;
        if (repeat && !busy) start(t, false);
    }
    prev_ren = ren;
    prev_ifc = ifc;

    // acceptor: takes part with ATN (commands for all) or when addressed to listen
    bool part = atn || listener;
    if (!part && ah != 0) { ah = 0; ah_at = UINT64_MAX; }
    if (part && ah == 0) ah = dav ? 4 : 1;   // joining in the middle of a byte: wait for its end
    if (ah == 1 && dav && !prev_dav) {
        take_byte(ims->dio_lines, atn, ims->asserted(IMS_EOI), t);
        if (ah != 0) {   // the byte may have unaddressed it (UNL with ATN keeps it: ATN still on)
            ah = 2;
            ah_at = t + ims->us(ACCEPT_US);
        }
    } else if ((ah == 3 || ah == 2 || (ah == 4 && ah_at == UINT64_MAX)) && !dav) {
        ah = 4;
        ah_at = t + ims->us(PROCESS_US);
    }
    prev_dav = dav;

    // source: stops while ATN is on
    if (atn && sh != 0) {
        sh = 0;
        sh_at = UINT64_MAX;
    }
    if (sh == 1 && sh_at == UINT64_MAX && !ims->asserted(IMS_NRFD)) sh_at = t;   // was waiting for NRFD
    if (sh == 2 && !ims->asserted(IMS_NDAC)) {   // all accepted
        out_pos++;
        sh = 3;
        sh_at = t + ims->us(BETWEEN_US);
        if (out_pos >= out.size()) { out.clear(); out_pos = 0; }
    }
    if (sh == 0 && talker && !atn && out_pos < out.size()) {
        sh = 1;
        sh_at = t + ims->us(T1_US);
    }
    update_drives();
}

uint64_t M1t330::next_event() const
{
    uint64_t e = ah_at;
    if (sh_at < e) e = sh_at;
    if (meas_at < e) e = meas_at;
    return e;
}

void M1t330::on_event(uint64_t t)
{
    if (ah_at <= t) {
        ah_at = UINT64_MAX;
        if (ah == 2) ah = 3;        // NDAC released: accepted
        else if (ah == 4) ah = (ims && (ims->asserted(IMS_ATN) || listener)) ? 1 : 0;
    }
    if (sh_at <= t) {
        sh_at = UINT64_MAX;
        if (sh == 1) {
            if (ims && !ims->asserted(IMS_NRFD)) sh = 2;   // all ready: DAV
            // else wait for NRFD (lines_changed)
        } else if (sh == 3) {
            sh = 0;
            if (talker && ims && !ims->asserted(IMS_ATN) && out_pos < out.size()) {
                sh = 1;
                sh_at = t + ims->us(T1_US);
            }
        }
    }
    if (meas_at <= t) {
        meas_at = UINT64_MAX;
        finish(t);
    }
    update_drives();
}

void M1t330::take_byte(uint8_t b, bool atn, bool eoi, uint64_t t)
{
    if (atn) command(b, t);
    else if (listener) data_char((char)(b & 0x7F), eoi, t);
}

void M1t330::command(uint8_t b, uint64_t t)
{
    b &= 0x7F;
    bool ren = ims && ims->asserted(IMS_REN);
    if (b == 0x20 + address) {
        listener = true;
        if (ren) remote = true;
    } else if (b == 0x3F) {
        listener = false;
    } else if (b == 0x40 + address) {
        talker = true;
        if (ren) remote = true;
    } else if (b >= 0x40 && b < 0x5F) {
        talker = false;   // another talker
    } else if (b == 0x5F) {
        talker = false;
    } else if (b == 0x14 || (b == 0x04 && listener)) {
        device_clear();
    } else if (b == 0x01 && listener) {
        remote = false;   // GTL
    } else if (b == 0x08 && listener) {
        start(t, true);   // GET
    }
}

// The commands: a letter and its number (R0-R4, D0/D1, K0/K1, W0-W99), E, C, Q. A command ends with
// the next letter, CR, LF, a space, a comma, '!' or EOI.
void M1t330::data_char(char c, bool eoi, uint64_t t)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c >= '0' && c <= '9' && cmd) {
        cmd_num = (cmd_num < 0 ? 0 : cmd_num * 10) + (c - '0');
        if (cmd_num > 99) cmd_num = 99;
    } else if (c >= 'A' && c <= 'Z') {
        flush_cmd(t);
        cmd = c;
        cmd_num = -1;
        if (c == 'E' || c == 'C' || c == 'Q') flush_cmd(t);   // no number
    } else {
        flush_cmd(t);
    }
    if (eoi) flush_cmd(t);
}

void M1t330::flush_cmd(uint64_t t)
{
    char c = cmd;
    int n = cmd_num;
    cmd = 0;
    cmd_num = -1;
    switch (c) {
    case 'R':
        if (n == 4) autorange = true;
        else if (n >= 0 && n <= 3) { autorange = false; range = n; }
        break;
    case 'D': filter = n == 1; break;
    case 'K': autocal = n != 0; break;
    case 'W': wait_10ms = n < 0 ? 0 : n; break;
    case 'E': start(t, true); break;
    case 'Q': reply_error = true; break;
    case 'C':
        busy = true;   // calibration: no result
        calibrating = true;
        remote_trigger = false;
        meas_at = t + (uint64_t)(CAL_MS * 1e-3 * (ims ? ims->hz : 2e6));
        break;
    default: break;
    }
}

// ---------------------------------------------------------------- measuring

void M1t330::start(uint64_t t, bool from_bus)
{
    if (!power) return;
    double hz = ims ? ims->hz : 2e6;
    double ms = wait_10ms * 10.0 + CONV_MS * (filter ? 4 : 1) + OVERHEAD_MS;
    if (autocal && cal_count % 8 == 0) ms += AUTOCAL_MS;
    busy = true;
    remote_trigger = from_bus;
    meas_at = t + (uint64_t)(ms * 1e-3 * hz);
}

double M1t330::noise(void)
{
    // about one count rms: the sum of four uniform numbers
    double s = 0;
    for (int i = 0; i < 4; i++) {
        seed = seed * 1103515245u + 12345u;
        s += ((seed >> 8) & 0xFFFF) / 65535.0;
    }
    return (s - 2.0) * 1.7;
}

int32_t M1t330::convert(int r)
{
    double res = full_scale(r) / 30000.0;
    double n = vin / res + noise() * (filter ? 0.3 : 1.0);
    return (int32_t)lround(n);
}

void M1t330::finish(uint64_t t)
{
    double hz = ims ? ims->hz : 2e6;
    if (!busy) return;
    if (calibrating) {   // C: zero and reference measured again, no result
        calibrating = false;
        busy = false;
        if (repeat && !remote) start(t + (uint64_t)(REPEAT_MS * 1e-3 * hz), false);
        return;
    }
    int32_t c = convert(range);
    if (autorange) {
        // one step at a time, each with a relay and a new conversion
        if ((c > 30000 || c < -30000) && range < 3) {
            range++;
            meas_at = t + (uint64_t)((RANGE_MS + CONV_MS) * 1e-3 * hz);
            return;
        }
        if (c < 2900 && c > -2900 && range > 0) {
            range--;
            meas_at = t + (uint64_t)((RANGE_MS + CONV_MS) * 1e-3 * hz);
            return;
        }
    }
    cal_count++;
    busy = false;
    counts = c;
    overload = c > 30000 || c < -30000;
    value = c * full_scale(range) / 30000.0;
    valid = true;
    measured++;
    if (remote_trigger) {
        out = format_reply();
        out_pos = 0;
        reply_error = false;
        remote_trigger = false;
        if (ims && talker && sh == 0 && !ims->asserted(IMS_ATN)) {
            sh = 1;
            sh_at = t + ims->us(T1_US);
        }
    }
    single_pending = false;
    if (repeat && !remote) {
        start(t + (uint64_t)(REPEAT_MS * 1e-3 * hz), false);
    }
}

double M1t330::error_pct(void) const
{
    // 0.01 % of the value and 2 counts (a guess for the display of CHYBA and the reply of Q)
    double res = full_scale(range) / 30000.0;
    double v = fabs(value);
    if (v < res) return 99.999;
    return 0.01 + 2.0 * res / v * 100.0;
}

std::string M1t330::format_reply(void) const
{
    char s[40];
    char letter = reply_error ? '%' : 'V';
    if (!reply_error && overload) {
        snprintf(s, sizeof s, "V+9.9999E+9\r\n");
        return s;
    }
    double v = reply_error ? error_pct() : value;
    if (v == 0) {
        snprintf(s, sizeof s, "%c+0.0000E+0\r\n", letter);
        return s;
    }
    int e = (int)floor(log10(fabs(v)));
    double m = v / pow(10.0, e);
    if (fabs(m) >= 9.99995) { m /= 10; e++; }
    snprintf(s, sizeof s, "%c%c%.4fE%c%d\r\n", letter, m < 0 ? '-' : '+', fabs(m), e < 0 ? '-' : '+', abs(e));
    return s;
}

std::string M1t330::display(void) const
{
    if (!power) return "";
    char s[16];
    if (show_error) {
        snprintf(s, sizeof s, "%6.4f", error_pct() > 9.9999 ? 9.9999 : error_pct());
        return s;
    }
    if (!valid) return "     .";
    if (overload) return counts < 0 ? "-OL" : " OL";
    // 5 digits with the decimal point of the range: 300 mV ddd.dd, 3 V d.dddd, 30 V dd.ddd, 300 V ddd.dd
    static const int decimals[4] = { 2, 4, 3, 2 };
    int32_t a = counts < 0 ? -counts : counts;
    double shown = range == 0 ? a / 100.0 : a * full_scale(range) / 30000.0;
    char num[16];
    snprintf(num, sizeof num, "%.*f", decimals[range], shown);
    snprintf(s, sizeof s, "%c%s", counts < 0 ? '-' : ' ', num);
    return s;
}

// ---------------------------------------------------------------- the front panel

void M1t330::key_auto(uint64_t t)
{
    if (!power || remote) return;
    autorange = !autorange;
}

void M1t330::key_filter(uint64_t t)
{
    if (!power || remote) return;
    filter = !filter;
}

void M1t330::key_rep(uint64_t t)
{
    if (!power || remote) return;
    repeat = true;
    if (!busy) start(t, false);
}

void M1t330::key_single(uint64_t t)
{
    if (!power || remote) return;
    repeat = false;
    single_pending = true;
    if (!busy) start(t, false);
}

void M1t330::key_error(void)
{
    if (!power) return;
    show_error = !show_error;
}

void M1t330::state(StateIO& io)
{
    io.v(power);
    io.v(autorange);
    io.v(filter);
    io.v(repeat);
    io.v(remote);
    io.v(show_error);
    int32_t r = range;
    io.v(r);
    range = r;
    io.v(counts);
    io.v(overload);
    io.v(valid);
    io.v(value);
    io.v(measured);
    io.v(listener);
    io.v(talker);
    int32_t a = ah, s = sh;
    io.v(a);
    io.v(s);
    ah = a;
    sh = s;
    io.v(ah_at);
    io.v(sh_at);
    io.v(prev_dav);
    io.v(prev_ren);
    io.v(prev_ifc);
    io.v(out);
    uint32_t op = (uint32_t)out_pos;
    io.v(op);
    out_pos = op;
    uint8_t cm = (uint8_t)cmd;
    io.v(cm);
    cmd = (char)cm;
    io.v(calibrating);
    int32_t cn = cmd_num, w = wait_10ms, cc = cal_count;
    io.v(cn);
    io.v(w);
    io.v(cc);
    cmd_num = cn;
    wait_10ms = w;
    cal_count = cc;
    io.v(autocal);
    io.v(busy);
    io.v(single_pending);
    io.v(remote_trigger);
    io.v(reply_error);
    io.v(meas_at);
    io.v(seed);
    io.v(vin);
    io.v(dio);
    io.v(ctl);
}
