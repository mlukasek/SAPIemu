#include "chips/rtc72421.h"

#include <time.h>

// Register map: 0 S1, 1 S10, 2 MI1, 3 MI10, 4 H1, 5 H10 (bit 2 = PM in 12h mode),
// 6 D1, 7 D10, 8 MO1, 9 MO10, A Y1, B Y10, C W (weekday), D CD (b0 HOLD, b1 BUSY,
// b2 IRQ FLAG, b3 30s ADJ), E CE (b0 MASK, b1 ITRPT/STND, b3-2 period),
// F CF (b0 RESET, b1 STOP, b2 24/12, b3 TEST).

// days since 1970-01-01 for a civil date (proleptic Gregorian)
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static void civil_from_days(int64_t z, int& y, int& m, int& d)
{
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t yy = (int64_t)yoe + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    m = (int)(mp < 10 ? mp + 3 : mp - 9);
    y = (int)(yy + (m <= 2));
}

// host local wall time as seconds since 1970 (no timezone conversion)
static int64_t host_local_seconds(void)
{
    time_t t = time(NULL);
    struct tm lt;
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    return days_from_civil(lt.tm_year + 1900, (unsigned)(lt.tm_mon + 1), (unsigned)lt.tm_mday) * 86400 +
           lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
}

void Rtc72421::reset(void)
{
    // the RTC is battery backed: /RES of the computer does not affect it
}

void Rtc72421::load_fields(void)
{
    int64_t t = host_local_seconds() + offset;
    int64_t days = t / 86400;
    int sec = (int)(t % 86400);
    int y, mo, d;
    civil_from_days(days, y, mo, d);
    int h = sec / 3600, mi = (sec / 60) % 60, s = sec % 60;
    int yy = y % 100;
    reg[0] = (uint8_t)(s % 10); reg[1] = (uint8_t)(s / 10);
    reg[2] = (uint8_t)(mi % 10); reg[3] = (uint8_t)(mi / 10);
    if (mode24) {
        reg[4] = (uint8_t)(h % 10); reg[5] = (uint8_t)(h / 10);
    } else {
        int h12 = h % 12 == 0 ? 12 : h % 12;
        reg[4] = (uint8_t)(h12 % 10); reg[5] = (uint8_t)((h12 / 10) | (h >= 12 ? 4 : 0));
    }
    reg[6] = (uint8_t)(d % 10); reg[7] = (uint8_t)(d / 10);
    reg[8] = (uint8_t)(mo % 10); reg[9] = (uint8_t)(mo / 10);
    reg[10] = (uint8_t)(yy % 10); reg[11] = (uint8_t)(yy / 10);
    reg[12] = (uint8_t)(((days % 7) + 7 + 4) % 7);   // 1970-01-01 was a Thursday (4), 0 = Sunday
}

void Rtc72421::store_fields(void)
{
    int s = reg[0] + reg[1] * 10, mi = reg[2] + reg[3] * 10;
    int h;
    if (mode24) {
        h = reg[4] + (reg[5] & 3) * 10;
    } else {
        int h12 = reg[4] + (reg[5] & 1) * 10;
        h = (h12 % 12) + ((reg[5] & 4) ? 12 : 0);
    }
    int d = reg[6] + reg[7] * 10, mo = reg[8] + reg[9] * 10, yy = reg[10] + reg[11] * 10;
    if (mo < 1) mo = 1;
    if (mo > 12) mo = 12;
    if (d < 1) d = 1;
    int y = yy < 70 ? 2000 + yy : 1900 + yy;
    int64_t t = days_from_civil(y, (unsigned)mo, (unsigned)d) * 86400 + h * 3600 + mi * 60 + s;
    offset = t - host_local_seconds();
}

uint8_t Rtc72421::read(int r, uint64_t now, uint32_t cpu_hz)
{
    r &= 15;
    if (r < 13) {
        if (!hold && !stop && !fields_dirty) load_fields();
        return reg[r] & 0x0F;
    }
    if (r == 13) {
        uint8_t v = (uint8_t)(cd & 0x09);   // HOLD, 30s ADJ
        if (irq_flag) v |= 0x04;
        return v;                           // BUSY never set (no update in progress)
    }
    if (r == 14) return ce & 0x0F;
    return cf & 0x0F;
}

void Rtc72421::write(int r, uint8_t v, uint64_t now, uint32_t cpu_hz)
{
    r &= 15;
    v &= 0x0F;
    if (r < 13) {
        if (!fields_dirty) load_fields();
        reg[r] = v;
        fields_dirty = true;
        if (!hold && !stop) { store_fields(); fields_dirty = false; }
        return;
    }
    if (r == 13) {
        bool new_hold = (v & 1) != 0;
        if (!(v & 4)) irq_flag = false;       // writing 0 clears the IRQ flag
        if (v & 8) {                           // 30 s adjust
            load_fields();
            int s = reg[0] + reg[1] * 10;
            offset += (s >= 30) ? (60 - s) : -s;
        }
        cd = v & 0x09;
        if (hold && !new_hold && fields_dirty && !stop) { store_fields(); fields_dirty = false; }
        hold = new_hold;
        return;
    }
    if (r == 14) {
        ce = v;
        next_irq_cycle = 0;
        return;
    }
    bool new_stop = (v & 2) != 0;
    if (stop && !new_stop && fields_dirty) { store_fields(); fields_dirty = false; }
    stop = new_stop;
    if (v & 1) {
        // RESET: seconds cleared
        load_fields();
        offset -= reg[0] + reg[1] * 10;
    }
    mode24 = (v & 4) != 0;
    cf = v;
}

bool Rtc72421::irq(uint64_t now, uint32_t cpu_hz)
{
    static const double periods[4] = { 1.0 / 64.0, 1.0, 60.0, 3600.0 };
    double period = periods[(ce >> 2) & 3];
    uint64_t period_cycles = (uint64_t)(period * cpu_hz);
    if (period_cycles == 0) period_cycles = 1;
    if (next_irq_cycle == 0) next_irq_cycle = now + period_cycles;
    if (now >= next_irq_cycle) {
        irq_flag = true;
        next_irq_cycle += period_cycles * ((now - next_irq_cycle) / period_cycles + 1);
    }
    if (ce & 1) return false;   // MASK
    return irq_flag;            // STND mode: level until the flag is cleared
}
