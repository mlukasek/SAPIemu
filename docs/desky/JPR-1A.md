# JPR-1A – procesorová deska 8080 pro ZPS-3

Stav: **emulovaná** (varianta karty JPR-1 v `src/cards/jpr1.*`), ověřeno v ZPS-3 (`tests/test_zps3.cpp`).

`BKP` = `H:\SAPI-1\sapi.cz_BKP_2023-02-22\sapi`, `D11` = `H:\SAPI-1\SAPI_DOKU_2011-09-10` (stejné soubory jsou
i ve snapshotech 2012-05-23, 2012-09-03, 2019-01-04).

## Podklady

- **Návod (OCR):** `BKP\jpr-1a.php`, obsahuje tabulku propojek, konektory X2/X3, rozpisku a PROM D2C v Intel HEX.
- **Schéma:**
  - překreslené v EAGLE: `BKP\pdf\jpr-1a.pdf` = `H:\SAPI-1\JPR-1A\jpr-1a.pdf`;
  - originální sken: `H:\SAPI-1\JPR-1A\schema.tif`;
  - Eagle soubory: `D11\sapi_eagle\REPLIKY\JPR-1A\`.
- **Skeny originálních příloh** po deskách: `D11\sapi1zps3.zip`, složka `6xk19966`.
- **Fotky desky s boot ROM:** `D11\FOTO_poklady_petr\SAPI\JPR-1A_*`.
- **Celý manuál ZPS-3:** `BKP\pdf\zps3.pdf` (79 stran, sken bez textu). Na str. 6 je mapa portů, na str. 9
  klávesnice a tiskárna.

## Zapojení (podle návodu a schématu)

- **CPU:** 8080A, 8224 s krystalem 18 MHz (2 MHz), 8228. Na desce není RAM.
- **EPROM:** dvě patice 2716 = 0000–0FFF (OE1 → D3B 0000–07FF, OE2 → D4B 0800–0FFF).
- **BOOT klopný obvod** (LED V2):
  - RESET ho nastaví a EPROM je na 0000–0FFF jen pro čtení. Do tohoto rozsahu se zapisuje do RAM pod ní,
    tři-stavově se odpojuje jen MR, takže ROM se zkopíruje sama do RAM.
  - Libovolný `OUT 00h` ho vypne a celých 64K je RAM.
- **Porty:** šest 3212 (3 vstupy, 3 výstupy) jako **I/O porty 01, 02, 03** (u JPR-1 paměťově 2400/2800/2C00).
  Konektory X2/X3 jsou zapojené stejně jako u JPR-1.
- **Klávesnice Consul 259.11:**
  - data D1–D7 na P1-IN0..6 (port 02), /CLR na P1-IN7;
  - SC na P0-IN0 (port 01 bit 0);
  - /AC na P0-OUT0, ZVUK na P0-OUT1.
  - Je to stejný protokol, jaký emulátor už má na JPR-1ZI.
- **Tiskárna Consul 2111:** data na P2-OUT (port 03), strobe P1-OUT1, AC na P2-IN6.
- **Přerušení:** obvod 3214 tu není. INT0 a INT1 jsou sečtené.
  - JP1 1-2 = RST 7 (INTA přes 1k na +12 V).
  - JP1 1-3 = vektor ze sběrnice.
- **PROM D2C (74S571, 512 × 4):** Intel HEX v `jpr-1a.php`, přepsaný z tabulky, ne fyzický dump.
  - Vstupy A0..A8 = A15, A9, A8, A12, A13, A14, A10, A11, BOOT; /CE = /AEN.
  - Výstupy aktivní v nule: D0 PORTY, D1 INT/EXT, D2 OE1, D3 OE2.
  - V režimu RAM jsou PORTY jen pro A15..A8 = 01–03, `OUT 00` tedy nic nedělá.
  - V režimu ROM: OE1 0000–07FF, OE2 0800–0FFF, PORTY 0000–03FF (včetně portu 00).

## Software (EPROM)

| ROM | Soubor | Zdroj |
|---|---|---|
| JPR-1A 48K MON V1.0-C1 (Tesla Eltos IMA 2/86), standard ZPS-3 | `D11\SAPI_EPROM\BOOT_pro_CPM\B_V10_JPR-1A_RPD-1_ZPS-3.BIN` (2 KB, stejné jako `Foto\Jiri_Slansky\SW\JPR-1A 48K MON V1.0-C1\` a `D11\SW_SAPI_Petr\EMIL\JPR1A_MON1(a).bin`) | `MON48C1.MAC` v `BKP\data2\cpmzps3_soubory.rar` |
| JPR-1a 64K V c3 (Tesla IMA, CP/M 64K), v repu `roms/jpr-1a/MON64C3.bin`, sestava `ZPS-3_CPM64K_IMA.sapi`. Zkopíruje se na F800 a vypne EPROM, video F000 | `SAPI_DOKU\SAPI_EPROM\BOOT_pro_CPM\JPR1A_BOOT_64k.BIN` (= `JPR1A_BOOT_64k_C3.BIN`) | `MON64C3.MAC` (`SAPI_DOKU_FTP\...\!!!!_TRIDENO_!!!!\CPM64K\CPM64K_004`) |
| VUT Brno 64K MON V1.0-X2 | `M1_0_CPM.BIN` | |
| LIAZ V1.3 | `Foto\...\JPR-1A_LIAZ_1-3.BIN` | |
| Peřina CP/M 2.2/P MOR48A, MOR62A, MOR48Z, MOR62Z (+F pro FDC-1) | `BKP\data\mor*_hex.php`, `rpd_soubory\MOR*.HEX` | `MOR24.MAC`, `MOR24F.MAC` |

**MON V1.0-C1** postupuje takto:
1. Zkopíruje se na B800 a provede `OUT 00`.
2. Podle DCD na DSM-1 zvolí konzoli: terminál, jinak AND-1A na E800 a klávesnici Consul.
3. Ovladačem RPD-1 (porty 20–25, 3C–3E) načte stopu 0, sektor 1 na 0080h a skočí tam. Boot sektor pak volá
   monitor+21h/24h.

## Emulace

- Typ `JPR-1A` je stejné 8080 jako JPR-1, liší se dekodérem a periferiemi karty.
- **Parametry:**
  - `rom` (D3B + D4B od 0000, až 4 KB);
  - `decoder` (PROM D2C jako soubor, výchozí `roms/jpr-1a/JPR-1A_D2C_74S571.bin`, přepis z návodu);
  - `jp1` (`1-2` RST 7, `1-3` vektor ze sběrnice), `crystal_hz`, `keyboard`.
- **Dekodér:** kód z PROM jen čte výstupy podle zapojení ze schématu (vstupy A15, A9, A8, A12, A13, A14, A10,
  A11, BOOT). Jiná adresace = jiný soubor PROM.
- **BOOT:** RESET ho nastaví. Čtení s aktivním OE1/OE2 jde z EPROM, zápisy vždy na sběrnici (do RAM pod
  EPROM). `OUT 00h` v režimu BOOT (PORTY pro port 00) ho vypne.
- **Porty** 01–03 (PORTY) jsou P0–P2: klávesnice na P0/P1 stejně jako u JPR-1ZI. P2-IN (tiskárna) čte FFh.
- **Přerušení:** INT0 nebo INT1 ze sběrnice, bez priorit.
- **ROM v repu:** `roms/jpr-1a/JPR-1A_MON_V1.0-C1.bin` (originál ZPS-3) a `roms/jpr-1a/MOR48A.bin` (Peřina CP/M
  2.2/P 48K, 2060 B, podpis „1988 PERINA“ přesahuje do D4B).

## Nejasnosti

- Fyzický dump PROM D2C (zatím tabulka z návodu).
- Rozpiska osazuje jen **D4B**, ale OE1 pro 0000 vybírá **D3B**. Ve které patici je boot EPROM (fotka nebo HW)?
- Jak jsou nastavené propojky na skutečné desce (JP1, přerušení)?
