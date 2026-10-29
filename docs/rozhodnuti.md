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
  Je to kompromis mezi bezpečností dat a zápisem 4 MB. (Od 2026-10-29 průběžně 3 s po posledním zápisu, při pauze
  a ztrátě fokusu, viz etapa 4 plánu vzhledu.)
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
- Od 2026-10-29 je F9 breakpoint jako ve Visual Studiu a reset jen F12 (rozhodl vedoucí workflow za autora,
  oddíl „2026-10-29 – etapa 6 plánu vzhledu: před betou“).

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
  (jen v AppData) se při prvním spuštění zkopírovalo k exe; od 2026-10-29 už ne (viz tam). Každé sestavení (debug,
  release) má tedy své nastavení.
- **Co se pamatuje (rozhodnutí autora, vše):** poslední sestava, velikost, poloha a maximalizace okna, otevřená
  okna, luminofor, škálování, poměr TV, vyhlazování, typy karet, volby terminálu (emulace, CR, echo, TCP port
  a protokol, COM) a rychlost.
- **Poslední sestava** je jen taková, která se úspěšně spustila (ne neuložená sestava z konfigurátoru). Když
  soubor zmizí, naběhne ZPS-1 s MIKROBASICem (od 2026-10-28, dřív MikroMon), když sestava nejde spustit, MikroMon.
  `--machine` má přednost.
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
  a DGD-1 mají jeden FRB (na deskách dole, K2). VGA D-sub mají AND-1V, AND-1B, CGA-1V a DGD-1V (BGP-1S má FRB K2 pro
  TV, opraveno 2026-10-06). FRB se kreslí jako
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

## 2026-10-05 – Karta DPP-1 s PMD32-SD

- **DPP-1** (`src/cards/dpp1.*`): dva 8255, adresa přepínači S1 přesně podle tabulek 2 a 3 návodu (00h–38h po 8).
  PMD32-SD na portu A zvoleného kanálu v módu 2, stejný kód jako u IDE-1 (`cards/pmd32link.*`, společná konfigurace
  a popis; `Card::pmd32sd()` pro okno Média a MCP).
- **Přepínače S2–S4 (směr 3216, polarita PC) se neemulují**: předpokládá se nastavení, které PMD32-SD potřebuje.
  Bez připojeného zařízení čte kanál jedničky. Tiskárna na kanálu 1 zatím není.
- Výchozí základ 28h a kanál 2, jak je používají ECMON a CP/M J. Bíby. Autorova sestava `cpm_zrmd` se nemění,
  DPP-1 má nová `cpm_zrmd_dpp1` (autor DPP-1 ve svém stroji asi nemá, má IDE-1).
- 5,25" diskety J. Bíby z `PETR\2MB` jsou na SD kartě `media/pmd32sd` (sdílí ji všechny sestavy s PMD32-SD).
- **Pracovní kopie složky** (SD karta PMD32-SD ve `work/pmd32sd`) dostane soubory, které do `media/` přibyly
  později; soubory, které v kopii už jsou, se nepřepisují (jsou to pracovní kopie autora). Dřív se kopie
  vytvořila jen jednou a nové obrazy v ní chyběly.

## 2026-10-05 – Obrazy ImageDisk (.imd) v mechanikách

