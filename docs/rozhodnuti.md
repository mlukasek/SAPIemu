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

## 2026-10-03 – Data a repozitář

- **ROM, PROM a všechny obrazy médií jsou v repu** (asi 14 MB). Emulátor má po naklonování fungovat hned.
  - Média v `media/` jsou nedotčené šablony, pracuje se s kopiemi ve `work/` (v `.gitignore`).
  - Duplicitní 10MB `.hex` RAM disku se vynechává.
- **MikroMon je jen jako binárka** (se souhlasem autora L. Lasoty), zdroje ne.
- **UI česky**, ale přes tabulku řetězců, aby šlo později přepínat. Font DejaVu Sans s diakritikou je vendorovaný.
- **Lokální commity průběžně, push jen na vyžádání.**
