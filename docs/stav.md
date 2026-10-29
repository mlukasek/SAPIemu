# Ukládání stavu stroje

Stav zachytí celý stroj v jednom okamžiku a dovolí pokračovat přesně odtud: CPU, paměti, karty, klávesnici a
sériové kanály. Hodí se při ladění (zastavit těsně před chybou a vracet se tam) i při hraní.

## Ovládání

- **GUI:**
  - Soubor → Uložit stav… / Načíst stav… (soubor `.sapistate`, výchozí složka `work/state/`).
  - Rychlý slot: Soubor → Rychle uložit / Rychle načíst, **Ctrl+F2** / **Ctrl+F3**. Jeden slot na sestavu:
    `work/state/<jméno souboru sestavy>.sapistate`. F2 a F3 bez Ctrl zůstávají klávesnici SAPI.
  - Úspěch ukáže stavový řádek (7 s, zůstane v Logu), chyba otevře okno Chyba.
  - **Stav jiné verze SAPIemu:** stavy alf od 0.3.0 se načtou. Stav, který tato verze neumí přečíst, se odmítne
    s hláškou, kterou verzí byl uložený, a běžící sestava zůstane, jak byla (podrobnosti v oddílu Formát).
    Alfy do 0.6.0 stavy této verze nenačtou.
- **MCP:**
  - `save_state` / `load_state` s `name` (pojmenovaný stav v paměti emulátoru, výchozí `default`) nebo `file`.
  - Načtení zachová breakpointy, watchpointy a pauzu, takže jde o rychlé „checkpointy“ při ladění.
- **CLI:** `sapiemu-cli ... --save-state soubor` a `--load-state soubor`. Načtení postaví sestavu ze stavu, i když
  `--machine` říká jinou.

## Co stav obsahuje

- Popis sestavy (JSON, včetně parametrů změněných za chodu) a soubor sestavy, ze kterého pochází.
- Čas sběrnice, stav MAP1/MAP2, počet kolizí.
- Každá karta svůj proměnný stav. Co dává konfigurace (ROM, PROM, propojky), se při načtení postaví znovu
  z popisu:
  - CPU: Z80 a 8080 včetně rozpracované instrukce, čekací stavy, BOOT, porty;
  - všechny RAM, videopaměti, palety CGA-1V;
  - **RAM disky a zálohované SRAM** (ZRMD-1, ZRD-1V, RMD-1, DMR-1). Jsou to paměti, takže se vrátí do okamžiku
    stavu. RAM disk v režimu `rw` se pak uloží i do pracovní kopie ve `work/` (autor): změny od uložení stavu
    zmizí i tam;
  - obvody: MHB1012, 8255, 8253/82C54, RTC 72421, YM3812 (jádro Nuked-OPL3 celé, ukazatele uvnitř jako posuny),
    IDE řadič (registry a buffer sektoru), firmware PMD32-SD, řadič RPD-1 (stopa v bufferu, čítače),
    zámek ZRD-1V a MAX187.
- Klávesnice (typ, když byla vyměněna za chodu, fronta napsaných kláves, strobe a ACK) a sériové kanály (DCD,
  CTS, bajty od PC, které SAPI ještě nepřevzal).

## Co stav neobsahuje

- **Velká média jen odkazem:** IDE disk, SD karta PMD32-SD, diskety RPD-1, kazety DSM-1 (autor) a kazety KZD-1.
  Kazeta KZD-1 se před uložením stavu zapíše do svého souboru, ve stavu je jen mechanika (poloha, rychlost,
  přetížení, rozpracovaný záznam).
- Kresba Minigrafu (tahy na papíře) je součástí stavu, jako by papír zůstal v zapisovači. Stav si pamatuje
  vložený soubor, pozici kazety a polohu hlavy. Když je při načtení vložený jiný soubor, vloží ten ze stavu. Když se
  soubor od uložení změnil (velikost, čas), napíše varování do logu. Data se neukládají.
- Věci PC: připojení sériových linek (TCP, COM), zvukový buffer, gamepad, zachycení myši, čekající pohyb myši.
- Hodiny RTC (72421 na ZRMD-1, Timekeeper na RAM-1V) jdou podle času PC jako skutečné zálohované hodiny. Po
  načtení ukazují aktuální čas (s posunem, který si program nastavil), ne čas uložení.

## Formát

Binární soubor: `SAPISTAT`, formát (32 bitů), od formátu 2 verze SAPIemu, která stav uložila (řetězec, např.
`0.7.0-alpha`), pak popis sestavy a soubor sestavy (řetězce). Pak pojmenované bloky s délkou: `bus`,
`card X<slot> <typ>` po slotech, `keyboard`, `serial <n>`, `plant` (technologie APS-5: teploty pecí, poruchy, vstupy,
graf; měřicí pracoviště IMS-2: zdroj, voltmetr, vodiče sběrnice a její monitor). Čísla jsou little endian.

