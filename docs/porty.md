# Mapa I/O portů a paměti SAPI-1

## I/O porty (CP/M sestavy)

Zdroj: `PORTY_v_SAPI-1_CPM.xls` (archiv autora), tabulka „čistě portové desky“, doplněná z ROM a ovladačů.

| Porty | Deska |
|---|---|
| 00–03 | JPR-1Z/ZI (00 = BOOT off, 01–03 = P0–P2; P2 a P1-OUT1 = tiskárna na X3, `docs/tiskarna.md`) |
| x0–x1 | ADP-1: +0 data (NSD, LSD), +1 stav (READY, polarita, MSD) / řízení (kanál, zesílení); báze SA1–SA4, APS-5 00h (`docs/desky/ADP-1.md`) |
| x0–xF | DMT-1: jen zápis, dekóduje jen A7–A4 (16 zrcadel), APS-5 80h (`docs/desky/DMT-1.md`) |
| 40–77 | DRV-1A: jediný port 40–47, 50–57, 60–67 nebo 70–77h (z výroby 77h), jen zápis (`docs/desky/DRV-1A.md`) |
| 04–0F | FDC-1 (8257 04–0B, 8272 0C–0D, FDC-1B 0E–0F: 0E výběr/SA2, 0F motory M3–M0 a hlavy; PROM D5B, `docs/desky/FDC-1.md`) nebo MFC-1A (8257 04, 05, 08, 8272 0C–0D, registr mechanik 0F: mechanika, motor D3 D2, reset, INT, D6; PROM DD9, `docs/desky/MFC-1A.md`) |
| 10–13, 14–17, 18–1B, 1C–1F | DSM-1 / DMR-1, kanály #1–#4 |
| 20–25 | RPD-1A (8253 20–23, data 24, řízení/stav 25; ověřeno z EBIOS) |
| 00–03, 0E | PKT-1 (SAPI-1.5): 8255 00–03 (tiskárna, klávesnice), 0E = BOOT, MAP2, MAP1 (`docs/desky/PKT-1.md`) |
| 18–1F | RSW-1 (SAPI-1.5): 8251 18–19, 8253 1A–1D, maska přerušení 1E, watchdog 1F (`docs/desky/RSW-1.md`) |
| E0–E1 | DBS-1A (SAPI-1.5): 6845 adresa a data (`docs/desky/DBS-1A.md`) |
| 00–3F | DPP-1: 8 portů na základu 00h, 08h … 38h podle přepínačů S1 (+0–3 kanál 1, +4–7 kanál 2; ECMON a CP/M J. Bíby 28–2Fh: 28–2Bh `PUN:` nebo tiskárna (mód 1), PMD32-SD na 2C–2Fh; MIKOS/K 30–33h: KZD-1 na kanálu 1, 30h řízení, 31h stav, 33h = 8Bh, `docs/desky/KZD-1.md`; Minigraf 0507 na 31h
(port B, 33h = 99h), `docs/desky/Minigraf.md`; `docs/desky/DPP-1.md`) |
| 30–37 | DPP-1/7 |
| 30–37 | DPS-1 (pevně): 8255 D2C 30–33h řízení IMS-2 (PA, PB vstup, PC výstup, 33h = 92h), 8255 D1C 34–37h data IMS-2 (34h vstup, 35h výstup, 36h stav, 37h = B0h posluchač / 94h mluvčí); `docs/desky/DPS-1.md` |
| 38–3B | BWP-1 |
| 3C–3F | RPD-1B |
| 40–44 | RMD-1/1 |
| 48–4D | DSP-1 (programy J. Bíby): kanál 2 / X2 48–49h, kanál 1 / X3 4A–4Bh, maska přerušení 4C–4Dh; horní číslice SA2, skupiny X0/X8 přepínačem SA1 (`docs/desky/DSP-1.md`) |
| 50–57 | MPH-1V (82C54, STATUS/IEN/IACK, joystick, YM3812), báze S1 |
| 58–5B | IDE-1, IDE 8255 (Martinovo nastavení) |
| 5C–5F | IDE-1, PMD32-SD 8255 (Martinovo nastavení) |
| 60–63 | ZRMD-1 / ZRD-1V (63h = mapovací registr) |
| 70–7F | ZRMD-1 RTC |
| 00–FF po 16 | RTC-1: 8255 +0–3 (čítač reálného času), 8253 DD5 +4–7, DD6 +8–11, zápis času +12–15; základ SA1–SA8 a propojka A7, programy J. Bíby A0–AFh (`docs/desky/RTC-1.md`) |
| B0–B1 | DPI-1L, Repository API (soubory na SD kartě nad složkou PC, NET): B0h příkaz (OUT) a stav (IN), B1h parametry, výstup a bajty souboru; báze z `DPI.INI`, bez zrcadel. Kódy NET A0–A9h jsou příkazy na B0h, ne porty (`docs/desky/DPI-1L.md`) |
| C0–CB | RMP-1: 8255 D1B C0–C3 (data pro pásku a z ní), 8255 D2B C4–C7 (povely, stav, výběr jednotky), 8253 C8–CB (`docs/desky/RMP-1.md`) |
| F0–F7 | DSE-1 (simulátor a programátor EPROM): 8255 F0–F3h (PA data, PB adresa A0–A7, PC A8–A11, W, R, PROG.PULS, R/W), registr 74LS174 F4h jen zápis (Vpp, ON LINE, LED, MAP), F5–F7h jeho zrcadla, F8–FFh deska nedekóduje. Základ 00h…F0h přepínači S1, z výroby F0h. Koliduje se ZDP-1 a s RTC-1 na F0h v sestavě `aps5`. Emilův program (OK2KOJ) má základ 20h, tam koliduje s RPD-1A 20–25h (`docs/desky/DSE-1.md`) |
| F0–F7 | ZDP-1 |
| F8–F9 | DSP-1/MS80 |

