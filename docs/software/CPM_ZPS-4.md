# CP/M pro ZPS-4 (JPR-1Z, RAM-1Z, AND-1Z, RPD-1, RMD-1)

**Stav (2026-10-04): ZPS-4 běží.**
- **`machines/zps/ZPS-4.sapi` (Peřina, MOR62Z + `media/rpd-1/CPM22P_62Z_Perina.img`):** `SAPI-1 ZPS4-62K`,
  `CPM V2.2 V/P1`, `A>`. RTEST inicializuje RAM disk E: („RAM DISK READY“), SAVE na E: funguje.
- **`machines/zps/ZPS-4_ELIS.sapi` (ELIS v7.01 + `media/rpd-1/ELIS_61K.img`):** „Dobrý den přeje ELIS 62k MON-č
  R62Z v7.01“, 61k CP/M V2.2. DIR, PIP a RAM disk C: po INIRMD5 (volba 2) fungují.
- **ELIS v3.00** systém zavede, ale s Consulem 262.3 bere každou klávesu dvakrát (STROBE jako pulz) nebo
  donekonečna (STROBE držený úpravou 7474). Jak klávesnici obsluhuje, zatím nevím. Výchozí je proto v7.01.
- Test `tests/test_zps4.cpp`.

Desky: `docs/desky/JPR-1Z.md`, `RAM-1Z.md`, `RMD-1.md`, `AND-1Z-89.md`.

## Sestava (`BKP\doku2\zps.php`, originální dokumentace ZPS-4 není k dispozici)

- **Desky:** JPR-1Z, RAM-1Z, AND-1Z, DSM-1, RPD-1 (A + B, KB-17), RMD-1 („pro CP/M není potřeba“), ARB-1,
  JZS-1A, ZDR-1A, SPN-1, kabely KB-06A a KB-13 (Consul 259.11). Sloty, propojky ani mapa paměti nejsou uvedené.
- **Odvozená paměť:** boot EPROM 0000–07FF (čtení), RAM-1Z 0000–F7FF (62 KB), AND-1Z F800–FFFF.
- **Odvozené porty:** 00, 01–03 (JPR-1Z), 10–13 (DSM-1), 20–25 a 3C–3F (RPD-1), 40–43 a 45 (RMD-1),
  případně DPP na 08–0B (MOR) nebo 28–2F (JB).

## Systémy

1. **Peřina CP/M 2.2/P (1988):**
   - ROM MOR62Z (`BKP\data\mor62z_hex.php`, 2028 B, „SAPI-1 ZPS4-62K“);
   - disketa `BKP\data2\cpm62z_img.rar` (77 × 26 × 128, skew 6, OFF 2, bootovatelná, od CPM62A se liší
     2 bajty);
   - podle CPM.DOC je základní verze ZPS-4 „CPM62A/Z + MOR62Z, JPR-1Z, AND-1Z“;
   - AND-1Z na F800 s 20 řádky, monitor na F000;
   - RAM disk RMD-1 je E: (porty 40–45), `RSYS62A.COM` ho dá na A:, `RTEST.COM` ho inicializuje.
   - Varianty 48Z jsou pro JPR-1A.
2. **ELIS** (nejbližší dobový ZPS-4 s RMD-1):
   - disketa `BKP\data2\elis_img.rar` = `...\SD_EC1045_2019-01-31\MELSOFT\001.8SD`;
   - hlášky „ELIS 61k CP/M V2.2; A,B: RPD-1 8"; C: RAM-DISK Rmd-1; AND-1Z; Consul 259.11; LST D-100/P“;
   - BIOS v kódu Z80;
   - dvě ROM: v3.00 (`elis_hex.php`) a v7.01 (`JPR-1Z_BIOS_ELIS.BIN`), obě se kopírují na F000.
3. **JB (1987, zdroje 2011):**
   - ROM „JPR-1Z S AND-1Z CESTINA V131087JB“ (`MO2711.MAC`) a BIOS V1310JB s RMD-1 až 1 MB na 40h;
   - diskety v `...\IMAGE_8\RPD\` a `DROPBOX\SYSPROG\a_sys_rpd.8sd`.

**Nepatří k ZPS-4:** `cpm64k_img` 001–003 jsou IMA „JPR-1A 60K CPM V2.2“ s MON64C3 (JPR-1A, video na F000).

## Chybí

- Originální boot EPROM Tesla IMA pro ZPS-4 a systémová disketa.
