# Sestavy a konfigurátor

Sestava je popis počítače: sběrnice, karty ve slotech a jejich jumpery. Je uložená v souboru `.sapi`
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
  - Sériové karty (DMR-1, DSM-1) bývají v sestavě víckrát, proto mají pod zkratkou nahoře i dole port, např.
    `#10`, `#14`. Stejně se jmenují i v seznamech, bublinách a v okně Sériový terminál („DMR-1 #10“).
  - Dvě karty na stejném portu jsou chyba (sestava se nezapne). Dvě DMR-1 se zapnutou RAM nebo se stejným
    souborem baterie jsou varování. Druhá DMR-1 se obvykle používá jen jako sériový port: RAM vypnout
    (parametr „RAM 128 KB“) a JP2 nastavit na jiný port.
  - X1 je procesorová karta (výjimkou je ARB-1C s JP3 = 1-2 pro PKT-1).
  - Počet slotů určuje typ sběrnice.
  - Karty, které zatím nejsou emulované, jsou šedé. Červená tečka znamená chybu (např. karta mimo X1).
- **Klik na slot** vybere kartu a pod rámem se ukáže její nastavení: výběr karty, jumpery a soubory ROM nebo
  obrazů s nabídkou známých souborů a tlačítkem `…`. Pravé tlačítko otevře rovnou seznam karet. Klik na rám
  mimo karty ukáže název a popis sestavy a nastavení sběrnice.
- **Panel ZDR:**
  - NAPÁJENÍ je vypínač s aretací. Za chodu počítač vypne: obrazovka a LED zhasnou, RAM se ztratí. SRAM
    DMR-1 s baterií (JP7) a RAM disky se uloží, spojení TCP a COM se zavřou. Další stisk počítač zapne se
    sestavou z konfigurátoru, včetně neuložených změn. Totéž je v menu Stroj → Vypnout/Zapnout a v MCP
    (`power`). Tlačítko „Zapnout s touto sestavou“ dělá vypnutí a zapnutí jedním krokem;
  - RESET funguje jako F12, TURBO jako F8 a přepíná dokola takt CPU (2 MHz) → 4 MHz → maximum;
  - displej FREKVENCE CPU ukazuje skutečnou rychlost emulace.
- **Změny se projeví až po vypnutí a zapnutí** (rozhodnutí autora). Obsah RAM se ztratí, RAM disky se před
  vypnutím uloží, pracovní kopie médií ve `work/` zůstávají.
- **Nová:** prázdná sestava, nebo kopie šablony či jiné sestavy (uloží se přes „Uložit jako…“).
- **Uložit** zapíše soubor s komentáři z katalogu: u každého jumperu je jeho označení a význam zvolené
  hodnoty. Ruční komentáře z původního souboru se při uložení z konfigurátoru ztratí.
- Dokud v konfigurátoru nejsou neuložené změny, sleduje běžící sestavu (i když ji změní MCP).

## Mapa portů a paměti

Záložka „Mapa portů a paměti“ pod rámem (a MCP nástroj `machine_map`) ukazuje, která karta odpovídá na
který port a paměťovou adresu.

- **Zdroj je dekódování samotných karet:** tytéž PROM dumpy a jumpery, podle kterých karta odpovídá za běhu
  (JPR-1ZI D3C/D6B, DMR-1 D2C a JP3, AND-1Z D1 a S1, ZRMD-1 GAL, IDE-1 JP1–JP5). Karty se pro výpočet postaví ve
  zkušebním režimu, takže se neotevírají ani nekopírují obrazy disků.
- **Paměť:** pruh 64 KB, v němž má každá karta svou barvu. EPROM procesorové karty (jen po resetu, BOOT = 1)
  je proužek nahoře, protože se čte na kartě a zápis jde na sběrnici. Pod pruhem je tabulka rozsahů s funkcí
  a přístupem (R = čtení, W = zápis).
- **Porty:** tabulka rozsahů. Neemulované sériové karty (DSM-1) jsou v mapě s porty z konfigurátoru, ostatní
  neemulované karty jsou vypsané jako „dekódování neznámé“.
- **MAP1/MAP2:** když na nich mapa závisí (AND-1Z se S1, DMR-1 s A6/A7 z MAP), jde vybrat stav linek.
- **Kolize:** dvě karty, které na čtení budí sběrnici na stejné adrese, jsou chyba a sestava se nezapne. Jsou
  červeně v pruhu i v tabulce. Zápis do dvou karet je varování. Nikým neobsazené adresy čtou FFh.

## Šablony ZPS

`machines/zps/ZPS-1.sapi` až `ZPS-4.sapi` podle https://sapi.cz/sapi/doku2/zps.php:

| Sestava | Karty | Klávesnice |
|---|---|---|
| ZPS-1 | JPR-1, REM-1, AND-1, DSM-1, BDK-1 | ANK-1 |
| ZPS-2 | JPR-1, REM-1, RAM-1, AND-1, DSM-1, BDK-1 | ANK-1 |
| ZPS-3 | JPR-1A, RAM-1A, AND-1A, DSM-1, RPD-1 (2 sloty) | Consul 259.11 |
| ZPS-4 | JPR-1Z, RAM-1Z, AND-1Z, DSM-1, RPD-1, RMD-1 | Consul 259.11 |

Pořadí karet ve slotech je odhad (viz Nejasnosti v `docs/SOUHRN.md`). **ZPS-1 běží** s MIKROBASICem 2.4,
**ZPS-2** s Monitorem V4.0 a MIKOSem 4.0F (RAM-1/48K, programy z kazety, `docs/software/MIKOS.md`).
`ZPS-2_MIKOS_V5.sapi` je ZPS-2 s Monitorem V5.0F, MIKOSem V5.0, klávesnicí Consul 262.3 a grafickým displejem
DGD-1 (C000–FFFF, RAM-1 proto jen 4000–BFFF). Grafika má vlastní okno.
`ZPS-2_MIKOS_D.sapi` je ZPS-2 s disketovým MIKOSem/D V5.0, řadičem RPD-1 (dva sloty) a 8" mechanikou se
systémovou disketou (`docs/desky/RPD-1.md`). BDK-1
(bastldeska) je prázdná deska pro vlastní zapojení uživatele, na sběrnici nic nedělá a jen zabírá slot. ZPS-3 a ZPS-4 čekají na emulaci JPR-1A a JPR-1Z.
Navíc je tu `machines/zps/MIKROBASIC_AR1983.sapi`: nejmenší sestava z AR/B 1983 (JPR-1 s EPROM a 1 KB RAM,
AND-1, ANK-1). Popis v `docs/software/MIKROBASIC.md`.

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

`sapiemu --ui-snapshot soubor.png [--machine …] [--ui-open sestava.sapi] [--ui-rack SLOT] [--ui-map]` vykreslí GUI do
skrytého okna, uloží obrázek a skončí. Rozložení oken uživatele (`imgui.ini`) se nemění.
