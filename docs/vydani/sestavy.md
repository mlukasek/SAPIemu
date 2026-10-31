# Sestavy pro veřejné vydání: rozřazení

Návrh z 2026-10-17, upravený podle autora týž den. Kategorie podle historie SAPI-1 rozhodl autor 2026-10-28
(`docs/vydani/plan-vzhled.md`, etapa 2):
- SAPI-1 byl průmyslový řídicí počítač v rámu, ne počítač pro lidi. Hlavní jsou základní sestavy ZPS.
- Lidi, kteří SAPI neznají, přitáhnou přístroje.
- Hry na SAPI nikdy nebyly, hry a dema jsou novodobý doplněk.

V okně Vybrat sestavu jsou kategorie v pořadí Základ → Přístroje a technologie → Desky Libora Lasoty → Hry a dema →
Ostatní. Nahoře je krátký úvod: SAPI-1 byl průmyslový řídicí počítač, začněte ZPS-1, pak zkuste přístroje, hry jsou
novodobý doplněk. Texty kategorií jsou vidět pod nadpisem. Ve vydání jsou všechny sestavy, Prince of Persia
s upozorněním pro nositele práv. Po prvním spuštění se otevře výběr sestav. Software a jeho majitele shrnuje
`docs/vydani/souhlasy.md`.

Zkratky v tabulkách:

- **JB:** JPR-1ZI, DMR-1, AND-1Z-89, klávesnice Consul 259.11, tiskárna.
- **EC:** totéž s ECMONem a klávesnicí EKL-1.
- **V:** Liborovy desky V (JPR-1V, RAM-1V, AND-1V, DSM-1V, ZRD-1V, IDE-1, MPH-1V, CGA-1V).

## 1. Základ: historické sestavy Tesla

Původní základní počítačové sestavy (ZPS) v pořadí, jak šly po sobě. Každá má náhled a průvodce „co vyzkoušet“.

| Pořadí | Soubor | Co ukazuje | Software |
|---|---|---|---|
| 1 | `zps/ZPS-1.sapi` | ZPS-1: JPR-1 8080, AND-1, ANK-1, kazeta | MIKROBASIC 2.4 (Tesla) |
| 2 | `zps/ZPS-2.sapi` | ZPS-2: MIKOS V4.0F, kazeta | MIKOS V4 (Tesla ELTOS) |
| 3 | `zps/ZPS-2_MIKOS_V5.sapi` | ZPS-2 s MIKOSem V5 a grafikou DGD-1 | MIKOS V5 (Tesla DIZ), BASIC EXP z `media/mikos` |
| 4 | `zps/ZPS-3_CPM64K_IMA.sapi` | ZPS-3: JPR-1A, AND-1A, RPD-1, původní CP/M 64K Tesla IMA | CP/M 2.2 Tesla IMA, MBASIC na disketě |
| 5 | `zps/ZPS-4.sapi` | ZPS-4: JPR-1Z, AND-1Z, RMD-1, CP/M 62K | CP/M 2.2/P R. Peřiny (MOR62Z), podle CPM.DOC základní verze ZPS-4 |
| 6 | `zps/ZPS-6.sapi` | SAPI-1.5 (ZPS-6): PKT-1, DBS-1A, MFC-1A, CP/M 62K | CP/M Melsoft 1990 |

## 2. Přístroje a technologie

Přístroje a zařízení se SAPI-1, každý nakreslený v okně. Od 2026-10-28 samostatná kategorie s průvodci, dřív
byly přístroje v Hrách a demech a v Ostatních. XY 4130 a plotter z AR 1990 dostaly průvodce česky i anglicky.

