# AND-1A – alfanumerický displej pro ZPS-3

Stav: **emulovaná** (typ `AND-1A` třídy AND-1 v `src/cards/and1.*`), ověřeno v ZPS-3.

## Podklady

- **Návod (OCR):** `BKP\and-1a.php`, obsahuje tabulku adres, tabulku znaků, konektory a obě PROM.
- **Schéma:**
  - překreslené: `BKP\pdf\and-1a.pdf`;
  - originální sken: `D11\sapi1zps3.zip`, složka `6xk199137`.
- **Tabulky propojek:** `BKP\obrazky\andpriloha-d1a.png`, `andpriloha-d2.png`. Fotky propojek
  `H:\SAPI-1\Foto\Jiri_Slansky\Test_Prog\SAPI-1 ANK\*AND-1A*.JPG`.
- **Ovladače:** `...\SAPI1_ARCHIV\ze SAPI\zdrojemercl\and1a.txt`,
  `H:\SAPI-1\LiborLasota\JPR-1S\jpr1s_mikrobios\test_and1a.asm`.

## Zapojení

- **Obraz stejný jako AND-1** (`docs/desky/AND-1.md`):
  - 40 × 24 znaků (nebo 20 řádků), matice 5 × 7, 6 MHz, řádek 64 µs, 320 řádků;
  - INT1 na konci snímku přes propojku 49-50;
  - bajt: D7–D6 režim (normální, blikání, blikající podtržení, dvojitá šířka), D5–D0 kód znaku;
  - znakový generátor MHB 2501/2502;
  - PROM D3C 74188 je shodná s AND-1 (`roms/and-1/AND-1_D3C_74188.bin`).
- **Rozdíl je jen v dekodéru adresy D3A:** 74S287 místo 3205, krok 2 KB.
  - Výchozí okno E800–EFFF (ZPS-3).
  - Propojka 44-45 (S1) sepnutá dává 3800 (MIKROBASIC, MIKOS).
  - Řádek r začíná na základ + r · 40h jako u AND-1, 24 řádků tedy končí na EDE7.
- **PROM D3A:** Intel HEX v `and-1a.php`.
  - Vstupy A0..A5 = A15, A13, S1, A14, A11, A12; A6/A7 pevně; O4 = /SEL.
  - Položky DFh → E800 (S1 rozpojená), F2h → 3800 (S1 sepnutá).
  - Funguje to jen s A6/A7 na **log. 1**. Originální sken ukazuje +5 V, EAGLE překreslení GND, což je
    nejspíš chyba překreslení.
- **Domácí varianta 3800/F800** (FFh → F800) pro CP/M 62K:
  `D11\..\SAPIMAN\CPM_SYSTÉMY\ZMENA_CPM\FUNKCNI\JEN 8 PALCU\FDC\FDC_AND1A130711_RMD\dekoder AND1ACPM.txt`.
- **Varianta 3800/F000** (EFh → F000) pro CP/M 64K Tesla IMA (`videom equ 0F000h` v MON64C3):
  `roms/and-1a/AND-1A_D3A_CPM64K_74S287.bin`. **Předpoklad** (standardní obsah s posunutou položkou), originál
  nemáme.

## Emulace

- Stejný obraz, font a režimy jako AND-1. Video RAM 1 KB se stejným skládáním sloupců 32–63 jako u AND-1.
- **Parametry:** `charset`, `rows`, `int1`, `s1` (propojka 44-45), `decoder` (PROM D3A, výchozí
  `roms/and-1a/AND-1A_D3A_74S287.bin`), `monitor` (barva luminoforu monitoru).
- Výběr 2 KB bloků se počítá z PROM: vstupy A15, A13, S1 (sepnutá = 0), A14, A11, A12, A6/A7 = 1, výstup D3 =
  /SEL. Standardní obsah dává E800, s S1 sepnutou 3800.
- U AND-1 (typ `AND-1`) zůstává pevně 3800–3FFF (dekodér 3205).

## Nejasnosti

- Dump MHB 2501 (stejná mezera jako u AND-1, font je překreslený).
- Video RAM 1 KB, nebo 2 KB? Návod píše 2 KB, rozpiska má 2× 2114 (1 KB) a parametry 1 KB. Co se čte
  z E800–EFFF mimo viditelné buňky?
- PROM D3A pro F000, kterou potřebuje monitor IMA 64K (`videom equ 0F000h` v MON64C3), a pro F800 (CP/M 62K).
  Emulace má pro obě předpoklad (`AND-1A_D3A_CPM64K_74S287.bin`, `…_CPM62K_…`).
- „EB00H–EFFFH“ v návodu DGD-1 je nejspíš chyba OCR místo E800H.
