#pragma once
// Intel 8272 / NEC uPD765 floppy disk controller (FDC-1A, MFC-1A) with the drives of
// chips/floppy.h: 8" SS SD FM disks and PC formats (MFM, 512 B sectors, two sides). Time is in
// bus cycles (hz per second).
//
// Registers: A0 = 0 main status (MSR, read only), A0 = 1 data (command bytes, result bytes,
// data in non-DMA mode).
// MSR: D7 RQM, D6 DIO (1 = to the CPU), D5 EXM (non-DMA execution), D4 CB (busy),
// D3-D0 drive 3-0 seeking.
//
// Commands: READ DATA, READ DELETED DATA, WRITE DATA, WRITE DELETED DATA, READ TRACK,
// READ ID, FORMAT A TRACK, RECALIBRATE, SEEK, SENSE INTERRUPT STATUS, SENSE DRIVE STATUS,
// SPECIFY. SCAN is not emulated (answered as an invalid command). The head sees ID marks when
// the disk has the head, its recording (FM/MFM, the command's MF bit) and the data rate the
// board gives the chip for the drive (fdc_kbps); sector sizes N = 0-3.
//
// Timing: the disk turns with its own rpm and data rate (a byte every 32 us FM 8" or 16 us MFM
// HD); step rate 16 - SRT ms, head load HLT x 2 ms, head unload HUT x 16 ms after the last
// command (0 = the maximum), all for the 8 MHz clock.
#include <stdint.h>

class StateIO;
struct FloppyDrive;
struct FloppyGeom;

// What the board connects to the chip.
class I8272Host {
public:
    virtual ~I8272Host() {}
    // Drive of unit 0-3 (DS1, DS0), nullptr when none is connected.
    virtual FloppyDrive* fdc_drive(int unit) = 0;
    // READY input for the unit.
    virtual bool fdc_ready(int unit) = 0;
    // DMA cycle for one data byte (DRQ, DACK): to_memory = the FDC reads the disk. Returns
    // false when the DMA controller does not answer (the byte is lost: overrun); tc = TC
    // input during the cycle.
    virtual bool fdc_dma(bool to_memory, uint8_t& byte, bool& tc) = 0;
    // Data rate (kbit/s) the board's clock gives the chip for the unit in FM or MFM: an 8 MHz
    // clock gives 250 FM, 500 MFM.
    virtual int fdc_kbps(int unit, bool mfm) { return mfm ? 500 : 250; }
};

struct I8272 {
    I8272Host* host = nullptr;
    uint32_t hz = 2000000;

    void reset(uint64_t now);           // RESET pin (also power on); keeps the SPECIFY values
    uint8_t read(int a0, uint64_t now);
    void write(int a0, uint8_t value, uint64_t now);
    void advance(uint64_t now);
    bool interrupt(void) const { return int_result || int_byte || pend[0] || pend[1] || pend[2] || pend[3]; }
    uint8_t msr(uint64_t now) const;
    bool head_loaded(void) const { return hd_loaded; }
    bool busy(void) const { return phase != PH_CMD; }
    int current_unit(void) const { return unit; }
    void state(StateIO& io);

private:
    enum Phase { PH_CMD, PH_EXEC, PH_RESULT };
    enum Exec { X_NONE, X_LOAD, X_SEARCH, X_DATA, X_SECEND, X_FMT_INDEX, X_FMT_ID, X_FMT_DATA, X_FMT_END };
    enum Op { OP_READ, OP_WRITE, OP_READ_ID, OP_FORMAT, OP_READ_TRACK };

    uint64_t byte_cyc(void) const;
    uint64_t us(uint32_t n) const { return (uint64_t)hz * n / 1000000; }
    uint64_t byte_end(uint64_t b) const { return (b + 1) * byte_cyc(); }   // time a disk byte has passed
    FloppyDrive* drive(int u) { return host ? host->fdc_drive(u) : nullptr; }
    bool ready(int u) { return host && host->fdc_ready(u); }
    bool has_index(void);               // a disk turns in the drive: index pulses
    bool ids_readable(void);            // the head sees ID marks (disk, head, recording, data rate, track)
    const FloppyGeom& geom(void);       // format of the disk in the unit (8" SS SD when none)
    void command(uint64_t now);
    void start_exec(uint64_t now);
    void exec_event(void);
    void search(uint64_t from_b);
    void sector_done(void);
    void fail_search(void);
    void finish(uint8_t ic, uint8_t s1, uint8_t s2, uint8_t c, uint8_t h, uint8_t r, uint8_t n);
    void result(const uint8_t* r, int n, bool irq, uint64_t now);
    void seek_start(int u, int target, bool recal, uint64_t now);
    void seek_step(int u);
    void set_int(uint8_t st0, uint8_t pcn);   // for the unit in st0 D1-D0
    void poll_ready(void);
    bool data_byte(uint8_t& b, bool to_memory);   // one byte through DMA or the data register

    Phase phase = PH_CMD;
    uint8_t cmd[9] = {};
    int cmd_n = 0, cmd_len = 0;
    uint8_t res[7] = {};
    int res_n = 0, res_pos = 0;
    uint64_t rqm_at = 0;                // RQM comes back after a byte
    bool int_result = false;            // INT at the start of the result phase
    bool int_byte = false;              // INT for a data byte in non-DMA mode

    // SPECIFY
    uint8_t srt = 0, hut = 0, hlt = 0;
    bool nd = false;

    // head load (HDL is common to all drives)
    bool hd_loaded = false;
    uint64_t unload_at = 0;

    // seeks, per unit; PCN is the controller's idea of the track, the drive has its own
    uint8_t pcn[4] = {};
    uint8_t seek_hd[4] = {};
    bool seeking[4] = {};
    bool recal[4] = {};
    int target[4] = {};
    int steps_left[4] = {};
    uint64_t next_step[4] = {};
    uint8_t seek_busy = 0;              // MSR D3-D0 until SENSE INTERRUPT STATUS
    bool last_ready[4] = {};

    // pending interrupt status per unit (seek end or ready change; a newer one replaces it)
    bool pend[4] = {};
    uint8_t pend_st0[4] = {}, pend_pcn[4] = {};

    // execution
    Exec ex = X_NONE;
    uint64_t ev_t = 0;                  // time of the next execution event
    int op = OP_READ;
    int unit = 0, head = 0;
    bool mt = false, mfm = false, sk = false, deleted = false;
    uint8_t c_ = 0, h_ = 0, r_ = 0, n_ = 0, eot = 0, gpl = 0, dtl = 0;
    uint8_t st1 = 0, st2 = 0;
    uint64_t search_end_b = 0;          // second index pulse: give up
    bool saw_id = false;
    uint64_t data_b = 0;                // disk byte of the first data byte of the sector
    int data_i = 0, data_len = 0;
    int sec_found = 0;                  // sector found (from 1)
    bool tc = false;
    bool overrun = false;
    uint8_t buf[1024] = {};
    int sectors_done = 0;
    // non-DMA mode: one data byte waiting for the CPU (read) or wanted from it (write)
    bool nd_full = false;
    uint8_t nd_byte = 0;
    // FORMAT, READ TRACK: the revolution started at an index pulse (disk byte)
    uint64_t index_b = 0;
    int fmt_sec = 0, fmt_i = 0;
    uint8_t fmt_id[4] = {};
    uint8_t fill = 0xE5;
};
