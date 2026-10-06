# Mapa I/O portů a paměti SAPI-1

## I/O porty (CP/M sestavy)

Zdroj: `H:\SAPI-1\PORTY_v_SAPI-1_CPM.xls`, tabulka „čistě portové desky“, doplněná z ROM a ovladačů.

| Porty | Deska |
|---|---|
| 00–03 | JPR-1Z/ZI (00 = BOOT off, 01–03 = P0–P2; P2 a P1-OUT1 = tiskárna na X3, `docs/tiskarna.md`) |
| 04–0F | FDC-1 (8257 04–0B, 8272 0C–0D, FDC-1B 0E–0F: 0E výběr/SA2, 0F motory M3–M0 a hlavy; PROM D5B, `docs/desky/FDC-1.md`) nebo MFC-1A (8257 04, 05, 08, 8272 0C–0D, registr mechanik 0F: mechanika, motor D3 D2, reset, INT, D6; PROM DD9, `docs/desky/MFC-1A.md`) |
| 10–13, 14–17, 18–1B, 1C–1F | DSM-1 / DMR-1, kanály #1–#4 |
| 20–25 | RPD-1A (8253 20–23, data 24, řízení/stav 25; ověřeno z EBIOS) |
| 00–3F | DPP-1: 8 portů na základu 00h, 08h … 38h podle přepínačů S1 (+0–3 kanál 1, +4–7 kanál 2; ECMON a CP/M J. Bíby 28–2Fh: 28–2Bh `PUN:` nebo tiskárna (mód 1), PMD32-SD na 2C–2Fh; `docs/desky/DPP-1.md`) |
| 30–37 | DPP-1/7 |
| 38–3B | BWP-1 |
| 3C–3F | RPD-1B |
| 40–44 | RMD-1/1 |
| 45–4A | DSP-1 |
| 50–57 | MPH-1V (82C54, STATUS/IEN/IACK, joystick, YM3812), báze S1 |
| 58–5B | IDE-1, IDE 8255 (Martinovo nastavení) |
| 5C–5F | IDE-1, PMD32-SD 8255 (Martinovo nastavení) |
| 60–63 | ZRMD-1 / ZRD-1V (63h = mapovací registr) |
| 70–7F | ZRMD-1 RTC |
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
