# Sestavy pro veřejné vydání: návrh rozřazení

Návrh k odsouhlasení (2026-10-17). Ve vydání zůstanou všechny sestavy kromě Prince of Persia. V prohlížeči
sestav (připravuje se) budou rozdělené do kategorií, aby nováček začal u základu a her a ostatní našel v sekci
Ostatní. Software a jeho majitele shrnuje `docs/vydani/souhlasy.md`.

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
| 5 | `zps/ZPS-4.sapi` | ZPS-4: JPR-1Z, AND-1Z, RMD-1, CP/M 62K | CP/M R. Peřiny (MOR62Z) |
| 6 | `zps/ZPS-6.sapi` | SAPI-1.5 (ZPS-6): PKT-1, DBS-1A, MFC-1A, CP/M 62K | CP/M Melsoft 1990 |
| 7 | `mikromon.sapi` | Nejmenší sestava s MikroMonem L. Lasoty (JPR-1ZI, AND-1Z-89, EKL-1) | MikroMon 1.3 (L. Lasota) |

## 2. Liborovy desky

| Pořadí | Soubor | Co ukazuje | Software |
|---|---|---|---|
| 1 | `jpr1s_bgp1s.sapi` | **8080:** JPR-1S a grafika BGP-1S (jako PMD-85), klávesnice ANK-1M2 s joystickem | MIKROBIOS 1.02 (L. Lasota), hry z `media/jpr-1s` (Manic Miner…) |
| 2 | `sapi1v.sapi` | **Z80:** sestava V (JPR-1V, AND-1V, CGA-1V, MPH-1V se zvukem YM3812, IDE, RAM disk), ECMON a CP/M | ECMON, CP/M, MikroBIOS 3.0, programy na disku C: |

## 3. Hry a dema

| Pořadí | Soubor | Obsah | Poznámka |
|---|---|---|---|
| 1 | **nová** `sapi1v_hry.sapi` | „SAPI-1 V: hry a dema“: CGA-1V i DGD-1V; RAM disk FXS4, Flappy (CGA), Bomberman, BKP; vydávací IDE disk: FLAPDGD (DGD-1V), SPLAY a RAT s hudbou, MPH-TEST, Y1STDEMO, SNDDEMON, obrázky CGA-1V | nahradí `sapi1v_DGD_Flapy` (ta půjde do Ostatních nebo pryč) |
| 2 | `sapi1v_BadApple.sapi` | demo Bad Apple!! (video 3,1 MB z RAM disku, hudba na YM3812) | vlastní RAM disk |
| 3 | `jpr1s_bgp1s.sapi` | Manic Miner a hra „auto“ na BGP-1S (odkaz na sestavu z oddílu 2) | jen odkaz, ne druhý soubor |
| – | `sapi1v_PoP.sapi` | Prince of Persia | **jen v repu**, do vydání ne |

`sapi1v_BomberNet.sapi` (karta DPI-1L, síťová hra) zatím do Ostatních. Do her přijde, až bude BomberNet pro SAPI
ve vydání.

## 4. Ostatní

### 4a. Přístroje a periferie

| Soubor | Co ukazuje |
|---|---|
| `aps5.sapi` | měřicí jednotka APS-5 s regulací pecí (původní firmware Tesla) |
| `zps/ZPS-2_DPS-1_IMS-2.sapi` | DPS-1, sběrnice IMS-2, voltmetr M1T 330 a zdroj BS 525, BASIMS |
| `zps/ZPS-2_MIKOS_K.sapi` | kazetopáskový záznamník dat KZD-1, MIKOS/K |
| `zps/ZPS-2_MIKOS_V5_Minigraf.sapi` | zapisovač ARITMA Minigraf 0507 |
| `zps/ZPS-2_MIKOS_V5_XY4130.sapi` | grafická jednotka XY 4130 |
| `plotter_ar1990.sapi` | plotter z Amatérského radia 1990 (JPR-1 jako řadič Minigrafu) |
| `rkd1_tesla_cpm.sapi` | kazetový disk RKD-1, CP/M Tesla Elstroj 1987 |
| `rkd1_jb_cm5400.sapi` | RKD-1 s mechanikou CM 5400, CP/M J. Bíby (BIOS dodělaný v emulátoru) |
| `rmp1_jb_izot.sapi` | magnetická páska RMP-1 a IZOT SM 5300, CP/M J. Bíby |

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
| `sapi1v_BomberNet.sapi` | sestava V s kartou DPI-1L pro síťový BomberNet |
| `sapi1v_DGD_Flapy.sapi` | sestava V s DGD-1V pro Flappy (po vzniku `sapi1v_hry` zrušit, nebo nechat) |

Celkem: 7 + 2 + 2 (+ nová) + 9 + 13 + 7 + 17 + 4 = 61 sestav, z toho `sapi1v_PoP` jen v repu.

## Otázky k rozhodnutí

1. **Základ:** souhlasí výběr sedmi sestav? Chybí ZPS-3 s CP/M 48K Peřiny nebo ZPS-4 s ELISem?
2. **Liborova 8080:** `jpr1s_bgp1s` (grafika BGP-1S a hry), nebo `zps/ZPS-1_JPR-1S_AND-1B` (MIKROBASIC
   s AND-1B), případně `jpr1x_bgp1s`?
3. **Herní sestava:**
   - jedna s CGA-1V i DGD-1V (jako `libor_doma`) a co na RAM disk a IDE disk;
   - co je BKP na RAM disku se hrami;
   - mají tam být i Liborova dema z `media/cga-1v` a `media/mph-1v` (SPLAY, RAT, Y1STDEMO, MPH-TEST, obrázky)?
4. **`sapi1v_DGD_Flapy`:** zrušit, až bude herní sestava, nebo nechat v Ostatních?
5. **Výchozí sestava po prvním spuštění:** prohlížeč sestav (doporučeno), nebo rovnou některá ze základu?