| Pořadí | Soubor | Co ukazuje | Okno |
|---|---|---|---|
| 1 | `aps5.sapi` | měřicí jednotka APS-5 s regulací pecí (kotelna), původní firmware Tesla, tlačítko DEMO | panel HRK-1 a pece |
| 2 | `zps/ZPS-2_DPS-1_IMS-2.sapi` | DPS-1, sběrnice IMS-2, voltmetr M1T 330 a zdroj BS 525 (knoflíky se točí myší), BASIMS | měřicí pracoviště |
| 3 | `zps/ZPS-2_MIKOS_V5_Minigraf.sapi` | zapisovač Minigraf 0507, ukázka z IQ 151, `KRES` | zapisovač |
| 4 | `zps/ZPS-2_MIKOS_V5_XY4130.sapi` | grafická jednotka XY 4130, ovladač TESLA DIZ za MIKOSem V5 | zapisovač |
| 5 | `plotter_ar1990.sapi` | plotter z Amatérského radia 1990 (JPR-1 jako řadič Minigrafu, povely Sharp MZ-1P16) | zapisovač |
| 6 | `zps/ZPS-2_MIKOS_K.sapi` | kazetopáskový záznamník dat KZD-1, MIKOS/K | Média (mechanika) |
| 7 | `rkd1_tesla_cpm.sapi` | kazetový disk RKD-1, CP/M Tesla Elstroj 1987 | Média (kazetový disk) |
| 8 | `rmp1_jb_izot.sapi` | magnetická páska RMP-1 a IZOT SM 5300 | Média (páskové jednotky) |
| 9 | `zps/ZPS-2_DSE-1.sapi` | simulátor a programátor EPROM DSE-1, program DSE Tesla Elstroj (`G1000`), prázdná 2716 v patici (od 2026-10-30, průvodce česky i anglicky) | Programátor EPROM DSE-1 (krabička kabelu s paticí, LED, UV lampa) |

## 3. Desky Libora Lasoty (moderní)

| Pořadí | Soubor | Co ukazuje | Software |
|---|---|---|---|
| 1 | `jpr1s_bgp1s.sapi` | **8080:** JPR-1S a grafika BGP-1S (jako PMD-85), klávesnice ANK-1M2 s joystickem | MIKROBIOS 1.02 (L. Lasota), hry z `media/jpr-1s` (Manic Miner…) |
| 2 | `sapi1v.sapi` | **Z80:** sestava V (JPR-1V, AND-1V, CGA-1V, MPH-1V se zvukem YM3812, IDE, RAM disk), ECMON a CP/M | ECMON, CP/M, MikroBIOS 3.0 |
| 3 | `mikromon.sapi` | Nejmenší sestava s MikroMonem L. Lasoty (JPR-1ZI, AND-1Z-89, EKL-1); do 2026-10-28 v Základu, autor: patří k Liborovým deskám | MikroMon 1.3 (L. Lasota) |

## 4. Hry a dema (novodobé)

SAPI-1 nebyl herní počítač (autor: „Hry na SAPI nikdy nebyly“; na disketách J. Bíby jsou jen textové hry CP/M jako
STARTREK.COM a MAZEWAR.COM, viz `docs/software/SAPIMAN_diskety.md`). Libor Lasota udělal moderní karty CGA-1V a
MPH-1V a dema. Autor převedl Flappy, Bad Apple!!, Bomberman, Prince of Persia a FXS4 a s kartou DPI-1L i BomberNet
Martina Matyáše. Hry jsou zábavný novodobý doplněk a běží i na skutečném SAPI-1 (sestava V, Manic Miner na JPR-1S
a BGP-1S). Pořadí: hry a dema na sestavě V, Prince of Persia, BomberNet, Manic Miner.

### 4a. Hry a dema na sestavě V

**`sapi1v_hry.sapi`** „SAPI-1 V: hry a dema“ (hotová 2026-10-17):
- sestava V s oběma grafikami, CGA-1V i DGD-1V, přepínanými přes MAP2 jako u Libora doma; bez IDE;
- RAM disk `media/zrmd-1/hry-a-dema.bin` z `tools/make_hry_disk.py`: obraz Bad Apple!!, ke kterému se přidají ostatní
  soubory. BADAPPLE.DAT zůstane na svém místě, volných 244 KB;
- všechno ověřené v emulátoru: obraz na CGA-1V, DGD-1V a AND-1V, zvuk SNDDEMONu.

