#ifndef I8080_I8080_H_
#define I8080_I8080_H_

// SAPIemu: an 8085 mode added (i8085 = true after i8080_init): 8085 T-states, RIM/SIM,
// TRAP and RST 5.5/6.5/7.5, the undocumented instructions and the V and K flags
// (docs/8085.md). The 8080 mode is the original core.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct i8080 {
  // memory + io interface
  uint8_t (*read_byte)(void*, uint16_t); // user function to read from memory
  void (*write_byte)(void*, uint16_t, uint8_t); // same for writing to memory
  uint8_t (*port_in)(void*, uint8_t); // user function to read from port
  void (*port_out)(void*, uint8_t, uint8_t); // same for writing to port
  void* userdata; // user custom pointer

  unsigned long cyc; // cycle count

  uint16_t pc, sp; // program counter, stack pointer
  uint8_t a, b, c, d, e, h, l; // registers
  // flags: sign, zero, half-carry, parity, carry, interrupt flip-flop
  bool sf : 1, zf : 1, hf : 1, pf : 1, cf : 1, iff : 1;
  bool halted : 1;

  bool interrupt_pending : 1;
  uint8_t interrupt_vector;
  uint8_t interrupt_delay;

  // 8085
  bool i8085 : 1;      // 8085 mode
  bool vf : 1, kf : 1; // V (overflow, PSW bit 1) and K (UI, PSW bit 5)
  bool sod : 1;        // SOD latch (SIM)
  bool sid : 1;        // SID input (RIM bit 7), set by the user
  bool trap : 1, rst75 : 1, rst65 : 1, rst55 : 1;   // input levels, set with i8085_lines()
  bool trap_ff : 1;    // TRAP: rising edge latched (taken while the pin stays high)
  bool r75_ff : 1;     // RST 7.5: rising edge latched
  bool trap_rim : 1;   // the next RIM returns trap_ie (IE before the TRAP)
  bool trap_ie : 1;
  uint8_t masks;       // M7.5 M6.5 M5.5 (bits 2..0, 1 = masked)
} i8080;

void i8080_init(i8080* const c);
void i8080_step(i8080* const c);
void i8080_interrupt(i8080* const c, uint8_t opcode);
void i8080_debug_output(i8080* const c, bool print_disassembly);

// 8085: the levels of TRAP, RST 7.5, RST 6.5 and RST 5.5 (rising edges of TRAP and
// RST 7.5 are latched).
void i8085_lines(i8080* const c, bool trap, bool rst75, bool rst65, bool rst55);
// 8085 PSW with V and K (8080: bit 1 = 1, bits 3 and 5 = 0)
uint8_t i8080_flags(const i8080* const c);

#endif // I8080_I8080_H_
