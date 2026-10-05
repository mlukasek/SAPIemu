# Deník rozhodnutí

Nejnovější rozhodnutí jsou dole. U každého je uveden důvod. Při změně rozhodnutí se původní text nemaže, ale doplní se nový záznam.

## 2026-10-03 – Základní technologie

- **C++20 + CMake**, sestavení ve Visual Studiu 2022 přes CMake presety.
  - Důvod: autor zná C. VS 2022 umí CMake nativně a přechod na Linux znamená jen jiný preset.
  - Zamítnuto: C# s Avalonia (oficiální MCP SDK, ale potřebuje runtime a model sběrnice je méně přirozený), Rust, Python.
- **Styl kódu „C s třídami“:** C hlavičky, struktury, minimum šablon. Na přání autora.
- **CPU jádro floooh/chips `z80.h`** (licence zlib; v plánu bylo omylem uvedeno MIT).
  - Důvod: emuluje Z80 na úrovni pinů a T-stavů, což přesně odpovídá modelu sběrnice s kartami. Wait stavy se vkládají pinem WAIT (RDY u AND-1Z). Jádro se v projektu chips-test ověřuje testy ZEXDOC/ZEXALL, my je spustíme také.
  - Pro 8080 později vlastní jádro se stejným pinovým rozhraním.
- **GUI: SDL3 + Dear ImGui (docking).** Jsou multiplatformní, lehké a dobře se v nich dělají ladicí okna.
- **MCP: vlastní JSON-RPC 2.0 přes Streamable HTTP** (cpp-httplib + nlohmann/json) na 127.0.0.1.
  - Důvod: pro C++ neexistuje oficiální SDK a protokol je malý. Server běží uvnitř emulátoru, takže Claude se připojí k běžící instanci s GUI.
- **Závislosti:** SDL3 přes CMake FetchContent, ostatní knihovny vendorované v `third_party/`.
  - Změna proti plánu: plán počítal s vcpkg. FetchContent ale funguje stejně ve VS, z příkazové řádky i na Linuxu, bez nastavování VCPKG_ROOT a baseline.
  - Na Linuxu se přednostně použije systémové SDL3.
- **Formát konfigurace sestavy: JSON s komentáři** (`machines/*.sapi`). Načítá se přes nlohmann/json s `ignore_comments`, takže stačí jedna knihovna pro konfiguraci i MCP.

## 2026-10-03 – Chování emulace

- **Přesnost na T-stavy**, reálná rychlost podle konfigurovatelného krystalu (JPR-1ZI: 16 MHz, děliče, CPU a Φ2 na 2 nebo 4 MHz). K dispozici turbo a pauza.
- **Dekodéry adres podle skutečných PROM dumpů.** Konfigurace jumperů pak automaticky odpovídá HW. Kde dump chybí, použije se tabulka z dokumentace s poznámkou.
- **Volná sběrnice čte FFh.** Konflikt dvou karet na datech se loguje jako varování.
- **EKL-1 se emuluje jen na úrovni portů JPR-1ZI**, bez vnitřku AT89C2051. Výchozí chování odpovídá Consul 262.3, později přibudou Consul 259.11 a ANK-1.
- **PMD32-SD se emuluje na úrovni protokolu přes 8255 desky IDE-1**, bez ATmega. Jako SD karta slouží složka na hostiteli.
- **IDE disk je surový `.img`** bez MBR, jak ho používá BIOS SAPI.
- **ZRMD-1 má 4 MB a režimy `rw`, `nosave` a `readonly`**, k tomu emulované přepínače WP S1–S4. RTC bere čas z hostitele.
- **Sériová linka:**
  1. Vlastní okno terminálu.
  2. TCP socket, ke kterému se připojí TeraTerm přímo, bez virtuálního COM ovladače.
  3. Hostitelský COM port.
  - Virtuální COM (com0com) jen jako návod.

## 2026-10-03 – Implementace F1

- **Model sběrnice:** karty dostávají přístupy postupně podle pořadí slotů (`mem_read/mem_write/io_read/io_write`).
  Výsledek čtení vrací karta, která odpověděla. Volná sběrnice čte FFh, dvě odpovídající karty se zapíšou
  do logu jako konflikt.
  - Čas se měří v taktech CPU (`Bus::now`). Karty se synchronizují líně, a to před každým I/O přístupem
    a každých 64 taktů. Ve stejném intervalu se vzorkují přerušení.
- **Wait stavy (RDY):** jádro z80.h testuje WAIT dřív, než vystaví požadavek na sběrnici. Čekání proto řešíme
  „zastavením“ CPU na N taktů po přístupu. Celkový počet taktů tím zůstává přesný.
- **Porty 00–03h JPR-1ZI** obsluhuje karta jako běžnou kartu na sběrnici. Díky tomu funguje i detekce konfliktů.
- **Breakpointy a hlášené PC:** hranice instrukce je takt M1 fetch bez aktivního prefixu. PC se hlásí jako adresa
  tohoto fetche. Po zastavení na breakpointu je opkód už načtený a po pokračování se instrukce dokončí.
- **EKL-1, vkládání textu:** skutečný firmware nečeká na ACK=0, takže program, který nechá ACK v 1, může přijít
  o znak. Při vkládání textu z PC (`strict: false`, výchozí) proto emulátor posílá další znak až po ACK=0.
  Mezera mezi znaky je 1 ms. `strict: true` se chová přesně jako firmware.
- **MHB1012, délka slova:** předpoklad b3 = NB1, b2 = NB2 (pro 8 bitů, které MikroMon používá, je jedno).
  Ověřit podle datasheetu AY-5-1013 nebo na HW.
- **Dva spustitelné soubory:**
  - `sapiemu.exe` je okenní aplikace bez konzole; s `--headless` se připojí ke konzoli, ze které byla spuštěna.
  - `sapiemu-cli.exe` je konzolový nástroj pro skripty a testy.
  - Argumenty se na Windows čtou jako UTF-8 přes `GetCommandLineW`, kvůli diakritice v `--type`.
- **Znaková sada:** převod KOI-8čs i semigrafiky na Unicode vychází přímo z obsahu znakového generátoru.
  Pořadí bitů semigrafiky odpovídá Unicode sextantům (U+1FB00) a ověřuje ho test.
- **Testy ZEXDOC/ZEXALL** běží jen v release buildu (label `slow`), v debug buildu trvají příliš dlouho.

## 2026-10-03 – Data a repozitář

- **ROM, PROM a všechny obrazy médií jsou v repu** (asi 14 MB). Emulátor má po naklonování fungovat hned.
  - Média v `media/` jsou nedotčené šablony, pracuje se s kopiemi ve `work/` (v `.gitignore`).
  - Duplicitní 10MB `.hex` RAM disku se vynechává.
- **MikroMon je jen jako binárka** (se souhlasem autora L. Lasoty), zdroje ne.
- **UI česky**, ale přes tabulku řetězců, aby šlo později přepínat. Font DejaVu Sans s diakritikou je vendorovaný.
- **Lokální commity průběžně, push jen na vyžádání.**

## 2026-10-03 – Implementace F2

- **Jedno emulační vlákno.** Emulace běží v hlavním vlákně GUI, případně ve smyčce `sapiemu-cli --mcp`.
  MCP server (cpp-httplib) běží ve vlastním vlákně a příkazy předává přes `EmuHost::call()`. Ty se provedou
  v `EmuHost::poll()` mezi snímky.
  - Důvod: stroj není potřeba zamykat a stav se nikdy nečte uprostřed instrukce.
  - Synchronní běhy z MCP (`run_for`, `run_until`) na dobu běhu blokují GUI. To je přijatelné, protože
    v release buildu jde o desetiny sekundy.
- **MCP bez SSE.** Na `POST` se odpovídá prostým JSON a `GET` vrací 405. To specifikace Streamable HTTP
  dovoluje a pro nástroje to stačí.
  - Server poslouchá jen na 127.0.0.1 a kontroluje hlavičku Origin.
  - Výchozí port je 8580.
- **Hodnoty v argumentech MCP:** řetězce jsou hex, čísla v JSON desítková. Odpovídá to zvyklostem
  při práci se SAPI (adresy se vždy píší hex).
- **Krokování:** CPU si pamatuje, zda stojí hned po fetchi instrukce (`at_boundary`). Krok pak buď skončí
  na příští hranici, nebo jednu přeskočí (po resetu a po změně PC). Krok přes CALL, RST, DJNZ a opakovaných
  blokových instrukcí používá dočasný breakpoint na adrese za instrukcí.
- **Watchpointy** se vyhodnocují na sběrnici (`Bus::mem_watch`, `io_watch`). Zápisy z debuggeru a MCP
  (`Machine::poke`) je nespouštějí.
- **Sériový kanál** si drží historii (1 MB) a každý čtenář (terminál, MCP, režim bez okna) má vlastní pozici.
- **Klávesy GUI:** F5 běh/pauza, F8 turbo, F10 krok přes, F11 krok, F12 reset. Odpovídá to zvyklostem
  Visual Studia pro F10/F11. Turbo se kvůli tomu přesunulo na F8.
