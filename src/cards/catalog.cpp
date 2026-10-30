#include "cards/catalog.h"

#include <string.h>

// ---------------------------------------------------------------- parameters

static const ParamOption k_none[] = { { nullptr, nullptr, nullptr } };

// JPR-1ZI
// what is plugged into an Atari joystick connector (input/joyport.h): MPH-1V K4/K2, ANK-1M2
static const ParamOption k_joyport[] = {
    { "\"none\"", "nothing", "nic" },
    { "\"joystick\"", "joystick (Atari)", "joystick (Atari)" },
    { "\"mouse_atari\"", "mouse Atari ST", "myš Atari ST" },
    { "\"mouse_amiga\"", "mouse Amiga", "myš Amiga" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_cpu_mhz[] = {
    { "2", "2 MHz (position 3)", "2 MHz (poloha 3)" },
    { "4", "4 MHz (position 4)", "4 MHz (poloha 4)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_bus_mhz[] = {
    { "2", "2 MHz (position 2)", "2 MHz (poloha 2)" },
    { "4", "4 MHz (position 1)", "4 MHz (poloha 1)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_d3c[] = {
    { "\"2k\"", "ROM 0000-07FF (2 KB)", "ROM 0000-07FF (2 KB)" },
    { "\"8k\"", "ROM 0000-1FFF (8 KB)", "ROM 0000-1FFF (8 KB)" },
    { nullptr, nullptr, nullptr },
};
// PK_FILE options are suggestions
static const ParamOption k_jpr_roms[] = {
    { "\"roms/mikromon/MikroMon1_3_f000.txt\"", "MikroMon 1.3", "MikroMon 1.3" },
    { "\"roms/jpr-1zi/JPR-1ZI_ECMON_2k_Martin.bin\"", "ECMON 2k", "ECMON 2k" },
    { "\"roms/jpr-1zi/JPR-1ZI_ECMON_2k_IDE-1_Martin.bin\"", "ECMON 2k with IDE-1", "ECMON 2k s IDE-1" },
    { "\"roms/jpr-1zi/JPR-1ZI_ECMON_2k_EC1045.bin\"", "ECMON 2k EC1045 (menu FDC8, FDC3, FDC5, HDD)",
      "ECMON 2k EC1045 (menu FDC8, FDC3, FDC5, HDD)" },
    { "\"roms/jpr-1zi/JPR-1ZI_MON281211_JB_MFC.bin\"", "MON 281211 J. Biba (CP/M from an 8\" disk, MFC-1A)",
      "MON 281211 J. Bíba (CP/M z 8\" diskety, MFC-1A)" },
    { "\"roms/jpr-1zi/JPR-1ZI_MON130711_JB_FDC.bin\"", "MON 130711 J. Biba (CP/M from an 8\" disk, FDC-1)",
      "MON 130711 J. Bíba (CP/M z 8\" diskety, FDC-1)" },
    { "\"roms/jpr-1zi/JPR-1ZI_MON271111_JB_RPD.bin\"", "MON 271111 J. Biba (CP/M from an 8\" disk, RPD-1)",
      "MON 271111 J. Bíba (CP/M z 8\" diskety, RPD-1)" },
    { "\"roms/jpr-1zi/JPR-1ZI_MON260112_JB_RPD_FDC.bin\"", "MON 260112 J. Biba (long CP/M, RPD-1 + FDC-1 HD)",
      "MON 260112 J. Bíba (dlouhé CP/M, RPD-1 + FDC-1 HD)" },
    { "\"roms/jpr-1zi/JPR-1ZI_MON151111_JB_64K_TERM.bin\"", "MON 151111 64K J. Biba (CP/M 64K, terminal only, FDC-1)",
      "MON 151111 64K J. Bíba (CP/M 64K, jen terminál, FDC-1)" },
    { "\"roms/jpr-1zi/JPR-1ZI_ZZRMD_epr2k.bin\"", "ZZRMD 2k", "ZZRMD 2k" },
    { "\"roms/jpr-1zi/JPR-1ZI_ZZRMD_epr8k.bin\"", "ZZRMD 8k", "ZZRMD 8k" },
    { "\"\"", "none (FFh)", "žádná (FFh)" },
    { nullptr, nullptr, nullptr },
};
// printer (chips/printer.h): on the JPR cards (X3) and on DPP-1
static const ParamOption k_prn_charset[] = {
    { "\"koi8cs\"", "KOI-8 cs2 (as SAPI)", "KOI-8 čs2 (jako SAPI)" },
    { "\"kamenicky\"", "Kamenicky (KEYBCS2)", "Kamenických (KEYBCS2)" },
    { "\"cp852\"", "CP852 (Latin 2)", "CP852 (Latin 2)" },
    { "\"ascii\"", "ASCII only", "jen ASCII" },
    { nullptr, nullptr, nullptr },
};
#define PRINTER_PARAMS                                                                                                 \
    { "printer.output", PK_FILE, "Printer: output file (the bytes as received, ESC/P kept)",                           \
      "Tiskárna: výstupní soubor (bajty, jak přišly, i ESC/P)", "\"work/printer/tiskarna.prn\"", k_none, 0, 0 },     \
    { "printer.charset", PK_CHOICE, "Printer: character set of the text view (80h-FFh)",                              \
      "Tiskárna: znaková sada textového zobrazení (80h-FFh)", "\"koi8cs\"", k_prn_charset, 0, 0 },                   \
    { "printer.busy_us", PK_INT, "Printer: BUSY after each byte (us; 0 = at once, a deviation)",                       \
      "Tiskárna: BUSY po každém bajtu (µs; 0 = hned, odchylka)", "100", nullptr, 0, 1000000 },

static const ParamOption k_xy4130_cable[] = {
    { "\"mikos\"", "the driver behind MIKOS V5 (D0 pen, D1 X/Y, D6 +/-, D7 step; sapi.cz doku2/xy4130.php)",
      "ovladač za MIKOSem V5 (D0 pero, D1 X/Y, D6 +/-, D7 krok; sapi.cz doku2/xy4130.php)" },
    { "\"kres\"", "KRES of BASIC EXP V5.0/G+SZ (D0 step, D2 X/Y, D3 +/-, D5 READY, D7 pen)",
      "KRES z BASICu EXP V5.0/G+SZ (D0 krok, D2 X/Y, D3 +/-, D5 READY, D7 pero)" },
    { nullptr, nullptr, nullptr },
};
#define XY4130_PARAMS(def_cable)                                                                                         { "xy4130.cable", PK_CHOICE, "XY 4130: cable", "XY 4130: kabel", def_cable, k_xy4130_cable, 0, 0 },                   { "xy4130.pen_on", PK_BOOL, "XY 4130: PEN pressed at power on (a deviation; the plotter waits for the PEN button)",        "XY 4130: PEN stisknuté už po zapnutí (odchylka; zapisovač čeká na tlačítko PEN)", "false", nullptr, 0, 0 },

static const ParamOption k_z80_type[] = {
    { "\"nmos\"", "NMOS (Z80A, U880...): an interrupt is taken after LD A,I / LD A,R",
      "NMOS (Z80A, U880...): přerušení se přijme i po LD A,I / LD A,R" },
    { "\"cmos\"", "CMOS (Z84C00): no interrupt right after LD A,I / LD A,R (as after EI)",
      "CMOS (Z84C00): hned po LD A,I / LD A,R se přerušení nepřijme (jako po EI)" },
    { nullptr, nullptr, nullptr },
};
#define Z80_TYPE_PARAM(def)                                                                                            \
    { "z80", PK_CHOICE, "Z80 chip: NMOS or CMOS", "Čip Z80: NMOS, nebo CMOS", def, k_z80_type, 0, 0 },
static const ParamOption k_open_bus[] = {
    { "\"last\"", "the last byte of the data bus (as the board)", "poslední bajt datové sběrnice (jako deska)" },
    { "\"ff\"", "FFh (a deviation from the board)", "FFh (odchylka od desky)" },
    { nullptr, nullptr, nullptr },
};
#define OPEN_BUS_PARAM                                                                                                 \
    { "open_bus", PK_CHOICE, "A read nobody answers gives", "Čtení, na které nikdo neodpoví, vrací", "\"last\"",      \
      k_open_bus, 0, 0 },
static const ParamOption k_ststb[] = {
    { "\"mreq_iorq\"", "from /MREQ and /IORQ, as the board (an interrupt with the PC in a video card window freezes)",
      "z /MREQ i /IORQ, jako deska (přerušení s PC v okně videokarty zamrzne)" },
    { "\"mreq\"", "from /MREQ only (patch IC15 of L. Lasota: no STSTB at INTA, no freeze)",
      "jen z /MREQ (záplata IC15 L. Lasoty: bez STSTB při INTA, nezamrzá)" },
    { nullptr, nullptr, nullptr },
};

static const CardParam k_jpr1zi[] = {
    { "rom", PK_FILE, "EPROM 2764", "EPROM 2764", "\"\"", k_jpr_roms, 0, 0 },
    { "decoder", PK_CHOICE, "PROM D3C: memory decoder", "PROM D3C: dekodér paměti", "\"2k\"", k_d3c, 0, 0 },
    { "io_decoder", PK_FILE, "PROM D6B: I/O decoder", "PROM D6B: dekodér I/O",
      "\"roms/jpr-1zi/JPR-1ZI_D6B.bin\"", k_none, 0, 0 },
    { "cpu_mhz", PK_CHOICE, "JP2/JP3: CPU clock", "JP2/JP3: takt CPU", "2", k_cpu_mhz, 0, 0 },
    { "bus_mhz", PK_CHOICE, "JP2/JP3: bus clock F2", "JP2/JP3: takt sběrnice F2", "2", k_bus_mhz, 0, 0 },
    { "crystal_hz", PK_INT, "Crystal (Hz)", "Krystal (Hz)", "16000000", nullptr, 1000000, 40000000 },
    Z80_TYPE_PARAM("\"nmos\"")
    OPEN_BUS_PARAM
    { "keyboard", PK_KEYBOARD, "Keyboard (connector X2)", "Klávesnice (konektor X2)", "null", nullptr, 0, 0 },
    { "keyboard.strict", PK_BOOL, "Keyboard: true to the hardware (keys may be lost)",
      "Klávesnice: věrně jako hardware (klávesy se mohou ztratit)", "false", nullptr, 0, 0 },
    { "keyboard.beeper", PK_BOOL, "Keyboard: beeper (EKL-1, Consul: buzzer on X2 pin 14; ANK-1: P0-OUT D6)",
      "Klávesnice: bzučák (EKL-1, Consul: bzučák na X2 pin 14; ANK-1: P0-OUT D6)", "true", nullptr, 0, 0 },
    { "keyboard.joyport", PK_CHOICE, "Keyboard ANK-1M2: joystick connector (P1-IN D5-D0)",
      "Klávesnice ANK-1M2: konektor joysticku (P1-IN D5-D0)", "\"joystick\"", k_joyport, 0, 0 },
    { "printer", PK_SECTION, "X3: printer (Centronics cable: data P2-OUT, /STROBE P1-OUT1, BUSY P2-IN6)",
      "X3: tiskárna (kabel Centronics: data P2-OUT, /STROBE P1-OUT1, BUSY P2-IN6)", "false", nullptr, 0, 0 },
    PRINTER_PARAMS
    { "printer.pin16", PK_BOOL, "Printer: X3 pin 16 (P2-IN7) at 1 (ECMON then drops the characters)",
      "Tiskárna: X3 kolík 16 (P2-IN7) v 1 (ECMON pak znaky zahodí)", "false", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// JPR-1Z (ZPS-4)
static const ParamOption k_jpr1z_roms[] = {
    { "\"roms/jpr-1z/MOR62Z.bin\"", "MOR62Z: CP/M 2.2/P 62K (Perina 1988)", "MOR62Z: CP/M 2.2/P 62K (Peřina 1988)" },
    { "\"roms/jpr-1z/MOR62ZF.bin\"", "MOR62ZF: CP/M 2.2/P 62K for FDC-1 (Perina 1988)",
      "MOR62ZF: CP/M 2.2/P 62K pro FDC-1 (Peřina 1988)" },
    { "\"roms/jpr-1z/ELIS_V7.01.bin\"", "ELIS v7.01 (Z80, C259.11/C262.3)", "ELIS v7.01 (Z80, C259.11/C262.3)" },
    { "\"roms/jpr-1z/ELIS_V3.00.bin\"", "ELIS v3.00 (8080, C262.3)", "ELIS v3.00 (8080, C262.3)" },
    { "\"roms/jpr-1z/ROMES_RKD1_870210.bin\"", "Boot ROM RKD-1 (Tesla Elstroj 1987, cartridge disk CM 5403)",
      "Boot ROM RKD-1 (Tesla Elstroj 1987, kazetový disk CM 5403)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_jpr1z[] = {
    { "rom", PK_FILE, "EPROM 2716", "EPROM 2716", "\"\"", k_jpr1z_roms, 0, 0 },
    { "decoder", PK_FILE, "PROM D14: memory decoder", "PROM D14: dekodér paměti", "\"roms/jpr-1z/JPR-1Z_D14_74S287.bin\"",
      k_none, 0, 0 },
    { "io_decoder", PK_FILE, "PROM D10: I/O decoder", "PROM D10: dekodér I/O", "\"roms/jpr-1z/JPR-1Z_D10_74S287.bin\"",
      k_none, 0, 0 },
    { "cpu_mhz", PK_CHOICE, "JP2/JP3: CPU clock", "JP2/JP3: takt CPU", "2", k_cpu_mhz, 0, 0 },
    { "bus_mhz", PK_CHOICE, "JP2/JP3: bus clock F2", "JP2/JP3: takt sběrnice F2", "2", k_bus_mhz, 0, 0 },
    { "crystal_hz", PK_INT, "Crystal (Hz)", "Krystal (Hz)", "16000000", nullptr, 1000000, 40000000 },
    Z80_TYPE_PARAM("\"nmos\"")
    OPEN_BUS_PARAM
    { "keyboard", PK_KEYBOARD, "Keyboard (connector X2)", "Klávesnice (konektor X2)", "null", nullptr, 0, 0 },
    { "keyboard.strict", PK_BOOL, "Keyboard: true to the hardware (keys may be lost)",
      "Klávesnice: věrně jako hardware (klávesy se mohou ztratit)", "false", nullptr, 0, 0 },
    { "keyboard.beeper", PK_BOOL, "Keyboard: beeper (EKL-1, Consul: buzzer on X2 pin 14; ANK-1: P0-OUT D6)",
      "Klávesnice: bzučák (EKL-1, Consul: bzučák na X2 pin 14; ANK-1: P0-OUT D6)", "true", nullptr, 0, 0 },
    { "keyboard.joyport", PK_CHOICE, "Keyboard ANK-1M2: joystick connector (P1-IN D5-D0)",
      "Klávesnice ANK-1M2: konektor joysticku (P1-IN D5-D0)", "\"joystick\"", k_joyport, 0, 0 },
    { "printer", PK_SECTION, "X3: printer (Centronics cable: data P2-OUT, /STROBE P1-OUT1, BUSY P2-IN6)",
      "X3: tiskárna (kabel Centronics: data P2-OUT, /STROBE P1-OUT1, BUSY P2-IN6)", "false", nullptr, 0, 0 },
    PRINTER_PARAMS
    { "printer.pin16", PK_BOOL, "Printer: X3 pin 16 (P2-IN7) at 1 (ECMON then drops the characters)",
      "Tiskárna: X3 kolík 16 (P2-IN7) v 1 (ECMON pak znaky zahodí)", "false", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// JPR-1V (Libor Lasota)
static const ParamOption k_jpr1v_roms[] = {
    { "\"roms/jpr-1v/JPR-1V_28C64_MikroBIOS_ECMON.bin\"", "bank 0 MikroBIOS 3.0, bank 1 ECMON IDE-1",
      "banka 0 MikroBIOS 3.0, banka 1 ECMON IDE-1" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_jpr1v_bios[] = {
    { "\"fitted\"", "fitted: bank 0 (EEPROM 0000-07FF)", "osazená: banka 0 (EEPROM 0000-07FF)" },
    { "\"open\"", "open: bank 1 (EEPROM 0800-0FFF)", "rozpojená: banka 1 (EEPROM 0800-0FFF)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_jpr1v[] = {
    { "rom", PK_FILE, "EEPROM 28C64 (two 2 KB banks)", "EEPROM 28C64 (dvě banky po 2 KB)",
      "\"roms/jpr-1v/JPR-1V_28C64_MikroBIOS_ECMON.bin\"", k_jpr1v_roms, 0, 0 },
    { "bios", PK_CHOICE, "Jumper BIOS: EEPROM bank", "Propojka BIOS: banka EEPROM", "\"fitted\"", k_jpr1v_bios, 0, 0 },
    { "turbo", PK_BOOL, "TURBO: CPU 4 MHz (else 2 MHz), bus always 2 MHz", "TURBO: CPU 4 MHz (jinak 2 MHz), sběrnice vždy 2 MHz",
      "false", nullptr, 0, 0 },
    { "osc_hz", PK_INT, "Oscillator (Hz, bus = /4, CPU = /4 or /2)", "Oscilátor (Hz, sběrnice = /4, CPU = /4 nebo /2)",
      "8000000", nullptr, 1000000, 40000000 },
    Z80_TYPE_PARAM("\"cmos\"")
    OPEN_BUS_PARAM
    { "ststb", PK_CHOICE, "STSTB", "STSTB", "\"mreq_iorq\"", k_ststb, 0, 0 },
    { "keyboard", PK_KEYBOARD, "Keyboard (connector K2)", "Klávesnice (konektor K2)", "null", nullptr, 0, 0 },
    { "keyboard.strict", PK_BOOL, "Keyboard: true to the hardware (keys may be lost)",
      "Klávesnice: věrně jako hardware (klávesy se mohou ztratit)", "false", nullptr, 0, 0 },
    { "keyboard.beeper", PK_BOOL, "Keyboard: beeper (EKL-1, Consul: buzzer on X2 pin 14; ANK-1: P0-OUT D6)",
      "Klávesnice: bzučák (EKL-1, Consul: bzučák na X2 pin 14; ANK-1: P0-OUT D6)", "true", nullptr, 0, 0 },
    { "keyboard.joyport", PK_CHOICE, "Keyboard ANK-1M2: joystick connector (P1-IN D5-D0)",
      "Klávesnice ANK-1M2: konektor joysticku (P1-IN D5-D0)", "\"joystick\"", k_joyport, 0, 0 },
    { "printer", PK_SECTION, "X3: printer (Centronics cable: data P2-OUT, /STROBE P1-OUT1, BUSY P2-IN6)",
      "X3: tiskárna (kabel Centronics: data P2-OUT, /STROBE P1-OUT1, BUSY P2-IN6)", "false", nullptr, 0, 0 },
    PRINTER_PARAMS
    { "printer.pin16", PK_BOOL, "Printer: X3 pin 16 (P2-IN7) at 1 (ECMON then drops the characters)",
      "Tiskárna: X3 kolík 16 (P2-IN7) v 1 (ECMON pak znaky zahodí)", "false", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// DSM-1V (Libor Lasota)
static const ParamOption k_dsm1v_base[] = {
    { "\"10h\"", "10h", "10h" }, { "\"14h\"", "14h", "14h" }, { "\"18h\"", "18h", "18h" }, { "\"1Ch\"", "1Ch", "1Ch" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dsm1v_baud[] = {
    { "115200", "115200 (N = FEh)", "115200 (N = FEh)" }, { "57600", "57600 (N = FCh)", "57600 (N = FCh)" },
    { "38400", "38400 (N = FAh)", "38400 (N = FAh)" }, { "19200", "19200 (N = F4h)", "19200 (N = F4h)" },
    { "9600", "9600 (N = E8h)", "9600 (N = E8h)" }, { "4800", "4800 (N = D0h)", "4800 (N = D0h)" },
    { "2400", "2400 (N = A0h)", "2400 (N = A0h)" }, { "1200", "1200 (N = 40h)", "1200 (N = 40h)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_dsm1v[] = {
    { "a", PK_SECTION, "Channel A (K3)", "Kanál A (K3)", "true", nullptr, 0, 0 },
    { "a.enabled", PK_BOOL, "S3 5-6: channel enabled", "S3 5-6: kanál povolen", "true", nullptr, 0, 0 },
    { "a.base", PK_CHOICE, "S3 1-2, 3-4: ports", "S3 1-2, 3-4: porty", "\"10h\"", k_dsm1v_base, 0, 0 },
    { "a.baud", PK_CHOICE, "S2: baud rate", "S2: rychlost", "115200", k_dsm1v_baud, 0, 0 },
    { "a.connected", PK_BOOL, "Terminal connected (DCD)", "Terminál připojen (DCD)", "true", nullptr, 0, 0 },
    { "b", PK_SECTION, "Channel B (K2)", "Kanál B (K2)", "true", nullptr, 0, 0 },
    { "b.enabled", PK_BOOL, "S3 11-12: channel enabled", "S3 11-12: kanál povolen", "true", nullptr, 0, 0 },
    { "b.base", PK_CHOICE, "S3 7-8, 9-10: ports", "S3 7-8, 9-10: porty", "\"14h\"", k_dsm1v_base, 0, 0 },
    { "b.baud", PK_CHOICE, "S1: baud rate", "S1: rychlost", "115200", k_dsm1v_baud, 0, 0 },
    { "b.connected", PK_BOOL, "Terminal connected (DCD)", "Terminál připojen (DCD)", "true", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// AND-1B (Libor Lasota): S2 address and INT1, S1 colour like AND-1V
static const ParamOption k_and1b_base[] = {
    { "\"3800h\"", "3800h (7-8 fitted, AND-1, MIKROBASIC)", "3800h (osazená 7-8, AND-1, MIKROBASIC)" },
    { "\"E800h\"", "E800h (5-6 fitted, AND-1A in ZPS-3)", "E800h (osazená 5-6, AND-1A v ZPS-3)" },
    { "\"F800h\"", "F800h (both open)", "F800h (obě rozpojené)" },
    { "\"2800h\"", "2800h (both fitted)", "2800h (obě osazené)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_and1b_int1[] = {
    { "\"off\"", "off (1-2 and 3-4 open)", "vypnuto (1-2 i 3-4 rozpojené)" },
    { "\"50hz\"", "50 Hz (1-2 fitted, 5 pulses of 6 frames)", "50 Hz (osazená 1-2, 5 pulzů ze 6 snímků)" },
    { "\"60hz\"", "60 Hz (3-4 fitted, every frame)", "60 Hz (osazená 3-4, každý snímek)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_and1b_fonts[] = {
    { "\"roms/and-1b/FontAND1B11.bin\"", "FontAND1B11 (Libor Lasota, MHB 2501 set)", "FontAND1B11 (Libor Lasota, sada MHB 2501)" },
    { nullptr, nullptr, nullptr },
};
extern const ParamOption k_and1v_color[];
static const CardParam k_and1b[] = {
    { "base", PK_CHOICE, "S2 /A15:14, /A12: VRAM window (2 KB)", "S2 /A15:14, /A12: okno VRAM (2 KB)", "\"3800h\"",
      k_and1b_base, 0, 0 },
    { "int1", PK_CHOICE, "S2 50Hz / 60Hz: INT1 at the end of the picture (317 us)",
      "S2 50Hz / 60Hz: INT1 na konci obrazu (317 us)", "\"off\"", k_and1b_int1, 0, 0 },
    { "color", PK_CHOICE, "S1 /R GL GH /B: ink colour", "S1 /R GL GH /B: barva", "\"green\"", k_and1v_color, 0, 0 },
    { "font", PK_FILE, "Character generator 27C256 (upper half)", "Generátor znaků 27C256 (horní polovina)",
      "\"roms/and-1b/FontAND1B11.bin\"", k_and1b_fonts, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// BGP-1S (Libor Lasota): S3 window and MAP1, S1 colours, S2 interrupt
static const ParamOption k_bgp1s_base[] = {
    { "\"C000h\"", "C000h (AD15, AD14 fitted, like PMD-85)", "C000h (osazené AD15, AD14, jako PMD-85)" },
    { "\"8000h\"", "8000h (AD15 fitted)", "8000h (osazená AD15)" },
    { "\"4000h\"", "4000h (AD14 fitted)", "4000h (osazená AD14)" },
    { "\"0000h\"", "0000h (none)", "0000h (žádná)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_bgp1s_map1[] = {
    { "\"ignore\"", "IGNORE (MDIS fitted)", "IGNORE (osazená MDIS)" },
    { "\"high\"", "HIGH: when MAP1 = H (none fitted)", "HIGH: při MAP1 = H (nic neosazeno)" },
    { "\"low\"", "LOW: when MAP1 = L (MLEV fitted)", "LOW: při MAP1 = L (osazená MLEV)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_bgp1s_colors[] = {
    { "\"ace\"", "ACE (Colorace, 7 colours of 6x2 cells)", "ACE (Colorace, 7 barev buněk 6x2)" },
    { "\"rgbm\"", "RGBM (4 colours like PMD-85)", "RGBM (4 barvy jako PMD-85)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_bgp1s_int[] = {
    { "\"int0\"", "/INT0", "/INT0" }, { "\"int1\"", "/INT1", "/INT1" },
    { "\"none\"", "none (no jumper)", "žádné (bez propojky)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_bgp1s_output[] = {
    { "\"rgb\"", "colour TV (RGB, SCART)", "barevná TV (RGB, SCART)" },
    { "\"mono\"", "B/W TV (composite)", "černobílá TV (kompozit)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_bgp1s[] = {
    { "base", PK_CHOICE, "S3 AD15, AD14: video RAM window (16 KB, write only)",
      "S3 AD15, AD14: okno videoram (16 KB, jen zápis)", "\"C000h\"", k_bgp1s_base, 0, 0 },
    { "map1", PK_CHOICE, "S3 MDIS, MLEV: MAP1 mode", "S3 MDIS, MLEV: režim MAP1", "\"ignore\"", k_bgp1s_map1, 0, 0 },
    { "colors", PK_CHOICE, "S1: colours", "S1: barvy", "\"ace\"", k_bgp1s_colors, 0, 0 },
    { "int", PK_CHOICE, "S2: interrupt at line 256 (cleared by a fetch from the window)",
      "S2: přerušení na řádku 256 (nuluje ho fetch z okna)", "\"int0\"", k_bgp1s_int, 0, 0 },
    { "output", PK_CHOICE, "K2: TV on", "K2: TV na výstupu", "\"rgb\"", k_bgp1s_output, 0, 0 },
    { "wait_states", PK_BOOL, "Wait for the CPU slot when writing (RDY)", "Čekání na okno CPU při zápisu (RDY)",
      "true", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// AND-1V (Libor Lasota)
static const ParamOption k_and1v_map[] = {
    { "\"always\"", "MAP1OFF fitted: always", "MAP1OFF osazená: vždy" },
    { "\"map1_h\"", "MAP1OFF open, H/L fitted: MAP1 = H", "MAP1OFF rozpojená, H/L osazená: MAP1 = H" },
    { "\"map1_l\"", "both open: MAP1 = L", "obě rozpojené: MAP1 = L" },
    { "\"never\"", "both fitted: disabled", "obě osazené: vypnuto" },
    { nullptr, nullptr, nullptr },
};
const ParamOption k_and1v_color[] = {
    { "\"white\"", "white (S1 all open)", "bílá (S1 vše rozpojené)" }, { "\"green\"", "green", "zelená" },
    { "\"amber\"", "amber", "jantarová" }, { "\"red\"", "red", "červená" }, { "\"blue\"", "blue", "modrá" },
    { "\"cyan\"", "cyan", "azurová" }, { "\"yellow\"", "yellow", "žlutá" }, { "\"magenta\"", "magenta", "purpurová" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_and1v_fonts[] = {
    { "\"roms/and-1v/font_and1v22_1024.bin\"", "10x24 type 1 (font_and1v22, on the author's board)",
      "10x24 typ 1 (font_and1v22, na desce autora)" },
    { "\"roms/and-1v/newfont_and1v22_1024.bin\"", "10x24 type 2 (newfont_and1v22: A rounded, 0 without a slash)",
      "10x24 typ 2 (newfont_and1v22: zaoblené A, 0 bez přeškrtnutí)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_and1v[] = {
    { "base", PK_HEX, "S2 /A11-/A15: VRAM base (2 KB steps)", "S2 /A11-/A15: začátek VRAM (po 2 KB)", "\"F800h\"",
      nullptr, 0, 0xF800 },
    { "map", PK_CHOICE, "S2 MAP1OFF, H/L", "S2 MAP1OFF, H/L", "\"map1_l\"", k_and1v_map, 0, 0 },
    { "color", PK_CHOICE, "S1: ink colour", "S1: barva", "\"white\"", k_and1v_color, 0, 0 },
    { "font", PK_FILE, "Character generator 27C256", "Generátor znaků 27C256", "\"roms/and-1v/font_and1v22_1024.bin\"",
      k_and1v_fonts, 0, 0 },
    { "wait_states", PK_BOOL, "Wait states (CPU waits for its slot of the video)",
      "Čekací stavy (CPU čeká na svůj takt videa)", "true", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// CGA-1V (Libor Lasota)
static const ParamOption k_cga1v_base[] = {
    { "\"C000h\"", "C000h (/A15 and /A14 open)", "C000h (/A15 i /A14 rozpojené)" },
    { "\"8000h\"", "8000h (/A14 fitted)", "8000h (/A14 osazená)" },
    { "\"4000h\"", "4000h (/A15 fitted)", "4000h (/A15 osazená)" },
    { "\"0000h\"", "0000h (/A15 and /A14 fitted)", "0000h (/A15 i /A14 osazené)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_cga1v_map[] = {
    { "\"h\"", "EN fitted: MAP = H", "EN osazená: MAP = H" },
    { "\"l\"", "EN and LO fitted: MAP = L", "EN a LO osazené: MAP = L" },
    { "\"ignore\"", "both open: MAP ignored", "obě rozpojené: MAP se ignoruje" },
    { "\"never\"", "LO fitted only: card disabled", "jen LO osazená: karta vypnutá" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_cga1v[] = {
    { "base", PK_CHOICE, "S1 /A15 /A14: base (16 KB)", "S1 /A15 /A14: báze (16 KB)", "\"C000h\"", k_cga1v_base, 0, 0 },
    { "map1", PK_CHOICE, "S1 M1EN M1LO: MAP1", "S1 M1EN M1LO: MAP1", "\"h\"", k_cga1v_map, 0, 0 },
    { "map2", PK_CHOICE, "S1 M2EN M2LO: MAP2", "S1 M2EN M2LO: MAP2", "\"h\"", k_cga1v_map, 0, 0 },
    { "wait_states", PK_BOOL, "Wait states (CPU waits for its slot of the video)",
      "Čekací stavy (CPU čeká na svůj takt videa)", "true", nullptr, 0, 0 },
    { "frame_blend", PK_BOOL, "Blend the last two frames (page flipping every frame, like the eye)",
      "Prolínat dva poslední snímky (střídání stránek po snímcích, jako oko)", "true", nullptr, 0, 0 },
    { "beam", PK_BOOL, "Draw line by line with the beam (changes in the middle of a frame show, as on the monitor)",
      "Kreslit po řádcích s paprskem (změny uprostřed snímku jsou vidět jako na monitoru)", "true", nullptr, 0, 0 },
    { "palette_snow", PK_BOOL, "Show palette writes during the display as dots (Bt476 \"snow\", approximate)",
      "Zápis do palety během zobrazování ukázat jako body („sníh“ Bt476, přibližně)", "false", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// DGD-1V (Libor Lasota): S2 like S1 of CGA-1V, S1 colour like AND-1V
static const ParamOption k_dgd1v_map[] = {
    { "\"ignore\"", "both open: MAP ignored", "obě rozpojené: MAP se ignoruje" },
    { "\"h\"", "EN fitted: MAP = H", "EN osazená: MAP = H" },
    { "\"l\"", "EN and LOW fitted: MAP = L", "EN i LOW osazené: MAP = L" },
    { "\"never\"", "only LOW fitted: card off", "jen LOW osazená: karta vypnutá" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_dgd1v[] = {
    { "base", PK_CHOICE, "S2 /A15 /A14: graphics RAM 16 KB from", "S2 /A15 /A14: grafická RAM 16 KB od", "\"C000h\"",
      k_cga1v_base, 0, 0 },
    { "map1", PK_CHOICE, "S2 MAP1EN MAP1LOW: MAP1", "S2 MAP1EN MAP1LOW: MAP1", "\"ignore\"", k_dgd1v_map, 0, 0 },
    { "map2", PK_CHOICE, "S2 MAP2EN MAP2LOW: MAP2", "S2 MAP2EN MAP2LOW: MAP2", "\"ignore\"", k_dgd1v_map, 0, 0 },
    { "color", PK_CHOICE, "S1: ink colour", "S1: barva", "\"white\"", k_and1v_color, 0, 0 },
    { "wait_states", PK_BOOL, "Wait states (CPU waits for its slot of the video)",
      "Čekací stavy (CPU čeká na svůj takt videa)", "true", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// RAM-1V (Libor Lasota)
static const ParamOption k_ram1v_cond[] = {
    { "\"off\"", "no hole", "bez díry" },
    { "\"ignore\"", "MAP ignored", "MAP se ignoruje" },
    { "\"h\"", "only when MAP = H", "jen při MAP = H" },
    { "\"l\"", "only when MAP = L", "jen při MAP = L" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_ram1v_c[] = {
    { "\"none\"", "empty", "prázdná" }, { "\"sram\"", "62256 SRAM", "SRAM 62256" },
    { "\"m48t35\"", "M48T35 Timekeeper (RTC at 7FF8-7FFF)", "M48T35 Timekeeper (RTC na 7FF8-7FFF)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_ram1v_csel[] = {
    { "\"never\"", "never", "nikdy" }, { "\"map1\"", "1-2: MAP1 = H", "1-2: MAP1 = H" },
    { "\"map2\"", "3-4: MAP2 = H", "3-4: MAP2 = H" }, { nullptr, nullptr, nullptr },
};
static const CardParam k_ram1v[] = {
    { "sram_a", PK_BOOL, "SRAM A (0000-7FFF)", "SRAM A (0000-7FFF)", "true", nullptr, 0, 0 },
    { "sram_b", PK_BOOL, "SRAM B (8000-FFFF)", "SRAM B (8000-FFFF)", "true", nullptr, 0, 0 },
    { "sram_c", PK_CHOICE, "Socket C (instead of A)", "Patice C (místo A)", "\"none\"", k_ram1v_c, 0, 0 },
    { "c_select", PK_CHOICE, "S4 CONF: chip C when", "S4 CONF: obvod C při", "\"never\"", k_ram1v_csel, 0, 0 },
    { "hole16_base", PK_HEX, "S5 /A14 /A15: 16 KB hole at", "S5 /A14 /A15: díra 16 KB na", "\"C000h\"", nullptr, 0, 0xC000 },
    { "hole16_map1", PK_CHOICE, "S5 MAP1MODE", "S5 MAP1MODE", "\"h\"", k_ram1v_cond, 0, 0 },
    { "hole16_map2", PK_CHOICE, "S5 MAP2MODE", "S5 MAP2MODE", "\"ignore\"", k_ram1v_cond, 0, 0 },
    { "hole2_base", PK_HEX, "S6 /A11-/A15: 2 KB hole at", "S6 /A11-/A15: díra 2 KB na", "\"F800h\"", nullptr, 0, 0xF800 },
    { "hole2_map1", PK_CHOICE, "S6 MAP1MODE", "S6 MAP1MODE", "\"l\"", k_ram1v_cond, 0, 0 },
    { "map_port", PK_HEX, "S3: MAP register port", "S3: port registru MAP", "\"63h\"", nullptr, 0, 0xFF },
    { "map1_enable", PK_BOOL, "S1 EN: drive MAP1 (pin 21)", "S1 EN: budit MAP1 (pin 21)", "true", nullptr, 0, 0 },
    { "map1_bit", PK_INT, "S1 B1/B2/B4: MAP1 data bit", "S1 B1/B2/B4: bit dat pro MAP1", "6", nullptr, 0, 7 },
    { "map2_enable", PK_BOOL, "S2 EN: drive MAP2 (pin 22)", "S2 EN: budit MAP2 (pin 22)", "true", nullptr, 0, 0 },
    { "map2_bit", PK_INT, "S2 B1/B2/B4: MAP2 data bit", "S2 B1/B2/B4: bit dat pro MAP2", "7", nullptr, 0, 7 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// JPR-1 (8080)
static const ParamOption k_eprom_type[] = {
    { "\"2708\"", "2708 (1 KB)", "2708 (1 KB)" },
    { "\"2716\"", "2716 (2 KB)", "2716 (2 KB)" },
    { nullptr, nullptr, nullptr },
};
// Redukce34KB01 (Libor Lasota): the keyboard cable adapter with an RS-232 receive line on P1-IN7
static const ParamOption k_k2_adapter[] = {
    { "\"none\"", "plain (FRB30 - IDC34)", "obyčejná (FRB30 - IDC34)" },
    { "\"redukce34kb01\"", "Redukce34KB01 rev 2.0: RS-232 RX on P1-IN7 (INT, T keys not connected)",
      "Redukce34KB01 rev 2.0: RS-232 RX na P1-IN7 (klávesy INT, T odpojené)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_jpr1_roms[] = {
    { "\"\"", "none (ZPS-1: EPROMs on REM-1)", "žádné (ZPS-1: EPROM na REM-1)" },
    { "\"roms/mikrobasic/MIKROBASIC_AR1983_1k.bin\"", "MIKROBASIC AR/B 1983 (1 KB RAM on JPR-1)",
      "MIKROBASIC AR/B 1983 (1 KB RAM na JPR-1)" },
    { "\"roms/mikrobasic/MIKROBASIC_2.4_ZPS-1.bin\"", "MIKROBASIC 2.4", "MIKROBASIC 2.4" },
    { "\"roms/ar1990/PLOTTER_AR1990.bin\"", "Plotter of AR 1990 (V. Julius; 2x 2716, RAM needed)",
      "Plotter z AR 1990 (V. Julius; 2x 2716, potřebuje RAM)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_jpr1[] = {
    { "rom", PK_FILE, "EPROM sockets D4E-D7E (from 0000)", "Patice EPROM D4E-D7E (od 0000)", "\"\"", k_jpr1_roms, 0, 0 },
    { "eprom_type", PK_CHOICE, "Jumpers 7-8/4-5 or 9-7/4-6: EPROM type", "Propojky 7-8/4-5 nebo 9-7/4-6: typ EPROM",
      "\"2708\"", k_eprom_type, 0, 0 },
    { "ram", PK_BOOL, "RAM 1 KB at 2000 (2x 2114)", "RAM 1 KB na 2000 (2x 2114)", "false", nullptr, 0, 0 },
    { "int_key", PK_BOOL, "Jumper 10-11: INT key -> interrupt R7", "Propojka 10-11: klávesa INT -> přerušení R7", "true",
      nullptr, 0, 0 },
    { "wait_state", PK_BOOL, "Jumper 12-14: one wait state", "Propojka 12-14: jeden čekací stav", "false", nullptr, 0, 0 },
    { "crystal_hz", PK_INT, "Crystal (Hz, CPU = crystal / 9)", "Krystal (Hz, CPU = krystal / 9)", "18000000", nullptr,
      1000000, 40000000 },
    { "keyboard", PK_KEYBOARD, "Keyboard (connector X2)", "Klávesnice (konektor X2)", "null", nullptr, 0, 0 },
    { "keyboard.strict", PK_BOOL, "Keyboard: true to the hardware (keys may be lost)",
      "Klávesnice: věrně jako hardware (klávesy se mohou ztratit)", "false", nullptr, 0, 0 },
    { "keyboard.beeper", PK_BOOL, "Keyboard: beeper (EKL-1, Consul: buzzer on X2 pin 14; ANK-1: P0-OUT D6)",
      "Klávesnice: bzučák (EKL-1, Consul: bzučák na X2 pin 14; ANK-1: P0-OUT D6)", "true", nullptr, 0, 0 },
    { "keyboard.joyport", PK_CHOICE, "Keyboard ANK-1M2: joystick connector (P1-IN D5-D0)",
      "Klávesnice ANK-1M2: konektor joysticku (P1-IN D5-D0)", "\"joystick\"", k_joyport, 0, 0 },
    { "k2_adapter", PK_CHOICE, "K2 adapter (cable of the keyboard)", "Redukce na K2 (kabel klávesnice)", "\"none\"",
      k_k2_adapter, 0, 0 },
    { "rx_baud", PK_INT, "Redukce34KB01: speed of the PC sending (Bd)", "Redukce34KB01: rychlost vysílání z PC (Bd)",
      "19200", nullptr, 50, 115200 },
    { "rx_stop_bits", PK_INT, "Redukce34KB01: stop bits", "Redukce34KB01: stop bity", "2", nullptr, 1, 2 },
    { "xy4130", PK_SECTION, "X3: plotter XY 4130 (P2-OUT 2C00h; window Minigraf)", "X3: zapisovač XY 4130 (P2-OUT 2C00h; okno Minigraf)",
      "false", nullptr, 0, 0 },
    XY4130_PARAMS("\"mikos\"")
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

static const ParamOption k_k3[] = {
    { "\"none\"", "nothing (pull-ups)", "nic (pull-upy)" },
    { "\"diag\"", "diagnostic board rev 2.0 (2x 7 segments, buttons + / -)",
      "deska diagnostiky rev 2.0 (2x sedmisegmentovka, tlačítka + / -)" },
    { nullptr, nullptr, nullptr },
};
// JPR-1S (Libor Lasota, rev 2.2)
static const ParamOption k_jpr1s_roms[] = {
    { "\"roms/jpr-1s/MIKROBIOS_1.02.bin\"", "MIKROBIOS 1.02 + Falcon 6.1L (04/2022, display BGP-1S)",
      "MIKROBIOS 1.02 + Falcon 6.1L (04/2022, displej BGP-1S)" },
    { "\"roms/jpr-1s/MIKROBIOS_2.1.bin\"", "MikroBIOS 2.1 (11/2024, display AND-1V at 3800)",
      "MikroBIOS 2.1 (11/2024, displej AND-1V na 3800)" },
    { "\"roms/jpr-1s/MIKROBIOS_1.0.bin\"", "MIKROBIOS 1.0 (08/2021, display BGP-1S)", "MIKROBIOS 1.0 (08/2021, displej BGP-1S)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_jpr1s_ack[] = {
    { "\"rst7\"", "2-3 RST7 (the 8228 inserts RST 7)", "2-3 RST7 (8228 vloží RST 7)" },
    { "\"bus\"", "1-2 RSTx (/INTA on the bus, the vector from a card)", "1-2 RSTx (/INTA na sběrnici, vektor z karty)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_jpr1s[] = {
    { "rom", PK_FILE, "EEPROM 28C64 (0000-1FFF)", "EEPROM 28C64 (0000-1FFF)", "\"roms/jpr-1s/MIKROBIOS_1.02.bin\"",
      k_jpr1s_roms, 0, 0 },
    { "ram_4000", PK_BOOL, "RAM_EN 1: RAM bank 4000-7FFF", "RAM_EN 1: banka RAM 4000-7FFF", "true", nullptr, 0, 0 },
    { "ram_8000", PK_BOOL, "RAM_EN 2: RAM bank 8000-BFFF", "RAM_EN 2: banka RAM 8000-BFFF", "true", nullptr, 0, 0 },
    { "ram_c000", PK_BOOL, "RAM_EN 3: RAM bank C000-FFFF", "RAM_EN 3: banka RAM C000-FFFF", "true", nullptr, 0, 0 },
    { "int_int0", PK_BOOL, "INT_SRC 1-2: /INT0 of the bus", "INT_SRC 1-2: /INT0 ze sběrnice", "false", nullptr, 0, 0 },
    { "int_int1", PK_BOOL, "INT_SRC 3-4: /INT1 of the bus", "INT_SRC 3-4: /INT1 ze sběrnice", "true", nullptr, 0, 0 },
    { "int_p2in0", PK_BOOL, "INT_SRC 5-6: P2-IN0 (K3-20)", "INT_SRC 5-6: P2-IN0 (K3-20)", "false", nullptr, 0, 0 },
    { "int_p2in1", PK_BOOL, "INT_SRC 7-8: P2-IN1 (K3-17)", "INT_SRC 7-8: P2-IN1 (K3-17)", "false", nullptr, 0, 0 },
    { "int_ack", PK_CHOICE, "INT_ADR: interrupt response", "INT_ADR: odezva na přerušení", "\"rst7\"", k_jpr1s_ack, 0, 0 },
    { "crystal_hz", PK_INT, "Crystal (Hz, CPU = crystal / 9)", "Krystal (Hz, CPU = krystal / 9)", "18000000", nullptr,
      1000000, 40000000 },
    { "keyboard", PK_KEYBOARD, "Keyboard (connector K2)", "Klávesnice (konektor K2)", "null", nullptr, 0, 0 },
    { "keyboard.strict", PK_BOOL, "Keyboard: true to the hardware (keys may be lost)",
      "Klávesnice: věrně jako hardware (klávesy se mohou ztratit)", "false", nullptr, 0, 0 },
    { "keyboard.beeper", PK_BOOL, "Keyboard: beeper (EKL-1, Consul: buzzer on X2 pin 14; ANK-1: P0-OUT D6)",
      "Klávesnice: bzučák (EKL-1, Consul: bzučák na X2 pin 14; ANK-1: P0-OUT D6)", "true", nullptr, 0, 0 },
    { "keyboard.joyport", PK_CHOICE, "Keyboard ANK-1M2: joystick connector (P1-IN D5-D0)",
      "Klávesnice ANK-1M2: konektor joysticku (P1-IN D5-D0)", "\"joystick\"", k_joyport, 0, 0 },
    { "k3", PK_CHOICE, "K3: what is plugged in", "K3: co je zasunuté", "\"none\"", k_k3, 0, 0 },
    { "k2_adapter", PK_CHOICE, "K2 adapter (cable of the keyboard)", "Redukce na K2 (kabel klávesnice)", "\"none\"",
      k_k2_adapter, 0, 0 },
    { "rx_baud", PK_INT, "Redukce34KB01: speed of the PC sending (Bd)", "Redukce34KB01: rychlost vysílání z PC (Bd)",
      "19200", nullptr, 50, 115200 },
    { "rx_stop_bits", PK_INT, "Redukce34KB01: stop bits", "Redukce34KB01: stop bity", "2", nullptr, 1, 2 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// JPR-1X (Libor Lasota, rev 1.0, 80C85)
static const ParamOption k_jpr1x_roms[] = {
    { "\"roms/jpr-1x/JPR-1X_MIKROBIOS_MIKROBASIC.bin\"",
      "lower bank MIKROBIOS 1.02 + Falcon 6.1L (BGP-1S), upper bank MIKROBASIC 2.4",
      "dolní banka MIKROBIOS 1.02 + Falcon 6.1L (BGP-1S), horní banka MIKROBASIC 2.4" },
    { "\"roms/jpr-1s/MIKROBIOS_1.02.bin\"", "MIKROBIOS 1.02 of the JPR-1S (8 KB: both banks)",
      "MIKROBIOS 1.02 z JPR-1S (8 KB: obě banky)" },
    { "\"roms/jpr-1s/MIKROBIOS_2.1.bin\"", "MikroBIOS 2.1 of the JPR-1S (8 KB, AND-1V at 3800)",
      "MikroBIOS 2.1 z JPR-1S (8 KB, AND-1V na 3800)" },
    { "\"\"", "none (ROM socket empty: the bus)", "žádná (patice ROM prázdná: sběrnice)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_jpr1x_bios[] = {
    { "\"fitted\"", "fitted: the lower bank", "osazená: dolní banka" },
    { "\"open\"", "open: the upper bank", "rozpojená: horní banka" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_jpr1x_vector[] = {
    { "\"rst7\"", "RSTN/7 open: always RST 7", "RSTN/7 rozpojená: vždy RST 7" },
    { "\"level\"", "RSTN/7 fitted: RST by the 3214 level", "RSTN/7 osazená: RST podle úrovně 3214" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_jpr1x_key[] = {
    { "\"none\"", "factory: not connected (R6 = P2-IN6)", "z výroby: nezapojená (R6 = P2-IN6)" },
    { "\"r7\"", "11-10: request R7 (as JPR-1 jumper 10-11)", "11-10: požadavek R7 (jako propojka 10-11 JPR-1)" },
    { "\"r6\"", "10-R6 and R6-10: request R6 (recommended)", "10-R6 a R6-10: požadavek R6 (doporučeno)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_jpr1x[] = {
    { "rom", PK_FILE, "ROM IC11 (27C256: two 8 KB banks at 4000h/6000h of the chip)",
      "ROM IC11 (27C256: dvě banky po 8 KB na 4000h/6000h čipu)", "\"roms/jpr-1x/JPR-1X_MIKROBIOS_MIKROBASIC.bin\"",
      k_jpr1x_roms, 0, 0 },
    { "bios", PK_CHOICE, "Jumper BIOS: ROM bank (read at RESET)", "Propojka BIOS: banka ROM (čte se při RESETu)", "\"fitted\"",
      k_jpr1x_bios, 0, 0 },
    { "turbo", PK_BOOL, "TURBO: CPU 4 MHz (else 2 MHz), bus always 2 MHz", "TURBO: CPU 4 MHz (jinak 2 MHz), sběrnice vždy 2 MHz",
      "false", nullptr, 0, 0 },
    { "ram", PK_BOOL, "RAM 6116 at 2000-23FF", "RAM 6116 na 2000-23FF", "true", nullptr, 0, 0 },
    { "int_vector", PK_CHOICE, "Jumper RSTN/7: interrupt vector", "Propojka RSTN/7: vektor přerušení", "\"rst7\"",
      k_jpr1x_vector, 0, 0 },
    { "int_key", PK_CHOICE, "Jumpers 11-10-R6, P2IN6-R6-10: INT key", "Propojky 11-10-R6, P2IN6-R6-10: klávesa INT",
      "\"none\"", k_jpr1x_key, 0, 0 },
    { "osc_hz", PK_INT, "Oscillator (Hz, bus = /8, CPU = /4 or /8)", "Oscilátor (Hz, sběrnice = /8, CPU = /4 nebo /8)",
      "16000000", nullptr, 1000000, 40000000 },
    { "keyboard", PK_KEYBOARD, "Keyboard (connector K2)", "Klávesnice (konektor K2)", "null", nullptr, 0, 0 },
    { "keyboard.strict", PK_BOOL, "Keyboard: true to the hardware (keys may be lost)",
      "Klávesnice: věrně jako hardware (klávesy se mohou ztratit)", "false", nullptr, 0, 0 },
    { "keyboard.beeper", PK_BOOL, "Keyboard: beeper (EKL-1, Consul: buzzer on X2 pin 14; ANK-1: P0-OUT D6)",
      "Klávesnice: bzučák (EKL-1, Consul: bzučák na X2 pin 14; ANK-1: P0-OUT D6)", "true", nullptr, 0, 0 },
    { "keyboard.joyport", PK_CHOICE, "Keyboard ANK-1M2: joystick connector (P1-IN D5-D0)",
      "Klávesnice ANK-1M2: konektor joysticku (P1-IN D5-D0)", "\"joystick\"", k_joyport, 0, 0 },
    { "k3", PK_CHOICE, "K3: what is plugged in", "K3: co je zasunuté", "\"none\"", k_k3, 0, 0 },
    { "k2_adapter", PK_CHOICE, "K2 adapter (cable of the keyboard)", "Redukce na K2 (kabel klávesnice)", "\"none\"",
      k_k2_adapter, 0, 0 },
    { "rx_baud", PK_INT, "Redukce34KB01: speed of the PC sending (Bd)", "Redukce34KB01: rychlost vysílání z PC (Bd)",
      "19200", nullptr, 50, 115200 },
    { "rx_stop_bits", PK_INT, "Redukce34KB01: stop bits", "Redukce34KB01: stop bity", "2", nullptr, 1, 2 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// JPR-1A (ZPS-3)
static const ParamOption k_jpr1a_roms[] = {
    { "\"roms/jpr-1a/JPR-1A_MON_V1.0-C1.bin\"", "48K MON V1.0-C1 (Tesla IMA 1986, CP/M ZPS-3)",
      "48K MON V1.0-C1 (Tesla IMA 1986, CP/M ZPS-3)" },
    { "\"roms/jpr-1a/MOR48A.bin\"", "MOR48A: CP/M 2.2/P 48K, RPD-1 (Perina 1988)",
      "MOR48A: CP/M 2.2/P 48K, RPD-1 (Peřina 1988)" },
    { "\"roms/jpr-1a/MOR62A.bin\"", "MOR62A: CP/M 2.2/P 62K, RPD-1 (Perina 1988)",
      "MOR62A: CP/M 2.2/P 62K, RPD-1 (Peřina 1988)" },
    { "\"roms/jpr-1a/MOR48AF.bin\"", "MOR48AF: CP/M 2.2/P 48K, FDC-1 (Perina 1988)",
      "MOR48AF: CP/M 2.2/P 48K, FDC-1 (Peřina 1988)" },
    { "\"roms/jpr-1a/MOR62AF.bin\"", "MOR62AF: CP/M 2.2/P 62K, FDC-1 (Perina 1988)",
      "MOR62AF: CP/M 2.2/P 62K, FDC-1 (Peřina 1988)" },
    { "\"roms/jpr-1a/MON64C3.bin\"", "64K MON V1.0-C3 (Tesla IMA, CP/M ZPS-3 64K)",
      "64K MON V1.0-C3 (Tesla IMA, CP/M ZPS-3 64K)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_jpr1a_jp1[] = {
    { "\"1-2\"", "1-2: interrupt = RST 7", "1-2: přerušení = RST 7" },
    { "\"1-3\"", "1-3: vector from the bus (INTA)", "1-3: vektor ze sběrnice (INTA)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_jpr1a_dec[] = {
    { "\"roms/jpr-1a/JPR-1A_D2C_74S571.bin\"", "standard (manual)", "standardní (návod)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_jpr1a[] = {
    { "rom", PK_FILE, "EPROM D3B + D4B (2x 2716, 0000-0FFF after RESET)", "EPROM D3B + D4B (2x 2716, 0000-0FFF po RESET)",
      "\"\"", k_jpr1a_roms, 0, 0 },
    { "decoder", PK_FILE, "PROM D2C: decoder (EPROM, ports 00-03)", "PROM D2C: dekodér (EPROM, porty 00-03)",
      "\"roms/jpr-1a/JPR-1A_D2C_74S571.bin\"", k_jpr1a_dec, 0, 0 },
    { "jp1", PK_CHOICE, "Jumper JP1: interrupt vector", "Propojka JP1: vektor přerušení", "\"1-2\"", k_jpr1a_jp1, 0, 0 },
    { "crystal_hz", PK_INT, "Crystal (Hz, CPU = crystal / 9)", "Krystal (Hz, CPU = krystal / 9)", "18000000", nullptr,
      1000000, 40000000 },
    { "keyboard", PK_KEYBOARD, "Keyboard (connector X2)", "Klávesnice (konektor X2)", "null", nullptr, 0, 0 },
    { "keyboard.strict", PK_BOOL, "Keyboard: true to the hardware (keys may be lost)",
      "Klávesnice: věrně jako hardware (klávesy se mohou ztratit)", "false", nullptr, 0, 0 },
    { "keyboard.beeper", PK_BOOL, "Keyboard: beeper (EKL-1, Consul: buzzer on X2 pin 14; ANK-1: P0-OUT D6)",
      "Klávesnice: bzučák (EKL-1, Consul: bzučák na X2 pin 14; ANK-1: P0-OUT D6)", "true", nullptr, 0, 0 },
    { "keyboard.joyport", PK_CHOICE, "Keyboard ANK-1M2: joystick connector (P1-IN D5-D0)",
      "Klávesnice ANK-1M2: konektor joysticku (P1-IN D5-D0)", "\"joystick\"", k_joyport, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// REM-1
static const ParamOption k_block[] = {
    { "0", "0000-1FFF", "0000-1FFF" }, { "1", "2000-3FFF", "2000-3FFF" }, { "2", "4000-5FFF", "4000-5FFF" },
    { "3", "6000-7FFF", "6000-7FFF" }, { "4", "8000-9FFF", "8000-9FFF" }, { "5", "A000-BFFF", "A000-BFFF" },
    { "6", "C000-DFFF", "C000-DFFF" }, { "7", "E000-FFFF", "E000-FFFF" },
    { "\"none\"", "not connected", "nezapojeno" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_rem1_roms[] = {
    { "\"roms/mikrobasic/MIKROBASIC_2.4_ZPS-1.bin\"", "MIKROBASIC 2.4 (ZPS-1)", "MIKROBASIC 2.4 (ZPS-1)" },
    { "\"roms/mikrobasic/MIKROBASIC_AR1983_1k.bin\"", "MIKROBASIC AR/B 1983", "MIKROBASIC AR/B 1983" },
    { "\"roms/mikos/MIKOS_V4.0F_ANK-1.bin\"", "Monitor V4.0F + MIKOS 4.0F (ANK-1)", "Monitor V4.0F + MIKOS 4.0F (ANK-1)" },
    { "\"roms/mikos/MIKOS_V4.0D_ANK-1.bin\"", "Monitor V4.0C + MIKOS 4.0D (ANK-1)", "Monitor V4.0C + MIKOS 4.0D (ANK-1)" },
    { "\"roms/mikos/MIKOS_V4.1F_Consul.bin\"", "Monitor V4.1F + MIKOS 4.0F (Consul)", "Monitor V4.1F + MIKOS 4.0F (Consul)" },
    { "\"roms/mikos/MONITOR_V5.0F_MIKOS_V5.0.bin\"", "Monitor V5.0F + MIKOS V5.0 (Consul, 5 KB)", "Monitor V5.0F + MIKOS V5.0 (Consul, 5 KB)" },
    { "\"roms/mikos/MONITOR_V5.0E_MIKOS_V5.0.bin\"", "Monitor V5.0E + MIKOS V5.0 (Consul, 5 KB)", "Monitor V5.0E + MIKOS V5.0 (Consul, 5 KB)" },
    { "\"roms/mikos/MONITOR_V5.0F_MIKOS_V5.0_MINIGRAF.bin\"", "Monitor V5.0F + MIKOS V5.0 + Minigraf 0507 driver at 1800h (8 KB)", "Monitor V5.0F + MIKOS V5.0 + ovladač Minigrafu 0507 na 1800h (8 KB)" },
    { "\"roms/mikos/MONITOR_V5.0E_MIKOS_K_V5.0.bin\"", "Monitor V5.0E + MIKOS/K V5.0 for the KZD-1 (6 KB)", "Monitor V5.0E + MIKOS/K V5.0 pro KZD-1 (6 KB)" },
    { "\"roms/dse-1/MIKOS_V4.0F_DSE_1000.bin\"", "Monitor V4.0F + MIKOS 4.0F (ANK-1) + DSE program for the DSE-1 at 1000h (6 KB)",
      "Monitor V4.0F + MIKOS 4.0F (ANK-1) + program DSE pro DSE-1 na 1000h (6 KB)" },
    { "\"\"", "empty sockets", "prázdné patice" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_rem1[] = {
    { "rom", PK_FILE, "EPROMs (from socket 1)", "EPROM (od patice 1)", "\"\"", k_rem1_roms, 0, 0 },
    { "eprom_type", PK_CHOICE, "Jumpers H/G/I/J: EPROM type", "Propojky H/G/I/J: typ EPROM", "\"2708\"", k_eprom_type, 0, 0 },
    { "eprom_block", PK_CHOICE, "Jumper E: EPROM block", "Propojka E: blok EPROM", "0", k_block, 0, 0 },
    { "eprom_block2", PK_CHOICE, "Jumper F: second EPROM block (2716)", "Propojka F: druhý blok EPROM (2716)", "\"none\"",
      k_block, 0, 0 },
    { "ram_block", PK_CHOICE, "Jumper R: RAM block", "Propojka R: blok RAM", "2", k_block, 0, 0 },
    { "ram_kb", PK_INT, "RAM fitted (KB, 2 per 4x 2114)", "Osazená RAM (KB, 2 na 4x 2114)", "2", nullptr, 0, 8 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// RAM-1
static const ParamOption k_ram1_kb[] = {
    { "16", "RAM-1/16K (one row of 4116)", "RAM-1/16K (jedna řada 4116)" },
    { "32", "RAM-1/32K", "RAM-1/32K" },
    { "48", "RAM-1/48K", "RAM-1/48K" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_ram1_proms[] = {
    { "\"roms/ram-1/RAM-1_D1A_74S287.bin\"", "RAM-1 (from 4000)", "RAM-1 (od 4000)" },
    { "\"roms/ram-1a/RAM-1A_D1A_74S287.bin\"", "RAM-1A (ZPS-3: 0000-BFFF)", "RAM-1A (ZPS-3: 0000-BFFF)" },
    { "\"roms/ram-1/RAM-1_16K_D1A_74S287.bin\"", "RAM-1/16K (C000)", "RAM-1/16K (C000)" },
    { "\"roms/ram-1a/RAM-1A_D1A_62K_74S287.bin\"", "RAM-1A for CP/M 62K: S1 S2 10 = C000-F7FF (assumed)",
      "RAM-1A pro CP/M 62K: S1 S2 10 = C000-F7FF (předpoklad)" },
    { "\"roms/ram-1a/RAM-1A_D1A_64K_74S287.bin\"", "RAM-1A for CP/M 64K: S1 S2 10 = C000-EFFF + F800-FFFF (assumed)",
      "RAM-1A pro CP/M 64K: S1 S2 10 = C000-EFFF + F800-FFFF (předpoklad)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_ram1[] = {
    { "fitted_kb", PK_CHOICE, "Fitted memory", "Osazená paměť", "48", k_ram1_kb, 0, 0 },
    { "s1", PK_BOOL, "S1 (S1 S2: 10 = 4000-FFFF, 01 = 4000-BFFF, 11 = 4000-7FFF, 00 = 4000-DFFF)",
      "S1 (S1 S2: 10 = 4000-FFFF, 01 = 4000-BFFF, 11 = 4000-7FFF, 00 = 4000-DFFF)", "true", nullptr, 0, 0 },
    { "s2", PK_BOOL, "S2", "S2", "false", nullptr, 0, 0 },
    { "prom", PK_FILE, "PROM D1A: address decoder", "PROM D1A: dekodér adres", "\"roms/ram-1/RAM-1_D1A_74S287.bin\"",
      k_ram1_proms, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};
static const CardParam k_ram1a[] = {
    { "fitted_kb", PK_CHOICE, "Fitted memory", "Osazená paměť", "48", k_ram1_kb, 0, 0 },
    { "s1", PK_BOOL, "S1 (S1 S2: 01 = 0000-BFFF, 11 = 4000-FFFF, 10 = C000-FFFF, 00 = C000-E7FF)",
      "S1 (S1 S2: 01 = 0000-BFFF, 11 = 4000-FFFF, 10 = C000-FFFF, 00 = C000-E7FF)", "false", nullptr, 0, 0 },
    { "s2", PK_BOOL, "S2", "S2", "true", nullptr, 0, 0 },
    { "prom", PK_FILE, "PROM D1A: address decoder", "PROM D1A: dekodér adres", "\"roms/ram-1a/RAM-1A_D1A_74S287.bin\"",
      k_ram1_proms, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// AND-1
static const ParamOption k_and1_cs[] = {
    { "\"MHB2501\"", "MHB 2501 latin (jumper 42-43)", "MHB 2501 latinka (propojka 42-43)" },
    { "\"MHB2502\"", "MHB 2502 cyrillic (jumper 41-42)", "MHB 2502 azbuka (propojka 41-42)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_and1_rows[] = {
    { "24", "24 rows (jumper 47-48)", "24 řádků (propojka 47-48)" },
    { "20", "20 rows (jumper 46-47)", "20 řádků (propojka 46-47)" },
    { nullptr, nullptr, nullptr },
};
// Phosphor of the monochrome monitor of a display card without colour jumpers.
static const ParamOption k_monitor[] = {
    { "\"white\"", "white", "bílá" }, { "\"green\"", "green", "zelená" }, { "\"amber\"", "amber", "jantarová" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_and1[] = {
    { "charset", PK_CHOICE, "Character generator", "Generátor znaků", "\"MHB2501\"", k_and1_cs, 0, 0 },
    { "rows", PK_CHOICE, "Rows", "Řádků", "24", k_and1_rows, 0, 0 },
    { "int1", PK_BOOL, "Jumper 49-50: INT1 at the end of the frame", "Propojka 49-50: INT1 na konci snímku", "true",
      nullptr, 0, 0 },
    { "monitor", PK_CHOICE, "Monitor: phosphor colour (not on the card)", "Monitor: barva luminoforu (není na kartě)",
      "\"white\"", k_monitor, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

static const ParamOption k_rpd1_step[] = {
    { "8", "8 ms (Consul 7213)", "8 ms (Consul 7213)" },
    { "12", "12 ms (Consul 7212)", "12 ms (Consul 7212)" },
    { "3", "3 ms (Momflex)", "3 ms (Momflex)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_rpd1_disks[] = {
    { "\"media/rpd-1/MIKOS_D_V5.0_system.img\"", "MIKOS/D V5.0 system disk", "Systémová disketa MIKOS/D V5.0" },
    { "\"media/rpd-1/CPM22P_48A_Perina.img\"", "CP/M 2.2/P 48K (Perina, AND-1A)", "CP/M 2.2/P 48K (Peřina, AND-1A)" },
    { "\"media/rpd-1/CPM22P_62A_Perina.img\"", "CP/M 2.2/P 62K (Perina, AND-1A)", "CP/M 2.2/P 62K (Peřina, AND-1A)" },
    { "\"media/rpd-1/CPM48K_IMA_004.img\"", "CP/M 2.2 46K Tesla IMA, disk 004 (system rebuilt by MOVCPM48, M80, F80, sources)",
      "CP/M 2.2 46K Tesla IMA, disketa 004 (systém z MOVCPM48, M80, F80, zdroje)" },
    { "\"media/rpd-1/CPM48K_IMA_003.img\"", "CP/M 2.2 Tesla IMA ZPS-3, disk 003 (Pascal MT+, no system)",
      "CP/M 2.2 Tesla IMA ZPS-3, disketa 003 (Pascal MT+, bez systému)" },
    { "\"media/rpd-1/CPM64K_IMA_01.img\"", "CP/M 2.2 64K Tesla IMA, disk 1 (system, ED, PIP, M80, F80, ASM)",
      "CP/M 2.2 64K Tesla IMA, disketa 1 (systém, ED, PIP, M80, F80, ASM)" },
    { "\"media/rpd-1/CPM64K_IMA_02.img\"", "CP/M 2.2 64K Tesla IMA, disk 2 (STAT, FORMAT, MBASIC, COBOL)",
      "CP/M 2.2 64K Tesla IMA, disketa 2 (STAT, FORMAT, MBASIC, COBOL)" },
    { "\"media/rpd-1/CPM64K_IMA_03.img\"", "CP/M 2.2 64K Tesla IMA, disk 3 (Pascal MT+)",
      "CP/M 2.2 64K Tesla IMA, disketa 3 (Pascal MT+)" },
    { "\"media/sapiman/r1_zrmd4m_271111.img\"", "CP/M boot disk J. Biba (MON 271111, C: ZRMD-1)",
      "Bootovací CP/M J. Bíba (MON 271111, C: ZRMD-1)" },
    { "\"media/sapiman/r1_rmd1m_271111.img\"", "CP/M boot disk J. Biba (MON 271111, C: RMD-1)",
      "Bootovací CP/M J. Bíba (MON 271111, C: RMD-1)" },
    { "\"media/sapiman/r2_mfc256x14_261211.img\"", "CP/M boot disk J. Biba 261211 (C: RMD-1, D:, E: MFC-1A 8\" FM 14 x 256)",
      "Bootovací CP/M J. Bíba 261211 (C: RMD-1, D:, E: MFC-1A 8\" FM 14 × 256)" },
    { "\"media/sapiman/r3_mfc512x8_261211.img\"", "CP/M boot disk J. Biba 261211 3K (C: RMD-1, D:, E: MFC-1A 8\" FM 8 x 512)",
      "Bootovací CP/M J. Bíba 261211 3K (C: RMD-1, D:, E: MFC-1A 8\" FM 8 × 512)" },
    { "\"media/sapiman/r2_rmd1m_fdc_261211.img\"", "CP/M boot disk J. Biba 260811 (C: RMD-1, D:, E: FDC-1 8\")",
      "Bootovací CP/M J. Bíba 260811 (C: RMD-1, D:, E: FDC-1 8\")" },
    { "\"media/sapiman/Z1_RPD271111_programy.img\"", "J. Biba Z1: CP/M boot, programs (C: ZRMD-1 SS256)",
      "J. Bíba Z1: bootovací CP/M, programy (C: ZRMD-1 SS256)" },
    { "\"media/sapiman/Z2_RPD271111_zdroje.img\"", "J. Biba Z2: sources of the RPD-1 system",
      "J. Bíba Z2: zdroje systému pro RPD-1" },
    { "\"media/sapiman/Z9_RPD050412_dlouhe_CPM_boot.img\"", "J. Biba Z9 + long CP/M 050412 (MON 260112, FDC-1 HD)",
      "J. Bíba Z9 + dlouhé CP/M 050412 (MON 260112, FDC-1 HD)" },
    { "\"media/sapiman/Z9_RPD050412_dlouhe_CPM_boot_3.5HD.img\"", "J. Biba Z9 + long CP/M 050412 for 3.5\" HD",
      "J. Bíba Z9 + dlouhé CP/M 050412 pro 3,5\" HD" },
    { "\"media/sapiman/Z3_RPD050412_dlouhe_CPM_zdroje.img\"", "J. Biba Z3: sources of the long CP/M",
      "J. Bíba Z3: zdroje dlouhého CP/M" },
    { "\"media/rmp-1/a_lad_izo.img\"", "J. Biba: magnetic tape programs (MTDAT, DIRMT, MTCOPY, MTZAP, RMP-1)",
      "J. Bíba: programy pro magnetickou pásku (MTDAT, DIRMT, MTCOPY, MTZAP, RMP-1)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_rpd1[] = {
    { "drive_a", PK_SECTION, "Drive 0 (8\" SS SD)", "Mechanika 0 (8\" SS SD)", "true", nullptr, 0, 0 },
    { "drive_a.image", PK_FILE, "Disk image (.img 77 x 26 x 128 B, or ImageDisk .imd of such a disk)", "Obraz diskety (.img 77 x 26 x 128 B, nebo ImageDisk .imd takové diskety)", "\"\"",
      k_rpd1_disks, 0, 0 },
    { "drive_a.write_protect", PK_BOOL, "Write protected", "Chráněná proti zápisu", "false", nullptr, 0, 0 },
    { "drive_a.cpm", PK_TEXT, "CP/M letter of the drive (Media window; empty = seen while running)",
      "Písmeno mechaniky v CP/M (okno Média; prázdné = zjistí se za chodu)", "\"\"", nullptr, 0, 0 },
    { "drive_b", PK_SECTION, "Drive 1 (8\" SS SD)", "Mechanika 1 (8\" SS SD)", "false", nullptr, 0, 0 },
    { "drive_b.image", PK_FILE, "Disk image (.img 77 x 26 x 128 B, or ImageDisk .imd of such a disk)", "Obraz diskety (.img 77 x 26 x 128 B, nebo ImageDisk .imd takové diskety)", "\"\"",
      k_rpd1_disks, 0, 0 },
    { "drive_b.write_protect", PK_BOOL, "Write protected", "Chráněná proti zápisu", "false", nullptr, 0, 0 },
    { "drive_b.cpm", PK_TEXT, "CP/M letter of the drive (Media window; empty = seen while running)",
      "Písmeno mechaniky v CP/M (okno Média; prázdné = zjistí se za chodu)", "\"\"", nullptr, 0, 0 },
    { "working_copy", PK_BOOL, "Work on copies in work/", "Pracovat s kopiemi ve work/", "true", nullptr, 0, 0 },
    { "step_ms", PK_CHOICE, "Drive: step period", "Mechanika: perioda kroku", "8", k_rpd1_step, 0, 0 },
    { "settle_ms", PK_INT, "Drive: head load settle time (ms)", "Mechanika: ustálení přiklopené hlavy (ms)", "35",
      nullptr, 0, 500 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// RMP-1 magnetic tape controller (IZOT SM 5300)
static const ParamOption k_rmp1_tapes[] = {
    { "\"work/rmp-1/paska0.tap\"", "Own tape in work/ (created blank)", "Vlastní páska ve work/ (vytvoří se prázdná)" },
    { "\"media/rmp-1/sapi01_ukazka.tap\"", "Sample tape SAPI01 (JSEP labels, EBCDIC; MTDAT)",
      "Ukázková páska SAPI01 (návěští JSEP, EBCDIC; MTDAT)" },
    { "\"\"", "no tape", "bez pásky" },
    { nullptr, nullptr, nullptr },
};

static const CardParam k_rmp1[] = {
    { "unit0", PK_SECTION, "Unit 0 (IZOT SM 5300)", "Jednotka 0 (IZOT SM 5300)", "true", nullptr, 0, 0 },
    { "unit0.tape", PK_FILE, "Tape image (SIMH .tap; a new file = a blank tape)",
      "Obraz pásky (SIMH .tap; nový soubor = prázdná páska)", "\"work/rmp-1/paska0.tap\"", k_rmp1_tapes, PF_NEW_MEDIUM, 0 },
    { "unit0.write_ring", PK_BOOL, "Write enable ring in the reel (without it WPT)",
      "Kroužek povolení zápisu v cívce (bez něj WPT)", "true", nullptr, 0, 0 },
    { "unit1", PK_SECTION, "Unit 1 (IZOT SM 5300)", "Jednotka 1 (IZOT SM 5300)", "false", nullptr, 0, 0 },
    { "unit1.tape", PK_FILE, "Tape image (SIMH .tap; a new file = a blank tape)",
      "Obraz pásky (SIMH .tap; nový soubor = prázdná páska)", "\"\"", k_rmp1_tapes, PF_NEW_MEDIUM, 0 },
    { "unit1.write_ring", PK_BOOL, "Write enable ring in the reel (without it WPT)",
      "Kroužek povolení zápisu v cívce (bez něj WPT)", "true", nullptr, 0, 0 },
    { "unit2", PK_SECTION, "Unit 2 (IZOT SM 5300)", "Jednotka 2 (IZOT SM 5300)", "false", nullptr, 0, 0 },
    { "unit2.tape", PK_FILE, "Tape image (SIMH .tap; a new file = a blank tape)",
      "Obraz pásky (SIMH .tap; nový soubor = prázdná páska)", "\"\"", k_rmp1_tapes, PF_NEW_MEDIUM, 0 },
    { "unit2.write_ring", PK_BOOL, "Write enable ring in the reel (without it WPT)",
      "Kroužek povolení zápisu v cívce (bez něj WPT)", "true", nullptr, 0, 0 },
    { "ready_at_start", PK_BOOL, "Tapes threaded and on-line at power on (comfort: LOAD and ON-LINE pressed)",
      "Pásky založené a ON-LINE po zapnutí (pohodlí: stisknuto LOAD a ON-LINE)", "true", nullptr, 0, 0 },
    { "working_copy", PK_BOOL, "Work on copies in work/", "Pracovat s kopiemi ve work/", "true", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

static const ParamOption k_rkd1_carts[] = {
    { "\"media/rkd-1/tesla_cpm_system.rkd\"", "CP/M 2.2 Tesla Elstroj 1987 (system cartridge, 24 x 256 B)",
      "CP/M 2.2 Tesla Elstroj 1987 (systémová kazeta, 24 x 256 B)" },
    { "\"work/rkd-1/kazeta.rkd\"", "Own cartridge in work/ (created unformatted)", "Vlastní kazeta ve work/ (vytvoří se nenaformátovaná)" },
    { nullptr, nullptr, nullptr },
};

static const ParamOption k_rkd1_drive[] = {
    { "\"CM 5403\"", "CM 5403 (Zbrojovka Brno; CP/M of Tesla Elstroj)", "CM 5403 (Zbrojovka Brno; CP/M Tesla Elstroj)" },
    { "\"CM 5400\"", "CM 5400 (IZOT, Bulgaria; cable SM 5400, FORMRKD of J. Biba, 12 x 512 B)",
      "CM 5400 (IZOT, Bulharsko; kabel SM 5400, FORMRKD J. Bíby, 12 x 512 B)" },
    { nullptr, nullptr, nullptr },
};

static const ParamOption k_rkd1_sectors[] = {
    { "24", "24 x 256 B (Tesla Elstroj CP/M)", "24 x 256 B (CP/M Tesla Elstroj)" },
    { "12", "12 x 512 B (SMEP, FORMRKD)", "12 x 512 B (SMEP, FORMRKD)" },
    { nullptr, nullptr, nullptr },
};

static const CardParam k_rkd1[] = {
    { "unit0", PK_SECTION, "Unit 0 (cartridge + fixed disk)", "Jednotka 0 (kazeta + pevný disk)", "true",
      nullptr, 0, 0 },
    { "unit0.drive", PK_CHOICE, "Drive", "Mechanika", "\"CM 5403\"", k_rkd1_drive, 0, 0 },
    { "unit0.cartridge", PK_FILE, "Cartridge image (.rkd; a new file = an unformatted cartridge)",
      "Obraz kazety (.rkd; nový soubor = nenaformátovaná kazeta)", "\"\"", k_rkd1_carts, PF_NEW_MEDIUM, 0 },
    { "unit0.write_protect", PK_BOOL, "Cartridge write protected", "Kazeta chráněná proti zápisu", "false", nullptr, 0, 0 },
    { "unit0.fixed", PK_FILE, "Fixed disk image (.rkd, created when missing)", "Obraz pevného disku (.rkd, vytvoří se, když chybí)",
      "\"work/rkd-1/unit0_fixed.rkd\"", nullptr, 0, 0 },
    { "unit1", PK_SECTION, "Unit 1 (cartridge + fixed disk)", "Jednotka 1 (kazeta + pevný disk)", "false",
      nullptr, 0, 0 },
    { "unit1.drive", PK_CHOICE, "Drive", "Mechanika", "\"CM 5403\"", k_rkd1_drive, 0, 0 },
    { "unit1.cartridge", PK_FILE, "Cartridge image (.rkd; a new file = an unformatted cartridge)",
      "Obraz kazety (.rkd; nový soubor = nenaformátovaná kazeta)", "\"\"", k_rkd1_carts, PF_NEW_MEDIUM, 0 },
    { "unit1.write_protect", PK_BOOL, "Cartridge write protected", "Kazeta chráněná proti zápisu", "false", nullptr, 0, 0 },
    { "unit1.fixed", PK_FILE, "Fixed disk image (.rkd, created when missing)", "Obraz pevného disku (.rkd, vytvoří se, když chybí)",
      "\"work/rkd-1/unit1_fixed.rkd\"", nullptr, 0, 0 },
    { "unit2", PK_SECTION, "Unit 2 (cartridge + fixed disk)", "Jednotka 2 (kazeta + pevný disk)", "false",
      nullptr, 0, 0 },
    { "unit2.drive", PK_CHOICE, "Drive", "Mechanika", "\"CM 5403\"", k_rkd1_drive, 0, 0 },
    { "unit2.cartridge", PK_FILE, "Cartridge image (.rkd; a new file = an unformatted cartridge)",
      "Obraz kazety (.rkd; nový soubor = nenaformátovaná kazeta)", "\"\"", k_rkd1_carts, PF_NEW_MEDIUM, 0 },
    { "unit2.write_protect", PK_BOOL, "Cartridge write protected", "Kazeta chráněná proti zápisu", "false", nullptr, 0, 0 },
    { "unit2.fixed", PK_FILE, "Fixed disk image (.rkd, created when missing)", "Obraz pevného disku (.rkd, vytvoří se, když chybí)",
      "\"work/rkd-1/unit2_fixed.rkd\"", nullptr, 0, 0 },
    { "unit3", PK_SECTION, "Unit 3 (cartridge + fixed disk)", "Jednotka 3 (kazeta + pevný disk)", "false",
      nullptr, 0, 0 },
    { "unit3.drive", PK_CHOICE, "Drive", "Mechanika", "\"CM 5403\"", k_rkd1_drive, 0, 0 },
    { "unit3.cartridge", PK_FILE, "Cartridge image (.rkd; a new file = an unformatted cartridge)",
      "Obraz kazety (.rkd; nový soubor = nenaformátovaná kazeta)", "\"\"", k_rkd1_carts, PF_NEW_MEDIUM, 0 },
    { "unit3.write_protect", PK_BOOL, "Cartridge write protected", "Kazeta chráněná proti zápisu", "false", nullptr, 0, 0 },
    { "unit3.fixed", PK_FILE, "Fixed disk image (.rkd, created when missing)", "Obraz pevného disku (.rkd, vytvoří se, když chybí)",
      "\"work/rkd-1/unit3_fixed.rkd\"", nullptr, 0, 0 },
    { "new_sectors", PK_CHOICE, "New cartridge: sector pulses per turn (CM 5400: 12)", "Nová kazeta: sektorových značek na otáčku (CM 5400: 12)", "24",
      k_rkd1_sectors, 0, 0 },
    { "spinup_s", PK_INT, "Drive start after a cartridge goes in (s; 0 = at once, comfort)",
      "Rozběh mechaniky po vložení kazety (s; 0 = hned, pohodlí)", "0", nullptr, 0, 120 },
    { "working_copy", PK_BOOL, "Work on copies in work/", "Pracovat s kopiemi ve work/", "true", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

static const ParamOption k_fdc1_disks[] = {
    { "\"media/fdc-1/CPM22P_48AF_Perina.img\"", "CP/M 2.2/P 48K for FDC-1 (Perina, AND-1A)",
      "CP/M 2.2/P 48K pro FDC-1 (Peřina, AND-1A)" },
    { "\"media/fdc-1/CPM22P_62AF_Perina.img\"", "CP/M 2.2/P 62K for FDC-1 (Perina, AND-1A)",
      "CP/M 2.2/P 62K pro FDC-1 (Peřina, AND-1A)" },
    { "\"media/fdc-1/CPM22P_62ZF_Perina.img\"", "CP/M 2.2/P 62K for FDC-1 (Perina, ZPS-4, AND-1Z)",
      "CP/M 2.2/P 62K pro FDC-1 (Peřina, ZPS-4, AND-1Z)" },
    { "\"media/sapiman/f1_zrmd4_130711.img\"", "CP/M boot disk J. Biba (MON 130711, C: ZRMD-1)",
      "Bootovací CP/M J. Bíba (MON 130711, C: ZRMD-1)" },
    { "\"media/sapiman/f1_rmd1m_130711.img\"", "CP/M boot disk J. Biba (MON 130711, C: RMD-1)",
      "Bootovací CP/M J. Bíba (MON 130711, C: RMD-1)" },
    { "\"media/sapiman/f2_irmd1m_130711.img\"", "CP/M boot disk J. Biba (MON 130711, C: RMD-1 set up by the BIOS)",
      "Bootovací CP/M J. Bíba (MON 130711, C: RMD-1 připraví BIOS)" },
    { "\"media/sapiman/f2_pmd2d2m_140212.img\"", "CP/M boot disk J. Biba 140212 (C:, D: PMD32-SD on DPP-1)",
      "Bootovací CP/M J. Bíba 140212 (C:, D: PMD32-SD na DPP-1)" },
    { "\"media/sapiman/f2_pmd1d2m_140212.img\"", "CP/M boot disk J. Biba 140212 (C: PMD32-SD on DPP-1)",
      "Bootovací CP/M J. Bíba 140212 (C: PMD32-SD na DPP-1)" },
    { "\"media/sapiman/Z4_FDC130711_programy.img\"", "J. Biba Z4: CP/M boot, programs (C: ZRMD-1 SS256)",
      "J. Bíba Z4: bootovací CP/M, programy (C: ZRMD-1 SS256)" },
    { "\"media/sapiman/Z10_FDC130711_zdroje.img\"", "J. Biba Z10: CP/M boot, sources of the FDC-1 system",
      "J. Bíba Z10: bootovací CP/M, zdroje systému pro FDC-1" },
    { "\"media/sapiman/Z8_FDC64K_TERM_zdroje.img\"", "J. Biba Z8: CP/M 64K terminal (MON 151111 64K), sources",
      "J. Bíba Z8: CP/M 64K s terminálem (MON 151111 64K), zdroje" },
    { "\"media/fdc-1/prazdna_3.5HD_CPM.img\"", "empty 3.5\" HD 1.44 MB (CP/M, BIOS ECBIF3)",
      "prázdná 3,5\" HD 1,44 MB (CP/M, BIOS ECBIF3)" },
    { "\"media/fdc-1/prazdna_5.25HD_CPM.img\"", "empty 5.25\" HD 1.2 MB (CP/M, BIOS ECBIF5)",
      "prázdná 5,25\" HD 1,2 MB (CP/M, BIOS ECBIF5)" },
    { "\"media/fdc-1/prazdna_8SSSD.img\"", "empty 8\" SS SD (CP/M)",
      "prázdná 8\" SS SD (CP/M)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_fdc1_dec[] = {
    { "\"roms/fdc-1/FDC-1A_D5B_74S287.bin\"", "standard: 8257 04-0BH, 8272 0CH-0DH, FDC-1B 0EH-0FH",
      "standardní: 8257 04-0BH, 8272 0CH-0DH, FDC-1B 0EH-0FH" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_fdc1_sw[] = {
    { "\"8sd\"", "8\" SD (read by IN 0EH: D1 = 1, D0 = 0; 8 MHz: FM 250, MFM 500 kbit/s)",
      "8\" SD (čte IN 0EH: D1 = 1, D0 = 0; 8 MHz: FM 250, MFM 500 kbit/s)" },
    { "\"8dd\"", "8\" DD (D1 = 1, D0 = 1; 8 MHz, also PC HD drives 3.5\" and 5.25\")",
      "8\" DD (D1 = 1, D0 = 1; 8 MHz, i mechaniky PC HD 3,5\" a 5,25\")" },
    { "\"5sd\"", "5.25\" SD (D1 = 0, D0 = 0; 4 MHz: FM 125 kbit/s)", "5,25\" SD (D1 = 0, D0 = 0; 4 MHz: FM 125 kbit/s)" },
    { "\"5dd\"", "5.25\" DD (D1 = 0, D0 = 1; 4 MHz: MFM 250 kbit/s, DD disks)",
      "5,25\" DD (D1 = 0, D0 = 1; 4 MHz: MFM 250 kbit/s, diskety DD)" },
    { nullptr, nullptr, nullptr },
};
#define FDC1_DRIVE(k, l, def)                                                                                            \
    { k, PK_SECTION, "Drive " l, "Mechanika " l, def, nullptr, 0, 0 },                    \
    { k ".image", PK_FILE, "Disk image (.img by its size: 8\" SS SD, PC 3.5\"/5.25\" HD/DD; or ImageDisk .imd)",                \
      "Obraz diskety (.img podle velikosti: 8\" SS SD, PC 3,5\"/5,25\" HD/DD; nebo ImageDisk .imd)", "\"\"",         \
      k_fdc1_disks, 0, 0 },                                                                                              \
    { k ".write_protect", PK_BOOL, "Write protected", "Chráněná proti zápisu", "false", nullptr, 0, 0 },                    \
    { k ".cpm", PK_TEXT, "CP/M letter of the drive (Media window; empty = seen while running)",              \
      "Písmeno mechaniky v CP/M (okno Média; prázdné = zjistí se za chodu)", "\"\"", nullptr, 0, 0 },
static const CardParam k_fdc1[] = {
    FDC1_DRIVE("drive_a", "0", "true")
    FDC1_DRIVE("drive_b", "1", "false")
    FDC1_DRIVE("drive_c", "2", "false")
    FDC1_DRIVE("drive_d", "3", "false")
    { "working_copy", PK_BOOL, "Work on copies in work/", "Pracovat s kopiemi ve work/", "true", nullptr, 0, 0 },
    { "decoder", PK_FILE, "PROM D5B (FDC-1A): port decoder", "PROM D5B (FDC-1A): dekodér portů",
      "\"roms/fdc-1/FDC-1A_D5B_74S287.bin\"", k_fdc1_dec, 0, 0 },
    { "rdy_ground", PK_BOOL, "FDC-1A X3-20 /RDY grounded (as supplied, Consul 7113): all drives ready",
      "FDC-1A X3-20 /RDY spojený se zemí (z výroby, Consul 7113): všechny mechaniky připravené", "true", nullptr, 0, 0 },
    { "motor_line", PK_BOOL, "Drives turn only with MOTOR ON (FDC-1B X3 /M0-/M3, register 0FH: PC drives); else always (8\" AC motors)",
      "Mechaniky se točí jen s MOTOR ON (FDC-1B X3 /M0–/M3, registr 0FH: mechaniky PC); jinak stále (8\" se střídavým motorem)",
      "false", nullptr, 0, 0 },
    { "motor_hl", PK_BOOL, "MOTOR ON from /HL of the 8272 (FDC-1A X3, straight cable as for 8\", EC1045): drives turn while the head is loaded",
      "MOTOR ON z /HL 8272 (FDC-1A X3, přímý kabel jako pro 8\", EC1045): mechaniky se točí, když je hlava přiklopená",
      "false", nullptr, 0, 0 },
    { "spinup_ms", PK_INT, "Drive: spin-up after MOTOR ON (ms)", "Mechanika: rozběh po MOTOR ON (ms)", "500", nullptr, 0, 5000 },
    { "sa2_a", PK_CHOICE, "FDC-1B switches SA2 S1, S5: drive A", "FDC-1B přepínače SA2 S1, S5: mechanika A", "\"8sd\"",
      k_fdc1_sw, 0, 0 },
    { "sa2_b", PK_CHOICE, "FDC-1B switches SA2 S2, S6: drive B", "FDC-1B přepínače SA2 S2, S6: mechanika B", "\"8sd\"",
      k_fdc1_sw, 0, 0 },
    { "sa2_c", PK_CHOICE, "FDC-1B switches SA2 S3, S7: drive C", "FDC-1B přepínače SA2 S3, S7: mechanika C", "\"8sd\"",
      k_fdc1_sw, 0, 0 },
    { "sa2_d", PK_CHOICE, "FDC-1B switches SA2 S4, S8: drive D", "FDC-1B přepínače SA2 S4, S8: mechanika D", "\"8sd\"",
      k_fdc1_sw, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};
static const ParamOption k_mfc1a_disks[] = {
    { "\"media/sapi15/SAPI15_1.sp2\"", "SAPI-1.5 CP/M 62K Melsoft disk 1: system, PIP, STAT, SYSGEN, FORMAT, SETDBS1",
      "SAPI-1.5 CP/M 62K Melsoft disketa 1: systém, PIP, STAT, SYSGEN, FORMAT, SETDBS1" },
    { "\"media/sapi15/SAPI15_2.sp2\"", "SAPI-1.5 disk 2: ASM, ED, M80, L80, F80, WM, SORT",
      "SAPI-1.5 disketa 2: ASM, ED, M80, L80, F80, WM, SORT" },
    { "\"media/sapi15/SAPI15_3_opr.sp2\"", "SAPI-1.5 disk 3: MBASIC, BASCOM, COBOL (system track from disk 1)",
      "SAPI-1.5 disketa 3: MBASIC, BASCOM, COBOL (systémová stopa z diskety 1)" },
    { "\"media/sapi15/SAPI15_4.sp2\"", "SAPI-1.5 disk 4: Pascal MT+", "SAPI-1.5 disketa 4: Pascal MT+" },
    { "\"media/sapi15/SAPI15_7.sp2\"", "SAPI-1.5 disk 7: sources (ROMBIOS.MAC), CONNECT, SIMPROG, TAPEREC",
      "SAPI-1.5 disketa 7: zdrojáky (ROMBIOS.MAC), CONNECT, SIMPROG, TAPEREC" },
    { "\"media/sapi15/SAPI15_8.sp2\"", "SAPI-1.5 disk 8: ROMBIOS.MAC/PRN/REL, TAPEREC",
      "SAPI-1.5 disketa 8: ROMBIOS.MAC/PRN/REL, TAPEREC" },
    { "\"media/sapiman/prazdna_8_FM_14x256.imd\"", "empty 8\" FM 77 x 14 x 256 (ImageDisk, BIOS V261211JB)",
      "prázdná 8\" FM 77 × 14 × 256 (ImageDisk, BIOS V261211JB)" },
    { "\"media/sapiman/prazdna_8_FM_8x512.imd\"", "empty 8\" FM 77 x 8 x 512 (ImageDisk, BIOS-3K V261211JB)",
      "prázdná 8\" FM 77 × 8 × 512 (ImageDisk, BIOS-3K V261211JB)" },
    { "\"media/rpd-1/CPM22P_48A_Perina.img\"", "CP/M 2.2/P 48K (Perina), as a data disk",
      "CP/M 2.2/P 48K (Peřina), jako datová disketa" },
    { "\"media/fdc-1/CPM22P_48AF_Perina.img\"", "CP/M 2.2/P 48K for FDC-1 (Perina), as a data disk",
      "CP/M 2.2/P 48K pro FDC-1 (Peřina), jako datová disketa" },
    { "\"media/sapiman/m1_281211_zrmd.img\"", "CP/M boot disk J. Biba (MON 281211, C: ZRMD-1)",
      "Bootovací CP/M J. Bíba (MON 281211, C: ZRMD-1)" },
    { "\"media/sapiman/m1_281211_rmd.img\"", "CP/M boot disk J. Biba (MON 281211, C: RMD-1)",
      "Bootovací CP/M J. Bíba (MON 281211, C: RMD-1)" },
    { "\"media/sapiman/Z6_MFC281211_RMD_programy.img\"", "J. Biba Z6: CP/M boot, programs (C: RMD-1)",
      "J. Bíba Z6: bootovací CP/M, programy (C: RMD-1)" },
    { "\"media/sapiman/Z5_MFC281211_RMD_zdroje.img\"", "J. Biba Z5: CP/M boot, sources of the MFC-1A system",
      "J. Bíba Z5: bootovací CP/M, zdroje systému pro MFC-1A" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_mfc1a_dec[] = {
    { "\"roms/mfc-1a/MFC-1A_DD9_74S287.bin\"", "standard: 8257 04H, 05H, 08H, 8272 0CH-0DH, drive register 0FH",
      "standardní: 8257 04H, 05H, 08H, 8272 0CH-0DH, registr mechanik 0FH" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_mfc1a_res[] = {
    { "\"level\"", "1 holds the reset (schematic; the unfixed ECBIM8 hangs at its initialization)",
      "1 drží reset (schéma; neopravený ECBIM8 se při inicializaci zasekne)" },
    { "\"pulse\"", "pulse on the rising edge (DEVIATION from the schematic, the unfixed BIOS ECBIM8 works)",
      "pulz na náběžné hraně (ODCHYLKA od schématu, neopravený BIOS ECBIM8 funguje)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_mfc1a_wiring[] = {
    { "\"ec1045\"", "Tesla board as made (No. 0124) = redraw EC1045: D5 reset, D4 INT, D6 = 1 drives on",
      "deska Tesla, jak se vyráběla (v. č. 0124) = překreslení EC1045: D5 reset, D4 INT, D6 = 1 mechaniky zapnuté" },
    { "\"tesla\"", "Tesla schematic and manual (differ from the board): D4 reset, D5 INT, D6 = 1 drives off",
      "schéma a návod Tesla (liší se od desky): D4 reset, D5 INT, D6 = 1 mechaniky vypnuté" },
    { nullptr, nullptr, nullptr },
};
#define MFC1A_DRIVE(k, l, def)                                                                                           \
    { k, PK_SECTION, "Drive " l, "Mechanika " l, def, nullptr, 0, 0 },                    \
    { k ".image", PK_FILE, "Disk image (.img by its size: 8\" SS SD, PC 3.5\"/5.25\" HD/DD, SAPI-1.5 .sp2; or ImageDisk .imd)",  \
      "Obraz diskety (.img podle velikosti: 8\" SS SD, PC 3,5\"/5,25\" HD/DD, SAPI-1.5 .sp2; nebo ImageDisk .imd)", "\"\"", \
      k_mfc1a_disks, 0, 0 },                                                                                             \
    { k ".write_protect", PK_BOOL, "Write protected", "Chráněná proti zápisu", "false", nullptr, 0, 0 },                    \
    { k ".cpm", PK_TEXT, "CP/M letter of the drive (Media window; empty = seen while running)",              \
      "Písmeno mechaniky v CP/M (okno Média; prázdné = zjistí se za chodu)", "\"\"", nullptr, 0, 0 },
static const CardParam k_mfc1a[] = {
    MFC1A_DRIVE("drive_a", "0", "true")
    MFC1A_DRIVE("drive_b", "1", "false")
    MFC1A_DRIVE("drive_c", "2", "false")
    MFC1A_DRIVE("drive_d", "3", "false")
    { "working_copy", PK_BOOL, "Work on copies in work/", "Pracovat s kopiemi ve work/", "true", nullptr, 0, 0 },
    { "decoder", PK_FILE, "PROM DD9: port decoder", "PROM DD9: dekodér portů", "\"roms/mfc-1a/MFC-1A_DD9_74S287.bin\"",
      k_mfc1a_dec, 0, 0 },
    { "rdy_ground", PK_BOOL, "X2-19 RDY grounded (8\" Consul 7113 have no READY); else READY from the drive (disk in)",
      "X2-19 RDY spojený se zemí (8\" Consul 7113 nemají READY); jinak READY z mechaniky (vložená disketa)", "false",
      nullptr, 0, 0 },
    { "motor_line", PK_BOOL, "Drives turn only with MOTOR ON (MO0n-MO3n, register 0FH D3 D2: 5.25\" drives); else always (8\" AC motors)",
      "Mechaniky se točí jen s MOTOR ON (MO0n–MO3n, registr 0FH D3 D2: mechaniky 5,25\"); jinak stále (8\" se střídavým motorem)",
      "false", nullptr, 0, 0 },
    { "spinup_ms", PK_INT, "Drive: spin-up after MOTOR ON (ms)", "Mechanika: rozběh po MOTOR ON (ms)", "500", nullptr, 0, 5000 },
    { "wiring", PK_CHOICE, "Wiring of the drive register 0FH", "Zapojení registru mechanik 0FH", "\"ec1045\"",
      k_mfc1a_wiring, 0, 0 },
    { "reset_mode", PK_CHOICE, "Register 0FH: the reset bit of the 8272", "Registr 0FH: bit resetu 8272", "\"level\"",
      k_mfc1a_res, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};
#undef MFC1A_DRIVE
#undef FDC1_DRIVE

static const ParamOption k_dgd1_base[] = {
    { "\"C000\"", "C000H (switch SP1-2 open, ZPS-2, ZPS-3)", "C000H (přepínač SP1-2 rozepnutý, ZPS-2, ZPS-3)" },
    { "\"0000\"", "0000H (switch SP1-2 closed, ZPS-4, SAPI-1Z)", "0000H (přepínač SP1-2 sepnutý, ZPS-4, SAPI-1Z)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_dgd1[] = {
    { "base", PK_CHOICE, "Switch SP1-2: graphics RAM 16 KB from", "Přepínač SP1-2: grafická RAM 16 KB od", "\"C000\"",
      k_dgd1_base, 0, 0 },
    { "mapping", PK_BOOL, "Switch SP1-1: with mapping (selected by MAP1)", "Přepínač SP1-1: s mapováním (výběr signálem MAP1)",
      "false", nullptr, 0, 0 },
    { "wait_states", PK_BOOL, "Wait states (CPU waits for its window of the display)",
      "Čekací stavy (CPU čeká na své okno displeje)", "true", nullptr, 0, 0 },
    { "monitor", PK_CHOICE, "Monitor: phosphor colour (not on the card)", "Monitor: barva luminoforu (není na kartě)",
      "\"white\"", k_monitor, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

static const ParamOption k_and1a_dec[] = {
    { "\"roms/and-1a/AND-1A_D3A_74S287.bin\"", "standard: E800, S1 closed 3800", "standardní: E800, S1 sepnutá 3800" },
    { "\"roms/and-1a/AND-1A_D3A_CPM62K_74S287.bin\"", "CP/M 62K (home made): F800, S1 closed 3800",
      "CP/M 62K (domácí): F800, S1 sepnutá 3800" },
    { "\"roms/and-1a/AND-1A_D3A_CPM64K_74S287.bin\"", "CP/M 64K (assumed): F000, S1 closed 3800",
      "CP/M 64K (předpoklad): F000, S1 sepnutá 3800" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_and1a[] = {
    { "charset", PK_CHOICE, "Character generator", "Generátor znaků", "\"MHB2501\"", k_and1_cs, 0, 0 },
    { "rows", PK_CHOICE, "Rows", "Řádků", "24", k_and1_rows, 0, 0 },
    { "int1", PK_BOOL, "Jumper 49-50: INT1 at the end of the frame", "Propojka 49-50: INT1 na konci snímku", "true",
      nullptr, 0, 0 },
    { "s1", PK_BOOL, "Jumper 44-45 (S1): VRAM at 3800 instead of E800", "Propojka 44-45 (S1): VRAM na 3800 místo E800",
      "false", nullptr, 0, 0 },
    { "decoder", PK_FILE, "PROM D3A: address decoder", "PROM D3A: dekodér adresy", "\"roms/and-1a/AND-1A_D3A_74S287.bin\"",
      k_and1a_dec, 0, 0 },
    { "monitor", PK_CHOICE, "Monitor: phosphor colour (not on the card)", "Monitor: barva luminoforu (není na kartě)",
      "\"white\"", k_monitor, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

static const ParamOption k_ram1z_kb[] = {
    { "16", "16 KB (1 row)", "16 KB (1 řada)" },
    { "32", "32 KB (2 rows)", "32 KB (2 řady)" },
    { "48", "48 KB (3 rows)", "48 KB (3 řady)" },
    { "64", "64 KB (4 rows)", "64 KB (4 řady)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_ram1z_proms[] = {
    { "\"roms/ram-1z/RAM-1Z_D3B_74S571.bin\"", "standard (dump)", "standardní (dump)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_ram1z[] = {
    { "fitted_kb", PK_CHOICE, "Fitted memory", "Osazená paměť", "64", k_ram1z_kb, 0, 0 },
    { "s1_1", PK_BOOL, "S1/1 closed (open = no F800-FFFF, 62 KB)", "S1/1 sepnutý (rozpojený = bez F800-FFFF, 62 KB)",
      "false", nullptr, 0, 0 },
    { "s1_2", PK_BOOL, "S1/2 closed (open = no F000-FFFF, 60 KB)", "S1/2 sepnutý (rozpojený = bez F000-FFFF, 60 KB)",
      "true", nullptr, 0, 0 },
    { "map", PK_BOOL, "S1/4: MAP1 removes 0000-3FFF (else S1/3, ground)", "S1/4: MAP1 vypíná 0000-3FFF (jinak S1/3, zem)",
      "false", nullptr, 0, 0 },
    { "prom", PK_FILE, "PROM D3B: address decoder", "PROM D3B: dekodér adres", "\"roms/ram-1z/RAM-1Z_D3B_74S571.bin\"",
      k_ram1z_proms, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};
static const ParamOption k_rmd1_kb[] = {
    { "256", "256 KB (32x 4164)", "256 KB (32x 4164)" },
    { "1024", "1 MB (32x 41256)", "1 MB (32x 41256)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_rmd1_fill[] = {
    { "\"random\"", "random (like the DRAM, initialise it)", "náhodný (jako DRAM, nutno inicializovat)" },
    { "\"e5\"", "E5h (empty CP/M disk)", "E5h (prázdný disk CP/M)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_rmd1_proms[] = {
    { "\"roms/rmd-1/RMD-1_74S287.bin\"", "standard: ports 40-43, 45", "standardní: porty 40-43, 45" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_rmd1[] = {
    { "size_kb", PK_CHOICE, "Memory", "Paměť", "256", k_rmd1_kb, 0, 0 },
    { "prom", PK_FILE, "PROM: port decoder", "PROM: dekodér portů", "\"roms/rmd-1/RMD-1_74S287.bin\"", k_rmd1_proms, 0, 0 },
    { "power_on", PK_CHOICE, "Content after power on", "Obsah po zapnutí", "\"random\"", k_rmd1_fill, 0, 0 },
    { "image", PK_FILE, "Keep the content in a file (emulator only)", "Uchovat obsah v souboru (jen emulátor)", "\"\"",
      k_none, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// CPU cards not emulated yet: only what the configurator should remember
static const CardParam k_cpu_basic[] = {
    { "rom", PK_FILE, "Monitor EPROM", "EPROM monitoru", "\"\"", k_none, 0, 0 },
    { "keyboard", PK_KEYBOARD, "Keyboard", "Klávesnice", "null", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// DMR-1
static const ParamOption k_dmr_ram[] = {
    { "0", "0: whole 64 KB", "0: celých 64 KB" },
    { "1", "1: 0000-F7FF (display at F800)", "1: 0000-F7FF (displej na F800)" },
    { "2", "2: 0000-EFFF", "2: 0000-EFFF" },
    { "3", "3: none", "3: žádná" },
    { "4", "4: 4000-FFFF", "4: 4000-FFFF" },
    { "5", "5: 4000-F7FF", "5: 4000-F7FF" },
    { "6", "6: 4000-EFFF", "6: 4000-EFFF" },
    { "7", "7: none", "7: žádná" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dmr_base[] = {
    { "\"10h\"", "10h", "10h" }, { "\"14h\"", "14h", "14h" },
    { "\"18h\"", "18h", "18h" }, { "\"1Ch\"", "1Ch", "1Ch" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_baud[] = {
    { "75", "75 Bd", "75 Bd" }, { "150", "150 Bd", "150 Bd" }, { "300", "300 Bd", "300 Bd" },
    { "600", "600 Bd", "600 Bd" }, { "1200", "1200 Bd", "1200 Bd" }, { "2400", "2400 Bd", "2400 Bd" },
    { "4800", "4800 Bd", "4800 Bd" }, { "9600", "9600 Bd", "9600 Bd" }, { "19200", "19200 Bd", "19200 Bd" },
    { "38400", "38400 Bd", "38400 Bd" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_flow[] = {
    { "\"none\"", "none", "žádné" },
    { "\"rts_cts\"", "RTS/CTS", "RTS/CTS" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_tcp_proto[] = {
    { "\"telnet\"", "telnet", "telnet" },
    { "\"raw\"", "raw", "raw" },
    { "\"auto\"", "auto (telnet when the client starts it)", "auto (telnet, když ho klient zahájí)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_dmr1[] = {
    { "ram", PK_BOOL, "RAM 128 KB (off = serial port only)", "RAM 128 KB (vypnuto = jen sériový port)", "true",
      nullptr, 0, 0 },
    { "battery", PK_BOOL, "JP7: battery backup of the SRAM", "JP7: zálohování SRAM baterií", "true", nullptr, 0, 0 },
    { "sram_file", PK_FILE, "SRAM content kept by the battery", "Obsah SRAM držený baterií",
      "\"work/dmr-1/sram.bin\"", k_none, 0, 0 },
    { "ram_config", PK_CHOICE, "JP3: RAM range", "JP3: rozsah RAM", "1", k_dmr_ram, 0, 0 },
    { "a6_map1", PK_BOOL, "JP3: A6 from MAP1", "JP3: A6 z MAP1", "false", nullptr, 0, 0 },
    { "a7_map2", PK_BOOL, "JP3: A7 from MAP2", "JP3: A7 z MAP2", "false", nullptr, 0, 0 },
    { "decoder", PK_FILE, "PROM D2C: RAM decoder", "PROM D2C: dekodér RAM", "\"roms/dmr-1/DMR-1_D2C.bin\"",
      k_none, 0, 0 },
    { "uart", PK_BOOL, "UART MHB1012", "UART MHB1012", "true", nullptr, 0, 0 },
    { "uart_base", PK_CHOICE, "JP2: UART ports", "JP2: porty UART", "\"10h\"", k_dmr_base, 0, 0 },
    { "baud", PK_CHOICE, "JP8: baud rate", "JP8: rychlost", "9600", k_baud, 0, 0 },
    { "int0", PK_BOOL, "UART interrupt on INT0", "Přerušení UART na INT0", "true", nullptr, 0, 0 },
    { "connected", PK_BOOL, "Terminal connected at start (DCD)", "Terminál připojen při startu (DCD)", "true",
      nullptr, 0, 0 },
    { "cts", PK_BOOL, "Terminal ready at start (CTS/DSR)", "Terminál připraven při startu (CTS/DSR)", "true",
      nullptr, 0, 0 },
    { "flow_control", PK_CHOICE, "Flow control", "Řízení toku", "\"none\"", k_flow, 0, 0 },
    { "serial.pacing", PK_BOOL, "PC side: no overrun (pacing)", "Strana PC: bez přetečení", "true", nullptr, 0, 0 },
    { "serial.tcp", PK_SECTION, "PC side: TCP server", "Strana PC: TCP server", "false", nullptr, 0, 0 },
    { "serial.tcp.port", PK_INT, "TCP port", "TCP port", "2323", nullptr, 1, 65535 },
    { "serial.tcp.bind", PK_TEXT, "Address (127.0.0.1 = this PC only)", "Adresa (127.0.0.1 = jen tento PC)",
      "\"127.0.0.1\"", nullptr, 0, 0 },
    { "serial.tcp.protocol", PK_CHOICE, "Protocol", "Protokol", "\"telnet\"", k_tcp_proto, 0, 0 },
    { "serial.tcp.dcd_follows", PK_BOOL, "DCD follows the client connection", "DCD podle připojení klienta",
      "true", nullptr, 0, 0 },
    { "serial.com", PK_SECTION, "PC side: COM port", "Strana PC: COM port", "false", nullptr, 0, 0 },
    { "serial.com.port", PK_TEXT, "Port (COM5, /dev/ttyUSB0)", "Port (COM5, /dev/ttyUSB0)", "\"COM1\"", nullptr, 0, 0 },
    { "serial.com.baud", PK_INT, "Baud rate", "Rychlost", "9600", nullptr, 50, 921600 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// DSM-1 (not emulated yet): what the configurator should remember
static const ParamOption k_dsm_baud[] = {
    { "50", "50 Bd", "50 Bd" }, { "75", "75 Bd", "75 Bd" }, { "150", "150 Bd", "150 Bd" },
    { "300", "300 Bd", "300 Bd" }, { "600", "600 Bd (factory)", "600 Bd (z výroby)" },
    { "1200", "1200 Bd", "1200 Bd" }, { "2400", "2400 Bd", "2400 Bd" }, { "4800", "4800 Bd", "4800 Bd" },
    { "9600", "9600 Bd", "9600 Bd" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_dsm1[] = {
    { "base", PK_CHOICE, "Jumpers 16-12..16-15: ports", "Propojky 16-12 až 16-15: porty", "\"10h\"", k_dmr_base, 0, 0 },
    { "baud", PK_CHOICE, "Jumper: terminal baud rate", "Propojka: rychlost terminálu", "600", k_dsm_baud, 0, 0 },
    { "int0", PK_BOOL, "UART interrupt on INT0", "Přerušení UART na INT0", "true", nullptr, 0, 0 },
    { "tape", PK_FILE, "Cassette in the recorder (WAV, MP3; a .hex program becomes a MIKOS tape)",
      "Kazeta v magnetofonu (WAV, MP3; program .hex se převede na kazetu MIKOS)", "\"\"", k_none, 0, 0 },
    { "tape_write_protect", PK_BOOL, "Cassette write protected", "Kazeta chráněná proti zápisu", "true", nullptr, 0, 0 },
    { "connected", PK_BOOL, "Terminal connected at start (DCD)", "Terminál připojen při startu (DCD)", "true",
      nullptr, 0, 0 },
    { "cts", PK_BOOL, "Terminal ready at start (CTS/DSR)", "Terminál připraven při startu (CTS/DSR)", "true",
      nullptr, 0, 0 },
    { "flow_control", PK_CHOICE, "Flow control", "Řízení toku", "\"none\"", k_flow, 0, 0 },
    { "serial.pacing", PK_BOOL, "PC side: no overrun (pacing)", "Strana PC: bez přetečení", "true", nullptr, 0, 0 },
    { "serial.tcp", PK_SECTION, "PC side: TCP server", "Strana PC: TCP server", "false", nullptr, 0, 0 },
    { "serial.tcp.port", PK_INT, "TCP port", "TCP port", "2323", nullptr, 1, 65535 },
    { "serial.tcp.bind", PK_TEXT, "Address (127.0.0.1 = this PC only)", "Adresa (127.0.0.1 = jen tento PC)",
      "\"127.0.0.1\"", nullptr, 0, 0 },
    { "serial.tcp.protocol", PK_CHOICE, "Protocol", "Protokol", "\"telnet\"", k_tcp_proto, 0, 0 },
    { "serial.tcp.dcd_follows", PK_BOOL, "DCD follows the client connection", "DCD podle připojení klienta",
      "true", nullptr, 0, 0 },
    { "serial.com", PK_SECTION, "PC side: COM port", "Strana PC: COM port", "false", nullptr, 0, 0 },
    { "serial.com.port", PK_TEXT, "Port (COM5, /dev/ttyUSB0)", "Port (COM5, /dev/ttyUSB0)", "\"COM1\"", nullptr, 0, 0 },
    { "serial.com.baud", PK_INT, "Baud rate", "Rychlost", "600", nullptr, 50, 921600 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// AND-1Z / AND-1Z-89
static const ParamOption k_chargen[] = {
    { "\"roms/and-1z-89/AND-1Z_CharGen.bin\"", "KOI-8 cs", "KOI-8 cs" },
    { "\"roms/and-1z-89/AND-1Z_CharGen_upr10-1F.bin\"", "KOI-8 cs, modified 10h-1Fh", "KOI-8 cs, upravené 10h-1Fh" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_and1z[] = {
    { "chargen", PK_FILE, "Character generator EPROM", "EPROM generátoru znaků",
      "\"roms/and-1z-89/AND-1Z_CharGen.bin\"", k_chargen, 0, 0 },
    { "decoder", PK_FILE, "PROM D1: address decoder", "PROM D1: dekodér adresy",
      "\"roms/and-1z-89/PROM_D1.hex.bin\"", k_none, 0, 0 },
    { "map_enabled", PK_BOOL, "S1: MAP1 disconnects the card", "S1: MAP1 odpojí kartu", "false", nullptr, 0, 0 },
    { "wait_states", PK_BOOL, "RDY wait states on VRAM access", "Čekání (RDY) při přístupu do VRAM", "true",
      nullptr, 0, 0 },
    { "monitor", PK_CHOICE, "Monitor: phosphor colour (not on the card)", "Monitor: barva luminoforu (není na kartě)",
      "\"white\"", k_monitor, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// ZRMD-1 / ZRD-1V
static const ParamOption k_zrmd_images[] = {
    { "\"media/zrmd-1/SAPI_CPM_Martin_2019-10-07.bin\"", "CP/M (Martin 2019)", "CP/M (Martin 2019)" },
    { "\"media/zrmd-1/SAPI_CPM_Martin_2019-10-07_fixMFC.bin\"", "CP/M (Martin 2019), BIOS ECBIM8 (MFC-1A) fixed",
      "CP/M (Martin 2019), opravený BIOS ECBIM8 (MFC-1A)" },
    { "\"media/zrmd-1/SAPI_CPM_Martin_2019-10-07_sysEC1045.bin\"", "CP/M (Martin 2019), systems of EC1045 (3.5\"/5.25\" HD)",
      "CP/M (Martin 2019), systémy EC1045 (3,5\"/5,25\" HD)" },
    { "\"media/zrmd-1/ZRMD-1_prazdny_OFF0.bin\"", "empty, CP/M over all 4 MB (systems of J. Biba 2011)",
      "prázdný, CP/M přes celé 4 MB (systémy J. Bíby 2011)" },
    { "\"media/zrmd-1/ECSYS_zaloha_0-3.hex\"", "ECSYS 0-3 (HEX)", "ECSYS 0-3 (HEX)" },
    { "\"media/zrmd-1/puvodni_SYS0_3_s_CD.HEX\"", "original SYS 0-3 with CD (HEX)", "původní SYS 0-3 s CD (HEX)" },
    { "\"media/zrmd-1/ba-ramdisk.bin\"", "CP/M with the demo Bad Apple!! (CGA-1V, MPH-1V)", "CP/M s demem Bad Apple!! (CGA-1V, MPH-1V)" },
    { "\"media/zrmd-1/hry-a-dema.bin\"", "CP/M with games and demos: Bad Apple!!, FXS4, Flappy (CGA-1V, DGD-1V), Bomberman, BKP, demos of L. Lasota, music",
      "CP/M s hrami a demy: Bad Apple!!, FXS4, Flappy (CGA-1V, DGD-1V), Bomberman, BKP, dema L. Lasoty, hudba" },
    { "\"media/zrmd-1/pop-ramdisk.bin\"", "CP/M with Prince of Persia, DOS graphics and music (CGA-1V, MPH-1V)",
      "CP/M s Prince of Persia, grafika a hudba DOS (CGA-1V, MPH-1V)" },
    { "\"media/zrmd-1/bombernet.bin\"", "CP/M with the network game BomberNet (CGA-1V, MPH-1V, DPI-1L)",
      "CP/M se síťovou hrou BomberNet (CGA-1V, MPH-1V, DPI-1L)" },
    { "\"\"", "empty", "prázdný" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_zrmd_mode[] = {
    { "\"rw\"", "read/write, saved", "čtení i zápis, ukládá se" },
    { "\"nosave\"", "read/write, not saved", "čtení i zápis, neukládá se" },
    { "\"readonly\"", "read only", "jen čtení" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_rtc_int[] = {
    { "\"none\"", "none", "žádné" },
    { "\"int0\"", "INT0", "INT0" },
    { "\"int1\"", "INT1 (NMI)", "INT1 (NMI)" },
    { nullptr, nullptr, nullptr },
};
#define ZRMD_COMMON \
    { "image", PK_FILE, "Disk image", "Obraz disku", "\"\"", k_zrmd_images, 0, 0 }, \
    { "working_copy", PK_BOOL, "Work on a copy in work/", "Pracovat s kopií ve work/", "true", nullptr, 0, 0 }, \
    { "mode", PK_CHOICE, "Mode", "Režim", "\"rw\"", k_zrmd_mode, 0, 0 }, \
    { "chips", PK_INT, "SRAM chips (512 KB each)", "Počet SRAM (po 512 KB)", "8", nullptr, 1, 8 }, \
    { "base", PK_HEX, "Port base", "Báze portů", "\"60h\"", nullptr, 0, 0xE0 },
static const CardParam k_zrmd1[] = {
    ZRMD_COMMON
    { "write_enable.0", PK_BOOL, "S1: write enable, 1st MB", "S1: povolení zápisu, 1. MB", "true", nullptr, 0, 0 },
    { "write_enable.1", PK_BOOL, "S2: write enable, 2nd MB", "S2: povolení zápisu, 2. MB", "true", nullptr, 0, 0 },
    { "write_enable.2", PK_BOOL, "S3: write enable, 3rd MB", "S3: povolení zápisu, 3. MB", "true", nullptr, 0, 0 },
    { "write_enable.3", PK_BOOL, "S4: write enable, 4th MB", "S4: povolení zápisu, 4. MB", "true", nullptr, 0, 0 },
    { "rtc", PK_BOOL, "RTC 72421", "RTC 72421", "true", nullptr, 0, 0 },
    { "rtc_int", PK_CHOICE, "JP3: RTC interrupt", "JP3: přerušení RTC", "\"none\"", k_rtc_int, 0, 0 },
    { "map_jp2", PK_BOOL, "JP2: port 63h b6/b7 drive MAP1/MAP2", "JP2: port 63h b6/b7 budí MAP1/MAP2", "false",
      nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};
static const ParamOption k_zrd1v_rev[] = {
    { "\"2.1\"", "rev 2.1 (lock, LEDs, battery)", "rev 2.1 (zámek, LED, baterie)" },
    { "\"1.1\"", "rev 1.1 (no lock, no LEDs)", "rev 1.1 (bez zámku a LED)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_zrd1v[] = {
    { "revision", PK_CHOICE, "Board revision", "Revize desky", "\"2.1\"", k_zrd1v_rev, 0, 0 },
    ZRMD_COMMON
    { "battery_mv", PK_INT, "Backup battery (mV, rev 2.1 measures it)", "Zálohovací baterie (mV, rev 2.1 ji měří)", "3000",
      nullptr, 0, 4095 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// MPH-1V
static const CardParam k_mph1v[] = {
    { "base", PK_HEX, "S1: port base (8 ports)", "S1: báze portů (8 portů)", "\"50h\"", nullptr, 0, 0xF8 },
    { "k4", PK_CHOICE, "K4 (OVL): plugged in", "K4 (OVL): připojeno", "\"joystick\"", k_joyport, 0, 0 },
    { "k2", PK_CHOICE, "K2 (MYŠ): plugged in", "K2 (MYŠ): připojeno", "\"mouse_atari\"", k_joyport, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// DPP-1
static const ParamOption k_dpp1_base[] = {
    { "\"00h\"", "00h-07h (2S1, 3S1 closed)", "00h-07h (2S1, 3S1 sepnuté)" },
    { "\"08h\"", "08h-0Fh (2S1 closed)", "08h-0Fh (2S1 sepnuté)" },
    { "\"10h\"", "10h-17h (3S1 closed)", "10h-17h (3S1 sepnuté)" },
    { "\"18h\"", "18h-1Fh (all open)", "18h-1Fh (všechny rozepnuté)" },
    { "\"20h\"", "20h-27h (1S1, 2S1, 3S1 closed)", "20h-27h (1S1, 2S1, 3S1 sepnuté)" },
    { "\"28h\"", "28h-2Fh (1S1, 2S1 closed)", "28h-2Fh (1S1, 2S1 sepnuté)" },
    { "\"30h\"", "30h-37h (1S1, 3S1 closed)", "30h-37h (1S1, 3S1 sepnuté)" },
    { "\"38h\"", "38h-3Fh (1S1 closed)", "38h-3Fh (1S1 sepnuté)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dpp1_channel[] = {
    { "2", "channel 2 (base+4..7, X3), as ECMON and J. Biba", "kanál 2 (základ+4..7, X3), jako ECMON a J. Bíba" },
    { "1", "channel 1 (base+0..3, X2)", "kanál 1 (základ+0..3, X2)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dpp1_prn_channel[] = {
    { "1", "channel 1 (base+0..3, X2), as ECMON and J. Biba", "kanál 1 (základ+0..3, X2), jako ECMON a J. Bíba" },
    { "2", "channel 2 (base+4..7, X3)", "kanál 2 (základ+4..7, X3)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dpp1_xy_channel[] = {
    { "1", "channel 1 (port B 31h with base 30h, X2), as KRES (configuration 0Dh)", "kanál 1 (port B 31h při základu 30h, X2), jako KRES (konfigurace 0Dh)" },
    { "2", "channel 2 (port B, X3)", "kanál 2 (port B, X3)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dpp1_mg_channel[] = {
    { "1", "channel 1 (port B 31h with base 30h, X2), as the driver V.1-MGF", "kanál 1 (port B 31h při základu 30h, X2), jako ovladač V.1-MGF" },
    { "2", "channel 2 (port B, X3)", "kanál 2 (port B, X3)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dpp1_kzd_channel[] = {
    { "1", "channel 1 (base+0..3, X2), as MIKOS/K (30h)", "kanál 1 (základ+0..3, X2), jako MIKOS/K (30h)" },
    { "2", "channel 2 (base+4..7, X3)", "kanál 2 (základ+4..7, X3)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_dpp1[] = {
    { "base", PK_CHOICE, "S1: ports", "S1: porty", "\"28h\"", k_dpp1_base, 0, 0 },
    { "pmd_channel", PK_CHOICE, "PMD32-SD on", "PMD32-SD na", "2", k_dpp1_channel, 0, 0 },
    { "pmd32sd", PK_SECTION, "PMD32-SD floppy emulator", "Emulátor disket PMD32-SD", "false", nullptr, 0, 0 },
    { "pmd32sd.sd_card", PK_DIR, "SD card (directory)", "SD karta (složka)", "\"media/pmd32sd\"", k_none, 0, 0 },
    { "pmd32sd.working_copy", PK_BOOL, "Work on a copy in work/", "Pracovat s kopií ve work/", "true",
      nullptr, 0, 0 },
    { "pmd32sd.drives.0", PK_TEXT, "Drive A", "Mechanika A", "null", nullptr, 0, 0 },
    { "pmd32sd.drives.1", PK_TEXT, "Drive B", "Mechanika B", "null", nullptr, 0, 0 },
    { "pmd32sd.drives.2", PK_TEXT, "Drive C", "Mechanika C", "null", nullptr, 0, 0 },
    { "pmd32sd.drives.3", PK_TEXT, "Drive D", "Mechanika D", "null", nullptr, 0, 0 },
    { "pmd32sd.cpm.0", PK_TEXT, "Drive A: CP/M letter (empty = seen while running)", "Mechanika A: písmeno v CP/M (prázdné = zjistí se za chodu)", "null", nullptr, 0, 0 },
    { "pmd32sd.cpm.1", PK_TEXT, "Drive B: CP/M letter (empty = seen while running)", "Mechanika B: písmeno v CP/M (prázdné = zjistí se za chodu)", "null", nullptr, 0, 0 },
    { "pmd32sd.cpm.2", PK_TEXT, "Drive C: CP/M letter (empty = seen while running)", "Mechanika C: písmeno v CP/M (prázdné = zjistí se za chodu)", "null", nullptr, 0, 0 },
    { "pmd32sd.cpm.3", PK_TEXT, "Drive D: CP/M letter (empty = seen while running)", "Mechanika D: písmeno v CP/M (prázdné = zjistí se za chodu)", "null", nullptr, 0, 0 },
    { "pmd32sd.card_locked", PK_BOOL, "SD card write protected", "SD karta chráněná proti zápisu", "false",
      nullptr, 0, 0 },
    { "pmd32sd.startup_ms", PK_INT, "Start-up delay (ms, real 3000)", "Prodleva po zapnutí (ms, skutečně 3000)",
      "0", nullptr, 0, 10000 },
    { "kzd1_channel", PK_CHOICE, "KZD-1 on", "KZD-1 na", "1", k_dpp1_kzd_channel, 0, 0 },
    { "kzd1", PK_SECTION, "KZD-1 cassette data recorder (MIKOS/K)", "Kazetopáskový záznamník dat KZD-1 (MIKOS/K)", "false",
      nullptr, 0, 0 },
    { "kzd1.cassette", PK_FILE, "Cassette (.kzd, created empty when missing)", "Kazeta (.kzd, prázdná se vytvoří)",
      "\"work/kzd-1/kazeta.kzd\"", k_none, PF_NEW_MEDIUM, 0 },
    { "minigraf_channel", PK_CHOICE, "Minigraf on", "Minigraf na", "1", k_dpp1_mg_channel, 0, 0 },
    { "minigraf", PK_SECTION, "Plotter ARITMA Minigraf 0507 (port B: X PB0-2, Y PB3-5, pen PB7; window Minigraf)",
      "Zapisovač ARITMA Minigraf 0507 (port B: X PB0-2, Y PB3-5, pero PB7; okno Minigraf)", "false", nullptr, 0, 0 },
    { "xy4130_channel", PK_CHOICE, "XY 4130 on", "XY 4130 na", "1", k_dpp1_xy_channel, 0, 0 },
    { "xy4130", PK_SECTION, "Plotter XY 4130 (port B, cable of KRES: STEP PB0, X/Y PB2, +/- PB3, READY PB5, pen PB7)",
      "Zapisovač XY 4130 (port B, kabel KRESu: KROK PB0, X/Y PB2, +/- PB3, READY PB5, pero PB7)", "false", nullptr, 0, 0 },
    XY4130_PARAMS("\"kres\"")
    { "printer_channel", PK_CHOICE, "Printer on (8255 mode 1: /OBF = /STROBE, /ACK)",
      "Tiskárna na (8255 mód 1: /OBF = /STROBE, /ACK)", "1", k_dpp1_prn_channel, 0, 0 },
    { "printer", PK_SECTION, "Printer (Centronics), ECMON and J. Biba: PUN:, LST: = UL1:",
      "Tiskárna (Centronics), ECMON a J. Bíba: PUN:, LST: = UL1:", "false", nullptr, 0, 0 },
    PRINTER_PARAMS
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// IDE-1
static const CardParam k_ide1[] = {
    { "ide_base", PK_HEX, "JP1-JP3, JP5: IDE ports", "JP1-JP3, JP5: porty IDE", "\"58h\"", nullptr, 0, 0xFC },
    { "pmd_base", PK_HEX, "PMD32-SD ports", "Porty PMD32-SD", "\"5Ch\"", nullptr, 0, 0xFC },
    { "ide", PK_SECTION, "IDE disk", "IDE disk", "false", nullptr, 0, 0 },
    { "ide.image", PK_FILE, "Disk image (.img)", "Obraz disku (.img)", "\"\"", k_none, 0, 0 },
    { "ide.create_mb", PK_INT, "Create if missing (MB, 0 = no)", "Vytvořit, pokud chybí (MB, 0 = ne)", "0",
      nullptr, 0, 8192 },
    { "ide.readonly", PK_BOOL, "Read only", "Jen pro čtení", "false", nullptr, 0, 0 },
    { "ide.working_copy", PK_BOOL, "Work on a copy in work/", "Pracovat s kopií ve work/", "true", nullptr, 0, 0 },
    { "pmd32sd", PK_SECTION, "PMD32-SD floppy emulator", "Emulátor disket PMD32-SD", "false", nullptr, 0, 0 },
    { "pmd32sd.sd_card", PK_DIR, "SD card (directory)", "SD karta (složka)", "\"media/pmd32sd\"", k_none, 0, 0 },
    { "pmd32sd.working_copy", PK_BOOL, "Work on a copy in work/", "Pracovat s kopií ve work/", "true",
      nullptr, 0, 0 },
    { "pmd32sd.drives.0", PK_TEXT, "Drive A", "Mechanika A", "null", nullptr, 0, 0 },
    { "pmd32sd.drives.1", PK_TEXT, "Drive B", "Mechanika B", "null", nullptr, 0, 0 },
    { "pmd32sd.drives.2", PK_TEXT, "Drive C", "Mechanika C", "null", nullptr, 0, 0 },
    { "pmd32sd.drives.3", PK_TEXT, "Drive D", "Mechanika D", "null", nullptr, 0, 0 },
    { "pmd32sd.cpm.0", PK_TEXT, "Drive A: CP/M letter (empty = seen while running)", "Mechanika A: písmeno v CP/M (prázdné = zjistí se za chodu)", "null", nullptr, 0, 0 },
    { "pmd32sd.cpm.1", PK_TEXT, "Drive B: CP/M letter (empty = seen while running)", "Mechanika B: písmeno v CP/M (prázdné = zjistí se za chodu)", "null", nullptr, 0, 0 },
    { "pmd32sd.cpm.2", PK_TEXT, "Drive C: CP/M letter (empty = seen while running)", "Mechanika C: písmeno v CP/M (prázdné = zjistí se za chodu)", "null", nullptr, 0, 0 },
    { "pmd32sd.cpm.3", PK_TEXT, "Drive D: CP/M letter (empty = seen while running)", "Mechanika D: písmeno v CP/M (prázdné = zjistí se za chodu)", "null", nullptr, 0, 0 },
    { "pmd32sd.card_locked", PK_BOOL, "SD card write protected", "SD karta chráněná proti zápisu", "false",
      nullptr, 0, 0 },
    { "pmd32sd.startup_ms", PK_INT, "Start-up delay (ms, real 3000)", "Prodleva po zapnutí (ms, skutečně 3000)",
      "0", nullptr, 0, 10000 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// ---------------------------------------------------------------- SAPI-1.5 (PKT-1, RSW-1, DBS-1A)

static const ParamOption k_pkt1_roms[] = {
    { "\"roms/pkt-1/PKT-1_MON-V100690_EPROM.bin\"", "MON-V100690 (EPROM dump, ZPS-6/7: DBS-1, MFC-1A, Consul 262.3)",
      "MON-V100690 (dump EPROM, ZPS-6/7: DBS-1, MFC-1A, Consul 262.3)" },
    { "\"roms/pkt-1/PKT-1_ROMBIOS_MELSOFT.bin\"", "MON-V100690 revised (Melsoft ROMBIOS.MAC: printer status, timeout)",
      "MON-V100690 opravený (Melsoft ROMBIOS.MAC: stav tiskárny, časový limit)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_pkt1_eprom[] = {
    { "\"2716\"", "2716 (2 KB, standard)", "2716 (2 KB, standard)" },
    { "\"2732\"", "2732 (4 KB)", "2732 (4 KB)" },
    { "\"2764\"", "2764 (8 KB)", "2764 (8 KB)" },
    { "\"27128\"", "27128 (16 KB)", "27128 (16 KB)" },
    { "\"27256\"", "27256 (32 KB)", "27256 (32 KB)" },
    { "\"27512\"", "27512 (64 KB)", "27512 (64 KB)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_pkt1_dd8[] = {
    { "\"roms/pkt-1/PKT-1_DD8_74S287.bin\"", "Tesla 6XN 902 54: EPROM 0000-07FF while BOOT",
      "Tesla 6XN 902 54: EPROM 0000-07FF při BOOT" },
    { "\"roms/pkt-1/PKT-1_DD8_8k_replika.bin\"", "replica: EPROM 0000-1FFF while BOOT (8 KB)",
      "replika: EPROM 0000-1FFF při BOOT (8 KB)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_pkt1_mhz[] = {
    { "2", "2 MHz (XT5 24-25, standard)", "2 MHz (XT5 24-25, standard)" },
    { "4", "4 MHz (XT5 23-24)", "4 MHz (XT5 23-24)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_pkt1_wait[] = {
    { "\"rdy\"", "only from RDY (XT4 20-21)", "jen z RDY (XT4 20-21)" },
    { "\"always\"", "one in every machine cycle (XT4 21-22)", "jeden v každém strojovém cyklu (XT4 21-22)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_pkt1_boot[] = {
    { "\"register\"", "BOOT from the register 0Eh (XT3 6-7)", "BOOT z registru 0Eh (XT3 6-7)" },
    { "\"off\"", "EPROM always off (XT3 5-6)", "EPROM trvale vypnutá (XT3 5-6)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_pkt1_busy[] = {
    { "\"pc3\"", "PC3 (fixed board, the ROM BIOS expects it)", "PC3 (opravená deska, čeká to ROM BIOS)" },
    { "\"pc6\"", "PC6 /ACK (original Tesla wiring: the printer does not work)",
      "PC6 /ACK (původní zapojení Tesla: tiskárna nefunguje)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_pkt1[] = {
    { "rom", PK_FILE, "EPROM (DD9)", "EPROM (DD9)", "\"roms/pkt-1/PKT-1_MON-V100690_EPROM.bin\"", k_pkt1_roms, 0, 0 },
    { "eprom", PK_CHOICE, "XT2: EPROM type", "XT2: typ EPROM", "\"2716\"", k_pkt1_eprom, 0, 0 },
    { "decoder", PK_FILE, "PROM DD8: memory decoder", "PROM DD8: dekodér paměti", "\"roms/pkt-1/PKT-1_DD8_74S287.bin\"",
      k_pkt1_dd8, 0, 0 },
    { "io_decoder", PK_FILE, "PROM DD12: I/O decoder", "PROM DD12: dekodér I/O", "\"roms/pkt-1/PKT-1_DD12_74S287.bin\"",
      k_none, 0, 0 },
    { "cpu_mhz", PK_CHOICE, "XT5: CPU clock (the bus F2 is 2 MHz)", "XT5: takt CPU (sběrnice F2 je 2 MHz)", "2", k_pkt1_mhz,
      0, 0 },
    { "wait", PK_CHOICE, "XT4: wait states", "XT4: čekací stavy", "\"rdy\"", k_pkt1_wait, 0, 0 },
    { "boot", PK_CHOICE, "XT3: EPROM at reset", "XT3: EPROM po resetu", "\"register\"", k_pkt1_boot, 0, 0 },
    { "map1", PK_BOOL, "XT1 3-4: MAP1 to the bus", "XT1 3-4: MAP1 na sběrnici", "true", nullptr, 0, 0 },
    { "map2", PK_BOOL, "XT1 1-2: MAP2 to the bus", "XT1 1-2: MAP2 na sběrnici", "true", nullptr, 0, 0 },
    { "keyboard", PK_KEYBOARD, "Keyboard (connector XP2, 8255 port B)", "Klávesnice (konektor XP2, 8255 port B)", "null",
      nullptr, 0, 0 },
    { "keyboard.strict", PK_BOOL, "Keyboard: true to the hardware (keys may be lost)",
      "Klávesnice: věrně jako hardware (klávesy se mohou ztratit)", "false", nullptr, 0, 0 },
    { "keyboard.beeper", PK_BOOL, "Keyboard: beeper (/ZVUK = 8255 PC5)", "Klávesnice: bzučák (/ZVUK = 8255 PC5)", "true",
      nullptr, 0, 0 },
    { "printer", PK_SECTION, "XP3: printer (Centronics: data 8255 PA, /STROBE PC7)",
      "XP3: tiskárna (Centronics: data 8255 PA, /STROBE PC7)", "false", nullptr, 0, 0 },
    PRINTER_PARAMS
    { "printer.busy", PK_CHOICE, "Printer: BUSY wired to", "Tiskárna: BUSY zapojený na", "\"pc3\"", k_pkt1_busy, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

static const ParamOption k_rsw1_src[] = {
    { "\"L\"", "L", "L" },
    { "\"H\"", "H", "H" },
    { "\"MAP1\"", "MAP1", "MAP1" },
    { "\"MAP2\"", "MAP2", "MAP2" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_rsw1[] = {
    { "decoder", PK_FILE, "PROM DD1: RAM decoder", "PROM DD1: dekodér RAM", "\"roms/rsw-1/RSW-1_DD1_74S571.bin\"", k_none,
      0, 0 },
    { "xt1_a6", PK_CHOICE, "XT1: DD1 input A6", "XT1: vstup DD1 A6", "\"L\"", k_rsw1_src, 0, 0 },
    { "xt1_a7", PK_CHOICE, "XT1: DD1 input A7", "XT1: vstup DD1 A7", "\"H\"", k_rsw1_src, 0, 0 },
    { "xt1_a8", PK_CHOICE, "XT1: DD1 input A8 (MAP2: B000-BFFF free for DBS-1 while MAP2 = 1)",
      "XT1: vstup DD1 A8 (MAP2: B000-BFFF volné pro DBS-1 při MAP2 = 1)", "\"MAP2\"", k_rsw1_src, 0, 0 },
    { "io_decoder", PK_FILE, "PROM DD2: I/O decoder (18h-1Fh)", "PROM DD2: dekodér I/O (18h-1Fh)",
      "\"roms/rsw-1/RSW-1_DD2_74S287.bin\"", k_none, 0, 0 },
    { "int0", PK_BOOL, "XT3 13-14: interrupt on INT0", "XT3 13-14: přerušení na INT0", "true", nullptr, 0, 0 },
    { "watchdog_reset", PK_BOOL, "XT2 11-12: watchdog resets the system (/RTL)", "XT2 11-12: watchdog resetuje systém (/RTL)",
      "false", nullptr, 0, 0 },
    { "connected", PK_BOOL, "Terminal connected at start (DCD)", "Terminál připojen při startu (DCD)", "true", nullptr,
      0, 0 },
    { "cts", PK_BOOL, "Terminal ready at start (CTS/DSR)", "Terminál připraven při startu (CTS/DSR)", "true", nullptr,
      0, 0 },
    { "serial.pacing", PK_BOOL, "PC side: no overrun (pacing)", "Strana PC: bez přetečení", "true", nullptr, 0, 0 },
    { "serial.tcp", PK_SECTION, "PC side: TCP server", "Strana PC: TCP server", "false", nullptr, 0, 0 },
    { "serial.tcp.port", PK_INT, "TCP port", "TCP port", "2323", nullptr, 1, 65535 },
    { "serial.tcp.bind", PK_TEXT, "Address (127.0.0.1 = this PC only)", "Adresa (127.0.0.1 = jen tento PC)",
      "\"127.0.0.1\"", nullptr, 0, 0 },
    { "serial.tcp.protocol", PK_CHOICE, "Protocol", "Protokol", "\"telnet\"", k_tcp_proto, 0, 0 },
    { "serial.tcp.dcd_follows", PK_BOOL, "DCD follows the client connection", "DCD podle připojení klienta", "true",
      nullptr, 0, 0 },
    { "serial.com", PK_SECTION, "PC side: COM port", "Strana PC: COM port", "false", nullptr, 0, 0 },
    { "serial.com.port", PK_TEXT, "Port (COM5, /dev/ttyUSB0)", "Port (COM5, /dev/ttyUSB0)", "\"COM1\"", nullptr, 0, 0 },
    { "serial.com.baud", PK_INT, "Baud rate", "Rychlost", "9600", nullptr, 50, 921600 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

static const ParamOption k_dbs1a_fonts[] = {
    { "\"roms/dbs-1a/DBS-1A_CharGen_59.bin\"", "Tesla EPROM \"59\" (dump EC1045, 2026)", "EPROM Tesla „59“ (dump EC1045, 2026)" },
    { "\"roms/dbs-1a/DBS-1A_CharGen_nahrada.bin\"", "substitute from the AND-1Z font (made before the dump was found)",
      "náhrada z fontu AND-1Z (z doby před nalezením dumpu)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dbs1a_xt5[] = {
    { "\"2732\"", "2732, A11 = RA3: 256 characters up to 16 lines (27-28, standard)",
      "2732, A11 = RA3: 256 znaků až 16 řádků (27-28, standard)" },
    { "\"2716\"", "2716: 256 characters, 8 lines (28-33)", "2716: 256 znaků, 8 řádků (28-33)" },
    { "\"2732_attr\"", "2732, A11 = attribute bit 3: 512 characters, 8 lines (28-29)",
      "2732, A11 = bit 3 atributu: 512 znaků, 8 řádků (28-29)" },
    { "\"2732_ma12\"", "2732, A11 = MA12: two sets by R12 bit 4, 8 lines (28-31)",
      "2732, A11 = MA12: dvě sady podle R12 bit 4, 8 řádků (28-31)" },
    { "\"2764_attr\"", "2764, A11 = RA3, A12 = attribute bit 3 (27-28, 29-30)",
      "2764, A11 = RA3, A12 = bit 3 atributu (27-28, 29-30)" },
    { "\"2764_ma12\"", "2764, A11 = RA3, A12 = MA12 (27-28, 30-31)", "2764, A11 = RA3, A12 = MA12 (27-28, 30-31)" },
    { "\"2764_ma12_attr\"", "2764, A11 = MA12, A12 = attribute bit 3 (28-31, 29-30)",
      "2764, A11 = MA12, A12 = bit 3 atributu (28-31, 29-30)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dbs1a_port[] = {
    { "\"E0h\"", "E0h/E1h (XT2 10-42, XT3 12-17, standard)", "E0h/E1h (XT2 10-42, XT3 12-17, standard)" },
    { "\"E2h\"", "E2h/E3h (XT2 10-42, XT3 12-15)", "E2h/E3h (XT2 10-42, XT3 12-15)" },
    { "\"E4h\"", "E4h/E5h (XT2 10-42, XT3 12-13)", "E4h/E5h (XT2 10-42, XT3 12-13)" },
    { "\"E6h\"", "E6h/E7h (XT2 10-42, XT3 11-12)", "E6h/E7h (XT2 10-42, XT3 11-12)" },
    { "\"E8h\"", "E8h/E9h (XT2 8-10, XT3 12-17 by the PROM table)", "E8h/E9h (XT2 8-10, XT3 12-17 podle tabulky PROM)" },
    { "\"EAh\"", "EAh/EBh (XT2 8-10, XT3 12-15)", "EAh/EBh (XT2 8-10, XT3 12-15)" },
    { "\"ECh\"", "ECh/EDh (XT2 8-10, XT3 12-13)", "ECh/EDh (XT2 8-10, XT3 12-13)" },
    { "\"EEh\"", "EEh/EFh (XT2 8-10, XT3 11-12)", "EEh/EFh (XT2 8-10, XT3 11-12)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dbs1a_window[] = {
    { "\"B000h\"", "B000-BFFF with MAP1 = L, MAP2 = H (XT1 6-7, XT4 19-20, standard)",
      "B000-BFFF při MAP1 = L, MAP2 = H (XT1 6-7, XT4 19-20, standard)" },
    { "\"A000h\"", "A000-AFFF (XT1 6-7, XT4 20-21)", "A000-AFFF (XT1 6-7, XT4 20-21)" },
    { "\"9000h\"", "9000-9FFF (XT1 6-7, XT4 20-22)", "9000-9FFF (XT1 6-7, XT4 20-22)" },
    { "\"8000h\"", "8000-8FFF (XT1 6-7, XT4 20-23)", "8000-8FFF (XT1 6-7, XT4 20-23)" },
    { "\"F000h\"", "F000-FFFF, MAP ignored (XT1 5-6, XT4 19-20 by the PROM table)",
      "F000-FFFF, MAP nerozhoduje (XT1 5-6, XT4 19-20 podle tabulky PROM)" },
    { "\"E000h\"", "E000-EFFF, MAP ignored (XT1 5-6, XT4 20-21)", "E000-EFFF, MAP nerozhoduje (XT1 5-6, XT4 20-21)" },
    { "\"D000h\"", "D000-DFFF, MAP ignored (XT1 5-6, XT4 20-22)", "D000-DFFF, MAP nerozhoduje (XT1 5-6, XT4 20-22)" },
    { "\"C000h\"", "C000-CFFF, MAP ignored (XT1 5-6, XT4 20-23)", "C000-CFFF, MAP nerozhoduje (XT1 5-6, XT4 20-23)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dbs1a_dots[] = {
    { "8", "8 dots: 64 columns (XT7 38-39)", "8 bodů: 64 sloupců (XT7 38-39)" },
    { "6", "6 dots: 80 columns (XT7 37-38)", "6 bodů: 80 sloupců (XT7 37-38)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dbs1a_rg[] = {
    { "\"manual\"", "manual: bit 2 = R, bit 1 = G", "návod: bit 2 = R, bit 1 = G" },
    { "\"schematic\"", "schematic: bit 1 = R, bit 2 = G", "schéma: bit 1 = R, bit 2 = G" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dbs1a_out[] = {
    { "\"rgb\"", "RGB + intensity (monitor GRM 4423)", "RGB + jas (monitor GRM 4423)" },
    { "\"composite\"", "composite, grey from RGB (XT6 35-36)", "kompozitní, šedá z RGB (XT6 35-36)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_dbs1a[] = {
    { "font", PK_FILE, "Character generator EPROM (DD15)", "EPROM znakového generátoru (DD15)",
      "\"roms/dbs-1a/DBS-1A_CharGen_59.bin\"", k_dbs1a_fonts, 0, 0 },
    { "xt5", PK_CHOICE, "XT5: character generator", "XT5: znakový generátor", "\"2732\"", k_dbs1a_xt5, 0, 0 },
    { "port", PK_CHOICE, "XT2/XT3: 6845 ports", "XT2/XT3: porty 6845", "\"E0h\"", k_dbs1a_port, 0xE0, 0xEE },
    { "window", PK_CHOICE, "XT1/XT4: video RAM window (write only)", "XT1/XT4: okno video RAM (jen zápis)", "\"B000h\"",
      k_dbs1a_window, 0x8000, 0xF000 },
    { "map1", PK_BOOL, "XT1 1-2: MAP1 decoded (else ignored)", "XT1 1-2: MAP1 se dekóduje (jinak nerozhoduje)", "true",
      nullptr, 0, 0 },
    { "map2", PK_BOOL, "XT1 3-4: MAP2 decoded (else ignored)", "XT1 3-4: MAP2 se dekóduje (jinak nerozhoduje)", "true",
      nullptr, 0, 0 },
    { "dots", PK_CHOICE, "XT7: dots per character", "XT7: bodů na znak", "8", k_dbs1a_dots, 0, 0 },
    { "rgb_order", PK_CHOICE, "Attribute: order of R and G (manual / schematic differ)",
      "Atribut: pořadí R a G (návod a schéma se liší)", "\"manual\"", k_dbs1a_rg, 0, 0 },
    { "output", PK_CHOICE, "Output (monitor)", "Výstup (monitor)", "\"rgb\"", k_dbs1a_out, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

static const ParamOption k_crm1_base[] = {
    { "\"0000h\"", "0000-1FFF (jumper 1-2)", "0000-1FFF (propojka 1-2)" },
    { "\"2000h\"", "2000-3FFF (jumper 1-3)", "2000-3FFF (propojka 1-3)" },
    { "\"4000h\"", "4000-5FFF (jumper 1-4)", "4000-5FFF (propojka 1-4)" },
    { "\"6000h\"", "6000-7FFF (jumper 1-5)", "6000-7FFF (propojka 1-5)" },
    { "\"8000h\"", "8000-9FFF (jumper 1-6)", "8000-9FFF (propojka 1-6)" },
    { "\"A000h\"", "A000-BFFF (jumper 1-7)", "A000-BFFF (propojka 1-7)" },
    { "\"C000h\"", "C000-DFFF (jumper 1-8)", "C000-DFFF (propojka 1-8)" },
    { "\"E000h\"", "E000-FFFF (jumper 1-9), standard", "E000-FFFF (propojka 1-9), standard" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_crm1_kb[] = {
    { "\"0000h\"", "+0000h (jumper 14+0)", "+0000h (propojka 14+0)" },
    { "\"0400h\"", "+0400h (jumper 14+1)", "+0400h (propojka 14+1)" },
    { "\"0800h\"", "+0800h (jumper 14+2)", "+0800h (propojka 14+2)" },
    { "\"0C00h\"", "+0C00h (jumper 14+3)", "+0C00h (propojka 14+3)" },
    { "\"1000h\"", "+1000h (jumper 14+4)", "+1000h (propojka 14+4)" },
    { "\"1400h\"", "+1400h (jumper 14+5)", "+1400h (propojka 14+5)" },
    { "\"1800h\"", "+1800h (jumper 14+6)", "+1800h (propojka 14+6)" },
    { "\"1C00h\"", "+1C00h (jumper 14+7)", "+1C00h (propojka 14+7)" },
    { "\"none\"", "not fitted (no MHB 1902 chips)", "neosazeno (bez obvodů MHB 1902)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_crm1[] = {
    { "base", PK_CHOICE, "Jumpers 1-2..1-9: the 8 KB block", "Propojky 1-2..1-9: blok 8 KB", "\"E000h\"", k_crm1_base, 0, 0 },
    { "kb1", PK_CHOICE, "1st kilobyte (CS1): offset in the block", "1. kilobajt (CS1): posun v bloku", "\"0000h\"", k_crm1_kb, 0, 0 },
    { "kb2", PK_CHOICE, "2nd kilobyte (CS2): offset in the block", "2. kilobajt (CS2): posun v bloku", "\"0400h\"", k_crm1_kb, 0, 0 },
    { "kb3", PK_CHOICE, "3rd kilobyte (CS3): offset in the block", "3. kilobajt (CS3): posun v bloku", "\"0800h\"", k_crm1_kb, 0, 0 },
    { "kb4", PK_CHOICE, "4th kilobyte (CS4): offset in the block", "4. kilobajt (CS4): posun v bloku", "\"0C00h\"", k_crm1_kb, 0, 0 },
    { "write_protect", PK_BOOL, "SA1: no writes into the 1st kilobyte (does not work on the boards as made)",
      "SA1: zákaz zápisu do 1. kilobajtu (na deskách z výroby nefunguje)", "false", nullptr, 0, 0 },
    { "battery", PK_BOOL, "Backup batteries (the content survives power off)", "Záložní baterie (obsah přežije vypnutí)", "true",
      nullptr, 0, 0 },
    { "file", PK_FILE, "Content kept by the batteries", "Obsah držený bateriemi", "\"work/crm-1/crm.bin\"", k_none, 0, 0 },
    { "rdy_ns", PK_INT, "DD6: RDY low after each access (ns; 0 = no wait states, a deviation)",
      "DD6: RDY v L po každém přístupu (ns; 0 = bez čekání, odchylka)", "590", nullptr, 0, 10000 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

static const ParamOption k_rtc1_base[] = {
    { "\"00h\"", "00-0Fh (SA1, jumper 2-3)", "00-0Fh (SA1, propojka 2-3)" },
    { "\"10h\"", "10-1Fh (SA2, jumper 2-3)", "10-1Fh (SA2, propojka 2-3)" },
    { "\"20h\"", "20-2Fh (SA3, jumper 2-3)", "20-2Fh (SA3, propojka 2-3)" },
    { "\"30h\"", "30-3Fh (SA4, jumper 2-3)", "30-3Fh (SA4, propojka 2-3)" },
    { "\"40h\"", "40-4Fh (SA5, jumper 2-3)", "40-4Fh (SA5, propojka 2-3)" },
    { "\"50h\"", "50-5Fh (SA6, jumper 2-3)", "50-5Fh (SA6, propojka 2-3)" },
    { "\"60h\"", "60-6Fh (SA7, jumper 2-3)", "60-6Fh (SA7, propojka 2-3)" },
    { "\"70h\"", "70-7Fh (SA8, jumper 2-3)", "70-7Fh (SA8, propojka 2-3)" },
    { "\"80h\"", "80-8Fh (SA1, jumper 1-2 + 3-4)", "80-8Fh (SA1, propojka 1-2 + 3-4)" },
    { "\"90h\"", "90-9Fh (SA2, jumper 1-2 + 3-4)", "90-9Fh (SA2, propojka 1-2 + 3-4)" },
    { "\"A0h\"", "A0-AFh (SA3, jumper 1-2 + 3-4)", "A0-AFh (SA3, propojka 1-2 + 3-4)" },
    { "\"B0h\"", "B0-BFh (SA4, jumper 1-2 + 3-4)", "B0-BFh (SA4, propojka 1-2 + 3-4)" },
    { "\"C0h\"", "C0-CFh (SA5, jumper 1-2 + 3-4)", "C0-CFh (SA5, propojka 1-2 + 3-4)" },
    { "\"D0h\"", "D0-DFh (SA6, jumper 1-2 + 3-4)", "D0-DFh (SA6, propojka 1-2 + 3-4)" },
    { "\"E0h\"", "E0-EFh (SA7, jumper 1-2 + 3-4)", "E0-EFh (SA7, propojka 1-2 + 3-4)" },
    { "\"F0h\"", "F0-FFh (SA8, jumper 1-2 + 3-4)", "F0-FFh (SA8, propojka 1-2 + 3-4)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_rtc1_mode[] = {
    { "\"async\"", "5-6, 9-11: interrupt while OUT1 of DD6 is high, PC3 = bit 19 (factory)",
      "5-6, 9-11: přerušení, dokud je OUT1 DD6 v H, PC3 = bit 19 (z výroby)" },
    { "\"realtime\"", "5-7, 6-8, 10-11: OUT1 = /STB A of the 8255 (mode 1), interrupt = INTR A, 19 bits",
      "5-7, 6-8, 10-11: OUT1 = /STB A 8255 (mód 1), přerušení = INTR A, 19 bitů" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_rtc1_time[] = {
    { "\"pc\"", "the PC clock (first power on: the time since Monday 0:00)",
      "hodiny PC (po prvním zapnutí čas od pondělní půlnoci)" },
    { "\"emulated\"", "the emulated time (the same in every run)", "čas emulace (v každém běhu stejný)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_adp1_base[] = {
    { "\"00h\"", "00h (SA 1111)", "00h (SA 1111)" },
    { "\"10h\"", "10h (SA 1110)", "10h (SA 1110)" },
    { "\"20h\"", "20h (SA 1101)", "20h (SA 1101)" },
    { "\"30h\"", "30h (SA 1100)", "30h (SA 1100)" },
    { "\"40h\"", "40h (SA 1011)", "40h (SA 1011)" },
    { "\"50h\"", "50h (SA 1010)", "50h (SA 1010)" },
    { "\"60h\"", "60h (SA 1001)", "60h (SA 1001)" },
    { "\"70h\"", "70h (SA 1000)", "70h (SA 1000)" },
    { "\"80h\"", "80h (SA 0111)", "80h (SA 0111)" },
    { "\"90h\"", "90h (SA 0110)", "90h (SA 0110)" },
    { "\"A0h\"", "A0h (SA 0101)", "A0h (SA 0101)" },
    { "\"B0h\"", "B0h (SA 0100)", "B0h (SA 0100)" },
    { "\"C0h\"", "C0h (SA 0011)", "C0h (SA 0011)" },
    { "\"D0h\"", "D0h (SA 0010)", "D0h (SA 0010)" },
    { "\"E0h\"", "E0h (SA 0001)", "E0h (SA 0001)" },
    { "\"F0h\"", "F0h (SA 0000)", "F0h (SA 0000)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dmt1_base[] = {
    { "\"00h\"", "00h (SA 1111)", "00h (SA 1111)" },
    { "\"10h\"", "10h (SA 1110)", "10h (SA 1110)" },
    { "\"20h\"", "20h (SA 1101)", "20h (SA 1101)" },
    { "\"30h\"", "30h (SA 1100)", "30h (SA 1100)" },
    { "\"40h\"", "40h (SA 1011)", "40h (SA 1011)" },
    { "\"50h\"", "50h (SA 1010)", "50h (SA 1010)" },
    { "\"60h\"", "60h (SA 1001)", "60h (SA 1001)" },
    { "\"70h\"", "70h (SA 1000)", "70h (SA 1000)" },
    { "\"80h\"", "80h (SA 0111)", "80h (SA 0111)" },
    { "\"90h\"", "90h (SA 0110)", "90h (SA 0110)" },
    { "\"A0h\"", "A0h (SA 0101)", "A0h (SA 0101)" },
    { "\"B0h\"", "B0h (SA 0100)", "B0h (SA 0100)" },
    { "\"C0h\"", "C0h (SA 0011)", "C0h (SA 0011)" },
    { "\"D0h\"", "D0h (SA 0010)", "D0h (SA 0010)" },
    { "\"E0h\"", "E0h (SA 0001)", "E0h (SA 0001)" },
    { "\"F0h\"", "F0h (SA 0000)", "F0h (SA 0000)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_drv1a_port[] = {
    { "\"40h\"", "40h", "40h" },
    { "\"41h\"", "41h", "41h" },
    { "\"42h\"", "42h", "42h" },
    { "\"43h\"", "43h", "43h" },
    { "\"44h\"", "44h", "44h" },
    { "\"45h\"", "45h", "45h" },
    { "\"46h\"", "46h", "46h" },
    { "\"47h\"", "47h", "47h" },
    { "\"50h\"", "50h", "50h" },
    { "\"51h\"", "51h", "51h" },
    { "\"52h\"", "52h", "52h" },
    { "\"53h\"", "53h", "53h" },
    { "\"54h\"", "54h", "54h" },
    { "\"55h\"", "55h", "55h" },
    { "\"56h\"", "56h", "56h" },
    { "\"57h\"", "57h", "57h" },
    { "\"60h\"", "60h", "60h" },
    { "\"61h\"", "61h", "61h" },
    { "\"62h\"", "62h", "62h" },
    { "\"63h\"", "63h", "63h" },
    { "\"64h\"", "64h", "64h" },
    { "\"65h\"", "65h", "65h" },
    { "\"66h\"", "66h", "66h" },
    { "\"67h\"", "67h", "67h" },
    { "\"70h\"", "70h", "70h" },
    { "\"71h\"", "71h", "71h" },
    { "\"72h\"", "72h", "72h" },
    { "\"73h\"", "73h", "73h" },
    { "\"74h\"", "74h", "74h" },
    { "\"75h\"", "75h", "75h" },
    { "\"76h\"", "76h", "76h" },
    { "\"77h\"", "77h", "77h" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_adp1_wiring[] = {
    { "\"none\"", "inputs not connected", "vstupy nezapojené" },
    { "\"aps5\"", "APS-5: S1 = DMT-1 U1, S2 = DMT-1 U2, S5 = battery, S6 = 0 V, S7 = +9.99 V (KAL)",
      "APS-5: S1 = DMT-1 U1, S2 = DMT-1 U2, S5 = baterie, S6 = 0 V, S7 = +9,99 V (KAL)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_adp1[] = {
    { "base", PK_CHOICE, "SA1-SA4: base address (ports +0, +1)", "SA1-SA4: bázová adresa (porty +0, +1)", "\"30h\"",
      k_adp1_base, 0, 0 },
    { "wiring", PK_CHOICE, "Cables to the inputs S1-S16 (X3)", "Kabely na vstupy S1-S16 (X3)", "\"none\"", k_adp1_wiring, 0, 0 },
    { "ref_mv", PK_INT, "RP1: reference +9.99 V (mV)", "RP1: reference +9,99 V (mV)", "9990", nullptr, 9000, 11000 },
    { "conversion_ms", PK_INT, "One conversion of the C 520 D (ms)", "Jeden převod C 520 D (ms)", "20", nullptr, 1, 1000 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};
static const CardParam k_dmt1[] = {
    { "base", PK_CHOICE, "SA1-SA4: address (A7-A4, 16 mirrors)", "SA1-SA4: adresa (A7-A4, 16 zrcadel)", "\"80h\"",
      k_dmt1_base, 0, 0 },
    { "break_detect", PK_BOOL, "Jumper 5-6: an open input shows as about -15 mV", "Propojka 5-6: rozpojený vstup jako asi -15 mV",
      "true", nullptr, 0, 0 },
    { "int1", PK_BOOL, "Jumper 7-8: interrupt INT1 600 ms after bit 5", "Propojka 7-8: přerušení INT1 600 ms po bitu 5",
      "false", nullptr, 0, 0 },
    { "supply_15v", PK_BOOL, "+-15 V on XC3 (1-2, 3-4 open): output up to +-10 V", "+-15 V na XC3 (1-2, 3-4 rozpojeno): výstup až +-10 V",
      "false", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};
// DSE-1 EPROM simulator and programmer: S1 positions of S1/1 (A6), S1/2 (A7), S1/3 (A5), S1/4 (A4), 1 = closed = bit 0
static const ParamOption k_dse1_base[] = {
    { "\"00h\"", "00h (S1/1-4: 1 1 1 1)", "00h (S1/1-4: 1 1 1 1)" },
    { "\"10h\"", "10h (S1/1-4: 1 1 1 0)", "10h (S1/1-4: 1 1 1 0)" },
    { "\"20h\"", "20h (S1/1-4: 1 1 0 1)", "20h (S1/1-4: 1 1 0 1)" },
    { "\"30h\"", "30h (S1/1-4: 1 1 0 0)", "30h (S1/1-4: 1 1 0 0)" },
    { "\"40h\"", "40h (S1/1-4: 0 1 1 1)", "40h (S1/1-4: 0 1 1 1)" },
    { "\"50h\"", "50h (S1/1-4: 0 1 1 0)", "50h (S1/1-4: 0 1 1 0)" },
    { "\"60h\"", "60h (S1/1-4: 0 1 0 1)", "60h (S1/1-4: 0 1 0 1)" },
    { "\"70h\"", "70h (S1/1-4: 0 1 0 0)", "70h (S1/1-4: 0 1 0 0)" },
    { "\"80h\"", "80h (S1/1-4: 1 0 1 1)", "80h (S1/1-4: 1 0 1 1)" },
    { "\"90h\"", "90h (S1/1-4: 1 0 1 0)", "90h (S1/1-4: 1 0 1 0)" },
    { "\"A0h\"", "A0h (S1/1-4: 1 0 0 1)", "A0h (S1/1-4: 1 0 0 1)" },
    { "\"B0h\"", "B0h (S1/1-4: 1 0 0 0)", "B0h (S1/1-4: 1 0 0 0)" },
    { "\"C0h\"", "C0h (S1/1-4: 0 0 1 1)", "C0h (S1/1-4: 0 0 1 1)" },
    { "\"D0h\"", "D0h (S1/1-4: 0 0 1 0)", "D0h (S1/1-4: 0 0 1 0)" },
    { "\"E0h\"", "E0h (S1/1-4: 0 0 0 1)", "E0h (S1/1-4: 0 0 0 1)" },
    { "\"F0h\"", "F0h (S1/1-4: 0 0 0 0, as supplied)", "F0h (S1/1-4: 0 0 0 0, z výroby)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dse1_vpp[] = {
    { "\"pin50\"", "DSE-1B: +30 V from pin 50 (terminal N of ZDR-1A)", "DSE-1B: +30 V z kolíku 50 (svorka N zdroje ZDR-1A)" },
    { "\"converter\"", "DSE-1A: converter from +12 V", "DSE-1A: měnič z +12 V" },
    { "\"none\"", "DSE-1B without +30 V (Vpp always 0 V: a 2716 does not even read)",
      "DSE-1B bez +30 V (Vpp vždy 0 V: 2716 nejde ani číst)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dse1_cable[] = {
    { "\"2716\"", "for 2716, as supplied (18 = PC6, 20 = /PC7, 21 = Vpp)", "pro 2716, z výroby (18 = PC6, 20 = /PC7, 21 = Vpp)" },
    { "\"2732\"", "for 2732, modified after DSE.DOC (21 = PA11, 18 = /CE, 20 = OE/Vpp)",
      "pro 2732, upravený podle DSE.DOC (21 = PA11, 18 = /CE, 20 = OE/Vpp)" },
    { "\"dual\"", "two sockets 2716 + 2732 (replica; the EPROM picks its socket)",
      "dvě patice 2716 + 2732 (replika; EPROM si vybere svou patici)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dse1_eproms[] = {
    { "\"media/dse-1/prazdna_2716.bin\"", "Blank 2716 (2 KB, all FFh)", "Prázdná 2716 (2 KB, celá FFh)" },
    { "\"media/dse-1/prazdna_2732.bin\"", "Blank 2732 (4 KB, all FFh)", "Prázdná 2732 (4 KB, celá FFh)" },
    { "\"\"", "socket empty", "patice prázdná" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_dse1[] = {
    { "base", PK_CHOICE, "S1: ports (base+0..3 8255, base+4..7 register)", "S1: porty (základ+0..3 8255, základ+4..7 registr)",
      "\"F0h\"", k_dse1_base, 0, 0 },
    { "jp1_map", PK_BOOL, "JP1: bit D5 drives MAP1 (pin 21 of X1)", "JP1: bit D5 budí MAP1 (kolík 21 X1)", "true", nullptr, 0, 0 },
    { "vpp", PK_CHOICE, "Programming voltage Vpp (variant)", "Programovací napětí Vpp (provedení)", "\"pin50\"", k_dse1_vpp, 0, 0 },
    { "x2_cable", PK_CHOICE, "Programming cable X2", "Programovací kabel X2", "\"2716\"", k_dse1_cable, 0, 0 },
    { "eprom", PK_FILE,
      "EPROM in the socket of X2 (.bin: 2048 B = 2716, 4096 B = 2732; a pulse of 1 ms or longer programs, an assumption)",
      "EPROM v patici X2 (.bin: 2048 B = 2716, 4096 B = 2732; programuje pulz od 1 ms, předpoklad)",
      "\"media/dse-1/prazdna_2716.bin\"", k_dse1_eproms, 0, 0 },
    { "working_copy", PK_BOOL, "Work on copies in work/", "Pracovat s kopiemi ve work/", "true", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};
static const ParamOption k_drv1a_load[] = {
    { "\"none\"", "nothing connected", "nic nepřipojeno" },
    { "\"plant\"", "heaters of the furnaces (APS-5, plant of the machine)", "topení pecí (APS-5, technologie sestavy)" },
    { nullptr, nullptr, nullptr },
};
// DPS-1 instrument bus IMS-2 (fixed ports 30h-37h); the cable of X2 goes to the plant "IMS-2" of the machine
static const ParamOption k_dps1_prom[] = {
    { "\"roms/dps-1/DPS-1_D3D_74S571.bin\"", "PROM D3D of the manual (codes 00-08h; with ATN 00-06h -> 9-15)",
      "PROM D3D z návodu (kódy 00-08h; s ATN 00-06h -> 9-15)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_dps1[] = {
    { "prom", PK_FILE, "D3D: PROM 74S571 of the multi-line messages (512 x 4)",
      "D3D: PROM 74S571 vícevodičových zpráv (512 x 4)", "\"roms/dps-1/DPS-1_D3D_74S571.bin\"", k_dps1_prom, 0, 0 },
    { "jp1", PK_BOOL, "JP1: IFC from the RESET button (/RTL)", "JP1: IFC od tlačítka RESET (/RTL)", "true", nullptr, 0, 0 },
    { "jp2", PK_BOOL, "JP2: answer of a parallel poll on DIO1", "JP2: odpověď na paralelní hlášení na DIO1", "false",
      nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};
static const CardParam k_drv1a[] = {
    { "port", PK_CHOICE, "Jumpers: the port", "Propojky: port", "\"77h\"", k_drv1a_port, 0, 0 },
    { "load", PK_CHOICE, "Contacts S1-S8 switch", "Kontakty S1-S8 spínají", "\"none\"", k_drv1a_load, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};
static const ParamOption k_rtc1_xc2[] = {
    { "\"none\"", "nothing (DD5 without clocks)", "nic (DD5 bez hodin)" },
    { "\"aps5\"", "APS-5: OUT2 of DD6 clocks DD5, OUT0/OUT1 to JPR-1 P2-IN5/4",
      "APS-5: OUT2 DD6 taktuje DD5, OUT0/OUT1 do JPR-1 P2-IN5/4" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_rtc1[] = {
    { "base", PK_CHOICE, "SA1-SA8 and the A7 jumper: the 16 ports", "SA1-SA8 a propojka A7: 16 portů", "\"A0h\"",
      k_rtc1_base, 0, 0 },
    { "mode", PK_CHOICE, "Jumpers 5-11: interrupt mode", "Propojky 5-11: režim přerušení", "\"async\"", k_rtc1_mode, 0, 0 },
    { "int", PK_CHOICE, "Cable from XC2/14 (/INT) to the computer", "Kabel z XC2/14 (/INT) do počítače", "\"none\"",
      k_rtc_int, 0, 0 },
    { "time", PK_CHOICE, "The real time counter follows", "Čítač reálného času běží podle", "\"pc\"", k_rtc1_time, 0, 0 },
    { "battery", PK_BOOL, "Backup cells (the counter survives power off)", "Záložní články (čítač přežije vypnutí)", "true",
      nullptr, 0, 0 },
    { "file", PK_FILE, "Counter kept by the cells", "Čítač držený články", "\"work/rtc-1/rtc.bin\"", k_none, 0, 0 },
    { "xc2", PK_CHOICE, "Cable on XC2 (DD5 clocks and outputs)", "Kabel na XC2 (hodiny a výstupy DD5)", "\"none\"", k_rtc1_xc2, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// DPI-1L (Martin Lukasek): only the Repository API with the NET commands (docs/desky/DPI-1L.md)
static const ParamOption k_dpi1l_transport[] = {
    { "\"ws\"", "WebSocket (ws://host:port/net, api.mzpico.com, relay.py 8765)",
      "WebSocket (ws://host:port/net, api.mzpico.com, relay.py 8765)" },
    { "\"tcp\"", "TCP with JSON lines (relay.py 8766)", "TCP s řádky JSON (relay.py 8766)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_dpi1l[] = {
    { "base", PK_HEX, "Repository API ports (DPI.INI)", "Porty Repository API (DPI.INI)", "\"B0h\"", nullptr, 0, 0xFE },
    { "net.transport", PK_CHOICE, "NET: connection to the game relay", "NET: spojení s relay pro hry", "\"ws\"",
      k_dpi1l_transport, 0, 0 },
    { "net.host", PK_TEXT, "NET: relay address", "NET: adresa relay", "\"api.mzpico.com\"", nullptr, 0, 0 },
    { "net.port", PK_INT, "NET: relay port (80; relay.py 8765 ws, 8766 tcp)",
      "NET: port relay (80; relay.py 8765 ws, 8766 tcp)", "80", nullptr, 1, 65535 },
    { "net.tcp_default", PK_TEXT, "TCP: target of TCPOPEN until the program sets one (a.b.c.d:port)",
      "TCP: cíl TCPOPEN, dokud ho program nenastaví (a.b.c.d:port)", "\"\"", nullptr, 0, 0 },
    { "sd.folder", PK_DIR, "SD card: folder of the PC (its root /)", "SD karta: složka PC (její kořen /)",
      "\"work/dpi-1l/sdcard\"", nullptr, 0, 0 },
    { "sd.sectors", PK_INT, "SD card: size in sectors of 512 B as GETFREE gives it (a full card)",
      "SD karta: velikost v sektorech po 512 B jako GETFREE (plná karta)", "62309376", nullptr, 64, 4294967295ll },
    { "sd.flush_on_exit", PK_BOOL, "SD card: write the open file's page at switching off (a deviation)",
      "SD karta: při vypnutí zapsat rozepsanou stránku souboru (odchylka)", "false", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

static const ParamOption k_dsp1_high[] = {
    { "0", "0xh (SA2 closed A7, A6, A5, A4)", "0xh (SA2 sepnuté A7, A6, A5, A4)" },
    { "1", "1xh (SA2 closed A7, A6, A5)", "1xh (SA2 sepnuté A7, A6, A5)" },
    { "2", "2xh (SA2 closed A7, A6, A4)", "2xh (SA2 sepnuté A7, A6, A4)" },
    { "3", "3xh (SA2 closed A7, A6)", "3xh (SA2 sepnuté A7, A6)" },
    { "4", "4xh (SA2 closed A7, A5, A4)", "4xh (SA2 sepnuté A7, A5, A4)" },
    { "5", "5xh (SA2 closed A7, A5)", "5xh (SA2 sepnuté A7, A5)" },
    { "6", "6xh (SA2 closed A7, A4)", "6xh (SA2 sepnuté A7, A4)" },
    { "7", "7xh (SA2 closed A7)", "7xh (SA2 sepnuté A7)" },
    { "8", "8xh (SA2 closed A6, A5, A4)", "8xh (SA2 sepnuté A6, A5, A4)" },
    { "9", "9xh (SA2 closed A6, A5)", "9xh (SA2 sepnuté A6, A5)" },
    { "10", "Axh (SA2 closed A6, A4)", "Axh (SA2 sepnuté A6, A4)" },
    { "11", "Bxh (SA2 closed A6)", "Bxh (SA2 sepnuté A6)" },
    { "12", "Cxh (SA2 closed A5, A4)", "Cxh (SA2 sepnuté A5, A4)" },
    { "13", "Dxh (SA2 closed A5)", "Dxh (SA2 sepnuté A5)" },
    { "14", "Exh (SA2 closed A4)", "Exh (SA2 sepnuté A4)" },
    { "15", "Fxh (SA2 all open)", "Fxh (SA2 vše rozepnuto)" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dsp1_ch2[] = {
    { "\"X0\"", "X0, X1 (SA1 2)", "X0, X1 (SA1 2)" },
    { "\"X8\"", "X8, X9 (SA1 6)", "X8, X9 (SA1 6)" },
    { "\"none\"", "off", "vypnuto" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dsp1_ch1[] = {
    { "\"X2\"", "X2, X3 (SA1 3)", "X2, X3 (SA1 3)" },
    { "\"XA\"", "XA, XB (SA1 7)", "XA, XB (SA1 7)" },
    { "\"none\"", "off", "vypnuto" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dsp1_int[] = {
    { "\"X4\"", "X4, X5 (SA1 4)", "X4, X5 (SA1 4)" },
    { "\"XC\"", "XC, XD (SA1 8)", "XC, XD (SA1 8)" },
    { "\"none\"", "off", "vypnuto" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_dsp1_rate[] = {
    { "0", "N = 0, 153.6 kHz: 9600 Bd (x16), 2400 Bd (x64)", "N = 0, 153,6 kHz: 9600 Bd (x16), 2400 Bd (x64)" },
    { "1", "N = 1, 76.8 kHz: 4800 / 1200 Bd", "N = 1, 76,8 kHz: 4800 / 1200 Bd" },
    { "2", "N = 2, 38.4 kHz: 2400 / 600 Bd", "N = 2, 38,4 kHz: 2400 / 600 Bd" },
    { "3", "N = 3, 19.2 kHz: 1200 / 300 Bd", "N = 3, 19,2 kHz: 1200 / 300 Bd" },
    { "4", "N = 4, 9.6 kHz: 600 / 150 Bd", "N = 4, 9,6 kHz: 600 / 150 Bd" },
    { "5", "N = 5, 4.8 kHz: 300 / 75 Bd", "N = 5, 4,8 kHz: 300 / 75 Bd" },
    { "6", "N = 6, 3.2 kHz: 200 / 50 Bd", "N = 6, 3,2 kHz: 200 / 50 Bd" },
    { "7", "N = 7, external clock (pin 15)", "N = 7, vnější hodiny (kolík 15)" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_dsp1[] = {
    { "high", PK_CHOICE, "SA2: upper hex digit of the ports (closed = 0)", "SA2: horní číslice portů (sepnuto = 0)", "4",
      k_dsp1_high, 0, 0 },
    { "ch2", PK_CHOICE, "SA1: channel 2 (connector X2)", "SA1: kanál 2 (konektor X2)", "\"X8\"", k_dsp1_ch2, 0, 0 },
    { "ch1", PK_CHOICE, "SA1: channel 1 (connector X3)", "SA1: kanál 1 (konektor X3)", "\"XA\"", k_dsp1_ch1, 0, 0 },
    { "int", PK_CHOICE, "SA1: interrupt mask register (INT0)", "SA1: registr povolení přerušení (INT0)", "\"XC\"",
      k_dsp1_int, 0, 0 },
    { "x2", PK_SECTION, "Connector X2: channel 2 (8251)", "Konektor X2: kanál 2 (8251)", "true", nullptr, 0, 0 },
    { "x2.rate", PK_CHOICE, "SA3: TxC = RxC", "SA3: TxC = RxC", "0", k_dsp1_rate, 0, 0 },
    { "x2.ext_hz", PK_INT, "External clock on pin 15 (Hz, rate 7)", "Vnější hodiny na kolíku 15 (Hz, N = 7)", "0", nullptr, 0,
      2000000 },
    { "x2.connected", PK_BOOL, "Terminal connected at start (DCD)", "Terminál připojen při startu (DCD)", "true", nullptr,
      0, 0 },
    { "x2.cts", PK_BOOL, "Full cable, terminal ready (CTS/DSR)", "Úplný kabel, terminál připraven (CTS/DSR)", "true",
      nullptr, 0, 0 },
    { "x2.serial.pacing", PK_BOOL, "PC side: no overrun (pacing)", "Strana PC: bez přetečení", "true", nullptr, 0, 0 },
    { "x2.serial.tcp", PK_SECTION, "PC side: TCP server", "Strana PC: TCP server", "false", nullptr, 0, 0 },
    { "x2.serial.tcp.port", PK_INT, "TCP port", "TCP port", "2324", nullptr, 1, 65535 },
    { "x2.serial.tcp.bind", PK_TEXT, "Address (127.0.0.1 = this PC only)", "Adresa (127.0.0.1 = jen tento PC)",
      "\"127.0.0.1\"", nullptr, 0, 0 },
    { "x2.serial.tcp.protocol", PK_CHOICE, "Protocol", "Protokol", "\"telnet\"", k_tcp_proto, 0, 0 },
    { "x2.serial.tcp.dcd_follows", PK_BOOL, "DCD follows the client connection", "DCD podle připojení klienta", "true",
      nullptr, 0, 0 },
    { "x2.serial.com", PK_SECTION, "PC side: COM port", "Strana PC: COM port", "false", nullptr, 0, 0 },
    { "x2.serial.com.port", PK_TEXT, "Port (COM5, /dev/ttyUSB0)", "Port (COM5, /dev/ttyUSB0)", "\"COM1\"", nullptr, 0, 0 },
    { "x2.serial.com.baud", PK_INT, "Baud rate", "Rychlost", "9600", nullptr, 50, 921600 },
    { "x3", PK_SECTION, "Connector X3: channel 1 (8251)", "Konektor X3: kanál 1 (8251)", "true", nullptr, 0, 0 },
    { "x3.rate", PK_CHOICE, "SA4: TxC = RxC", "SA4: TxC = RxC", "0", k_dsp1_rate, 0, 0 },
    { "x3.ext_hz", PK_INT, "External clock on pin 15 (Hz, rate 7)", "Vnější hodiny na kolíku 15 (Hz, N = 7)", "0", nullptr, 0,
      2000000 },
    { "x3.connected", PK_BOOL, "Terminal connected at start (DCD)", "Terminál připojen při startu (DCD)", "true", nullptr,
      0, 0 },
    { "x3.cts", PK_BOOL, "Full cable, terminal ready (CTS/DSR)", "Úplný kabel, terminál připraven (CTS/DSR)", "true",
      nullptr, 0, 0 },
    { "x3.serial.pacing", PK_BOOL, "PC side: no overrun (pacing)", "Strana PC: bez přetečení", "true", nullptr, 0, 0 },
    { "x3.serial.tcp", PK_SECTION, "PC side: TCP server", "Strana PC: TCP server", "false", nullptr, 0, 0 },
    { "x3.serial.tcp.port", PK_INT, "TCP port", "TCP port", "2325", nullptr, 1, 65535 },
    { "x3.serial.tcp.bind", PK_TEXT, "Address (127.0.0.1 = this PC only)", "Adresa (127.0.0.1 = jen tento PC)",
      "\"127.0.0.1\"", nullptr, 0, 0 },
    { "x3.serial.tcp.protocol", PK_CHOICE, "Protocol", "Protokol", "\"telnet\"", k_tcp_proto, 0, 0 },
    { "x3.serial.tcp.dcd_follows", PK_BOOL, "DCD follows the client connection", "DCD podle připojení klienta", "true",
      nullptr, 0, 0 },
    { "x3.serial.com", PK_SECTION, "PC side: COM port", "Strana PC: COM port", "false", nullptr, 0, 0 },
    { "x3.serial.com.port", PK_TEXT, "Port (COM5, /dev/ttyUSB0)", "Port (COM5, /dev/ttyUSB0)", "\"COM1\"", nullptr, 0, 0 },
    { "x3.serial.com.baud", PK_INT, "Baud rate", "Rychlost", "9600", nullptr, 50, 921600 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// ---------------------------------------------------------------- cards

#define CARD(var, type, panel, en, cs, cls, width, emulated, params) \
    static const CardInfo var = { type, panel, en, cs, cls, width, emulated, params };

// CPU
CARD(c_jpr1, "JPR-1", "JPR", "CPU card 8080, memory mapped ports", "Procesorová deska 8080, porty v paměti",
     CC_CPU, 1, true, k_jpr1)
CARD(c_jpr1s, "JPR-1S", "JPR", "CPU card 8080, JPR-1 remake with RAM banks (Libor Lasota)",
     "Procesorová deska 8080, nová JPR-1 s bankami RAM (Libor Lasota)", CC_CPU, 1, true, k_jpr1s)
CARD(c_jpr1x, "JPR-1X", "JPR", "CPU card 8085, JPR-1 remake with 3214, two ROM banks, TURBO (Libor Lasota)",
     "Procesorová deska 8085, nová JPR-1 s 3214, dvěma bankami ROM a TURBO (Libor Lasota)", CC_CPU, 1, true, k_jpr1x)
CARD(c_jpr1a, "JPR-1A", "JPR", "CPU card 8080 \"A\" (I/O ports, boot EPROM)", "Procesorová deska 8080 \"A\" (porty I/O, boot EPROM)",
     CC_CPU, 1, true, k_jpr1a)
CARD(c_jpr1z, "JPR-1Z", "JPR", "CPU card Z80 (ZPS-4)", "Procesorová deska Z80 (ZPS-4)", CC_CPU, 1, true, k_jpr1z)
CARD(c_jpr1zi, "JPR-1ZI", "JPR", "CPU card Z80 (new design)", "Procesorová deska Z80 (nová konstrukce)",
     CC_CPU, 1, true, k_jpr1zi)
CARD(c_jpr1v, "JPR-1V", "JPR", "CPU card Z80 (V)", "Procesorová deska Z80 (V)", CC_CPU, 1, true, k_jpr1v)
CARD(c_pkt1, "PKT-1", "PKT", "SAPI-1.5 CPU card Z80: keyboard, printer, BOOT, MAP (any slot: ARB-1C JP3 = 1-2)",
     "Procesorová deska SAPI-1.5 Z80: klávesnice, tiskárna, BOOT, MAP (libovolný slot: ARB-1C JP3 = 1-2)", CC_CPU, 1,
     true, k_pkt1)
// memory
CARD(c_rem1, "REM-1", "REM", "RAM and EPROM card", "Deska pamětí RAM a EPROM", CC_MEMORY, 1, true, k_rem1)
CARD(c_ram1v, "RAM-1V", "RAM", "Configurable SRAM 64 KB with MAP (V)", "Konfigurovatelná SRAM 64 KB s MAP (V)", CC_MEMORY, 1,
     true, k_ram1v)
CARD(c_ram1, "RAM-1", "RAM", "Dynamic RAM up to 48 KB", "Dynamická RAM až 48 KB", CC_MEMORY, 1, true, k_ram1)
CARD(c_ram1a, "RAM-1A", "RAM", "Dynamic RAM up to 48 KB \"A\"", "Dynamická RAM až 48 KB \"A\"", CC_MEMORY, 1, true,
     k_ram1a)
CARD(c_ram1z, "RAM-1Z", "RAM", "Dynamic RAM up to 64 KB", "Dynamická RAM až 64 KB", CC_MEMORY, 1, true, k_ram1z)
CARD(c_rsw1, "RSW-1", "RSW", "SAPI-1.5: 64 KB RAM, serial port 8251, watchdog",
     "SAPI-1.5: RAM 64 KB, sériový port 8251, watchdog", CC_MEMORY, 1, true, k_rsw1)
CARD(c_ram64, "RAM-64", "RAM", "Battery backed RAM up to 64 KB", "Zálohovaná RAM až 64 KB", CC_MEMORY, 1, false,
     nullptr)
CARD(c_crm1, "CRM-1", "CRM", "Battery backed CMOS RAM 4 KB", "Zálohovaná paměť CMOS 4 KB", CC_MEMORY, 1, true, k_crm1)
CARD(c_dmr1, "DMR-1", "DMR", "128 KB RAM and serial port", "RAM 128 KB a sériový port", CC_MEMORY, 1, true, k_dmr1)
CARD(c_rmd1, "RMD-1", "RMD", "RAM disk", "RAM disk", CC_DISK, 1, true, k_rmd1)
CARD(c_zrmd1, "ZRMD-1", "ZRMD", "RAM disk 4 MB with RTC", "RAM disk 4 MB s RTC", CC_DISK, 1, true, k_zrmd1)
CARD(c_zrd1v, "ZRD-1V", "ZRD", "RAM disk 4 MB (V)", "RAM disk 4 MB (V)", CC_DISK, 1, true, k_zrd1v)
// display
CARD(c_and1v, "AND-1V", "AND", "VGA display 64x20 (V)", "Displej VGA 64x20 (V)", CC_DISPLAY, 1, true, k_and1v)
CARD(c_bgp1s, "BGP-1S", "BGP", "Graphics 288x256 like PMD-85, TV (Libor Lasota)",
     "Grafika 288x256 jako PMD-85, TV (Libor Lasota)", CC_DISPLAY, 1, true, k_bgp1s)
CARD(c_and1b, "AND-1B", "AND", "VGA replacement of AND-1/AND-1A, 40x24 (Libor Lasota)",
     "Náhrada AND-1/AND-1A s VGA, 40x24 (Libor Lasota)", CC_DISPLAY, 1, true, k_and1b)
CARD(c_and1, "AND-1", "AND", "Alphanumeric display 40x24", "Alfanumerický displej 40x24", CC_DISPLAY, 1, true, k_and1)
CARD(c_and1a, "AND-1A", "AND", "Alphanumeric display \"A\"", "Alfanumerický displej \"A\"", CC_DISPLAY, 1, true,
     k_and1a)
CARD(c_and1z, "AND-1Z", "AND", "Alphanumeric display \"Z\"", "Alfanumerický displej \"Z\"", CC_DISPLAY, 1, true,
     k_and1z)
CARD(c_and1z89, "AND-1Z-89", "AND", "Alphanumeric display \"Z\" 1989", "Alfanumerický displej \"Z\" 1989",
     CC_DISPLAY, 1, true, k_and1z)
CARD(c_and30c, "AND-30C", "AND", "Display with MC6845", "Displej s MC6845", CC_DISPLAY, 1, false, nullptr)
CARD(c_dgd1, "DGD-1", "DGD", "Graphic display 320x240", "Grafický displej 320x240", CC_DISPLAY, 1, true, k_dgd1)
CARD(c_dgd1v, "DGD-1V", "DGD", "Graphic display 320x240, VGA (V)", "Grafický displej 320x240, VGA (V)", CC_DISPLAY, 1,
     true, k_dgd1v)
CARD(c_cga1v, "CGA-1V", "CGA", "Colour graphics 320x200, VGA (V)", "Barevná grafika 320x200, VGA (V)", CC_DISPLAY, 1, true,
     k_cga1v)
CARD(c_dbs1a, "DBS-1A", "DBS", "SAPI-1.5 colour semigraphics, 6845, 64x20",
     "Barevná semigrafika SAPI-1.5, 6845, 64x20", CC_DISPLAY, 1, true, k_dbs1a)
// serial
CARD(c_dsm1v, "DSM-1V", "DSM", "Two serial channels (V)", "Dva sériové kanály (V)", CC_SERIAL, 1, true, k_dsm1v)
CARD(c_dsm1, "DSM-1", "DSM", "Serial modem (cassette, terminal)", "Sériový modem (kazeta, terminál)", CC_SERIAL, 1,
     true, k_dsm1)
CARD(c_dsp1, "DSP-1", "DSP", "Two serial ports (8251), RS-232 and current loop",
     "Dva sériové porty (8251), RS-232 a proudová smyčka", CC_SERIAL, 1, true, k_dsp1)
// disks
CARD(c_rpd1, "RPD-1", "RPD", "Floppy disk controller (boards A + B)", "Řadič pružných disků (desky A + B)", CC_DISK,
     2, true, k_rpd1)
CARD(c_rkd1, "RKD-1", "RKD", "Cartridge disk controller CM 5403 / CM 5400 (boards A + B)",
     "Řadič kazetového disku CM 5403 / CM 5400 (desky A + B)", CC_DISK, 2, true, k_rkd1)
CARD(c_rmp1, "RMP-1", "RMP", "Magnetic tape controller (IZOT SM 5300)", "Řadič magnetické pásky (IZOT SM 5300)",
     CC_DISK, 1, true, k_rmp1)
CARD(c_rpd1z, "RPD-1Z", "RPD", "Floppy disk controller \"Z\"", "Řadič pružných disků \"Z\"", CC_DISK, 1, false,
     nullptr)
CARD(c_mfc1a, "MFC-1A", "MFC", "Mini floppy controller 8272 + DMA", "Řadič minidisket 8272 + DMA", CC_DISK, 1, true,
     k_mfc1a)
CARD(c_fdc1, "FDC-1", "FDC", "Floppy disk controller 8272 + DMA (boards A + B)",
     "Řadič pružných disků 8272 + DMA (desky A + B)", CC_DISK, 2, true, k_fdc1)
CARD(c_ide1, "IDE-1", "IDE", "IDE disk and PMD32-SD", "IDE disk a PMD32-SD", CC_DISK, 1, true, k_ide1)
// I/O
CARD(c_dpp1, "DPP-1", "DPP", "Parallel ports (2x 8255), PMD32-SD", "Paralelní porty (2x 8255), PMD32-SD", CC_IO, 1,
     true, k_dpp1)
CARD(c_dps1, "DPS-1", "DPS", "Instrument bus IMS-2 (IEEE 488), controller", "Přístrojová sběrnice IMS-2 (IEEE 488), řidič",
     CC_IO, 1, true, k_dps1)
CARD(c_mph1v, "MPH-1V", "MPH", "Sound YM3812, timer 82C54, joystick (V)", "Zvuk YM3812, časovač 82C54, joystick (V)",
     CC_IO, 1, true, k_mph1v)
CARD(c_rtc1, "RTC-1", "RTC", "Real time clock, timers and watchdog", "Reálný čas, čítače a watchdog", CC_IO, 1, true,
     k_rtc1)
CARD(c_adp1, "ADP-1", "ADP", "A/D converter C 520 D, 16 inputs", "A/D převodník C 520 D, 16 vstupů", CC_IO, 1, true,
     k_adp1)
CARD(c_dmt1, "DMT-1", "DMT", "Measuring amplifier, 8 thermocouple inputs", "Měřicí zesilovač, 8 vstupů pro termočlánky",
     CC_IO, 1, true, k_dmt1)
CARD(c_dse1, "DSE-1", "DSE", "EPROM simulator and programmer 2716/2732", "Simulátor a programátor EPROM 2716/2732",
     CC_IO, 1, true, k_dse1)
CARD(c_drv1a, "DRV-1A", "DRV", "Relay outputs (8 reed relays)", "Reléové výstupy (8 jazýčkových relé)", CC_IO, 1, true,
     k_drv1a)
CARD(c_dpi1l, "DPI-1L", "DPI", "Repository API and NET (network games through a relay)",
     "Repository API a NET (síťové hry přes relay)", CC_IO, 1, true, k_dpi1l)
CARD(c_bdk1, "BDK-1", "BDK", "Prototyping board (empty, no function)", "Bastldeska (prázdná, bez funkce)", CC_OTHER, 1,
     true, nullptr)

const std::vector<const CardInfo*>& card_catalog(void)
{
    static const std::vector<const CardInfo*> list = {
        &c_jpr1zi, &c_jpr1, &c_jpr1s, &c_jpr1x, &c_jpr1a, &c_jpr1z, &c_jpr1v, &c_pkt1,
        &c_dmr1, &c_rsw1, &c_rem1, &c_ram1v, &c_ram1, &c_ram1a, &c_ram1z, &c_ram64, &c_crm1,
        &c_and1z89, &c_and1z, &c_and1v, &c_and1b, &c_bgp1s, &c_and1, &c_and1a, &c_and30c, &c_dgd1, &c_dgd1v, &c_cga1v, &c_dbs1a,
        &c_dsm1v, &c_dsm1, &c_dsp1,
        &c_zrmd1, &c_zrd1v, &c_ide1, &c_rmd1, &c_rpd1, &c_rkd1, &c_rmp1, &c_rpd1z, &c_mfc1a, &c_fdc1,
        &c_mph1v, &c_dpp1, &c_dps1, &c_dse1, &c_rtc1, &c_adp1, &c_dmt1, &c_drv1a, &c_dpi1l, &c_bdk1,
    };
    return list;
}

const CardInfo* card_info(const std::string& type)
{
    for (const CardInfo* c : card_catalog())
        if (type == c->type) return c;
    return nullptr;
}

// ---------------------------------------------------------------- buses

static const BusInfo b_arb1 = { "ARB-1", "ARB-1 (original, 8 positions)", "ARB-1 (původní, 8 pozic)", 8, false };
static const BusInfo b_arb1a = { "ARB-1A", "ARB-1A (extended, 13 positions)", "ARB-1A (prodloužená, 13 pozic)", 13,
                                 false };
static const BusInfo b_arb1au = { "ARB-1Au", "ARB-1Au (extended, PC power supply, 13 positions)",
                                  "ARB-1Au (prodloužená, zdroj PC, 13 pozic)", 13, false };
static const BusInfo b_arb1b = { "ARB-1B", "ARB-1B (extended, 14 positions)", "ARB-1B (prodloužená, 14 pozic)", 14,
                                 false };
static const BusInfo b_arb1c = { "ARB-1C", "ARB-1C (extended, 8282 drivers, 14 positions)",
                                 "ARB-1C (prodloužená, budiče 8282, 14 pozic)", 14, true };
static const BusInfo b_arb1c8 = { "ARB-1C-8", "ARB-1C 8 positions (ZPC)", "ARB-1C 8 pozic (ZPC)", 8, true };
// Libor Lasota, 19" rack: X1 for the CPU card, its A0-A15 through 4 x MH3216 (off by /AEN) to the others
static const BusInfo b_imbus9 = { "ImBUS9", "ImBUS9 (L. Lasota, 1 CPU + 8 positions)", "ImBUS9 (L. Lasota, 1 CPU + 8 pozic)",
                                  9, false };
static const BusInfo b_imbus12 = { "ImBUS12", "ImBUS12 (L. Lasota, 1 CPU + 11 positions)",
                                   "ImBUS12 (L. Lasota, 1 CPU + 11 pozic)", 12, false };

const std::vector<const BusInfo*>& bus_catalog(void)
{
    static const std::vector<const BusInfo*> list = { &b_arb1, &b_arb1a, &b_arb1au, &b_arb1b, &b_arb1c, &b_arb1c8, &b_imbus9, &b_imbus12 };
    return list;
}

const BusInfo* bus_info(const std::string& type)
{
    for (const BusInfo* b : bus_catalog())
        if (type == b->type) return b;
    return nullptr;
}

const char* card_port_param(const CardInfo* ci)
{
    if (!ci) return nullptr;
    if (!strcmp(ci->type, "DMR-1")) return "uart_base";
    if (!strcmp(ci->type, "DSM-1")) return "base";
    return nullptr;
}

// docs/desky/RAM-1V.md: the MAP register of RAM-1V has the port and the bits of ZRMD-1 (63h, MAP1 = bit 6, MAP2 =
// bit 7), so one OUT 63h sets the RAM disk address A16-A21 (bits 0-5) and MAP1/MAP2 together (MikroMon's OUT 63h,3F
// at the start). Not with a ZRMD-1 driving MAP itself (JP2): its function is another one in the map, a warning stays.
static const SharedWrite k_shared_writes[] = {
    { "RAM-1V", "MAP register", "ZRD-1V", "RAM disk address A16-A21",
      "RAM-1V takes bits 6-7 (MAP1, MAP2), the RAM disk bits 0-5 (A16-A21): the port and the bits of ZRMD-1, one "
      "OUT sets both.",
      "RAM-1V bere bity 6-7 (MAP1, MAP2), RAM disk bity 0-5 (A16-A21): port a bity jako u ZRMD-1, jeden OUT nastaví "
      "oboje." },
    { "RAM-1V", "MAP register", "ZRMD-1", "RAM disk address A16-A21",
      "RAM-1V takes bits 6-7 (MAP1, MAP2), the RAM disk bits 0-5 (A16-A21): the port and the bits of ZRMD-1, one "
      "OUT sets both.",
      "RAM-1V bere bity 6-7 (MAP1, MAP2), RAM disk bity 0-5 (A16-A21): port a bity jako u ZRMD-1, jeden OUT nastaví "
      "oboje." },
};

const SharedWrite* card_shared_write(const std::string& type, const std::string& what, const std::string& type2,
                                     const std::string& what2)
{
    for (const SharedWrite& s : k_shared_writes) {
        if (type == s.type && what == s.what && type2 == s.type2 && what2 == s.what2) return &s;
        if (type2 == s.type && what2 == s.what && type == s.type2 && what == s.what2) return &s;
    }
    return nullptr;
}

const char* card_class_name(int cls, bool czech)
{
    switch (cls) {
    case CC_CPU: return czech ? "procesor" : "CPU";
    case CC_MEMORY: return czech ? "paměť" : "memory";
    case CC_DISPLAY: return czech ? "displej" : "display";
    case CC_SERIAL: return czech ? "sériové rozhraní" : "serial";
    case CC_DISK: return czech ? "disk" : "disk";
    case CC_IO: return czech ? "vstup/výstup" : "I/O";
    default: return czech ? "ostatní" : "other";
    }
}
