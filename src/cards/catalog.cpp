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
static const CardParam k_ram1[] = {
    { "fitted_kb", PK_CHOICE, "Fitted memory", "Osazená paměť", "48", k_ram1_kb, 0, 0 },
    { "s1", PK_BOOL, "S1 (S1 S2: 10 = 4000-FFFF, 01 = 4000-BFFF, 11 = 4000-7FFF, 00 = 4000-DFFF)",
      "S1 (S1 S2: 10 = 4000-FFFF, 01 = 4000-BFFF, 11 = 4000-7FFF, 00 = 4000-DFFF)", "true", nullptr, 0, 0 },
    { "s2", PK_BOOL, "S2", "S2", "false", nullptr, 0, 0 },
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
static const CardParam k_and1[] = {
    { "charset", PK_CHOICE, "Character generator", "Generátor znaků", "\"MHB2501\"", k_and1_cs, 0, 0 },
    { "rows", PK_CHOICE, "Rows", "Řádků", "24", k_and1_rows, 0, 0 },
    { "int1", PK_BOOL, "Jumper 49-50: INT1 at the end of the frame", "Propojka 49-50: INT1 na konci snímku", "true",
      nullptr, 0, 0 },
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
    { "map_jp2", PK_BOOL, "JP2: MAP1/MAP2 from the address register", "JP2: MAP1/MAP2 z adresového registru",
      "false", nullptr, 0, 0 },
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
CARD(c_jpr1a, "JPR-1A", "JPR", "CPU card 8080 \"A\"", "Procesorová deska 8080 \"A\"", CC_CPU, 1, false, k_cpu_basic)
CARD(c_jpr1z, "JPR-1Z", "JPR", "CPU card Z80", "Procesorová deska Z80", CC_CPU, 1, false, k_cpu_basic)
CARD(c_jpr1zi, "JPR-1ZI", "JPR", "CPU card Z80 (new design)", "Procesorová deska Z80 (nová konstrukce)",
     CC_CPU, 1, true, k_jpr1zi)
CARD(c_jpr1v, "JPR-1V", "JPR", "CPU card Z80 (V)", "Procesorová deska Z80 (V)", CC_CPU, 1, false, k_cpu_basic)
// memory
CARD(c_rem1, "REM-1", "REM", "RAM and EPROM card", "Deska pamětí RAM a EPROM", CC_MEMORY, 1, true, k_rem1)
CARD(c_ram1, "RAM-1", "RAM", "Dynamic RAM up to 48 KB", "Dynamická RAM až 48 KB", CC_MEMORY, 1, true, k_ram1)
CARD(c_ram1a, "RAM-1A", "RAM", "Dynamic RAM up to 48 KB \"A\"", "Dynamická RAM až 48 KB \"A\"", CC_MEMORY, 1, false,
     nullptr)
CARD(c_ram1z, "RAM-1Z", "RAM", "Dynamic RAM up to 64 KB", "Dynamická RAM až 64 KB", CC_MEMORY, 1, false, nullptr)
CARD(c_ram64, "RAM-64", "RAM", "Battery backed RAM up to 64 KB", "Zálohovaná RAM až 64 KB", CC_MEMORY, 1, false,
     nullptr)
CARD(c_crm1, "CRM-1", "CRM", "Battery backed memory", "Deska zálohovaných pamětí", CC_MEMORY, 1, false, nullptr)
CARD(c_dmr1, "DMR-1", "DMR", "128 KB RAM and serial port", "RAM 128 KB a sériový port", CC_MEMORY, 1, true, k_dmr1)
CARD(c_rmd1, "RMD-1", "RMD", "RAM disk", "RAM disk", CC_DISK, 1, false, nullptr)
CARD(c_zrmd1, "ZRMD-1", "ZRMD", "RAM disk 4 MB with RTC", "RAM disk 4 MB s RTC", CC_DISK, 1, true, k_zrmd1)
CARD(c_zrd1v, "ZRD-1V", "ZRD", "RAM disk 4 MB (V)", "RAM disk 4 MB (V)", CC_DISK, 1, true, k_zrd1v)
// display
CARD(c_and1, "AND-1", "AND", "Alphanumeric display 40x24", "Alfanumerický displej 40x24", CC_DISPLAY, 1, true, k_and1)
CARD(c_and1a, "AND-1A", "AND", "Alphanumeric display \"A\"", "Alfanumerický displej \"A\"", CC_DISPLAY, 1, false,
     nullptr)
CARD(c_and1z, "AND-1Z", "AND", "Alphanumeric display \"Z\"", "Alfanumerický displej \"Z\"", CC_DISPLAY, 1, true,
     k_and1z)
CARD(c_and1z89, "AND-1Z-89", "AND", "Alphanumeric display \"Z\" 1989", "Alfanumerický displej \"Z\" 1989",
     CC_DISPLAY, 1, true, k_and1z)
CARD(c_and30c, "AND-30C", "AND", "Display with MC6845", "Displej s MC6845", CC_DISPLAY, 1, false, nullptr)
CARD(c_dgd1, "DGD-1", "DGD", "Graphic display", "Grafický displej", CC_DISPLAY, 1, false, nullptr)
CARD(c_dbs1a, "DBS-1A", "DBS", "Colour semigraphics", "Barevná semigrafika", CC_DISPLAY, 1, false, nullptr)
// serial
CARD(c_dsm1, "DSM-1", "DSM", "Serial modem (cassette, terminal)", "Sériový modem (kazeta, terminál)", CC_SERIAL, 1,
     true, k_dsm1)
CARD(c_dsp1, "DSP-1", "DSP", "Serial ports", "Sériové porty", CC_SERIAL, 1, false, nullptr)
// disks
CARD(c_rpd1, "RPD-1", "RPD", "Floppy disk controller (boards A + B)", "Řadič pružných disků (desky A + B)", CC_DISK,
     2, false, nullptr)
CARD(c_rpd1z, "RPD-1Z", "RPD", "Floppy disk controller \"Z\"", "Řadič pružných disků \"Z\"", CC_DISK, 1, false,
     nullptr)
CARD(c_mfc1a, "MFC-1A", "MFC", "Mini floppy controller", "Řadič minidisket", CC_DISK, 1, false, nullptr)
CARD(c_fdc1, "FDC-1", "FDC", "Floppy disk controller", "Řadič disket", CC_DISK, 1, false, nullptr)
CARD(c_ide1, "IDE-1", "IDE", "IDE disk and PMD32-SD", "IDE disk a PMD32-SD", CC_DISK, 1, true, k_ide1)
// I/O
CARD(c_dpp1, "DPP-1", "DPP", "Parallel ports", "Paralelní porty", CC_IO, 1, false, nullptr)
CARD(c_rtc1, "RTC-1", "RTC", "Real time clock and timer", "Reálný čas a čítač", CC_IO, 1, false, nullptr)
CARD(c_bdk1, "BDK-1", "BDK", "Universal board", "Univerzální deska", CC_OTHER, 1, false, nullptr)

const std::vector<const CardInfo*>& card_catalog(void)
{
    static const std::vector<const CardInfo*> list = {
        &c_jpr1zi, &c_jpr1, &c_jpr1a, &c_jpr1z, &c_jpr1v,
        &c_dmr1, &c_rem1, &c_ram1, &c_ram1a, &c_ram1z, &c_ram64, &c_crm1,
        &c_and1z89, &c_and1z, &c_and1, &c_and1a, &c_and30c, &c_dgd1, &c_dbs1a,
        &c_dsm1, &c_dsp1,
        &c_zrmd1, &c_zrd1v, &c_ide1, &c_rmd1, &c_rpd1, &c_rpd1z, &c_mfc1a, &c_fdc1,
        &c_dpp1, &c_rtc1, &c_bdk1,
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