| Program | Co to je | Odkud | Autor |
|---|---|---|---|
| BADAPPLE | demo Bad Apple!! (video a hudba YM3812) | `ba-ramdisk.bin` | port M. Lukášek |
| FXS4 | Fuxoft Soundtrack IV, hudební demo | `..\SAPI-FXS4\build` | F. Fuka, port M. Lukášek |
| FLAPPY | Flappy na CGA-1V | `..\SAPI-Flappy\build` | dB-SOFT 1984, port M. Lukášek |
| FLAPDGD | Flappy na DGD-1V (beta 26. 3. 2026) | `..\Games\flappy_dgd.hex` | port **L. Lasota** (titulek „2025 Libor L.A.“) |
| BOMBER | Bomberman | `..\SAPI-Bomberman\build` | Hudson Soft 1983, port M. Lukášek |
| BKP | textová hra Bratrstvo kočičí pracky | `..\Games\Bratrstvo\bkp.com` | R. Bořík (Tolkien 2017), verze pro SAPI 2026 **L. Lasota** („-LAILA-“) |
| Y1STDEMO | demo zvuku YM3812 na DGD-1V | `media/mph-1v` | **L. Lasota** |
| SNDDEMON | zvukové demo (jen zvuk) | `..\SAPI-karty\MPH-1V\snddemon.hex` | **L. Lasota** |
| CASTLE, REMBRAN1, REMBRAN2 | obrázky na CGA-1V (hrad, Rembrandt) | `media/cga-1v` | **L. Lasota** |
| CGATEST | test karty CGA-1V | `media/cga-1v` | **L. Lasota** |
| MPHTEST | test karty MPH-1V | `media/mph-1v` | **L. Lasota** |
| SPLAY + 11 skladeb VGM | přehrávač skladeb VGM (Hellfire, Tyrian, Monkey Island…) | `media/mph-1v`, `..\SAPI-karty\MPH-1V\SPLAY` | přehrávač a převody **L. Lasota**, hudba her (různí autoři) |
| RAT + 15 modulů RAD | přehrávač modulů RAD v1 | `media/mph-1v`, `..\SAPI-karty\MPH-1V\RATplayer` | přehrávač **L. Lasota** („-LAILA-“), moduly z 3eality.com |

`sapi1v_BadApple.sapi` a `sapi1v_DGD_Flapy.sapi` nová sestava nahradila, autor je 2026-10-17 smazal.
`ba-ramdisk.bin` zůstal jako základ pro `make_hry_disk.py`.

### 4b. Prince of Persia

`sapi1v_PoP.sapi` „SAPI-1 V: Prince of Persia“ je od 2026-10-28 v Hrách a demech, v betě i ve veřejném vydání, s
upozorněním pro nositele práv (`docs/vydani/souhlasy.md`), s verzí s grafikou a hudbou DOS (RAM disk `media/zrmd-1/pop-ramdisk.bin`
z `build\ramdisk_dos.bin` repa SAPI-Prince-of-Persia, větev `dos-graphics`). Dřív jen v repu s verzí 1.0.0
převedenou z Apple.

### 4c. BomberNet

`sapi1v_BomberNet.sapi` „SAPI-1 V: BomberNet (síťová hra)“ je od 2026-10-28 v Hrách a demech a ve vydání (autor:
„Herní, i ve vydání“). Herní sestava na ARB-1C-8 bez IDE-1: JPR-1V (TURBO), RAM-1V, AND-1V, DSM-1V, ZRD-1V, DPI-1L,
CGA-1V, MPH-1V. RAM disk `media/zrmd-1/bombernet.bin` (`tools/make_hry_disk.py --bombernet`) s `BOMBNET.COM` 0.3.0 z
`..\SAPI-BomberNet\build`, neukládá se, takže dvě instance na jednom PC si nepřekážejí. Do sítě se karta připojí až
po volbě HOST nebo JOIN ve hře. Autor 0.3.0 vyzkoušel 2026-10-28 na skutečném SAPI-1 i v emulátoru.

### 4d. Manic Miner na 8080

`jpr1s_bgp1s.sapi`: Manic Miner a hra „auto“ na grafice BGP-1S. Sestava je v Deskách Libora Lasoty a od 2026-10-28
i tady, s titulkem „Manic Miner (JPR-1S + BGP-1S)“.

## 5. Ostatní