- **Screenshot:** dvoubarevné obrazovky se ukládají jako PNG s 1bitovou paletou. Obrázek má desítky kB,
  bez nutnosti psát kompresor deflate.

## 2026-10-03 – Implementace F3

- **Dekódování ZRMD-1 přesně podle GAL:** 60h data, 61–63h jen zápis, 70–7Fh RTC; 64–6Fh nic.
  Ochrana zápisu S1–S4 po 1 MB.
- **Pracovní kopie médií:** obrazy z `media/` se při prvním použití zkopírují do `work/` (`media_path()`).
  Nastavením `working_copy: false` se použije přímo zadaný soubor.
- **RAM disk se ukládá** při ukončení, při načtení jiné sestavy a automaticky 5 s emulovaného času po změně.
  Je to kompromis mezi bezpečností dat a zápisem 4 MB.
- **RTC běží podle místního času PC** s posunem, takže nastavení času v SAPI se neprojeví na PC a „přežije“
  jen do ukončení. Posun se zatím neukládá.
- **Připojený terminál (DCD)** je vlastnost sériového kanálu, ne karty. Software se podle DCD rozhoduje,
  kam posílat konzoli, a uživatel to chce přepínat za běhu.
- **Výchozí sestava CP/M** má terminál odpojený (konzole na obrazovce). MikroMon má terminál připojený
  (ozvěna na UART).

## 2026-10-03 – Implementace F4

- **PMD32-SD se emuluje přepisem firmwaru, ne vlastní interpretací protokolu.** Hlavní smyčka `pmd32.c`
  je přepsaná jako stavový automat (makro `DO()`, jedno čekání na řádek). Tím zůstávají zachované všechny
  timeouty i zvláštnosti, například přepsání NAK prezentací.
- **SD karta = adresář** s `images.cfg`. Kopie z `media/pmd32sd` vzniká ve `work/pmd32sd`.
  Obsah EEPROM (vložené obrazy) nahrazuje konfigurace sestavy.
- **IDE disk = surový obraz**, ve výchozím stavu 128 MB (16 slotů po 8 MB) vyplněných E5h, takže
  CP/M vidí prázdné jednotky. Příkazy disk provede okamžitě. BIOS nemá timeouty, rychlost tedy nevadí.
- **Signály IDE se vyhodnocují na hranách** (/DIOR náběžná = čtení, /DIOW sestupná = zápis), jako na
  skutečné sběrnici. Proto funguje přesně posloupnost z `IDE.MAC`.

## 2026-10-03 – Sériová linka na straně PC

- **TCP server místo virtuálního COM.** TeraTerm umí TCP přímo a nepotřebuje ovladač. Virtuální COM (com0com)
  je jen volitelný návod pro programy, které umí výhradně COM.
- **Telnet se rozpoznává automaticky:** první bajt IAC od klienta přepne spojení do telnetu (vyjednání ECHO,
  SGA, BINARY; IAC v datech se zdvojuje; CR NUL / CR LF → CR). Jinak se data posílají beze změny (raw).
- **Na TCP server bez hesla** se lze ve výchozím stavu připojit jen z vlastního PC (bind 127.0.0.1).
- **DCD sleduje připojení klienta** (`dcd_follows`). ECMON a MikroMon volí konzoli podle DCD při startu.
- **„Bez přetečení“ (pacing) je výchozí.** Při vložení textu z PC (TeraTerm, okno terminálu, MCP)
  by se jinak znaky ztrácely kvůli přetečení UART, protože software je zpracovává pomaleji, než přicházejí.
  Kdo chce věrné chování skutečné linky, volbu vypne.
- **Most do sítě nebo COM portu běží ve vlastním vlákně** a s emulací komunikuje jen přes thread-safe
  `SerialChannel`. Před načtením jiné sestavy se mosty zastaví, aby nová sestava mohla otevřít stejný port.

## 2026-10-03 – Reset na F9 místo F12

- Pod debuggerem si Windows rezervují **F12** jako klávesu „zastav v debuggeru“ (UserDebuggerHotKey).
  Při ladění ve Visual Studiu proto F12 vyvolala „Breakpoint Instruction Executed“ a do emulátoru se
  nedostala. Reset je teď na **F9**, F12 emulátor nepoužívá.
- Doplnění: na přání autora je reset znovu i na **F12** (zvyklost emulátorů). F9 zůstává jako alternativa
  pro ladění pod debuggerem. Klávesu debuggeru lze ve Windows přesunout hodnotou `UserDebuggerHotKey`
  v `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\AeDebug` (např. 0x13 = Pause, platí po restartu).
  Emulátor to nemění.

## 2026-10-03 – Telnet jako výchozí protokol TCP, IPv6

- **Telnet je výchozí.** Původní automatika (telnet až po IAC od klienta) nestačila: TeraTerm v režimu Telnet
  čeká na nabídku serveru a bez WILL ECHO / WILL SGA zůstal v řádkovém režimu (posílal až po Enteru).
  Server teď nabídku posílá hned po připojení a nepodporované volby klienta odmítne (DONT/WONT).
  Režimy `raw` a `auto` zůstávají volitelné.
- **TCP server na loopbacku poslouchá i na IPv6 `::1`.** `telnet localhost 2323` ve Windows zkouší nejdřív
  IPv6 a se samotným IPv4 se nepřipojil.

## 2026-10-03 – DCD a CTS zvlášť, doba přenosu přijímaného znaku

- **CTS/DSR jsou samostatné, nezávislé na DCD.** DOPC10.COM (přenos XMODEM s řízením RTS/CTS) čekal
  na CTS, které bylo svázané s volbou „Terminál připojen (DCD)“. Tu autor měl vypnutou, aby konzole ECMON
  zůstala na obrazovce. Na skutečné lince jde o dva signály: DCD volí konzoli, CTS povoluje vysílání.
  CTS je ve výchozím stavu aktivní (`cts` v DMR-1, okno terminálu, MCP).
- **Přijímaný znak je v SAPI k dispozici až po době svého přenosu po lince** (start, data, stop bity).
  Dřív se objevil okamžitě. DOPC po odvysílání paketu vyprázdní přijímač, takže okamžité ACK zahodil
  a čekal do timeoutu. Teď ACK přijde až po 1 ms (9600 Bd), jako na skutečném drátě.

## 2026-10-03 – Bezpečnostní opravy (security review)

- **MCP server:** kontrola `Origin` porovnávala jen začátek řetězce, takže `http://localhost.attacker.tld`
  prošel. Kontrola `Host` chyběla, takže šel použít DNS rebinding. Cizí webová stránka tak mohla přes
  `load_binary` a `read_memory` přečíst libovolný soubor uživatele.
  - Teď: `Host` musí být loopback, `Origin` (je-li uvedený) přesně loopback na libovolném portu a
    `Content-Type` musí být JSON.
  - Token ani omezení cest jsme nezavedli, aby se nezhoršilo pohodlí. Lokální programy mají stejná práva
    jako uživatel.
- **PMD32-SD:** jméno obrazu od programu v emulátoru (příkazy H a N) se nekontrolovalo. Šlo tak přistoupit
  k souborům mimo SD kartu, a přes síťovou cestu dokonce prozradit hash hesla k Windows. Jména jsou teď
  omezená na cesty uvnitř SD karty (`Pmd32Sd::safe_path`). Podadresáře zůstávají povolené.

## 2026-10-03 – Výběr klávesnice

- **Podporované klávesnice:** EKL-1, Consul 262.3, Consul 259.11 a ANK-1. Volí se v sestavě, za běhu
  v menu Stroj → Klávesnice, v MCP (`set_keyboard`) a v `sapiemu-cli` (`--keyboard`). Výměna za běhu
  odpovídá přepojení konektoru X2: nová klávesnice hned vidí aktuální stav P0-OUT.
- **Consul 262.3 je ve výchozím stavu s úpravou 7474** (strobe drží do potvrzení), protože s původním
  pulzem 1 ms může program při pollingu klávesu přehlédnout. Původní chování je preset „bez 7474“.
- **Consul 259.11 má dvě varianty v konfiguraci:** KOI-7 s klávesou CLEAR na P1-IN7 (výchozí, podle
  dokumentace) a KOI-8 cs s osmým bitem na P1-IN7 (ovladač ECMON „CONSUL 259.11 KOI8 CS2“).
- **ANK-1 je hotová teď a ověřená testem na portech**, přestože pro JPR-1ZI zatím nemá software. Text z PC
  se převádí na stisky kláves matice (SHIFT podle tabulek MIKOS V4.0), každý stisk trvá 40 ms.
- **Mapování kláves PC (schváleno autorem):** šipky na šipky, numerika na B0–B9/AE (262.3), Home = 8Dh,
  F1 ROL, F2 COPY, F3 BREAK, F4/F6/F7 = klávesy 61–63 (D0–D2), Pause = CLEAR (259.11). F5 a F8–F12 zůstávají
  emulátoru. Rozhraní klávesnice dostává text (Unicode) a klávesy bez textu (`HostKey`) zvlášť, takže
  frontend nezávisí na typu klávesnice.
