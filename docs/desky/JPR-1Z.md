# JPR-1Z – procesorová deska Z80 pro ZPS-4

Stav: **emulovaná** jako typ `JPR-1Z` třídy JPR-1ZI (`src/cards/jpr1zi.*`), ověřeno v ZPS-4 (`tests/test_zps4.cpp`).
JPR-1Z je předchůdce JPR-1ZI (`docs/desky/JPR-1ZI.md`).

- EPROM 2716 se zrcadlí po 2 KB.
- Dekodéry jsou dumpy `roms/jpr-1z/JPR-1Z_D14_74S287.bin` (paměť) a `JPR-1Z_D10_74S287.bin` (I/O). D14 má
  proti D3C (2k) jinou polaritu nepoužitých bitů a jiný obsah v polovině s A7 = 1, která je uzemněná. Pro
  používané adresy vybírá stejně.

`BKP` = web `https://sapi.cz/sapi/…` ([přehled desek](https://sapi.cz/sapi/desky.php)), `PR` = podklady Petra Všijanského k výrobě replik (archiv autora).

## Podklady

- **Návod Tesla (OCR):** `BKP\jpr-1z.php`, obsahuje tabulky PROM, zapojení X1–X3 a zapojení Consul 259.11/262.3.
- **Schéma:**
  - překreslené EC1045 2011: `BKP\pdf\jpr-1z.pdf` = `PR\JPR-1ZI\JPR-1Z.pdf`;
  - zapojení konektorů: `PR\JPR-1ZI\JPR-1Z_*_connector_pinout.txt`.
- **Skeny originálního návodu:** sedm stran od J. Slanského (archiv autora).

## Rozdíly proti JPR-1ZI (podle schématu)

- **EPROM 2716 (2 KB)** místo 2764.
- **Bez obvodu XINTA** (pomocné obvody pro 8259). /INT0 → Z80 /INT, INT1 → /NMI.
- **Takt:** stejný jako u ZI (krystal 16 MHz, 74LS93, JP2/JP3), standard 2 MHz CPU i sběrnice. Návod připouští
  i krystal 10 MHz s 7490 (÷5) nebo 8 MHz (÷4). Petrova skutečná deska má 8 MHz
  (`PR\JPR-1ZI\JPR-1Z_PROM_Petr.txt`).
- **Dekodér paměti D14** (= ZI D3C), zapojení stejné jako u ZI:
  - A0 = A13, A1 = A12, A2 = GND, A3 = A14, A4 = A15, A5 = A11, A6 = BOOT, A7 = GND, CE = /AEN.
  - Tabulka Tesla funkčně odpovídá `roms/jpr-1zi/JPR-1ZI_D3C_2k.bin` (významný je jen bit 3, ROM/RAM).
  - Dumpy: `PR\JPR-1ZI\JPR-1Z_PROM_D14.hex`, `JPR1Z-D14.287` a dump skutečné desky ELIS
    `JPR-1Z_ELIS_PROM-D14-00/FF.HEX`. Všechny jsou rovnocenné.
- **Dekodér I/O D10** (= ZI D6B): 07 0B 0D 0E, shodný s `JPR-1ZI_D6B.bin` (`PR\JPR-1ZI\JPR-1Z_PROM_D10.hex`).
- **Porty:** 00 zápis = vypnutí BOOT, 01–03 = P0–P2 (3212). Zápisy jdou i během BOOT do RAM na sběrnici.
- **Klávesnice Consul 259.11:**
  - data /D1–/D7 na P1-IN0–6 (port 02), /CLR na P1-IN7;
  - /SC na P0-IN0, /AC na P0-OUT0;
  - /ZVUK na X2 pin 14 = P0-OUT1. Tabulka v návodu uvádí dvakrát P0-OUT0, to je překlep.
- **Mapování:** X1 piny 21/22 (MAP1/MAP2) jsou prázdné. ZPS-4 mapování nepoužívá.

## ROM pro JPR-1Z (2716)

| ROM | Soubor | Poznámka |
|---|---|---|
| MOR62Z (Peřina 1988) | `BKP\data\mor62z_hex.php` | „SAPI-1 ZPS4-62K“, CP/M 2.2/P, zdroj MOR24.MAC (ZPS4 EQU) |
| „SAPI-1 ZPS4-62K *ZF“ | `1Z.BIN` od J. Slanského (archiv autora) | varianta MOR62Z |
| ELIS v3.00 | `BKP\data\elis_hex.php` | kód 8080, text „C262.3“ |
| ELIS v7.01 | `PR\JPR-1ZI\JPR-1Z_BIOS_ELIS.BIN` | kód Z80, „C259.11/C262.3 + K6313“ |
| JB „ROM JPR-1Z S AND-1Z CESTINA V131087JB“ | zdroj `MO2711.MAC/.HEX` na `...\IMAGE_8\RPD\60_r1_rmd1m_271111.8sd` | RPD-1 + RMD-1/1MB + AND-1Z + C259.11 |

Originální boot EPROM Tesla IMA pro ZPS-4 se nenašla.

## Nejasnosti

- Originální boot EPROM ZPS-4 (Tesla IMA) a systémová disketa.
- **Prázdná sběrnice:** od 2026-10-14 poslední bajt datové sběrnice jako JPR-1V, předběžně podle paměti autora
  (parametr `open_bus`, `ff` = dřívější chování). Změřit.
- **Čip Z80: NMOS, nebo CMOS?** Parametr `z80` (výchozí `nmos`, v ZPS-4 nejspíš NMOS U880 nebo Z80A). Viz JPR-1V.
- **STSTB při INTA a RESET jen s /M1** (L. Lasota 2026-10-10 píše o JPR-1Z/1V): dává JPR-1Z STSTB i od /IORQ
  jako JPR-1V a je RESET synchronizovaný s /M1? Záplata pro refresh na JPR-1Z podle Libora je. Popis v
  `JPR-1V.md`. U JPR-1V změřeno a emulováno (2026-10-11), JPR-1Z v emulaci zatím ne.
- Která ROM ELIS patří k disketě ELIS (v3.00 má C262.3, disketa hlásí C259.11)?