- **Mechanika drží disketu jako stopy** (`FloppyTrack`): záznam FM/MFM, rychlost, délka sektoru a sektory v pořadí
  od indexu s ID (C H R N), příznaky (deleted, chyba CRC dat, bez datové značky) a daty. Obyčejné obrazy se do
  stop převedou při načtení, 8272 hledá sektory podle ID na stopě (libovolné číslování, prokládání, smíšené
  FM/MFM, 8" DD, 9 × 512 FM…).
- **ImageDisk:** módy 0–5 = rychlost řadiče PC; disketa dostane svou skutečnou rychlost (FM 500 → 250 kbit/s 8",
  MFM 500 → 500, „300“ a „250“ → disk 300 ot/min s 250 MFM / 125 FM). Otáčky: 360 pro 500 kbit/s, 300 když se
  stopa nevejde (3,5" HD) nebo pro DD. Zápisy a FORMAT se ukládají zpět jako ImageDisk (stejnorodé sektory
  komprimované, hlavička zachovaná). FORMAT stopu IMD přestaví podle zadaných ID; obyčejný obraz přijme jen svůj
  vlastní formát.
- **8" SS SD v IMD se standardními ID** drží data v logickém pořadí jako obyčejný obraz, takže ho bere i RPD-1
  (ověřeno: MIKOS/D z Jílkovy diskety `JILEK\01\01.IMD` nabootuje na ZPS-2).
- MAX hlavy zůstává 79, RECALIBRATE dál 77 kroků (jako čip). (Od 2026-10-05 je doraz 82, viz „Formátovací
  programy SAPIMANa“.)

## 2026-10-05 – Diskety PC (MFM, HD/DD) pro FDC-1 a MFC-1A

- **Formát diskety podle velikosti obrazu** (`FloppyGeom`): 8" SS SD a formáty PC (3,5" HD, 5,25" HD, DD 720/360 KB).
  Obrazy PC jsou prosté sektory jako na PC, takže se dají připravit i tam. RPD-1 dál bere jen 8" SS SD.
- **Disketa nese svou rychlost a otáčky, řadič dává hodiny** (`I8272Host::fdc_kbps`): 8272 vidí ID jen při shodě
  záznamu (FM/MFM), strany a rychlosti. FDC-1: SA2 „8"“ = 8 MHz (MFM 500 kbit/s = PC HD), „5,25"“ = 4 MHz
  (odhad, v Nejasnostech). MFC-1A: 8 MHz FM, 4 MHz MFM podle návodu, tedy jen DD.
- **Rozložení stopy MFM podle IBM System 34** (mezery 4E, sync, A1 A1 A1); časování a hledání sektorů jako u FM.
- Buffer sektoru 8272 má 1024 B, **formát stavu FDC-1 a MFC-1A se změnil** (starší uložené stavy s nimi nejdou).
- Motory (FDC-1 0FH, MFC-1A D3 D2) se dál neemulují.

## 2026-10-05 – Sestavy s bootem CP/M z 8" diskety (J. Bíba 2011)

- **EPROM z `.hex` J. Bíby, ne z našeho překladu:** `mon281211`, `mon130711`, `MON271111` jsou přesně tak, jak je
  autor systému vypálil. Překlad zdrojáků přes `cpmrun` je ověřený (liší se jen proměnné DS), slouží jako kontrola.
- **Vlastní prázdný RAM disk `ZRMD-1_prazdny_OFF0.bin`:** systémy z roku 2011 mají CP/M přes celé 4 MB (OFF 0),
  ECMON od roku 2015 OFF 128. Sdílet obraz nejde, adresáře by se přepisovaly.
- **Diskety `.8sd` ze `SAPI_DOKU\SAPIMAN\PMD32-SD_IMAGE8` přejmenované na `.img`** do `media/sapiman/`, obsah
  beze změny.
- **MFC-1A v sestavě `wiring = ec1045`:** systém zapíná mechaniky D6 = 1, jinak nenabootuje.

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
  **Změněno 2026-10-07:** originální deska odpovídá překreslení, výchozí je `ec1045` (níže).
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

## 2026-10-05 – Formátovací programy SAPIMANa, doraz hlavy, nová disketa jako ImageDisk

- **Ověřeno originálními programy** (`SAPI_DOKU\SAPIMAN\CPM_PROGRAMY\FORMATOVANI`, přeložené M80/L80 pod `cpmrun`):
  FDC-1 5,25" HD PC (`fd5p_512_30`, MFM 2 × 15 × 512) v dlouhém CP/M a MFC-1A 3,5" DD (`fd3m_512_18`, MFM
  2 × 9 × 512) i 5,25" FM (`fd5_256_16`, 2 × 8 × 256). Všechny naformátují a kontrolním READ TRACK hlásí
  „Disketa je OK“. Na disketu z FDC-1 pak PIPU zkopíroval celý disk B: s ověřením.
- **Doraz hlavy je stopa 82** (`FloppyDrive::LAST_TRACK`, dřív 79, pro 8272 i RPD-1). Programy SAPIMANa formátují
  stopy 0–80. S dorazem 79 se 81. stopa zapsala přes stopu 79 s C = 80 a CP/M by o ni přišlo. Mechaniky na
  80 stop dojedou o pár stop dál. **Hodnota 82 je předpoklad** (Nejasnost u FDC-1), stejná pro všechny mechaniky.
- **Stopa za koncem obyčejného obrazu** (např. 80 u 720K nebo 1,2M) zůstane v paměti, dokud je disketa v mechanice.
  Do souboru se neuloží, protože obraz `.img` má pevnou velikost. ImageDisk ji uloží.
- **Nová disketa jako ImageDisk:** okno Média s názvem `.imd` založí ImageDisk naformátovaný zvoleným formátem
  (`FloppyDrive::create`). Ten pak přijme libovolný formát FORMATu, třeba číslování sektorů 10–18 na straně 1,
  které používají programy SAPIMANa pro MFC-1A a které obyčejný obraz PC neumí. Prázdné (nenaformátované) IMD se
  nedělá: ImageDisk nemá údaj o otáčkách, ty se odvozují z obsahu.

## 2026-10-05 – 8272: změna READY nepřepíše čekající stav, MFC-1A `rdy_ground` nezávisí na D6

- **Polling READY 8272** přeskočí jednotku, která má nevyzvednutý stav (jako MAME: změna se ohlásí, až se stav
  přečte). Dřív „ready changed“ přepsal třeba dokončený SEEK.
- **MFC-1A `rdy_ground = true`** dává RDY trvale, i při vypnutých mechanikách (D6). Dřív READY sledovalo D6 vždy.
- Podklad: výpisy z reálných desek v `SAPI15\MFC_TEST_CPM` (2010, `docs/desky/MFC-1A.md`). S `rdy_ground = true`
  dává emulace stejné bajty jako originál. Výchozí `rdy_ground = false` zatím zůstává (ECBIM8 by s trvalým RDY bez
  diskety v jednotce 3 čekal věčně).
- **2026-10-06, autor:** sestavy SAPIMANa `cpm_boot_mfc1a` a `cpm_boot_mfc1a_rmd` mají `rdy_ground = true`
  (podle výpisů z reálných desek). `cpm_zrmd_mfc1a` zůstává `false` kvůli ECBIM8.

## 2026-10-06 – Motory mechanik FDC-1 a MFC-1A

- **Parametr `motor_line`** (výchozí ne) a `spinup_ms` (výchozí 500) u obou řadičů. S ním se disketa točí jen se
  zapnutým motorem: FDC-1 bity M3–M0 registru 0FH, MFC-1A mechanika s číslem v D3 D2 (povolená jako výběr, D6).
  Disk je na plných otáčkách `spinup_ms` po zapnutí, vypnutím se zastaví hned (doběh se nemodeluje).
- **8272** hledá index a značky až od chvíle, kdy se disk točí (`I8272Host::fdc_turning_from`). Když čeká na disk,
  který se netočí, a motor se zapne, hledání pokračuje (`I8272::motor_changed`). Operace, která už běží, se po
  vypnutí motoru dokončí.
- **Výchozí ne:** 8" Consul mají střídavý motor a točí se stále. Monitory a BIOSy pro 8" motory nezapínají
  (MON130711 na FDC-1, BIOS 281211 na MFC-1A nechává D3 D2 = 0), s `motor_line` by nefungovaly.
- **Zapnuté** v `cpm_ec1045` a `cpm_boot_dlouhe_rpd1_fdc1*` (mechaniky PC): BIOS zapne motor při SELECT a
  nečeká, rozběh odčeká 8272. Monitor motory po nečinnosti vypíná (časovače `TIM_Mx` v CONST, vstup BIOSu `MOTOR`).
- `/RES` nuluje registr 0FH FDC-1 (předpoklad, Nejasnost). Stav karet má nově masku motorů a časy rozběhu.

## 2026-10-06 – Tiskárna Centronics (X3 karet JPR, DPP-1)

- **Kde:** ECMON i monitory J. Bíby tisknou `LST:` přes X3 procesorové karty (P2 data, P1-OUT1 /STROBE, P2-IN6
  BUSY, P2-IN7 = kolík 16). DPP-1 kanál 1 je u nich `PUN:` (mód 1). Dřívější zápis „kanál 1 = tiskárna“ byl omyl.
  Autor chtěl tiskárnu na obou místech.
- **Jedno zařízení** `Printer` (`src/chips/printer.*`) pro obě připojení. Bajty jdou beze změny do souboru,
  textové zobrazení vynechá ESC/P a mapuje 80h–FFh podle znakové sady (výchozí KOI-8 čs2).
- **Časování:** BUSY drží `busy_us` po bajtu (výchozí 100 µs jako tiskárna s vyrovnávací pamětí), pak přijde
  /ACK. Hodnota 0 je odchylka (okamžitě).
- **X3:** bajt se bere při náběžné hraně /STROBE (předpoklad). Kolík 16 je podle autora 0. Úvodní bajt 00h po
  `CINIT` ECMONu se zapíše, protože ho dostane i skutečná tiskárna. JPR-1/1A (8080) tiskárnu nemají
  (P2-IN = přerušení).
- **DPP-1:** 8255 dostal mód 1 na portu A (vstup i výstup), /OBF = /STROBE, /ACK tiskárny na PC6.
- **Testy** do souborů tiskárny nepíšou (stejný přepínač jako soubory baterií), `test_printer` si ho zapne pro
  soubory v `work/test/`.
- **Sestavy:** `cpm_zrmd` (X3), `cpm_zrmd_dpp1` (X3 a DPP-1 kanál 1).

## 2026-10-06 – Mechaniky v okně Média: číslo jednotky a písmeno v CP/M

- **Proč:** písmena A:–D: v okně Média neodpovídala CP/M (ECMON má B: a C: na jednotkách 0 a 1, dlouhé CP/M D: na
  FDC-1 jednotce 0…). Mechaniky jsou teď označené číslem jednotky řadiče (0–3), PMD32-SD si nechává svá A–D.
- **Písmeno ze sestavy:** `"cpm": "B"` v `drive_a`–`drive_d` (RPD-1, FDC-1, MFC-1A), u PMD32-SD `"cpm": ["D", "E"]`.
  Vyplněné ve všech sestavách CP/M, kde je známé z hlášek BIOSů. Klíče `drive_a`… zůstávají (kompatibilita),
  v konfigurátoru jsou to Mechanika 0–3.
- **Odhad za chodu** (když písmeno v sestavě chybí, ukáže se „asi B:“): karta CPU zachytí volání BIOS SELDSK
  (adresa z `JP WBOOT` na 0000h, přepočítá se při zápisu do 0000–0002h a jednou za asi 30 ms) a zapamatuje si
  registr C. Řadič přiřadí toto písmeno jednotce při nejbližším **čtení** sektoru (8272 READ DATA, RPD-1 čtení
  dat, PMD32-SD příkazy Q a R). Zápisy se nepočítají, protože BIOS s odloženým zápisem (deblocking) zapisuje
  předchozí disk až po výběru dalšího. Reset odhady smaže (může naběhnout jiný systém). Do stavu se neukládá.
- **Test** `test_drive_letters`: ECMON (FDC-1, MFC-1A), monitory J. Bíby (RPD-1, FDC-1, MFC-1A), dlouhé CP/M,
  EC1045, PMD32-SD na DPP-1 a CP/M 2.2/P na ZPS-3 (8080).
- **MCP:** popis karty má u mechanik `unit` (dřív `drive` s písmenem), `cpm` a `cpm_seen`.

## 2026-10-06 – Sestavy a knihovna disket J. Bíby, DUP, tiskárna v sestavách

- **Sestavy s RMD-1** (`cpm_boot_fdc1_rmd`, `cpm_boot_rpd1_rmd`): bootovací diskety nemají inicializační program,
  RMD-1 je po zapnutí náhodná jako DRAM. Do B: proto disketa Z6 s `IRMD40`, nic se neobchází.
- **`cpm_pmd_fdc1`** bez RAM disku, jak ho systém 140212 hlásí (A, B = FDC-1, C, D = PMD32-SD).
- **Knihovna disket** (rozhodnutí autora): soupis všech asi 180 disket v `docs/software/SAPIMAN_diskety.md`
  a jen výběr 16 v repu (`media/sapiman/knihovna`, 4 MB). Komerční software je otázka pro veřejné vydání.
  Diskety mimo `media/` emulátor používá přímo (bez pracovní kopie), proto doporučení otevírat je s ochranou.
- **Tiskárna na X3** ve všech sestavách s ECMONem a monitory J. Bíby (kromě `cpm_term64k_fdc1` bez klávesnice),
  protože jejich `LST:` na ni tiskne (ověřeno).
- **DUP-80** ověřený a test `dup8` s DUP8.COM z RAM disku `cpm_zrmd` (bez M80 v repu).

## 2026-10-06 – ZPS-3 s originálním CP/M 64K od Tesly IMA

- **ROM a diskety:** EPROM `JPR1A_BOOT_64k.BIN` (SAPI_DOKU) jako `roms/jpr-1a/MON64C3.bin`, diskety 01–03 z archivu
  EC1045 (`DEX\SAPI_ZPS3`) jako `media/rpd-1/CPM64K_IMA_01–03.img`. Kopie z `!!!!_TRIDENO_!!!!\CPM64K` mají
  stejné systémové stopy, liší se jen soubory, do repa nejdou.
- **Paměť:** monitor se zkopíruje na F800 a video je na F000, RAM tedy musí být 0000–EFFF a F800–FFFF. Jako u 62K
  druhá RAM-1A (16K, S1 S2 = 10) s upravenou PROM D1A (`RAM-1A_D1A_64K_74S287.bin`) a AND-1A s upravenou D3A
  (`AND-1A_D3A_CPM64K_74S287.bin`, F000). **Obě PROM jsou předpoklad**, každá jako samostatný soubor (jiná
  adresace = jiný soubor), originální sestavu nemáme. Otázka zůstává v „Nejasnostech“ (bod 19).
- **Sestava** `machines/zps/ZPS-3_CPM64K_IMA.sapi`: A: disketa 1 (systém), B: disketa 2 (MBASIC, COBOL).
  Test `ima64k` v `test_zps3`.
- **Diskety ze Slušovic** (`04A`, `04B`, `05` ve stejné složce) jsou zapsané jako plán v `docs/software/CPM_ZPS-3.md`.

## 2026-10-06 – ZPS-3: rekonstrukce diskety IMA CP/M 48K originálním MOVCPM48

- **Proč takhle:** bootovatelná disketa IMA 48K se nedochovala, ale máme originální `MOVCPM48.COM` a `SYSGEN.COM`
  od IMA a disketu „!CPMZPS3.004“ se všemi soubory. Místo překladu a ručního skládání stop je spustíme v emulátoru
  (pod Peřinovým CP/M 48K) stejně, jako by to udělal uživatel na skutečném stroji: `MOVCPM48 46 *`, `SYSGEN` na B:.
  Výsledek je ověřený proti zdrojům (BOOT0C1, BIOS48C1 přes M80/L80 v `cpmrun`) a proti nepřemístěnému `SYS48.BIN`.
- **46K:** MOVCPM48 nepřijme víc než 46 (monitor MON V1.0-C1 je na B800), proto „JPR-1A 46K CPM V2.2“.
- **Disketa v repu** `media/rpd-1/CPM48K_IMA_004.img`: soubory originální, systémové stopy vyrobené (v popisu
  sestavy i v `media/README.md` je to uvedené). B: = disketa 003 (Pascal MT+). Sestava
  `machines/zps/ZPS-3_CPM48K_IMA.sapi`, `ZPS-3.sapi` (podle manuálu) zůstává s Peřinovou disketou.
- **Test** `ima48k` v `test_zps3` celý postup zopakuje a porovná systémové stopy.

## 2026-10-06 – Přepínání jazyka GUI

- **Menu:** Zobrazení → „Jazyk / Language“ (název v obou jazycích, aby se našel z kteréhokoli), položky „Čeština“
  a „English“. Přepne se hned, texty se překládají každý snímek. Volba se ukládá do `sapiemu-settings.json`
  (`"language"`), jen když ji uživatel změnil.
- **Výchozí jazyk podle jazyka Windows** (rozhodnutí autora): čeština pro české nebo slovenské Windows, jinak
  angličtina. Jen zobrazovací jazyk, **ne místní formáty**: autor má anglické Windows s českými formáty a GUI chce
  anglicky (krátce zkoušené „i podle formátů“ zamítl).
- **`--lang cs|en`** jen pro jedno spuštění (snímky `--ui-snapshot`), neukládá se.
- **Pevná ID oken** (`tr_id`: „Média###Media“): ImGui ukládá polohu okna podle jména, bez toho by po přepnutí okna
  skočila na výchozí místo. Stávající `imgui.ini` s českými jmény (`[Window][Média]`) se při startu převede na
  `[Window][Media]` (to ImGui ukládá u jmen s `###`), rozložení zůstane.
- **Nepřekládá se:** sestavy (`.sapi`, data autora), výstup emulovaných programů, potisk čelního panelu (NAPÁJENÍ,
  FREKVENCE CPU, ZDROJ jsou česky na štítcích Liborovy modernizované skříně, podle kterých se panely kreslí), MCP
  a `sapiemu-cli`.
- **Opravené překlady:** test `test_i18n` našel dvojí klíče. „(empty)“ měl „(prázdný)“ i „(prázdná)“ a platil jen
  druhý, takže prázdný slot v konfigurátoru byl „(prázdná)“. Slot má teď vlastní text v `rack.cpp`.

## 2026-10-06 – Linux build ve WSL

- **Proč WSL:** autor má jen WSL 2 (Ubuntu 24.04, WSLg), Linux ve VMware by šel jen pracně. WSL stačí na překlad,
  testy, `sapiemu-cli` i GUI (WSLg). Nevyzkouší se fyzické COM porty a gamepad.
- **Windows má přednost** (autor): opravy pro Linux se dělají ve Windows repu a commitují až po `build.cmd debug test`
  i `release test`. Klon ve WSL (`~/SAPIemu`, lokální klon z `E:`) je jen na zkoušku, změny se do něj kopírují.
- **Opravy:** `static constexpr` místo `static const` u konstant AND-1 a AND-1Z (GCC chtěl definici), `\r` místo
  doslovného CR v `test_mph1v.cpp` (soubor se tím v Gitu normalizuje na LF). Jinak se kód přeložil beze změn.
- **CI** zatím ne (soukromé repo, minuty), viz SOUHRN.

## 2026-10-06 – Úrovně testů a paralelní běh

- **Proč:** celá sada trvala 100 s (debug) a release s testy procesoru 264 s, autor nechce pouštět všechno vždy.
- **Paralelně** (`execution.jobs: 8` v test presetech): debug 25 s, release bez pomalých 9 s, `release full` 161 s.
  Překážky byly dvě a obě jsou opravené: testy spoléhaly, že složku `work/test` vytvořil dřívější test
  (`test_init()` ji teď vytvoří), a dva procesy mohly současně vytvářet stejnou pracovní kopii média (teď
  dočasné jméno a přejmenování v `media_path()`, prospěje i dvěma spuštěným emulátorům).
- **Release ve výchozím stavu bez pomalých testů** (label `slow`: ZEXDOC, ZEXALL, 8080EXM), všechny jen
  `tools\build.cmd release full` (preset `windows-release-full`). `build.cmd debug test REGEX` spustí jen vybrané.
- **Úrovně** podle změny (tabulka v `docs/build.md`): dotčené testy během práce, celá debug sada před commitem,
  release před pushem, `full` po změně jader CPU a před vydáním, Linux ve WSL po změně kódu závislého na platformě.

## 2026-10-06 – Klávesnice ANK-1M2 a společný joyport

- **ANK-1M2 = ANK-1 + konektor joysticku** podle schématu rev. 1.2: matice, LED, bzučák a INT/„T“ jsou zapojené
  stejně, proto stejný kód (`Ank1` s příznakem `m2`) a programy pro ANK-1 běží beze změny.
- **Jedno jméno** „ANK-1M2“ v menu, konfigurátoru a MCP (autor), mapování pro MIKOS jen parametrem `software`.
- **ANK-1M5 = ANK-1M2** (autor): ANK-1M5 je starší předchůdce, kde se v podkladech objeví, bere se jako ANK-1M2.
  Typ `ANK-1M5` v sestavě je druhé jméno ANK-1M2 (bez vlastní položky v menu).
  Stávající sestavy se nemění (autor), ANK-1M2 se vybírá v Stroj → Klávesnice nebo v konfigurátoru.
- **Joystick i myš** (autor): joyport má stejné pořadí bitů jako konektory MPH-1V, proto společný kód
  `input/joyport.*` (zařízení, kvadraturní myš). MPH-1V ho používá také, formát uloženého stavu se nezměnil.
- **`Bus::mouse_connected`** je maska (MPH-1V, klávesnice), aby jedno zařízení nevypnulo zachytávání myši
  druhému. Klávesnice svůj bit smaže, když se vymění.
- **Konfigurátor:** volby s vlastním ovládáním (`strict`, `beeper`, `joyport`) nejsou součástí předvolby klávesnice,
  proto se ANK-1M2 s myší dál ukazuje jako „ANK-1M2“ a volby zůstanou při výměně klávesnice (`joyport` jen pro
  ANK-1M2). Dřív se vypnutý bzučák ukazoval jako „vlastní: …“.

## 2026-10-06 – AND-1B, BGP-1S a JPR-1S (desky Libora Lasoty)

- **Podklady z poslední revize** (autor: Libor dělá víc iterací, obrázky, PDF a EAGLE se mohou lišit): AND-1B rev 1.4,
  BGP-1S rev 4.2, JPR-1S rev 2.2. Binární schémata EAGLE se dají offline převést na netlist přes `eaglecon.exe`
  (na kopii), fakta jsou ověřená z netlistů.
- **AND-1B** je samostatný typ karty (ne varianta AND-1): jiné časování (VGA), font z EPROM, atributy řízené obsahem
  fontu (banky, bit 7 = dvojitá šířka), jako v hardwaru. Výchozí propojky podle autorovy desky (fotky rev 1.4):
  3800h, INT1 vypnuto, zelená.
- **BGP-1S** (název podle autora; schéma a BIOS říkají BGP-1, přijímá se jako alias): samostatný typ. Videoram jen pro
  zápis, karta při čtení nedrží sběrnici a v mapě se hlásí jen pro zápis (čte se RAM pod ní). Přerušení ruší
  čtení operačního kódu z okna (adresa = PC instrukce). Výstup TV (FRB K2), ne VGA. Výchozí podle fotky: C000h,
  MAP1 ignorován, Colorace, /INT0.
- **JPR-1S** je varianta třídy JPR-1 (stejné porty v paměti), zápisy na desce jdou i na sběrnici (8228 /BUSEN
  na GND). SRAM má po zapnutí pseudonáhodný obsah jako RMD-1: MIKROBIOS by z nulové RAM vzal VideoBase 0000h.
  Výchozí propojky podle fotky rev 2.2: všechny banky RAM, INT1, RST7. Výchozí BIOS 1.02 (pro BGP-1S a hry).
- **Redukce34KB01** (sériová linka na P1-IN7, `k2_adapter`) patří k ANK-1M5 (= ANK-1M2) a karta ji umí i na JPR-1.
  S ní nejdou klávesy INT a T (kolíky 7 a 9 nejsou zapojené), jako na skutečné redukci. Úroveň linky se počítá
  z času sběrnice při čtení P1, takže BOOT v BIOSu čte bit po bitu jako na stroji.
- **`CpuCard::poke_local()`:** nahrání programu (Soubor → Nahrát, `--load-hex`, MCP) dosud šlo jen přes sběrnici
  a RAM na procesorové kartě (JPR-1 2000h, banky JPR-1S) nedostalo. Teď ji zapíše i procesorová karta.
- **Sestavy** (autor: dvě, stávající beze změny): `machines/jpr1s_bgp1s.sapi` (MIKROBIOS 1.02, BGP-1S, ANK-1M2,
  redukce na TCP 2325) a `machines/zps/ZPS-1_JPR-1S_AND-1B.sapi` (ekvivalent ZPS-1: MIKROBASIC 2.4 v EEPROM JPR-1S,
  AND-1B, DSM-1). Binárky (BIOSy, font, hry) jsou v repu se souhlasem autora.

## 2026-10-06 – Odesílání souborů a schránka v okně terminálu

- **Proč:** BOOT MIKROBIOSu (JPR-1S) a programy CP/M (DOCPM) potřebovaly TeraTerm nebo PowerShell.
- **Tři způsoby** (autor): soubor tak, jak je; textový soubor (UTF-8 → kódy SAPI, konce řádků → CR, stejně jako
  psaní); XMODEM. XMODEM je v jádru (`core/xmodem`), ne v GUI, kvůli testu a případnému MCP. Časový limit
  (10 s bez odpovědi = poslat znovu) počítá GUI v reálném čase.
- **Schránka:** kopíruje se výběr myší, bez výběru celý text terminálu (výběr doplněn 2026-10-06, viz níže). Ctrl+C a Ctrl+V musí dál chodit do SAPI
  jako řídicí znaky, proto zkratky **Alt+C / Alt+V jako v TeraTermu** (autor) a Ctrl/Shift+Insert; AltGr
  (Ctrl+Alt) dál píše znaky.
- **Příjem XMODEM** (autor): XMODEM nenese jméno souboru, proto se cíl zvolí dialogem „Uložit jako“ předem (jako
  TeraTerm). Výplň 1Ah na konci se ve výchozím stavu odřízne (text CP/M), zaškrtávátko pro binární soubory.
  Přijímač je v jádru (`XmodemReceiver`), test ho ověřuje proti DOPC10.COM.
- **Řádek tlačítek se zalamuje**, když se do okna nevejde (výchozí šířka 700 bodů).

## 2026-10-06 – Kazeta z programu (HEX, BIN → MIKOS, MIKROBASIC)

- **Formát** podle zdrojáků ROM (MIKOS V4.0F, V5.0, MIKROBASIC 2.4, bajtově shodné s ROM v repu) a tří skutečných
  nahrávek; generátor dává bajtově stejné bloky jako originální TOOL 80 (`docs/software/kazeta.md`).
- **Vložení** (autor): program se vloží do magnetofonu jako kazeta a emulátor ji hned spočítá. Kazeta je jen pro
  čtení a nikdy se nezapíše zpět (soubor je HEX). Kopii pro skutečné SAPI dá „Uložit jako WAV…“, cli
  `--make-tape` bez spuštění stroje.
- **Oba formáty** (autor): MIKOS i MIKROBASIC, volba v dialogu. Jméno na kazetě podle jména souboru, jde přepsat
  (autor), nejvýš 11 znaků (pole jména v bloku MIKOS).
- **Zvuk** 48 kHz (přesně 20 vzorků na bit), obdélník jako záznam emulované DSM-1, mezery jsou tón a ne ticho
  (originální nahrávky jsou souvislý tón, chování skutečného dekodéru po tichu neznáme). Délky tónů jako na
  originálech, ne minima, která stačí emulátoru: kazeta má projít i skutečným magnetofonem.
- **Vstupní bod MIKOS:** `.J` skáče na nejnižší adresu, jiný start se řeší skokem `JMP start` těsně před programem
  (ve V5 pak program spustí i `.L`). Pod 4400h nic (RAM MIKOSu, nahrání přes ni rozbije čtení).
- **MIKROBASIC a strojový kód:** LOAD nahrává vždy od 40EFh. Kód od 4102h dostane řádek `1 CALL start` / `2 END`,
  takže ho spustí RUN (ověřeno v emulátoru). Mezi 40F0h a 4101h nic (řádek se nevejde).
- **Stav:** ukládá soubor, formát, jméno a adresy a kazetu vyrobí znovu. Při tom se opravila chyba, kdy se
  kazeta po načtení stavu do nového stroje převinula na začátek (týkalo se i WAV). Verze formátu stavu se
  nezvyšovala: starší stav se sestavou s DSM-1 se odmítne chybou bloku, ostatní jdou načíst dál.

## 2026-10-06 – Konfigurátor ukazuje změny, které běžící stroj ještě nemá

- **Proč** (autor): po změně karty nebo propojky nebylo vidět, že se nic nestalo a je potřeba stroj zapnout znovu
  (třeba nová DSM-1 nebyla v okně Média).
- **Kde** (autor, všechna tři místa): oranžové tlačítko a pruh v konfigurátoru, značky u slotu a parametru (bublina
  s hodnotou běžícího stroje), stavový text v hlavním okně i se zavřeným konfigurátorem. Kliknutí na stavový text
  jen otevře konfigurátor, restart nedělá (ztratila by se paměť bez varování).
- **Tlačítko** za chodu „Restartovat s touto sestavou“ (autor), vypnuté „Zapnout s touto sestavou“.
- **Porovnání** s popisem, se kterým stroj běží (`Machine::config`): bez názvu a popisu sestavy, karty po slotech
  podle parametrů katalogu (hodnota zapsaná stejná jako výchozí není změna). Parametry „(platí hned)“ se posílají
  stroji hned, značku nedostanou.
- Ověřeno snímky (`--ui-set` pro snímek se změnou) a na všech 44 sestavách bez falešného označení po startu.
- **Vrátit změny** (autor: po smazání karty bylo neaktivní, protože soubor byl uložený nebo sestava nová): při
  oranžových změnách vrací na sestavu běžícího stroje, jinak na uložený soubor.
- **Parametry „(platí hned)“ oběma směry:** konfigurátor si pamatuje hodnotu běžícího stroje z minulého snímku.
  Změnil-li ji stroj (MCP `turbo`), převezme ji, jinak pošle stroji hodnotu z konfigurátoru. Dřív konfigurátor
  změnu z MCP hned vracel.

## 2026-10-06 – ANK-1 z klávesnice PC: držíš = drží, režim pro hry

- **Proč** (autor): v Manic Mineru a hře s autem se klávesy hromadily, na české klávesnici se občas psaly dva
  znaky a u „,“ i „N“. ANK-1 je matice, kterou čte program, fronta stisků tam nemá co dělat.
- **Příčiny:** každý znak z PC (i autorepeat Windows při držení) šel do fronty ťuků po 40 ms; „,“ = SHIFT + N
  šlo dolů naráz, a když to padlo doprostřed skenování, MIKROBIOS přečetl N bez SHIFT a pak „,“.
- **Řešení** (autor: „držíš = drží“ a režim pro hry, jen ANK-1, ANK-1 MIKOS a ANK-1M2; kódované EKL-1 a Consul
  zůstávají s frontou, odpovídá to jejich hardwaru):
  - klávesa matice je dole, dokud je dole klávesa PC (aspoň `press_ms` a dokud ji program nepřečte), autorepeat
    PC se zahazuje;
  - SHIFT dolů dřív, klávesa až po přečtení řádku se SHIFT; nahoru opačně;
  - psané klávesy po sobě s fází uvolnění jako dřív fronta (MIKROBASIC chce mezi klávesami kolo bez stisku);
  - režim pro hry (Ctrl+F7): podle polohy, Shift = SHIFT, klávesy naráz. Zkratka vedle Ctrl+F8 (joystick);
    F6 a F7 bez Ctrl zůstávají INT a T;
  - fronta (vkládání, MCP) používá stejný mechanismus ťuků, takže i ona už „N,“ nepíše.
- **Ověřeno:** `test_keyboards` (porty: pořadí SHIFT a klávesy, ťuk kratší než sken, režim pro hry; MIKROBASIC
  třikrát řádek s „,;?“ psaný překrývajícími se klávesami, klávesa držená 2 s = jeden znak; MIKROBIOS: obraz po
  psaní drženými klávesami je stejný jako po frontě, „D 4400,44;FF?“; dřív fronta psala „D 4400N,44B;FF2?“).

## 2026-10-06 – JPR-1X a procesor 8085

- **Procesor** (autor: plné jádro 8085): režim 8085 přímo v jádru 8080 (`third_party/i8080`, přepínač `i8085`), ne
  samostatná kopie. Jádro 8080 tak zůstává jedno a jeho testy hlídají, že se režim 8080 nezměnil. Takty podle Intel
  MCS-80/85 (shoda s MAME), dokumentované příznaky ověřené 8085EXM proti CRC skutečného D8085AH-1. V a K podle
  analýzy křemíku (K. Shirriff), nejisté body rozhodnuté a zapsané v `docs/8085.md`.
- **Deska** jako varianta třídy `Jpr1` (stejná mapa a porty jako JPR-1). Podle netlistu: všechny zápisy i na
  sběrnici, čtení 3000–FFFF ze sběrnice, 3214 s INTE v 1 zachytí požadavek i pod DI.
- **TURBO** jako u JPR-1V: časová základna 4 MHz, bez TURBO trvá takt CPU dvě jednotky; sběrnice 2 MHz.
- **ROM** (autor: dump zatím není): výchozí obraz dolní banka MIKROBIOS 1.02, horní banka MIKROBASIC 2.4. Parametr
  `rom` bere 8, 16 i 32 KB obraz podle čipu (28C64, 27C128, 27C256).
- **Sestavy** (autor): ekvivalent ZPS-1 a JPR-1X s BGP-1S (MIKROBIOS, RAM-1 místo bank JPR-1S).
- **Debugger:** u karet 8080/8085 se opkódy, které nejsou instrukcemi Z80, ukazují jako aliasy 8080 (`NOP*`,
  `JP*`, `CALL*`, `RET*`) nebo jako instrukce 8085 (RIM, SIM, DSUB…). Dřív se ukazovaly jako instrukce Z80 (JR, EXX).

## 2026-10-06 – Deska diagnostiky na K3 (JPR-1S, JPR-1X)

- **Revize 2.0** podle autorovy fotky (dvě tlačítka + na P2.0 a − na P2.7). Schéma máme jen 1.00 s jedním
  tlačítkem. Displeje jsou přímo na P2-OUT (vlevo) a P1-OUT (vpravo), segmenty a–g, dp = bity 0–7.
- **Parametr `k3`** u JPR-1S a JPR-1X (`none` / `diag`), okno v GUI (tlačítka se drží myší) a MCP `diag_board`.
  Stisk tlačítek je vstup z PC, do uloženého stavu se nepíše (jako joystick).
- **Software** pro desku se v podkladech nenašel (autor tušil MikroMon nebo Falcon). Ukázka (autor): čítač 00–99
  ve dvou verzích, s přerušením 3214 (JPR-1X) a čtením portu (JPR-1S i JPR-1X), vlastní assembler `tools/asm8080.py`.
- **BGP-1S bez přerušení v `jpr1x_bgp1s`:** na JPR-1X je tlačítko + na stejném vodiči jako /INT0 a BGP-1S (S2 =
  INT0) by ho držela stisknuté. Přerušení BGP-1S nikdo nepoužívá.

## 2026-10-06 – Výběr myší v okně terminálu

- **Jako v běžném terminálu:** výběr jde po proudu textu (od místa stisku po místo puštění, přes řádky), ne obdélník.
  Hranice jsou mezi znaky (nejbližší hranice buňky), dvojklik = slovo (znaky bez mezer), trojklik = řádek.
- **Kopírovat** (tlačítko, Alt+C, Ctrl+Insert, menu) bere výběr, bez výběru celý text jako dřív. Prostý klik výběr
  zruší, takže se „celý text“ dá dostat vždy.
- **Výběr drží svůj text** i když historie (3000 řádků) odroluje: řádky se počítají absolutně (`dropped` + index).
  Logika je v jádru (`core/term`), test `test_term`; GUI jen převádí myš na buňky a kreslí podklad.
- **Okno se tažením v textu nehýbe** (`NoMove` u textové části), jinak by ImGui tažením posouval celé okno.
- Snímek pro ověření: `--ui-terminal`, `--ui-term-text TEXT` a `--ui-term-sel L0,C0-L1,C1`.
- **Výběr rovnou do schránky** (autor, 2026-10-07): po puštění tlačítka myši (a hned po dvojkliku či trojkliku)
  se výběr zkopíruje, jako v PuTTY a TeraTermu. Volba „Výběr rovnou do schránky“ je ve výchozím stavu zapnutá
  a ukládá se do nastavení (`terminal.copy_on_select`). Prostý klik bez tažení schránku nemění.

## 2026-10-07 – FDC-1: MOTOR ON z /HL (zapojení EC1045)

- **Proč:** EC1045 připojoval mechaniky 5,25" HD přímým kabelem jako 8" a MOTOR ON vedl z /HL (přiklopení
  hlavy 8272), /M0–/M3 z FDC-1B nepoužíval (odpověď 2026-10-06). Autor chtěl tohle zapojení v emulaci.
- **Jak:** parametr FDC-1 `motor_hl`. Disk se točí od přiklopení hlavy + `spinup_ms`, dokud ji 8272 neodklopí
  (HUT po posledním povelu). 8272 si pamatuje čas přiklopení (`head_load_time`, ukládá se do stavu). Registr 0FH
  v tom režimu nic neřídí. Vylučuje se s `motor_line` (motory z 0FH, BIOSy J. Bíby a mechaniky s /M0–/M3).
- **Sestava `cpm_ec1045`** má `motor_hl` a SA2 mechanik A, B jako pro 8" (`8sd`, Petrovo `11000011`).

## 2026-10-07 – Přetahování karet v konfigurátoru

- **Přání autora** („kosmetická frajeřinka“). Na volný slot se karta přesune, na slot, kde začíná stejně široká
  karta, se obě prohodí. Jiné případy (CPU mimo X1, přesah rámu, různá šířka, zčásti obsazený cíl) nejdou,
  cíl je červený s důvodem. Pravidla jsou stejná jako u výběru karty ze seznamu (`card_allowed`).
- Mění se jen `slot` v popisu sestavy, nic jiného na slotu nezávisí. **Sloty sběrnice jsou rovnocenné** (autor:
  pořadí je estetická záležitost; v emulaci se souběh na sběrnici skládá jako wired AND v libovolném pořadí),
  proto se přesun provede rovnou i v běžícím stroji (`Machine::move_card`, `set_card_slots`: čísla slotů karet
  a jejich záznamy v `config`). Sestava je pak jen neuložená, ne oranžová. Když běžící stroj v dotčených slotech
  nemá totéž co editor (čekající změna), přesun zůstane jen v editoru a oranžový. „Vrátit změny“ na uložený
  soubor vrátí karty na původní místa i v běžícím stroji, pokud se obě sestavy liší jen pozicemi karet.
  Test `test_state`: stroj s přesunutými kartami běží stejně jako bez přesunu a jeho stav jde načíst.
- **Vzhled:** zdroj ztmavne, karta (u RPD-1 a FDC-1 obě desky s kabelem) jede za myší poloprůhledná a zvednutá,
  cíl je zelený nebo červený, kurzor ruka.
- **Ověření:** snímek `--ui-drag Z:DO[:drop]` simuluje tažení událostmi myši ImGui (poloha slotů z posledního
  snímku v `RackEditor::geo_*`).

## 2026-10-07 – Release bez záchranné cesty ke zdrojákům

- **Proč:** autor má vedle repa rozbalenou alfu 0.2.0 (`..\SAPIemu-release`) a v repu se objevila přepsaná sestava.
  Alfa za to nemohla (data hledá od složky s exe, má vlastní `roms/` a `machines/`). Každý build ale měl
  zakompilovanou cestu ke zdrojákům jako poslední záchranu, takže zabalené exe spuštěné ze složky bez dat by na
  autorově PC sáhlo do repa.
- **Jak:** volba CMake `SAPIEMU_SOURCE_FALLBACK` (výchozí zapnutá pro debug a lokální build mimo repo), presety
  `windows-release` a `linux-release` ji vypínají, `tools/package.py` build se zapnutou volbou odmítne.
- Přepsanou sestavu `sapi1v.sapi` (jen komentáře zapisovače) nejspíš uložilo jiné sezení přes MCP `set_machine_config`
  se `save_as` nad cli z repa. Vráceno.

## 2026-10-07 – SAPI-1.5: ZPS-6 (PKT-1, RSW-1, DBS-1A)

- **Sestavy** (autor): nejdřív jen ZPS-6. ZPS-5 (PKT-1 + RSW-1, bez displeje a disket) nemá dochovaný firmware,
  přijde na řadu, až se najde. Software ani monitor se nevymýšlí.
- **Sběrnice** (autor): ARB-1C s JP3 = 1-2 (AEN vypnutý) a volným X1 místo SBI-1/SBK-1; rozdíly sběrnic (kolík 7 /REF,
  47 BOOT, 21/22 MAP) emulace nepotřebuje.
- **ROM** (autor): výchozí dump EPROM MON-V100690, opravená verze Melsoft jako volba. **Takt 2 MHz** (XT5 jako
  v dodávce ZPS-8, smyčky ROM jsou na 2 MHz).
- **Znakový generátor DBS-1** (autor: náhrada s označením): originál se nedochoval. Náhrada z fontu AND-1Z, který
  má stejné rozložení kódů (ASCII, semigrafika 2 × 3, KOI-8 čs2), v konfigurátoru i dokumentaci označená NÁHRADA.
  Dump skutečné EPROM se bude shánět. **Večer téhož dne** ho poslal EC1045 a je výchozí (níže).
- **Dekodéry** podle dumpů PROM ze sapi.cz (PKT-1 DD8, DD12; RSW-1 DD1, DD2). Tabulky v návodech Tesla jim místy
  odporují (port 0Eh PKT-1, pořadí portů RSW-1, propojky XT1 RSW-1); platí dump a schéma.
- **RSW-1 XT1** výchozí A6 = L, A7 = H, A8 = MAP2: jediné nastavení, při kterém dump DD1 dává mapu ZPS-8 (díra
  B000–BFFF pro video RAM při MAP2 = 1). Předpoklad, v Nejasnostech.
- **MFC-1A v ZPS-6** se zapojením EC1045: ROM BIOS i FORMAT a SYSGEN SAPI-1.5 zapínají mechaniky D6 = 1.
- **Disketa `.sp2`** (MELSOFT, 40 × 2 × 16 × 256 B) jako další formát podle velikosti (327 680 B se s ničím nekryje);
  disketa 3 má poškozený sektor BDOS, vedle originálu je `SAPI15_3_opr.sp2` se systémovou stopou z diskety 1.
- **Nové části emulátoru:** 8251 (asynchronně, znaky se správným časováním, rychlost z děliče čítače 0 8253),
  port B 8255 v režimu 1 jako vstup, /RTL (`Card::rtl_line`, watchdog RSW-1 resetuje sestavu přes PKT-1), 6845
  v DBS-1A (velikost obrazu z registrů, blikání atributu a kurzoru podle času sběrnice).
- **Klávesnice na PKT-1:** Consul 262.3 bez úpravy 7474 (8255 strobe zachytí). Dokud program kód nepřečte (IBF),
  další klávesa nejde (pohodlí, `keyboard.strict` věrně).

## 2026-10-07 – Snímky obrazovek (a plán videa)

- **Přání autora:** uložit obrázek z obrazovek (CGA-1V, DBS-1, AND-1…), později video se zvukem, na pravém tlačítku.
- **Zvyklosti:** MAME F12 (u nás reset), DOSBox Ctrl+F5 snímek a Ctrl+Alt+F5 video, PrtScn ve Windows 11 otevírá
  Výstřižky. Proto **Ctrl+F5** (snímek okna s fokusem) a pro video bude Ctrl+Alt+F5.
- **Formát:** PNG s přesnými body emulovaného obrazu (bezeztrátově, jako cli `--screenshot`), volba „zvětšený 3×
  (jak je vidět)“: každý bod 3 řádky a šířka podle poměru stran TV (DBS-1A 1024 × 720). Automatický název
  `work/snimky/<karta>_<datum-čas>.png`, nebo dialog. Schránka dostane PNG i BMP (DIB ve Windows bere každý program).
  **Oprava 2026-10-09:** SDL 3 ve Windows dá do schránky jen první formát obrázku (`WIN_SetClipboardData`), takže
  tam bylo jen „PNG“: Word ho vložil, Salamander, terminály a další programy, které čtou `CF_DIB`, ne. Ve Windows
  proto schránku plní emulátor sám přes Win32 (CF_DIB i PNG), jinde dál SDL. Ověřené `--ui-copy` a .NET
  `Clipboard.GetImage()`.
  Kodér PNG zůstává bez komprese (malé obrázky, žádná knihovna navíc).
- **Video (hotové týž den):** MP4 (H.264 + AAC) přes **ffmpeg**, který je potřeba mít (PATH, nebo `"video":
  {"ffmpeg"}` v nastavení); přehraje se všude a je malé. DOSBox má AVI s vlastním kodekem ZMBV, MAME nekomprimované
  AVI. Jak to funguje:
  - `EmuHost::tick_hook` dělí běh emulace na úseky po 1/50 s emulovaného času a na každé hranici se vykreslí snímek
    (přesné body karty, BGRA rourou do ffmpeg, bez okna konzole přes CreateProcess). Zvětšení jako u snímku „jak je
    vidět“, nejvýš 1080 řádků (VGA 2×, ostatní 3×), libx264 `-crf 18`; **změněno týž den večer** (níže);
  - zvuk zapisuje `Bus::audio_record` od stejného okamžiku; na konci se zkrátí na délku obrazu a druhý běh ffmpeg
    spojí obě stopy (`-c:v copy`, AAC) ve vlákně, GUI mezitím běží dál;
  - obraz a zvuk drží synchron při každé rychlosti emulace, pauza video pozastaví; výměna sestavy nahrávání ukončí;
  - log ffmpeg je `work/videa/ffmpeg.log`.

## 2026-10-07 – CRM-1 (zálohovaná CMOS RAM) a ZPS-7 až ZPS-9

- **CRM-1** podle návodu Tesla 6XV 122 83 (OCR na sapi.cz), autor desku nemá a software pro ni neznáme. Propojky
  (blok 8 KB, posun každého kilobajtu CS1–CS4 nebo neosazeno) a SA1 jsou v konfigurátoru, obsah drží soubor jako SRAM
  DMR-1 (`work/crm-1/crm.bin`, jen s bateriemi).
- **SA1** emulujeme jako opravenou desku (zápis do CS1 zablokuje). Na deskách z výroby podle EC1045 nefunguje,
  chování neopravené desky neznáme.
- **RDY:** impulz DD6 asi 590 ns (`rdy_ns`, výpočet z R10 a C1), z toho 1 WAIT při 2 MHz a 2 při 4 MHz. `rdy_ns = 0`
  je odchylka pro pohodlí.
- **ZPS-7 až ZPS-9** jako samostatné sestavy neděláme: od ZPS-6 se liší jen rámem, sběrnicí SBK a počtem mechanik,
  což emulace nerozlišuje. Stačí poznámka v `docs/software/SAPI-1.5.md`.

## 2026-10-07 – Odpovědi EC1045: font DBS-1, zapojení MFC-1A, přepínače DPP-1

- **Znakový generátor DBS-1:** EC1045 poslal dump EPROM „59“ (Intel HEX). Je výchozím fontem
  (`roms/dbs-1a/DBS-1A_CharGen_59.bin`), náhrada z AND-1Z zůstává jako volba (kdo si ji zvolil, ať o ni nepřijde).
  Znak 98h je v EPROM kopií 86h; emulujeme EPROM, jak je (věrnost HW), a ptáme se v Nejasnostech.
- **Textový výstup DBS-1A** (MCP, `--print-screen`) má vlastní převod kódů, kde se EPROM liší od AND-1Z (rámečky,
  řecká písmena, symboly). Kód 00h zůstává mezerou, ačkoli EPROM tam má roh rámečku: prázdná video RAM by jinak
  byla text plný rohů. Obraz je přesně podle EPROM.
- **MFC-1A:** originál v. č. 0124 odpovídá překreslení EC1045. Výchozí `wiring` je proto `ec1045` (dřív `tesla`),
  volba `tesla` zůstává pro naskenované schéma. Hodnoty se nepřejmenovaly, uložené sestavy platí dál; všechny
  sestavy v repu už `ec1045` měly.
- **DPP-1:** přepínače z fotky EC1045 potvrzují předpoklad emulace (28h, kanál 2 na X3 obousměrný). Emulace
  přepínače S2–S4 dál nemodeluje, jen jsou rozepsané v `docs/desky/DPP-1.md` i s chybou tabulek 7 a 9 návodu.

## 2026-10-07 – RTC-1

- **Podklady:** návod Tesla (OCR), originální schéma (sken J. Bíby `TMP86/87.tif`) a replika EC1045; netlist repliky
  se s originálem shoduje. Programy J. Bíby (`RTCZ.COM`) slouží jako test.
- **Čas čítače** výchozí podle hodin PC, stejně jako RTC 72421 na ZRMD-1. Čítač počítá dvousekundy od libovolného
  začátku, který si určí program. Po prvním zapnutí (bez souboru baterie) proto ukazuje čas od pondělní půlnoci:
  týdenní hodiny J. Bíby pak hned ukážou den a čas PC. Volba `emulated` běží podle času emulace (deterministicky).
  Hodnota a čas PC se při vypnutí ukládají do souboru baterie, v režimu PC čítač „běží dál“.
- **Přerušení** jde na desce jen na XC2/14, proto parametr „kabel“ s výchozím `none`.
- **8255 v módu 1 bez /STB A** (propojky z výroby): čtení PA dává aktuální hodnotu (předpoklad podle funkčního
  programu J. Bíby), v Nejasnostech.
- **/RTL pro všechny procesorové karty:** `Bus::rtl()` zkouší JPR-1, JPR-1Z/ZI/V/S/X i PKT-1 při synchronizaci
  a resetují celou sestavu (na deskách /RTL vede na reset přes 8224, monostabil nebo MAX707).
- **Φ2 sběrnice** (`Bus::phi2_hz`, `phi2_hz()` procesorové karty): u JPR-1ZI, JPR-1V a JPR-1X takt sběrnice, jinak takt
  CPU. Na něm běží systémový 8253 RTC-1.
- **8253:** zapsaný počet v režimu BCD se nepřeváděl z BCD (BCD 2000 se počítal jako 8192), opraveno. Nová metoda
  `ticks()` přeskakuje mezi změnami OUT v módech 2 a 3, aby 8253 na 2 MHz nezdržoval emulaci; test ji porovnává
  s jednotlivými takty.

## 2026-10-07 – RTC 72421 na ZRMD-1: HOLD

- `RTC421.COM` J. Bíby čte čas postupem z datasheetu (HOLD = 1, kontrola BUSY, čtení, HOLD = 0) a nic nevypsal:
  emulace registry v HOLD neobnovovala. Náběh HOLD teď zachytí čas toho okamžiku (čip během HOLD jen nepřičítá).
- Den v týdnu zůstává 0 = neděle (zvyklost Epsonu, tak ho emulace plní z času PC). Bíbův program ukazuje W + 1.

## 2026-10-07 – DSP-1

- **Podklady:** návod Tesla (OCR a sken), replika EC1045 (originální schéma Tesla se nenašlo), programy XMODEM
  J. Bíby. Programy jsou zkopírované do `media/dsp-1` (autor souhlasil), aby je mohl používat test.
- **Výchozí nastavení jako J. Bíba** (48h, 4Ah, 4Ch, 9600 Bd, úplný kabel), protože jediný známý software ho čeká.
  SA2 je v konfigurátoru jako horní číslice, ne po spínačích, protože pořadí A5/A6 na přepínači se mezi návodem
  a replikou liší.
- **RTS do PC se nemodeluje**, místo řízení toku slouží `pacing` (SAPI dostane další znak, až přečte předchozí).

## 2026-10-07 – Video: sudé zvětšení

- **Autor:** video funguje, ale body nejsou úplně ostré. Příčina: H.264 v `yuv420p` (jiný formát mnoho přehrávačů
  neumí) má jednu barvu na blok 2 × 2. Při zvětšení 3× bod nesedí na bloky a barevné hrany se rozmažou.
- **Změřeno** (obraz BGP-1S s barevným šumem v pozadí, podíl bodů s chybou > 24): ×3 crf 18 0,20 %, ×3 crf 12 0,03 %,
  ×3 4:4:4 0,09 %, **×4 crf 18 0,01 %** a soubor je navíc 2,5× menší.
- **Teď:** zvětšení vždy sudé, 4× (2× u VGA, nejvýš 1440 řádků), šířka podle poměru stran zaokrouhlená na sudé číslo,
  `-crf 14`. ZPS-6 tedy 2048 × 960, BGP-1S 1152 × 1024, VGA 1280 × 960.
- Rozmazání může přidat i přehrávač, když video zvětšuje nebo zmenšuje do okna (bilineárně). To emulátor neovlivní.

## 2026-10-08 – Sběrnice ImBUS9 a ImBUS12

- **Přání autora:** sběrnice Libora Lasoty ImBUS9 (rev. 1.1, `H:\SAPI-1\LiborLasota\ImBUS9 rev. 1.1\`) a stejná
  na 12 karet jako typy `ImBUS9` a `ImBUS12`.
- **Ze schématu:** X1 (K1) je pro procesorovou kartu, její A0–A15 jdou přes 4 × MH3216 (vypíná je /AEN) do ostatních
  pozic, data a řídicí signály vedou přímo. Propojka jako JP3 u ARB-1C tu není, proto `has_jp3 = false` a X1 jen
  pro CPU (stejné pravidlo jako ostatní sběrnice). Napájení z ATX, LED a JP1/JP2 (S4, /RTL) emulace nepotřebuje.

## 2026-10-08 – APS-5: desky, panel a technologie

- **Přání autora:** emulovat APS-5 tak, aby bylo co měřit a na co koukat (jinak než v emulátoru už to asi nikdo
  nezapojí). Proto vznikla simulace pecí s oknem.
- **Technologie mimo sběrnici** (`Plant`, `Bus::plant`, oddíl `"plant"` sestavy). Pece, termočlánky
  a svorkovnice nejsou desky SAPI. Karty (DMT-1, ADP-1, DRV-1A, HRK-1) si z technologie berou napětí a vstupy
  a posílají jí kontakty a výstupy. Konfigurátor ji neukazuje, upravuje se v souboru a v okně APS-5.
- **Panel HRK-1 jako „klávesnice“ JPR-1:** HRK-1 i všechny kabely APS-5 vedou na X2/X3, kde je u JPR-1 klávesnice.
  `KeyboardDevice` proto dostal P2-IN, P1-OUT, P2-OUT a /RTL. Nový typ vazby se zavádět nemusel.
- **Zapojení odvozené z firmwaru** (návod ho nepopisuje): ADP-1 S6 = 0 V, S7 = KAL (kalibrace měří nejdřív
  nulu), P2-IN4/5 = 1 Hz / 50 Hz z DD5 RTC-1, kódy kláves, P2-IN7 = /PFAIL. P2-IN7 jsem nejdřív zkusil jako
  kontakt relé KA1, ale firmware pak po resetu dostal trvalou žádost R7 (RST 0). Jako výpadek sítě to sedí na
  všechna místa i na návod (uložení stavu, změření výpadku, „CHYBA n“ po resetu watchdogem).
- **Logika 3214 opravená pro všechny JPR-1 a JPR-1X:** stav ~D & 7, projde úroveň vyšší, D3 = 1 všechno. Dokládá
  to firmware APS-5 (04h, 02h) a schéma JPR-1X (/B0–/B2, /SGS). Vlastní ukázka `diag_int` a test JPR-1X teď
  zapisují 08h.
- **Vypnutí = výpadek sítě s varováním** (`Machine::power_down`, jen když to zařízení umí, tj. APS-5). Bez toho by
  každé vypnutí emulátoru vypadalo jako reset watchdogem a firmware by po „CHYBA“ smazal programy. Skutečný
  zdroj varování dává vždy. Násilné ukončení procesu ho nedá, pak se APS-5 chová jako po chybě.
- **Termočlánky podle norem** (NIST, DIN 43710, GOST), ne podle firmwaru (ten počítá v plovoucí čárce).
  Shoda 0–3 °C v celém rozsahu.
- **Zesílení DMT-1** podle tabulky firmwaru (100, 199, 498, 1000), aby měření sedělo jako na seřízené desce.

## 2026-10-09 – APS-5: tlačítko DEMO

- **Přání autora:** stručný návod, jak zadat něco, co ukazuje (`docs/aps5-ukazka.md`), a pak tlačítko, které ho
  odťuká samo.
- **DEMO jako obsluha, ne zápis do paměti:** klávesy jdou přes panel HRK-1 a po každé se čeká na výzvu firmwaru
  (`src/plant/aps5_demo.*`). Zadání tak dělá původní firmware a zůstává věrné. Zápis do CRM-1 by vyžadoval znát
  jeho datové struktury a obešel by kontroly firmwaru.
- **Zrychlení:** během zadávání a 25 minut nahřívání běží emulace naplno, pak se vrátí původní rychlost. Autor:
  „klidně i zrychli emulaci a jen to oznam“, proto je oznámení pod tlačítkem i ve stavovém řádku.
- **Program 1:** firmware nedovolí smazat program přiřazený peci. Demo proto program, který začíná jeho
  segmentem, nechá a jiný nepřepíše.
- **RESET zůstává věrný** („nech to věrně“): u APS-5 vede na CHYBA 0 a studený start, v dokumentaci je varování.


## 2026-10-09 – KZD-1 (kazetopáskový záznamník dat) a MIKOS/K

- **Přání autora:** připojit KZD-1 k SAPI-1 („jsem přesvědčen, že se k SAPI připojoval“). Potvrzuje to MIKOS/K
  V5.0 (TESLA ELTOS DIZ 1988) s kabelem na DPP-1 X2. Plán (DPP-1, soubory `.kzd`, sestava, kazeta s cívkami, testy)
  autor odsouhlasil.
- **Model na úrovni signálů rozhraní, ne bajtů:** páska je seznam změn magnetizace (poloha v µm, stopa), mechanika
  má rozběh, doběh a přetížení motorů na konci. MIKOS/K tak běží nezměněný se svým časováním (hledání mezer,
  převíjení do přetížení) a jde použít i jiný software (ovladač z PMD 85, CKM).
- **Vlastní formát `.kzd`** místo zvuku: KZD-1 je digitální záznamník (impulsy 40 µs, žádný analogový signál),
  WAV by nic nepřidal. Soubor nese obě strany a zoubky proti zápisu.
- **Zoubek jen hlásí** (ZÁZN. U.V.*), zápis neblokuje, jak je to na skutečném přístroji. Kontrolu dělá software.
- **ROM = originální dump 6. 4. 88** (`MIKOS5K.BIN`). Verzi 18. 5. 88 (`KZD2.MAC`) jsem přeložil pro DPP-1 jen
  kvůli ověření. Chová se stejně, do repa nejde.
- **K_R před K_L nechávám věrně:** po K_R hledá K_L od fyzického začátku pásky. Úsek 55h z K_I přečte jako vadný
  blok a jméno porovná se starým bufferem. Po K_D tak skončí `NENI KONEC SOUBORU`. Ověřeno trasováním obou verzí
  MIKOSu/K, chyba je ve firmwaru, ne v emulaci. Návod k načítání od začátku nabízí K_Z nebo `.JMÉNO`, test je používá.
- **Bez povoleného zápisu souborů** (testy, `battery` vypnuté) se chybějící kazeta vytvoří jen v paměti. Testy tak
  nezakládají soubory ve `work/`.
- **CKM z originálního HEXu** (sapi.cz), ne z vlastního překladu. Překlad `CKM.MAC` se s ním shoduje, jen jeden
  symbol se musel opravit (`SPSTAR` → `SPSTART`, starý M80 rozlišoval 6 znaků).
- **Program → kazeta `.kzd` podle toho, co zapisuje MIKOS/K** (rozložení, mezery a rozestupy bitů změřené na páskách,
  které zapsal emulovaný MIKOS/K), ne podle vlastního zjednodušeného formátu. Načte ho originální K_L. Kazeta se
  ukládá do `work/kzd-1/JMÉNO.kzd`, existující soubor se nepřepisuje (`JMÉNO-2.kzd`…).
- **Chyba CKM s délkou bloku zůstává věrně** (nalezeno při psaní návodu): po čtení z KZD-1 CKM zapíše do `DEBL` 0
  místo FFh a další zápisy na KZD-1 dělají jednobajtové bloky. Je to chyba originálního programu (zdroják i HEX ze
  sapi.cz). Emulátor ani `CKM.hex` ji neopravují, návod ukazuje obejití (`.K_X FF`, reset).
- **Opravená kopie CKM vedle originálu** (na přání autora): `media/mikos/CKM_opraveny.hex`, vyrobená skriptem
  `tools/ckm_oprava.py` z nedotčeného `CKM.hex`, ne ručně. Místo vložení chybějící `CMA` (posunula by kód a skoky)
  jeden bajt `XRA A` → `SBB A`, který na tom místě dává FFh díky vždy nastavenému CY z `CPI 0CH`. Na obrazovce je
  poznat podle nápisu `OPRAVENY` v prvním řádku menu. Návod doporučuje opravenou kopii, originál zůstává pro věrnost.
- **Návody k obsluze** (`docs/navod-kzd1.md`, `docs/navod-minigraf.md`) jsou zvlášť od technických popisů desek.
  Jsou psané pro uživatele, krok za krokem, a výpisy obrazovky v nich jsou opsané z emulátoru, ne z původních
  návodů. Co se nezkoušelo, je tak označené.

## 2026-10-09 – zapisovač ARITMA Minigraf 0507

- **Proč Minigraf:** ze zapisovačů u SAPI-1 (Minigraf, XY 4130, BASIC EXP V5.0/G+SZ, plotter z AR 1990) má úplný
  a ověřený software (tři verze ovladače s výpisy), popis rozhraní od výrobce, žádné zpětné signály a hlavně
  skutečné výstupy ze SAPI-1 na sapi.cz, se kterými jde emulaci porovnat. XY 4130 nemá ukázku ani technické údaje.
  Autor ho měl v zásobníku nápadů jako „nejvíc na koukání“.
- **Model na úrovni signálů** (fázové kódy, pero, dorazy) a původní ovladač V.1-MGF v EPROM, ne vlastní příkazy.
  Měřítko 0,125 mm na změnu kódu podle letáku. Průzkum z výpisu usoudil na půl kroku na jednotku, ale ovladač
  rozdíl souřadnic zdvojuje (SMR, `DAD H`), takže jednotka je celý krok. Potvrzuje to i shoda se skutečnými kresbami
  (znaková sada 83,75 × 184,75 mm, ukázka z IQ 151).
- **Papír jako pás šířky A4:** originální ovladač posouvá papír při nastavení počátku o 212 mm a TEST o 437 mm.
  Na list A4 by se nevešel a nevíme, jak se papír zakládal. Pás kreslí všechno a export se ořízne na kresbu.
- **Rychlost se neomezuje:** kroky nad 400/s z letáku se jen počítají. Ovladač je při kreslení dělá běžně, kresby
  na sapi.cz jsou přesto v pořádku. Otázka je v Nejasnostech.
- **Pero dole po zapnutí** (port B je vstup a budiče dávají H) zůstává věrně. Tečku smaže Nový papír.
- **BASIC EXP V5.0/G+SZ s upravenými porty:** jediná dochovaná kopie má porty F3h/F1h, kam DPP-1 nedosáhne. Šest
  bajtů je vrácených na 33h/31h podle tabulky „původní obsah“ v návodu. Úprava je popsaná v `media/README.md`,
  originál zůstává na sapi.cz.
- **Export oříznutý na kresbu** (MCP `crop`) pro obrázky do dokumentace; okno ukládá celou šířku papíru.
- **Ukázky s `KRES` začínají dojetím na levý doraz** (`KRES #I -220,0,, #I 90,0,,`): `KRES` počítá od místa, kde
  pero stojí, a to se liší (po zapnutí 60 mm od dorazu, po `.G=1800` 10 mm). Dojetí na doraz používá i původní
  ovladač, na skutečném zapisovači je to běžný způsob, jak najít počátek (nemá koncové spínače).
- **Vložení textu na klávesnici** (Stroj → Klávesnice → Vložit text, Ctrl+Shift+V): ukázku v BASICu by jinak bylo
  nutné opisovat. Klávesy jdou frontou klávesnice stejně jako `type_text` přes MCP. BASIC EXP stíhá celý program
  vložený najednou (ověřeno).

## 2026-10-09 – podněty z portu Prince of Persia a dema Bad Apple!!

- **Čekací stavy CGA-1V podle schématu, ne podle průměru:** RDY drží IC9B od STSTB do konce okna CPU, které
  přidělí IC7B na hraně QA. Čtení a zápis se liší jen hranou Z80 (/RD na sestupné hraně T1, /WR na sestupné
  hraně T2), čekání je v celých taktech CPU (`Bus::cpu_step` pro JPR-1V při 2 MHz). Jediné volné číslo, součet
  zpoždění 90 ns, je nastavené podle měření ZRDBENCH na skutečné SAPI-1 V. Ladit na setiny nemá smysl:
  výsledek `LDIR` se mění o desetiny podle přesného poměru taktu CPU a okna. Rozdíl 0,3 T u zápisu při 2 MHz je
  v Nejasnostech CGA-1V.
- **Zvuk se míchá průběžně i během jednoho dlouhého běhu** (`Machine::run_cycles` volá `audio_collect` po každém
  kousku). Zdroje smí přidat nejvýš 4 s před smíchanou částí a karta dohání nejvýš 1 s naráz, takže jedno
  `run_for 40000` dřív nechalo v nahrávce ticho. Zahozené vzorky se počítají (`Bus::audio_lost`) a `audio_record`
  je hlásí, aby ticho nešlo splést s chybou programu.
- **Vylepšení z podnětů zadal autor** („jdi do těch vylepšení“, do příští alfy, 0.4.0 už byla odeslaná):
  - **CGA-1V se kreslí s paprskem, výchozí zapnuto.** Řádek pixelů y tvoří řádky VGA 2y a 2y+1. Karta před každým
    zápisem (videopaměť, CONFIG, COLMASK, paleta) dokreslí řádky, kterými paprsek mezitím prošel, starým
    stavem. Trhání a změny uprostřed snímku jsou tak vidět jako na monitoru. Na HW se to ukázalo
    u PoP: první verze „zasněžila“ a přeblikla, v emulátoru vidět nebylo. Volba `beam` vrací kreslení celého
    snímku z jednoho stavu. Prolínání snímků (REMBRAN2) zůstává, prolínají se dva hotové snímky. Kreslený obraz
    se neukládá do stavu, po načtení se nakreslí znovu z videopaměti.
  - **„Sníh“ Bt476 jen jako volba (`palette_snow`, výchozí vypnuto)**: jak přesně vypadá na železe, nevíme.
    Přibližné zobrazení je pomůcka, ne věrná emulace, a je tak označené. Věrná je statistika: zápisy do palety
    během zobrazování a přepnutí uprostřed snímku v `describe` (MCP `machine_info`), první zápis jednou v logu.
  - **Náhodný obsah pamětí po zapnutí je volba sestavy (`power_on_memory`), výchozí jsou nuly.** Věrnější je
    náhodný obsah, ale na nulách stojí stávající sestavy a testy (deterministické výsledky). Generátor je
    xorshift32 se semínkem z hodin, každé zapnutí je jiné. Paměti zálohované baterií a RAM disky mají dál
    obsah ze souboru, nezálohovaná RAM, videopaměti i palety jsou náhodné.
  - **Log přerušení** je kruhový (1024 záznamů) a zapíná se přes MCP `int_log`. Vektor IM 2 čtený z paměti
    zobrazovací karty je skoro jistě chyba programu (I ukazuje do videopaměti), proto ho log hlásí vždy, jednou
    za kartu.
  - **ECMON se nemění.** Reset s podpisem `AA 55 00` a `(003Bh) > 3` bootuje dál bez kontroly, jako originální
    EPROM. Emulátor jen napíše do logu varování. Podpis najde v paměti F000–FFFF podle instrukcí, které ho
    testují (`CHECK EQU $+2`), takže funguje pro všechny překlady ECMONu. Kontrola běží jednou za reset:
    držená /RTL (watchdog, výpadek sítě u APS-5) resetuje sběrnici při každé synchronizaci JPR-1 a kontrola
    při každém volání zpomalila `test_aps5` z 8 s na 113 s (release). Tlačítko RESET kontroluje vždy.
  - **MCP `quit`** stroj nejdřív vypne (média se uloží jako při vypnutí), odpověď ještě odejde, pak program skončí.
  - **Čekání v celých taktech CPU i na JPR-1V při 2 MHz**: Z80 vzorkuje /WAIT na svých hranách, půl taktu stát
    nemůže.
- **ZRDBENCH převzatý do `media/zrd-1v`** (port PoP to nabídl, vlastní kód bez dat hry). Test spouští skutečný
  program pod CP/M při 4 i 2 MHz a porovnává všech sedm řádků s měřením na železe. Testovat originální program
  je lepší než napodobený `LDIR`: smyčky, okraje bloků a kalibrace časovačem jsou stejné jako na HW. ZRDTEST
  hlídá test také, okno 74LS123 při 2 MHz vyjde „19..20 ms“ jen kvůli kalibraci jeho smyčky
  (`docs/desky/ZRMD-1.md`).

## 2026-10-09 – čekací stavy AND-1V

- **Podle schématu rev 2.3, list 4/7, stejným výpočtem jako CGA-1V.** Obě desky Libora Lasoty mají stejný
  synchronizátor: STSTB se SEL stáhne RDY, žádost CPU se zachytí na začátku okna CPU (u AND-1V sestupná
  hrana VIDEO, jednou za znak = 397 ns) a RDY se uvolní na konci okna (o půl znaku později). Výpočet je proto
  jeden, `Bus::window_waits(write, slot, open, req, rel)`, karty mu dají jen své časy.
- **Zpoždění převzatá z CGA-1V** (40 ns k žádosti, 50 ns k uvolnění), protože AND-1V zatím nikdo neměřil a jde
  o stejné rodiny obvodů (74LS74, 74LS00, 74LS32). Pro ověření je vlastní měřicí program VRAMBENCH
  (`media/and-1v`, podle ZRDBENCH), otázka je v Nejasnostech AND-1V.
- **Výchozí zapnuté** (parametr `wait_states`) jako u CGA-1V: věrnost má přednost, vypnutí je pohodlí pro
  rychlejší výpis. Textový výstup přes AND-1V se tím zpomalí o pár procent, jako na železe.
- **`mem_peek` AND-1V už nevolá `mem_read`**, aby nahlížení debuggeru a MCP nepřidávalo čekání.

## 2026-10-09 – řadič kazetového disku RKD-1

- **Emulace podle ovladače a schématu, ověření originálním softwarem.** Mechaniku CM 5403 nikdo nepopsal a lidi,
  kteří RKD-1 provozovali, autor nezná. Model proto vychází z toho, co čte a čeká ovladač `RKD.MAC` (Tesla Elstroj
  1987), z popisu řadiče ([R1] na sapi.cz a sken) a ze schématu repliky EC1045. Měřítkem je, že originální programy
  (FORMTRKD, ES, ROMES, CBIOS) běží beze změn: formátují, ověřují čtení, bootují a kopírují.
- **Obraz kazety jako bajty zapsané do sektorů**, ne jako data sektorů: řadič nemá CRC ani adresové značky,
  hlavičku a součet píše software. Tak funguje formát 24 × 256 i 12 × 512, rozbité sektory i zápis přes konec
  sektoru bez zvláštních případů. Vlastní formát `.rkd` s hlavičkou, skutečné kazety stejně nikdo nemá.
- **Polarita DISK podle Tesly** (kazeta = PC6 0): CBIOS bootuje z média 0 a dává mu systémové stopy. J. Bíba ji
  ve FORMRKD pro bulharskou CM 5400 má opačně, nejspíš jiná mechanika.
- **T00 jako hladina z klopného obvodu, který maže PC0** (netlist z vektorového PDF repliky, Eagle se zasekl na
  dialogu). Tím se vysvětlil i kabel SM 5400 se T00 na nejnižším bitu cylindru.
- **Časy mechaniky jsou odhad** (vystavení hlav jako DEC RK05, otáčky a délka bitu z popisu řadiče), rozběh po
  vložení kazety je volba `spinup_s` (výchozí 0, pohodlí).
- **Boot EPROM ze zdrojáků:** `ROMES.COM` z diskety je přeložený na 0100h (zavaděč z CP/M), jako EPROM nefunguje.
  Zdrojáky 87-02-10 jsou o tři dny starší než binárka, novější se nenašly; pro boot na tom nezáleží.
- **Náš SYSGEN jen jako krok výroby kazety:** systém z `ES 59` se zachytí v paměti při skoku do CBIOSu a zapíše se
  na stopy 0 a 1 média 0, odkud ho čtou boot ROM i WBOOT. Vše ostatní (formát, soubory) dělají originální programy
  a postup se dá zopakovat testem (`RKD_MAKE`).
- **Bulharskou CM 5400 a jednotky 1–3 software Tesly nepoužívá** (ovladač nastavuje jen jednotku 0). Jednotky 1–3
  karta umí, bulharskou mechaniku s posunutým kabelem zatím ne.
- **Zápis na chráněnou kazetu se tiše neprovede** (doplněno večer při psaní návodu). První odhad „ochrana = porucha
  mechaniky ERR“ systém zasekl: ovladač Tesly bit WPROTECT nečte, poruchu maže jen `RK_HOME` při opakování čtení,
  WBOOT ani boot ROM ho nevolají a `RK_SEEK` se při chybě vrací s `PUSH BC` na zásobníku. Ze zaseknutí nepomohl ani
  Reset. Mechaniky rodiny RK05 ochranou jen vypnou zápisový proud, emulace teď dělá totéž: nic se nezapíše, chyba
  nevznikne, CP/M o ničem neví. Otázka zůstává v Nejasnostech.
- **Výměnu kazety za běhu emulace nehlídá ani neopravuje:** CBIOS drží poslední sektor ve vyrovnávací paměti a výměnu
  nepozná, takže zápis po výměně poškodí adresář nové kazety, jako by to udělal na železe. Návod radí měnit kazetu
  s Resetem a používat jen kopie systémové kazety (bez SYSGENu jinou cestu nemáme).
- **Panely v konfigurátoru jako u RPD-1:** obě desky mají dva FRB 30, horní (X3) propojuje plochý kabel (bez názvu,
  v podkladech není), dolní (X2) vede k mechanice. Signály mechaniky jsou rozdělené mezi RKD-1A a RKD-1B.
- **SYSGEN jako přiznaná pomoc emulátoru** (tlačítko Systém… v okně Média, večer 2026-10-09): originál se nenašel a
  bez něj nejde udělat novou systémovou kazetu. Tlačítko kopíruje stopy 0 a 1 ze systémové kazety v `media/`
  (stejné bajty, jaké by zapsal SYSGEN) a volitelně formátuje stejnými bajty jako FORMTRKD. Soubory shellu se
  kopírují v CP/M přes pevný disk, takže jde o jediný krok, který dělá emulátor místo programu.
- **CM 5400 jako druhý typ mechaniky** (`unitN.drive`): obrácená polarita DISK (podle FORMRKD) a kabel SM 5400
  (cylindr 2N + T00, návrat nezapojený). Mechanika bere 408 cylindrů, protože FORMRKD formátuje stopy 0–203;
  s 203 cylindry by skončil na stopě 102. Odporuje to údaji 2,45 MB na disk, proto je to v Nejasnostech.
- **Hlavičky sektorů se neopravují ani nekontrolují:** FORMRKD.COM píše starší hlavičku (jen stopa, nižší bajt
  první) a emulace ji uloží, jak přišla. Obraz `.rkd` drží bajty, ne význam, takže fungují oba ovladače.
- **Test RKD-1 ve třech částech** běžících souběžně (`RKD_PART`, `COST` v `tests/CMakeLists.txt`): celá debug sada
  zůstává kolem 30 s.
- **Vlastní BIOS pro CM 5400** (autor: „udělat podporu disků bez toho, aniž by šly použít, mi přijde divné … byť bych
  to někde otevřeně uvedl“): BIOS-3K poskládaný ze 3K BIOSu J. Bíby (rámec a deblokování 512 B, přeložený shodně
  s originální disketou), ovladače ZRMD-1 z jeho BIOSu-1K a `RKD.MAC` Tesly ve tvaru z FORMRKD, aby šly číst disky
  naformátované dobovým programem. Je to nový software, proto: úvodní hláška „BIOS-3K RKD SAPIemu“, poznámka
  v hlavičce zdrojáku, v popisu sestavy, v návodu a v `media/README.md`. Zdroják vzniká skriptem z originálů, takže
  každá změna je dohledatelná. Opravená chyba `RK_SEEK` (BC na zásobníku) je jen v tomto BIOSu, ROM a CP/M Tesly
  ji mají dál.

## 2026-10-10 – RAM disky s hrami pro sestavu V

- **Obrazy RAM disků s hrami autora jsou v `media/zrmd-1`** (Bad Apple!!, Prince of Persia, záloha se všemi hrami)
  a dvě sestavy `sapi1v_BadApple.sapi`, `sapi1v_PoP.sapi` (autor: „chci je tam, naopak asi i commitnout“). Sestavy
  pracují na kopiích ve `work/`, originály v `media/` zůstávají.
- **Prince of Persia jen v repu:** POP.DAT jsou data hry (Jordan Mechner, Broderbund). Repo je neveřejné, ale balíček
  alfy jde kamarádům, proto ho `tools/package.py` vynechá (`REPO_ONLY`: oba obrazy s PoP a `sapi1v_PoP.sapi`).
  V konfigurátoru jsou volby obrazů i tak; v balíčku by obraz chyběl.

## 2026-10-10 – magnetická páska RMP-1

- **Mechanika IZOT SM 5300 bez manuálu:** parametry z RMP-1 (12,5 palce/s, 800 bpi, 9 stop), z robotrontechnik.de
  (cívka 216 mm, 350 m, převíjení asi 5 min) a z toho, co čeká ovladač `RMP.MAC` (čtení po zápisu do 32 ms, mezery
  20 ms při čtení). Vzdálenost hlav 0,15", rozběh a doběh 0,2" (ECMA-62), značka BOT 28 mm a EOT jsou odhady
  v Nejasnostech `docs/desky/RMP-1.md`.
- **Rozběh a doběh pásky se modelují:** bez nich DIRMT po přeskočení bloku vzad (`mt.bsr`) hlásí „Chyba cteni“.
  Ovladač zastaví 9 ms za blokem a při čtení vpřed čeká 20 ms bez dat, počítá tedy s doběhem mechaniky. Pásku
  posouvá rychlost s rampou 32,8 ms. Převíjení rampu nemá (trvá minuty).
- **Značka BOT drží 1,1":** `RMP.MAC` se na BOT ptá až po rozjetí pásky. S bodovou značkou zapsal VOL1 bez dlouhé
  mezery od BOT a čtení od BOT (260 ms bez čtení) ho pak přeskočilo.
- **Obraz pásky SIMH `.tap`:** záznamy a značky, jak je znají emulátory velkých počítačů a jejich nástroje. Uvnitř
  emulace je páska řádky s polohou, při načtení se rozloží s mezerami, jaké píše `RMP.MAC`, CRC a LRC se dopočítají.
  Chybný záznam (parita, CRC, LRC) se uloží s příznakem chyby SIMH.
- **`ready_at_start` (pohodlí, výchozí zapnuto, přiznané):** páska je po zapnutí zavedená a ON-LINE. Na železe to
  dělá obsluha tlačítky. Vypnutím se chová jako železo. Vložená páska za chodu se zavádí LOAD a ON-LINE vždy.
- **Ukázková páska vyrobená programem MTDAT** v testu (`RMP_MAKE`), ne skriptem: návěští a bloky jsou přesně, jak
  je dobový program zapisuje. Disketa s programy `a_lad_izo` je v repu jako `media/rmp-1/a_lad_izo.img`.
- **Panel mechaniky v okně Média je kreslený:** skutečný panel SM 5300 neznáme. Kontrolky odpovídají stavovým
  signálům rozhraní (port C5h), tlačítka běžným ovládacím prvkům páskových jednotek. MTDAT tomu, čemu emulátor
  říká ON-LINE, říká „DISTANC“.

## 2026-10-10 – plotter z AR 1990

- **Proč AR 1990 a ne XY 4130** (autor: „zkus ty Plottery, rozhoduj se sám“): plotter z AR 1990 má kompletní
  podklady (schéma, tabulku portů, celý výpis programu a manuál povelů) a kreslí Minigrafem, který už emulátor
  má. U XY 4130 chybí plocha, dorazy, polarity i jakákoli skutečná kresba ze SAPI na porovnání, zůstává
  v zásobníku (`docs/desky/XY4130.md`).
- **Program přepsaný z výpisu v AR, ne z přepisu v archivu:** přepis v `SAPI_DOKU` je „s mírnými
  zjednodušeními“ a má přes deset funkčních chyb (chybějící RET, JMP, XCHG, POP, záměny registrů a portů, čísla).
  Výpis v AR byl přepsán stránku po stránce a porovnán s archivním přepisem. Každý rozdíl je ověřený ve
  zvětšeném výřezu skenu. V repu je výpis tak, jak je vytištěný (`roms/ar1990/PLOTTER.MAC`). Adresy JPR-1
  dosazuje skript z komentářů výpisu, tištěný text se nemění.
- **Ruční oprava FAST ve skenu se nepoužívá:** v emulátoru je tištěná verze, oprava je zapsaná v
  `docs/desky/AR1990.md`.
- **Zařízení jako „klávesnice“ JPR-1** (KeyboardDevice, stejně jako panel HRK-1 u APS-5): plotter visí na K2 a K3,
  potřebuje všechny tři porty dovnitř i ven a reproduktor. Minigraf dostává bajt P0-OUT přímo
  (`Minigraf::set_lines`), JPR-1 ho vydá přes `minigraf()`, takže okno Minigraf, export i MCP `plotter` fungují
  beze změny.
- **Po zapnutí plotter pípá a čeká na READY** (počáteční inicializace podle článku). Emulátor to nepřeskakuje.
- **Řídicí počítač je fronta bajtů** v okně Minigraf (soubor, ukázka, řádek povelů s `\xNN`). Handshake RDP/RDA
  je jako na kabelu, takže plotter přijímá tempem programu (buffer 256 znaků).
- **Celý test** (tlačítko): TEST podržený při resetu JPR-1, protože v GUI nejde stihnout stisk při zapnutí. Reset
  vynuluje výstupy, motory skočí do fáze 0 a pár kroků se ztratí. Je to věrné, a tak to zůstává.

## 2026-10-10 – rozložení oken

- **Zobrazení → Výchozí rozložení oken** (autor: „může na to narazit kdokoli“): okno přetažené ke kraji se
  zadokuje (ImGui docking) a pak ho řídí dok, ne zoom. Položka odlepí všechna okna, vrátí je na místa a velikosti
  z prvního spuštění a obrazovku přepne na přizpůsobení oknu. Volba zoomu (menu i pravé tlačítko) zadokované okno
  sama odtrhne, aby zoom fungoval.
- **Sestava… je v menu Zobrazení** (autor: je to zapnutí a vypnutí okna, nic jiného nekonfiguruje), ne ve Stroji.

## 2026-10-10 – výměnná média a sestava

- **Výměnná média jsou parametry „platí hned“** (autor: soubor vložený v okně Média se v konfigurátoru neobjevil):
  karta pojmenuje parametr média jednotky (`Card::media_param`) a umí ho vyměnit (`media_change`), výchozí
  implementace pokrývá diskety všech řadičů. Okno Média i konfigurátor mění média přes `Machine::set_live_param`,
  takže popis sestavy drží krok. Výměna média tak sestavu označí jako neuloženou. Tak to na stroji opravdu je
  (v mechanice je jiné médium), a kdo chce sestavu s původním médiem, změnu zahodí.
- **Program udělaný v okně Média do kazety** (HEX → kazeta MIKOS, KZD-1) zapíše do sestavy zdrojový soubor
  (DSM-1: `.hex`, ze kterého se kazeta při zapnutí udělá znovu) nebo vytvořenou kazetu (`work/kzd-1/…`).
- **Vysunout bez uložení a Obnovit z originálu** (autor si omylem přepsal ukázkovou pásku): `media_discard` zapomene
  změny v paměti, obnovení přepíše pracovní kopii ve `work/` originálem z `media/` (jen u médií z `media/`
  s pracovní kopií, s potvrzením). Médium se potom vloží znovu. (Od 2026-10-29 se média ukládají průběžně, Vysunout
  bez uložení tak zahodí jen posledních pár sekund.)
- **Nová… v konfigurátoru jen u médií, kde nový soubor = prázdné médium** (páska, kazeta RKD-1, kazeta KZD-1;
  `CardParam.min = PF_NEW_MEDIUM`). Diskety potřebují zvolit formát, ty dál zakládá okno Média.
- **Souborové dialogy:** složka dostane koncový oddělovač (`dialog_location`), soubor nabízený k uložení zůstává.

## 2026-10-11 – změny podle měření na skutečném HW

- **Nejdřív návrh, pak implementace** (autor: „podezřelé věci prosím bezhlavě neimplementuj, ale nejdřív to spolu
  probereme“). Každá změna i potvrzení beze změny se zapisuje do `docs/zmeny-podle-hw.md` s doklady a programem,
  kterým jde měření zopakovat (`tools/hwbench/`). Měří se na 125 MS/s, podezřelé hrany i analogově.
- **DSM-1V MR do prvního /MR:** emulováno podle schématu a měření. Do uloženého stavu se nedává: trvá jen do
  prvního čtení RAM a nové pole by znečitelnilo uložené stavy sestav V (stav má jen jednu verzi formátu).
  Původní DSM-1 a DMR-1 bez změny, dokud se neprojdou jejich schémata.
- **Chování desek podle HW jde vypnout** (autor 2026-10-14): parametry procesorové karty `open_bus` (JPR-1V,
  JPR-1ZI, JPR-1Z: poslední bajt, nebo FFh) a `ststb` (JPR-1V: jako deska se zamrzáním, nebo záplata IC15).
  Výchozí je vždy chování desky. JPR-1ZI a JPR-1Z vrací poslední bajt **předběžně** podle autorovy paměti (disky
  XYXY bez IDE), otázka zůstává v Nejasnostech k měření. Sestavy CP/M s ECMONem bez řadiče disket proto startují
  volbou 3 (návody, testy).
- **AND-1Z/89 čeká do konce okna CPU se změřeným znakovým taktem 600,2 ns** (autor schválil 2026-10-13): stejný
  model jako CGA-1V. Takt je změřený na autorově desce, ne kulatých 600 ns: s nimi se okno v emulaci zamklo na takt
  CPU (600 a 500 ns jsou soudělné) a LDIR při 2 MHz čekal o 291 taktů déle než deska, kde jsou krystaly 10 MHz
  a 8 MHz nezávislé.
- **INTA bez vektoru dává v emulátoru FFh** (autor 2026-10-13): na desce JPR-1V Z80 přečte bajt, který je zrovna na
  datové sběrnici, a ten se za 1–2 µs rozplyne k FFh podle bitů a karet (`docs/zmeny-podle-hw.md`). Analogový jev
  se neemuluje, správný program v IM 2 vektor od karty dostane.
- **CMOS, nebo NMOS Z80 je parametr procesorové karty** (`z80`, autor 2026-10-13): JPR-1V má výchozí CMOS (na
  autorově desce Z84C0020PEG), JPR-1ZI a JPR-1Z NMOS. CMOS nepřijme přerušení hned po `LD A,I` / `LD A,R`
  (změřeno). Úprava je v jádru `third_party/chips/z80.h` (příznak `cmos` ve výplni struktury, takže uložené stavy
  platí dál; karta ho po každém resetu a načtení stavu nastaví z konfigurace). NMI se neměří, beze změny.
- **Zamrzání FXS4 se emuluje** (autor schválil po měření na desce): JPR-1V dává STSTB i při INTA, karta s oknem
  (CGA-1V, AND-1V, DGD-1V) pak drží RDY napořád a RESET nezabere, jako na železe. Kdo to nechce, vypne kartě
  čekací stavy (`wait_states`), což je zřetelná odchylka od desky. Do uloženého stavu se zaseknutí nedává.
- **Emulátor podle stroje bez měřicí karty** (autor schválil): karta, přes kterou je analyzátor připojený,
  zatíží sběrnici a čtení z CGA-1V při 4 MHz posune z 22,52 na 22,76 T na bajt (čtení leží na hraně okna).
  Žádost při čtení je proto 63 ns (měření časovačem bez analyzátoru), ne 72 ns ze sond. Jiné karty v rámu
  posunou čtení podobně, emulátor drží sestavu autora.
- **Prázdná sběrnice na JPR-1V vrací poslední bajt** (autor schválil, potvrdil i chováním CP/M bez IDE-1).
  Emulace je věrná i s důsledkem, že ECMON na sestavách V volbami 0–2 bez řadiče visí; návody a testy proto
  volí 3. Ostatní procesorové karty zůstávají u FFh, dokud se nezměří.

## 2026-10-10 – DPS-1 a měřicí pracoviště IMS-2

- **Pracoviště je „technologie mimo počítač“** (`plant` typu `IMS-2`) jako pece APS-5: sběrnice s přístroji
  nepatří do skříně SAPI, karta DPS-1 se k ní dostane přes `Bus::plant` (kabel X2). Sestava má nejvýš jedno
  pracoviště.
- **Sběrnice po vodičích**, ne po zprávách: DPS-1 i přístroje jsou uzly, které budí své vodiče (wired-OR),
  handshake DAV, NRFD, NDAC běží jako na kabelu s časy. Software tak vidí stejné stavy jako na železe, včetně
  podivností desky (bajt 00h s ATN, chyba BASIMS bez posluchače).
- **Deska podle schématu (netlist repliky EC1045 ověřený proti skenu Tesly), s jedinou výjimkou podle návodu:**
  vlastní NDAC desky blokuje LIST/TALK (PC7 D2C). Čistě podle schématu by deska při vysílání držela NDAC sama
  a BASIMS by se zasekl (ověřeno). Návod i BASIMS říkají, že PC7 vlastní NDAC blokuje. Nejasnost k ověření.
- **Voltmetr Metra M1T 330:** jediný dobový přístroj s IMS-2, ke kterému máme příkazy (skripta VUT) i příklad
  s odpovědí (návod BASIMS). Vzhled podle fotografií. Odhady (CR LF s EOI, Q, přetečení, časy) jsou v Nejasnostech.
- **Zdroj Tesla BS 525 bez sběrnice** (autor vybral z nabídky: BS 525 / BK 127 / vymyšlený programovatelný zdroj):
  dobový zdroj s IMS-2 se nenašel, vymyšlený by nebyl věrný. BS 525 se ovládá myší, voltmetr měří jeho výstup.
- **Sestava ZPS-2 s MIKOSem V4.0F a kazetou:** BASIMS volá podprogramy MIKOSu V4.0F, pod MIKOSem/D V5 (kde je
  BASIMS4.SYS na disketě) by neběžel.
- **Ukázkové programy na kazetě za BASIMS** (autor: „Šlo by ten program v BASIC uložit na kazetu, aby se mohl
  nahrát za tím BASICem samotným?“): kazetu nahrál originální MIKOS V4.0F a BASIMS v emulátoru
  (`tools/dps1_kazeta.py`), ne generátor kazet, aby byla jako ze skutečného stroje. Formát WAV 22050 Hz, 8 bitů
  (1,3 MB): MP3 rozbilo předěl mezi nahrávkami a druhý soubor se nenačetl.
- **Konektory na panelech podle fotografií desek** (autor: DPS-1 má jeden FRB 30, DPP-1 dva): DPS-1 FRB nahoře
  (K3 = X2), DPP-1 X2 nahoře (K3), X3 dole (K2), jako u RMP-1.

## 2026-10-10 – fokus obrazovky po spuštění

- **Obrazovka dostane klávesnici, ale nepřekryje ostatní okna.** Fokus v ImGui okno zároveň vytáhne dopředu.
  U sestavy plotteru z AR 1990 (bez zobrazovací karty) tak prázdné okno obrazovky zakrylo okno Minigraf
  s tlačítkem READY a plotter jen pípal (autor: „jen drnčí a nenaběhne“). Okna, která byla nad obrazovkou
  (otevřená se sestavou: Minigraf, Média…), se po fokusu vrátí dopředu.
- **Důležitá okna určuje sestava** (autor: „u některých sestav byly důležitá okna nahoře“, Minigraf zůstal za
  oknem Sestava). Klíč `windows` v sestavě, třeba `["minigraf"]`, a zaškrtávátka v konfigurátoru. Po spuštění se
  okna otevřou, dají nahoru (v doku vybraná záložka) a klávesnice zůstane obrazovce. Autor zvolil seznam
  v sestavě místo automatiky podle karet: sestava ví, co je pro ni hlavní (Média u pásky, plotter bez
  obrazovky). Okna Minigraf a APS-5 se dál otevírají samy podle karet, jen bez dání nahoru.

## 2026-10-10 – opravená kopie MTCOPY

- **MTCOPYO = původní kód MTCOPY + novější knihovna MTKAN** (autor: „udělat novou opravenou verzi“). Chyba
  v počtu bloků EOF1 není v MTCOPY samotném, ale ve starší MTKAN, se kterou je slinkovaný. Novější MTKAN V060588JB
  je na téže disketě a má ji i MTDAT. Skript `tools/mtcopy_oprava.py` ji přeloží původním M80 a L80 (`cpmrun`) na
  3D70h, kde stará knihovna začínala. Kód MTCOPY 0100h–3D6Fh zůstává bajt po bajtu, jen s nadpisem `(OPRAVENY)`.
  Nepřekládá se zpětně získaný `MTCOPY.MAC`, protože nedává stejné bajty jako MTCOPY.COM. Ruční záplata starší
  knihovny by zase byla jen napodobenina toho, co J. Bíba v knihovně opravil sám.
- **Opravená disketa má MTCOPYO místo MTCOPY** (na originálu jsou volné jen 2 KB). Originál zůstává
  v `a_lad_izo.img` a sestava `rmp1_jb_izot.sapi` ho dál používá. Opravená kopie se vkládá v okně Média, stejně
  jako `CKM_opraveny.hex` stojí vedle originálu CKM.

## 2026-10-10 – zasekávání FXS4 (zjištění L. Lasoty)

- **Chyby JPR-1Z/1V se zatím neemulují** (autor vybral zápis do dokumentace a zkoušku v emulátoru, ne emulaci):
  výběr grafické karty při INTA (STSTB i od /IORQ, RDY se neuvolní) a RESET jen s /M1. Program, který zabloudí
  nad BFFFh s povoleným přerušením, tak v SAPIemu běží dál, na železe se zasekne. Kdyby se emulace dělala, pak
  přepínatelně podle revize (originál / Liborova záplata IC15).
- **Nejdřív příčina poškození paměti, pak emulace** (autor: „nemá smysl implementovat Liborovi opravy, když to
  v emulátoru netuhne“). Zásek je až poslední článek: na železe se nejdřív přepisuje paměť (font, možná zásobník
  nebo kód), program pak skočí nad BFFFh a teprve přerušení zablokuje RDY. V emulátoru se paměť nepřepisuje,
  takže by INTA a RESET s /M1 nic nezměnily. Postup: výpis paměti ze železa (po RESETu, dokud ještě zabere),
  z adres a hodnot poškozených bajtů odvodit mechanismus (třeba zápis do CGA, který dopadne i do RAM se ztraceným
  A14/A15, nebo adresa posunutá o bit; emulátor umí porovnat se zápisy programu), ten nasimulovat jako vadu
  a teprve pak doplnit INTA, RESET s /M1 a záplatu.

## 2026-10-16 – DPI-1L jen se síťovou částí

- **Z DPI-1L jen Repository API a NET** (autor: „jen její síťovou část … Emulaci karet, kterou dělá skutečná DPI,
  nepřidávej“). Karty, které DPI emuluje na sběrnici (DPP-1, IDE-1, RAM disk), SAPIemu má samo. Soubory nad složkou
  PC (příkazy 20–56h, zadání §7) jsou volitelná druhá část, zatím nejsou. Zadání je v
  `..\SAPI_DPI-firmware\docs\sapiemu_dpi.md`, popis v `docs/desky/DPI-1L.md`.
- **Vzor je ověřený firmware DPI:** jeden program má běžet stejně v emulátoru i na kartě. Kde se firmware v0.2 lišil
  od zadání (stav po /RES, zrušené CREATE místnost přesto otevřelo, kód JOIN delší než 7 znaků), šly rozdíly
  autorovi jako otázky. Autor je probral se session firmwaru: firmware se opravil podle zadání (`b72a8d0`) a emulátor
  s ním (`docs/desky/DPI-1L.md`, zadání §9).
- **Síť ve vlastním vlákně, emulace jednovláknová.** Vlákno spojení řádky od relay jen řadí do fronty bez omezení
  délky, karta je zpracuje při nejbližším přístupu Z80 na své porty. Z80 jiný rozdíl nepozná a nemůže se ztratit
  řádek (chyba MZPico s frontou 10 řádků). Vlákno si drží vlastní odkaz na spojení, nikdo na něj nečeká, takže
  zavření spojení nebrzdí emulaci.
- **WebSocket vlastní, jako ve firmwaru**, ne z `httplib`: pár desítek řádků a stejné chování (Host bez portu,
  ping → pong, bez ověření Accept). Limit 10 s pro CREATE a JOIN se měří časem PC, relay běží v reálném čase.
- **Sestava `sapi1v_BomberNet.sapi` na ARB-1C (14 pozic)**, ne vyndáním karty ze `sapi1v`: devět karet se na ARB-1C-8
  nevejde a všechny programy sestavy V tak běží beze změny.
- **Release 0.6.0-alpha pro SAPI-BomberNet:** ostatní projekty smějí spouštět jen release SAPIemu
  (`..\SAPIemu-release`), ne vývojový strom.

## 2026-10-16 – DPI-1L: SD karta nad složkou PC

- **Repository API celé, soubory nad složkou PC** (autor: „zbytek MZFRepo do emulace DPI … směřoval do složky
  sdcard“, pro vývoj softwaru pro SAPI, který s DPI pracuje). Parametr `sd.folder` (výchozí `work/dpi-1l/sdcard`),
  složka ve `media/` přes pracovní kopii jako PMD32-SD. TCP a DNS (80–89h) jako druhý krok.
- **Vzorem je firmware DPI**, `repo_fs.c` a `koi8.c` převzaté co nejvěrněji, i s chováním FatFs, které program vidí
  (výsledky FR_*, velikost v adresáři až po SYNC/CLOSE, LOCKED u otevřeného souboru). Kde to věrně nejde (pořadí
  položek, čísla krátkých jmen, čas souborů), je to v `docs/desky/DPI-1L.md` a čeká na záznamy ze skutečné karty.
- **Verze jako skutečná karta** (autor 2026-10-20: „uprav verzi v SAPIemu“): REV a REVD hlásí verzi firmwaru,
  který má skutečná DPI-1L (od 2026-10-22 0.4.1, od 2026-10-20 0.4.0, REV i se třetím číslem `DPI-1L v0.4.1`). Protokol se od 0.2
  nezměnil, takže chování zůstává podle v0.2.
- **Stránka 4 KB věrně** (autor souhlasil): program, který zapomene CLOSE, přijde o data jako na železe. Odchylka
  `sd.flush_on_exit` (výchozí vypnutá) zapíše stránku při vypnutí emulátoru.
- **Uložený stav drží otevřený soubor** i s nezapsanou stránkou a CWD (autor souhlasil). Dřív se karta po načtení
  chovala jako po RESET; síťové spojení se uložit nedá dál, místnost se opustí.
- **Velikost karty parametrem** `sd.sectors` (sektory jako GETFREE), výchozí autorova SanDisk 32 GB (62 309 376,
  podle záznamů firmwaru; původně `sd.size_mb` 32 podle „32 MB“). Skutečný disk PC by dal nesmyslné GETFREE. Plná
  karta vrátí FR_DENIED jako `flush_page` ve firmwaru.
- **Podle záznamů ze skutečné karty** (`sapiemu_dpi_repo.md`, 2026-10-16): pořadí položek jako na FAT (pořadí
  vytvoření, emulátor si pamatuje, co vytvořil, a na Windows posune čas vytvoření), krátká jména přesně podle FatFs
  s kódovou stránkou 437, a opravy firmwaru `f3e583b` (RESET zavře TCP, DNSREQ vynuluje IP, chyba odmítnutých
  parametrů zůstane).
- **`/DPI/DPI.INI` se založí a GETCONFIG/SETCONFIG s ním pracují, emulátor se jím neřídí** (autor souhlasil): relay
  a port jsou parametry karty. SECRET.INI a starý `/DPI.INI` skryté jako na kartě.
- **BOOT jako /RTL 100 ms, RTC podle hodin PC s posunem, SDCARD bez USB = chyba 5** (autor souhlasil s BOOT; zbytek
  podle firmwaru).
- **TCP a DNS ze sítě PC** (autor: „určitě taky prosím implementovat“): jedno spojení ve vlastním vlákně, fronta
  16 KB s řízením toku přes okno TCP, limity 10 s a 5 s časem PC, RESET spojení nezavře, cíl na začátku parametrem
  `net.tcp_default`. Program na SAPI tak může mluvit s libovolným serverem, na který dosáhne PC.

## 2026-10-17 – grafická jednotka XY 4130

Autor body níže potvrdil („souhlas“, 2026-10-17).

- **Podle řady XY 41x0** (autor: „Zkusil bych ten plotter XY 4130 … návod … by mohlo lecčemus pomoci“): pro XY 4130
  samotný podklady nejsou, rozhraní a chování jsou z návodů XY 4131 a XY 4140 (XY 4140 je „funkčně identická“ s XY
  4131), ze schématu XY 4140, z dokumentace XY 4150 a z projektu plottercontroller, který s XY 41xx skutečně kreslí.
  Že stejné rozhraní má i XY 4130, potvrzují oba ovladače pro SAPI (signály a jejich význam sedí).
- **READY a PEN jako ve schématu XY 4140** (výchozí podle HW): pero se spustí jen po dojetí vozíku na levý doraz a
  po stisku PEN. Ovladač z EPROM i program s `KRES #I -220,0,,` na doraz dojedou samy, PEN musí stisknout obsluha.
  Odchylka `xy4130.pen_on` stiskne PEN už při zapnutí. Jestli to tak měla i XY 4130, je v Nejasnostech.
- **Směr papíru odvozený ze softwaru:** +/− = L u papíru posune pero po listu nahoru. Jen tak oba ovladače píšou
  nezrcadleně (u vozíku směr potvrzuje plottercontroller: k dorazu je +/− = L).
- **Dráha vozíku 190 mm od 10 mm papíru je odhad** (plocha 180 mm, počátek ovladače 8 mm za dorazem). Papír je pruh
  bez dorazů jako u Minigrafu.
- **Společná mechanika `chips/plotter`:** kresba, export, okno a MCP jsou pro Minigraf i XY 4130 stejné, liší se jen
  dekódování kabelu. Okno si ponechalo ID `Minigraf` (uložené rozložení), titulek ukazuje typ zapisovače. MCP
  `plotter` hlásí `steps_too_fast` a `max_steps_per_s` místo `steps_over_400_per_s`.
- **Kabely dva** (`mikos`, `kres`): sapi.cz u ovladače v EPROM a návod KRESu uvádějí jiná zapojení; zapisovač jde na
  JPR-1 X3 i na DPP-1. READY je zapojený jen v kabelu KRESu (P2-IN5, PB5), na JPR-1 by šel do žádosti o přerušení R5
  jako každý vstup P2-IN.

## 2026-10-17 – výběr sestav a průvodci

- **Po prvním spuštění se otevře výběr sestav** (autor: „po první spuštění by měl být nějaký výběr sestav“). Dřív
  se otevíral konfigurátor za obrazovkou (0.5.0), teď je dostupný ze Zobrazení a z výběru (Otevřít v konfigurátoru).
- **Rejstřík `machines/_vyber.json` místo nových polí v `.sapi`:** kategorie, pořadí a texty pro nováčky jsou na
  jednom místě, sestavy zůstávají beze změny a konfigurátor je nemusí znát. Náhledy a průvodci se hledají podle
  jména souboru (`machines/previews`, `machines/guides`). Vlastní sestavy se ukážou v „Nezařazené a vlastní“.
- **Průvodce se otevře sám jen u Základu, Liborových desek a Her a dem**, jde vypnout u sestavy i celkově a
  zkušené neobtěžuje (autor: „snadno skrytelné a neotravující pro zkušenější uživatele“).
- **Soubor → Otevřít sestavu** (plochý seznam 60 cest) nahradilo okno výběru; „Otevřít soubor…“ se jmenuje
  „Otevřít soubor sestavy…“. MCP `list_machines` vrací cesty jako dřív a navíc `entries` s texty z rejstříku.
- **Sestava „SAPI-1 V: hry a dema“** (`sapi1v_hry`) má CGA-1V i DGD-1V přepínané přes MAP2 a jeden RAM disk z
  `tools/make_hry_disk.py`. `sapi1v_BadApple` a `sapi1v_DGD_Flapy` nahradila a autor je smazal. Hudba VGM i RAD
  zůstala (autor), „-LAILA-“ je L. Lasota.

## 2026-10-17 – úklid ovládání (etapa 4 vydání)

- **Server MCP v GUI podle Nastavení** (autor: ve vydání vypnutý, zapnout v Nastavení). Nová instalace ho má vypnutý.
  Soubor nastavení starší verze bez položky `mcp` ho nechá zapnutý jako dřív, aby sessions s Claude běžely dál.
  `--mcp-port` ho zapne vždy, `--no-mcp` ne. Zapnutí a vypnutí v okně Nastavení platí hned.
- **Zkratky emulátoru se při psaní do textového pole neuplatní** (dřív F9/F12 resetovaly stroj i z políčka
  konfigurátoru). Všechny zkratky jsou v jedné tabulce v okně Nápověda → Klávesové zkratky; Ctrl+písmeno, Esc, F1–F4,
  F6 a F7 zůstávají klávesnicím SAPI.
- **Menu:** Soubor má Poslední sestavy; Ladění má Log a Krok jen při pauze; Zobrazení má Nastavení… a okna přístrojů,
  které sestava nemá, jsou nedostupná; Nápověda místo řádků textu otevírá Průvodce sestavou, První kroky, Návody,
  Klávesové zkratky a O programu. Hlasitost a citlivost myši se přestěhovaly do Nastavení.
- **Nástrojová okna** (debugger, paměť, I/O log, terminál, Média, tiskárna, Log) se poprvé otevřou u pravého okraje,
  ne přes obrazovku. Uložené rozložení (`imgui.ini`) má přednost.
- **Průvodce se sám otevře jen po spuštění sestavy** z okna Vybrat sestavu nebo z menu, ne při každém startu programu
  s poslední sestavou.


## 2026-10-17 – příprava vydání (etapa 5)

- **Vývojářské poznámky zůstávají v `docs/`**, do veřejného zipu nejdou (`package.py --public`, seznam `DEV_DOCS`:
  `SOUHRN.md`, `rozhodnuti.md`, `docs/vydani/`). Plán počítal s přesunem do `docs/dev/`, to by ale rozbilo odkazy
  v CLAUDE.md, README a dokumentaci a ve veřejném repu by byly stejně vidět. Otázky na HW (`hw-otazky*.md`) a změny
  podle HW se balí dál, můžou zajímat lidi se skutečným SAPI.
- **`work/` mimo složku programu:** když do `work/` ve složce dat nejde zapisovat (zip rozbalený do `Program Files`),
  pracovní kopie, soubory s bateriemi a výstupy tiskáren jdou do `%LOCALAPPDATA%\SAPIemu\work` (Linux
  `$XDG_DATA_HOME` nebo `~/.local/share/SAPIemu/work`). Cesty v sestavách zůstávají `work/...`, mapuje je
  `data_path` a zpět `data_relative`. Nastavení už dřív šlo do `%APPDATA%` (SDL pref path).
- **Log do souboru:** GUI píše `sapiemu.log` vedle nastavení, při každém spuštění znovu (pro hlášení chyb).
- **Chyba při startu** (SDL, chybějící `roms/` a `machines/`) se ukáže v okně, ne jen na stderr, které spuštění
  z Průzkumníka nevidí.
- **Ikona (autor: „z toho udělej ikonu“):** jednička z loga SAPI 1 z příruček Tesla, překreslená podle fotky loga.
  Je to dráha souběžných linek kolem kostry: ostrá špička, vlajka se zaobleným koncem, patka asi pětkrát širší
  než dřík se zaoblenými konci, ve středu jedna linka. Bílá na černém zaobleném čtverci. Ve 24 a 32 px má jen obrys
  a středovou linku, v 16 px je plná. Kreslí ji `tools/make_icon.py`. Původní návrh (zelené „SAPI“ a oranžové „-1“)
  autor nechtěl.
- **Verze 0.7.0-alpha.** O „beta“ nebo „1.0“ rozhodne autor před vydáním.
- **Logo „SAPI 1“** (`docs/img/logo.png`, `tools/make_icon.py`) je nahoře v README a v O programu.

## 2026-10-18 – O programu a karty ve výběru sestav

- **O programu bez obrázku skříně:** obrázek byl Liborova skříň, ne původní SAPI-1, a logo grafice stačí (autor).
  Později možná menší obrázek původního šasi. Název a verze jsou v pravém horním rohu vedle loga. Text podle autora:
  osmibitový mikropočítač, navrhl Ing. Eduard Smutný, vyráběla Tesla Liberec, původní desky a sestavy ZPS i desky
  Libora Lasoty a Petra Všijanského, přístroje. Přibyl odkaz pmd85.cz.
- **Výběr sestav ukazuje karty a příslušenství** (autor: „zobrazit i jména karet + příslušenství“): seznam
  `machine_cfg_parts()` vychází z katalogu karet. Příslušenství jsou klávesnice (a joystick ANK-1M2), co je v K2/K3,
  oddíly karet, které jsou zařízením (tabulka `k_device_sections` a mechaniky a jednotky), magnetofon DSM-1
  a technologie mimo počítač. U médií je jméno obrazu, u tiskárny ne (výstupní soubor na PC médium není). Kanály
  sériových karet a strana PC se neukazují.

## 2026-10-28 – první sestava GUI

- **GUI bez nastavení spustí ZPS-1 s MIKROBASICem** (`machines/zps/ZPS-1.sapi`) místo MikroMonu (autor: „jako
  základní sestavu udělat fakt tu nejjednodušší s MIKROBASIC“, má být vybraná ve výběru sestav). Je první ze
  Základu, výběr sestav vybírá běžící sestavu. `sapiemu-cli` bez `--machine` (headless, MCP) a záloha GUI, když se
  sestava nenačte, zůstávají u `mikromon.sapi`: na tom stojí návody k MCP a testy.
- **Odkazy v Průvodci sestavou** jsou tlačítka pod odstavcem, zarovnaná s jeho textem a zalomená podle šířky
  okna. Dřív stály za posledním řádkem odstavce a u dlouhého textu utekly za okraj.
- **MikroMon 1.3 patří do Desek Libora Lasoty** (autor), ne do Základu, který jsou jen historické sestavy Tesla.
- **Běžící sestava je ve výběru zelená** s nápovědou „Právě běží“. Šipka před názvem se pletla se šipkami
  rozbalovacích nadpisů (autor).
- **ID položek GUI ve smyčkách** musí být jedinečná (`PushID` nebo přípona `##`), jinak ImGui (1.92, 
  `ConfigDebugHighlightIdConflicts`) ukáže chybové okno „conflicting ID“. Projevilo se u sestavy ve dvou skupinách
  výběru (JPR-1S a BGP-1S). Při prohledání frontendu se našla a opravila i Zobrazení → Barva se dvěma displeji
  (AND-1V a DGD-1V), PMD32-SD na dvou kartách v okně Média a kanály se stejným jménem v Sériovém terminálu.

## 2026-10-28 – aktuální verze her v sestavách

- **Soupis (2026-10-28):** Bad Apple!! 1.0.0, FXS4 1.0.2, Flappy 1.0.0 a Bomberman 1.0.1 byly ve vydávaných
  sestavách (`hry-a-dema.bin`, `ba-ramdisk.bin`) už aktuální, bajt po bajtu shodné s buildy repozitářů. Zastaralý
  byl Prince of Persia (1.0.0 převedená z Apple) a v sestavě `sapi1v_BomberNet` hra chyběla.
- **Prince of Persia s grafikou a hudbou DOS verze** (větev `dos-graphics`, `build\ramdisk_dos.bin`) je v Hrách
  a demech a **v betě pro testery** (autor: „dej to i do vydání, tady bych se toho nebál“; repo je zatím soukromé).
  Do veřejného vydání „to možná nedáme“: autor se zeptá kolegů, podklad je průzkum webu o portech PoP a právech.
  Do té doby bylo `REPO_ONLY`, protože data hry jsou Broderbundu.
- **PoP zůstává i ve veřejném vydání** (autor, po průzkumu `docs/vydani/pop-prava.md`): je to ukázka historické
  technologie staré 40 let na emulátoru velmi specifického, většinou průmyslového počítače, autor nečeká velké
  rozšíření ani dopady. Ke hře patří upozornění pro nositele práv (průvodce, popis sestavy, výběr sestav, README):
  neoficiální nekomerční port, kdo drží práva, licence SAPIemu se na data hry nevztahuje, na žádost cokoli upravíme
  nebo odstraníme. Historie gitu se nepřepisuje. Repo SAPI-Prince-of-Persia zůstane bez hry v binární podobě.
- **Starý kombinovaný disk** `ramdisk_pop-fxs4-flappy-bomberman-bkp.bin` (PoP 1.0.0, Bomberman 1.0.0) smazaný.
- **Mimo git aktualizované (autor):** IDE disk `work/ide/sapi_hdd.img` (C: BOMBER 1.0.1, FXS4 1.0.2; jde do alfa
  zipů bez `--public`), FXS4 1.0.2 v autorově pracovní kopii `work/zrmd-1/SAPI_CPM_Martin_2019-10-07.bin`. Pracovní
  kopie `work/zrmd-1/pop-ramdisk.bin` nahrazená novým obrazem (emulátor ji z `media/` sám neobnoví).
- **BomberNet:** nejdřív port SAPI-BomberNet na BomberNet 0.3.0 (autor), pak herní sestava s vlastním RAM diskem
  v Hrách a demech a ve vydání. Port je hotový v repu SAPI-BomberNet (submodul 9961800, BUILD_ID beze změny, hraje i
  s 0.2.0; po kontrole opravené staré ESC a klávesa v kódu místnosti), `BOMBNET.COM` 0.3.0 MD5 800a0d8f….
- **Sestava `sapi1v_BomberNet` je herní** (autor: „Herní, i ve vydání“, souhlas M. Matyáše má): ARB-1C-8 bez IDE-1,
  DPI-1L v X6, RAM disk `media/zrmd-1/bombernet.bin` jen s `BOMBNET.COM` (`tools/make_hry_disk.py --bombernet`).
  Nahrazuje rozhodnutí z 2026-10-16 o vývojové sestavě na ARB-1C se 14 pozicemi: plná sestava V s DPI-1L se dá
  složit v konfigurátoru, ve výběru sestav má být hra. RAM disk se neukládá (`nosave`), aby si dvě instance na
  jednom PC, které spolu hrají, nepřepisovaly pracovní kopii; hra na disk nic nezapisuje.
- **BomberNet 0.3.0 ověřený autorem** (2026-10-28 večer): na skutečném SAPI-1 (A: na ZRMD-1, sestava s DPI-1,
  „funguje to jako víno“) i v GUI vývojového SAPIemu se sestavou `sapi1v_BomberNet` („funguje parádně“).

## 2026-10-28 – plán nového vzhledu GUI

Plán po etapách: `docs/vydani/plan-vzhled.md`, podle hodnocení `docs/vydani/ux-hodnoceni.md`, se kterým autor
v zásadě souhlasí („pustil bych se do úprav“).

- **SAPI-1 byl průmyslový řídicí počítač v rámu, ne počítač pro lidi** (autor). Pro uživatele emulátoru jsou
  hlavní základní sestavy ZPS. Lidi, kteří SAPI neznají, přitáhnou přístroje (kotelna APS-5, měřák IMS-2 se
  zdrojem, zapisovače, KZD-1, RKD-1, RMP-1). Výběr sestav to řekne v úvodu.
- **Hry na SAPI nikdy nebyly** (autor). Libor Lasota udělal moderní karty CGA-1V a MPH-1V a dema, autor převedl
  Flappy, Bad Apple, PoP a FXS4 a udělal DPI-1L s BomberNetem. Hry jsou zábavný novodobý doplněk, skupina „Začněte
  zde“ s PoP z hodnocení proto odpadá.
- **Kategorie výběru sestav:** Základ → Přístroje a technologie → Desky Libora Lasoty → Hry a dema → Ostatní.
  XY 4130 a AR 1990 přejdou do přístrojů s průvodci, RKD-1 s CM 5400 zůstane v Ostatních, Manic Miner
  (`jpr1s_bgp1s`) bude v Liborových deskách i v hrách.
- **Rám v konfigurátoru je záměrná odchylka** (autor): kreslí se podle Liborovy modernizované skříně s popsanými
  panely. Původní Tesla měla holé karty za krycím panelem (http://www.osmibitovemuzeum.cz/fotogalerie/sapi-1_2/1.jpg),
  což je nezajímavé. V etapě 2 zapsané do tooltipu Typů karet na panelech, README, Prvních kroků a `build.md`.
- **Vzhled jako aplikace Windows, světlý i tmavý** (autor: výchozí vzhled ImGui je „přinejlepším nudný“). Tři
  varianty (Windows 11, Windows klasický, SAPI průmyslový) se ukážou na skutečných snímcích emulátoru v galerii
  a autor vybere, ladit se dá i za chodu.
- **Beta pro Libora, Petra a Michala až s novým vzhledem** (autor), zip až na jeho pokyn.
- **Docking ani F9 = reset nejsou dogma** (autor): výchozí zadokované rozložení je v pořádku, když ho většina
  uživatelů preferuje a GUI s ním bude hezké; F9 vznikla kvůli ladění. Plán proto porovná plovoucí a zadokované
  rozložení a navrhne zkratky ladění jako ve Visual Studiu (F9 = breakpoint, reset jen F12).
- **Linux ve WSL nemusí být zelený** (autor: „prostě co má smysl“): pouští se tam, kde to má smysl (kód závislý na
  platformě, písma), a není podmínkou commitu ani pushe.

## 2026-10-28 – vzhled GUI: Windows 11

- **Jen Windows 11, světlý i tmavý** (autor po galerii šesti vzhledů: „Jdeme do Windows 11, ostatní vzhledy zatím
  zruš, ať to ještě nezesložiťujeme“). Režim se řídí Windows, jde přepnout v Nastavení → Vzhled.
- **Mechanismus pro víc vzhledů zůstává** (autor: kdyby byla poptávka, nastavování víc vzhledů se může vrátit).
  Vzhled jsou data v `src/frontend/theme_data.cpp`. Palety Windows klasický a SAPI průmyslový jsou v commitu
  `1fc9a21` a v příloze A plánu `docs/vydani/plan-vzhled.md`.
- **Logo SAPI 1 v O programu** (autor): ve světlém režimu inverzní, tmavé písmo na světlém okně. V tmavém je černý
  obdélník srovnaný s pozadím okna, tedy bílé písmo na barvě okna. Kreslí se maska `docs/img/logo-alpha.png` (z
  `tools/make_icon.py`) v barvě textu; `logo.png` s černým pruhem jako v příručkách zůstává v README.

## 2026-10-28 – výběr sestav podle historie SAPI (etapa 2 plánu vzhledu)

- **Kategorie** podle autora: Základ → Přístroje a technologie → Desky Libora Lasoty (moderní) → Hry a dema
  (novodobé) → Ostatní, nad seznamem úvod o průmyslovém SAPI-1, texty skupin pod nadpisy. XY 4130 a plotter z AR 1990
  mají průvodce. Manic Miner je v Hrách a demech pod vlastním titulkem (`titles` kategorie).
- **Enter v okně Vybrat sestavu spustí jen sestavu, která neběží** (oponentura): běžící sestavu restartuje jen
  tlačítko Restartovat myší, jinak by jedna klávesa zahodila paměť (třeba napsaný program). Při prvním spuštění
  ZPS-1 nikdo nespustil: tlačítko je Spustit a Enter funguje.
- **Nápověda → Průvodce sestavou průvodce otevře a přinese dopředu** (dřív přepínal): okna přístrojů ho při prvním
  použití překrývají (stejné místo vpravo nahoře), řeší etapa 5. Zavírá se křížkem.
- **Barvy:** autor chce „malinko víc používat barvy tam, kde to má smysl“. Nadpisy (název sestavy ve výběru,
  nadpisy průvodců, SAPIemu v O programu) mají zase teplou barvu jako dřív: v tmavém režimu původní #FFD973,
  ve světlém tmavší #B05000 (kontrast 5:1). Zelená „ok“ ve světlém režimu je tmavší #0B6A0B, aby byla čitelná i na
  vybraném řádku (test `test_theme` to hlídá).
- **Logo v O programu** se zmenšuje na procesoru průměrem plochy přesně na velikost, ve které se kreslí (autor:
  hrany masky byly „lehce hrubě zubaté“). Okno O programu má pevnou šířku a výšku podle obsahu (autor: zbytečně
  se posouvalo o 2 px).
- **Checkpoint B (autor):** texty schválené, jen první věta úvodu je „SAPI-1 (Tesla, 1983–1990) byl průmyslový
  řídicí počítač do racku.“ (anglicky „was a rack-mounted industrial control computer“). Roky 1983–1990 zůstávají.
- **Tlačítka v průvodcích** (autor: šedá jsou „smutné a málo viditelné“): odkazy na dokumenty jsou odkazy v barvě
  odkazů (podtržené, se šipkou ↗), Napsat do SAPI je modré hlavní tlačítko bloku kódu, Kopírovat zůstává normální.
- **MCP a cli si Claude doplňuje sám** (autor: „cokoli ti bude chybět v MCP nebo cli, tak si dodělej“), třeba
  tlačítka panelu plotteru z AR 1990 pro náhledy a testy.
- **SAPI-1 je v textech rodu mužského** („byl průmyslový řídicí počítač“, „pracoval“), APS-5 je „měřicí jednotka“
  jako v původním návodu Tesly.

## 2026-10-29 – etapa 3 plánu vzhledu: tlačítka zapisovačů, okno Windows, náhledy

- **Tlačítka zapisovačů pro MCP a cli** (autor: „Tlačítka pro MCP a cli vyrob“): jedna společná funkce
  `plotter_action` (`src/core/plotter_ctl`) pro okno, MCP (`plotter`: `ar_button`, `demo`, `send`, `send_file`) i
  cli (`--plotter AKCE`, `--print-plotter`, `--plotter-png`, `--plotter-svg`). `check_machines` u AR 1990 kreslí
  zkoušku (28 tahů), u XY 4130 ukázku (19 tahů). Povely `send` znají jen `\xNN` jako řádek okna.
- **Titulek okna Windows** podle vzhledu: tmavý režim (atribut DWM 20), ve Windows 11 barva titulku = lišta menu
  (35) a text (36), u neaktivního okna tlumený. dwmapi se načítá za běhu: `dwmapi.h` přes `rpcndr.h` definuje makro
  `small`. Po změně barvy zvýraznění Windows (bez přepnutí světlý/tmavý) může SDL do další změny fokusu nastavit
  režim titulku podle systému; projeví se jen u pevného vzhledu jiného než systém.
- **Měřítko podle monitoru okna**, i po přesunu na jiný monitor; okna ImGui drží velikost v pixelech.
  `--ui-scale` má přednost.
- **Náhledy přístrojů** v tmavém vzhledu Windows 11 při 100 % (nezávisí na PC). Nově plotter z AR 1990 (celá
  ukázka) a XY 4130 (znaková sada `G181E`). APS-5 ostřejší a s legendou.
- **Okno APS-5** má výchozí výšku 50 em místo 44: v novém vzhledu se obsah nevešel (posuvník, uříznutá legenda).
- **Cesty médií v okně Média** relativně k datům (`work/…`), celá cesta v tooltipu: náhled RMP-1 ukazoval absolutní
  cestu z PC, kde vznikl.

## 2026-10-29 – etapa 4 plánu vzhledu: ovládání

Etapu udělal workflow (3 agenti, oponentura, opravy nálezů), autor spal a rozhodnutí nechal na Claudovi. Chování
skutečného SAPI-1 se nemění: jde o pohodlnost na straně PC, reset APS-5 zůstává věrný (jen se GUI nejdřív zeptá).
Co má autor ještě posoudit a vyzkoušet ručně, je na konci oddílu.

### Klávesnice, vypínač, stavový řádek

- **Klávesnice se vrací displeji použitému naposledy** (`ui.focus_display`, `focus_display_window` v `app.cpp`) po
  Napsat do SAPI, Spustit/Restartovat, Nahrát program a binární soubor, vložení média z okna Média (i program na
  kazetu), Vložit text z menu a po zavření průvodce nebo výběru sestav. Ostatní okna zůstávají v pořadí, žádné se
  nevytáhne dopředu (průvodce zůstane nad obrazovkou). Obecné vracení po každém použití menu (Reset z menu, Snímek…)
  zatím není, menu si fokus bere dál.
- **Nápis „Klikněte sem a pište“** dole na obrazu displeje použitého naposledy, jen když žádný displej nemá
  klávesnici, sestava má klávesnici (ne plotter AR 1990) a v tomto běhu se do SAPI ještě nic nenapsalo, nebo se
  právě zahodil psaný znak (pak 5 s, po každém zahozeném znaku znovu; doplněno po revizi). Ne trvale, kdykoli
  displej klávesnici nemá: při práci v debuggeru nebo jiném okně by nápis jen zakrýval obraz. Kreslí se
  do okna, ne do obrazu, takže není ve snímcích ani videu. Světlý text na tmavém poli ve všech vzhledech (obraz je
  monitor). **Zahozený znak** (žádný displej nemá klávesnici, nejde o textové pole) jednou za běh ohlásí stavový
  řádek: „Psaní teď nejde do SAPI: klikněte na jeho obrazovku.“ Počítá se jen tisknutelný znak, ne Enter
  nebo šipky (Enter ve výběru sestav spouští).
- **Stroj → Vypnout a zapnout (Ctrl+F12)** je vypínač: sestava naběhne tak, jak běžela (`Machine::config` s médii
  vloženými za běhu a přepnutými přepínači), paměť se ztratí, RAM disky a paměti s baterií se uloží a načtou.
  **Odchylka od plánu „stejnou cestou jako Restartovat“:** Restartovat ve výběru sestav načte znovu soubor a na
  neuložené změny konfigurátoru se ptá. Vypínač skutečného SAPI kazetu z magnetofonu nevyndá ani nevymění karty,
  proto `EmuHost::power_cycle` (stejné pro MCP `power cycle`; `power off` + `on` teď také zapne sestavu, jak
  běžela). Konfigurátor se neptá: neuložené změny drží dál (`follow_machine`) a změny uložené v souboru, ale čekající
  na restart, zůstanou oranžové, místo aby je nahradil popis běžícího stroje. Ctrl+F12 dřív dělal totéž co F12
  (reset), jinde použitý není.
  - Držené Ctrl+F12, F12 a F9 se neopakují (autorepeat PC): vypínač ani reset se neopakuje. Otevřená otázka na reset
    APS-5 se zavře, když se mezitím stroj vypne a zapne nebo vymění.
  - `sapiemu-cli --power-cycle` dělá totéž na místě (`Machine::power_cycle`, stejná přestavba jako `--load-state`
    jiné sestavy); `check_machines.py` tím u `cpm_zrmd` ověřuje Vypnout a zapnout i zápis RAM disku.
- **Průvodce se sám otevře jen po výslovném spuštění** (rozhodnutí 2026-10-17): Spustit a Restartovat ve výběru,
  Soubor → Poslední sestavy a Otevřít soubor sestavy (`ui.guide_start_gen`), a při prvním spuštění po zavření
  výběru. Ne po Vypnout a zapnout, Restartovat s touto sestavou v konfigurátoru, zapnutí, načtení stavu ani MCP.
- **Stavový řádek** nekreslí přes menu: když se nevejde, vypustí nejdřív upozornění, pak klávesy u režimů a zkrátí
  „Sestava změněna: restartovat“ na ● (text v tooltipu), pak takt CPU a MCP, pak stav klávesnice. Pauza, REC a
  rychlost zůstanou; když se nevejde ani to, není vidět nic. „emulace“ → „rychlost“. Upozornění svítí 7 s a všechna
  jdou i do Logu (`ui_notice`, chyby jako LOGE); čtyři kopie pomocné funkce v `media`, `shot`, `plotter` a `video`
  nahradila jedna.
- **Pauza a rychlost přes obraz:** při změně pauzy nebo rychlosti (F5, F8, menu, zarážka, MCP) nápis nahoře na
  obrazu na 2,5 s, poslední půlsekundu slábne (hodnocení UX chtělo 2–3 s; „Pauza (F5)“, u zarážky její důvod jako ve
  stavovém řádku, „Rychlost 200 %“, „Rychlost: maximum“, u sestav se zvukem „(bez zvuku)“, protože
  zvuk hraje jen při 100 %). Při pauze je v titulku okna „[pauza]“. Stav při startu programu se neohlašuje. Trvalý
  odznak při pauze nedělám, stav je ve stavovém řádku a titulku. Snímek GUI to umí ukázat přes `--ui-pause`.
- **Otevřít v konfigurátoru** (výběr sestav, `--ui-open`) jde přes `request(PEND_OPEN)`: na neuložené změny se zeptá.
- **První dialog** Vložit…, Nahrát program, Nahrát binární soubor a vložení diskety, kazety RKD-1 nebo pásky RMP-1
  začíná v `media/` (disky, kazety RKD-1 a pásky z něj se používají přes pracovní kopie, kazeta WAV se vloží chráněná
  proti zápisu), nové soubory (Nová kazeta…, Nová disketa…) dál ve `work/`. Uložená poslední složka má přednost.

### Chyby nahlas, průběžné ukládání médií

- **Chyby nahlas** (`ui_notice`): odmítnuté médium v okně Média (s důvodem), nepovedená nová kazeta (DSM-1, KZD-1)
  nebo disketa a nepovedené Nahrát program / binární soubor ukáže okno Chyba. Povedené nahrání stavový řádek
  a Log („Nahráno 0100-1FFFh, 7936 B: soubor“). Zapnout (Stroj → Zapnout, NAPÁJENÍ) se zavřeným konfigurátorem
  ukáže chybu v okně Chyba, s otevřeným ji dál píše konfigurátor (a Log).
- **Špatný obraz diskety nechá v mechanice starou disketu** (FDC-1, MFC-1A, RPD-1; u RPD-1 i jiný formát než 8" SS
  SD): obraz se nejdřív zkusí přečíst, teprve pak se stará disketa vysune a uloží. Dřív se vysunula hned a mechanika
  zůstala bez hlášky prázdná. Po revizi stejně i kazety RKD-1, pásky RMP-1, kazety KZD-1 a DSM-1 (MP3 se dekóduje
  jen jednou: přečtená kazeta se přesune do magnetofonu; jen stejný soubor s neuloženou nahrávkou se po uložení
  čte znovu). Jinak by vypínač (Ctrl+F12, MCP `power`) z `Machine::config` potichu vrátil médium, které okno Média
  ukázalo jako vysunuté.
- **Log barevně:** řádky `error:` barvou chyby, `warn:` barvou varování (`theme()`). **„✖ Chyby: N“** ve stavovém
  řádku (barva chyby, křížek i slovo, ne jen barva; v úzkém okně „✖ N“) počítá chyby zapsané do Logu **od jeho
  posledního otevření**, ne od startu: jedna chyba při startu by jinak svítila do konce. Kliknutí Log otevře
  a vytáhne dopředu. Snímek GUI: `--ui-log`.
- **Média se ukládají průběžně** (`EmuHost::autosave`, `Card::unsaved`): 3 s po posledním zápisu (zápisy počítá
  `media_written()`), při trvalém zápisu nejpozději po minutě, hned při pauze (F5, zarážka, MCP) a při ztrátě fokusu
  okna programu. Kontroluje se 4× za sekundu bez práce se soubory; obraz se zapíše celý, jen když se změnil. Po
  nepovedeném uložení se to zkusí znovu po dalším zápisu nebo za minutu (Log se nezahltí); pauza a ztráta fokusu
  ukládají jen po novém zápisu (`EmuHost::save_media_new`), jinak by každá zapsala do Logu stejnou chybu znovu.
  Nepovedené uložení jde jen do Logu (jednou za minutu) a do počtu „✖ Chyby: N“, okno Chyba ho zatím neukáže:
  chtělo by to příznak selhání u každého média ve všech kartách. Platí pro diskety, RAM disky ZRMD-1/ZRD-1V
  (`rw`) a RMD-1, kazety RKD-1, pásky RMP-1, kazety DSM-1 (WAV) a KZD-1. Kazeta KZD-1 se
  během záznamu (zapnutá záznamová hlava) průběžně neukládá, až pár sekund po jeho konci: uložení uprostřed by
  záznam rozdělilo a přechod zaokrouhlený o půl mikrometru před hlavu mohla další část smazat (ukončení, vysunutí
  a uložení stavu záznam dál uloží hned). Rozdělení záznamu při změně rychlosti pásku během záznamu zůstává, bylo
  tu už před etapou 4. `nosave`, `readonly`, chráněné kazety a testy nezapíšou nic.
  - **Paměti s baterií** (CRM-1, DMR-1, RTC-1) se dál ukládají jen při vypnutí a ukončení: program do nich zapisuje
    pořád, takže by „ticho“ nenastalo a zdržovalo by ukládání disků.
  - IDE disk a PMD32-SD zapisují rovnou do souboru; DPI-1L se chová jako karta (nezapsaná stránka se vypnutím ztratí).
  - **Zápis přes dočasný soubor** vedle obrazu (`jméno.tmpNNN`), `_commit`/`fsync` a přejmenování
    (`file_write_safe`): pád ani výpadek proudu nenechá napůl zapsaný obraz. Když Windows přejmenování odmítne
    (soubor drží otevřený jiný program), zapíše se jako dřív přímo. Stejně se zapisují i paměti s baterií.
  - Dosavadní ukládání ZRMD-1 po 5 s emulovaného času je pryč: při maximální rychlosti zapisovalo 4 MB mnohokrát za
    sekundu a při pauze nic.
  - **Ukládají se všechna média, i pásky a kazety:** ztráta nahrávky po pádu bolí nejvíc a skutečná mechanika
    zapisuje hned. **Vysunout bez uložení** proto teď zahodí jen zápisy posledních pár sekund (tooltip to říká),
    ukázkové médium vrátí Obnovit z originálu. U vlastních obrazů mimo `media/` cesta zpět není (na rozhodnutí
    autora, viz konec oddílu).
  - **Čas měří hodiny PC, ne emulovaný čas:** při maximální rychlosti se tak neukládá častěji a při pauze se
    nezastaví. Nepovedený zápis (plný disk) nechá starý obraz beze změny; když nejde založit dočasný soubor,
    zapíše se obraz přímo jako dřív.
  - Ověřeno: `test_zrmd` (3 s po posledním zápisu, nový zápis čeká znovu, pauza, žádný dočasný soubor, `nosave`
    nic), `test_floppy_imd` (špatný obraz, `unsaved`, `save_media`), `sapiemu-cli --mcp` s RAM diskem v dočasné
    složce (zápis přes `io_write`, soubor změněný za 3,3 s za běhu) a `check_machines.py`, který nově hlásí zbylé
    dočasné soubory. Po revizi i `test_kzd1` (uložení uprostřed záznamu nic neuloží, po konci záznamu stejné přechody
    jako bez něj, špatný soubor kazetu nevysune) a `test_rkd1` (nové disky a zapsaný sektor `unsaved`, `save_media`
    zapíše soubor, špatný obraz kazetu nevysune).

### Průvodci her, reset APS-5, konfigurátor

- **Průvodci her** (`sapi1v_hry`, `sapi1v_PoP`, `sapi1v_BomberNet`, česky i anglicky): volba `3` je blok kódu
  s tlačítkem Napsat do SAPI a u ní věta, že volby 0–2 čekají na řadič disket, který sestava nemá, reset sám
  nepomůže (ECMON si volbu pamatuje v RAM), pomůže stisknout `3` hned po resetu nebo Stroj → Vypnout a zapnout.
  Stejně i průvodce sestavy V (`sapi1v`). V českých průvodcích „palba“ (jako tabulka zkratek), ne „fire“. Blok
  píše „3“ a Enter, Enter navíc ukáže v CP/M jen druhé `A>` (ověřeno `sapiemu-cli`).
  - Každá hra a hlavní dema mají vlastní blok kódu (jméno ověřené výpisem RAM disku `tools/cpmdisk.py --dir`)
    a u něj ovládání. Ukončení ověřené v emulátoru: ESC u FLAPPY, BOMBER, FXS4, SPLAY a RAT, klávesa u BADAPPLE,
    KONEC + A u BKP. Dema, která se sama neukončí (REMBRAN2, Y1STDEMO, SNDDEMON, CGATEST, MPHTEST, FLAPDGD), zastaví
    reset F12: ECMON za pár sekund sám znovu spustí volbu 3 (ověřeno přes MCP vlastního `sapiemu-cli --mcp`).
  - Dřívější blok `SPLAY HELLFIRE.VGM` + `RAT SMJOOP.RAD` napsal oba řádky za sebou, druhý šel do běžícího
    přehrávače; teď jsou to dva bloky.
  - U Bad Apple!! věta „Obraz místy rychle bliká“ (návrh z hodnocení UX, 5.x).
  - Řádek „Normální při startu: PMD32-SD nenalezen, INIT HDD YXYX…“ jen u her a BomberNetu. Prince of Persia má IDE-1
    s PMD32-SD a hlásí `PMD32-SD OK`.
- **Souhrny sestav s ECMONem** v Ostatních (`cpm_zrmd`, `cpm_ide`, `cpm_zrmd_fdc1`, `cpm_zrmd_mfc1a`, `cpm_zrmd_dpp1`,
  `cpm_zrmd_rtc_dsp`, `cpm_ec1045`, po revizi i `libor_doma` z Dalších sestav s deskami Libora Lasoty; jeho popis
  radil volbu 1, která visí) v `machines/_vyber.json`: co sestava je a kterou volbu stisknout po zapnutí
  (podle `tools/check_machines.py` a prvních kroků). Titulky (kratší názvy v seznamu) jsem nepřidával, v seznamu
  zůstávají technické názvy ze souborů sestav.
- **Reset APS-5 se nejdřív zeptá:** F12, F9, Stroj → Reset, RESET na panelu ZDR i Reset v debuggeru jdou přes
  `request_reset` (`aps5.cpp`). U sestavy s pecemi (`FurnacePlant`) otevře okno „Resetovat jednotku APS-5?“ s
  důvodem (skutečná jednotka RESET nemá, firmware po něm smaže programy, typy termočlánků a kalibraci) a tlačítky
  Ano, resetovat / Zrušit, Esc = Zrušit, Enter nedělá nic (reset omylem nejde). Reset sám zůstává věrný (CHYBA 0,
  INICIALIZACE). Jiné sestavy se resetují hned, MCP `reset` se neptá nikdy. Pracoviště IMS-2 (`plant` typu
  IMS-2, ZPS-2 s BASIMSem) se neptá: reset počítače tam nic neničí. Snímek GUI: `--ui-reset`.
- **Konfigurátor:** rám se zmenší i podle výšky okna: s checkboxem zabere nejvýš 55 % místa pod lištou, nejmenší
  měřítko kleslo z 0,034 na 0,022 velikosti písma. Typ karty na panelu se zmenší do volného místa podél i napříč
  panelem, nejvýš na 0,6 velikosti písma; menší se nekreslí (po revizi: velikost písma 0 zastavila debug build
  assertem ImGui při 1400–1600 × 900–1000 u sestav s 8 kartami, při 1280×720 typ přesahoval přes sousední
  panely). Nastavení pod ním dostane zbytek, nejméně 8 řádků (v hodně nízkém okně se pak posouvá celé okno).
  Při 1100×760 a 1280×720 je po kliknutí na X7 vidět 5 řádků nastavení CGA-1V (dřív žádný). Rám zůstává zarovnaný
  vlevo. Vedlejší účinek: popisek JPR-1V (úzký pruh vedle konektorů) se zhruba od 1600×900 dolů nekreslí.
  - **Kliknutí na slot** vybere záložku Nastavení karty (i když byla otevřená Mapa portů a paměti) a posune
    nastavení do pohledu: když ho hlášky nad ním odsunou, dostane se záložka nahoru; v nízkém okně se okno posune
    k nastavení. Klik do prázdného rámu záložku nepřepne ani neposune (posuvník by jinak překvapil).
    `--ui-rack SLOT` to ukáže jako po kliknutí.
  - **Textová pole a cesty médií** (`PK_TEXT`, `PK_FILE`, `PK_DIR`) se potvrdí až Enterem nebo opuštěním pole jako
    `PK_HEX`, Esc vrátí původní text. Dřív každý napsaný znak znovu načetl médium (nebo založil nové z rozepsaného
    jména).

### Na rozhodnutí autora

- **Průběžné ukládání a Vysunout bez uložení.** Doporučení: nechat, jak je (ukládat všechna média). Další možnosti:
  volba média „neukládat průběžně“ pro starý postup „zahodit celé sezení“, nebo pásky a kazety (DSM-1, KZD-1,
  RMP-1) průběžně neukládat a nechat jim Vysunout bez uložení v plné síle. **Rozhodnuto 2026-10-29:** ukládat
  všechna média potvrdil autor, Vysunout bez uložení zmizelo (ráno: připomínky autora).
- **Vypnout a zapnout nenačítá soubor sestavy znovu** (odchylka od plánu, důvod výše). Doporučení: nechat. Kdo chce
  sestavu ze souboru, má Restartovat ve výběru sestav.
- **Názvy sestav s ECMONem v Ostatních** zůstaly technické (jména souborů), dostaly jen souhrny. Kratší titulky
  by šly doplnit do `_vyber.json`. **Rozhodnuto 2026-10-29:** krátké titulky doplněné (autor souhlasil).

### Ruční zkouška autora a známá omezení

Snímek skrytého okna tyto věci neukáže:

- Psaní: návrat klávesnice po Napsat do SAPI, Spustit, Nahrát program, vložení média, Vložit text a zavření
  průvodce; nápis „Klikněte sem a pište“ a hláška o zahozeném znaku.
- „[pauza]“ v titulku okna; uložení média při přepnutí do jiného okna Windows (zapsat na disk, přepnout, podívat se
  na soubor); okno Chyba u odmítnutého média.
- Známé omezení: po jiných akcích z menu (Reset, Snímek…) si klávesnici dál nechá menu, obecné vracení fokusu
  zatím není. Nápis „Klikněte sem a pište“ a hláška o zahozeném znaku to zatím pokryjí.

## 2026-10-29 – etapa 5 plánu vzhledu: okna

Etapu udělal workflow (3 agenti: plovoucí rozmístění, zadokované rozložení, celá obrazovka a zvětšení; galerie
s oponenturou, opravy nálezů), autor spal a rozhodnutí nechal na Claudovi. Platí dál: uložená místa oken (`imgui.ini`)
mají vždy přednost, výchozí místo okno dostane jen při prvním použití (`ImGuiCond_FirstUseEver`) nebo po Zobrazení →
Výchozí rozložení oken. Docking ani F9 nejsou dogma (2026-10-28). Rozměry oken jsou ve výškách písma (em). Co má
autor rozhodnout a vyzkoušet ručně, je na konci oddílu.

### Checkpoint C (rozhodl vedoucí agent, autor potvrdil 2026-10-29: plovoucí okna)

- **Výchozí zůstávají plovoucí okna s chytrým rozmístěním, zadokované rozložení je volba** v Zobrazení → Nastavení
  → Vzhled → Okna: Plovoucí / Zadokovaná (uloží se jako `appearance.layout`). Důvod: plán chtěl obě rozložení
  porovnat na snímcích a vybrat má autor. Plovoucí okna jsou dosavadní chování a místa uložená dřívějšími verzemi
  platí dál. Zadokované má při 1280 × 720 kompromisy (menší displeje ve sloupci, Debugger a Paměť jako záložky,
  vidět je jedna).
- **Galerie obou rozložení pro autora:** `python tools\ui_gallery.py --layouts` (8 obrazovek, 1280 × 720
  a 1920 × 1080, plovoucí vedle zadokovaného). Po opravách `out\ui-gallery\20261029-044908-layouts\index.html`
  (100 %), `out\ui-gallery\20261029-044925-layouts\` (125 %) a `out\ui-gallery\20261029-044940-layouts\` (150 %,
  i 1100 × 760). Skript místo jednorázových: galerie jde zopakovat po každé změně. Výchozí měřítko 1, aby obrázky
  nezávisely na PC. Přístroje a Média otevírá sestava sama jako uživateli, průvodce je jen u ZPS-1 a XY 4130.

### Plovoucí okna

- **Jeden pomocník rozmístění** `src/frontend/layout.cpp` nahradil `first_use_right` a všechna volání
  `SetNextWindowPos/Size` při prvním použití (obrazovka, grafika, diagnostika, IMS-2, APS-5, zapisovač, průvodce,
  konfigurátor, Média, terminál, tiskárna, debugger, paměť, log I/O, Log, výběr sestav, Nastavení, zkratky,
  O programu). Mrtvý kód `rack_behind` je pryč.
- **Tabulka `k_wins`:** velikost, nejmenší velikost a role každého okna v em. Dřívější pixely jsou přepočtené při
  16 px: při 100 % vypadají okna stejně, s měřítkem rostou. Média mají nově 48 × 28 em.
- **Hlavní výstup vlevo nahoře** v největším celočíselném zvětšení, které se vejde do 65 % šířky a celé výšky
  plochy. Hlavní výstup je konzole, nebo displej z nového klíče sestavy `main_display` (jméno nebo typ karty):
  CGA-1V v `sapi1v_hry`, `sapi1v_PoP` a `sapi1v_BomberNet`, BGP-1S v `jpr1s_bgp1s`. Konfigurátor klíč zachová
  a okomentuje (`test_machine_cfg`). Důvod: u her je hra na grafické kartě, konzole s ECMONem je vedlejší.
- **Zvětšení hlavního výstupu klesne** (ne pod 200 %), když by se vedle něj nevešel přístroj nebo Média, které
  sestava otevírá. APS-5 při 1920 × 1080 tak má konzoli 300 % a nic se nepřekrývá. Kvůli průvodci se nezmenšuje,
  ten je dočasný.
- **Ostatní okna do volného místa**, nejdřív do sloupce u pravého kraje:
  1. volné místo mimo všechna okna;
  2. mimo okna, na kterých mu záleží, a přes ostatní co nejméně. Váhy: přístroje a Média 16, hlavní výstup 4,
     konzole 3, další displeje a průvodce 2, nástroje 1;
  3. jen průvodce: zmenší se až na 0,6 své nejmenší velikosti, než by šel přes přístroj;
  4. nástroje bez místa k pravému kraji: první nahoře, další od poloviny výšky dolů, každý o krok níž. Registry
     a tlačítka Debuggeru tak zůstanou vidět (Paměť je zakrývala). Ostatní okna tam, kde zakryjí nejméně.
- **Pořadí:** displeje, přístroje a Média dostanou místo před průvodcem, nástroji a dialogy. Proto se změnilo pořadí
  kreslení v `app_run`: hlavní výstup (může to být okno grafiky), další displeje, zapisovač, IMS-2, diagnostika,
  APS-5, Média, průvodce, nástroje, dialogy. Průvodce tak leží před přístroji.
- **Přístroje a Média drží velikost, průvodce a displeje raději zakryjí méně.** Důvod: APS-5 stlačené na polovinu
  výšky bylo k ničemu, průvodce jde posunout. APS-5 se raději zúží až na 40 em, než by zakrylo obrazovku jednotky.
  Při 1280 × 720 jsou pak pece 4 a 8 uříznuté (ukáže je větší okno), což je lepší než schovaná polovina AND-1.
- **Průvodce a Média nezakrývají hlavní výstup**, když se vejdou jinam (průvodce i menší, než chce). Průvodce nejde
  přes přístroj ani Média, je-li kdekoli místo (dřív je při prvním použití zakrýval, rozhodnutí 2026-10-28).
- **Další displeje** v tomto pořadí: 1:1 a víc ve volném místě (největší celočíselné zvětšení do pravého sloupce,
  poloviční výška, když sestava otevírá přístroj); 3/4 až 1:1 ve volném místě; 1:1 mimo jiné displeje; 1:1 mimo
  hlavní výstup; jinak kde zakryjí nejméně. Nejmenší 1:1 natvrdo není: při 1280 × 720 by AND-1V 1:1 zakryl hru.
- **Displej menší než 1:1 se kreslí hladce** (lineární filtr), od 1:1 ostré body, pokud uživatel nezvolil
  Vyhlazování. Důvod: nejbližší bod vynechával celé řádky bodů písmen („DOOT SS25G“ místo „BOOT SS256“).
- **Dialogy uprostřed plochy** (výběr sestav, Nastavení…), nikdy přes lištu menu: výběr sestav ji dřív zakrýval.
- **První start s výběrem sestav:** když se první sestava spustí z výběru a `imgui.ini` ještě neexistovalo, dostanou
  okna výchozí místa pro tuto sestavu (`ui.layout_reset`, dřív `App.reset_layout`). Jinak by obrazovka zůstala na
  místě naplánovaném pro ZPS-1.
- **Výchozí rozložení oken smaže i záznamy oken, která ještě nevznikla** (`ImGuiWindowSettings::WantDelete`, stejně
  jako `ClearWindowSettings`). Jinak by okno otevřené později přišlo na staré místo.
- **Plocha je `GetBuildWorkRect` po kreslení lišty menu:** `WorkPos` v prvním snímku lištu ještě nemá. Neviditelné
  náhradní okno ImGui `Debug##Default` se nepočítá. Displeje, které jsou zatím jen v `imgui.ini`, se počítají na
  svých uložených místech.
- **Plán se počítá jen po změně** (generace sestavy, plocha, písmo, měřítko, okna otevíraná sestavou), ne v každém
  snímku.
- **Bez prvních míst za celé obrazovky a minimalizace** (`layout_hold`): okno otevřené v tu chvíli by dostalo místo
  podle monitoru nebo velikost 1 px a ta by se uložila. Místo dostane v prvním snímku po návratu.
- **Okno programu** zabere při prvním startu 88 % použitelné plochy hlavního monitoru (bez hlavního panelu). Uložená
  velikost se zmenší do použitelné plochy svého monitoru (rám odhadnutý na 16 × 40 px krát měřítko) a okno se
  posune jen tehdy, když se muselo zmenšit. Důvod: velikost uložená na velkém monitoru by na notebooku přesahovala.

### Zadokované rozložení

- **`layout_dock()` přes DockBuilder:** hlavní výstup ve středovém uzlu (zadokované okno řídí dok, displej jede
  v režimu Přizpůsobit oknu, rozhodnutí 2026-10-10). Sloupec u pravého kraje má shora tři části: další displeje;
  okna sestavy jako záložky (přístroje, Média, terminál, tiskárna); průvodce a ladicí okna jako záložky. Část bez
  otevřeného okna místo nezabírá.
- **Ve středu je jen hlavní výstup:** dva displeje vedle sebe by mu vzaly polovinu zvětšení.
- **Přístroje jsou zadokované ve sloupci**, ne plovoucí: plovoucí APS-5 nad dokem by zakrylo hlavní výstup nebo
  sloupec, což má dok právě odstranit. Sloupec se místo toho rozšíří na šířku přístroje (nejvýš 55 %) a přístroj
  dostane 70 % jeho výšky. Záložku jde vytáhnout ven.
- **Průvodce a ladicí okna mají vlastní část**, oddělenou od oken sestavy: průvodce i přístroj nebo Média jsou
  vidět zároveň, jako u plovoucích oken.
- **Šířka sloupce** je 36 % okna (26–48 em), pro přístroj nebo Média až 55 %, pro další displeje 1:1 až 45 %.
  Hlavnímu výstupu ale nevezme šířku, kterou má v plovoucím rozložení (sloupec aspoň 25 %): při 1280 × 720 mají
  hry 300 % místo 200 % a RMP-1 200 % místo 100 %. Výšky ve sloupci: další displeje 65 % (se dvěma 70 %, 35 %, když
  sestava otevírá přístroj nebo Média), okna sestavy 60 % zbytku (s přístrojem 70 %), průvodce a ladicí okna zbytek.
  Otevřený průvodce má aspoň 12 em, jinak by byl vidět jen nadpis. Při změně velikosti okna drží sloupec šířku
  v pixelech (pravidlo středového uzlu ImGui), displej dostane zbytek.
- **Všech 5 oken displejů je ve sloupci předem**, takže displej sestavy spuštěné později nikdy neplave nad dokem.
- **Vybrané záložky:** průvodce mezi ladicími okny, když je otevřený, a první okno sestavy mezi jejími okny. Drží se
  4 snímky, protože nové okno si s fokusem vezme i svou záložku.
- **Lišta záložek zůstává i u jediné záložky** (ne `AutoHideTabBar`): nese jméno karty a zvětšení a je to úchyt
  k vytažení okna. Uzly nemají vlastní křížek (`NoCloseButton`), jen zdvojoval křížek záložky.
- **Rozložení se postaví** jen bez svého souboru míst, volbou v Nastavení a Výchozím rozložením oken, jinak nikdy
  (uložená místa mají přednost). Výchozí rozložení odstraní dok přes `DockBuilderRemoveNode`, i s prázdnými děleními.
- **Po změně sestavy** jde doprostřed hlavní výstup nové sestavy (`layout_dock_main`) a displej, který tam byl, na
  jeho místo ve sloupci. Ostatní okna a velikosti částí zůstanou (uživatelovo rozložení), okno, které uživatel z doku
  vytáhl, zůstane venku. Důvod: bez toho zůstala po ZPS-1 → hry uprostřed konzole a CGA-1V ve sloupci. Uzel, který
  zůstane prázdný, se drží (`DockContextProcessUndockWindow` s ponecháním ID), jinak by ho ImGui smazalo a vyměněný
  displej by skončil v novém plovoucím uzlu.
- **Každé rozložení má svůj soubor míst oken** (`imgui.ini` plovoucí, `imgui-docked.ini` zadokované; u `--settings
  SOUBOR` `SOUBOR.imgui.ini` a `SOUBOR.imgui-docked.ini`). Volba uloží místa opouštěného rozložení a načte druhé,
  výchozí místa jen při jeho prvním použití. Důvod: první verze volby spustila celé Výchozí rozložení a smazala
  uložená místa opouštěného rozložení, tedy výchozí místa bez pokynu uživatele. Potvrzovací dialog by byl horší,
  takhle je přepnutí vratné.
- **ID dokovací plochy** se počítá jako výchozí ID ImGui (`WindowOverViewport_%08X` / `DockSpace`), takže dok
  uložený v `imgui.ini` dřívějšími verzemi platí dál (debug build to hlídá `IM_ASSERT`).
- **`focus_screen_window`** zadokovanému oknu sestavy vybere záložku a hostitelské okno doku nikdy nevytáhne
  dopředu: zakrylo by plovoucí okna.
- **`--ui-layout floating|docked`** vybere rozložení jen pro jedno spuštění, jako `--ui-theme`. Když se liší od
  uloženého, začne z výchozích míst a soubory míst nečte ani nezapisuje (ověřeno hashem souboru). Volba v Nastavení
  přepsání ukončí a místa se zase ukládají. `--ui-layout-switch` přepne rozložení jako volba v Nastavení (pro snímky).

### Celá obrazovka a zvětšení

- **Alt+Enter** se zachytí v `handle_key` dřív, než Enter dojde do SAPI (AltGr ne; při psaní do textového pole se
  neuplatní jako ostatní zkratky). Přepne celou obrazovku displeje použitého naposledy (`SDL_SetWindowFullscreen`,
  okno bez rámu přes plochu). Esc dál patří SAPI, F11 zůstává krokem debuggeru.
- **`draw_fullscreen` kreslí jen ten displej** přes celou obrazovku, s celočíselným zvětšením a poměrem TV. Okna
  displejů v tu chvíli svůj obraz nekreslí, aby se nekreslil dvakrát.
- **Ostatní okna se skryjí, nevynechají:** kreslí se v každém snímku a před `Render` dostanou příznak Hidden. Po
  návratu se tak neobjeví jinde, nevezmou fokus a nezmění pořadí. Vynechání by zastavilo i práci, kterou okna dělají
  před `Begin` (otázka konfigurátoru při ukončení, otázky Médií, XMODEM, DEMO APS-5). Dialogy a otázky se ukážou
  přes obraz.
- **Lišta menu se dál kreslí, jen skrytá:** plocha zůstane stejná, dok ani plán oken se nepřestaví.
- **Po startu sestavy je displejem použitým naposledy hlavní výstup** a dostane klávesnici (u her CGA-1V):
  Alt+Enter, Ctrl+F5 i Zobrazení → Zvětšení obrazovky pak platí pro hru. Konzole má `NoFocusOnAppearing`, aby si
  klávesnici nebrala jen tím, že se objeví.
- **Celá obrazovka se neukládá**, každé spuštění začne v okně; jako „maximalizované“ se uloží stav před celou
  obrazovkou. Při změně sestavy zůstane zapnutá a ukáže nový hlavní výstup. Když ji ukončí systém
  (`SDL_EVENT_WINDOW_LEAVE_FULLSCREEN`), skončí i naše.
- **Nápis** „Celá obrazovka: Alt+Enter ji ukončí“ na 2,5 s po zapnutí. Při pauze zůstane přes obraz malý nápis,
  protože titulek ani stavový řádek nejsou vidět.
- **Menu pravého tlačítka** v celé obrazovce: Celá obrazovka (Alt+Enter), výběr displeje sestavy, celočíselné
  zvětšení, poměr TV, snímek a video. Položka Celá obrazovka je i v Zobrazení a v menu pravého tlačítka každého
  displeje, Alt+Enter v tabulce zkratek.
- **Zvětšení:** „Přizpůsobit oknu“ je první položka, zaškrtnutá, když není zvolené pevné zvětšení. Zobrazení →
  Zvětšení obrazovky platí pro displej použitý naposledy a jmenuje ho (dřív jen pro konzoli).
- **Okna displejů nejdou sbalit** (`NoCollapse`) a okna se posouvají jen za titulek
  (`ConfigWindowsMoveFromTitleBarOnly`): tažení po obrazu nebo panelu přístroje okno neposune, klik do obrazu
  zachytí myš SAPI.
- **Snímky** (`--ui-snapshot`) `SDL_SetWindowFullscreen` nevolají, obrázek má velikost `--ui-size`. `--ui-fullscreen`
  stiskne Alt+Enter, dvakrát zapne a zase vypne (obrázky pak jsou shodné s obrázky bez celé obrazovky). `--ui-zoom`
  platí pro displej použitý naposledy, stejně jako menu.

### Na rozhodnutí autora

- **Výchozí rozložení (checkpoint C):** plovoucí (doporučení, teď výchozí), nebo zadokované. Podle galerie výše.
- **Okna mimo plochu se při startu ani po zmenšení okna nevracejí dovnitř** (ImGui nechá vidět aspoň 19 px,
  `DisplayWindowPadding`). Návrh: zvětšit na asi 3 em. Posune to ale okna z `imgui.ini`, proto to čeká na autora.
  Rozhodnuto 2026-10-29 (autor souhlasil): okno víc než napůl mimo plochu se vrátí dovnitř (ráno: připomínky
  autora níže).
- **RMP-1 s plovoucími okny při 1280 × 720 a 100 %:** Média zakryjí pravý okraj konzole 200 % (sloupce BLSIZ a db).
  Druhá možnost je konzole 100 % bez překryvu. Doporučení: nechat 200 %.

### Ruční zkouška autora a známá omezení

Snímek skrytého okna tyto věci neukáže:

- Celá obrazovka na skutečném monitoru: Alt+Enter tam a zpět, ukončení systémem (Alt+Tab, klávesa Windows).
- Dva monitory: okno a celá obrazovka na druhém monitoru, zmenšení uložené velikosti na menší monitor.
- Notebook: velikost okna při prvním startu (88 %), rozmístění při 125 a 150 %.
- Zadokované rozložení v tmavém vzhledu a změna sestavy za běhu (výběr sestav, MCP) se zadokovanými okny.

Známá omezení:

- V malém okně se překryvu nejde vyhnout: při 1100 × 760 zakryje APS-5 většinu obrazovky a průvodce část APS-5;
  při 1280 × 720 se tři displeje `sapi1v_hry` vejdou jen se zmenšeným DGD-1V; s IMS-2 při 1600 a 1920 zakryje
  průvodce roh hlavní obrazovky; Debugger při 1100 × 760 a 150 % zakryje pravý okraj konzole 400 % (nejmenší šířka
  26 em). APS-5 a IMS-2 se při 1280 × 720 celé nevejdou v žádném rozložení.
- APS-5 nemá vodorovný posuvník: uříznuté pece při 1280 × 720 ukáže jen větší okno.
- Plovoucí okna po změně sestavy: hlavní okno zůstává na místě první sestavy (uložená místa mají přednost),
  pomůže Výchozí rozložení oken.
- Průvodce má nejvýš 36 em a Média jsou při 1920 × 1080 nízká (pod nimi je volno).
- Zadokované: Debugger a Paměť jsou záložky, vidět je jedna. Prázdné okno DGD-1 sestavy s XY 4130 zabírá horní část
  sloupce; je to skutečný displej sestavy, proto zůstal.

## 2026-10-29 – etapa 6 plánu vzhledu: před betou

Etapu udělal workflow (3 agenti: menu a zkratky, bezpečí dat, pro testery; 3 oponenti: data, texty a GUI, plán;
opravy nálezů), autor spal a rozhodnutí nechal na Claudovi. Chování skutečného SAPI-1 se nemění: jde o ovládání,
konfigurátor a ukládání stavu na straně PC a o podklady pro testery. Beta zip se nestavěl, postaví se až na autorův
pokyn. Změny po oponentuře jsou označené. Co má autor rozhodnout a vyzkoušet ručně, je na konci oddílu.

### F9 = breakpoint, reset jen F12 (rozhodl vedoucí workflow za autora)

- **Zkratky ladění jako ve Visual Studiu:** F5 běh / pauza, F10 krok přes a F11 krok zůstaly, **F9 přepne
  breakpoint**, reset je jen **F12** (Ctrl+F12 vypnout a zapnout). Plán to měl jako návrh ke schválení. Rozhodl
  vedoucí workflow za autora: autor nechal rozhodnutí na Claudovi a F9 = reset nebylo dogma (2026-10-28). Důvod:
  kdo ladí, zná F9 z Visual Studia, a reset na dvou klávesách nic nepřidával. Autor to může vrátit (níže „Na
  rozhodnutí autora“).
- **Tím končí rozhodnutí z 2026-10-03** (F9 = reset kvůli ladění pod Visual Studiem). Pod debuggerem PC si F12 bere
  Windows (`UserDebuggerHotKey`), reset pak jde z menu Stroj → Reset nebo tlačítkem Reset (F12) v debuggeru
  (`docs/build.md`).
- **F9 působí jen s otevřeným debuggerem:** na řádku disassembleru pod myší, jinak na PC. Bez debuggeru jen
  upozorní ve stavovém řádku: „F9 nastavuje breakpoint v debuggeru (Ladění → Debugger). Reset je F12.“ Důvod: alfa
  testeři měli F9 jako reset a breakpoint na PC běžícího stroje by ho potichu zastavil.
- **Ladění → Breakpoint zap / vyp (F9)** je povolené jen s otevřeným debuggerem a z menu platí pro PC (myš je
  v tu chvíli na menu). Debugger má nápovědu „adresa; klik na řádek nebo F9 = breakpoint“, tlačítko Reset (F12)
  a odkaz na Klávesové zkratky. Nastavený breakpoint ohlásí stavový řádek („Breakpoint nastaven na 0DB8h“).
- Nepoužitý klíč překladu „Keys: F5 run/pause…“ (zmiňoval F9 jako reset) je pryč.

### Menu, schránka, Enter a Esc

- **Přejmenování:** Zobrazení → „Sestava…“ je **Konfigurátor sestavy…** („Machine configurator…“), Soubor →
  „Otevřít soubor sestavy…“ je **Spustit sestavu ze souboru…** („Start a machine from a file…“). Důvod: položka
  sestavu spustí, kdežto Otevřít… v konfigurátoru ji jen načte k úpravám. Okno se jmenuje „Konfigurátor sestavy -
  jméno“ se stejným ID `###rack`, takže místa v `imgui.ini` zůstanou. Plán čekal asi 77 zmínek v dokumentaci,
  aktuálních bylo asi 15 (`docs/`, průvodci APS-5). Historie (tento soubor, poznámky k vydání, plán, hodnocení UX)
  zůstala, jak byla.
- **Kopírovat text obrazovky (Ctrl+Shift+C)** je v menu Zobrazení pod Snímkem obrazovky a v menu pravého tlačítka
  konzole. Grafické displeje ho nemají, text nemají. Do schránky jde `screen_text` konzole bez prázdných řádků
  nahoře a dole jako UTF-8 (SDL ve Windows doplní CR LF), výsledek ohlásí stavový řádek. Ctrl+C dál jde do SAPI jako
  řídicí kód. Ctrl+Shift+C dřív posílalo také ^C a nikde jinde použité nebylo.
- **V sériovém terminálu** Ctrl+Shift+C kopíruje jeho text a Ctrl+Shift+V do něj vkládá, jako Alt+C a Alt+V
  (stejně jako Windows Terminal). Důvod: text konzole by tam překvapil. ^C a ^V zůstávají na Ctrl+C a Ctrl+V.
- **Enter a Esc v modálních oknech** (`popup_enter`, `popup_escape` v `app.cpp`). Odpovídá jen okno vpředu, a ne
  v prvním snímku, kdy se otevřelo. Důvod: Enter, který okno otevřel, ho nesmí hned zavřít. Tlačítko pro Enter má
  barvu zvýraznění, jako Windows značí výchozí tlačítko.
  - Chyba: Enter i Esc zavřou.
  - Neuložené změny: Enter i Esc = **Zrušit**. Důvod (hodnocení UX): nic nezapíše ani nezahodí.
  - Nahrát program (Intel HEX) a Nahrát binární soubor: Enter = Nahrát, i z pole adresy. Program na kazetu: Enter =
    Vložit. Esc všude zruší.
  - **Enter nedělá nic, kde je jediná akce nevratná:** reset APS-5 (rozhodnutí etapy 4), Obnovit z originálu a zápis
    systému RKD-1 (SYSGEN). Esc tam zruší.

### Nahlásit chybu a log předchozího spuštění

- **Nápověda → Nahlásit chybu…** řekne, co popsat (sestava, kroky, co se čekalo a co se stalo, snímek, stav,
  upravený soubor sestavy) a kam to poslat. Ukáže diagnostiku: verze (u debug buildu „debug build“), běžící sestava
  se souborem (i s ponechanými změnami, níže), karty po slotech, stav a rychlost, složka dat, pracovní složka,
  soubor nastavení, log a předchozí log, systém (Windows 10/11 s číslem sestavení přes `RtlGetVersion`, 64bitový
  program, vlákna CPU, RAM) a obraz (renderer, velikost okna, měřítko, jazyk, verze SDL a Dear ImGui).
- **Kopírovat diagnostiku** dá do schránky totéž a posledních 200 řádků logu. **Otevřít složku logu** ji otevře
  v Průzkumníku (`SDL_OpenURL(file:///…)`). Okno upozorní, že v cestách je jméno uživatele Windows.
- **Kopie diagnostiky je anglicky** jako log, popisky a hodnoty v okně jsou přeložené (běží / pauza, rychlost,
  vypnuto…; hodnoty po oponentuře). Důvod: autor čte kopii stejně v každém jazyce testera, tester čte okno ve svém.
- **Kam hlásit:** do zveřejnění SAPIemu cestou, kterou tester program dostal, potom přes GitHub Issues. Žádný
  e-mail ani jiný osobní kontakt do programu, repa ani zipu. Adresa Issues je do zveřejnění jen text v závorce, ne
  odkaz (po oponentuře): repo je soukromé a odkaz by vedl na 404. Odkaz zapne `k_issues_public` v `help.cpp` při
  zveřejnění (etapa 9). Texty říkají „do zveřejnění … potom“ místo „v betaverzi“, aby seděly, ať se verze bude
  jmenovat alfa, nebo beta. Totéž říkají První kroky (dřív jen odkaz na Issues), `GETTING-STARTED.md` a README.
- **Log předchozího spuštění `sapiemu.prev.log`** (po oponentuře): start před otevřením nového logu ten starý
  přejmenuje (`file_rename` v `core/util`), jedna generace; při `--settings SOUBOR` je to `SOUBOR.prev.log`. Důvod:
  po pádu tester program spustí, aby se dostal k Nahlásit chybu, a ten start dřív log pádu přepsal. Rada „pošlete
  log před dalším spuštěním“ tak nešla splnit. Nahlásit chybu má řádek Předchozí log a větu „Po pádu pošlete
  i předchozí log…“.

### Dotaz na neuložené změny

- **Dotaz vyjmenuje skutečné změny proti souboru** (`machine_cfg_changes` v `src/core/machine_cfg.cpp`): název,
  popis, sběrnice, náhodná paměť, okna nahoře, karty přidané, odebrané a přesunuté („X2 DMR-1: přesunuta do X4“)
  a parametry podle katalogu jménem a novou hodnotou („X1 JPR-1V: TURBO: CPU 4 MHz, sběrnice vždy 2 MHz →
  vypnuto“). Ukáže nejvýš 9 řádků a „a další změny (N)“, aby okno zůstalo na obrazovce. Celý seznam je v bublině nad
  „neuložené změny“ v konfigurátoru.
  - Jen nová hodnota: rozhoduje se, co s ní, stará je v souboru.
  - Jméno parametru je popisek katalogu bez závorek. Dřív řez u první závorky dělal z „Čekání (RDY) při přístupu do
    VRAM“ jen „Čekání“. Parametr sekce má jméno sekce („Mechanika 0, obraz diskety“), soubor jen jméno, volba svůj
    popisek.
  - Vypnutá sekce je jeden řádek, ne všechny její parametry. Klíče, které katalog nezná, jsou „jiná nastavení“.
    Přesun se pozná podle typu karty, nejdřív se stejným nastavením.
- **Neuložená změna je jen skutečná změna:** hodnota zapsaná jinak, ale stejná (výchozí hodnota vypsaná, `10h`
  a 16), se nepočítá ani pro dotaz, ani pro hvězdičku v titulku. Důvod: dřív stačil jiný zápis v JSON a dotaz přišel
  bez jediné změny, kterou by šlo vyjmenovat.
- **Tlačítka: Uložit / Uložit jako… / Ponechat pro příští spuštění / Zahodit změny / Zrušit**, výchozí Zrušit.
  „Zahodit změny“ místo „Zapomenout“ z plánu: jasně říká, co se zahodí. Uložit má bublinu „Zapíše změny do
  machines/…“: dodané sestavy se přepisují (kopie do „Moje sestavy“ je v etapě 8).
- **Po přepnutí sestavy zvenku** dotaz říká „Mezitím se spustila jiná sestava (načtený stav nebo MCP)“. Dřív vinil
  MCP i u načteného stavu (hodnocení UX).

### Ponechat pro příští spuštění

- **Jen u sestavy se souborem.** Nová neuložená sestava nemá k čemu změny vztáhnout, nabízí se Uložit jako….
- **Změny jdou do nastavení** (`kept_machines`: soubor → `{ base, cfg }`), soubor sestavy zůstane nedotčený.
  Důvod: dotaz přichází hlavně při ukončení programu, v paměti by se změny ztratily.
- **Celý popis i se základem, ne rozdíl:** rozdíl podle indexů karet by po změně souboru trefil jinou kartu. Se
  základem se pozná, že se soubor změnil (nová verze dodané sestavy). Ponechané změny se pak zapomenou a **řekne to
  okno Chyba** (po oponentuře, dřív jen řádek v Logu): uživatel přišel o svou práci, má to vidět.
- **Základ je soubor na disku v okamžiku Ponechat** (po oponentuře), ne popis v editoru. Důvod: po načtení stavu je
  v editoru popis ze stavu a další start by změny potichu zahodil jako „soubor se změnil“.
- **Platí pro každé další spuštění téhož souboru z GUI** (výběr sestav i Restartovat, Poslední sestavy, Spustit ze
  souboru, sestava minulé relace při startu programu) a při otevření v konfigurátoru, dokud je někdo neuloží nebo
  nevrátí. MCP a `sapiemu-cli` je nepoužívají: skripty a testy mají dostat soubor, jak je. Název tlačítka zůstal
  podle plánu, bublina říká „při každém spuštění … dokud je neuložíte nebo v konfigurátoru nestisknete Vrátit
  změny“.
- **Ať o nich uživatel ví i mimo konfigurátor** (po oponentuře, dřív platily potichu): každý start s nimi ohlásí
  stavový řádek („Běží se změnami ponechanými z konfigurátoru, v souboru sestavy nejsou…“, 7 s a Log), výběr sestav
  dá k sestavě `*` a pod popis větu, jak je zapomenout nebo zapsat, Nahlásit chybu je napíše u sestavy. Hlášení je
  bez jména sestavy: stavový řádek ukáže upozornění, jen když se vejde, a s celým jménem ZPS-1 se při 1600 bodech
  nevešlo.
- **Konfigurátor je dál ukazuje jako neuložené** („neuložené změny, ponechané pro příští spuštění“), aby šly kdykoli
  uložit nebo vrátit. Dotaz se na ně znovu neptá, dokud nepřibude další změna: rozhodnutí už padlo, jinak by se ptal
  při každém konci.
- **Zapomenou je** Uložit (i Uložit jako…, změny jsou pak v souboru), Vrátit změny a Zahodit změny.
- **S chybou v sestavě Ponechat nejde** (po oponentuře): tlačítko je šedé s bublinou proč. Další start by selhal
  a zapomněl by všechny ponechané změny, i ty dobré. Chyby počítá jedna funkce `editor_problems` pro konfigurátor
  i dotaz, jen po změně. Když start přesto selže (chybí médium), sestava se spustí podle souboru, změny se zapomenou
  a okno Chyba to řekne. Jinak by se chyba opakovala při každém spuštění.
- **Klíč ponechaných změn se počítá jednou na cestu** (`kept_key` s malou mapou, `kept_entry` bez práce, když nic
  ponechané není; po oponentuře). Důvod: `data_relative` volá třikrát `weakly_canonical` a konfigurátor i výběr
  sestav se ptají v každém snímku, na síťovém disku je to znát.

### Verze ve stavu

- **Co bylo:** hlavička `SAPISTAT` s formátem 1. Jiný formát se odmítal před `load_config`, jen anglicky a bez
  jména verze. Formát se ale od první alfy nezvedl, i když se data karet měnila. Stav starší alfy prošel hlavičkou
  a spadl až v kartách, kdy běžící sestava už byla pryč a zbyl nový stroj po resetu.
- **Formát 2** (`STATE_FORMAT`, `STATE_FORMAT_MIN` v `src/core/machine.h`): za formátem je verze SAPIemu, která stav
  uložila. Začátek hlavičky (`SAPISTAT`, formát, verze) zůstane i v dalších formátech, takže každá verze řekne,
  která stav uložila, i když ho neumí přečíst. `STATE_FORMAT_MIN` = 1: stavy alf se dál čtou. Alfy do 0.6.0 stav
  této verze odmítnou svou hláškou, ale také dřív, než se běžící sestavy dotknou.
- **Jiný formát se odmítne před `load_config`** se srozumitelnou hláškou: GUI česky („Uložila ho novější verze
  SAPIemu (0.9.0). Tato verze (0.7.0-alpha) ho neumí přečíst. Sestava běží dál, jak běžela.“), MCP a cli anglicky
  z jádra. Důvod: chyby jádra zůstávají anglicky pro skripty, GUI je překládá (`state_load_error`).
- **Návrat běžící sestavy,** když stav nesedí až v kartách: `EmuHost::load_state` si stav běžící sestavy uloží do
  paměti a po chybě ji z něj postaví znovu (paměti, obrazovka, breakpointy, watchpointy, pauza). Vypnutý stroj
  zůstane vypnutý. Hodnocení UX navrhovalo postavit nový stroj vedle starého (v plánu etapa 8 „starý stroj vypnout
  až po postavení nového“). Návrat je levnější a nepotřebuje dva stroje se stejnými soubory médií naráz. Zmizí tím
  i trvalý výpadek napájení HRK-1 po nepovedeném načtení stavu. Přepnutí sestavy souborem (ne stavem) se nemění.
  Důvod celku: testeři mají stavy alf a mají dostat srozumitelnou odpověď, ne přijít o rozdělanou práci.
- **Pravda o běžící sestavě** (po oponentuře): `load_state` vrací v `back_out` (`StateBack` v `src/core/host.h`),
  co se s ní stalo: beze změny nebo vrácená („Sestava běží dál, jak běžela.“), postavená znovu z popisu, když se její
  stav předem nepovedl uložit („vypnula se a zapnula, obsah její paměti je ztracený“), nebo nevrácená a resetovaná
  („proběhl reset“, důvod v Logu). GUI i MCP podle toho volí větu. Dřív GUI psalo „běží dál, jak běžela“ i tam, kde
  to neplatilo. Výstupní parametr, ne člen třídy: volání zůstane bezpečné pro vlákna.
- **Česky napřed, anglický důvod jádra na konec** za „Podrobnosti:“, když GUI ví, že stav uložila jiná verze („Stav
  uložila jiná verze SAPIemu (starší alfa), toto je verze 0.7.0-alpha.“; po oponentuře).
- **Stavy alf od 0.3.0 se načtou.** Alfy 0.1.0 a 0.2.0 mají také formát 1, ale `state()` klávesnice a DSM-1V se
  2026-10-06 změnilo. Takový stav selže v kartách, GUI ho odmítne jako „starší alfu“ a sestava zůstane. Formát se
  kvůli tomu dodatečně nezvyšuje, ty alfy jsou venku. Říká to `docs/stav.md` a text pro testery, aby to testeři
  nehlásili jako chybu.
- **Pravidlo pro dál:** změna dat `state()` karty = `STATE_FORMAT` + 1; když starý blok nejde přečíst, posunout
  i `STATE_FORMAT_MIN` (`docs/stav.md`).

### Pro testery

- **Anglické `docs/getting-started.md`** (v zipu `GETTING-STARTED.md` vedle `PRVNI-KROKY.md`): krátké, ne překlad
  Prvních kroků. Důvod: podrobnosti jsou v anglických průvodcích sestavami v programu, tabulka všech 60 sestav by se
  v angličtině jen přepisovala. Názvy sestav a menu jsou přesně podle anglického GUI (`_vyber.json`, klíče `tr()`).
- **Nápověda → První kroky s anglickým rozhraním otevře `docs/getting-started.md`** (když v datech je), jinak
  `prvni-kroky.md`. Důvod: anglický tester by z menu dostal český text. Okno nápovědy v programu je v etapě 8.
- **README:** věta o MCP pryč z prvního českého odstavce i z anglického shrnutí. MCP zůstává v „Co umí“ a „Síť
  a soukromí“. Anglické shrnutí odkazuje na `docs/getting-started.md`. Sekce „Pro vývojáře“, zkrácení README
  a odkazy na `docs/vydani/sestavy.md` a `docs/SOUHRN.md`, které veřejný zip vynechá (na GitHubu fungují), čekají
  na přestavbu README v etapě 9 (hodnocení UX, kapitola 6).
- **`docs/vydani/beta-testeri.md`:** text pro autora k rozeslání se zástupnými místy pro verzi a termín. Pět úkolů
  podle hodnocení UX (bod 9), čtyři otázky (co mátlo, kde se zasekli, co vyhodit, co je SAPIemu jednou větou), jak
  hlásit, známá omezení. Je v `docs/vydani/`, takže do veřejného zipu nejde (`DEV_DOCS`).
  - Úkoly: ZPS-1 a BASIC, APS-5 DEMO, hra, soubor v CP/M, konfigurátor.
  - **CP/M na ZPS-3 s CP/M 64K Tesla IMA** (`MBASIC`, `SAVE "POKUS"`, nové spuštění emulátoru, `DIR B:`). Důvod:
    historická sestava ze Základu, disketa B: jde přes pracovní kopii, takže úkol zkouší i průběžné ukládání médií.
    Ověřeno `sapiemu-cli` na kopiích obrazů disket mimo `work/`.
  - **Konfigurátor na ZPS-1:** AND-1 (X3) Generátor znaků → MHB 2502 azbuka. Důvod: změna je hned vidět a konec
    programu ukáže nový dotaz se seznamem změn (ověřeno snímkem s `--ui-set` a `--ui-exit`). Jak dostat původní
    ZPS-1 zpátky, je až za volbou (po oponentuře), aby úkol dál zkoušel, čemu tester z dotazu rozumí.
  - Po pádu poslat `sapiemu.prev.log`. Kdo měl alfu, dozví se, že F9 už není reset.
- **SHA-256 k zipu:** `package.py` zapíše `<zip>.sha256` ve formátu `sha256sum` (`<hex>  <jméno>`, LF) a vypíše ho,
  na Windows ho ověří `certutil`. Zapíše ho až po kontrolách (veřejný zip bez poznámek vývoje, MikroMon z rozbaleného
  zipu), `.sha256` staršího zipu na začátku smaže a při chybě vypíše „NOT READY: do not publish …“ (po oponentuře,
  dřív hned po zabalení). Důvod: zip, který neprošel, nesmí mít vedle sebe platný součet a vypadat hotově.
- **`package.py` odmítne běh, když první kroky nejsou v gitu** (po oponentuře). Důvod: `docs/` bere z gitu, první
  kroky z disku. Nepřidaný `docs/getting-started.md` by byl nahoře v zipu, ale chyběl by v `docs/` a anglická
  Nápověda → První kroky by otevřela český soubor.
- **„Testovací verze“** v Prvních krocích místo „Verze alfa“ (po oponentuře). Důvod: jméno verze rozhodne autor,
  text sedí v obou případech.

### Snímky GUI a testy

- Nové volby snímků (`docs/sestavy.md`): `--ui-report`, `--ui-debugger`, `--ui-menu JMÉNO`, `--ui-key KLÁVESA`
  (přes frontu událostí SDL, opakovatelné), `--ui-mouse X,Y`, `--ui-exit` (Soubor → Konec, s `--ui-set` ukáže
  dotaz) a `--ui-state` se stavem, který nejde načíst, ukáže okno Chyba. Důvod: menu, klávesy a dotazy by snímek
  jinak neukázal.
- Testy: `test_machine_cfg` (`test_changes`: změny česky i anglicky, jiný zápis stejné hodnoty není změna),
  `test_state` (`state_formats`: verze v hlavičce, odmítnutý formát bez dotyku sestavy, formát 1, useknutý stav
  vrátí sestavu, `back_out`). Celá debug sada prošla (59/59).

### Na rozhodnutí autora

Rozhodnuto 2026-10-29 (autor), podrobně v oddílu „2026-10-29 – ráno: připomínky autora“:
F9 = breakpoint zůstává, verze `0.7.0-beta`, testeři hlásí odpovědí na zprávu a Issues přijdou se zveřejněním
repa (věta „do zveřejnění …“ sedí).

- **F9 = breakpoint, reset jen F12** (rozhodl vedoucí workflow). Doporučení: nechat. Možnost z hodnocení UX:
  v debug buildu F9 bez otevřeného debuggeru = reset (pod Visual Studiem je teď reset jen z menu nebo tlačítkem
  v debuggeru).
- **Jméno verze bety:** `0.7.0-beta`, nebo dál `0.7.0-alpha` (`SAPIEMU_VERSION_SUFFIX` v `CMakeLists.txt`, jméno
  poznámek k vydání, „alfa“ v README).
- **Kam testeři hlásí:** odpovědí na zprávu, se kterou betu dostanou (tak to teď říkají texty), nebo je přidat jako
  spolupracovníky soukromého repa, aby fungovaly Issues. Sedí věta „do zveřejnění cestou, kterou jste program
  dostali, potom přes GitHub Issues“?
- Před rozesláním doplnit v `beta-testeri.md` verzi a termín a přidat Liborovi a Petrovi žádost o souhlasy
  (`docs/vydani/souhlasy.md`).
- **Beta zip** (`package.py --public`) až na autorův pokyn. Před ním přidat do gitu `docs/getting-started.md`
  a `docs/vydani/beta-testeri.md` (`package.py` jinak odmítne).

### Ruční zkouška autora a známá omezení

Snímek skrytého okna tyto věci neukáže:

- Ctrl+Shift+C a vložení do Poznámkového bloku (řádky, čeština, rámečky); Ctrl+Shift+C a Ctrl+Shift+V v sériovém
  terminálu.
- F9 při ladění: myš na řádku disassembleru a F9, myš mimo disassembler (PC), F9 bez debuggeru (jen upozornění).
- Enter a Esc v oknech Neuložené změny, Nahrát program, Nahrát binární soubor, Program na kazetu a SYSGEN RKD-1
  (snímek je neumí otevřít; stejný kód je ověřený v okně Chyba a v otázce APS-5).
- Ponechat pro příští spuštění: vložit kazetu nebo přepnout TURBO, Konec → Ponechat, spustit znovu (kazeta je
  vložená, hlášení ve stavovém řádku, `*` ve výběru sestav, Konec se už neptá), pak Vrátit změny; šedé Ponechat
  a jeho bublina.
- Načíst stav z alfy 0.6.0 (formát 1) v sestavě, kterou měnila etapa 4 nebo 5.
- Nahlásit chybu: Otevřít složku logu, Kopírovat diagnostiku do zprávy, po pádu řádek Předchozí log.
- Nápověda → První kroky s anglickým rozhraním (otevře vnější program).

Známá omezení:

- Cesty `STATE_BACK_RESTARTED` a `STATE_BACK_RESET` se těžko vyvolají a vyzkoušené nejsou, stejně jako skutečný
  pád se `sapiemu.prev.log`.
- `.sha256` je ověřený jen na malém souboru, zip se nestavěl.
- Stavy z alf 0.1.0 a 0.2.0 se nenačtou (výše).

## 2026-10-29 – etapa 7 plánu vzhledu: přístupnost a jazyky

Etapu udělal workflow (2 agenti: jazyky, přístupnost; oponentura; opravy nálezů), autor spal a rozhodnutí nechal
na Claudovi. Chování skutečného SAPI-1 se nemění. Pravidlo pro jazyky: co je natištěné na skutečném panelu nebo
přístroji a co vypisují emulované programy, zůstává česky jako na skutečném stroji. Překládá se rozhraní PC kolem
toho, u potisku bublinou nebo legendou vedle. Změny po oponentuře jsou označené „po kontrole“. Co má autor
rozhodnout a vyzkoušet ručně, je na konci oddílu.

### Jazyky

- **Stav APS-5 ve stavovém řádku:** HRK-1 vrací celé anglické texty („APS-5 key 2, FAULT“) a lišta je překládá přes
  `tr()` („APS-5 klíč 2, PORUCHA“). Jsou to čtyři pevné texty místo skládání, aby je hlídal test klíčů. MCP
  (`keyboard_status`) je teď anglicky jako zbytek MCP. Pole a změnu popisuje `docs/mcp.md` u `machine_info`
  (doplněno po kontrole: nic v repu ho nečte, ale programy přes MCP by mohly).
- **Pece:** výchozí jména „Pec 1“ až „Pec 8“ jsou potisk provozu, na peci zůstávají česky jako panel APS-5 a texty
  firmwaru. Legenda grafu a bublina pece patří k rozhraní PC, anglicky tedy „Furnace 1“. Jméno ze sestavy
  (`furnaces[].name`) se nepřekládá. Legenda se zalamuje u pravého okraje grafu, protože anglická jména jsou delší.
  Dvě jména jedné pece v anglickém okně (štítek „Pec 1“, legenda „Furnace 1“) jsou záměr, stejně jako u IMS-2:
  potisk česky, překlad hned vedle v rozhraní (legenda, bublina pece). Náhled APS-5 díky tomu nezávisí na jazyce.
  Kontrola navrhovala přeložit i štítek, ale pak by štítek nebyl potiskem a náhled by v angličtině ukazoval
  „Pec 1“ z českého snímku. V legendě a bublině grafu má číslo jen pec s vlastním jménem („3 Kalicí pec“),
  výchozí ho má ve jméně („Pec 3“, dřív „3  Pec 3“).
- **Filtry dialogů** (`media.cpp`): anglické klíče, `tr()` při otevření dialogu. SDL čte filtry až do zavření
  dialogu, proto je pole statické a texty jsou z tabulky (platí stále). „*“ zůstává.
- **Hlášení mapy:** zůstává jeden způsob jako u ostatních problémů sestavy: jádro dává `en` i `cs`, GUI vybírá
  `loc()`. Do českého textu dává jádro názvy funkcí přes překladač z GUI (`machine_map_problems(m, cs_what)`
  s `tr_lang`), takže hlášení zní „RAM-1V registr MAP“ místo „RAM-1V MAP register“. Přibylo 71 českých názvů funkcí
  karet, platí i v tabulce mapy. Technické zkratky (UART, VRAM, 8255 PC, SRAM A, RTC…) zůstávají.
- **`loc(en, cs)` s doslovnými texty** v konfigurátoru je převedené na `tr()` (12 míst). `loc()` zůstává jen pro data
  se dvěma jazyky (katalog karet, problémy sestavy), takže všechny texty GUI hlídá test. Prázdný slot má klíč
  „(no card)“ (česky dál „(prázdný)“), protože „(empty)“ je „(prázdná)“ u mechaniky.
- **Název sestavy** v titulku okna OS, v Soubor → Poslední sestavy, v titulku konfigurátoru a v otázce Neuložené
  změny bere `machine_title()` z rejstříku (`title_en` v angličtině), ale jen když jméno odpovídá souboru.
  Sestava přejmenovaná v konfigurátoru ukáže své jméno. Titulek okna se mění i při přepnutí jazyka. Sestavy mimo
  rejstřík ukazují jméno ze `.sapi` (data).
  - Po kontrole má `title_en` v rejstříku každá sestava, jejíž jméno má česká slova: přibylo 32 anglických titulků
    (varianty ZPS, CP/M s ECMONem, CP/M J. Bíby, „Libor at home“, „PMD-85 graphics“). Je to jen překlad jména ze
    `.sapi`, technické části zůstávají. Osm sestav s čistě technickým jménem („ZPS-3 CP/M 48K (MOR48A, RPD-1)“)
    titulek nepotřebuje. V anglickém výběru sestav je tak česky jen jméno ze `.sapi` pod titulkem.
  - Cesta z `--machine` se převádí na tvar v datech (`data_relative(data_path())`, „machines/x.sapi“), takže
    sestavu v rejstříku a v Posledních sestavách najde i zadání se zpětným lomítkem nebo celou cestou.
- **IMS-2:** potisk přístrojů je česky i v anglickém GUI, jak je na přístroji a v `docs/navod-dps1.md` (OCHRANA,
  NAPĚTÍ, PROUD, SÍŤ, MĚŘENÍ, „Stabilizovaný zdroj 2 x 30 V / 1 A“). V angličtině má nad textem anglickou bublinu.
  České GUI bubliny nad potiskem nemá, potisk se čte přímo. Bubliny ovládacích prvků jsou v obou jazycích jako dřív.
  Výběr zátěže je široký podle nejdelší volby (byl uříznutý) a zmizel překryv PROTECTION a MAINS.
- **Test `test_i18n_keys`:** čte `src/` jako C++ (komentáře, znakové literály, raw stringy, spojené literály,
  escape sekvence). Chybějící záznam: doslovný klíč `tr()`, `tr_id()`, `tr_lang()`, `tx()` (Nahlásit chybu)
  a `begin_menu()` ve `src/frontend` bez českého překladu. Nepoužitý záznam: text, který není literálem nikde
  v `src/` (klíče chodí i z jádra: názvy funkcí karet, kroky DEMA APS-5). Klíče předávané proměnnou (tabulky
  v `help.cpp`, texty jádra) na chybějící záznam nekontroluje. Výjimky s důvodem jsou v seznamu v testu (zatím
  prázdném). Test našel 14 mrtvých záznamů a ty jsou pryč, s nimi i 5 záznamů potisku IMS-2.
- **Drobnosti:** „chyba Windows“ při kopírování snímku, „kontrolní součet“ v XMODEM.
- **Záměrně nepřeloženo:**
  - druhy breakpointů v debuggeru (exec, mem read…), technické;
  - jména souborů a složek (`kresba_`, `snimky`, `videa`);
  - chybové texty jádra (`err` z `configure()`, chyba DEMA APS-5 „no "…" on the screen“);
  - podtitulek ve výběru sestav (jméno ze `.sapi`);
  - „Jazyk / Language“ v menu Zobrazení, je dvojjazyčné schválně (najde ho i ten, kdo druhému jazyku nerozumí).

### Přístupnost

- **Stav nejen barvou (WCAG 1.4.1), po místech:**
  - Média, magnetofon DSM-1: rozsvícené tlačítko nese značku režimu (▶ PŘEHRÁT, ● NAHRÁT, ■ STOP), barva zůstává.
    Značka jen u rozsvíceného tlačítka, takže sama říká, které svítí. ID tlačítek se nemění (`###PLAY`).
  - Graf APS-5: na konci každé čáry je číslo pece v rámečku její barvy. Stejné teploty dají čísla vedle sebe
    (1 vlevo od 2), ne přes sebe. Pod myší svislá čára a bublina s teplotami všech pecí v tom čase. Pec v legendě
    nebo její číslo pod myší kreslí svou čáru navrch a tlustší. Legenda u pece s vlastním jménem přidá číslo.
    Popisek osy „teď“ je „0 min“ jako ostatní značky („-15 min“), takže nezávisí na jazyce.
  - Kontrolky nakresleného hardwaru se liší jasem, ne jen odstínem, proto stačí bublina se stavem (svítí, nesvítí)
    přes `lamp_tooltip()`: APS-5 porucha a napájení (v angličtině i s významem potisku), LO1 a LO2 (do bubliny
    výstupů), RMP-1 (PON…WPT), plotter AR 1990 a XY 4130, IMS-2 (všechny kontrolky přístrojů i monitoru sběrnice).
    Topení pece má stav už v bublině pece, kontrolky rámu ARB-1 jen napájení, ty zůstaly.
  - Konfigurátor: problémy sestavy mají ✖ (chyba) nebo ⚠ (varování), v mapě portů a paměti má sdílený zápis slovo
    „SDÍLENÝ ZÁPIS“ jako kolize „KOLIZE“.
  - Stavový řádek už slova a značky má (✖ Chyby: N, ● se změnou sestavy, LED SHIFT/ERR), Log má „error:“ a „warn:“,
    běžící sestava „(běží)“. Hlášky v červené a žluté jinde jsou celé věty, beze změny.
- **Rozměry v jednotkách písma:** debugger (pole adres 4,4 řádku, disassembler 21 řádků, odsazení registrů podle
  „AF' “), paměť, terminál (port TCP a COM, výběr emulace a protokolu podle nejdelší volby `combo_width()`), Média
  (průběh pásky, obrazy disků 16 řádků). Ostatní čísla v pixelech jsou kreslení hardwaru (tloušťky čar).
- **Upozornění na blikání:** průvodce `sapi1v_hry` (česky i anglicky) má před dema větu, že dema rychle mění obraz
  a Bad Apple!! místy bliká. Totéž je ve shrnutí sestavy v okně Vybrat sestavu, v `docs/prvni-kroky.md`
  a `docs/getting-started.md`. Ostatní dema (FXS4, Y1STDEMO, obrázky) se nezkoumala snímek po snímku, proto obecná
  věta o demech, ne seznam. Hry (Prince of Persia, BomberNet, Manic Miner) se na blikání nezkoumaly.
- **README:** anglická věta v úvodu a sekce Přístupnost: rozhraní kreslí Dear ImGui a čtečky obrazovky ho nevidí,
  textové cesty (Kopírovat text obrazovky, `sapiemu-cli --print-screen`, sériový terminál), klávesnice, stav nejen
  barvou, velikost písma, blikání. Anglicky ještě v `docs/getting-started.md`.
- **Náhledy jen na obraz:** v plánu i hodnocení (5.4) jde o náhledy přístrojů, které byly snímky českého okna.
  Obrazovky z `sapiemu-cli --screenshot` jsou už jen obraz bez rámu, ty zůstaly. Nová volba `--ui-picture`:
  okno přístroje označí svůj nakreslený hardware (`picture_mark()`) a snímek ho ořízne. APS-5 v tom režimu
  vynechá ovládání pod panelem, přepínače simulace pod pecemi, nadpis a legendu grafu; ostatní okna jen označí
  obraz (IMS-2 přístroje s kabely, KZD-1 kazetu, RMP-1 první mechaniku, zapisovače viditelný papír). Přegenerováno
  všech 7 náhledů přístrojů a prohlédnuté. Zůstal potisk („porucha“, „Pec 1“, „OCHRANA“), je součástí kresby.
  - Po kontrole: „jednotka 0“ u RMP-1 není potisk, ale text emulátoru (`tr("unit")`), proto ho náhled vynechá.
    Tlačítka mechaniky mají šířku podle popisku (OFF-LINE bylo uříznuté i v okně), zbytek panelu se dělí rovným dílem.
  - Zapisovače: náhled je kresba s okrajem 10 mm, nejméně tak široká jako vysoká, oříznutá na viditelný papír.
    Znaková sada XY 4130 (asi 93 × 58 mm) byla na papíře 230 mm malá a náhled byl z 80 % prázdný. Okno XY 4130 je
    v náhledu 1000 bodů široké (`WINDOW_SIZE`), takže kresba vyjde těsně pod 480 bodů bez zmenšení. Náhled
    plotteru AR 1990 je teď těsně kolem rámečku ukázky, Minigraf zůstal stejný (kresba zabírá celou šířku).
- **Navigace klávesnicí (`NavEnableKeyboard`) zůstává zapnutá:** Tab a šipky v dialozích, Nastavení,
  konfigurátoru i přístrojích, mezerník a Enter stisknou prvek v rámečku. Okna, kam jdou klávesy SAPI, mají
  `ImGuiWindowFlags_NoNavInputs` (displeje, celá obrazovka, sériový terminál i jeho text), takže tam ImGui na šipky,
  mezerník, Enter, Esc ani Alt nereaguje (Alt by jinak přepínal do vrstvy menu). `handle_key` posílá klávesy podle
  fokusu, ne podle ImGui, takže jim navigace nic nebere. Ověřeno snímky: Tab, šipky a Enter na obrazovce ZPS-1
  došly do BASICu, okno zůstalo aktivní bez kurzoru navigace; Tab v Nastavení posouvá rámeček; v otázce na reset
  APS-5 začíná Tab na Zrušit a Tab + Enter nic neresetuje.
  - **Rámeček až po Tab** (po kontrole): `io.ConfigNavCursorVisibleAuto = false`, rámeček ukáže Tab bez Ctrl a Alt
    v okně, které navigaci bere (`nav_cursor_frame()` v `app.cpp`), kliknutí myší ho skryje. Pak ho posouvají
    i šipky. Bez rámečku ImGui mezerníkem ani Enterem nic nestiskne. Důvod: hráč, který klikl do průvodce a hraje
    šipkami a mezerníkem, by jinak rozsvítil rámeček šipkou a mezerníkem stiskl třeba Napsat do SAPI (BADAPPLE
    a Enter) nebo odkaz. Dřív se takové klávesy jen zahodily s hláškou „Psaní teď nejde do SAPI“, a tak to zůstává.
    Stejně se chovají Windows: rámeček fokusu se ukáže až po Tab. Šipka bez rámečku posune neviditelnou pozici
    navigace a do pohybu myši vypne zvýraznění pod myší, nic víc. Alt v okně s tlačítkem zavřít přepne neviditelně
    do vrstvy titulku; Tab tam ukáže rámeček na ×, Esc vrstvu vrátí. Ověřeno snímky: Nastavení s Down, Down, Space
    beze změny a bez rámečku; Tab, Tab rámeček na English; Tab, Down, Space vybere Podle systému.
  - **Enter s navigací:** když navigace ukazuje rámeček na prvku, patří Enter tomu prvku a `popup_enter()` ustoupí
    (`nav_cursor_on_item()`), jinak by Enter stiskl dvě tlačítka. Otázky, kde Enter nedělá nic (reset APS-5, Obnovit
    z originálu, zápis systému RKD-1), a neuložené změny začínají navigaci na Zrušit (`SetItemDefaultFocus`).
  - Vybrat sestavu: Enter spustí sestavu jen s rámečkem na řádku sestavy (nebo bez rámečku, jako dřív). Na
    kategorii, tlačítku nebo odkazu patří Enter jim.
  - Klávesové zkratky mají řádky Tab a šipky, Ctrl+Tab. Sloupec kláves jde po kontrole přes `tr()`: jména kláves
    zůstávají, slovo „arrows“ je česky „šipky“ (tabulku test klíčů nevidí, klíče jsou v proměnné).
- **Texty samotného ImGui** (po kontrole) jsou česky přes `ImGui::LocalizeRegisterEntries` při změně jazyka
  (`imgui_localize()` v `app.cpp`, překlady v `i18n.cpp`, hlídá je test klíčů): seznam oken Ctrl+Tab („(Hlavní
  menu)“, „(Vyskakovací okno)“, „(Bez názvu)“), bublina a menu odkazu („Otevřít „…““, „Kopírovat odkaz“)
  a ukotvená okna („Skrýt lištu záložek“…). Texty tabulek (velikost a pořadí sloupců) ne, žádná tabulka je nemá.
- **Legenda a bublina grafu APS-5** kreslí čáru pece na tmavém štítku barvy grafu (po kontrole): ve světlém vzhledu
  byla pec 8 (230, 230, 230) skoro neviditelná. Barvy čar se nemění, na tmavém grafu jsou dobře rozlišitelné
  a pece mají čísla.

### Na rozhodnutí autora

Rozhodl vedoucí workflow za autora, autor to může změnit (doporučení je první):

- **Štítek pece „Pec 1“** zůstává v anglickém okně česky jako potisk, nebo ho přeložit na „Furnace 1“. Pak by ale
  náhled APS-5 v anglickém výběru sestav ukazoval češtinu, nebo by potřeboval druhý náhled.
- **MCP `keyboard_status` u APS-5** je anglicky jako zbytek MCP, nebo vrátit češtinu.
- **Navigace klávesnicí** zůstává zapnutá s rámečkem až po Tab, nebo ji vypnout úplně. Bez ní jdou dialogy
  a Nastavení jen myší, Enter a Esc v modálních oknech zůstávají.

### Ruční zkouška autora a známá omezení

Snímek skrytého okna tyto věci neukáže:

- Titulek okna Windows s anglickým jménem sestavy (třeba ZPS-2) a jeho změna po přepnutí jazyka.
- Soubor → Poslední sestavy v angličtině (podmenu snímek v jednom kroku neotevře).
- Seznam oken Ctrl+Tab česky (je vidět jen při drženém Ctrl).
- Navigace naživo: sestava Hry a dema s průvodcem, klik do průvodce, šipky a mezerník nic nestisknou (ani Napsat
  do SAPI); Tab v konfigurátoru; Alt v okně, pak Esc zpět. Nastavení a otázka APS-5 jsou ověřené snímky.

Známá omezení:

- Chybové texty jádra (chyby `configure()` karet, chyba DEMA APS-5, část hlášek načtení stavu) jsou i v českém
  rozhraní anglicky. Hodnocení UX je odhaduje na velikost M, proto jsou přesunuté do etapy 8 plánu.
- Čtečky obrazovky rozhraní nevidí (Dear ImGui nemá UI Automation), zbývají textové cesty z README.
- Hry (Prince of Persia, BomberNet, Manic Miner) a ostatní dema se na blikání snímek po snímku nezkoumaly.
- Náhled Minigrafu se novým ořezem nezměnil, kresba asi zabírá celý viditelný papír (neověřeno).
- Testy `release` a překlad ve WSL se nespouštěly, podle plánu jsou před pushem.

## 2026-10-29 – rozložení oken podle autora

Autor ráno potvrdil checkpoint C: **výchozí zůstávají plovoucí okna**, zadokované rozložení je dál volba v Nastavení.
K plovoucím oknům měl čtyři připomínky a se změnami souhlasil. Udělal je workflow (agent, 2 oponenti, opravy).

- **Strop automatického zvětšení displeje** (autor: „v praxi se mi osvědčilo zhruba 200 % (to je moje rozlišení),
  400 % vypadá monstrózně a kostičkovatě“): samo rozmístění dá displeji nejvýš round(2 × měřítko Windows), tedy
  200 % při 100 % a 300 % při 125 a 150 %. Fyzická velikost obrazu tak zůstane zhruba stejná. Když se nevejde,
  zvětšení klesne až na 100 %. Zoom zvolený v menu a uložená místa oken strop nemají.
- **Další displej ne větší než hlavní výstup** (nález oponenta): jeho obraz nejvýš 1,5× širší a vyšší než obraz
  hlavního výstupu, jinak menší zvětšení, nejníž 100 %. U her (Hry a dema, BomberNet, Prince of Persia) zůstane
  hra na CGA-1V 200 % a konzole AND-1V s nabídkou ECMONu jde na 100 %. Bez toho měla konzole 1312 × 1018 bodů a hra
  byla nejmenší okno. DGD-1 vedle AND-1 v MIKOSu si 200 % nechá.
- **Displej s malým okrajem** (autor: „líbí se mi displeje s malým okrajem“): okno displeje, které umístilo
  rozložení, má velikost obrazu ve zvoleném celočíselném zvětšení a 16 bodů okraje, bez černé plochy kolem. Je to
  stejné jako po volbě zvětšení v menu.
- **Konfigurátor ve výchozím rozložení, když je místo** (autor: „kde je na obrazovce místo, je škoda, že není vidět
  Konfigurátor sestavy… pěkné okno, které ukazuje široké možnosti emulátoru“; jeho snímek 2000 × 1128: AND-1
  a DGD-1 po 200 % vlevo pod sebou, průvodce uprostřed nahoře, konfigurátor vpravo přes celou výšku). Má nejnižší
  prioritu: první start a Výchozí rozložení oken ho otevřou jen do volného místa aspoň 44 × 44 výšek písma (704 ×
  704 bodů při 100 %, na tolik potřebuje kresba racku), mimo displeje, průvodce a přístroje. Na notebooku (1280 × 720)
  zůstane zavřený. Nebere klávesnici a zůstane za ostatními okny i za dialogy. Okna otevřená později se mu
  nevyhýbají a mohou ho zakrýt. Dokud v něm uživatel neklikne, nestiskne klávesu nebo si ho nevyžádá z menu, platí
  za otevřený rozložením (`windows.configuration_auto`, i po dalším startu). Příští Výchozí rozložení oken pak
  rozhodne znovu. Chyba Zapnout ho za tu dobu neschová, jde i do okna Chyba.
- **Hledání oken** (autor: „okno se snadno schová za jiné a pak ho hledám, běžný uživatel ani neví, že existuje“):
  - Zobrazení → Okna vypíše otevřená okna, kliknutí dá okno dopředu a s klávesnicí.
  - Položky oken v Zobrazení a Ladění zakryté okno nejdřív dají dopředu a zavřou ho až další kliknutí, když je
    vpředu. Zaškrtnutí dál znamená otevřené.
  - Okno zadokované do plochy doku je zakryté jen jako sbalené nebo nevybraná záložka: dok zůstává vzadu a plovoucí
    okno před ním nejde dát dozadu (jinak by šlo okno z menu jen dávat „dopředu“ a nikdy zavřít).
  - Zobrazení → Přeskládat otevřená okna dá na výchozí místa jen otevřená okna. Zavřená si místa nechají, zvětšení
    zvolené v menu zůstane. V zadokovaném rozložení se dok postaví znovu jako u Výchozího rozložení oken.
  - Ctrl+Tab se nepoužije, Ctrl+klávesy patří SAPI jako řídicí kódy (seznam oken ImGui na Ctrl+Tab od etapy 7
    zůstává).

### Na rozhodnutí autora

- **Konfigurátor při 1920 × 1080:** s průvodcem se u MIKOSu nevejde, spodní polovina obrazovky zůstane volná.
  Vešel by se, kdyby průvodce šel pod hlavní výstup a DGD-1 vpravo od něj (autorovo uspořádání otočené). Doporučení:
  nechat, Full HD je „bez místa“. Druhá možnost je to udělat.
- **Strop i v zadokovaném rozložení:** tam hlavní výstup vyplní střed doku (AND-1 300 % při 1600 × 900). Doporučení:
  nechat, dok řídí velikost (rozhodnutí 2026-10-10). Druhá možnost je zvětšení omezit a nechat černý okraj.
- **Průvodce na malé obrazovce** (1280 × 720 až 1600 × 900, i 1920 × 1080 při 150 %) zůstane pod displeji jako úzký
  pruh. Bylo to tak i před změnou. Doporučení: nechat. Druhá možnost je dát dalšímu displeji menší zvětšení, když
  by průvodce měl méně než 12 výšek písma.

## 2026-10-29 – ráno: připomínky autora

Autor ráno program vyzkoušel a měl šest připomínek: tři k oknům, titulky, média a verzi. Se změnami souhlasil.
Technicky `docs/build.md` (rozmístění oken), pro testery `docs/release-notes/v0.7.0-beta.md`.

1. **Okno Vybrat sestavu větší ve větším okně** (autor: „mohlo by být větší, když je okno emulátoru větší; otevře se
   dost malé s posuvníky, i když by klidně mohlo být větší a přehlednější“): poprvé asi 75 % šířky a 80 % výšky
   plochy, aspoň dosavadních 58 × 38 výšek písma, nejvýš celá plocha, uprostřed jako dosud. Uložená velikost
   vyhrává, kdo má okno uložené z dřívějška, uvidí větší až po Zobrazení → Výchozí rozložení oken. Seznam sestav
   vlevo zůstává 24 výšek písma (dá se rozšířit tažením), náhled nejvýš 35 × 22.
2. **Okno displeje po změně sestavy podle nového obrazu** (autor: „po výběru sestavy se AND-1Z ukázal na 100 % a jeho
   okno bylo mnohem větší než obraz; okno by mělo sedět s malým okrajem jako po volbě 200 % pravým tlačítkem“):
   okno konzole `###screen` a grafická okna `###gfxN` jsou stejná okna pro všechny sestavy. Po změně sestavy si okno
   nechá místo a dostane velikost nového obrazu ve zvětšení, které by mu dalo rozložení (strop, malý okraj, další
   displej nejvýš 1,5× hlavního výstupu), posunuté dovnitř plochy. Okno se zvětšením zvoleným v menu dostane
   velikost nového obrazu v tomto zvětšení (nevejde-li se na plochu, přizpůsobí se oknu). Výjimky:
   - okno, které uživatel v tomto běhu zvětšil tažením (jeho velikost platí; další volba zvětšení, Výchozí
     rozložení oken a Přeskládat otevřená okna to zruší);
   - první sestava po startu programu (velikosti z `imgui.ini` vyhrávají jako dosud);
   - restart stejné sestavy a vypnutí a zapnutí (stejný soubor a stejné karty displejů, nic se nemění);
   - zadokované okno (velikost řídí dok).

   První verze zvolené zvětšení po změně sestavy nedržela (okno zůstalo velké podle staré sestavy, AND-1Z pak
   rozmazaně na 69 % s pruhy); oponentura to našla, opraveno.
3. **Okna mimo plochu zpět dovnitř** (autor souhlasil): okno víc než napůl mimo plochu (po zmenšení okna emulátoru,
   přesunu na menší monitor, uložené místo z větší obrazovky) se vrátí dovnitř při startu a po změně velikosti okna
   emulátoru. Dialog (Vybrat sestavu, Nastavení…) už když je venku jen částečně: větší uložený výběr sestav by na
   menší ploše skryl křížek. Posune se co nejméně (k nejbližšímu kraji, s mezerou rozložení), velikost zůstane, jen
   když se nevejde, zmenší se. Platí i pro zavřená okna a uložená místa oken, která se teprve otevřou (ne pro
   podřízená okna, třeba seznam ve Vybrat sestavu). Kontroluje se až velikost plochy, která vydrží 3 snímky (ne
   mezikroky ani první snímky před maximalizací okna). Okno napůl venku zůstane, kde je. Víc oken venku na stejné
   straně se sejde u kraje přes sebe. Volné místo se nehledá, posun je předvídatelný a okno jde najít i přes
   Zobrazení → Okna.
4. **Krátké titulky sestav s ECMONem** (autor souhlasil, ve výběru byla dlouhá jména ze `.sapi`, třeba „CP/M 2.2
   z RAM disku + FDC-1 (JPR-1ZI ECMON, DMR-1, AND-1Z-89, ZRMD-1, FDC-1)“): sedm sestav kategorie „CP/M s ECMONem“
   má `title` a `title_en` v `machines/_vyber.json` ve stylu základních sestav: CP/M z RAM disku, CP/M s IDE diskem
   a PMD32-SD, CP/M s disketami 8" (FDC-1), CP/M s disketami 8" (MFC-1A), CP/M s PMD32-SD přes DPP-1, CP/M
   s hodinami RTC-1 a sériovou deskou DSP-1, CP/M EC1045 s disketami 3,5" a 5,25" HD (tabulka
   `docs/vydani/sestavy.md`, 5b). „ECMON“ ani „RAM disk ZRMD-1“ v titulcích není, říká to nadpis kategorie a dlouhé
   jméno pod titulkem. Řadič je v závorce tam, kde odlišuje dvě jinak stejné sestavy (FDC-1 a MFC-1A). Každý titulek
   je ověřený proti kartám sestavy a během bez okna nad vlastní kopií `work/` (menu ECMONu, `DIR` disků, RTCZ,
   `STAT B:` u EC1045). Titulek platí i v titulku okna a v Posledních sestavách (`machine_title()`). Dlouhé jméno
   pod titulkem je v angličtině z nového `name_en` v rejstříku (dřívější anglické titulky s výčtem karet).
5. **Vysunout bez uložení zmizelo** z nabídky „…“ u média v okně Média. Média se od etapy 4 ukládají průběžně (pár
   sekund po posledním zápisu, při pauze, při přepnutí okna), položka by tak zahodila jen posledních pár sekund
   a mátla. Zůstává Vysunout a „…“ → Obnovit z originálu (s potvrzením). Tlačítko „…“ je aktivní jen u média
   z `media/` (jinak by nabízelo jen zašedlou položku). `Card::media_discard` zůstává, používá ho Obnovit
   z originálu. Průběžné ukládání všech médií, i pásek a kazet, autor potvrdil.
6. **Verze 0.7.0-beta** (`SAPIEMU_VERSION_SUFFIX` = `-beta`, poznámky `docs/release-notes/v0.7.0-beta.md` s úvodem,
   co beta je, „betaverze“ v README, Prvních krocích a Getting started, verze v textu pro testery). „Alfa“ zůstává
   tam, kde jde o starší verze (stavy z alf, „Kdo měl alfu“, F9 v alfách). **F9 = breakpoint** (reset jen F12)
   autor potvrdil.

**Cesta vydání** (autor), zapsaná i v `docs/vydani/plan-vzhled.md` pod „→ BETA zip“:

1. 0.7.0-beta jen třem testerům alf (Libor, Petr, Michal): zpětná vazba, souhlasy (`docs/vydani/souhlasy.md`)
   a aktuální verze Liborova softwaru; hlásí odpovědí na zprávu;
2. úprava podle nich;
3. 0.7.1-beta: nejdřív hotový zip v Releases na GitHubu, pak repozitář veřejně s GitHub Issues pro hlášení chyb,
   nakonec oznámení na fóru OldComp.cz.

**Snímky:** nová volba `--ui-switch SESTAVA` (víc po sobě) spustí sestavu za běhu jako Spustit v okně Vybrat
sestavu, aby šla změna sestavy ověřit snímkem.

### Na rozhodnutí autora

- **Displej víc než z poloviny uvnitř zůstane uříznutý** (pravidlo „víc než napůl venku“ z bodu 3): s místy
  uloženými na větší obrazovce přijde CGA-1V her při 1600 × 900 o spodních 138 bodů obrazu. Doporučení: okna
  displejů vracet celá jako dialogy, když se na plochu vejdou (uříznutý obraz je k ničemu). Druhá možnost je nechat.
- **Obsah většího okna Vybrat sestavu:** náhled a seznam mají pevnou velikost, větší okno přidá hlavně prázdné místo
  vpravo a seznam se při 2252 × 1198 pořád posouvá. Doporučení: náhled zvětšovat s oknem (do poloviny šířky
  podrobností, celočíselně, náhledy mají 288–480 bodů) a seznam rozšířit podle šířky okna. Druhá možnost je nechat.

### Ruční zkouška autora

- Jednou Zobrazení → Výchozí rozložení oken (jinak zůstane velikost Vybrat sestavu uložená dnes ráno), pak Soubor →
  Vybrat sestavu….
- Zmenšit okno emulátoru tažením za okraj (okna vpravo dole se vrátí dovnitř, jakmile se velikost ustálí), přesun
  na menší monitor.
- Změnit sestavu bez zásahu (okno podle obrazu), po volbě 100 % v menu (zůstane 100 %, okno podle nového obrazu)
  a po ručním zvětšení okna displeje (velikost zůstane).

## 2026-10-29 – okna displejů podle obrazu, co uživatel nastaví, zůstane

Autor po startu viděl AND-1 na 200 % v okně mnohem širším než obraz a konfigurátor se neotevřel, i když `imgui.ini`
a `sapiemu-settings.json` před startem smazal.

- **Příčina:** program při chybějícím `imgui.ini` zkopíroval rozložení starších verzí z `%APPDATA%\SAPIemu\SAPIemu\imgui.ini`
  (z 2026-10-03, okno obrazovky 816 × 549). Smazané rozložení se tak pokaždé vrátilo. **Kopírování je pryč:** verze
  před 2026-10-03 měl jen autor a staré rozložení se k novému vzhledu stejně nehodí. Soubor v AppData program
  nečte, autor ho může smazat (nečte ho jen tehdy, když jde o složku nastavení, protože vedle exe nejde zapisovat).
- **Co uživatel nastaví, zůstane** (autor: „když si to někdo nějak nastaví, tak bych mu to dovolil a tohle už se
  zapamatuje“): nastavení ukládá u každého okna displeje zvětšení zvolené v menu a ručně zvětšené okno. Okno se
  zvoleným zvětšením má po startu přesně velikost obrazu v něm, ručně zvětšené okno si velikost nechá i po změně
  sestavy. Ručně roztažené okno se k obrazu **nedorovnává** (autor nechce uživatele omezovat).
- **Displej částečně mimo plochu** (uříznutý obraz) se po startu a po zmenšení okna vrátí celý dovnitř, když se
  vejde (autor schválil). Ostatní okna až víc než napůl venku, dialogy při jakékoli části venku.
- **Dialog při otevření** (autor: dolní hrana Vybrat sestavu mimo hlavní okno, nešla chytit): částečně mimo plochu
  se vrátí dovnitř i při každém otevření, nejen po změně velikosti plochy.
- **Náhled ve výběru sestav** se dál nezvětšuje (autor: je dobré, když jsou vidět i položky pod náhledem).

## 2026-10-29 – etapa 8 plánu vzhledu: menu, nastavení a chování

Autor etapu pustil („jo, za mně dobrý, pokračuj další etapou“). Udělal ji workflow (5 agentů: menu, chování,
nápověda, nástroje, Moje sestavy; oponentura; opravy nálezů), rozhodnutí v ní nechal autor na něm. **Moje sestavy
rozhodl autor:** build pro vydání uloží dodanou sestavu jako kopii, ladicí build ji přepíše. Chování skutečného
SAPI-1 se nemění. Změny po oponentuře jsou označené „po kontrole“. Technicky `docs/build.md`, pro uživatele
`docs/prvni-kroky.md`, `docs/getting-started.md` a `docs/release-notes/v0.7.0-beta.md`. Co má autor rozhodnout
a vyzkoušet, je na konci oddílu.

### Menu (hodnocení 5.1)

- **Stroj → Média…** otevře stejné okno jako Zobrazení → Média (`window_item`, klíč `Media`): otevření, přenesení
  dopředu, zavření i fajfka se v obou menu chovají stejně.
- **Zobrazení → Obrazovky** vypíše okna displejů běžící sestavy podle karet (konzole první), fajfka = ukázané. Skryté
  okno se nekreslí, sestava běží dál.
  - Neukládá se. Jiná sestava (jiný soubor nebo jiné karty displejů, `displays_sig`) ukáže všechny, restart,
    Ctrl+F12 a Vypnout a zapnout téže sestavy je nechají skryté (co uživatel nastaví, zůstane).
  - Displej použitý naposledy se při skrytí přesune na ukázaný (hlavní výstup, jinak konzole, jinak první grafický).
    Když měl klávesnici, jde s ním i ona. Po kontrole: kliknutí v menu drží fokus menu, proto rozhoduje stav
    posledního snímku bez otevřeného menu (`App::screen_keys`). Když klávesnici mělo jiné okno, třeba Média, zůstane
    tam. Znovu ukázaný displej přijde dopředu s klávesnicí.
  - Celá obrazovka ukáže i skrytý displej vybraný v jejím menu, menu Obrazovky se tím nemění. Po odchodu z celé
    obrazovky jde displej použitý naposledy zase na ukázaný.
- **Zobrazení → Otevřít složku snímků a videí** je podnabídka: Snímky obrazovky (`work/snimky`) a Videa
  (`work/videa`), krátká cesta vpravo, celá v bublině. Plán měl jednu položku, ale jsou to dvě složky: jedna položka
  by otevřela jen jednu z nich a `work/` má dvacet dalších složek. Pomocník `open_folder()` (`help.cpp`) složku
  nejdřív vytvoří, takže jde otevřít i před prvním snímkem. Používá ho i Nahlásit chybu → Otevřít složku logu.
- **Okna přístrojů** (Sériový terminál, Tiskárna, Zapisovač, IMS-2, Deska diagnostiky, APS-5) jsou v Zobrazení
  povolená, jen když je sestava má, nebo když je okno otevřené (položka ho zavře, jako od etapy 5). Média jsou
  povolená vždy, jejich okno řekne, že sestava média nemá. Totéž Stroj → Média….
  - Zakázaná položka má zalomenou bublinu (`device_missing`): co chybí, kde je sestava s přístrojem (Soubor → Vybrat
    sestavu → kategorie → titulek, z rejstříku `_vyber.json` v jazyce rozhraní, takže se nerozejde s oknem Vybrat
    sestavu) a u některých přístrojů místo v konfigurátoru. Tiskárna a sériový port mají jen místo v konfigurátoru,
    žádná jedna sestava není spolehlivý příklad.
  - Stejnou větu ukáže okno přístroje v sestavě bez něj: APS-5, IMS-2 (dřív JSON), zapisovač, tiskárna, deska
    diagnostiky, terminál.
  - APS-5 a IMS-2 se rozlišují (`aps5_in_machine`, `ims2_in_machine`). Dřív byly obě položky povolené u každé sestavy
    s technologií.
  - **Vypnutá sestava** (po kontrole: všechny položky byly šedé s nepravdivou bublinou „Tato sestava nemá…“): položky
    se řídí přístroji sestavy, jak naposledy běžela (`machine_devices_seen`), protože Zapnout postaví stejný popis.
    Před první běžící sestavou jsou povolené všechny. Okno přístroje řekne „Sestava je vypnutá (Stroj → Zapnout).“
    Zamítnuté: číst popis JSON (chyběla by mapa karet na přístroje) a povolit vše (šedá položka by lhala obráceně).
- **Pořadí v Zobrazení:** Celočíselné zvětšení, Poměr stran jako na TV a Vyhlazování hned za Celou obrazovkou, za
  oddělovačem skupina Snímek, Kopírovat text obrazovky, Video a složky, oddělovač i za Konfigurátorem sestavy. Mezi
  menu se nic nestěhovalo.
- **Oprava chyby z etapy 5** (`07a3116`): hlavní grafický výstup her (CGA-1V, BGP-1S) se kreslí před konzolí a kód
  konzole mu přepsal příznak „obrazovka má klávesnici“. Psaní do kliknutého CGA-1V se v sestavách her zahodilo
  („Psaní teď nejde do SAPI“) a nešel ani joystick z klávesnice. Příznak teď smaže `draw_screens` a každý displej ho
  jen nastaví. Ověřeno snímkem: se starým řádkem se `3` zahodí, s opravou naběhne CP/M.

### Nastavení po sekcích

- **Jedna stránka s posuvníkem**, popisky ve sloupci a ovládání vedle nich, nadpisy `theme_section`, jak říká plán
  („SeparatorText nebo nadpisy“). Seznam sekcí vlevo jako ve Windows 11 je na rozhodnutí autora.
  - **Obecné:** Jazyk / Language, Velikost písma (hodnocení ji chce u jazyka), Průvodci se otevírají sami, Znovu
    ukazovat skryté průvodce.
  - **Vzhled:** Režim (Podle systému, Světlý, Tmavý) a Okna (Plovoucí, Zadokovaná). Výběr vzhledu jen při víc
    vzhledech. Řádek Okna si nechal jméno, cesta „Zobrazení → Nastavení → Vzhled → Okna“ v dokumentaci platí.
  - **Obraz:** celočíselné zvětšení, poměr TV, vyhlazování (tytéž příznaky jako v menu).
  - **Zvuk:** Zvuk zapnutý, Hlasitost, věta o rychlosti 100 %.
  - **Ovládání:** citlivost myši a „Myš PC jako myš SAPI (jen do ukončení)“ s bublinou, že se neukládá (stejná
    bublina je u položky v menu Stroj). Joystick z klávesnice (Ctrl+F8), který se také neukládá, tam není, plán ho
    nejmenuje.
  - **Záznam:** zvětšený snímek, pole ffmpeg s popiskem a řádkem, zda ffmpeg našel (hledá se jen při otevření okna
    a po změně pole, PATH má mnoho složek), složky snímků a videí s tlačítkem Otevřít složku.
  - **Pokročilé (pro vývojáře):** sbalitelné, server MCP. Sbalené, dokud server není zapnutý nebo nehlásí chybu;
    pak se jednou za běh samo otevře, aby vývojář viděl adresu. Starší soubory nastavení bez klíče `mcp` mají MCP
    zapnutý, takový uživatel ho proto uvidí otevřené.
- Klíče souboru nastavení se nemění.
- Výchozí velikost okna 40 × 56 výšek písma (dřív 34 × 30), nejvýš plocha, při 1280 × 720 se posouvá. Velikost
  uložená v `imgui.ini` vyhrává, novou dá Výchozí rozložení oken.
- Hláška videa bez ffmpeg radí Zobrazení → Nastavení → Záznam místo úpravy souboru nastavení. Stroj → Zvuk, joystick
  a myš → Hlasitost, citlivost myši… dá otevřené Nastavení dopředu.

### Nápověda v programu (hodnocení 5.3, 5.4)

- **Okno Nápověda** (`###Help`, dialog 50 × 44 výšek písma) má dvě záložky místo dvou oken: První kroky
  (`docs/prvni-kroky.md`, v angličtině `docs/getting-started.md`) a Slovníček (nová položka Nápověda → Slovníček,
  `docs/slovnicek.md`, `docs/glossary.md`). Chybí-li anglický soubor, ukáže český; chybí-li první kroky v `docs/`,
  vezme kopii nahoře v zipu (`PRVNI-KROKY.md`, `GETTING-STARTED.md`). Soubory hledá `data_path` jako ostatní data,
  ověřeno nad čistou kopií dat.
  - Kapitoly `##` jsou sbalitelné: v Prvních krocích je otevřená jen první, ve Slovníčku všechny (stránka
    k vyhledávání). Rozbalit vše a Sbalit vše platí pro otevřenou záložku, odkaz se jménem souboru otevře `.md`
    mimo program. Záložka si drží posun i sbalené kapitoly, změna jazyka soubor přečte znovu a začne nahoře.
- **Markdown** z `browser.cpp` je ve dvou nových souborech: `markdown.h/.cpp` (parser bez ImGui, test
  `test_markdown`) a `mdview.cpp` (kreslení). Průvodci sestavami ho používají také.
  - Tučné, `kód` (neproporcionální písmo na podkladu) a odkazy uvnitř zalomeného odstavce kreslí vlastní rozvržení
    po slovech (DrawList, odkazy přes `ItemAdd` a `ButtonBehavior`): ImGui zalamuje jen text jednoho písma a jedné
    barvy.
  - Tabulky jsou rozvržené ručně: tabulky ImGui po prvním vytvoření pevné šířky sloupců nepřevezmou a tady se
    sloupce řídí šířkou okna. Sloupec dostane aspoň své nejdelší slovo (nejvýš 45 % tabulky) a nejvýš celý text na
    jednom řádku, zbytek šířky podle množství textu. Řádek záhlaví je tučně.
  - Krátký `kód` (do 30 bajtů) se na mezerách nezalomí (`10 FOR I=1 TO 5` zůstane celé), delší ano.
  - Odrážky a číslované body s odsazením i vnořené, odsazený odstavec patří k bodu, bloky kódu, vodorovná čára,
    escape, komentáře HTML.
  - Odkazy: http(s) v prohlížeči; soubor relativně ke složce dokumentu, pak ke kořeni dat (otevře ho `open_doc` mimo
    program); `kód` se jménem existujícího `.md` je odkaz také; odkaz na soubor druhé stránky okna přepne záložku.
    Vypadají jako `TextLink` v O programu (barva, podtržení, cíl v bublině), bez šipky ↗. V průvodcích jsou teď
    v textu, dřív byly pod odstavcem. `GETTING-STARTED.md` je v repu jen kód, soubor existuje jen v zipu.
- **Nápověda → Návody k obsluze** (`navod-*.md`) se dál otevírají mimo program: mají obrázky, které okno nekreslí.
  Hned po etapě to autor změnil: okno Nápověda je prohlížeč všech `.md` (oddíl „dokumentace v okně Nápověda“).
- **Bubliny `tip(text)` a `tipf(formát, …)`** (`theme.h`) zalomí text na asi 35 šířek písma (bublina měla až 1470 px
  na řádku). Chovají se jako `SetTooltip` (`BeginTooltipEx` s `OverridePrevious`), podmínku `IsItemHovered()` píše
  volající. Skript nahradil všech 117 volání `ImGui::SetTooltip` v `src/frontend`, `menu_tip` z `app.cpp` a dvě
  ručně zalomené bubliny. Texty ani klíče `tr()` se nezměnily.
- **Slovníček** jsou dva soubory, česky a anglicky, se stejnými čtyřmi oddíly a řádky (pojem, druhý jazyk, význam):
  počítač SAPI-1, programy, média a data, program SAPIemu. Každý se tak čte přirozeně. `test_markdown` hlídá stejné
  oddíly a počty řádků, vyplněné buňky a odkazy všech čtyř dokumentů okna Nápověda. Po kontrole přibyl řádek Moje
  sestavy.
- **Zip:** `package.py` hlídá, že čtyři dokumenty okna Nápověda (`HELP_DOCS`) jsou v gitu a v `docs/` zipu, jinak
  zip není hotový. Do kořene zipu slovníček nejde.

### Debugger a konfigurátor

- **Debugger podle CPU** (`CpuCard::cpu_isa`):
  - 8080 a 8085 se jmény Intel: A F B C D E H L po bajtech vedle SP a PC, INTE. 8085 navíc nedokumentované příznaky
    K a V, IE, SID/SOD a řádek, co teď přečte RIM (nová pole `CpuRegs::rim` a `sod` jen pro čtení, plní je jen
    `Jpr1::get_regs`; RIM sestavené jako v jádře, bez zvláštnosti prvního RIM po TRAP). Debugger `CpuRegs` nejdřív
    nuluje, karty Z80 nová pole neplní.
  - Z80: AF BC DE HL, druhá sada AF'…HL', IX IY SP PC, I, R, IM, IFF1 a IFF2.
  - Příznaky jsou zaškrtávátka s písmenem (stav není jen barvou, etapa 7). Při pauze se mění hned, při běhu jsou
    šedé jako Krok. Pole registrů dál chtějí Enter. Nedokumentované příznaky (Y a X Z80, K a V 8085) mají bublinu.
  - Sloupec značek v disassembleru jako ve Visual Studiu: červená tečka = breakpoint, šipka = PC přes ni
    (`theme().error`, `theme().warn`, obrys barvou textu). Liší se tvarem, barvy řádků zůstaly. Kliknutí kamkoli na
    řádek dál přepíná breakpoint.
  - Paměť: bajt, jehož znak písmo nemá (hlavně semigrafika 80h–BFh za 16bitovými kódy atlasu písma), je „.“ místo
    náhradního znaku. Kontrola je ve frontendu (`mem_char`, `IsGlyphInFont`), ne v `core/charset.cpp`: záleží na
    písmu GUI a jádro na GUI nezávisí. Řídicí kódy 00h–1Fh a 7Fh jsou „.“ jako v klasickém výpisu. Výchozí šířka
    okna Paměť 46 výšek písma (dřív 40), vejde se celý řádek s textem.
  - Mnemoniky Intel (MOV, MVI, LXI…) nejsou: `core/disasm.cpp` má jen mnemoniky Zilog (a jména Intel instrukcí jen
    8085). Odloženo.
- **Port 63h desek V šedě:** zápis, který dvě karty sdílejí podle návrhu, je tabulka v katalogu (`SharedWrite`,
  `card_shared_write`) s páry typů karet a jmen funkcí z dekodérů. Nové pole `CardInfo` by sáhlo na každý `CARD()`.
  - Aby pár sedl jen na správný registr, jmenuje dekodér ZRMD-1/ZRD-1V base+3 „adresa RAM disku A16-A21“, s JP2
    ZRMD-1 „…, MAP1/MAP2“. ZRMD-1, který MAP budí sám, tak zůstává varováním.
  - Záměr je jen tam, kde na adresu píšou na sběrnici právě dvě karty a pár je v katalogu. Rozsah zůstává
    `shared_write` (MCP ukáže i `by_design`), ale `machine_map_problems` ho vynechá: žádné varování v konfigurátoru,
    MCP ani testech. V mapě šedě se slovy „sdílený zápis (záměr)“, bublinou a důvodem pod tabulkou. Dodané sestavy V
    žluté varování nemají, jiné sdílené zápisy zůstávají žluté a kolize červené. Test `test_shared_by_design`.
  - RAM-1V vedle ZRMD-1 bez JP2 je záměr stejně jako se ZRD-1V (stejný port, ZRMD-1 tam bere jen bity 0–5);
    potvrdit níže.
- **Jména souborů místo cest:** pomocník `file_name_text` ukáže jméno a celou absolutní cestu (na Windows se
  zpětnými lomítky) v zalomené bublině. Média (kazety DSM-1, diskety, kazety RKD-1, dřív s celou cestou, pásky RMP-1,
  IDE disk, složka PMD32-SD), výstup Tiskárny a dialogy Nahrát program. Nahradil cesty relativní k datům z etapy 3.
  Obrazy PMD32-SD ve výběru mechaniky jsou relativní k SD kartě, zůstaly.
- **Konfigurátor, pevné záložky:** „Sestava a sběrnice | X7 CGA-1V | Mapa portů a paměti“ s pevnými ID (`###bus`,
  `###card`, `###map`), takže výběr vydrží změnu titulku. Prostřední záložka je vždy: karta ve slotu, „X3
  (prázdný)“, nebo „Karta“ s nápovědou, když slot není vybraný. Nastavení sběrnice už nechce klik na prázdný rám.
  - Klik na slot vybere záložku karty, i z mapy, jako dřív.
  - Jiný slot pravým tlačítkem nebo přetažením karty přepne na kartu jen ze záložky sběrnice, z mapy ne.
  - Klik na prázdný rám dál zruší výběr slotu a na sběrnici přepne jen ze záložky karty.
  - Otevření nebo následování sestavy přepne ze záložky karty na sběrnici, nová prázdná sestava ukáže kartu CPU.
- **Nová ▸** v konfigurátoru: Prázdná sestava, pak podnabídky podle kategorií rejstříku s titulky jako ve Vybrat
  sestavu (kategorie skupiny Ostatní ve vlastní podnabídce), soubor a krátký text v bublině. Dřív asi 60 cest. Sestavy
  mimo rejstřík jsou v Ostatní → Nezařazené a vlastní sestavy, takže jsou dostupné všechny soubory. Rejstřík se čte
  při každém otevření nabídky, nově uložené sestavy v ní jsou. Kód `machine_index_menus` v `browser.cpp`.

### Chování (hodnocení 5.5)

- **Starý stroj se vypne až po postavení nového** (`EmuHost::load_config`, `src/core/host.cpp`):
  - Nejdřív **zkušební sestavení** (`Machine::trial`): nový stroj, kontrola popisu sériových linek
    (`serial_bridges_check`, nic se neotevře), u stavu i načtení stavu. Starý stroj zatím běží nedotčený.
  - Když zkouška selže, běžící sestava zůstane, jak byla: tentýž objekt a generace, paměť, breakpointy, pauza, nic
    neodběhlo, žádné varování výpadku (APS-5 s HRK-1 už neskončí v nekonečném výpadku), spojení TCP, COM i terminálu
    zůstanou. Okno Chyba připíše „Sestava běží dál, jak běžela.“ Platí pro Vybrat sestavu, Poslední sestavy, Spustit
    ze souboru, restart z konfigurátoru, Ctrl+F12 i MCP.
  - Po dobré zkoušce se starý stroj vypne jako dřív (spojení uvolní, varování výpadku, média a zálohované paměti se
    uloží) a nový se postaví **znovu** ze souborů, jak jsou teď. „Postavit nový napřed“ v jednom kroku nejde: nový
    stroj musí číst RAM disky, zálohované paměti a záznam výpadku APS-5 až po tom, co je starý zapsal (stejná sestava
    po Ctrl+F12 nebo restartu). Výhradní prostředky PC (porty TCP, COM) bere až skutečná stavba, takže povedené
    přepnutí se chová přesně jako dřív. Cena: dvě stavby (debug: MikroMon 1 ms, APS-5 4 ms, `cpm_zrmd_fdc1` 82 ms,
    `rkd1_jb_cm5400` 66 ms).
  - Zkušební stroj a stroj, který se nepostavil, při zrušení nic nezapíše (`Machine::trial`, `Machine::discard()`,
    `Card::trial` v `~Machine` a devíti destruktorech karet, které ukládají). Jinak by stav načtený napůl zapsal
    RAM disk do souboru RAM disku běžícího stroje (RMD-1 ukládá, když je změněný).
  - Hlášky zkoušky se podrží (`log_hold`, `log_replay`, pro každé vlákno zvlášť) a zapíšou jednou: po dobré zkoušce
    ty ze zkoušky (jen ona mohla vytvořit pracovní kopie, „working copy: added“), po špatné její.
  - Chyba typu hodnoty v popisu (výjimka JSON) se při stavbě chytí a je chybovým textem, dřív utekla do GUI.
  - `load_state` jde také přes zkoušku: stav, který nesedí, nechá sestavu netknutou (tatáž generace,
    `STATE_BACK_SAME`, okna se nepřizpůsobují). Stav běžící sestavy v paměti zůstává pro vzácné selhání po dobré
    zkoušce (soubory se mezitím změnily); ta cesta je stejná jako dřív a test ji bez podvržené chyby nevyvolá.
  - Prověřené prostředky PC: spojení sériových linek (výše), soubory médií (otevřené se sdílením, IDE `r+b`),
    zálohované paměti (čtou se celé, zapisují atomicky), zvuk (jeden proud SDL), vlákna relay a DPI-1L (startují,
    až je program použije, ve zkoušce nikdy).
  - Test `test_host`: nepovedené popisy (neznámá karta, chybějící ROM, COM bez portu, chyba typu JSON, kolize slotů,
    chybějící soubor sestavy), useknutý stav (obraz RMD-1 beze změny), APS-5 bez výpadku, klávesnice v popisu,
    `file_lock_try`, časy přepnutí. `test_state` čeká, že sestava po nesedícím stavu zůstane tatáž.
- **Poslední sestavy po každém startu:** `recent_add` (přesunuté z `browser.cpp`) po každém úspěšném startu z GUI
  (všechny jdou přes `start_machine` v `rack.cpp`: Vybrat sestavu, Poslední sestavy, Spustit sestavu ze souboru,
  přetažení souboru), po Restartovat s touto sestavou se souborem a po Uložit jako. Nepovedený nebo zrušený start
  seznam nemění, MCP a `sapiemu-cli` se nepřidávají.
  - Vybrat sestavu se zavře až po startu (`ui.browser_start`) a při Zrušit v otázce na neuložené změny zůstane
    otevřené. Fokus displeje a rozložení prvního startu jen po povedeném startu. Po nepovedeném se okno zavře také,
    důvod ukáže konfigurátor.
  - Po kontrole: oponent navrhl Uložit jako ze seznamu vyřadit. Zamítnuto, hodnocení (BEH-19) chce výslovně
    „po každém úspěšném startu a po Uložit jako“. Nadpis seznamu ve Vybrat sestavu je místo „Naposledy spuštěné“
    „Poslední sestavy“, jako v menu Soubor a jako to, co seznam drží.
- **Stroj → Klávesnice** jde přes popis sestavy (hodnocení IA-18): `Machine::set_live_param` klávesnice karty CPU
  (`Machine::plug_keyboard`), z menu `rack_set_keyboard` přes `rack_set_live_param` (vrací úspěch a chybu). Změna je
  v popisu běžící sestavy a v konfigurátoru jako neuložená (Uložit, Ponechat pro příští spuštění), vydrží Ctrl+F12.
  - Popis staví `rack_keyboard_cfg` podle pravidla konfigurátoru (vlastní volby zůstanou, joyport jen u ANK-1M2),
    sdílený s výběrem klávesnice v konfigurátoru.
  - Záměrně to **není** `Machine::live_param`: pole klávesnice v konfigurátoru se chová jako dřív (označené, platí po
    vypnutí a zapnutí) a volby strict, beeper a joyport se za běhu nezapojují.
  - Klávesnici, kterou karta CPU nevezme (PKT-1 bere jen klávesnice se strobe), nahradí klávesnice z popisu, nezůstane
    žádná. MCP `set_keyboard` se nemění (jen běžící klávesnice).
- **Varování při druhém okně:** GUI drží po celý běh zámky `<soubor nastavení>.lock` a `work/sapiemu-gui.lock`
  (`file_lock_try` v `util.cpp`).
  - Windows: `CreateFileW` bez sdílení s `FILE_FLAG_DELETE_ON_CLOSE`, po pádu nic nezůstane. Obsazené je jen
    `ERROR_SHARING_VIOLATION`, jiná chyba (složka jen pro čtení) varování nedá. Linux: `flock`, soubor zůstane.
  - Druhé okno nad stejným nastavením nebo stejnými daty (třeba debug a release exe jednoho repa přes `work/`) ukáže
    modální „Už běží jiné SAPIemu“ s cestami, až po případném okně Chyba ze startu. Jen varování: Pokračovat je
    výchozí (Enter i Esc), Ukončit odejde bez uložení nastavení a `imgui.ini`.
  - Druhé GUI nad stejným nastavením píše log do `sapiemu.2.log`, nepřepíše log prvního.
  - `sapiemu-cli` a `--ui-snapshot` zámek neberou ani nehlídají, testy a galerie jich pouští mnoho naráz. Snímek
    `--ui-second`.
  - Po kontrole: Ukončit ve varování zapsalo sdílené soubory, sestava mezitím běžela. Dokud varování čeká, okno teď
    nezapíše nic: sestava neběží (`host.frame`, tedy ani průběžné ukládání médií), příkazy MCP čekají (`host.poll`),
    ztráta fokusu média neukládá a ImGui neukládá rozložení (`io.IniFilename` po načtení v prvním snímku odložené
    v `App::ini_held`). Ukončit jde přes nové `EmuHost::drop()`: zavře spojení, `Machine::discard`, bez
    `power_down`, bez varování výpadku, RAM disky ani zálohované paměti se nezapíšou. Pokračovat vše pustí
    a rozložení označí k uložení. Před odpovědí vzniknou jen pracovní kopie, které při načtení chyběly, jako dřív.
    Načítat sestavu až po odpovědi by byla velká změna startu i snímků. Test `drop_saves_nothing`, snímkem ověřeno,
    že `crm.bin` APS-5 zůstane po Ukončit bajt po bajtu stejný.
- **Chyby jádra česky** (přesunuté z etapy 7): jádro zůstává anglicky (sdílí ho cli a MCP), GUI hlášky překládá při
  zobrazení `tr_core()` podle tabulky `k_core` v `i18n.cpp` (asi 85 pevných textů a vzorů s `%s`).
  - Platí první vzor, který sedí, proto jsou zvláštní vzory před obecnými. Zachycené části se přeloží také, u
    „<karta nebo soubor>: <text>“ jen text. Co v tabulce není, zůstane anglicky.
  - Pokryté: soubor a popis sestavy, ROM, média a pracovní kopie, formáty disket, pásek, KZD-1 a RKD-1, obrazy IDE,
    nahrání programu a program na kazetu, klávesnice, porty terminálu, stavy (i novější a starší formát), důvod
    zastavení DEMA APS-5. Použité v `app.cpp`, `rack.cpp`, `media.cpp`, `aps5.cpp`, `terminal.cpp`, `plotter.cpp`.
  - `test_i18n_keys` hlídá, že každý pevný kus vzoru je v literálech `src/` (v jednom literálu, nebo kde jádro spojí
    konec jednoho a začátek druhého), jinak je záznam mrtvý. `test_i18n` hlídá vzorky, stejný počet `%s` a že každý
    vzor přeloží jeho vlastní záznam (našel jednu chybu pořadí).
  - Anglicky zůstávají kontroly konfigurace („X must be …“, jen z ručně psaných popisů), podrobnosti ImageDisk, DPI-1L,
    IMS-2 a zapisovačů.
- **Moje sestavy** (rozhodl autor: release kopie, debug přepisuje):
  - V buildu pro vydání (`NDEBUG`, jedna funkce `save_copies()`, oba buildy překládají stejný kód) zapíše Uložit
    v konfigurátoru (i v otázce Neuložené změny) dodanou sestavu jako kopii do `work/machines/` (přes `work_root`,
    jde i s datovou složkou jen pro čtení). Ladicí build dál přepisuje, `--ui-save-copy` vynutí chování vydání.
  - **Dodaná** = jmenovaná v `machines/_vyber.json` (`MachineEntry::listed`, `machine_supplied`). Po kontrole: dřív
    každý soubor pod `machines/`, takže i vlastní soubor uživatele by šel do kopie. Bez čitelného rejstříku platí
    složka, aby se nic dodaného nepřepsalo. `test_index` hlídá, že každá sestava v `machines/` je v rejstříku.
  - Kopie má jméno souboru originálu (obsazené → `-2`, `-3`…: uložit originál znovu po týdnech nesmí tiše zahodit
    starší kopii), ke jménu „ (moje)“ nebo „ (mine)“ v jazyce rozhraní při uložení, „(moje 2)“ u druhé. Jméno změněné
    uživatelem zůstane bez značky. Dostane `based_on` s cestou originálu: konfigurátor ho zapíše s komentářem za
    `description`, `cfg_hw` ho za změnu hardwaru nebere.
  - Konfigurátor pak drží kopii, další Uložit je obyčejné. Běžící sestava se na kopii přejmenuje bez nového sestavení
    (`EmuHost::set_file`: soubor, jméno, `based_on` v `Machine::config` i `last_cfg`), paměť zůstane, titulek okna,
    Poslední sestavy a příští start jdou za kopií.
  - Rychlý slot kopie je `work/state/mine_<jméno>.sapistate`, nesdílí slot originálu se stejným jménem souboru.
  - Bublina u Uložit (lišta konfigurátoru, otázka Neuložené změny) řekne, co Uložit udělá: release „Zapíše změny do
    kopie (Moje sestavy): … Dodaná sestava zůstane, jak je: …“, debug, že dodanou přepíše a release by uložil kopii.
  - Po kontrole: v buildu pro vydání začíná Uložit jako u nové a dodané sestavy ve `work/machines` (data mohou být jen
    pro čtení a soubor v `machines/` by byl stejně jen uživatelův).
  - Rejstřík (`core/machine_index`) přidá kategorii `mine` („Moje sestavy“, „My machines“) nahoru, jen když
    `work/machines/*.sapi` existují (soubory začínající `_` přeskočí). Jména kategorie má jádro v obou jazycích jako
    u Nezařazených, klíče `tr()` nepotřebuje. Kopie má náhled a průvodce originálu, nepřejmenovaná titulek originálu
    se značkou v každém jazyce („ZPS-1 s MIKROBASICem 2.4 (moje)“) místo dlouhého jména souboru, hledání ji najde
    i podle titulků originálu. Po kontrole skládá i řádek s dlouhým jménem jméno originálu a značku v jazyce rozhraní
    (`MachineEntry::copy`). V souboru zůstává značka z doby uložení.
  - Ve Vybrat sestavu je kategorie otevřená. Kopie má řádek „Vychází z <titulek>“ (odkaz vybere originál),
    Spustit/Restartovat, Průvodce, Otevřít v konfigurátoru a Smazat… (šedé, dokud běží: běžící sestava by neměla
    soubor). Smazat se zeptá (Zrušit výchozí, Enter i Esc ruší) a smaže jen soubor kopie, její řádek v Posledních
    sestavách a ponechané změny. Konfigurátor, který ji držel s neuloženými změnami, je nechá jako novou neuloženou
    sestavu, jinak ukáže běžící sestavu. Pak je vybraný originál.
  - „Obnovit původní“ z plánu nahrazuje odkaz „Vychází z“ (originál je ve své kategorii beze změny) se Smazat….
  - MCP `list_machines` vypíše i kopie s `based_on`, `load_machine` je bere cestou `work/machines/…`.
  - Testy: `test_mine` v `test_machine_index` (vlastní kořen dat `work/test/mine_root`), `host_copy` v `test_state`.
- **Přetažení souborů na okno** (`SDL_EVENT_DROP_FILE`): soubory jdou do fronty, `drop_frame` v `app.cpp` bere jeden
  za snímek po liště menu (sestava a za ní disketa v jednom tahu: nejdřív se spustí sestava, disketa jde do nové).
  `file_drop` v `media.cpp` používá kód menu: odpověď dialogu je jedna funkce `file_chosen` pro dialog i přetažení.
  - `.sapi`, `.jsonc` jako Soubor → Spustit sestavu ze souboru (`rack_start_machine`, i s otázkou na neuložené
    změny); `.sapistate` načte stav; `.hex`, `.ihx` a `.txt` začínající `:` otevřou otázku Nahrát program; `.bin`
    otázku na adresu.
  - Diskety (`.img .imd .8sd .sp2 .sp1`), kazety RKD-1 (`.rkd`) a pásky RMP-1 (`.tap`) jen do první prázdné
    mechaniky. Žádná prázdná nebo žádná mechanika: okno Chyba (uživatel musí něco udělat; počítá se do „Chyby: N“
    jako jiné chyby uživatele). Kazety (`.wav`, `.mp3` do DSM-1, `.kzd` do KZD-1) do prvního prázdného magnetofonu,
    jinak prvního, kazetu vymění jako Vložit… (stará je uložená průběžně).
  - Neznámý soubor, vypnutá sestava, otevřený dialog: krátký řádek ve stavovém řádku. Při otevřeném modálním okně
    (`GetTopMostPopupModal`, otevřené menu nevadí) se soubor odmítne, když má v tom snímku vyskočit otázka nebo
    Chyba, počká snímek. Otevřený souborový dialog Windows také odmítne, jeho odpověď by zabrala místo souboru.
  - Povedené: „Vloženo do RPD-1, Mechanika 1: x.img“ a klávesnice se vrátí displeji. Okno SAPIemu se po přetažení
    nezvedá (`SDL_RaiseWindow` by na Windows mohlo blikat na liště), fokus Windows tak může zůstat Průzkumníku.

### Nehoda s daty autora

- Snímek GUI agenta chování (`--ui-switch machines/aps5.sapi`, pak MikroMon, 13:52:49) sestavu APS-5 vypnul
  a přepsal `work/aps-5/crm.bin` a `work/aps-5/rtc.bin`. `work/` není v gitu a `file_write_safe` soubory nahradil,
  předchozí obsah se obnovit nedá. Nový obsah je dosavadní CMOS RAM po asi 0,2 s teplého startu a uložení firmwaru při
  výpadku: programy a kalibrace by měly zůstat, změnily se záznamy výpadků a hodiny.
- Příčina: snímky běží nad skutečným `work/`, přepnutí ze sestavy se zálohovanou pamětí ji zapíše. Opravy po
  oponentuře už dělaly snímky nad kopií dat (`--data-root`). Pravidlo: snímky se sestavami se zálohovanou pamětí
  (APS-5, DMR-1, sestavy V) jen nad kopií dat.

### Na rozhodnutí autora

Rozhodl workflow, autor to může změnit (doporučení je první):

- **Snímky a videa ve dvou složkách** (podnabídka), nebo obojí do jedné složky jako záznamy v DOSBoxu (pak jedna
  položka, jak říká plán; změna `video.cpp` a návodů).
- **Skryté displeje se neukládají** (každý start ukáže všechny), nebo je pamatovat v nastavení `displays` vedle
  zvětšení.
- **Nastavení jedna stránka s posuvníkem** (při 1280 × 720 se posouvá), nebo seznam sekcí vlevo jako ve Windows 11.
- **Joystick z klávesnice** nechat v menu Stroj a na Ctrl+F8, nebo přidat do Nastavení → Ovládání s „(jen do
  ukončení)“.
- **Selhání skutečné stavby po dobré zkoušce** (jen když se soubory mezi nimi změní) nechat jako dřív (`load_config`
  nechá starý stroj vypnutý, APS-5 by byla ve výpadku; `load_state` sestavu vrátí ze stavu), nebo starý stroj postavit
  znovu z jeho uloženého stavu.
- **Klávesnice v konfigurátoru** dál platí po vypnutí a zapnutí, nebo hned jako Stroj → Klávesnice (skutečná
  klávesnice jde přepojit za chodu; za běhu by se zapojily i volby strict, beeper, joyport).
- **MCP `set_keyboard`** dál jen běžící klávesnice, nebo přes popis (konfigurátor by změnu z MCP ukázal jako
  neuloženou).
- **RAM-1V vedle ZRMD-1 bez JP2 na portu 63h** je záměr jako se ZRD-1V, nebo varování. Potvrdit podle skutečných
  desek.
- **Klik na prázdný rám** dál zruší výběr slotu, nebo jen přepne na záložku Sestava a sběrnice a slot nechá.
- **Paměť: 00h–1Fh a 7Fh jako „.“** (klasický výpis), nebo rámečky SAPI, které písmo má.
- **Příznaky v debuggeru platí hned** (registry po Enteru), nebo i příznaky až po potvrzení.
- **Mnemoniky Intel** pro 8080 a 8085 později jako přepínač v debuggeru, nebo vůbec.
- **Viditelné popisky** místo samotné bubliny (tlačítko „…“ u média, LO1/LO2, LI a/b APS-5; hodnocení 5.4) později,
  mění vzhled oken Médií a přístrojů.
- ~~**Návody k obsluze** nechat mimo program, nebo později v okně Nápověda~~ Rozhodl autor hned po etapě: v okně
  Nápověda (oddíl „dokumentace v okně Nápověda“).
- **Značka kopie v souboru** v jazyce uložení (pole Jméno v konfigurátoru ji tak ukáže), nebo jméno bez značky
  a značka jen v titulku.
- **Uložit jako v buildu pro vydání** začíná u nové a dodané sestavy ve `work/machines`, nebo v `machines/`.
- **Přetažená kazeta** vymění kazetu v plném magnetofonu jako Vložit…, nebo se odmítne jako disketa.
- **Okno se po přetažení nezvedá**, nebo `SDL_RaiseWindow`.
- **Plná nebo chybějící mechanika při přetažení** v okně Chyba (počítá se do „Chyby: N“), nebo jen stavový řádek.
- **Fokus ve snímcích:** snímky od etapy 4 fokus displeji nevracejí, přesun klávesnice po skrytí displeje šel ověřit
  jen dočasným buildem. Volbu `--ui-*` přidat, až bude potřeba.

### Ruční zkouška autora a známá omezení

Snímek skrytého okna tyto věci neukáže:

- Přetažení `.sapi`, `.sapistate`, `.hex`, `.bin`, `.img` a `.wav` z Průzkumníka na skutečné okno (cesta OLE v SDL,
  fokus potom), dvě diskety naráz do sestavy se dvěma volnými mechanikami.
- Build pro vydání (`tools\build.cmd release`): Uložit dodané sestavy v konfigurátoru a v otázce při ukončení
  (`work\machines\<jméno>.sapi`, titulek s „(moje)“, Poslední sestavy, příští start, `machines\` beze změny), totéž
  podruhé (`-2`, „(moje 2)“), vlastní soubor v `machines\` (obyčejné Uložit), Uložit jako, Smazat… v Mých sestavách.
- Dvě okna SAPIemu (stejné exe; debug a release nad jedním `work\`): varování, Ukončit nechá `crm.bin`, `rtc.bin`,
  `work\dmr-1\sram.bin`, nastavení i `imgui.ini` prvního okna, Pokračovat; po zavření žádný `.lock`.
- Stroj → Klávesnice → ANK-1 na MikroMonu: neuložená změna v konfigurátoru, Ctrl+F12 ji nechá, Vrátit změny, Uložit,
  Ponechat pro příští spuštění.
- Rozbitá sestava (kopie s neznámou kartou) za běhu APS-5: „Sestava běží dál, jak běžela.“, žádné VYPADEK NAPAJENI,
  klient TCP sériové linky zůstane připojený. Česky: Načíst stav na soubor, který stavem není, obraz diskety špatné
  velikosti, chybějící ROM.
- Rychlost přepnutí a Ctrl+F12 velkých sestav (RKD-1, CP/M ZRMD s FDC-1, sestava V s IDE): každé přepnutí staví
  sestavu dvakrát.
- Hry a dema: klik do CGA-1V, psaní a joystick z klávesnice (oprava). Zobrazení → Obrazovky: skrýt CGA-1V a AND-1V,
  klávesnice, Ctrl+F12, jiná sestava, Alt+Enter se skrytým displejem.
- Debugger na ZPS-1 (8080), ZPS-1 s JPR-1X (8085, SIM a RIM) a sestavě V (Z80); Paměť nad ROM se semigrafikou.
- Konfigurátor SAPI-1 V: 63h šedě, RAM-1V S3 na 62h žlutě; záložky (slot, pravé tlačítko, přetažení karty, prázdný
  rám, mapa); Nová ▸.
- Okno Nápověda: obě záložky, Rozbalit a Sbalit vše, odkazy (Issues v prohlížeči, `docs/aps5-ukazka.md`), Tab po
  odkazech, změna jazyka, z rozbaleného zipu.
- Nastavení s vaším `imgui.ini` (34 × 30, posouvá se), Záznam se špatnou cestou ffmpeg, Pokročilé po zapnutí MCP
  a restartu.
- `work\aps-5\crm.bin` a `rtc.bin` po nehodě výše.

Známá omezení:

- Build pro vydání a `release test` po commitu etapy (`0d03774`) prošly 62/62, WSL se nespouštělo.
- Nové soubory (`markdown.*`, `mdview.cpp`, `test_markdown.cpp`, `test_host.cpp`, slovníčky) jsou v gitu od `0d03774`.
- Třetí GUI spuštěné po ukončení prvního varování nedostane, i když druhé běží (druhé zámek nedrží). Pro varování to
  stačí.
- Zrušit v otázce při spuštění z Vybrat sestavu se snímkem neověřilo (klávesy snímku nedošly do pole hledání).

## 2026-10-29 – dokumentace v okně Nápověda (prohlížeč .md)

Autor: „některé ty návody a postupy odkazují na .md soubory, které se otevírají podle systému … nebylo by dobré ty
.md otevřít do vlastního okna s vlastním jednoduchým .md prohlížečem“. Souhlasil s návrhem; udělané hned po etapě 8
v hlavním proudu, s jedním oponentem.

- **Proč:** na čistých Windows `.md` nemá rozumný program (výběr aplikace nebo holý text se značkami), odkazy mezi
  dokumenty a obrázky v cizím programu nefungují. Okno Nápověda z etapy 8 už Markdown kreslilo.
- **Okno Nápověda je prohlížeč** každého `.md` z dat, který program otevírá: První kroky a Slovníček (tlačítka),
  Návody k obsluze (seznam v okně i v menu Nápověda) a dokumenty, na které vedou odkazy (popisy desek z návodů,
  návody z průvodců sestavami). Ostatní soubory (obrázek, sestava, složka) dál otevře program systému, odkaz vpravo
  nahoře celý `.md` také.
- **Historie jako v prohlížeči:** Zpět a Vpřed (šipky, Alt+šipka vlevo a vpravo, boční tlačítka myši), nový
  dokument zahodí ty, které byly před ním. Každý dokument si drží místo v textu i sbalené kapitoly. Titulek okna nese
  nadpis dokumentu („Nápověda – KZD-1: návod k obsluze“).
- **Stránky podle jazyka:** První kroky a Slovníček ukazují soubor v jazyce rozhraní. Odkaz na soubor druhého jazyka
  (anglický slovníček odkazuje český) otevře právě ten soubor.
- **Co prohlížeč umí navíc:** obrázky PNG (zmenšené na šířku okna, s rámečkem, kliknutím v plné velikosti; víc
  obrázků pod sebou v jednom odstavci jako v `docs/sestavy.md`), citace `>` se svislou čarou (`docs/desky/MFC-1A.md`),
  cesty s `..` (`docs/desky/Minigraf.md` → `../img`). Všechny dokumenty kromě Prvních kroků mají kapitoly otevřené,
  Sbalit vše je sbalí. Co nezná (HTML, poznámky pod čarou), ukáže jako text.
- **Dlouhé dokumenty** (popisy desek mají stovky řádků): výšky bloků se pamatují pro šířku a velikost písma,
  rozvrhují se jen bloky v pohledu.
- **Tabulky:** sloupec přesně tak široký jako jeho nejdelší slovo ho kvůli zaokrouhlení lámal („Velikos-t“
  v JPR-1S); když se nejdelší slova nevejdou, úzké sloupce si je nechají a široké se dělí o zbytek (dřív se
  zmenšily všechny poměrně).
- **Dokument, který v kopii není** (zip nemá vývojářské poznámky `SOUHRN.md`, `rozhodnuti.md`, `docs/vydani/`): okno
  řekne „Soubor … chybí.“.
- **Ověřeno:** `test_markdown` (obrázky, citace, cesty, výzva `>RUN` v bloku kódu není citace), snímky návodu KZD-1
  s obrázkem, MFC-1A s citací, Minigraf s `../img`, JPR-1S s tabulkou v tmavém vzhledu, anglický slovníček,
  chybějící soubor, odkaz → Zpět → Vpřed kliknutím.
- **Po oponentuře** (oponent porovnal starý a nový parser na všech 144 `.md` repa): citace pod bodem seznamu zůstane
  pod ním i s textem za ní (`docs/vydani/ux-hodnoceni.md`; předzpracování nechá odsazení před `>`, přechod do
  citace a z ní nemaže seznamy, svislá čára v úrovni citace); blok kódu v citaci skončí se svým plotem nebo s citací
  (dřív by spolkl zbytek dokumentu); plot uvnitř komentáře HTML neskryje citace za ním; obrázek z webu sám
  v odstavci je odkaz do prohlížeče; `md_norm_path` nechá kořen absolutní cesty (`/home/…`, UNC `//server/…`);
  Alt+šipka vlevo a vpravo jsou v Nápověda → Klávesové zkratky. Testy na všechno v `test_markdown`.

Ruční zkouška autora: Nápověda → Návody k obsluze (obrázky, klik na obrázek), odkaz z průvodce sestavy (AR 1990 →
návod k plotteru), Alt+šipky a boční tlačítka myši, přepnutí jazyka s otevřeným oknem.

## 2026-10-29 – sestava bez displeje: hlavní okno místo prázdné obrazovky

Autor: „sestava AR1990 … udělá velký screen, přesto, že sestava screen nemá. Naproti tomu okno plotteru, které je
zásadní je malé a chybí rack, který by se vešel.“ Snímek při jeho velikosti okna: černé okno „Sestava nemá kartu
displeje“ 1450 × 1088, zapisovač 600 × 840 vpravo, konfigurátor se nevešel.

- **Bez displeje konzole žádné okno obrazovky.** Platí i pro vypnutou sestavu (podle té, která běžela naposledy).
  Hláška „Sestava nemá kartu displeje.“ zmizela. Týká se dvou sestav: `plotter_ar1990` a `cpm_term64k_fdc1` (CP/M
  jen s terminálem). ZPS-6 má displej DBS-1A.
- **Hlavní výstup sestavy bez displeje je první okno jejího `windows`**, vlevo nahoře při prvním použití:
  - zapisovač na celou výšku plochy, široký tak, aby se vešel celý list A4 (stůl 230 × 297 mm pod ovládáním),
    nejvýš 65 % šířky; širší okno by jen přidalo okraje nebo posouvání;
  - jiné okno (terminál) ve své velikosti.
  Uložené místo z `imgui.ini` má přednost jako u ostatních oken.
- **`cpm_term64k_fdc1` dostala `"windows": [ "terminal" ]`:** bez displeje a bez otevřeného terminálu by po startu
  nebylo vidět nic.
- **Okno displeje, které nová sestava nekreslí, není překážkou** ani v prvním snímku po změně sestavy. Dřív se
  zapisovač po přepnutí ze ZPS-1 vyhnul místu, kde byla obrazovka ZPS-1.
- **Konfigurátor** se při 2252 × 1214 vejde vedle zapisovače (vpravo, s nejnižší prioritou jako vždy), při
  1280 × 720 ne (potřebuje 44 × 44 výšek písma).
- **V doku** je zapisovač uprostřed. Když ve sloupci nic není, dok mu dá celou šířku (list se pak posouvá); plovoucí
  rozložení je výchozí.

Ověřeno snímky při 2252 × 1214 (125 %) a 1280 × 720: AR 1990 přímo i po ZPS-1, CP/M s terminálem, ZPS-1 beze změny,
dok.

## 2026-10-20 – hláška opraveného BIOSu ECBIM8

- **Opravený BIOS ECBIM8 (MFC-1A, chybějící `RET` v `INIT_MFC`) se hlásí `BIOS-3K M050815JB`** místo `V050815JB`
  (autor: „aby bylo na první dobrou vidět, že je upravený BIOS“, délku textu zatím neměnit). Platí všude, kde je
  oprava: `_fixMFC` v `media/`, autorova pracovní kopie, zdroják `ECBIM8C.MAC` (`build.py --check` hlásí u SYS2
  právě dva rozdíly) a balíček pro EC1045. Neopravené obrazy hlásí dál `V`, takže se verze nepletou.
- **Oprava i v souboru `ECBIM8.BIN`** na RAM disku (3B832h), nejen v systémové oblasti, aby se chyba nevrátila při
  generování systému z toho souboru.
- **Původ chyby:** monitor `mon281211` má na stejném místě záměrný průchod do výběru disku 0, do BIOSu pro RAM disk
  se nejspíš přenesl i s ním (`docs/software/CPM_SAPIMAN.md`).