XY 4130 a plotter z AR 1990 přešly 2026-10-28 do Přístrojů a technologie. RKD-1 s CM 5400 zůstává v Ostatních
(autor), mezi CP/M J. Bíby (5c).

### 5a. Další sestavy ZPS a jejich varianty

`zps/MIKROBASIC_AR1983`, `zps/ZPS-1_JPR-1S_AND-1B`, `zps/ZPS-1_JPR-1X`, `zps/ZPS-2_MIKOS_D`,
`zps/ZPS-2_MIKOS_V5_DGD-1V`, `zps/ZPS-3`, `zps/ZPS-3_CPM48K`, `zps/ZPS-3_CPM48K_FDC-1`, `zps/ZPS-3_CPM48K_IMA`,
`zps/ZPS-3_CPM62K`, `zps/ZPS-3_CPM62K_FDC-1`, `zps/ZPS-4_ELIS`, `zps/ZPS-4_FDC-1` (13 sestav).

### 5b. CP/M s ECMONem (RAM disk ZRMD-1)

`cpm_zrmd`, `cpm_ide`, `cpm_zrmd_fdc1`, `cpm_zrmd_mfc1a`, `cpm_zrmd_dpp1`, `cpm_zrmd_rtc_dsp`, `cpm_ec1045`,
`cpm_and30c` (8 sestav; `cpm_and30c` přibyla 2026-10-31 s průzkumem displeje AND-30C, autor: „pojal bych to trochu
jako průzkum“). Souhrny pro výběr sestav (co sestava je a kterou volbu po zapnutí stisknout) jsou v
`machines/_vyber.json` od 2026-10-29. `cpm_zrmd` a `cpm_ide` nemají technický popis v souboru sestavy, doplnit.

Krátké titulky (autor souhlasil 2026-10-29, ve stylu základních sestav; dlouhé jméno ze `.sapi` s kartami je ve
výběru pod titulkem, v angličtině z `name_en` v rejstříku). Ověřené proti kartám sestav a během bez okna (menu
ECMONu, `DIR` disků):

| Soubor | Titulek | Anglicky |
|---|---|---|
| `cpm_zrmd` | CP/M z RAM disku | CP/M from the RAM disk |
| `cpm_ide` | CP/M s IDE diskem a PMD32-SD | CP/M with an IDE disk and PMD32-SD |
| `cpm_zrmd_fdc1` | CP/M s disketami 8" (FDC-1) | CP/M with 8" floppies (FDC-1) |
| `cpm_zrmd_mfc1a` | CP/M s disketami 8" (MFC-1A) | CP/M with 8" floppies (MFC-1A) |
| `cpm_zrmd_dpp1` | CP/M s PMD32-SD přes DPP-1 | CP/M with PMD32-SD through DPP-1 |
| `cpm_zrmd_rtc_dsp` | CP/M s hodinami RTC-1 a sériovou deskou DSP-1 | CP/M with the RTC-1 clock and the DSP-1 serial board |
| `cpm_ec1045` | CP/M EC1045 s disketami 3,5" a 5,25" HD | CP/M EC1045 with 3.5" and 5.25" HD floppies |
| `cpm_and30c` | CP/M s displejem 80 znaků AND-30C | CP/M with the 80-column display AND-30C |

### 5c. CP/M J. Bíby (SAPIMAN)

`cpm_boot_fdc1`, `cpm_boot_fdc1_irmd`, `cpm_boot_fdc1_rmd`, `cpm_boot_fdc1_ss256`, `cpm_boot_mfc1a`,
`cpm_boot_mfc1a_rmd`, `cpm_boot_rpd1`, `cpm_boot_rpd1_rmd`, `cpm_boot_rpd1_ss256`, `cpm_boot_rpd1_fdc1_rmd`,
`cpm_boot_rpd1_mfc256`, `cpm_boot_rpd1_mfc512`, `cpm_boot_dlouhe_rpd1_fdc1`, `cpm_boot_dlouhe_rpd1_fdc1_35`,
`cpm_pmd_fdc1`, `cpm_pmd_rpd1`, `cpm_term64k_fdc1`, `rkd1_jb_cm5400` (RKD-1 s mechanikou CM 5400, BIOS dodělaný
v emulátoru) a `cpm_dse1_fdc1` (19 sestav). `cpm_dse1_fdc1` přibyla 2026-10-31 na přání autora: titulek „CP/M
s programátorem EPROM DSE-1“ (anglicky „CP/M with the DSE-1 EPROM programmer“), `cpm_boot_fdc1` s deskou DSE-1
v X7, D: = EPROM.COM J. Bíby (i 2708), E: = DSE.COM J. Hlaváče (2716, 2732), okno programátoru při startu.
Průvodce `zps/ZPS-2_DSE-1` na ni odkazuje.

