#include "cards/catalog.h"

#include <string.h>

// ---------------------------------------------------------------- parameters

static const ParamOption k_none[] = { { nullptr, nullptr, nullptr } };

// JPR-1ZI
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
    { "\"roms/jpr-1zi/JPR-1ZI_ZZRMD_epr2k.bin\"", "ZZRMD 2k", "ZZRMD 2k" },
    { "\"roms/jpr-1zi/JPR-1ZI_ZZRMD_epr8k.bin\"", "ZZRMD 8k", "ZZRMD 8k" },
    { "\"\"", "none (FFh)", "žádná (FFh)" },
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
    { "keyboard", PK_KEYBOARD, "Keyboard (connector X2)", "Klávesnice (konektor X2)", "null", nullptr, 0, 0 },
    { "keyboard.strict", PK_BOOL, "Keyboard: true to the hardware (keys may be lost)",
      "Klávesnice: věrně jako hardware (klávesy se mohou ztratit)", "false", nullptr, 0, 0 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// JPR-1Z (ZPS-4)
static const ParamOption k_jpr1z_roms[] = {
    { "\"roms/jpr-1z/MOR62Z.bin\"", "MOR62Z: CP/M 2.2/P 62K (Perina 1988)", "MOR62Z: CP/M 2.2/P 62K (Peřina 1988)" },
    { "\"roms/jpr-1z/ELIS_V7.01.bin\"", "ELIS v7.01 (Z80, C259.11/C262.3)", "ELIS v7.01 (Z80, C259.11/C262.3)" },
    { "\"roms/jpr-1z/ELIS_V3.00.bin\"", "ELIS v3.00 (8080, C262.3)", "ELIS v3.00 (8080, C262.3)" },
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
    { "keyboard", PK_KEYBOARD, "Keyboard (connector X2)", "Klávesnice (konektor X2)", "null", nullptr, 0, 0 },
    { "keyboard.strict", PK_BOOL, "Keyboard: true to the hardware (keys may be lost)",
      "Klávesnice: věrně jako hardware (klávesy se mohou ztratit)", "false", nullptr, 0, 0 },
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
    { "keyboard", PK_KEYBOARD, "Keyboard (connector K2)", "Klávesnice (konektor K2)", "null", nullptr, 0, 0 },
    { "keyboard.strict", PK_BOOL, "Keyboard: true to the hardware (keys may be lost)",
      "Klávesnice: věrně jako hardware (klávesy se mohou ztratit)", "false", nullptr, 0, 0 },
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

// AND-1V (Libor Lasota)
static const ParamOption k_and1v_map[] = {
    { "\"always\"", "MAP1OFF fitted: always", "MAP1OFF osazená: vždy" },
    { "\"map1_h\"", "MAP1OFF open, H/L fitted: MAP1 = H", "MAP1OFF rozpojená, H/L osazená: MAP1 = H" },
    { "\"map1_l\"", "both open: MAP1 = L", "obě rozpojené: MAP1 = L" },
    { "\"never\"", "both fitted: disabled", "obě osazené: vypnuto" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_and1v_color[] = {
    { "\"white\"", "white (S1 all open)", "bílá (S1 vše rozpojené)" }, { "\"green\"", "green", "zelená" },
    { "\"amber\"", "amber", "jantarová" }, { "\"red\"", "red", "červená" }, { "\"blue\"", "blue", "modrá" },
    { "\"cyan\"", "cyan", "azurová" }, { "\"yellow\"", "yellow", "žlutá" }, { "\"magenta\"", "magenta", "purpurová" },
    { nullptr, nullptr, nullptr },
};
static const ParamOption k_and1v_fonts[] = {
    { "\"roms/and-1v/font_and1v22_1024.bin\"", "10x24 type 1", "10x24 typ 1" },
    { "\"roms/and-1v/newfont_and1v22_1024.bin\"", "10x24 type 2", "10x24 typ 2" },
    { nullptr, nullptr, nullptr },
};
static const CardParam k_and1v[] = {
    { "base", PK_HEX, "S2 /A11-/A15: VRAM base (2 KB steps)", "S2 /A11-/A15: začátek VRAM (po 2 KB)", "\"F800h\"",
      nullptr, 0, 0xF800 },
    { "map", PK_CHOICE, "S2 MAP1OFF, H/L", "S2 MAP1OFF, H/L", "\"map1_l\"", k_and1v_map, 0, 0 },
    { "color", PK_CHOICE, "S1: ink colour", "S1: barva", "\"white\"", k_and1v_color, 0, 0 },
    { "font", PK_FILE, "Character generator 27C256", "Generátor znaků 27C256", "\"roms/and-1v/font_and1v22_1024.bin\"",
      k_and1v_fonts, 0, 0 },
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
static const ParamOption k_jpr1_roms[] = {
    { "\"\"", "none (ZPS-1: EPROMs on REM-1)", "žádné (ZPS-1: EPROM na REM-1)" },
    { "\"roms/mikrobasic/MIKROBASIC_AR1983_1k.bin\"", "MIKROBASIC AR/B 1983 (1 KB RAM on JPR-1)",
      "MIKROBASIC AR/B 1983 (1 KB RAM na JPR-1)" },
    { "\"roms/mikrobasic/MIKROBASIC_2.4_ZPS-1.bin\"", "MIKROBASIC 2.4", "MIKROBASIC 2.4" },
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
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// JPR-1A (ZPS-3)
static const ParamOption k_jpr1a_roms[] = {
    { "\"roms/jpr-1a/JPR-1A_MON_V1.0-C1.bin\"", "48K MON V1.0-C1 (Tesla IMA 1986, CP/M ZPS-3)",
      "48K MON V1.0-C1 (Tesla IMA 1986, CP/M ZPS-3)" },
    { "\"roms/jpr-1a/MOR48A.bin\"", "MOR48A: CP/M 2.2/P 48K (Perina 1988)", "MOR48A: CP/M 2.2/P 48K (Peřina 1988)" },
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
    { nullptr, nullptr, nullptr },
};
static const CardParam k_rpd1[] = {
    { "drive_a", PK_SECTION, "Drive A (8\" SS SD)", "Mechanika A (8\" SS SD)", "true", nullptr, 0, 0 },
    { "drive_a.image", PK_FILE, "Disk image (.img, 77 x 26 x 128 B)", "Obraz diskety (.img, 77 x 26 x 128 B)", "\"\"",
      k_rpd1_disks, 0, 0 },
    { "drive_a.write_protect", PK_BOOL, "Write protected", "Chráněná proti zápisu", "false", nullptr, 0, 0 },
    { "drive_b", PK_SECTION, "Drive B (8\" SS SD)", "Mechanika B (8\" SS SD)", "false", nullptr, 0, 0 },
    { "drive_b.image", PK_FILE, "Disk image (.img, 77 x 26 x 128 B)", "Obraz diskety (.img, 77 x 26 x 128 B)", "\"\"",
      k_rpd1_disks, 0, 0 },
    { "drive_b.write_protect", PK_BOOL, "Write protected", "Chráněná proti zápisu", "false", nullptr, 0, 0 },
    { "working_copy", PK_BOOL, "Work on copies in work/", "Pracovat s kopiemi ve work/", "true", nullptr, 0, 0 },
    { "step_ms", PK_CHOICE, "Drive: step period", "Mechanika: perioda kroku", "8", k_rpd1_step, 0, 0 },
    { "settle_ms", PK_INT, "Drive: head load settle time (ms)", "Mechanika: ustálení přiklopené hlavy (ms)", "35",
      nullptr, 0, 500 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

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
    { "tape", PK_FILE, "Cassette in the recorder (WAV, MP3)", "Kazeta v magnetofonu (WAV, MP3)", "\"\"", k_none, 0, 0 },
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
    { "\"media/zrmd-1/ECSYS_zaloha_0-3.hex\"", "ECSYS 0-3 (HEX)", "ECSYS 0-3 (HEX)" },
    { "\"media/zrmd-1/puvodni_SYS0_3_s_CD.HEX\"", "original SYS 0-3 with CD (HEX)", "původní SYS 0-3 s CD (HEX)" },
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
static const CardParam k_zrd1v[] = {
    ZRMD_COMMON
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// MPH-1V
static const CardParam k_mph1v[] = {
    { "base", PK_HEX, "S1: port base (8 ports)", "S1: báze portů (8 portů)", "\"50h\"", nullptr, 0, 0xF8 },
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
    { "pmd32sd.drives.0", PK_TEXT, "Drive A (CP/M D:)", "Mechanika A (CP/M D:)", "null", nullptr, 0, 0 },
    { "pmd32sd.drives.1", PK_TEXT, "Drive B (CP/M E:)", "Mechanika B (CP/M E:)", "null", nullptr, 0, 0 },
    { "pmd32sd.drives.2", PK_TEXT, "Drive C", "Mechanika C", "null", nullptr, 0, 0 },
    { "pmd32sd.drives.3", PK_TEXT, "Drive D", "Mechanika D", "null", nullptr, 0, 0 },
    { "pmd32sd.card_locked", PK_BOOL, "SD card write protected", "SD karta chráněná proti zápisu", "false",
      nullptr, 0, 0 },
    { "pmd32sd.startup_ms", PK_INT, "Start-up delay (ms, real 3000)", "Prodleva po zapnutí (ms, skutečně 3000)",
      "0", nullptr, 0, 10000 },
    { nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0 },
};

// ---------------------------------------------------------------- cards

#define CARD(var, type, panel, en, cs, cls, width, emulated, params) \
    static const CardInfo var = { type, panel, en, cs, cls, width, emulated, params };

// CPU
CARD(c_jpr1, "JPR-1", "JPR", "CPU card 8080, memory mapped ports", "Procesorová deska 8080, porty v paměti",
     CC_CPU, 1, true, k_jpr1)
CARD(c_jpr1a, "JPR-1A", "JPR", "CPU card 8080 \"A\" (I/O ports, boot EPROM)", "Procesorová deska 8080 \"A\" (porty I/O, boot EPROM)",
     CC_CPU, 1, true, k_jpr1a)
CARD(c_jpr1z, "JPR-1Z", "JPR", "CPU card Z80 (ZPS-4)", "Procesorová deska Z80 (ZPS-4)", CC_CPU, 1, true, k_jpr1z)
CARD(c_jpr1zi, "JPR-1ZI", "JPR", "CPU card Z80 (new design)", "Procesorová deska Z80 (nová konstrukce)",
     CC_CPU, 1, true, k_jpr1zi)
CARD(c_jpr1v, "JPR-1V", "JPR", "CPU card Z80 (V)", "Procesorová deska Z80 (V)", CC_CPU, 1, true, k_jpr1v)
// memory
CARD(c_rem1, "REM-1", "REM", "RAM and EPROM card", "Deska pamětí RAM a EPROM", CC_MEMORY, 1, true, k_rem1)
CARD(c_ram1v, "RAM-1V", "RAM", "Configurable SRAM 64 KB with MAP (V)", "Konfigurovatelná SRAM 64 KB s MAP (V)", CC_MEMORY, 1,
     true, k_ram1v)
CARD(c_ram1, "RAM-1", "RAM", "Dynamic RAM up to 48 KB", "Dynamická RAM až 48 KB", CC_MEMORY, 1, true, k_ram1)
CARD(c_ram1a, "RAM-1A", "RAM", "Dynamic RAM up to 48 KB \"A\"", "Dynamická RAM až 48 KB \"A\"", CC_MEMORY, 1, true,
     k_ram1a)
CARD(c_ram1z, "RAM-1Z", "RAM", "Dynamic RAM up to 64 KB", "Dynamická RAM až 64 KB", CC_MEMORY, 1, true, k_ram1z)
CARD(c_ram64, "RAM-64", "RAM", "Battery backed RAM up to 64 KB", "Zálohovaná RAM až 64 KB", CC_MEMORY, 1, false,
     nullptr)
CARD(c_crm1, "CRM-1", "CRM", "Battery backed memory", "Deska zálohovaných pamětí", CC_MEMORY, 1, false, nullptr)
CARD(c_dmr1, "DMR-1", "DMR", "128 KB RAM and serial port", "RAM 128 KB a sériový port", CC_MEMORY, 1, true, k_dmr1)
CARD(c_rmd1, "RMD-1", "RMD", "RAM disk", "RAM disk", CC_DISK, 1, true, k_rmd1)
CARD(c_zrmd1, "ZRMD-1", "ZRMD", "RAM disk 4 MB with RTC", "RAM disk 4 MB s RTC", CC_DISK, 1, true, k_zrmd1)
CARD(c_zrd1v, "ZRD-1V", "ZRD", "RAM disk 4 MB (V)", "RAM disk 4 MB (V)", CC_DISK, 1, true, k_zrd1v)
// display
CARD(c_and1v, "AND-1V", "AND", "VGA display 64x20 (V)", "Displej VGA 64x20 (V)", CC_DISPLAY, 1, true, k_and1v)
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
CARD(c_dbs1a, "DBS-1A", "DBS", "Colour semigraphics", "Barevná semigrafika", CC_DISPLAY, 1, false, nullptr)
// serial
CARD(c_dsm1v, "DSM-1V", "DSM", "Two serial channels (V)", "Dva sériové kanály (V)", CC_SERIAL, 1, true, k_dsm1v)
CARD(c_dsm1, "DSM-1", "DSM", "Serial modem (cassette, terminal)", "Sériový modem (kazeta, terminál)", CC_SERIAL, 1,
     true, k_dsm1)
CARD(c_dsp1, "DSP-1", "DSP", "Serial ports", "Sériové porty", CC_SERIAL, 1, false, nullptr)
// disks
CARD(c_rpd1, "RPD-1", "RPD", "Floppy disk controller (boards A + B)", "Řadič pružných disků (desky A + B)", CC_DISK,
     2, true, k_rpd1)
CARD(c_rpd1z, "RPD-1Z", "RPD", "Floppy disk controller \"Z\"", "Řadič pružných disků \"Z\"", CC_DISK, 1, false,
     nullptr)
CARD(c_mfc1a, "MFC-1A", "MFC", "Mini floppy controller", "Řadič minidisket", CC_DISK, 1, false, nullptr)
CARD(c_fdc1, "FDC-1", "FDC", "Floppy disk controller", "Řadič disket", CC_DISK, 1, false, nullptr)
CARD(c_ide1, "IDE-1", "IDE", "IDE disk and PMD32-SD", "IDE disk a PMD32-SD", CC_DISK, 1, true, k_ide1)
// I/O
CARD(c_dpp1, "DPP-1", "DPP", "Parallel ports", "Paralelní porty", CC_IO, 1, false, nullptr)
CARD(c_mph1v, "MPH-1V", "MPH", "Sound YM3812, timer 8253, joystick (V)", "Zvuk YM3812, časovač 8253, joystick (V)",
     CC_IO, 1, true, k_mph1v)
CARD(c_rtc1, "RTC-1", "RTC", "Real time clock and timer", "Reálný čas a čítač", CC_IO, 1, false, nullptr)
CARD(c_bdk1, "BDK-1", "BDK", "Prototyping board (empty, no function)", "Bastldeska (prázdná, bez funkce)", CC_OTHER, 1,
     true, nullptr)

const std::vector<const CardInfo*>& card_catalog(void)
{
    static const std::vector<const CardInfo*> list = {
        &c_jpr1zi, &c_jpr1, &c_jpr1a, &c_jpr1z, &c_jpr1v,
        &c_dmr1, &c_rem1, &c_ram1v, &c_ram1, &c_ram1a, &c_ram1z, &c_ram64, &c_crm1,
        &c_and1z89, &c_and1z, &c_and1v, &c_and1, &c_and1a, &c_and30c, &c_dgd1, &c_dgd1v, &c_cga1v, &c_dbs1a,
        &c_dsm1v, &c_dsm1, &c_dsp1,
        &c_zrmd1, &c_zrd1v, &c_ide1, &c_rmd1, &c_rpd1, &c_rpd1z, &c_mfc1a, &c_fdc1,
        &c_mph1v, &c_dpp1, &c_rtc1, &c_bdk1,
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

const std::vector<const BusInfo*>& bus_catalog(void)
{
    static const std::vector<const BusInfo*> list = { &b_arb1, &b_arb1a, &b_arb1au, &b_arb1b, &b_arb1c, &b_arb1c8 };
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
