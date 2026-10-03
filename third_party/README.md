# Knihovny třetích stran (vendorované)

| Složka | Knihovna | Verze | Licence | Zdroj |
|---|---|---|---|---|
| `chips/` | floooh/chips `z80.h` | master @ 9e88298 (2026-10) | zlib | https://github.com/floooh/chips |
| `i8080/` | superzazu/8080 (Intel 8080 core) | master @ 274ffd7 (2020-10-14) | MIT | https://github.com/superzazu/8080 |
| `imgui/` | Dear ImGui (core + SDL3/SDLRenderer3 backend) | v1.92.9b-docking | MIT | https://github.com/ocornut/imgui |
| `httplib/` | cpp-httplib | v0.59.0 | MIT | https://github.com/yhirose/cpp-httplib |
| `json/` | nlohmann/json (single header) | v3.12.0 | MIT | https://github.com/nlohmann/json |
| `fonts/` | DejaVu Sans, DejaVu Sans Mono | 2.37 | Bitstream Vera / DejaVu (free) | https://github.com/dejavu-fonts/dejavu-fonts |

SDL3 není vendorované. CMake ho při prvním configure stáhne přes FetchContent (release-3.4.18).
Na Linuxu se přednostně použije SDL3 nainstalované v systému.

Všechny licence jsou kompatibilní s GPLv3 projektu. Soubory se needitují. Při aktualizaci se nahradí celé
a tady se upraví verze.