- Každá karta, čip i klávesnice má jednu funkci `state(StateIO&)` (`src/core/state.h`). Ta proměnné při ukládání
  zapíše a při načítání ve stejném pořadí přečte (`io.v(x)`).
- Blok, který přečte víc nebo míň, než se zapsalo, se ohlásí chybou a nic dalšího se neposune.
- **Formáty** (`STATE_FORMAT`, `STATE_FORMAT_MIN` v `src/core/machine.h`): SAPIemu zapisuje formát 2 a čte formáty
  1 až 2. Formát 1 (alfy 0.1.0 až 0.6.0) verzi programu v hlavičce nemá. Začátek hlavičky (`SAPISTAT`, formát, verze
  SAPIemu) zůstane stejný i v dalších formátech, takže každá verze pozná, kdo stav uložil, i když ho neumí přečíst.
- **Stavy z alf:** načtou se stavy od alfy 0.3.0 (ověřeno stavem z 2026-10-17). Alfy 0.1.0 a 0.2.0 mají formát 1
  také, ale `state()` klávesnice a DSM-1V se 2026-10-06 změnilo (blok klávesnice 70 → 124 B, DSM-1V 292 → 342 B).
  Takový stav projde hlavičkou a selže až v kartách: běžící sestava se vrátí (níže) a GUI řekne „Stav uložila jiná
  verze SAPIemu (starší alfa), toto je verze 0.7.0-alpha.“ Je to známé omezení, ne chyba.
- **Stav jiného formátu** (novější verze SAPIemu, nebo tak stará, že ji tato už nečte) se odmítne hned po hlavičce,
  ještě před `load_config`: běžící sestava se nedotkne. GUI to řekne česky („Uložila ho novější verze SAPIemu
  (0.9.0). Tato verze (0.7.0-alpha) ho neumí přečíst.“), MCP a `sapiemu-cli` anglicky.
- **Stav, který nesedí až v kartách** (karta jiné verze čte jiná data, popis se tu nedá postavit, poškozený
  soubor): `EmuHost::load_state` si před přepnutím uloží stav běžící sestavy do paměti a po chybě ji z něj postaví
  znovu, jak byla (paměti, obrazovka, breakpointy, pauza). Vypnutý stroj zůstane vypnutý. GUI k chybě připíše, že
  stav uložila jiná verze („starší alfa“), když to tak je; anglický důvod jádra pak dá až na konec („Podrobnosti:“).
  Co se stalo s běžící sestavou, vrací `load_state` v `back_out` (`StateBack` v `src/core/host.h`) a GUI i MCP
  podle toho volí větu: „Sestava běží dál, jak běžela.“ (`STATE_BACK_SAME`, i u odmítnuté hlavičky), „vypnula se
  a zapnula, obsah její paměti je ztracený“ (`STATE_BACK_RESTARTED`: její stav se předem nepodařilo uložit, postaví
  se znovu z popisu), „nevrátila se … proběhl reset“ (`STATE_BACK_RESET`, důvod v Logu).
- **Změna dat karty:** když `state()` nějaké karty začne zapisovat nebo číst něco jiného, zvýšit `STATE_FORMAT`.
  Když nová verze starý blok přečíst neumí, posunout na nový formát i `STATE_FORMAT_MIN`: starý stav se pak odmítne
  srozumitelně před přepnutím sestavy místo chyby v půlce bloků. Stav je vázaný na verzi emulátoru, ne na skutečný
  hardware.
- **Sdílené vodiče** (sběrnice IMS-2 mezi kartou a pracovištěm): karty se načítají před `plant`. Uzly se po
  načtení připojí bez přepočtu vodičů (`Ims2Bus::attach_loaded`), jinak by se sběrnice na okamžik spočítala jen
  z části uzlů a přístroje by zareagovaly na falešnou změnu (DPS-1: ztracený první znak odpovědi).

## Test

`tests/test_state.cpp` pro každou sestavu v `machines/`:
1. uloží stav A, nechá stroj běžet a uloží B1;
2. do nového stroje nahraje A, nechá ho běžet stejně dlouho a uloží B2;
3. B1 a B2 musí být stejné bajt po bajtu.

Proměnná, kterou karta zapomene uložit, se projeví rozdílem a test vypíše blok, ve kterém je. Ověřeno vyřazením
jedné proměnné MPH-1V. Navíc stejný test se zaměstnanou MPH-1V (tón YM, 82C54, myš), soubor přes EmuHost,
stav jiné sestavy a poškozená data. `state_formats`: verze SAPIemu v hlavičce, novější a příliš starý formát
odmítnuté bez dotyku běžící sestavy (tentýž objekt stroje, tatáž generace, paměť i obrazovka), formát 1 se načte,
useknutý stav vrátí běžící sestavu i s pamětí, breakpointem a pauzou, vypnutý stroj zůstane vypnutý. Hodiny RTC
čtou v testu pevný čas (`host_time_fix`).

**Nová karta:** přidat `void state(StateIO& io) override` se vším, co se za chodu mění. `test_state` to ověří,
jakmile je karta v některé sestavě v `machines/`.
