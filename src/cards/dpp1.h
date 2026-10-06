#pragma once
// DPP-1 - parallel ports (Tesla, 6XK 199 19): two nearly identical channels, each an 8255
// (MHB8255A) behind 3216 buffers, see docs/desky/DPP-1.md.
//
// Address: the decoder 3205 (D5A) with 7486 (D4A) and switches 1S1-3S1 selects one of eight
// spaces of 8 ports in 00h-3Fh (A7 = A6 = 0): A5 = 1S1 closed, A4 = 2S1 open, A3 = 3S1 open
// (manual tab. 2, 3). A2 selects the channel: base+0..3 channel 1 (1PA, 1PB, 1PC, control),
// base+4..7 channel 2. /RES resets both 8255.
// Connected here: the PMD32-SD floppy emulator on port A of one channel in mode 2, as the CP/M
// of ECMON and J. Biba use it (channel 2, 2Ch-2Fh), and a Centronics printer on port A of a
// channel in mode 1 output: /OBF (PC7) is its /STROBE, its /ACK goes to PC6 (/ACK of the 8255).
// ECMON and J. Biba send PUN: (and LST: = UL1:) there, channel 1 (28h-2Bh, control word AEh).
// The switches S2-S4 (direction of the 3216 buffers, polarity of the PC signals) are assumed
// set as the devices need; a channel without a device reads high.
#include "chips/i8255.h"
#include "chips/pmd32sd.h"
#include "chips/printer.h"
#include "core/card.h"

class Dpp1 : public Card {
public:
    Dpp1();
    ~Dpp1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    void flush() override;
    void describe(nlohmann::json& out) override;
    int decode_io(uint8_t port, const char** what) override;
    unsigned panel_leds() override;
    Pmd32Sd* pmd32sd() override { return pmd_enabled ? &pmd : nullptr; }
    Printer* printer() override { return prn_enabled ? &prn : nullptr; }

private:
    uint8_t base = 0x28;          // S1
    int pmd_channel = 1;          // 0 = channel 1, 1 = channel 2
    bool pmd_enabled = false;
    I8255 ppi[2];
    Pmd32Sd pmd;
    int prn_channel = 0;          // 0 = channel 1, 1 = channel 2
    bool prn_enabled = false;
    Printer prn;
    bool prn_wait_ack = false;    // the printer has the byte of /OBF, its /ACK is to come
    void printer_service(uint64_t now);
    uint64_t access_at = 0;       // activity LED of the panel drawing (not on the real card)
    bool accessed = false;
};
