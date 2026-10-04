# Bomberman pro SAPI-1 (port z MZ-700) – plán

Zadání autora (2026-10-05): původní single player Bomberman pro Sharp MZ-700 (Hudson Soft 1983) upravit
a přeložit pro SAPI. Cíl je sestava V (`machines/sapi1v.sapi`) s barevnou grafikou CGA-1V. Při ladění
používat MCP server emulátoru.

## Zdroj

- `E:\SAPI_GIT\BomberNet\` (autorův klon projektu BomberNet; síťová verze pro MZ-700/800 a ZX Spectrum).
- `bomber.asm`: kompletní komentovaný disassembler originálu (pasmo syntaxe), přeloží se bajt po bajtu stejně
  (`build.sh`, `bomber.mzf`). ORG 1200h, 1200h–320Fh, Z80.
- `README.md` BomberNet: paměťová mapa, model vykreslování.
- `c/` je přepis do C (z88dk) pro MZ a ZX se síťovou hrou. `c/platform/zx/tables.c` a `tools/make_zx_tables.py`
  obsahují glyfy (79 různých znaků hry a titulku), `docs/port-zx-spectrum.md` popisuje port na jiný stroj.

## Co je v originálu závislé na MZ-700

- **Monitor ROM:**
  - `GETKY` 001Bh: klávesa, nebo 0 bez čekání (volání na 13B2, 1C21, 1F85);
  - `MSTA` 0044h a `MSTP` 0047h: tón (8253 MZ, dělič v `RATIO` 11A1h).
- **Obrazovka:** VRAM D000h (znaky) a D800h (atributy barev). Zapisuje do ní jen `put_vram_char`. Hra kreslí do
  `draw_buffer` a `flush_screen` posílá do VRAM jen změněné buňky (`shadow_vram`).
  - Logické kódy buněk se překládají přes `game_table` a `title_table` (displejový kód MZ + atribut).
  - 40×25 buněk po 8×8 bodech = 320×200, přesně rozlišení CGA-1V.
- **Časování:** smyčky zpoždění pro Z80 na 3,5 MHz (snímek hry 60,8 ms podle BomberNet). JPR-1V má 4 MHz
  (TURBO) nebo 2 MHz, zpoždění se musí přepočítat nebo napojit na hodiny.
- **Zásobník** pod 1200h, přerušení zakázaná, celá paměť je RAM (MZ).

## Návrh portu na sestavu V

- **Program:** CP/M `.COM` od 0100h (ORG 0100h). Kód je celý symbolický, takže nový ORG stačí. Stará data za
  kódem (snímek RAM) nahradit `defs`.
- **Obraz CGA-1V na C000h:**
  - zapnout `OUT 63h,C0h` (MAP1 = MAP2 = H), na konci `OUT 63h,00h`;
  - po dobu hry je RAM C000–FFFF skrytá, takže BDOS se nevolá;
  - EGA režim, 4 body na bajt, buňka 8×8 = 2 bajty × 8 řádků. Nízký nibble je atribut (dvojice barev
    z palety Bt476), vysoký nibble ink/paper bodů;
  - MZ-700 má 8 barev popředí × 8 pozadí. Použité kombinace z `game_table`/`title_table` (do 16) namapovat na
    16 atributů a paletu nastavit podle barev MZ;
  - `put_vram_char` = vykreslit glyf buňky.
- **Glyfy:** 8×8 z CGROM MZ-700 pro použité displejové kódy. Vzít z BomberNet (tabulky ZX / extrakce) nebo
  z dumpu CGROM, pokud je v projektu.
- **Klávesnice:** MPH-1V joystick (port 54h, aktivní v 0) + klávesy přes P0/P1 JPR-1V (Consul 262.3 bez 7474,
  pulzní strobe: držet poslední kód po dobu snímku). `GETKY` nahradit rutinou, která vrátí kód jako MZ.
- **Zvuk:** `MSTA`/`MSTP` na MPH-1V YM3812 (jeden kanál, kmitočet z `RATIO`), případně bzučák klávesnice.
- **Časování snímku:** VBI CGA-1V (60 Hz, STATUS D7) nebo čítač 82C54 na MPH-1V, místo smyček.
- **Ladění:** MCP server emulátoru (`sapiemu-cli --mcp` na pozadí, JSON-RPC na `http://127.0.0.1:8591/mcp`, viz
  `docs/build.md`): `load_hex`, `type_text`, `screenshot` s `display` CGA-1V, `joystick`, `save_state`.

## Otevřené otázky (pro autora)

- **Assembler Z80:** na tomto PC není (pasmo, sjasmplus, z88dk chybí).
  - Návrh: rozšířit `tools/asm8080.py` o Z80 ve stylu pasmo a ověřit ho tím, že `bomber.asm` přeloží bajt po
    bajtu stejně jako originál.
  - Jinak nainstalovat sjasmplus nebo pasmo.
- **Kam s portem:** do repa BomberNet (`sapi/`), nebo do SAPIemu (`media/bomberman/`)?
- **Ovládání:** joystick MPH-1V i klávesnice? Které klávesy?

## Stav

- 2026-10-05: rozbor a plán, kód zatím není.
