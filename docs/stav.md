# Ukládání stavu stroje

Stav zachytí celý stroj v jednom okamžiku a dovolí pokračovat přesně odtud: CPU, paměti, karty, klávesnici a
sériové kanály. Hodí se při ladění (zastavit těsně před chybou a vracet se tam) i při hraní.

## Ovládání

- **GUI:**
  - Soubor → Uložit stav… / Načíst stav… (soubor `.sapistate`, výchozí složka `work/state/`).
  - Rychlý slot: Soubor → Rychle uložit / Rychle načíst, **Ctrl+F2** / **Ctrl+F3**. Jeden slot na sestavu:
    `work/state/<jméno souboru sestavy>.sapistate`. F2 a F3 bez Ctrl zůstávají klávesnici SAPI.
  - Úspěch ukáže stavový řádek na 3 s, chyba otevře okno.
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

Binární soubor: `SAPISTAT`, verze (32 bitů), popis sestavy a soubor sestavy (řetězce). Pak pojmenované bloky
s délkou: `bus`, `card X<slot> <typ>` po slotech, `keyboard`, `serial <n>`, `plant` (technologie APS-5: teploty pecí,
poruchy, vstupy, graf; měřicí pracoviště IMS-2: zdroj, voltmetr, vodiče sběrnice a její monitor). Čísla jsou little endian.

- Každá karta, čip i klávesnice má jednu funkci `state(StateIO&)` (`src/core/state.h`). Ta proměnné při ukládání
  zapíše a při načítání ve stejném pořadí přečte (`io.v(x)`).
- Blok, který přečte víc nebo míň, než se zapsalo, se ohlásí chybou a nic dalšího se neposune.
- Jiná verze formátu se odmítne. Stav je vázaný na verzi emulátoru, ne na skutečný hardware.
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
stav jiné sestavy a poškozená data. Hodiny RTC čtou v testu pevný čas (`host_time_fix`).

**Nová karta:** přidat `void state(StateIO& io) override` se vším, co se za chodu mění. `test_state` to ověří,
jakmile je karta v některé sestavě v `machines/`.
