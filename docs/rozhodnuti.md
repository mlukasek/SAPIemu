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
  FREKVENCE CPU, ZDROJ jsou na skutečném stroji česky), MCP a `sapiemu-cli`.
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
  s pracovní kopií, s potvrzením). Médium se potom vloží znovu.
- **Nová… v konfigurátoru jen u médií, kde nový soubor = prázdné médium** (páska, kazeta RKD-1, kazeta KZD-1;
  `CardParam.min = PF_NEW_MEDIUM`). Diskety potřebují zvolit formát, ty dál zakládá okno Média.
- **Souborové dialogy:** složka dostane koncový oddělovač (`dialog_location`), soubor nabízený k uložení zůstává.

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
