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
- **Formát konfigurace sestavy: JSON s komentáři** (`machines/*.jsonc`). Načítá se přes nlohmann/json s `ignore_comments`, takže stačí jedna knihovna pro konfiguraci i MCP.

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