Druhý průzkum přečetl některé rozsahy z XLS jinak: RPD-1A 24–25, DPP-1/6 28–2F, RMD-1 40–43 a 45, DSP-1 48–4D, DSP-1/MS80 FE–FF.
**Nejasnost.** Při práci na příslušné desce je potřeba ověřit přímo v XLS.

## Paměť

| Sestava | Rozložení |
|---|---|
| CP/M ZPS-4 | RAM 0000–F7FF (62 KB), AND-1Z na F800–FFFF |
| CP/M ZPS-3 s AND-1A | displej na E800h (48K), na F800h (62K, RAM 0000–F7FF) |
| MIKOS | displej na 3800h. MAP odebere 0000–3FFF z RAM-1Z/DMR-1, aby tam mohla být ROM. Autoritativní mapu MIKOS zatím nemáme |
| JPR-1ZI při BOOT=1 | EPROM přes 0000–07FF (nebo 0000–1FFF). Zápisy jdou do RAM pod ní |
| ZPS-6 (`zps/ZPS-6`, SAPI-1.5) | EPROM PKT-1 0000–07FF jen po resetu (BOOT, čtení), RSW-1 RAM 0000–FFFF; při MAP2 = 1 díra B000–BFFF, kam se jen zapisuje video RAM DBS-1A. ROM BIOS běží z kopie na F000h, CP/M 62K (CCP D400h) |
| JPR-1X (`jpr1x_bgp1s`, `zps/ZPS-1_JPR-1X`) | ROM 0000–1FFF (banka podle propojky BIOS), RAM 2000–23FF, porty P0/P1/P2 v paměti 2400/2800/2C00, 3214 na 3000 (jen zápis), čtení 3000–FFFF ze sběrnice, každý zápis i na sběrnici. Žádné porty I/O |
| JPR-1S + BGP-1S (`jpr1s_bgp1s`) | EEPROM 0000–1FFF, RAM 2000–23FF, porty P0/P1/P2 v paměti 2400/2800/2C00, RAM 4000–FFFF (banky JPR-1S); BGP-1S na C000 jen pro zápis, čte se banka pod ní. Žádné porty I/O |
| ZPS-1 s AND-1B | jako ZPS-1: AND-1B na 3800–3FFF (okno 2 KB, 1 KB RAM), bez portů I/O |
| APS-5 (`aps5`) | EPROM 0000–1FFF a 6000–77FF (REM-1), porty JPR-1 2400/2800/2C00, 3214 na 3000, AND-1 3800–3FFF, CRM-1 E000–EFFF; I/O: ADP-1 00–01, DRV-1A 77, DMT-1 80, RTC-1 F0–FC (`docs/desky/APS-5.md`) |
