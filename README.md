# SAPIemu

**Emulátor počítače Tesla SAPI-1**: Z80 a 8080, modulární karty na sběrnici ARB-1, od původních sestav ZPS-1 až
ZPS-4 po nové desky „V“ Libora Lasoty. Karty se dají skládat a nastavovat jako skutečné (propojky, dekodéry podle
PROM dumpů). Emulátor má vestavěný MCP server pro vývoj softwaru pro SAPI s Claude.

> *English: an emulator of the Czechoslovak Tesla SAPI-1 modular computer (Z80/8080, ARB-1 bus) for Windows.
> Cards are configured like the real boards (jumpers, address decoders from PROM dumps). Original ZPS-1 to ZPS-4
> machines with MIKROBASIC, MIKOS and CP/M, the new "V" boards (VGA, colour graphics, YM3812 sound), save
> states, a debugger and a built-in MCP server. Alpha version, the documentation is in Czech.*

![Sestava V: CP/M na AND-1V a obrázek na barevné grafice CGA-1V](docs/img/sapi1v-cpm-cga.png)

![RAT PLAYER hraje na zvukové kartě MPH-1V, vpravo konfigurátor sestavy](docs/img/sapi1v-rat-sestava.png)

![ZPS-2 s MIKOSem a BASICem EXP V5.0/G kreslí na grafice DGD-1](docs/img/zps2-mikos-dgd.png)

## Stažení

Hotová verze pro Windows 10/11 (64 bit) je v [Releases](https://github.com/mlukasek/SAPIemu/releases). Rozbalit,
spustit `sapiemu.exe`, nic se neinstaluje. Jak začít: [docs/prvni-kroky.md](docs/prvni-kroky.md).

Jde o **alfa verzi**. Hlavní věci fungují, ale chyby čekejte. Hlaste je v
[Issues](https://github.com/mlukasek/SAPIemu/issues).

## Co umí

- **Procesorové karty:** JPR-1 a JPR-1A (8080), JPR-1Z, JPR-1ZI a JPR-1V (Z80), včetně BOOT logiky a čekacích
  stavů.
- **Paměti:** REM-1, RAM-1, RAM-1A, RAM-1Z, RAM-1V (mapování MAP1/MAP2), DMR-1. RAM disky RMD-1, ZRMD-1
  (s RTC) a ZRD-1V (rev 1.1 i 2.1 se softwarovým zámkem zápisu).
- **Obraz:** AND-1, AND-1A, AND-1Z, AND-1Z-89, AND-1V (VGA). Grafika DGD-1, DGD-1V, barevná CGA-1V (Bt476).
- **Komunikace a disky:**
  - DSM-1 (sériový modem s magnetofonem: kazety WAV a MP3), DSM-1V;
  - řadič 8" disket RPD-1, IDE-1 s IDE diskem a PMD32-SD;
  - sériové linky do PC přes TCP nebo COM port.
- **Zvuk a hry:** MPH-1V (YM3812 přes Nuked-OPL3, 82C54, joystick a myš Atari ST/Amiga), bzučák klávesnice.
  Joystick z gamepadu nebo z klávesnice, myš PC.
- **Klávesnice:** EKL-1, Consul 262.3, Consul 259.11 a membránová ANK-1.
- **Software:** MikroMon, MIKROBASIC, MIKOS V4/V5 a MIKOS/D, CP/M 2.2 (ZRMD-1, RPD-1, IDE), ELIS, MikroBIOS,
  ECMON.
- **Konfigurátor sestavy:**
  - panely karet jako na skutečném stroji, LED podle emulace;
  - propojky, některé i za chodu;
  - mapa portů a paměti s kolizemi pro všechny stavy MAP1/MAP2.
- **Ladění:** debugger (registry, disassembler, paměť, breakpointy a watchpointy, I/O log), ukládání stavu (i
  rychlé checkpointy), nahrávání programů (Intel HEX, binární soubory), nahrávání zvuku do WAV.
- **Bez okna:** `sapiemu-cli` pro skripty a testy (`--type`, `--wait-text`, `--screenshot`, `--audio-wav`,
  `--save-state` …).
- **MCP server** (`http://127.0.0.1:8580/mcp`): Claude Code může emulátor ovládat, psát a ladit programy
  ([docs/mcp.md](docs/mcp.md)).

## Dokumentace

- [docs/prvni-kroky.md](docs/prvni-kroky.md): spuštění, sestavy, klávesy, co vyzkoušet.
- [docs/sestavy.md](docs/sestavy.md): konfigurátor, sestavy a katalog karet.
- [docs/desky/](docs/desky/): poznatky o jednotlivých deskách (porty, PROM, propojky, nejasnosti).
- [docs/stav.md](docs/stav.md): ukládání stavu. [docs/mcp.md](docs/mcp.md): MCP server.
  [docs/build.md](docs/build.md): sestavení a tipy pro vývoj.
- [docs/SOUHRN.md](docs/SOUHRN.md): stav projektu, plán, otázky k ověření na skutečném hardwaru.

## Sestavení ze zdrojů

- **Visual Studio 2022:** Soubor → Otevřít → Složka, preset `Windows x64 Debug` nebo `Release`.
- **Příkazová řádka:** `tools\build.cmd release test` (najde VS, sestaví a spustí testy).
- **Balík pro vydání:** `python tools\package.py`.
- SDL3 se stáhne při prvním configure (CMake FetchContent). Ostatní knihovny jsou v `third_party/`.

## Licence a poděkování

- **Licence:** emulátor je pod GPLv3 ([LICENSE](LICENSE)). Knihovny třetích stran mají vlastní licence
  ([third_party/README.md](third_party/README.md)).
- **ROM, PROM a programy** v `roms/` a `media/` patří svým autorům a jsou tu pro zachování historie SAPI-1.
  Zdroje jsou v [roms/README.md](roms/README.md) a [media/README.md](media/README.md).
- **Petr Všijanský (EC1045):** stránky [sapi.cz](https://www.sapi.cz) s dokumentací, schématy, výpisy a dumpy,
  ze kterých emulace vychází, a překreslená schémata desek.
- **Libor Lasota:** desky V, MikroMon a programy pro CGA-1V a MPH-1V, použité s jeho souhlasem.
- Software pro SAPI-1 od Tesla ELTOS / DIZ, CP/M 2.2/P od R. Peřiny a další programy, které se zachovaly.