- **EKL-1 posílá češtinu s prohozenou velikostí** (KOI-8 cs XOR 20h) jako jeho firmware. Dřív emulace
  posílala přímý kód KOI-8 cs. ECMON a CP/M velikost vracejí, MikroMon ne (nejasnost 13 v SOUHRN).

## 2026-10-03 – Konfigurátor sestavy

- **Okno Sestava ukazuje rám zepředu:** X1 (procesor) vpravo vedle panelů SPN a ZDR, další sloty doleva.
  Zatím je kreslený, později mohou přibýt fotky panelů jednotlivých karet.
- **Počet slotů podle typu sběrnice** (rozhodnutí autora): ARB-1 8, ARB-1A/Au 13, ARB-1B 14, ARB-1C 14
  (X1–X14 podle seznamu součástek), ARB-1C-8 = 8slotová deska ZPC. Dřívější `"ARB-1C", "slots": 8` je teď
  `ARB-1C-8`.
- **Změna sestavy se projeví vypnutím a zapnutím** (rozhodnutí autora). Úpravy se dělají v kopii, nejdřív se
  uloží RAM disky staré sestavy, pak se postaví nová.
- **Sestava je obyčejný soubor kdekoli** (rozhodnutí autora). Výchozí je `machines/` vedle emulátoru,
  GitHub slouží jen vývoji. Šablony ZPS se dodávají v `machines/zps/`.
- **Neemulované karty jsou v katalogu taky** (rozhodnutí autora). Zabírají slot, ale neodpovídají
  (`PlaceholderCard`). Sestavu s neemulovaným procesorem lze upravit a uložit, ale nespustí se.
- **Jeden popis parametrů v katalogu** (`src/cards/catalog.cpp`) slouží formuláři, komentářům v uloženém souboru,
  kontrole i MCP. Texty katalogu mají anglickou a českou variantu přímo u dat. Nejdou přes tabulku `tr()`, aby
  popis jumperu nebyl na dvou místech.
- **Uložený soubor generuje program** (klíče v pořadí katalogu, komentář u každého parametru). Ruční komentáře
  se při uložení z konfigurátoru ztratí. Ručně psané sestavy dál fungují.
- **RPD-1 zabírá dva sloty** (desky RPD-1A a RPD-1B spojené kabelem KB-17). V souboru se uvádí první slot.
- **Kontrola GUI bez snímku plochy:** `sapiemu --ui-snapshot` vykreslí okno skrytě a uloží obsah vlastního
  rendereru do PNG.

## 2026-10-03 – Rychlost 2 / 4 MHz / max

- **TURBO a F8 přepínají dokola:** takt CPU sestavy → 4 MHz (jen když je takt nižší) → maximum. Volba je i
  v menu Stroj → Rychlost a v MCP (`resume` s `speed_mhz`).
- **„4 MHz“ zrychluje celou emulaci** (emulované takty za sekundu), ne jen procesor. Proti skutečné JPR-1ZI
  s CPU na 4 MHz se tím zrychlí i UART a časování karet, které běží na vlastním krystalu. U terminálu a TCP
  na tom nezáleží. Skutečné přepnutí taktu CPU (jumper JP2/JP3, TURBO u JPR-1V) by potřebovalo přepočet
  časování všech karet, zatím není.
- **Výběr sestavy z menu Soubor padal:** menu dál používalo ukazatel na právě zrušený stroj (podmenu
  Klávesnice). Po načtení sestavy se teď ukazatel načte znovu, stejně u tlačítka NAPÁJENÍ.

## 2026-10-03 – Zamrzlá klávesnice, pád při změně klávesnice

- **Klávesnice „zamrzla“:** backend ImGui pro SDL3 vypne textový vstup SDL (`SDL_StopTextInput`), jakmile textové
  pole ImGui (debugger, konfigurátor) ztratí fokus. Pak nechodily znaky, jen klávesy bez textu. Hlavní smyčka
  teď textový vstup v každém snímku znovu zapne.
- **Pád v menu Stroj → Klávesnice:** smyčka přes nabídku dál používala ukazatel na právě vyměněnou klávesnici.
- **Diagnostika:** když program nebere klávesy, stavový řádek ukáže „čeká kláves: n“.

## 2026-10-03 – Pamatování nastavení GUI

- **Umístění (rozhodnutí autora):** `sapiemu-settings.json` a `imgui.ini` ve složce s exe, aby emulátor zůstal
  přenosný. Když tam nejde zapisovat, použije se `%APPDATA%\SAPIemu\SAPIemu`. Rozložení oken ze starších verzí
  (jen v AppData) se při prvním spuštění zkopíruje k exe. Každé sestavení (debug, release) má tedy své nastavení.
- **Co se pamatuje (rozhodnutí autora, vše):** poslední sestava, velikost, poloha a maximalizace okna, otevřená
  okna, luminofor, škálování, poměr TV, vyhlazování, typy karet, volby terminálu (emulace, CR, echo, TCP port
  a protokol, COM) a rychlost.
- **Poslední sestava** je jen taková, která se úspěšně spustila (ne neuložená sestava z konfigurátoru). Když
  soubor zmizí nebo nejde spustit, naběhne MikroMon. `--machine` má přednost.
- **Ukládá se při ukončení a každé 3 s, pokud se něco změnilo** (při pádu se nic neztratí). Okno se otevře na
  uložené pozici jen tehdy, když leží na některém připojeném monitoru.
- `--settings soubor.json` použije jiný soubor (profily, testy). `--ui-snapshot` bez něj nastavení nečte ani
  nezapisuje.

## 2026-10-03 – Soubory sestav s příponou .sapi

- **Sestavy mají příponu `.sapi`** (přání autora), obsah zůstává JSON s komentáři. Konfigurátor ukládá `.sapi`,
  dialogy a seznam sestav berou i starší `.jsonc` a `.json`. Uložená „poslední sestava“ s příponou `.jsonc` se
  při startu převede na `.sapi`.

## 2026-10-03 – Vypínač NAPÁJENÍ a baterie DMR-1

- **NAPÁJENÍ je vypínač s aretací** (rozhodnutí autora).
  - Vypnuto: stroj neexistuje. Obrazovka je tmavá a LED zhasnou. RAM disky a SRAM s baterií se uloží, TCP a COM
    se zavřou, MCP hlásí „switched off“.
  - Zapnutí postaví stroj znovu z popisu v konfigurátoru, včetně neuložených změn.
  - RESET je jen /RES: RAM zůstane, změny konfigurace se nepoužijí.
  - Rychlost 4 MHz vypnutí přežije, stejně jako přepínač na skutečném rámu.
- **DMR-1 JP7 baterie** (rozhodnutí autora). Parametr `battery` je ve výchozím stavu zapnutý, obsah SRAM se drží
  v `work/dmr-1/sram.bin` a přežije vypnutí i ukončení emulátoru. Testy zápis vypínají
  (`battery_files_enable(false)` v `test_init()`), aby nepřepisovaly soubory uživatele.

## 2026-10-03 – Více sériových karet, port na panelu

