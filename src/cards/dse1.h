#pragma once
// DSE-1 - EPROM simulator and programmer of SAPI-1 (Tesla Elstroj 3WK 055 04/07, DSE-1A / DSE-1B), see
// docs/desky/DSE-1.md.
//
// - Ports: 74LS85 compares A7-A4 with S1 (S1/1 = A6, S1/2 = A7, S1/3 = A5, S1/4 = A4, closed = 0; all open =
//   F0h), 74LS138 by A3-A2: base+0..3 the 8255 (MHB 8255A), base+4..7 the register 74LS174 (write only,
//   mirrors), base+8..F not decoded. The 8286 drives the bus on reads of all eight: base+3 (the control word,
//   the NMOS 8255A does not read it) and base+4..7 read FFh (assumption).
// - 8255: PA = PD0-7 (the inner data bus), PB = PA0-7 and PC0-3 = PA8-11 (the inner address bus), PC4 = W
//   (write the simulator RAM, a level: 2114 have no clock), PC5 = R (the RAM drives PD with W = 0), PC6 =
//   PROG.PULS, PC7 = R/W (read the EPROM).
// - Register: D0 U0V, D1 U25V (Vpp 0 V; 5 V with U0V; 25 V with both), D2 ONLA (the address of the probe
//   X3 on PA0-11 through 74LS257), D3 ONLD (data to the probe), D4 LED1 "progr.", D5 LED2 "map." and MAP1
//   (pin 21 of X1) through JP1. /RES clears it.
// - Simulator RAM 4 KB (8x 2114), only behind the 8255. The probe X3 is not connected in the emulator (ON
//   LINE is a state shown by the Media window and MCP).
// - Programming cable X2 with the socket (Eprom27, evaluated from its pins): wired for a 2716 as supplied
//   (18 = PC6, 20 = NOT PC7, 21 = Vpp), for a 2732 after DSE.DOC (21 = PA11, 18 = NOR(PC6, PC7), 20 =
//   Vpp), or two sockets (the replicas: the type of the chip picks its socket).
// - Pin levels: an 8255 pin set as input leaves the line floating; lines with an LS-TTL input (PA10, PA11,
//   PC4-7, PD0-7) are H, PA0-9 go only to MOS inputs and are taken as H too (assumption). Several drivers on
//   PD: wired AND, a warning in the log once per kind. A RAM write with PA0-9 floating (after /RES until the
//   first control word) is not modelled: the address is not determined.
// - Programming pulse: measured in bus time; PD is sampled while it lasts (AND), the write that ends it does
//   not count. A pulse of 1 ms or longer programs (cell AND data), a shorter one not (assumption: the
//   threshold of a real chip is unknown). One summary per programming run in the log (until Vpp leaves 25 V).
#include <stdint.h>
#include <string>
#include <vector>
#include "chips/eprom27.h"
#include "chips/i8255.h"
#include "core/card.h"

class Dse1 : public Card, public I8255Peripheral {
public:
    Dse1() : Card("DSE-1") {}
    ~Dse1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int map_lines() override;
    void flush() override;
    bool unsaved() override;
    void describe(nlohmann::json& out) override;
    void state(StateIO& io) override;
    std::string live_status() override;
    int decode_io(uint8_t port, const char** what) override;
    // the EPROM in the socket of the programming cable: "eprom"
    const char* media_param(int u) override { return u == 0 ? "eprom" : nullptr; }
    bool media_change(int u, const std::string& image, std::string& err) override;
    void media_discard(int u) override { if (u == 0) chip.dirty = false; }

    // I8255Peripheral: the levels of the lines while a port is an input
    uint8_t pins_a() override;
    uint8_t pins_b() override;
    uint8_t pins_c() override;

    enum { CABLE_2716 = 0, CABLE_2732 = 1, CABLE_DUAL = 2 };   // x2_cable
    enum { VPP_PIN50 = 0, VPP_CONVERTER = 1, VPP_NONE = 2 };    // vpp

