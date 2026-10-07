# Sestavy a konfigurátor

Sestava je popis počítače: sběrnice, karty ve slotech a jejich jumpery. Je uložená v souboru `.sapi`
(JSON s komentáři). Emulátor sestavy hledá ve složce `machines/` vedle sebe (i v podsložkách), ale soubor může
ležet kdekoli: otevírá se přes Soubor → Otevřít soubor… nebo v konfigurátoru.

## Konfigurátor (Stroj → Sestava…)

- **Čelní pohled na rám:** sloty X1…Xn zprava doleva, vpravo panely SPN a ZDR.
  - Panely jsou kreslené podle tiskových podkladů hliníkových štítků z rámu Libora Lasoty
    (`H:\SAPI-1\LiborLasota\Chassis\Potisk_ARTAX`, rozměry v bodech přímo z PDF): úzký panel 57 × 491,8 bodu,
    SPN dvě pozice, ZDR sedm pozic, otvory, konektory K2/K3 (D-sub), LED ZAM/DSK, popisky.
  - **Konektory:** desky původní konstrukce mají konektory FRB v hranatých otvorech, desky V většinou D-sub
    (autor). Rozměry jsou ze štítků: otvor FRB 22,7 × 150,2 bodu vlevo od středu (první sada, DSM/FDC/JPR), D-sub
    podle druhé sady. FRB je nakreslený jako zelený obdélník se zlatými piny ve dvou řadách.
    - všechny JPR (i JPR-1V) a FDC: K3 nahoře, K2 dole, JPR mezi nimi kulatý otvor pro LED. **LED svítí podle
      emulace** (u běžící sestavy): JPR-1ZI, JPR-1Z, JPR-1V a JPR-1A = BOOT (od resetu do `OUT 00h`), JPR-1 = LED
      na P1-OUT D7 (zápis na 2800h);
    - DSM-1: K3 terminál (X2), K2 magnetofon (X3, „MGF“); DMR-1: jen K3 terminál (X2) na stejném místě;
    - AND-1, AND-1A, AND-1Z, AND-1Z-89 a DGD-1: jeden FRB dole (K2), jako na deskách;
    - RPD-1: horní FRB obou desek (RPD-1A vpravo, RPD-1B vlevo) propojuje kabel KB-17, dolní FRB na RPD-1B
      („K2 FDD“) vede k mechanikám; svislé typy „RPD-1A“ a „RPD-1B“;
    - FDC-1: stejně, horní FRB obou desek (FDC-1A vpravo, FDC-1B vlevo) propojuje stejný kabel, k mechanikám vede
      dolní FRB na FDC-1A („K2 FDD“), dolní FRB na FDC-1B má vlastní signály SEL, HL a MOT;
    - IDE-1: nahoře IDC 40 pro disk („HDD“, SV1), LED, dole D-sub 15 dvouřadý pro PMD32-SD („PMD32“, X2). LED na
      skutečné desce není, emulátor ji přidává: svítí 50 ms po přístupu na disk nebo PMD32-SD;
    - DSM-1V: K3 „A“, K2 „B“ (D-sub); AND-1V, AND-1B, DGD-1V a CGA: K2 VGA uprostřed; BGP-1S: FRB K2 (TV);
      MPH (K4 OVL, K3 ZVUK, K2 MYŠ);
      ZRD-1V (LED podle rev 2.1: ZAM červená = zámek zápisu nastavený programem, odemkne reset; DSK zelená svítí
      20 ms po přístupu na 60h–63h; rev 1.1 LED nemá);
    - bez konektoru (autor): ZRMD-1, RAM-1, RAM-1A, RAM-1Z, RAM-1V, REM-1. Ostatní karty se projdou postupně.
  - Volba „Typy karet na panelech“ ukáže přesný typ karty ve volném místě panelu.
  - Sériové karty (DMR-1, DSM-1, DSM-1V) bývají v sestavě víckrát, proto je **port u každého sériového
    konektoru** („K3 #10“, u DSM-1V „#10“ nad K3 „A“ a „#14“ pod K2 „B“). Typ karty na panelu je bez portu.
    V seznamech, bublinách a v okně Sériový terminál zůstává jméno s portem („DMR-1 #10“).
  - Dvě karty na stejném portu jsou chyba (sestava se nezapne). Dvě DMR-1 se zapnutou RAM nebo se stejným
    souborem baterie jsou varování. Druhá DMR-1 se obvykle používá jen jako sériový port: RAM vypnout
    (parametr „RAM 128 KB“) a JP2 nastavit na jiný port.
  - X1 je procesorová karta (výjimkou je ARB-1C s JP3 = 1-2 pro PKT-1).
  - Počet slotů určuje typ sběrnice.
  - Karty, které zatím nejsou emulované, jsou šedé. Červená tečka znamená chybu (např. karta mimo X1).
- **Klik na slot** vybere kartu a pod rámem se ukáže její nastavení: výběr karty, jumpery a soubory ROM nebo
  obrazů s nabídkou známých souborů a tlačítkem `…`. Pravé tlačítko otevře rovnou seznam karet. Klik na rám
  mimo karty ukáže název a popis sestavy a nastavení sběrnice.
- **Přetažení karty myší** na jiný slot (od 2026-10-07): karta se zvedne a jede za myší. Zelený cíl = přesun na
  volné místo nebo prohození se stejně širokou kartou, která tam začíná; červený = nejde (procesorová karta
  patří do X1, nevejde se do rámu, jiná šířka), důvod je v bublině. Esc tažení zruší. Sloty sběrnice jsou
  rovnocenné, proto se přesun provede rovnou i v běžícím stroji: sestava je jen neuložená, restart netřeba.
  Oranžový (až po restartu) je přesun jen tam, kde běžící stroj v dotčených slotech ještě nemá to, co editor.
  „Vrátit změny“ vrátí karty na uložená místa i v běžícím stroji. Snímek: `--ui-drag Z:DO` (karta ještě
  v ruce), `--ui-drag Z:DO:drop`.
- **Panel ZDR:**
  - NAPÁJENÍ je vypínač s aretací. Za chodu počítač vypne: obrazovka a LED zhasnou, RAM se ztratí. SRAM
    DMR-1 s baterií (JP7) a RAM disky se uloží, spojení TCP a COM se zavřou. Další stisk počítač zapne se
    sestavou z konfigurátoru, včetně neuložených změn. Totéž je v menu Stroj → Vypnout/Zapnout a v MCP
    (`power`). Tlačítko „Zapnout s touto sestavou“ (za chodu „Restartovat s touto sestavou“) dělá vypnutí
    a zapnutí jedním krokem;
  - RESET funguje jako F12;
  - TURBO a BIOS patří kartě CPU jako na Liborově rámu: u JPR-1V TURBO přepíná CPU 2/4 MHz (svítí = 4 MHz) a BIOS
    propojku BIOS, tedy banku EEPROM (svítí = banka 1, čte se po RESETu). Ostatní karty CPU je nemají, tlačítka jsou
    šedá. Rychlost emulace je zvlášť (F8, menu Stroj → Rychlost emulace);
  - displej FREKVENCE CPU ukazuje efektivní takt CPU: takt karty × naměřená rychlost emulace (od 100 bez desetin).
- **Změny se projeví až po vypnutí a zapnutí** (rozhodnutí autora). Obsah RAM se ztratí, RAM disky se před
  vypnutím uloží, pracovní kopie médií ve `work/` zůstávají.
- **Co běžící stroj ještě nemá, je oranžové** (od 2026-10-06): tlačítko „Restartovat s touto sestavou“, pruh pod
  ním, označení slotu (X5 s tečkou) a pole parametru s tečkou před názvem; bublina u parametru ukáže hodnotu
  běžícího stroje. I se zavřeným konfigurátorem je vpravo nahoře „● Sestava změněna: restartovat“ (kliknutí
  otevře konfigurátor). Porovnává se s popisem, se kterým stroj běží; název a popis sestavy ani hodnota
  zapsaná stejná jako výchozí se nepočítají, parametry „(platí hned)“ se neoznačují.
- **Vrátit změny:** když jsou oranžové změny, vrátí konfigurátor na sestavu běžícího stroje (i třeba smazanou
  kartu po uložení nebo u neuložené nové sestavy), jinak na naposledy uloženou sestavu.
- Parametr „(platí hned)“ změněný jinde než v konfigurátoru (MCP `turbo`) se do konfigurátoru převezme, změna
  v konfigurátoru jde do stroje.
- **Výjimka: parametry označené „(platí hned)“** se u běžící sestavy použijí okamžitě, jako přepínač na skutečné
  desce: barva obrazu, režim zápisu RAM disku ZRMD-1/ZRD-1V (`mode`), přepínače S1–S4 ZRMD-1, TURBO a BIOS
  JPR-1V a věrný režim klávesnice („Klávesnice: věrně jako hardware“, `keyboard.strict`). Uložit je potřeba
  jako každou změnu sestavy. Při přepnutí z `rw` se RAM disk nejdřív uloží, po přepnutí na `rw` se uloží jeho
  aktuální obsah.
- **Nová:** prázdná sestava, nebo kopie šablony či jiné sestavy (uloží se přes „Uložit jako…“).
- **Uložit** zapíše soubor s komentáři z katalogu: u každého jumperu je jeho označení a význam zvolené
  hodnoty. Ruční komentáře z původního souboru se při uložení z konfigurátoru ztratí.
- **Konfigurátor ukazuje běžící sestavu.** Když má neuložené změny a má se nahradit jinou sestavou (menu
  Soubor → Otevřít sestavu, Otevřít soubor…, v konfigurátoru Nová a Otevřít…, i ukončení emulátoru), nejdřív
  se zeptá: **Uložit / Uložit jako… / Zahodit / Zrušit**. Zrušit akci nechá být. Když sestavu přepne MCP,
  dotaz přijde hned potom a Zrušit ponechá rozpracované změny v konfigurátoru. Vypnutí a zapnutí stejné sestavy
  (NAPÁJENÍ, „Zapnout s touto sestavou“) se neptá a změny zachová.

## Okna obrazovek a barva obrazu

- **Každý videovýstup má své okno pojmenované podle karty:** „AND-1V“, „AND-1Z-89“, „CGA-1V“, „DGD-1“… Konzole
  (první textový displej) je hlavní okno, grafické karty mají další. Při vypnutém stroji se hlavní okno jmenuje
  „Obrazovka“. Za jménem je aktuální zvětšení v celých procentech („CGA-1V  200 %“): body obrazovky PC na bod
  emulovaného obrazu, svisle (s poměrem TV se vodorovně liší).
- **Rychlé zvětšení:** pravé tlačítko na obraz nabídne 100 %, 200 %, 300 % a 400 %, okno se nastaví kolem
  obrazu s malým okrajem. Pro
  hlavní obrazovku je totéž v menu Zobrazení → Zvětšení obrazovky. Změna velikosti okna rukou vrátí přizpůsobení
  oknu (a celočíselné zvětšení, je-li zapnuté).
- **Barva jednobarevného obrazu je parametr karty v sestavě:**
  - AND-1V: propojky S1 (`color`: white, green, amber, red, blue, cyan, yellow, magenta), jako na desce;
  - AND-1, AND-1A, AND-1Z, AND-1Z-89 a DGD-1 barvu nemají, ta je dána luminoforem monitoru: parametr `monitor`
    (white, green, amber), v konfigurátoru „Monitor: barva luminoforu (není na kartě)“;
  - CGA-1V je barevná a barvu nemá.
  - AND-1B: propojky S1 (`color`) jako AND-1V, výchozí zelená (autorova deska);
  - BGP-1S: barvy dávají atributy (Colorace / RGBM); v menu Barva se volí výstup K2: `rgb` (barevná TV, SCART) nebo
    `mono` (černobílá TV, kompozit).
- **Menu Zobrazení → Barva** mění tentýž parametr jako konfigurátor. Změna se projeví hned, bez vypnutí, a
  v konfigurátoru se objeví jako neuložená změna sestavy (Uložit). Totéž platí obráceně: barva změněná
  v konfigurátoru se na běžící sestavě ukáže hned.
- Snímky přes MCP `screenshot` a `sapiemu-cli --screenshot` jsou ve stejné barvě.

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
- **MAP1/MAP2:** když na nich mapa závisí (AND-1Z se S1, DMR-1 s A6/A7 z MAP, RAM-1V, CGA-1V…), jsou pod sebou
  pruhy všech čtyř stavů, vždy ve stejném pořadí. Vybraný stav je tlustý (žlutý popisek) a tabulka je pro něj,
  ostatní jsou tenké: jejich mapa je ztlumená tam, kde je stejná jako ve vybraném stavu, a výrazná tam, kde se
  liší (šedá = tam nikdo neodpovídá). Alternativní mapování je tak vidět hned. Kliknutím na pruh nebo popisek se
  stav vybere.
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
DGD-1 (C000–FFFF, RAM-1 proto jen 4000–BFFF). Grafika má vlastní okno „DGD-1“. `ZPS-2_MIKOS_V5_DGD-1V.sapi` je
totéž s Liborovou DGD-1V (VGA, jantarová) místo DGD-1 (`docs/desky/DGD-1V.md`).
`ZPS-2_MIKOS_D.sapi` je ZPS-2 s disketovým MIKOSem/D V5.0, řadičem RPD-1 (dva sloty) a 8" mechanikou se
systémovou disketou (`docs/desky/RPD-1.md`). BDK-1
(bastldeska) je prázdná deska pro vlastní zapojení uživatele, na sběrnici nic nedělá a jen zabírá slot.
**ZPS-3 běží** s originálním monitorem 48K MON V1.0-C1 a CP/M 2.2/P z diskety (`docs/software/CPM_ZPS-3.md`).
`ZPS-3_CPM48K.sapi` a `ZPS-3_CPM62K.sapi` jsou ZPS-3 s Peřinovými monitory MOR48A a MOR62A a řadičem RPD-1,
`ZPS-3_CPM48K_FDC-1.sapi` a `ZPS-3_CPM62K_FDC-1.sapi` totéž s řadičem FDC-1 (8272 + DMA, dva sloty,
`docs/desky/FDC-1.md`). Sestavy 62K mají druhou RAM-1A na C000–F7FF a AND-1A na F800.
`ZPS-3_CPM48K_IMA.sapi` je originální CP/M 46K od Tesly IMA s monitorem MON V1.0-C1 (systémové stopy vyrobené
originálním MOVCPM48, disketa se nedochovala). `ZPS-3_CPM64K_IMA.sapi` je originální CP/M 64K od Tesly IMA (monitor MON64C3 v RAM na F800, druhá RAM-1A na
C000–EFFF a F800–FFFF, AND-1A na F000; obě PROM jsou předpoklad).
`cpm_zrmd_fdc1.sapi` a `cpm_zrmd_mfc1a.sapi` jsou `cpm_zrmd.sapi` s řadičem FDC-1 nebo MFC-1A (`docs/desky/MFC-1A.md`)
a 8" mechanikami. Při prvním zapnutí zvolit 0 (FDC8) nebo 2 (MFC8), B: je disketa CP/M 2.2/P, C: druhá mechanika.
`cpm_boot_mfc1a.sapi`, `cpm_boot_fdc1.sapi` a `cpm_boot_rpd1.sapi` bootují CP/M J. Bíby (2011) z 8" diskety A:
přes MFC-1A, FDC-1 nebo RPD-1 (EPROM `JPR-1ZI_MON*_JB_*`), C: je vlastní RAM disk ZRMD-1, B: disketa CP/M 2.2/P
jako data (`docs/software/CPM_SAPIMAN.md`). Ze systémových disket J. Bíby (`SAPI_SYS`): `cpm_boot_rpd1_ss256.sapi` a `cpm_boot_fdc1_ss256.sapi` (C: je RAM
disk ZRMD-1 sestavy `cpm_zrmd`, stejné rozložení), `cpm_boot_mfc1a_rmd.sapi` (C: RMD-1, inicializace `IRMD40`) a
`cpm_term64k_fdc1.sapi` (CP/M 64K jen s terminálem: okno Terminál nebo TCP 2323) `cpm_boot_dlouhe_rpd1_fdc1.sapi` (`_35` pro 3,5") a `cpm_pmd_rpd1.sapi` (CP/M 170212 s PMD32-SD přes DPP-1).
`cpm_boot_fdc1_rmd.sapi` a `cpm_boot_rpd1_rmd.sapi` jsou bootovací diskety J. Bíby s RAM diskem RMD-1 (inicializace
`B:IRMD40` z diskety Z6), `cpm_pmd_fdc1.sapi` je FDC-1 s PMD32-SD přes DPP-1 (C:, D:). `cpm_boot_rpd1_mfc256.sapi`,
`cpm_boot_rpd1_mfc512.sapi` a `cpm_boot_rpd1_fdc1_rmd.sapi` bootují z RPD-1 a mají druhý řadič pro D:, E: (MFC-1A
s 8" disketami FM 14 × 256 nebo 8 × 512, FDC-1 8"). V `cpm_boot_fdc1_irmd.sapi` si RAM disk RMD-1 připraví
BIOS sám (zjistí i velikost 256 KB / 1 MB). Sestavy s ECMONem a monitory
J. Bíby mají tiskárnu na X3 (`docs/tiskarna.md`).
`cpm_zrmd_dpp1.sapi` je `cpm_zrmd` s deskou DPP-1 a PMD32-SD (ECMON: D:, E:)
(„dlouhé“ CP/M s 3K BIOSem: RPD-1 8" a FDC-1 5,25" HD v jedné sestavě). `cpm_ec1045.sapi` je systém EC1045 s menu FDC8/FDC3/FDC5/HDD
a disketami PC 3,5" a 5,25" HD na FDC-1 (volba 1 nebo 2), mechaniky zapojené jako u EC1045 (MOTOR ON z /HL,
`motor_hl`).
**Desky Libora Lasoty k původním sestavám** (2026-10-06): `machines/jpr1s_bgp1s.sapi` je JPR-1S s MIKROBIOSem 1.02,
grafikou BGP-1S (PMD-85, Colorace) a klávesnicí ANK-1M2; hry z `media/jpr-1s` se nahrají přes Soubor → Nahrát
program (Intel HEX) a spustí `JUMP 4400`, nebo příkazem BOOT po sériové lince redukce Redukce34KB01 (TCP 2325, raw,
19200 Bd). Na K3 má desku diagnostiky (od 2026-10-07, čítač `media/jpr-1x/diag_poll.hex`).
`machines/zps/ZPS-1_JPR-1S_AND-1B.sapi` je ekvivalent ZPS-1: JPR-1S s MIKROBASICem 2.4 v EEPROM, AND-1B
(VGA) a ANK-1M2. `docs/desky/JPR-1S.md`, `BGP-1S.md`, `AND-1B.md`.

**JPR-1X** (2026-10-06, 80C85): `machines/jpr1x_bgp1s.sapi` je JPR-1X s MIKROBIOSem 1.02 z dolní banky ROM, RAM-1/48K,
BGP-1S (přerušení vypnuté) a ANK-1M2 s redukcí (hry jako u `jpr1s_bgp1s`), na K3 deska diagnostiky (čítač
`media/jpr-1x/diag_int.hex`). `machines/zps/ZPS-1_JPR-1X.sapi` je ekvivalent ZPS-1
s MIKROBASICem 2.4 z horní banky (propojka BIOS rozpojená), REM-1 jen s RAM, AND-1, DSM-1 a ANK-1. TURBO (CPU 4 MHz)
jde přepnout za chodu. `docs/desky/JPR-1X.md`, procesor `docs/8085.md`.

**SAPI-1.5, ZPS-6** (2026-10-07): `machines/zps/ZPS-6.sapi` je procesorová deska PKT-1 (Z80, klávesnice Consul 262.3
na 8255, tiskárna), RSW-1 (64 KB RAM, 8251, watchdog), řadič MFC-1A s jednou mechanikou 5,25" DS DD a barevná
semigrafika DBS-1A (64x20, náhradní znakový generátor z fontu AND-1Z). Místo sběrnice SBI-1 je ARB-1C s JP3 = 1-2
(AEN vypnutý, X1 volný). ROM MON-V100690 nabootuje CP/M 2.2 62K z diskety Melsoft (`media/sapi15/*.sp2`).
`docs/software/SAPI-1.5.md`, `docs/desky/PKT-1.md`, `RSW-1.md`, `DBS-1A.md`. ZPS-5 (bez displeje a disket) nemá
dochovaný firmware.

**Sestava V** (`machines/sapi1v.sapi`): Liborovy desky JPR-1V, RAM-1V, AND-1V, DSM-1V, ZRD-1V, IDE-1, CGA-1V
a MPH-1V s ECMON a CP/M z RAM disku. Propojky jsou jako na autorových deskách. CGA-1V (C000h při MAP1 = MAP2 = H) má
vlastní okno „CGA-1V“. Zvuková karta MPH-1V (50h) hraje přes zvuk PC při rychlosti 100 % (Stroj → Zvuk a joystick,
`docs/desky/MPH-1V.md`).
**ZPS-4 běží** s CP/M 2.2/P od Peřiny (`ZPS-4.sapi`, s řadičem FDC-1 `ZPS-4_FDC-1.sapi`) i se systémem ELIS
(`ZPS-4_ELIS.sapi`), včetně RAM disku RMD-1
(`docs/software/CPM_ZPS-4.md`).
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

`sapiemu --ui-snapshot soubor.png [--machine …] [--ui-open sestava.sapi] [--ui-rack SLOT] [--ui-map] [--ui-media] [--ui-printer]`
vykreslí GUI do skrytého okna, uloží obrázek a skončí. `--ui-media` otevře okno Média, `--ui-printer` okno Tiskárna,
`--ui-screen-menu` otevře menu pravého tlačítka obrazovky, `--ui-shot` uloží snímek (Ctrl+F5) do `work/snimky`,
`--ui-video SEKUNDY` nahraje video (Ctrl+Alt+F5) do `work/videa` a obrázek uloží až po jeho dokončení,
`--ui-drag 3:7` táhne kartu z X3 na X7 (s `:drop` ji pustí), `--ui-terminal` okno Sériový terminál (`--ui-term-text 'řádek\nřádek'` do něj vloží text, `--ui-term-sel 0,7-4,12`
v něm vybere od řádku 0, hranice 7 po řádek 4, hranici 12),
`--ui-set SLOT:KLÍČ=HODNOTA` změní kartu v konfigurátoru bez restartu (třeba `--ui-set 5:baud=1200`, ukáže
oranžové značení), `--ui-set SLOT:remove` kartu vyjme,
`--lang en` (nebo `cs`) vybere jazyk. Rozložení oken uživatele
(`imgui.ini`) se nemění. Se `--settings X.json` se ale rozložení ukládá do `X.json.imgui.ini` a při dalším snímku se
použije, proto ho před snímkem smazat.
