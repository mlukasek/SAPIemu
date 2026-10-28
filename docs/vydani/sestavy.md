# Sestavy pro veřejné vydání: rozřazení

Návrh z 2026-10-17, upravený podle autora týž den. Ve vydání zůstanou všechny sestavy kromě Prince of Persia. V
prohlížeči sestav (připravuje se) budou rozdělené do kategorií, aby nováček začal u základu a her a ostatní našel
v sekci Ostatní. Po prvním spuštění se otevře výběr sestav. Software a jeho majitele shrnuje
`docs/vydani/souhlasy.md`.

Zkratky v tabulkách:

- **JB:** JPR-1ZI, DMR-1, AND-1Z-89, klávesnice Consul 259.11, tiskárna.
- **EC:** totéž s ECMONem a klávesnicí EKL-1.
- **V:** Liborovy desky V (JPR-1V, RAM-1V, AND-1V, DSM-1V, ZRD-1V, IDE-1, MPH-1V, CGA-1V).

## 1. Základ: historické sestavy Tesla

Původní sestavy ZPS v pořadí, jak šly po sobě. Pro každou bude náhled a krátký průvodce „co vyzkoušet“.

| Pořadí | Soubor | Co ukazuje | Software |
|---|---|---|---|
| 1 | `zps/ZPS-1.sapi` | ZPS-1: JPR-1 8080, AND-1, ANK-1, kazeta | MIKROBASIC 2.4 (Tesla) |
| 2 | `zps/ZPS-2.sapi` | ZPS-2: MIKOS V4.0F, kazeta | MIKOS V4 (Tesla ELTOS) |
| 3 | `zps/ZPS-2_MIKOS_V5.sapi` | ZPS-2 s MIKOSem V5 a grafikou DGD-1 | MIKOS V5 (Tesla DIZ), BASIC EXP z `media/mikos` |
| 4 | `zps/ZPS-3_CPM64K_IMA.sapi` | ZPS-3: JPR-1A, AND-1A, RPD-1, původní CP/M 64K Tesla IMA | CP/M 2.2 Tesla IMA, MBASIC na disketě |
| 5 | `zps/ZPS-4.sapi` | ZPS-4: JPR-1Z, AND-1Z, RMD-1, CP/M 62K | CP/M 2.2/P R. Peřiny (MOR62Z), podle CPM.DOC základní verze ZPS-4 |
| 6 | `zps/ZPS-6.sapi` | SAPI-1.5 (ZPS-6): PKT-1, DBS-1A, MFC-1A, CP/M 62K | CP/M Melsoft 1990 |

## 2. Liborovy desky

| Pořadí | Soubor | Co ukazuje | Software |
|---|---|---|---|
| 1 | `jpr1s_bgp1s.sapi` | **8080:** JPR-1S a grafika BGP-1S (jako PMD-85), klávesnice ANK-1M2 s joystickem | MIKROBIOS 1.02 (L. Lasota), hry z `media/jpr-1s` (Manic Miner…) |
| 2 | `sapi1v.sapi` | **Z80:** sestava V (JPR-1V, AND-1V, CGA-1V, MPH-1V se zvukem YM3812, IDE, RAM disk), ECMON a CP/M | ECMON, CP/M, MikroBIOS 3.0 |
| 3 | `mikromon.sapi` | Nejmenší sestava s MikroMonem L. Lasoty (JPR-1ZI, AND-1Z-89, EKL-1); do 2026-10-28 v Základu, autor: patří k Liborovým deskám | MikroMon 1.3 (L. Lasota) |

## 3. Hry a dema

### 3a. Hry a dema na sestavě V

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

### 3b. Hry na 8080

`jpr1s_bgp1s.sapi`: Manic Miner a hra „auto“ na grafice BGP-1S (odkaz na sestavu z oddílu 2).

### 3c. Přístroje jako ukázky hardwaru

Od každého druhu jeden, hlavně ty s nakresleným přístrojem v okně:

| Soubor | Co ukazuje | Okno |
|---|---|---|
| `aps5.sapi` | měřicí jednotka APS-5 s regulací pecí (kotelna), původní firmware Tesla, tlačítko DEMO | panel HRK-1 a pece |
| `zps/ZPS-2_DPS-1_IMS-2.sapi` | DPS-1, sběrnice IMS-2, voltmetr M1T 330 a zdroj BS 525 (knoflíky se točí myší), BASIMS | měřicí pracoviště |
| `zps/ZPS-2_MIKOS_K.sapi` | kazetopáskový záznamník dat KZD-1, MIKOS/K | Média (mechanika) |
| `zps/ZPS-2_MIKOS_V5_Minigraf.sapi` | zapisovač Minigraf 0507, ukázka z IQ 151, `KRES` | zapisovač |
| `rkd1_tesla_cpm.sapi` | kazetový disk RKD-1, CP/M Tesla Elstroj 1987 | Média (kazetový disk) |
| `rmp1_jb_izot.sapi` | magnetická páska RMP-1 a IZOT SM 5300 | Média (páskové jednotky) |

