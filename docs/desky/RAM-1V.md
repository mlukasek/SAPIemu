# RAM-1V – konfigurovatelná SRAM s generátorem MAP (Libor Lasota, podle RAM-1A)

Stav: **emulovaná** (2026-10-04), sestava `machines/sapi1v.sapi`, test `tests/test_sapi1v.cpp`.
Karta `RAM-1V` (`src/cards/ram1v.*`):
- parametry pro všechny propojky S1–S6, osazení A/B a patici C (`none`, `sram`, `m48t35`);
- RTC M48T35 na 7FF8–7FFF obvodu C je volitelné podle autora, čas bere z PC.
- **Výchozí jako autorova deska** (fotka `..\SAPI-karty\RAM-1V\mRAM-1V rev. 1.1.jpg`, shodné s potiskem „ZPS-3/4 SETUP“
  na rev 1.2):
  - **registr MAP na portu 63h**, MAP1 = bit 6, MAP2 = bit 7, oba buzené. Je to stejný port a stejné bity
    jako u ZRMD-1, MikroMonovo `OUT 63h,3F` tedy dá MAP1 = MAP2 = 0.
  - díra 16 KB na C000 jen při MAP1 = H (MAP2 se ignoruje);
  - díra 2 KB na F800 jen při MAP1 = L (tam je AND-1V);
  - S4 rozpojené, patice C prázdná.
- **„ZPS-1/2 SETUP“** z potisku: S1–S4 rozpojené, S5 díra 0000 s ignorovaným MAP1, S6 bez díry.
- Při MAP1 = H je na C000–FFFF **CGA-1V** (autor), proto tam díra 16 KB.

Zdroje:
- schéma `..\SAPI-karty\RAM-1V\RAM-1V.pdf` (DPLA0059RAM rev 1.1, 07.04.2024);
- popis `..\SAPI-karty\RAM-1V\Popis-RAM-1V-rev.-1.1.pdf`;
- fotka `..\SAPI-karty\RAM-1V\Foto_moje\RAM-1V_front.jpg`;
- https://www.pmd85.cz/?page_id=1993.

**Bez PROM a GAL:** vše je logika 74LS a propojky, do `roms/` nic nepatří.

## Paměť

- 2 nebo 3× 62256 (32 KB), bez baterie.
- **A** (IC10) = 0000–7FFF, **B** (IC6) = 8000–FFFF.
- **C** (IC2, volitelná patice, „primárně pro Timekeeper“) nahradí A na 0000–7FFF, když je zvolený signál MAP
  v úrovni H. Volí se propojkou **S4 CONF** (osazená = 0):
  - nic nebo obě osazené: C se nepoužije;
  - jen 1-2: C podle MAP1 = H;
  - jen 3-4: C podle MAP2 = H.
- /OE = NAND(libovolné CS, MR), /WE přímo /MW sběrnice. Bez čekání.

## Díry v adresním prostoru

- **Díra 16 KB, S5** (74LS85): 9-10 = /A14, 11-12 = /A15 (osazená = bit 0). Podmínky MAP1MODE (1-2 a 3-4) a
  MAP2MODE (5-6 a 7-8) pro každou dvojici (X, Y):
  - X rozpojená, Y osazená: MAP se ignoruje;
  - obě rozpojené: díra vypnutá;
  - obě osazené: díra jen při MAP = H;
  - X osazená, Y rozpojená: díra jen při MAP = L.
  - Musí platit obě podmínky. Potisk: NO HOLE / MAPx IGNORE / MAPx=0 / MAPx=1.
- **Díra 2 KB, S6** (74LS688): 1-2…9-10 = /A11…/A15 (osazená = 0), 11-12 a 13-14 = MAP1MODE podle stejného
  pravidla. Typicky 3800h, E800h (AND-1A), F800h (AND-1Z).

## Generátor MAP (list 3)

- **Registr MAP:** 74LS688 na A0–A7 s /IOW, **S3 1-2…15-16 = /A7…/A0** (osazená = 0). Jen zápis. Typicky
  64h (D5 → MAP1, „v souladu s DSE-1“).
- Zápis ukládá do dvou 74LS74 (MAP1, MAP2) na náběžné hraně konce IOW. Bit dat vybírá 74LS151.
- **S1 (MAP1) a S2 (MAP2):**
  - 1-2 = EN: osazená zapne budič 74LS125 na pin 21 (MAP1), resp. 22 (MAP2), rozpojená = vysoká impedance;
  - 3-4, 5-6, 7-8 = B1, B2, B4: číslo bitu = součet vah osazených propojek (např. D5 = B1 + B4).
- Jeden OUT nastaví oba signály. /RES je vynuluje (po resetu MAP = L).
- Vlastní dekódování desky čte MAP ze sběrnice (74LS244).

## Vztah k RAM-1A

- Tabulku RAM-1A to nemá. Jde napodobit jen 0000–BFFF (díra 16 KB na C000) a 4000–FFFF (díra na 0000).

## Revize

- Rev 1.0 měla prohozený potisk MAP1/MAP2 a chybějící povolení 245 při zápisu do registru MAP.
- **Rev 1.2 (11/2025):** „čistší časování“, u díry 16 KB odpadla volba MAP2.

## Autorova deska (fotka)

- S5 = díra 0000–3FFF, S6 = díra F800 (MAP ignorován), S1–S4 bez propojek. Osazené A a B.

## Plán emulace

- Nová karta s parametry podle propojek: S1–S6, osazení A/B/C.
- Výstup MAP1/MAP2 na sběrnici: emulátor má `bus->map`, generátor ho musí nastavovat.
- Možná RTC Timekeeper v patici C.
