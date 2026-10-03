# SAPIemu

Emulátor počítače **Tesla SAPI-1** se Z80/8080 a modulárními kartami na sběrnici ARB-1.
Běží na Windows 10 a 11, v plánu je i Linux. Má vestavěný MCP server pro vývoj softwaru s Claude.

Projekt je ve vývoji. Stav a plán najdete v [docs/SOUHRN.md](docs/SOUHRN.md).

## Sestavení

- **Visual Studio 2022:** Soubor → Otevřít → Složka, pak vyberte preset `Windows x64 Debug`.
- **Příkazová řádka:** `tools\build.cmd debug test`.

## Licence

GPLv3, viz [LICENSE](LICENSE). Knihovny třetích stran mají vlastní licence, viz [third_party/README.md](third_party/README.md).