    // Media window, MCP: put an EPROM image into the socket (a file of media/ through its working copy, a dump
    // of roms/ through a copy in work/dse-1/), take it out
    bool insert(const std::string& image, std::string& err);
    void eject(void);
    // A new blank EPROM (all FFh, 2716 or 2732) as a new file `dir`/eprom_N.bin; rel = its path for insert()
    static bool new_blank(int type, const char* dir, std::string& rel, std::string& err);
    // emulator help (not the hardware): the simulator RAM into a .bin of 4 KB
    bool save_sim_ram(const std::string& path, std::string& err) const;

    // what the Media window and MCP show
    const Eprom27& eprom(void) const { return chip; }
    const uint8_t* sim_ram(void) const { return ram; }
    uint8_t reg(void) const { return regv; }
    int vpp_volts(void) const;          // 0, 5, 25
    bool led1(void) const { return (regv & 0x10) != 0; }
    bool led2(void) const { return (regv & 0x20) != 0; }
    bool onla(void) const { return (regv & 0x04) != 0; }
    bool onld(void) const { return (regv & 0x08) != 0; }
    int cable(void) const { return x2_cable; }
    int socket_wiring(void) const;      // CABLE_2716 or CABLE_2732: how the chip in the socket is wired
    double last_pulse_ms(void) const;   // the last programming pulse (0 = none yet)
    bool pulse_on(void) const { return pulse_active; }

    // A programming run: the pulses while Vpp is 25 V (summary in the log when it ends)
    struct Run {
        bool active = false;
        uint32_t pulses = 0;            // 1 ms or longer: programmed
        uint32_t short_pulses = 0;      // shorter: did not program
        uint64_t min_t = 0, max_t = 0;  // the shortest / longest programming pulse (bus time)
        uint32_t bytes = 0;             // cells that got a programming pulse
        double min_byte_ms = 0, max_byte_ms = 0;   // the pulses of a cell added up (after the run)
    };
    const Run& run(void) const { return runs; }
    std::vector<std::string> warnings(void) const;   // the warnings logged so far (once each)

private:
    // the levels of the inner buses and the socket after the last write
    struct Lines {
        uint16_t addr = 0xFFF;          // PA0-11
        bool addr_known = false;        // PA0-9 driven (8255 outputs or ON LINE)
        bool addr_driven = false;       // the 8255 drives some of PA0-11
        uint8_t pc = 0xFF;              // PC0-7 lines
        uint8_t pd = 0xFF;              // PD0-7
        int drivers = 0;                // DRV_* on PD
        EpromPins pins;
    };
    enum { DRV_PPI = 1, DRV_RAM = 2, DRV_EPROM = 4 };
    enum {   // warnings, logged once (bits of `warned`)
        W_PD_CLASH = 0,                 // + DRV_* combination (3, 5, 6, 7): bits 3..7
        W_ONLA = 8,                     // ON LINE address while PB / PC0-3 are outputs
        W_25V_P18 = 9, W_25V_P20 = 10, W_25V_P21 = 11,
        W_PULSE_ADDR = 12,              // the address changed during a pulse
    };

    Lines lines(void);
    uint8_t vpp_level(void) const;      // EpromLevel of the Vpp net
    void update(void);                  // after every write and reset: RAM write, pulses, warnings
    void warn_once(int bit, const char* fmt, ...);
    void pulse_end(void);
    void run_end(void);
    void take_out(void);                // the chip leaves the socket (written back when programmed)
    bool read_image(const std::string& image, Eprom27& to, std::string& err);

    I8255 ppi;
    uint8_t regv = 0;                   // 74LS174 (6 bits)
    uint8_t ram[4096];
    Eprom27 chip;
    // configuration
    uint8_t base = 0xF0;
    bool jp1 = true;
    int vpp_src = VPP_PIN50;
    int x2_cable = CABLE_2716;
    bool work_copy = true;
    // the programming pulse in progress
    bool pulse_active = false;
    uint64_t pulse_start = 0;
    uint16_t pulse_addr = 0;
    uint8_t pulse_data = 0xFF;          // PD while it lasts (AND)
    uint64_t last_pulse = 0;            // length of the last one (bus time)
    uint8_t last_vpp = EPL_L;
    Run runs;
    std::vector<uint32_t> run_us;       // per cell: the programming pulses of this run added up (us)
    uint32_t warned = 0;
};
