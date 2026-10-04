# CP/M a monitory pro ZPS-3 (JPR-1A + RPD-1)

**Stav (2026-10-03): ZPS-3 běží.** Sestava `machines/zps/ZPS-3.sapi`:
- **MON V1.0-C1 zavede z diskety Peřinův CP/M 2.2/P** (`media/rpd-1/CPM22P_48A_Perina.img`): banner
  `JPR-1A 48K MON V1.0-C1`, `CPM V2.2 V/P1`, `A>`.
- DIR, TYPE, SAVE a ERA fungují. Totéž s ROM MOR48A (`SAPI-1 ZPS3-48K`).
- S připojeným terminálem na DSM-1 (DCD) jde konzole na sériovou linku.
- AND-1A zobrazuje jen velká písmena a české znaky KOI-8 ukazuje jako jiné znaky MHB 2501 (např. W místo
  É). Tak to vypadá i na skutečném stroji.
- Test `tests/test_zps3.cpp`.

Desky: `docs/desky/JPR-1A.md`, `AND-1A.md`, `RAM-1.md`.

## Sestava ZPS-3 (manuál `BKP\pdf\zps3.pdf`, příloha 2 a 3)

- **Sloty:** X1 JPR-1A, X2 RAM-1A, X3 AND-1A, X4 DSM-1, X5 RPD-1A, X6 RPD-1B (kabel KB-17), X7/X8 volné.
  Dvě mechaniky JPD-1 8", klávesnice Consul 259.11, tiskárna Consul 2111.
- **Porty:** 00 JPR-1A (zápis = přepnutí ROM/RAM), 01–03 JPR-1A P0–P2, 10–13 DSM-1, 20–25 RPD-1A, 3C–3F RPD-1B.
  MOR* používá ještě 08–0B (tiskárna na DPP).
- **Paměť:**
  - po RESET: EPROM JPR-1A 0000–0FFF (jen čtení), RAM-1A 1000–BFFF, AND-1A E800–EFFF;
  - po `OUT 00h`: RAM-1A 0000–BFFF, AND-1A E800–EFFF;
  - s terminálem na DSM-1 smí být E800–EFFF RAM, ale pak nesmí být osazená AND-1A.

## Monitory (EPROM JPR-1A)

Tabulka je v `docs/desky/JPR-1A.md`. Standard je **MON V1.0-C1** (`B_V10_JPR-1A_RPD-1_ZPS-3.BIN`, zdroj
`MON48C1.MAC`).

## Obrazy disket (vše 256 256 B = 77 × 26 × 128)

- **cpm48a, cpm62a** (`BKP\data2\cpm48a_img.rar`, `cpm62a_img.rar`): Peřina CP/M 2.2/P pro RPD-1, bootovatelné,
  k ROM MOR48A/MOR62A. Na disketě jsou BIOS.MAC, BOOT.MAC, MOR24.MAC a CPM.DOC.
  - 48 = 48K, AND-1A na E800, monitor na B800.
  - 62 = 62K, AND-1A na F800, monitor na F000. Potřebuje dvě RAM-1A a nestandardní D3A.
- **Písmena:** `a` = AND-1A, `z` = AND-1Z, `f` = řadič FDC-1 místo RPD-1 (cpm48af, cpm62af).
- **cpmzps3_003/004.IMG** (`cpmzps3_img.rar`): navzájem shodné, systémové stopy vyplněné 40h, **nebootovatelné**.
  Soubory čtvrté diskety jsou v `cpmzps3_soubory.rar` (BIOS48C1.MAC, BOOT0C1/1C1.MAC, MON48C1.MAC,
  MOVCPM48.COM).
- **Originální Tesla IMA CP/M 64K, bootovatelný** („CCP V2.2-A1 TESLA ELTOS IMA PARDUBICE 15/11/1984“):
  `H:\SAPI-1\Petr_repliky_k_vyrobe\PMD32-SD\SD_EC1045_2019-01-31\DEX\SAPI_ZPS3\01–03.8SD`. Volá F821, patří
  k `JPR1A_BOOT_64k`.
- **48K zdrojové diskety** bez systémových stop: `...\IMAGE_8\ZDROJ_STARE\15_a/b_zps3_48k.8sd`.
- `BKP\data\bios_lst.php` je BIOS MIKOS/D, ne CP/M ZPS-3. Zdroje CP/M BIOS: BIOS48C1.MAC, BIOS64C2.MAC,
  BIOS.MAC.

## Chybí

- **Bootovatelná originální systémová disketa IMA 48K ZPS-3** k MON V1.0-C1. Zdroje máme, sestavenou disketu ne.