### 3d. Prince of Persia

`sapi1v_PoP.sapi` „SAPI-1 V: Prince of Persia“ je od 2026-10-28 v Hrách a demech a v betě pro testery (autor: „dej to i do
vydání, tady bych se toho nebál“; veřejné vydání zatím otevřené, „poptám se, jak to řeší kolegové“), s verzí s grafikou a hudbou DOS (RAM disk `media/zrmd-1/pop-ramdisk.bin`
z `build\ramdisk_dos.bin` repa SAPI-Prince-of-Persia, větev `dos-graphics`). Dřív jen v repu s verzí 1.0.0
převedenou z Apple.

### 3e. BomberNet

`sapi1v_BomberNet.sapi` „SAPI-1 V: BomberNet (síťová hra)“ je od 2026-10-28 v Hrách a demech a ve vydání (autor:
„Herní, i ve vydání“). Herní sestava na ARB-1C-8 bez IDE-1: JPR-1V (TURBO), RAM-1V, AND-1V, DSM-1V, ZRD-1V, DPI-1L,
CGA-1V, MPH-1V. RAM disk `media/zrmd-1/bombernet.bin` (`tools/make_hry_disk.py --bombernet`) s `BOMBNET.COM` 0.3.0 z
`..\SAPI-BomberNet\build`, neukládá se, takže dvě instance na jednom PC si nepřekážejí. Do sítě se karta připojí až
po volbě HOST nebo JOIN ve hře.

## 4. Ostatní

### 4a. Další přístroje a periferie

| Soubor | Co ukazuje |
|---|---|
| `zps/ZPS-2_MIKOS_V5_XY4130.sapi` | grafická jednotka XY 4130 |
| `plotter_ar1990.sapi` | plotter z Amatérského radia 1990 (JPR-1 jako řadič Minigrafu) |
| `rkd1_jb_cm5400.sapi` | RKD-1 s mechanikou CM 5400, CP/M J. Bíby (BIOS dodělaný v emulátoru) |

### 4b. Další sestavy ZPS a jejich varianty

`zps/MIKROBASIC_AR1983`, `zps/ZPS-1_JPR-1S_AND-1B`, `zps/ZPS-1_JPR-1X`, `zps/ZPS-2_MIKOS_D`,
`zps/ZPS-2_MIKOS_V5_DGD-1V`, `zps/ZPS-3`, `zps/ZPS-3_CPM48K`, `zps/ZPS-3_CPM48K_FDC-1`, `zps/ZPS-3_CPM48K_IMA`,
`zps/ZPS-3_CPM62K`, `zps/ZPS-3_CPM62K_FDC-1`, `zps/ZPS-4_ELIS`, `zps/ZPS-4_FDC-1` (13 sestav).

### 4c. CP/M s ECMONem (RAM disk ZRMD-1)

`cpm_zrmd`, `cpm_ide`, `cpm_zrmd_fdc1`, `cpm_zrmd_mfc1a`, `cpm_zrmd_dpp1`, `cpm_zrmd_rtc_dsp`, `cpm_ec1045`
(7 sestav). `cpm_zrmd` a `cpm_ide` nemají popis, doplnit.

### 4d. CP/M J. Bíby (SAPIMAN)

`cpm_boot_fdc1`, `cpm_boot_fdc1_irmd`, `cpm_boot_fdc1_rmd`, `cpm_boot_fdc1_ss256`, `cpm_boot_mfc1a`,
`cpm_boot_mfc1a_rmd`, `cpm_boot_rpd1`, `cpm_boot_rpd1_rmd`, `cpm_boot_rpd1_ss256`, `cpm_boot_rpd1_fdc1_rmd`,
`cpm_boot_rpd1_mfc256`, `cpm_boot_rpd1_mfc512`, `cpm_boot_dlouhe_rpd1_fdc1`, `cpm_boot_dlouhe_rpd1_fdc1_35`,
`cpm_pmd_fdc1`, `cpm_pmd_rpd1`, `cpm_term64k_fdc1` (17 sestav).

### 4e. Další sestavy s Liborovými deskami

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
| programy pro JPR-1S a BGP-1S (hra „auto“ a další, Manic Miner v jeho úpravě?) | `media/jpr-1s/` |
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
- Otevřené: kdo upravil Manic Miner pro BGP-1S.