- **DMR-1 i DSM-1 může být v sestavě víc** (autor). Druhá DMR-1 má vypnutou RAM (parametr `ram`) a UART na
  jiném portu. Karty stejného typu dostanou jméno podle slotu („DMR-1 X4“), sériové kanály podle portu
  („DMR-1 #14“, dřív „DMR-1@10h“).
- **Port na panelu** (autor): sériové karty mají pod zkratkou `#10`, `#14`… (nahoře i dole), stejně v seznamech
  a bublinách. Port udává parametr v katalogu (`card_port_param`: DMR-1 `uart_base`, DSM-1 `base`).
- **Kontrola sestavy:** dvě karty na stejném portu jsou chyba. Dvě DMR-1 s RAM nebo se společným souborem baterie
  jsou varování.
- DSM-1 (zatím neemulovaná) má v katalogu porty (propojky 16-12 až 16-15, z výroby 10h) a rychlost terminálu
  (z výroby 600 Bd).

## 2026-10-03 – Mapa portů a paměti

- **Mapa vychází z dekódování samotných karet** (`Card::decode_io`, `decode_mem`). Tatáž logika podle PROM dumpů
  a jumperů obsluhuje i běh emulace, takže mapa nemůže ukazovat něco jiného, než co karta za běhu dělá.
- **Karty se pro mapu staví ve zkušebním režimu** (`Card::probe`): ROM a PROM se načtou, ale obrazy disků se
  neotevírají, nevytvářejí ani nekopírují do `work/`.
- **Kolize = dvě karty budí data při čtení.** Je to chyba a sestava se nezapne. EPROM procesorové karty
  (BOOT = 1) se čte na kartě, proto s RAM pod ní nekoliduje. Zápis do dvou karet je jen varování.
- Mapa se počítá pro všechny čtyři stavy MAP1/MAP2. Volba stavu se ukáže, jen když na nich něco závisí.
- **Neemulované sériové karty** jsou v mapě s porty z konfigurátoru, takže kolize DSM-1 s DMR-1 je vidět i
  bez emulace DSM-1. Dřívější kontrola „stejné porty“ a varování „dvě DMR-1 s RAM“ v rychlé kontrole sestavy
  jsou nahrazené mapou (dvě RAM přes sebe jsou teď kolize, tedy chyba).
- Konfigurátor mapu přepočítává jen při změně sestavy. MCP `get_machine_config` a `set_machine_config`
  hlásí i kolize z mapy.

## 2026-10-03 – 64bitový překladač ve Visual Studiu

- **VS hlásilo C1060 „compiler is out of heap space“,** ale z příkazové řádky (`tools\build.cmd`) šel stejný preset
  přeložit. Presety určovaly jen cílovou architekturu (`x64`, external). VS pak zřejmě volí 32bitový hostitelský
  překladač (Hostx86), který má strop 4 GB na proces a na šablonách `json.hpp` mu dojde paměť.
- **Řešení:** `"toolset": { "value": "host=x64", "strategy": "external" }` v `windows-base`. Ve VS je potřeba
  Delete Cache and Reconfigure.

## 2026-10-03 – ZPS-1: JPR-1, REM-1, AND-1, MIKROBASIC

- **Jádro 8080:** převzato `superzazu/8080` (MIT, v C, krokuje po instrukcích), ne vlastní. Prošlo
  TST8080, 8080PRE, CPUTEST i 8080EXM. Karta JPR-1 si hranice instrukcí, breakpointy a krokování řeší sama
  (napodobuje chování JPR-1ZI: zastavení před instrukcí na `instr_pc`).
- **Dvě sestavy MIKROBASICu** (autor): 2.4 z výroby ZPS-1 (EPROM a RAM na REM-1) jako hlavní a starší verze
  z AR/B 1983 (EPROM a 1 KB RAM na JPR-1). ROM jsou v `roms/mikrobasic/` (autor souhlasí), výpisy ne.
- **Znakový generátor AND-1 je překreslený** z tabulky v katalogu MHB250x (autor), protože dump p-MOS ROM
  neexistuje. Převod automaticky po buňkách mřížky 5×8, několik vad skenu opraveno ručně.
- **ANK-1 drží klávesu, dokud ji program nepřečte**, a pak ji stejně drží uvolněnou. MIKROBASIC po každé klávese
  ~65 ms pípá a další bere až po kole bez stisku, pevné časování by při rychlém psaní znaky ztrácelo.
- **ANK-1 má volbu programu** (`mikrobasic` / `mikos`). Dva programy čtou stejnou matici s jinými tabulkami
  (klávesa vedle CR, SHIFT+písmeno).
- **AND-1:** skládání 64 adres řádku do 1 KB VRAM je předpoklad. Na viditelný obraz MIKROBASICu nemá vliv.
  Režimy (blikání, kurzor, dvojitá šířka) se kreslí podle popisu, PROM 74188 se zatím nečte.
- **IDE-1 odpovídá na portech vždy** (autor: jako skutečná deska), i bez disku nebo PMD32-SD.
- `DisplayDevice::dot_clock_hz()`: poměr TV se počítá podle bodového taktu karty (AND-1 6 MHz, AND-1Z 10 MHz).

## 2026-10-03 – DSM-1 a magnetofon

- **Magnetofon jako skutečný** (autor): tlačítka PŘEHRÁT, NAHRÁT, STOP a Převinout, motor ovládá relé DSM-1.
  Vložené kazety jsou chráněné proti zápisu, nahrává se na novou kazetu nebo po vypnutí ochrany.
- **Kazety odkudkoli ve WAV i MP3** (autor). MP3 dekóduje vendorovaná minimp3 (CC0). Nahrávky nejsou v repu,
  zůstávají v `H:\SAPI-1\SAPI_DOKU_2024-01-03\Kazetopáskové vybavení AUDIO WAV\` (test je přeskočí, když chybí).
- **Fázový dekodér místo prostého rozhodování podle délky intervalů.** Popis v manuálu (208/416 µs, práh čítače
  12) na čistých WAV fungoval, na MP3 kopiích dával stovky chyb rámce: jeden zákmit otočil úroveň linky. Fázový
  dekodér (mřížka bitů podle prvního start bitu, PLL, rozhodnutí každého bitu porovnáním polovin) čte všechny
  nahrávky bez chyby.
- **UART MHB1012 dostal bitovou úroveň** (`rx_inject`, `tx_log`). Znaková úroveň zůstává pro terminál, kazeta
  skládá znaky z demodulované linky přímo v kartě.

## 2026-10-03 – MIKOS a RAM-1

- **ZPS-2 běží s MIKOSem V4.0F místo MIKROBASICu.** To odpovídá dodávanému programovému vybavení ZPS-2 (manuál
  Kazetopáskové programové vybavení V4.0). MIKROBASIC zůstává v ZPS-1.
- **EPROM 2708 (4× 1 KB).** První 1 KB dumpu 2708 od PEPAXe se shoduje s V4.0F.
- **Přerušení v sestavě vypnutá** (AND-1 `int1`, DSM-1 `int0`, ANK-1 `int_key`), jak radí manuál, protože
  BASIC EX povolí přerušení a vektory RST v RAM nejsou připravené.
- **PROM D1A z RAM-1 se generuje z tabulky 1 manuálu.** Dump zatím nemáme.
- **Rozběh motoru magnetofonu = 50 ms ticha při nahrávání.** Navazující záznamy (`.K_A` a `.K_S`) jinak
  dělaly fázový skok, který fázový dekodér četl jako start bit. Skutečný magnetofon se také chvíli rozbíhá.

## 2026-10-03 – BDK-1

- **BDK-1 (bastldeska) je emulovaná jako prázdná karta** (autor). Je to vyvrtaná DPS pro vlastní zapojení
  uživatele, bez funkce. V ZPS-1 a ZPS-2 zůstává kvůli úplnosti, na sběrnici nic nedělá a jen zabírá slot.

## 2026-10-03 – MIKOS V5.0

- **Samostatná sestava `ZPS-2_MIKOS_V5.sapi`** vedle ZPS-2 s V4: stejné karty, jiná EPROM a klávesnice Consul
  262.3 s úpravou STROBE (autor: V5 běží na ZPS-2, jen chce Consul místo ANK-1).
- **ROM jen 5 KB (monitor + MIKOS).** Ovladač XY 4130 z `MIKOS5.BIN` se nepřidává. Má autostart z 1800h a
  sestava nemá zapisovač.
- **Výchozí V5.0F** (novější), V5.0E je v katalogu REM-1 jako alternativa.

## 2026-10-03 – DGD-1 a BASIC EXP

- **Grafika DGD-1 ve vlastním okně** (autor). Skutečná sestava měla dva videovýstupy (AND-1 a DGD-1). Konzole
  (`Machine::display`) je první textový displej, další výstupy jsou v `Machine::displays`.
- **Výběr DGD-1 podle tabulky SP1, ne podle PROM D1A.** Dump existuje, ale zapojení vstupů neznáme.
- **BASIC EXP jako Intel HEX v `media/mikos/`**, protože nahrávky na kazetě nemáme. Do GUI přibylo nahrání HEX do
  paměti (Soubor → Nahrát program do paměti) a do `sapiemu-cli` akce `--load-hex`.
- **Sestava `ZPS-2_MIKOS_V5.sapi` má DGD-1** a RAM-1 přepnutou na 4000–BFFF, jak předepisuje návod DGD-1.

## 2026-10-03 – RPD-1 a MIKOS/D

- **RPD-1 jako rotující stopa FM** generovaná z `.img`. Bajty přicházejí podle emulovaného času (32 µs),
  software (EBIOS) si značky, CRC i prokládání dělá sám jako na skutečném HW. Žádné zkratky na úrovni
  sektorů, aby fungovaly i jiné ovladače (CP/M na ZPS-3).
- **Obrazy v logickém pořadí (.img)**, ne IMD. Disketa MIKOS/D má fyzicky sektory 1..26 po sobě, IMD nepřináší
  nic navíc.
- **HLR nezávisí na vložené disketě** (monostabilní obvod po 3Dh). EBIOS na HLR čeká bez limitu a bez diskety
  by systém zamrzl místo hlášky „NELZE ZAVEST DISKOVY SYSTEM“.
- **Nová disketa = celá E5h** (naformátovaná, prázdný adresář CP/M). Formátování přes RPD-1 EBIOS neumí.

## 2026-10-03 – Dekodéry z PROM jako vyměnitelné soubory

- **Autor:** PROM dekodérů v SAPI sloužily k tomu, aby si uživatel změnil adresování. Kde nestačily
  propojky, osadila se jiná PROM. Zatím platí, co najdeme, ale do budoucna může být potřeba kartu adresovat
  jinde.
- **Proto:** u každé karty s dekodérem v PROM je obsah PROM parametrem (soubor v `roms/`, `PK_FILE`
  s návrhy). Výchozí je dump nebo tabulka z návodu. Kód karty z PROM jen čte vstupy a výstupy podle zapojení
  ze schématu a nemá v sobě natvrdo rozsahy adres. Týká se JPR-1A D2C, AND-1A D3A, RAM-1/RAM-1A D1A
  (RAM-1 se z generované tabulky převede na soubor) a dalších karet.

## 2026-10-03 – ZPS-3

- **JPR-1A je varianta třídy Jpr1** (stejné 8080, jiný dekodér a periferie), AND-1A a RAM-1A jsou typy tříd
  And1 a Ram1. Dekodéry jsou soubory PROM podle zásady výše.
- **RAM-1 převedena z generované tabulky na soubor PROM** z návodu. Obsah je shodný.
- **Sestava ZPS-3 má DSM-1 bez připojeného terminálu** (`connected: false`), aby MON V1.0-C1 zvolil AND-1A
  a klávesnici. S připojeným terminálem jde konzole na sériovou linku jako na skutečném stroji.
- **První CP/M na ZPS-3 je Peřinův CP/M 2.2/P** (bootovatelná disketa ze sapi.cz). Originální IMA 48K čeká na
  disketu od autora. Originální monitor V1.0-C1 Peřinovu disketu zavede.

## 2026-10-04 – ZPS-4

- **JPR-1Z = typ třídy JPR-1ZI** (EPROM 2716 zrcadlená, vlastní dumpy D14/D10). Obvod XINTA emulace ZI
  nemodeluje, takže v tom rozdíl není.
- **RAM-1Z a RMD-1 s dekodéry jako soubory PROM** (zásada z 2026-10-03). Přiřazení O3/O4 u RMD-1 podle
  softwaru, ne podle popisků schématu (ověří kamarád autora).
- **RMD-1 po zapnutí s náhodným obsahem** jako skutečná DRAM. Volba `power_on: e5` a soubor `image` jsou jen
  pohodlí emulátoru.
- **Sestavy ZPS-4 s Consulem 262.3** (autor: ZPS-4 je novější). ELIS výchozí v7.01, protože v3.00 má
  nevyjasněnou obsluhu klávesnice.

## 2026-10-04 – Desky V (Libor Lasota)

- **Desky s V za číslem** (JPR-1V, RAM-1V, AND-1V, DSM-1V, ZRD-1V) jsou nové desky Libora Lasoty. Vycházejí
  z původních, ale každá má vlastní vylepšení (autor):
  - JPR-1V je rychlejší (Z80 až 4 MHz);
  - RAM-1V je silně konfigurovatelná a dělá mapování (MAP);
  - AND-1V má výstup VGA;
  - DSM-1V má dva sériové kanály a nemá kazetu;
  - ZRD-1V se zálohuje tužkovými články AAA, nemá RTC ani mapování.
- Na deskách V běží stejný systém jako se ZRMD-1 (ECMON, CP/M z RAM disku) a funguje s nimi boot ROM i IDE-1.
- Informace: podklady v `..\SAPI-karty\JPR-1V`, `..\SAPI-karty\RAM-1V`, `..\SAPI-karty\AND-1V`, `..\SAPI-karty\DSM-1V`, `..\SAPI-karty\ZRD-1V` a
  https://www.pmd85.cz/?page_id=292. Další doplní autor.

## 2026-10-04 – Desky V: odpovědi autora

- **JPR-1V:** banka 0 EEPROM = MikroBIOS 3.0, banka 1 = ECMON IDE-1 (autor si ROM programuje dynamicky přes
  PicoROM, skutečný obsah 28C64 nemá).
- **AND-1V:** emulovat rev 2.3. Autor ji má na MAP1 = L kvůli budoucí CGA-1V. ECMON ani CP/M mapování
  nepoužívají, výchozí sestava může mít MAP ignorovaný nebo L.
- **RAM-1V:** rev 1.1, RTC Timekeeperu v patici C volitelně.
- **DSM-1V:** výchozí kanály 10h a 14h, oba 115200 Bd.
- **Do repa:** binárka MikroBIOS 3.0 a fonty AND-1V ano, zdroje ne.

## 2026-10-04 – Desky V: emulace

- **JPR-1V = typ JPR-1ZI** (pevná logika odpovídá D3C 2k a D6B, banky EEPROM propojkou BIOS).
- **DSM-1V = dva kanály třídy DSM-1** bez kazety v jedné kartě.
- **AND-1V a RAM-1V jsou nové karty.** Dekodéry jsou propojky, ne PROM, a všechny jsou parametry.
- **Výchozí propojky podle fotek autorových desek:**
  - RAM-1V: registr MAP na 63h (bity 6/7, jako ZRMD-1), díry C000/MAP1 = H a F800/MAP1 = L;
  - AND-1V: MAP1 = L;
  - DSM-1V: 10h/14h, 115200 Bd.
- **ZRD-1V:** reset nenuluje adresu (schéma). Vedlejší účinky čtení 61h–63h se neemulují.
- **Zjednodušení:**
  - čekací stavy AND-1V se neemulují;
  - TURBO JPR-1V jde nastavit jen v sestavě, ne za chodu;
  - STSTB bez refreshe se neemuluje.

## 2026-10-04 – TURBO JPR-1V za chodu

- **Autor:** na originálu TURBO za chodu funguje perfektně, má jít přepínat i v emulátoru.
- **Řešení:**
  - časová základna sběrnice zůstává pevná (osc/2 = 4 MHz) a při 2 MHz CPU vynechá každou druhou jednotku;
  - karty počítají čas v jednotkách sběrnice, takže přepnutí za chodu nic nerozbije;
  - obecné rozhraní `CpuCard::has_cpu_turbo` / `set_cpu_turbo` / `cpu_display_hz`.
- **Ovládání:** menu Rychlost a MCP `cpu_turbo`. TURBO/F8 emulátoru dál mění rychlost celé emulace.

## 2026-10-04 – CGA-1V

- **Podle schématu rev 1.1:**
  - registry 3FC0–3FFF (A2 = 0 CONFIG, A2 = 1 Bt476 RS0/RS1 = A0/A1);
  - zápis jde i do SRAM;
  - čtení vrací SRAM s D7 = VBI.
- **Výchozí jumpery z fotky autorovy desky:** C000h, MAP1 = MAP2 = H.
- **Prolínání dvou posledních snímků** (volba `frame_blend`, výchozí zapnuto):
  - software střídající stránky a palety po snímcích (REMBRAN2) počítá s tím, že barvy smíchá oko;
  - GUI kreslí nezávisle na snímcích emulace, takže bez prolínání by obraz nahodile blikal;
  - živý stav se kreslí, když se snímky neliší, takže zápisy jsou vidět i při zastaveném CPU.
- **Čekací stavy přibližně:** do konce okna CPU 318 ns.
- **Testy a pracovní kopie:**
  - ZRD-1V/ZRMD-1 v režimu `rw` při vypnutých souborech zálohované paměti (`battery_files_enable(false)`,
    testy) neukládá;
  - testy dřív zapisovaly do `work/zrmd-1/`.
- **Liborovy programy pro CGA-1V** (castle, rembran1/2, cga_test) smí do repa jako binárky (autor).

## 2026-10-04 – Repo na lokálním disku

- **Autor přesunul repo i podpůrné repozitáře desek** z `H:\SAPI-1\GIT\` (síťový disk) do `E:\SAPI_GIT\`.
  Důvod: sestavení ve VS na síťovém disku zlobilo (IntelliSense do `.vs\`, pomalost).
- **Archiv zůstává na `H:\SAPI-1\`** (záloha sapi.cz, `LiborLasota`, `_ROM`, nahrávky kazet). Odkazy v `docs/`
  a cesty v testech (`test_mikos`, `test_cassette`) na něj platí dál. Relativní odkazy `..\<deska>` také.
- **Po přesunu** se smazalo `out\build\*` (CMakeCache s cestami na `H:`), nastavení GUI se zachovalo a sestavení
  je čisté, 24 testů prochází. Lokální preset `local-debug` (`C:\SAPIemu-build`) už na tomto PC není potřeba.

## 2026-10-04 – Barva obrazu v sestavě, okna podle karet

- **Barva jednobarevného obrazu je parametr karty** (autor: nastavení v menu i v jumperech bylo nelogické).
  - AND-1V má barvu na propojkách S1 (`color`), jak to je na desce. Volba „podle nastavení emulátoru“ zmizela.
  - Karty bez propojek barvy (AND-1, AND-1A, AND-1Z, AND-1Z-89, DGD-1) dostaly parametr `monitor`: barva je dána
    luminoforem připojeného monitoru (bílá, zelená, jantarová), v konfigurátoru je označený „není na kartě“.
  - Menu Zobrazení → Barva i konfigurátor mění tentýž parametr a změna se projeví hned, bez vypnutí (barva
    neovlivňuje běh programu). Uloží se se sestavou. Globální „Luminofor“ v `sapiemu-settings.json` se už nečte.
  - Snímky přes MCP a `sapiemu-cli` mají stejnou barvu jako GUI.
- **Okna obrazovek se jmenují podle karty** (autor): „AND-1V“, „CGA-1V“, „DGD-1“ místo „Obrazovka“ a
  „Grafika …“. Okna mají pevné id (`###screen`, `###gfxN`), takže uložená poloha zůstává i po výměně karty.
  Starý záznam „Obrazovka“ v `imgui.ini` se při startu převede.
- **Sestava V má AND-1V jantarovou** (autor).

## 2026-10-04 – Konfigurátor vždy ukazuje otevřenou sestavu, zvětšení v titulku okna

- **Chyba (autor):** po otevření jiné sestavy z menu se okno Sestava nezměnilo. Konfigurátor sledoval běžící
  sestavu jen bez neuložených změn a ty vznikají i barvou z menu Zobrazení → Barva.
- **Teď (autor: odkládání stranou je nelogické):** před nahrazením neuložené sestavy v konfigurátoru se emulátor
  zeptá **Uložit / Uložit jako… / Zahodit / Zrušit**, jako běžné programy. Týká se otevření jiné sestavy (menu,
  Otevřít soubor…), Nová a Otevřít… v konfigurátoru a ukončení emulátoru. Sestavu přepnutou přes MCP nejde
  předem zastavit, dotaz proto přijde potom a Zrušit ponechá změny v konfigurátoru. Vypnutí a zapnutí stejné
  sestavy se neptá a změny zachová.
- **Zvětšení v titulku okna obrazovky** (autor): v celých procentech („CGA-1V  200 %“), u všech videovýstupů.
- **Rychlé zvětšení 100/200/300/400 %** (autor): pravé tlačítko na obraz, pro hlavní obrazovku i menu Zobrazení.
  Okno se nastaví kolem obrazu s malým okrajem (autor: ne natěsno). Ruční změna velikosti okna vrátí přizpůsobení oknu.

## 2026-10-04 – Sestavy se ukládají s konci řádků platformy

- **Autor:** sestava uložená z konfigurátoru (nebo MCP `save_as`) měla LF, pracovní kopie z gitu CRLF, takže git
  hlásil změněný soubor i beze změny obsahu.
- **Teď:** `file_write_text()` zapisuje konce řádků platformy (CRLF na Windows, LF jinde), stejně jako git checkout
  s `text=auto`. Hlídá to test `test_machine_cfg` (`test_line_ends`).

## 2026-10-04 – Port u konektoru, konektory FRB podle štítků

- **Autor:** port sériové karty patří k jejímu konektoru, ne ke jménu karty na panelu. U DSM-1V je u každého
  kanálu („#10“ u K3 „A“, „#14“ u K2 „B“), u DSM-1 a DMR-1 „K3 #10“. V seznamech, bublinách a v terminálu zůstává
  „DMR-1 #10“ (autor: jen panel).
- **Autor:** desky původní konstrukce mají konektory FRB v hranatých otvorech, desky V D-sub. Panely JPR (kromě
  JPR-1V), FDC, DSM-1 a DMR-1 jsou nakreslené podle první sady Liborových štítků (otvor 22,7 × 150,2 bodu vlevo od
  středu, JPR s otvorem pro LED). DSM-1: K3 = X2 terminál, K2 = X3 magnetofon. DMR-1 má jen X2 na stejném místě.
  Typ karty je u panelů s FRB svisle v pruhu vpravo od otvorů.

## 2026-10-04 – Konektory na panelech, DGD-1V

- **Autor:** všechny JPR (i JPR-1V) mají K3 a K2 jako FRB. Původní displeje AND-1, AND-1A, AND-1Z, AND-1Z-89
  a DGD-1 mají jeden FRB (na deskách dole, K2). VGA D-sub mají AND-1V, CGA-1V, BGP a DGD-1V. FRB se kreslí jako
  zelený obdélník se zlatými piny (fotka konektoru od autora).
- **DGD-1V (autor: stojí za to ji emulovat)** je varianta třídy DGD-1: stejná mapa videoram, VGA (body 2×2),
  S2 se stejnou logikou jako S1 CGA-1V, S1 barva jako AND-1V (sdílená funkce `vga_ink`). Výchozí C000h bez
  mapování podle prvního příkladu popisu. Sestava `ZPS-2_MIKOS_V5_DGD-1V.sapi` má jantarovou barvu jako
  autorova deska.

## 2026-10-04 – Panely IDE-1, RPD-1 a karet bez konektoru

- **Autor:** IDE-1 má IDC 40 pro disk a dvouřadý D-sub 15 pro PMD32-SD. Rozmístění podle fotky desky: IDC
  nahoře, LED, D-sub dole. Konektory jsou vlevo od středu jako FRB, typ karty je v pruhu vpravo.
- **Autor:** RPD-1A a RPD-1B mají FRB a jsou propojené. Podle fotek desek je spojka nahoře na obou deskách
  (kreslí se kabel KB-17 přes oba panely), dolní FRB na RPD-1B je pro mechaniky.
- **Autor:** ZRMD-1, RAM-1, RAM-1A, RAM-1Z, RAM-1V a REM-1 konektor na panelu nemají.

## 2026-10-04 – LED na panelu podle emulace

- **Autor:** LED na panelu JPR má ukazovat skutečný stav z emulace.
- `Card::panel_leds()` vrací stav LED panelu (bit n = n-tá LED nákresu), konfigurátor ho u běžící sestavy čte
  v každém snímku. JPR-1ZI, JPR-1Z, JPR-1V a JPR-1A: klopný obvod BOOT. JPR-1: P1-OUT D7 (polarita 1 = svítí je
  předpoklad). LED JPR je červená. Další LED (IDE-1, ZRD-1V) zatím jen nakreslené.

## 2026-10-04 – LED IDE-1 a ZRD-1V

- **IDE-1** LED nemá, emulátor ji na panelu kreslí (autor: může se hodit) a rozsvítí ji na 50 ms po každém
  přístupu na 8255 disku nebo PMD32-SD.
- **ZRD-1V** (autor: LED podle rev 2.0, jedna zámek zápisu, druhá přístup): DSK svítí 20 ms po přístupu k datům
  (74LS123 „20 msec“), žlutá. **ZAM podle zámku zápisu v konfiguraci karty** (autor): režim `mode` má tři stavy,
  LED je zelená `rw`, žlutá `nosave`, červená `readonly` (`Card::panel_led_color`).

## 2026-10-04 – Parametry, které platí za chodu

- **Autor:** zámek zápisu RAM disku má jít přepnout bez vypnutí sestavy.
- Obecně: `Card::live_param` / `set_live_param` určují parametry, které karta přijme za chodu (přepínače, které jde
  přepnout i na skutečné desce, režimy emulátoru). `Machine::set_live_param` je použije na kartě a zapíše do
  popisu sestavy. Konfigurátor je u běžící sestavy posílá hned a označuje je „(platí hned)“. Barva obrazu jde
  stejnou cestou.
- Zatím: barva displejů, `mode` ZRMD-1/ZRD-1V a S1–S4 ZRMD-1. Z `rw` se RAM disk nejdřív uloží, po přepnutí na
  `rw` se aktuální obsah zapíše (soubor pak odpovídá paměti).

## 2026-10-04 – Takt CPU a rychlost emulace odděleně

- **Autor:** TURBO, rychlost v menu a frekvence na displeji a ve stavovém řádku byly zmatené. Mění to rozhodnutí
  z 2026-10-03 (TURBO/F8 přepínalo takt → 4 MHz → max).
- **Takt CPU = karta:** tlačítko TURBO na panelu ZDR je TURBO karty CPU (JPR-1V: CPU 2/4 MHz), tlačítko BIOS
  propojka BIOS JPR-1V (banka EEPROM). Obojí jsou parametry „platí hned“ (`Machine::set_live_param`), menu Stroj
  a MCP `cpu_turbo` jdou stejnou cestou. Karty bez TURBO mají tlačítka šedá.
- **Rychlost emulace:** celý stroj proti reálnému času, 100/200/400 % a maximum (F8 dokola, menu Stroj → Rychlost
  emulace, MCP `resume speed_percent`/`max_speed`; `speed_mhz` zrušeno, `turbo` zůstává jako starší název
  `max_speed`). V nastavení GUI `speed.percent` a `speed.max`.
- **Zobrazení:** stavový řádek „CPU 4.00 MHz  emulace 100 %“ (takt karty a naměřená rychlost), displej FREKVENCE
  CPU efektivní takt CPU = takt karty × rychlost emulace, od 100 MHz bez desetin. Dřív displej počítal z časové
  základny sběrnice, u JPR-1V s CPU na 2 MHz proto ukazoval 4.

## 2026-10-04 – JPR-1V a Consul 262.3 bez úpravy 7474

- **Chyba (autor):** v sestavě V s MikroBIOSem šel napsat jen jeden znak. MikroBIOS nepotvrzuje klávesu ACK a čeká
  na uvolnění STROBE, úprava 7474 (jen na JPR-1Z/ZI) ho ale držela. JPR-1V úpravu nemá, sestava V má
  `latch_mod: false`, tedy původní pulz.
- **Pulzní STROBE a text z PC:** mimo režim `strict` emulace pošle klávesu znovu, když program během pulzu port 01h
  nečetl. Text vkládaný z PC nebo přes MCP se tak neztrácí (na skutečném stroji by se ztratil).
- **Debugger:** čtení paměti na portech klávesnice JPR-1 (2400h/2800h) vrací FFh, aby prohlížení paměti
  neovlivnilo klávesnici.

## 2026-10-04 – Věrný režim klávesnice viditelně v konfigurátoru

- **Autor:** zachovat věrnost hardwaru, pokud není nutné jinak. Pomoc pro text z PC (čekání na ACK=0, opakování
  přehlédnutého pulzu) zůstává výchozí kvůli vkládání textu a MCP, ale je vidět a jde vypnout: u každé karty CPU
  s paralelní klávesnicí přepínač „Klávesnice: věrně jako hardware (klávesy se mohou ztratit)“
  (`keyboard.strict`), platí hned. Výběr jiné klávesnice volbu zachová.

## 2026-10-04 – Mapa paměti s ostatními stavy MAP

- **Autor:** alternativní mapování (CGA-1V při MAP1 = MAP2 = H, díry RAM-1V…) má být vidět na první pohled.
- Pruhy všech čtyř stavů MAP1/MAP2 pod sebou v pevném pořadí (autor: prohazování bylo zmatené). Vybraný stav
  je tlustý, ostatní tenké: ztlumené tam, kde jsou stejné jako vybraný, výrazné tam, kde se liší (barva karty,
  šedá = nikdo). Kliknutím se stav vybere. Rozbalovací výběr stavu zmizel.

## 2026-10-04 – MPH-1V (zvuk a joystick)

- **YM3812 přes Nuked-OPL3** (YMF262 v režimu OPL2, přesný, LGPL-2.1+ kompatibilní s GPLv3). Co OPL3 v režimu
  OPL2 dělá jinak, řeší obal `Ym3812`: bit WSE (bez něj sinus), časovače T1/T2 a stavový registr s dolními bity
  110. Jádro běží na nativních 49716 Hz, převzorkování dělá SDL.
- **Bity registrů nejdřív podle programů:** dostupná PNG a PDF byla rozpracovaná, MPH-test 0.99 a SPLAY, RAT,
  y1stdemo se shodovaly na STATUS `F2 F1 T2 T1 T0 0 0 YM`, IEN `F2 F1 G2 G1 G0 – – YM`, IACK `F2 F1`. Schéma hotové
  desky (`..\SAPI-karty\MPH-1V\Eagle`, 6. 9. 2025) to potvrdilo včetně F1 z OUT1.
- **Časovač 82C54** (kusovník CP82C54-10): třída `I8253` má přepínač `is_8254` pro read-back a stavový bajt.
  8253 na jiných deskách zůstává bez read-back (tam je SC = 11 neplatné).
- **Čas:** 8253 a YM odvozují takty od bázového času sběrnice (14,31818 MHz, zbytek se přenáší). Karta dohání
  stav v `advance()` a před každým přístupem na port.
- **Zvuk na sběrnici:** `Bus::audio` (mono, `audio_rate`, `audio_cap` = 0 znamená, že nikdo neposlouchá, a vzorky
  se zahodí). Nahrávání (`audio_rec`) je na přehrávání nezávislé.
- **GUI hraje jen při 100 % a mimo pauzu.** Při jiné rychlosti by zvuk musel zrychlit nebo se trhat. Latence
  asi 60 ms, při zpoždění se zahodí nejstarší vzorky.
- **Joystick:** stav ovladačů hostitele je na sběrnici (`Bus::joystick`, bity v pořadí portu MPH-1V). Gamepad přes
  SDL3, klávesnice (šipky + mezerník) jen po zapnutí, protože jinak patří klávesnici SAPI. GUI zapisuje jen změny,
  takže stav nastavený přes MCP vydrží.

## 2026-10-04 – Myš a konektory MPH-1V

- **Autor:** myš chytat kliknutím do okna obrazovky, ne jen v hlavním okně (hra může být v CGA-1V nebo DGD-1V).
  Myš Atari ST i Amiga dává jen relativní pohyb, proto relativní režim SDL se skrytým kurzorem, ne pohyb nad
  oknem. Uvolnění prostředním tlačítkem, Ctrl+F10 (jako DOSBox) a ztrátou fokusu. Klávesnice dál píše do SAPI.
- **Co je na K4 a K2, je parametr karty** (nic / joystick / myš Atari ST / myš Amiga), jako na skutečné kartě, kam
  se zapojí, co je po ruce. Platí hned. Výchozí podle štítků: K4 joystick, K2 myš Atari ST (odpovídá popiskům
  MPH-test XA, XB, YA, YB).
- **Rychlost kroků omezená na 1000/s na osu v emulovaném čase:** skutečná myš nedá kroky libovolně rychle a
  program, který porty polluje, by je jinak přeskakoval. Pohyb navíc se drží nejvýš 250 ms.
- Výčet tlačítek na sběrnici je `MouseButton`, stav myši PC ve frontendu `HostMouse`.
- **Autor:** zachycení kliknutím bylo otravné. Myš PC se proto pro SAPI nejdřív zapne v menu (jako joystick
  z klávesnice), výchozí je vypnuto. Při zachycení i uvolnění dostane GUI uvolnění všech tlačítek a polohu
  kurzoru, jinak po uvolnění táhlo okno (stisk, který myš zachytil, GUI vidělo, jeho uvolnění ne).
- **Autor:** palba joysticku z klávesnice zůstává na mezerníku, joystick z klávesnice se ale rychle zapíná a vypíná
  Ctrl+F8 (F8 dál přepíná rychlost) a stavový řádek ukazuje, že je zapnutý.
- **Autor:** joystick z klávesnice i myš PC jsou po každém spuštění vypnuté, do nastavení se neukládají (citlivost
  myši ano).

## 2026-10-04 – ZRD-1V rev 2.1: zámek zápisu

- **Zámek je stav karty, ne konfigurace (autor):** na desce je to RS klopný obvod, který odemkne /RES. Kdyby se
  zápis zámku uložil do sestavy, karta by po restartu emulátoru zůstala zamčená, skutečná ne. Konfigurátor ho
  ukazuje živě (`Card::live_status`), LED ZAM je zámek, MCP `locked`. Parametr „Režim“ zůstává pro obraz
  v emulátoru (rw/nosave/readonly).
- **LED ZRD-1V podle desky rev 2.1:** ZAM = červená WRITE PROTECT, DSK = zelená ACCESS (po přístupu na 60h–63h).
  Dřívější barvy ZAM podle režimu (zelená/žlutá/červená) skončily. Rev 1.1 LED nemá.
- **Revize jako parametr** (`revision`, výchozí 2.1): rev 1.1 zámek, LED ani MAX187 nemá.
- **MAX187 s napětím v parametru** `battery_mv` (platí hned, baterie se vybíjí). Převod podle datasheetu: /CS =
  okno 74LS123, takt čtením 61h, DOUT na D5.

## 2026-10-04 – Ukládání stavu

- **Jedna funkce `state(StateIO&)` na kartu, čip a klávesnici** pro zápis i čtení, ve stejném pořadí. Nic se nepíše
  dvakrát a pořadí se nemůže rozejít. Pojmenované bloky s délkou ohlásí chybu místo posunu dat.
- **Konfigurace se neukládá po kartách:** stav nese popis sestavy, načtení ji postaví znovu (ROM, PROM, propojky)
  a pak nahraje proměnný stav. Při chybě zůstane původní stroj (EmuHost). CLI staví sestavu přímo v sobě
  (`load_state(..., rebuild)`).
- **RAM disky a zálohované SRAM ve stavu** (autor): jsou to paměti a stav je pak konzistentní. V režimu `rw` se
  pak uloží i do pracovní kopie.
- **Velká média jen odkazem** (autor): IDE, SD karta, diskety, kazety. Ukládá se soubor, velikost a čas, při
  rozdílu varování.
- **Ovládání (autor):** menu, rychlý slot Ctrl+F2 / Ctrl+F3 na sestavu ve `work/state/` (F2/F3 bez Ctrl patří
  klávesnici SAPI), MCP s pojmenovanými stavy v paměti pro ladění.
- **YM3812:** jádro Nuked-OPL3 se ukládá celé, jeho vnitřní ukazatele jako posuny od začátku čipu (stav nezávisí
  na adrese v paměti).
- **Test úplnosti:** běh z načteného stavu musí dát bajt po bajtu stejný stav jako původní běh, pro všechny sestavy.
  RTC v testu čtou pevný čas PC (`host_time_fix`).

## 2026-10-04 – Bzučák klávesnice a míchání zvuku

- **Dva druhy podle hardwaru:**
  - aktivní bzučák EKL-1 (ECMON jen drží P0-OUT1 v nule 160 ms, takže bzučák má vlastní oscilátor);
  - piezo ANK-1, které sleduje bit (MIKROBASIC ho přepíná).
- U Consulů je bzučák předpoklad (autor neví), vypnutelný parametrem.
- **Pípnutí po resetu věrně** (autor): latch P0 (3212) nuluje /RES, P0-OUT1 = 0, dokud ho monitor nenastaví.
- **Míchání na sběrnici:** zdroje přičítají vzorky podle indexu odvozeného z času sběrnice (14,31818 MHz / 288 =
  49716 Hz, kmitočet YM3812 na MPH-1V). Machine::run_cycles na konci smíchá všechno až k aktuálnímu času do
  `Bus::audio`. Zvuk je tak pro přehrávání, WAV i MCP stejný, ať zní MPH-1V, bzučák, nebo obojí.
- Hrany bzučáku jsou na přesném čase sběrnice. Vzorek je průměr úrovně za svou periodu, ustálená úroveň je díky
  potlačení stejnosměrné složky ticho.

## 2026-10-05 – Zdrojáky systému ZRMD-1 a nástroj cpmrun

- **`tools/cpmrun.cpp`** (cíl `cpmrun` v CMake): spouští programy pro CP/M 2.2 na PC nad Z80 jádrem emulátoru
  s jednoduchým BDOS (soubory v jedné složce). Vlastní nástroj místo cizího emulátoru CP/M nebo křížového
  assembleru: bez závislostí, přeloží zdrojáky M80 tak, jak je překládal autor na SAPI, a hodí se i pro další
  software (doporučení, autor schválil).
- **Zdrojáky systému JPR-1Z / ZRMD-1 jsou mimo repo** v `..\SAPI-karty\ZRMD-1\SRC\` (rozhodnutí autora: patří
  k ZRMD-1, vedle stávající `SW`). Jde o zdrojáky J. Bíby a EC1045. Originály jsou beze změny v `orig\`, pracovní
  verze mají jména 8.3 kvůli CP/M. `build.py` přeloží EPROM a systémovou oblast RAM disku a ověří je proti
  originálům.
- **Oprava ECBIM8 (MFC-1A) jen ve zdrojáku a kopiích obrazu:** `RET` + `NOP` místo `LD C,0` (stejná délka).
  Původní obraz v `media/` a pracovní kopie autora se nemění. Opravený systém funguje s resetem 8272 přesně podle
  schématu.

## 2026-10-05 – Řadič MFC-1A

- **Společná základní třída `Fdc8272Card`** (`src/cards/fdc8272.*`) pro desky s 8272 + 8257: mechaniky, DMA, stav,
  okno Média. FDC-1 a MFC-1A se liší jen dekodérem, registry a tím, kdo vybírá mechaniku. Formát stavu FDC-1 se
  tím změnil (karta je z téhož dne, starší stavy s ní nejsou).
- **MFC-1A:** porty podle dumpu PROM DD9, mechaniku vybírá registr 0FH (DS0/DS1 z 8272 nejsou zapojené).
- **Registr 0FH podle originálu Tesla, překreslení EC1045 jako volba** (parametr `wiring`, výchozí `tesla`).
  Původní verze (commit 48910b8) se řídila překreslením EC1045 (`MFC-1A_MP.pdf`). Sken originálního schématu
  Tesla a celý návod (`..\SAPI_DOKU\MFC-1A\`) ale ukázaly, že překreslení se liší ve dvou místech:
  - originál: D4 = reset 8272, D5 = povolení přerušení, DD23A Q do vstupu C dekodérů DD18 a DD19 (D6 = 1
    mechaniky vypnuté); souhlasí s návodem (str. 14) i s `MFC1.ASM` (`DRVOF = 40H` „konec práce s řadičem“);
  - překreslení: D4 a D5 prohozené, do dekodérů /Q (D6 = 1 mechaniky zapnuté); s tím počítá BIOS ECBIM8.
  Originál má přednost (pravidlo: původní skeny před překreslením), `ec1045` zůstává kvůli replice a ECBIM8.
- **Bit resetu 8272** (parametr `reset_mode`, výchozí `level` = schéma; nahradil `reset_d5`): BIOS ECBIM8 (ECMON
  varianta 2) kvůli chybějícímu `RET` v `INIT_MFC` zapíše FFh, v obou zapojeních tím drží 8272 v resetu a zasekne
  se. Autor: emulovat tak, aby varianta 2 bootovala, a **důrazně upozornit na rozdíl proti schématu** (důležité
  pro repliku). Sestava `cpm_zrmd_mfc1a.sapi` proto má `wiring = ec1045` a `reset_mode = pulse` (odchylka:
  náběžná hrana jen resetuje 8272 a D6 zůstane, jinak by BIOS čekal v RECALIBRATE). Výchozí hodnoty parametrů
  jsou podle originálu, odchylku zapíná jen sestava.
  **Doplněno večer:** po opravě BIOSu ve zdrojácích (`..\SAPI-karty\ZRMD-1\SRC`) má sestava RAM disk
  `media/zrmd-1/SAPI_CPM_Martin_2019-10-07_fixMFC.bin` a `reset_mode = level`, takže bez odchylky (autor
  schválil). `pulse` zůstává jako volba pro původní obraz.
- **READY u MFC-1A z mechaniky** (výchozí `rdy_ground = false`): ECBIM8 při inicializaci čte z jednotky 3 a
  s uzemněným RDY by bez diskety čekal věčně.
- Sestavy `cpm_zrmd_fdc1.sapi` a `cpm_zrmd_mfc1a.sapi` (B: = disketa CP/M 2.2/P jako data). Test `test_fdc_ecmon`
  používá původní obraz RAM disku jen pro čtení (pracovní kopie autora je jiná) a variantu píše předem.

## 2026-10-05 – Řadič FDC-1 a sestavy ZPS-3 CP/M 48K/62K

Zadání autora: otestovat ROM a diskety z `..\SAPI-sestavy\ZPS-3\` (MOR*.hex, CPM*.img; 48/62 = paměť, F = FDC-1),
udělat předkonfigurované sestavy a naimplementovat FDC-1 podle `..\SAPI-karty\FDC-1\`.

- **FDC-1 je jedna karta přes dva sloty** jako RPD-1 (FDC-1A v prvním, FDC-1B ve druhém).
- **Porty podle dumpu PROM D5B** (soubor `roms/fdc-1/FDC-1A_D5B_74S287.bin`, parametr `decoder`), vstupy A0–A7 přímo.
- **8272 a 8257 jako samostatné obvody** v `src/chips` (hostitelské rozhraní `I8272Host` pro mechaniky a DMA).
  Mechanika a obraz 8" diskety jsou nově společné s RPD-1 (`FloppyDrive`). Media okno používá virtuální
  `Card::floppy_drive`, `floppy_insert`, `floppy_eject`, takže další řadiče nic dalšího nepotřebují.
- **Přerušení 8272: jeden stav na jednotku** (jako MAME), po resetu „ready changed“ pro připravené jednotky, nový
  SEEK/RECALIBRATE stav své jednotky smaže. Fronta stavů dávala MOR po RESETu s hlavou na stopě 0 „BOOT ERROR“.
- **DMA bere čas CPU:** `Bus::hold` (cykly, kdy DMA drží sběrnici) přičte CPU karta před další instrukcí, 4 takty na
  bajt. Zbytek patří do stavu karty FDC-1, aby se nezměnil formát stavu sběrnice (starší uložené stavy platí).
- **Čtení 0EH vrací přepínače SA2 vybrané mechaniky** (D1 = 8", D0 = DD): odvozeno z FORM6, které před
  formátováním 8" disket čeká D1 = 1. Parametry `sa2_a`–`sa2_d`. Emulované mechaniky jsou vždy 8" SS SD.
- **/RDY uzemněný** (parametr `rdy_ground`, výchozí ano) podle návodu a FDC.DOC. Bez diskety pak 8272 čeká na
  index věčně, jako skutečný řadič.
- **62K:** druhá RAM-1A na C000–F7FF s upravenou PROM D1A (předpoklad, jiný soubor podle pravidla „jiná adresace =
  jiný soubor“) a AND-1A s domácí PROM D3A pro F800 z archivu SAPIMAN.
- **ROM a diskety:** MOR62A, MOR48AF, MOR62AF jako `.bin` vedle MOR48A, diskety v `media/rpd-1` a `media/fdc-1`.
  CPM48A.img od autora je shodný s dřívějším `CPM22P_48A_Perina.img`.

## 2026-10-05 – Podklady desek ve složce SAPI-karty

- **Autor přesunul složky desek, které nejsou na GitHubu,** z `E:\SAPI_GIT\<deska>` do
  `E:\SAPI_GIT\SAPI-karty\<deska>`: EKL-1, ZRMD-1, AND-1Z-89, IDE-1, DMR-1, JPR-1ZI, PMD32-SD, RAM-1Z, DSM-1,
  ARB-1C_8slot, JPR-1V, DSM-1V, AND-1V, RAM-1V, CGA-1V, DGD-1V, ZRD-1V, MPH-1V. Nové karty přibývají tamtéž.
- Repa z GitHubu (MikroMon, ZPS-3, SAPI_DPI-1, SAPI_DPI-1L, SAPI_DTR-1…) zůstávají přímo v `E:\SAPI_GIT`.
- Odkazy v `docs/`, `media/`, `roms/`, `CLAUDE.md` a v komentářích kódu jsou teď `..\SAPI-karty\<deska>`.
- `test_mph1v` hledá hudbu (SPLAY, RAT PLAYER) nejdřív v `..\SAPI-karty\MPH-1V`, pak ve starém `..\MPH-1V`, aby
  fungoval i na PC, kde ještě je staré rozložení.
