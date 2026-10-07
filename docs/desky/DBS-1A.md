# DBS-1A – deska barevné semigrafiky SAPI-1.5

Stav: **emulovaná** (typ `DBS-1A`, také `DBS-1`; `src/cards/dbs1a.*`) od 2026-10-07, **s náhradním znakovým
generátorem**. Sestava `machines/zps/ZPS-6.sapi`, test `test_sapi15`. Tesla Liberec 1990, deska 6XB 007 83,
schéma 6XK 199 246/Z, návod 6XV 123 45.

## Podklady

- **Návod Tesla** (sken J. Bíby, 33 stran, i originální schéma na str. 31–33): `..\SAPI_DOKU\SAPIMAN\DBS-1A.pdf`.
- **Repliky** (`..\SAPI-karty\DBS-1A\`): `DBS-1A`, `DBS-1A_u` (+12 V pro SCART), `DBS-1A_funkcni` (opravená,
  „funkční pro rastr 64x20“, J. Bíba 2014). Netlisty verzí 2014 jsou shodné až na +12 V. **„Chyba selektu“**
  replik z roku 2012: nezapojený chip select 6845 (DD16/25), opraveno 2014. `DBS-1A_POPIS.TXT`.
- Software: ROM BIOS PKT-1 (`docs/software/SAPI-1.5.md`), `CPM62-5.TXT` (bity atributu), `SETDBS1.COM` (nastavení
  barev, Turbo Pascal). Ovladač J. Bíby z roku 2012 je na H: (`docs/software/SAPI-1.5.md`).
- Rozbor: `work/sapi15/report_dbs1.md` (mimo git).

## Zapojení

- **6845** (na desce СМ607Р): porty **E0h adresa, E1h data** (A0 = RS), dvojice podle PROM DD27 a propojek
  XT2/XT3 (E0–EE). Číst jde jen R14–R17. Jiné registry deska nemá.
- **Video RAM 4 KB jen pro zápis** (datový buffer má DIR trvale k RAM): sudé adresy = znaky (DD18), liché =
  atributy (DD17), buňka = A1–A11 CPU = MA0–MA10 6845 (MA11, MA13 nezapojené, MA12 jen na propojku XT5).
  Obraz se opakuje po 2048 buňkách. Zápis proběhne nejpozději do jednoho znaku, bez sněžení a bez čekání.
- **Okno** podle PROM DD2 (A11–A15, MAP1, MAP2 přes XT1) a XT4: standard **B000–BFFF při MAP1 = L, MAP2 = H**.
  PKT-1 nastavuje MAP2 portem 0Eh (03h kolem zápisu do obrazovky, pak 01h). Horní skupina C000–FFFF MAP nedekóduje.
- **Znakový generátor** DD15 (2716/2732/2764): A0–A2 = RA0–RA2, **A3 = bit 7 kódu**, A4–A10 = bity 0–6, A11/A12
  podle XT5 (RA3, bit 3 atributu, MA12). Standard 2732 se 256 znaky 8 × 12 (XT5 27-28), KOI-8 čs2 se semigrafikou.
  Bit 7 = levý bod. **Dump EPROM (M2732A „59“) se nedochoval.**
- **Takt bodů 12 MHz**, 8 bodů na znak = 64 sloupců (XT7 38-39), 6 bodů = 80 sloupců (37-38; tabulka návodu
  str. 20 je obráceně).
- **Atribut:** bity 2–0 popředí, 6–4 pozadí, 3 = jas Y (nebo výběr sady podle XT5), 7 = **1 bliká** (buňka ukáže
  jen pozadí 16 snímků z 32). **Pořadí R a G:** návod i `CPM62-5.TXT` mají bit 2 = R, bit 1 = G, schéma Tesla
  i replika vedou bit 1 na R a bit 2 na G.
- Kurzor 6845 se XORuje s body. Bez světelného pera a bez přerušení. /RES jde jen na RESET 6845 (registry zůstanou).
- Výstupy X2: RGB TTL + Y, RGB ~1 V, H/V synchronizace obou polarit, kompozitní mono (XT6), na replice +12 V pro SCART.
- Registry ROM BIOSu PKT-1: R0–R15 = 62 40 4D(opravená verze 4A) 14 1A 00 14 17 00 0B 60 0B 00 00 00 00, tedy 64 × 20,
  řádek znaku 12 bodů, řádkový kmitočet 15,0 kHz a snímkový 46,3 Hz (sady z návodu dávají 15,625 kHz / 50 Hz).

## Náhradní znakový generátor

- `roms/dbs-1a/DBS-1A_CharGen_nahrada.bin` (4 KB, adresování 2732 výše) vyrábí `python tools/make_dbs1a_font.py`
  (`--show KÓD` ukáže znak) z fontu **AND-1Z** (`roms/and-1z-89/AND-1Z_CharGen.bin`). AND-1Z (Tesla 1989) má stejné
  rozložení kódů, jaké popisuje ROM BIOS DBS-1: 20–7Fh ASCII, 80–BFh semigrafika 2 × 3, C0–FFh česká písmena
  KOI-8 čs2, a `CPM62-5.TXT` uvádí, že DBS-1 je s AND-1Z kompatibilní na úrovni BIOSu.
- 00–1Fh jsou prázdné, 7Fh šachovnice, semigrafika jsou bloky 4 × 4 body (bit 0 vlevo nahoře až bit 5 vpravo
  dole; předpoklad). V konfigurátoru je font označený jako NÁHRADA.

## Emulace

- Parametry: `font`, `xt5` (adresování EPROM), `port` (XT2/XT3), `window` (XT1/XT4), `map1`/`map2` (XT1 1-2, 3-4),
  `dots` (XT7), `rgb_order` (`manual` výchozí, `schematic`), `output` (`rgb` s jasem, nebo `composite` šedý).
- Velikost obrazu podle registrů R1, R6, R9 (před nastavením 6845 černá plocha 512 × 240). Blikání a kurzor podle
  času sběrnice a snímkové doby z R0, R4, R5, R9. Okraj mimo zobrazovanou plochu se nekreslí.

## Nejasnosti k ověření na HW

1. Pořadí R a G v atributu (atributy 02h a 04h na X2/16 a X2/20).
2. Blikání: bit 7 = 1 bliká a blikající buňka ukáže pozadí (ne prohození barev)?
3. Horní poloviny PROM DD2 a DD27 (C000–F000, E8–EF): tabulky propojek a výpisy PROM v návodu se liší; ideálně
   vyčíst DD2 („53“) a DD27 („61“).
4. XT7 38-39 = 8 bodů? XT5 29-32 (JP32 je VCC)? Je Y vždy bit 3 atributu?
5. **Dump znakového generátoru** (M2732A „59“, 6XN 902 059?) a ověření adresování (A3 = bit 7, řádky 8–15
   v horních 2 KB).
6. Časování ROM BIOSu (15,0 kHz / 46,3 Hz): existovala DBS-1 s jiným krystalem, nebo to monitor snesl?
7. Kterou propojku XT5 předpokládá CP/M Melsoft (bit 3 atributu jako výběr sady potřebuje XT5 28-29 nebo 2764)?