### 5d. Další sestavy s Liborovými deskami

| Soubor | Co ukazuje |
|---|---|
| `jpr1x_bgp1s.sapi` | JPR-1X (80C85), MIKROBIOS a MIKROBASIC, deska diagnostiky |
| `libor_doma.sapi` | Liborova sestava doma: AND-1V, DGD-1V a CGA-1V přepínané přes MAP |

## Co je Liborovo a kde to ve vydání je (k potvrzení souhlasu)

| Co | Kde ve vydání |
|---|---|
| MikroMon 1.3 | `roms/mikromon/`, sestava `mikromon` (Liborovy desky) |
| MIKROBIOS 1.0, 1.02, 2.1; MikroBIOS 3.0 (JPR-1V); MIKROBIOS + MIKROBASIC (JPR-1X) | `roms/jpr-1s/`, `roms/jpr-1v/`, `roms/jpr-1x/`; sestavy `jpr1s_bgp1s`, `sapi1v`, `sapi1v_hry`, `jpr1x_bgp1s`, `libor_doma`, `zps/ZPS-1_JPR-1S_AND-1B`, `zps/ZPS-1_JPR-1X` |
| fonty AND-1B a AND-1V | `roms/and-1b/`, `roms/and-1v/` |
| programy pro JPR-1S a BGP-1S (hra „auto“ a další, Manic Miner v jeho převodu z PMD-85) | `media/jpr-1s/` |
| dema a testy CGA-1V (CASTLE, REMBRAN1, REMBRAN2, CGATEST) | `media/cga-1v/` a RAM disk `hry-a-dema.bin` |
| programy MPH-1V (MPHTEST, SPLAY, RAT, Y1STDEMO) | `media/mph-1v/` a RAM disk `hry-a-dema.bin` |
| SNDDEMON | jen RAM disk `hry-a-dema.bin` (z `..\SAPI-karty\MPH-1V`) |
| skladby VGM převedené pro SPLAY (11) a moduly RAD pro RAT (15) | jen RAM disk `hry-a-dema.bin` |
| Flappy pro DGD-1V (FLAPDGD) | jen RAM disk `hry-a-dema.bin` (z `..\Games\flappy_dgd.hex`) |
| BKP, verze pro SAPI („-LAILA-“ = L. Lasota) | jen RAM disk `hry-a-dema.bin` (z `..\Games\Bratrstvo`) |

## Rozhodnuto (autor, 2026-10-17)

- `sapi1v_BadApple` a `sapi1v_DGD_Flapy` smazané, nahradila je `sapi1v_hry`.
- „-LAILA-“ je L. Lasota (BKP, RAT).
- Hudba: skladby VGM i moduly RAD zůstanou.
- ~~Otevřené: kdo upravil Manic Miner pro BGP-1S.~~ Libor Lasota (autor 2026-10-30).

## Rozhodnuto (autor, 2026-10-28)

- Kategorie podle historie SAPI-1 v pořadí Základ → Přístroje a technologie → Desky Libora Lasoty (moderní) → Hry
  a dema (novodobé) → Ostatní, nahoře úvod o průmyslovém SAPI-1. Skupina „Začněte zde“ s Prince of Persia
  z hodnocení GUI odpadá.
- Přístroje a technologie jsou samostatná kategorie s průvodci, přešly do ní XY 4130 a plotter z AR 1990. RKD-1
  s CM 5400 zůstává v Ostatních.
- Manic Miner (`jpr1s_bgp1s`) je v Deskách Libora Lasoty i v Hrách a demech.
- MikroMon patří do Desek Libora Lasoty, Základ jsou jen historické sestavy Tesla.
