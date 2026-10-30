# DSM-1 – sériové rozhraní s kazetou

Zdroje:
- [sapi.cz/sapi/dsm-1.php](https://sapi.cz/sapi/dsm-1.php), [sapi.cz/sapi/obrazky/dsmobr3.png](https://sapi.cz/sapi/obrazky/dsmobr3.png)
- podklady k DSM-1 v archivu autora

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
- **Ověřeno na nahrávce** `tool_48.wav` z archivu SAPI_DOKU (složka `Kazetopáskové vybavení AUDIO WAV`): mezery jsou přesně 208 a 416 µs. Tyto WAV nahrávky jsou vhodné jako testovací vstup pro emulátor.

## Emulace (`src/cards/dsm1.*`, `src/chips/tape_deck.*`)

- **Registry a terminál** jako u DMR-1 (UART MHB1012, modemové bity, přerušení INT0). Terminálový kanál se jmenuje
  „DSM-1 #10“ a jde připojit přes TCP nebo COM stejně jako DMR-1 (`docs/seriova-linka.md`).
- **Parametry:** `base` (propojky 16-12 až 16-15), `baud` (rychlost terminálu, z výroby 600 Bd), `int0`, `tape`
  (kazeta vložená po zapnutí), `tape_write_protect`, `connected`, `cts`, `flow_control`, `serial` (TCP, COM).
- **Magnetofon** (okno Média, MCP `tape_status`, `tape_insert`, `tape_control`):
  - Kazeta je zvukový soubor WAV (PCM 8/16/24/32 bitů nebo float, libovolná frekvence, stereo se smíchá)
    nebo MP3 (knihovna minimp3). Otevře se odkudkoli.
  - **Kazeta z programu:** Intel HEX nebo binární soubor se při vložení převede na zvuk kazety MIKOS nebo
    MIKROBASIC (`src/core/tapegen.*`, formát a ovládání v `docs/software/kazeta.md`). Je jen pro čtení.
  - „Uložit jako WAV…“ (MCP `tape_control` `export`, cli `--tape-export`) uloží kazetu jako WAV, např. pro
    přehrání do skutečného SAPI.
  - Tlačítka PŘEHRÁT, NAHRÁT, STOP, Převinout, Vysunout, Nová kazeta, ochrana proti zápisu.
  - Motor se točí, jen když je stisknuté PŘEHRÁT nebo NAHRÁT a program sepne relé (bit 5).
  - Vložená kazeta je chráněná proti zápisu (MP3 vždy). Nahrávat jde na novou kazetu nebo po vypnutí ochrany.
    Nahraná kazeta se uloží jako WAV (16 bitů mono) pár sekund po posledním zápisu, při pauze a při přepnutí do
    jiného okna Windows (při dlouhém nahrávání nejpozději po minutě), při vysunutí, vypnutí a ukončení emulátoru.
- **KAZ = 1:** UART pracuje s magnetofonem na 2400 Bd, terminál je odpojený. RD = 1 čte z pásky, RD = 0 nahrává.
  Bit 4 stavu modemu je surová úroveň dekódované linky.
- **Záznam:** linka UART (start, data, parita, stop, klid = 1) XOR hodiny 2400 Hz, obdélník do WAV 44,1 kHz.
- **Rozběh motoru:** po každém zapnutí motoru při nahrávání se prvních 50 ms zapíše ticho. Bez toho na sebe dva
  záznamy (třeba hlavička z `.K_A` a data z `.K_S` v MIKOSu) navazovaly fázovým skokem hodin 2400 Hz. Dekodér
  ho bral jako start bit a další blok četl posunutý.
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
  - Nahrávky z archivu SAPI_DOKU (složka `Kazetopáskové vybavení AUDIO WAV`: BASIC EX V4.0, TOOL 48, TOOL 80)
    ve WAV i MP3 bez chyby rámce. Hlavička `10 10 10 01 "TOOL 48"`, tedy formát jiného programu než MIKROBASIC.
  - MIKROBASIC SAVE → WAV → LOAD → RUN (`tests/test_cassette.cpp`).

## Nejasnosti

- Pořadí bitů délky slova (b3–2) v registru řízení UART. OCR uvádí „10“ = 6 bitů.
- **Reset UARTu (MR):** DSM-1V ho drží do prvního čtení paměti na sběrnici (/MR), změřeno 2026-10-11
  (`docs/zmeny-podle-hw.md`). Má podobné prodloužení MR i původní DSM-1 a DMR-1 (MHB1012)? Ve schématech
  zkontrolovat, emulace je zatím bez něj.
- **Převzetí znaku do vysílacího registru:** 6402 na DSM-1V ho dělá až na hraně taktu 16× (změřeno 2026-10-11).
  MHB1012 (AY-5-1013) nejspíš stejně, změřit na skutečné DSM-1 (autor: TTL signály jen na čipu UART).
- Stav po resetu: dokumentace uvádí „DR=1“, což je pravděpodobně chyba OCR.
- Jak přesně skutečný obvod rozhoduje bity a jak se chová na začátku úseku (emulace: fázový dekodér, viz výše).
- Co dělá výstup do magnetofonu, když je KAZ = 0 (emulace: klidový tón).
- Kazeta z programu přehraná z PC do skutečné DSM-1: stačí obdélník z WAV, jaké jsou minimální mezery pro
  skutečný magnetofon (`docs/software/kazeta.md`, kapitola 5)?
