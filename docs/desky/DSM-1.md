# DSM-1 – sériové rozhraní s kazetou

Zdroje:
- `H:\SAPI-1\sapi.cz_BKP_2023-02-22\sapi\dsm-1.php`, `obrazky\dsmobr3.png`
- `H:\SAPI-1\DSM-1\`

## Základní parametry

- **UART:** MHB1012.
- **Krystal:** 12,288 MHz.
- **Bázová adresa:** jumpery 16-12, 16-13, 16-14 a 16-15 volí 10h, 14h, 18h nebo 1Ch. Z výroby je nastaveno 10h.
- **Rychlost terminálu:** 50–9600 Bd podle jumperu. Z výroby 600 Bd (jumper 1-6).
- **Registry:** stejné jako u [DMR-1](DMR-1.md), včetně významu bitů modemu.

## Kazeta

- **Přepínání:** stejný UART se přepíná bitem KAZ (base+0, bit 4). Bit RD (bit 3) volí čtení nebo zápis, bit 5 ovládá relé motoru.
- **Formát:** pevně 2400 Bd, 1 start bit, 8 datových bitů, 2 stop bity.
- **Kódování:** vysílaný bit XOR hodiny, tedy jedna perioda nosné na bit a změna fáze při změně dat. Prakticky jde o Manchester 2400 bit/s.
- **Čtení:** rozhoduje doba mezi průchody nulou. 208 µs znamená „beze změny“, 416 µs „změna“. Práh čítače je 12.
- **Ověřeno na nahrávce** `H:\SAPI-1\SAPI_DOKU_2024-01-03\Kazetopáskové vybavení AUDIO WAV\tool_48.wav`: mezery jsou přesně 208 a 416 µs. Tyto WAV nahrávky jsou vhodné jako testovací vstup pro emulátor.

## Emulace (`src/cards/dsm1.*`, `src/chips/tape_deck.*`)

- **Registry a terminál** jako u DMR-1 (UART MHB1012, modemové bity, přerušení INT0). Terminálový kanál se jmenuje
  „DSM-1 #10“ a jde připojit přes TCP nebo COM stejně jako DMR-1 (`docs/seriova-linka.md`).
- **Parametry:** `base` (propojky 16-12 až 16-15), `baud` (rychlost terminálu, z výroby 600 Bd), `int0`, `tape`
  (kazeta vložená po zapnutí), `tape_write_protect`, `connected`, `cts`, `flow_control`, `serial` (TCP, COM).
- **Magnetofon** (okno Média, MCP `tape_status`, `tape_insert`, `tape_control`):
  - Kazeta je zvukový soubor WAV (PCM 8/16/24/32 bitů nebo float, libovolná frekvence, stereo se smíchá)
    nebo MP3 (knihovna minimp3). Otevře se odkudkoli.
  - Tlačítka PŘEHRÁT, NAHRÁT, STOP, Převinout, Vysunout, Nová kazeta, ochrana proti zápisu.
  - Motor se točí, jen když je stisknuté PŘEHRÁT nebo NAHRÁT a program sepne relé (bit 5).
  - Vložená kazeta je chráněná proti zápisu (MP3 vždy). Nahrávat jde na novou kazetu nebo po vypnutí ochrany.
    Nahraná kazeta se uloží jako WAV (16 bitů mono) při vysunutí, vypnutí a ukončení emulátoru.
- **KAZ = 1:** UART pracuje s magnetofonem na 2400 Bd, terminál je odpojený. RD = 1 čte z pásky, RD = 0 nahrává.
  Bit 4 stavu modemu je surová úroveň dekódované linky.
- **Záznam:** linka UART (start, data, parita, stop, klid = 1) XOR hodiny 2400 Hz, obdélník do WAV 44,1 kHz.
- **Čtení** (demodulace celé pásky předem):
  - Průchody nulou se Schmittovým klopným obvodem (25 % obálky) po odstranění stejnosměrné složky.
  - První interval kolem 416 µs po úvodním tónu je začátek start bitu. Tím je dána mřížka bitů i polarita
    nahrávky.
  - Každý bit se pak rozhoduje zvlášť porovnáním signálu v první a druhé polovině bitu. Mřížka sleduje průchody
    uprostřed bitů jako PLL.
  - Ticho ukončí úsek, linka je v klidu (1).
  - Dekodér jen podle délky intervalů (jako popis v manuálu) se na kopiích kazet v MP3 rozpadal: jeden zákmit
    otočil úroveň linky. Fázový dekodér čte všechny dostupné nahrávky bez chyby.
- **Ověřeno:**
  - Nahrávky `H:\SAPI-1\SAPI_DOKU_2024-01-03\Kazetopáskové vybavení AUDIO WAV\` (BASIC EX V4.0, TOOL 48, TOOL 80)
    ve WAV i MP3 bez chyby rámce. Hlavička `10 10 10 01 "TOOL 48"`, tedy formát jiného programu než MIKROBASIC.
  - MIKROBASIC SAVE → WAV → LOAD → RUN (`tests/test_cassette.cpp`).

## Nejasnosti

- Pořadí bitů délky slova (b3–2) v registru řízení UART. OCR uvádí „10“ = 6 bitů.
- Stav po resetu: dokumentace uvádí „DR=1“, což je pravděpodobně chyba OCR.
- Jak přesně skutečný obvod rozhoduje bity a jak se chová na začátku úseku (emulace: fázový dekodér, viz výše).
- Co dělá výstup do magnetofonu, když je KAZ = 0 (emulace: klidový tón).
