# Sestavy a konfigurátor

Sestava je popis počítače: sběrnice, karty ve slotech a jejich jumpery. Je uložená v souboru `.jsonc`
(JSON s komentáři). Emulátor sestavy hledá ve složce `machines/` vedle sebe (i v podsložkách), ale soubor může
ležet kdekoli: otevírá se přes Soubor → Otevřít soubor… nebo v konfigurátoru.

## Konfigurátor (Stroj → Sestava…)

- **Čelní pohled na rám:** sloty X1…Xn zprava doleva, vpravo panely SPN a ZDR.
  - Panely jsou kreslené podle tiskových podkladů hliníkových štítků z rámu Libora Lasoty
    (`H:\SAPI-1\LiborLasota\Chassis\Potisk_ARTAX`, rozměry v bodech přímo z PDF): úzký panel 57 × 491,8 bodu,
    SPN dvě pozice, ZDR sedm pozic, otvory, konektory K2/K3 (D-sub), LED ZAM/DSK, popisky.
  - Rozložení konektorů podle zkratky panelu: JPR a FDC (K3 nahoře, K2 dole), DSM (K3 „A“, K2 „B“), AND, DGD,
    BGP a CGA (K2 VGA uprostřed), MPH (K4 OVL, K3 ZVUK, K2 MYŠ), ZRD-1V (LED ZAM, DSK). Ostatní karty mají
    zatím jen zkratku.
  - Volba „Typy karet na panelech“ ukáže přesný typ karty ve volném místě panelu.
  - X1 je procesorová karta (výjimkou je ARB-1C s JP3 = 1-2 pro PKT-1).
  - Počet slotů určuje typ sběrnice.
  - Karty, které zatím nejsou emulované, jsou šedé. Červená tečka znamená chybu (např. karta mimo X1).
- **Klik na slot** vybere kartu a pod rámem se ukáže její nastavení: výběr karty, jumpery a soubory ROM nebo
  obrazů s nabídkou známých souborů a tlačítkem `…`. Pravé tlačítko otevře rovnou seznam karet. Klik na rám
  mimo karty ukáže název a popis sestavy a nastavení sběrnice.
- **Panel ZDR:**
  - NAPÁJENÍ vypne a zapne počítač s upravenou sestavou (stejně jako tlačítko „Zapnout s touto sestavou“);
  - RESET a TURBO fungují jako F12 a F8;
  - displej FREKVENCE CPU ukazuje skutečnou rychlost emulace.
- **Změny se projeví až po vypnutí a zapnutí** (rozhodnutí autora). Obsah RAM se ztratí, RAM disky se před
  vypnutím uloží, pracovní kopie médií ve `work/` zůstávají.
- **Nová:** prázdná sestava, nebo kopie šablony či jiné sestavy (uloží se přes „Uložit jako…“).
- **Uložit** zapíše soubor s komentáři z katalogu: u každého jumperu je jeho označení a význam zvolené
  hodnoty. Ruční komentáře z původního souboru se při uložení z konfigurátoru ztratí.
- Dokud v konfigurátoru nejsou neuložené změny, sleduje běžící sestavu (i když ji změní MCP).

## Šablony ZPS

`machines/zps/ZPS-1.jsonc` až `ZPS-4.jsonc` podle https://sapi.cz/sapi/doku2/zps.php:

| Sestava | Karty | Klávesnice |
|---|---|---|
| ZPS-1 | JPR-1, REM-1, AND-1, DSM-1, BDK-1 | ANK-1 |
| ZPS-2 | JPR-1, REM-1, RAM-1, AND-1, DSM-1, BDK-1 | ANK-1 |
| ZPS-3 | JPR-1A, RAM-1A, AND-1A, DSM-1, RPD-1 (2 sloty) | Consul 259.11 |
| ZPS-4 | JPR-1Z, RAM-1Z, AND-1Z, DSM-1, RPD-1, RMD-1 | Consul 259.11 |

Pořadí karet ve slotech je odhad (viz Nejasnosti v `docs/SOUHRN.md`). Šablony zatím nepoběží, protože jejich
procesorové karty nejsou emulované. Dají se ale prohlížet, upravovat a ukládat.

## Formát souboru

```jsonc
{
    "name": "…",                      // název v titulku a v menu
    "description": "…",               // volitelný popis
    "bus": { "type": "ARB-1C-8", "jp3": "2-3" },
    "cards": [
        { "slot": 1, "type": "JPR-1ZI", "rom": "roms/…", "keyboard": { "type": "EKL-1" } },
        …
    ]
}
```

- **Sběrnice:**

  | Typ | Pozic | JP3 |
  |---|---|---|
  | `ARB-1` | 8 | ne |
  | `ARB-1A`, `ARB-1Au` | 13 | ne |
  | `ARB-1B` | 14 | ne |
  | `ARB-1C` | 14 | ano |
  | `ARB-1C-8` (8slotová deska ZPC) | 8 | ano |

  Klíč `slots` počet pozic přepíše.
- **Cesty** (ROM, obrazy) jsou relativně k datové složce emulátoru.
- **Parametry karet** popisuje katalog `src/cards/catalog.cpp`. V MCP ho vrací nástroj `card_catalog`.
- **Karta přes dva sloty** (RPD-1 = desky RPD-1A a RPD-1B) má v souboru první slot. Druhá deska je ve slotu
  vlevo od něj (o jedna vyšší číslo).

## Pro vývoj: nová karta

1. Třída karty v `src/cards/`, registrace v `src/cards/registry.cpp` (`card_create`, `card_types`).
2. V `src/cards/catalog.cpp` u karty nastavit `emulated = true` a popsat parametry (`CardParam`). Výchozí hodnota
   v katalogu musí odpovídat výchozí hodnotě v `configure()`.
3. `test_machine_cfg` ověří, že karta s výchozím nastavením naběhne a že sestavy projdou zápisem beze změny.

## Ověření vzhledu bez snímku plochy

`sapiemu --ui-snapshot soubor.png [--machine …] [--ui-open sestava.jsonc] [--ui-rack SLOT]` vykreslí GUI do
skrytého okna, uloží obrázek a skončí. Rozložení oken uživatele (`imgui.ini`) se nemění.
