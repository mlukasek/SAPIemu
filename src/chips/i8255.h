#pragma once
// Intel 8255 PPI (MHB8255A). Modes 0, 1 (port B output) and 2 (port A) as used
// on IDE-1. The attached peripheral supplies input pin levels and is told
// about output changes; strobed modes are driven through strobe_a()/ack_a().
#include <stdint.h>

class I8255Peripheral {
public:
    virtual ~I8255Peripheral() {}
    // Pin levels of ports configured as inputs (mode 0).
    virtual uint8_t pins_a() { return 0xFF; }
    virtual uint8_t pins_b() { return 0xFF; }
    virtual uint8_t pins_c() { return 0xFF; }
    // An output latch changed (mode 0 ports and port C bits configured as outputs).
    virtual void outputs_changed(int port, uint8_t value) {}
};

struct I8255 {
    I8255Peripheral* dev = nullptr;

    void reset(void);                 // /RES: all ports input, mode 0
    uint8_t read(int reg);            // reg 0..3 = PA, PB, PC, control (control reads FF)
    void write(int reg, uint8_t value);

    // Mode 2 (port A) handshake, peripheral side
    bool a_output_full(void) const { return obf_a; }   // /OBF low: CPU wrote a byte
    bool a_input_full(void) const { return ibf_a; }    // IBF high: CPU has not read yet
    uint8_t ack_a(void);              // /ACK pulse: take the CPU byte, /OBF goes high
    void strobe_a(uint8_t data);      // /STB pulse: latch data for the CPU, IBF high
    bool intr_a(void) const;          // INTRA (PC3)

    uint8_t control(void) const { return ctrl; }
    bool mode2_a(void) const { return (ctrl & 0x40) != 0; }
    uint8_t out_a = 0, out_b = 0, out_c = 0;   // output latches

private:
    uint8_t pc_value(void);           // port C as read by the CPU
    void set_pc_bit(int bit, bool v);

    uint8_t ctrl = 0x9B;              // all inputs, mode 0
    uint8_t in_a = 0xFF;              // strobed input latch (mode 2)
    bool obf_a = false, ibf_a = false;
    bool inte1 = false, inte2 = false;    // mode 2: output / input interrupt enable
    bool obf_b = false, inte_b = false;   // mode 1 port B output
};
