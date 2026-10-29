# SAPIemu – první kroky

Emulátor počítače Tesla SAPI-1. SAPI-1 (1983–1990) byl průmyslový řídicí počítač do racku.
Začněte základní sestavou ZPS-1, pak zkuste přístroje. Hry a dema jsou novodobý doplněk na nových deskách.

Betaverze (testovací verze): hodně věcí funguje, ale počítejte s chybami. Budeme rádi za každou zprávu, co nefunguje: Nápověda →
Nahlásit chybu… (do zveřejnění cestou, kterou jste program dostali, potom přes GitHub Issues:
https://github.com/mlukasek/SAPIemu/issues). Anglicky: `GETTING-STARTED.md` (`docs/getting-started.md`).

## Spuštění

1. Rozbalte zip kamkoli (třeba `C:\SAPIemu`). Exe najde ROM, média a sestavy ve složkách vedle sebe.
2. Spusťte **`sapiemu.exe`**. Windows u nepodepsaného programu ukážou varování SmartScreen: „Další informace“
   → „Přesto spustit“.
3. Nic se neinstaluje. Nastavení se ukládá vedle exe (`sapiemu-settings.json`, místa oken `imgui.ini`
   a `imgui-docked.ini`), stejně jako log posledního spuštění `sapiemu.log` a toho před ním `sapiemu.prev.log`
   (hodí se k hlášení chyby). Pracovní kopie disků a RAM disků vznikají ve složce `work\`. Originály v `media\`
   zůstávají netknuté. Když do složky programu nejde zapisovat (např. `Program Files`),
   ukládá se nastavení do `%APPDATA%\SAPIemu\SAPIemu` a `work` do `%LOCALAPPDATA%\SAPIemu\work`.

Potřebujete Windows 10 nebo 11, 64 bit.

## Sestavy

Soubor → **Vybrat sestavu…** otevře výběr sestav, při prvním spuštění se otevře sám, poprvé přes většinu hlavního
okna (asi 75 × 80 %; velikost, kterou mu dáte, si pamatuje). Za ním už běží nejjednodušší
sestava ZPS-1 s MIKROBASICem a ve výběru je vybraná. Nahoře je krátký úvod o SAPI-1 a sestavy jsou rozdělené do
skupin v tomto pořadí: **Základ: historické sestavy Tesla**, **Přístroje a technologie**, **Desky Libora Lasoty
(moderní)**, **Hry a dema (novodobé)** a **Ostatní**. U každé je náhled, krátký popis, karty po slotech
s připojeným příslušenstvím (klávesnice, mechaniky a obrazy disket, tiskárna, zapisovač, magnetofon…) a tlačítko
Spustit; dvojklik sestavu také spustí. U sestav z prvních čtyř skupin se po spuštění otevře **Průvodce sestavou**
s tím, co vyzkoušet (políčko dole ho vypne, Nápověda → Průvodce sestavou ho otevře kdykoli). Bloky kódu v průvodci
napíše do SAPI tlačítko Napsat do SAPI. Soubor sestavy odjinud spustí Soubor → Spustit sestavu ze souboru…,
konfigurátor sestavy (karty v rámu, propojky, mapa portů) je Zobrazení → Konfigurátor sestavy….

Sestavy po skupinách jako ve výběru (soubory jsou ve složce `machines\`):

### Základ: historické sestavy Tesla

Původní základní počítačové sestavy (ZPS) v pořadí, jak šly po sobě.

| Sestava | Co v ní je | Jak začít |
|---|---|---|
| `zps/ZPS-1.sapi` | ZPS-1 s MIKROBASICem 2.4 a membránovou klávesnicí ANK-1 | rovnou píšete v BASICu |
| `zps/ZPS-2.sapi` | ZPS-2 s MIKOSem V4 a magnetofonem | `Q` ukáže zařízení, `D0 1F` vypíše paměť, `.K_D` adresář kazety |
| `zps/ZPS-2_MIKOS_V5.sapi` | ZPS-2 s MIKOSem V5 a grafikou DGD-1 | monitor MIKOS; BASIC EXP je v `media\mikos` (viz níže) |
| `zps/ZPS-3_CPM64K_IMA.sapi` | ZPS-3: originální CP/M 2.2 64K Tesla IMA z 8" diskety (B: MBASIC, COBOL) | `B:`, `MBASIC` |
| `zps/ZPS-4.sapi` | ZPS-4 se Z80: CP/M 2.2/P 62K R. Peřiny z 8" diskety, RAM disk RMD-1 | `DIR`, `TYPE CPM.DOC` |
| `zps/ZPS-6.sapi` | SAPI-1.5 (1990): PKT-1, RSW-1, MFC-1A, barevná DBS-1A, CP/M 2.2 62K z 5,25" diskety | `DIR`, `SETDBS1` (barvy a celá znaková sada), další diskety Melsoft `media/sapi15` v okně Média |

### Přístroje a technologie

Přístroje a zařízení se SAPI-1. Každý je nakreslený v okně emulátoru.

| Sestava | Co v ní je | Jak začít |
|---|---|---|
| `aps5.sapi` | měřicí jednotka APS-5: původní firmware regulace osmi pecí, panel s klávesnicí a simulace pecí (okno APS-5) | tlačítko DEMO, nebo poprvé kalibrace `6R` → `9990` →, typ termočlánku `8R` → `3` →; postup níže |
| `zps/ZPS-2_DPS-1_IMS-2.sapi` | ZPS-2 s deskou DPS-1: přístrojová sběrnice IMS-2, voltmetr Metra M1T 330 a zdroj Tesla BS 525 (okno Měřicí pracoviště) | `.K_L BASIMS`, PLAY, `Y`, `.J`; pak `IMS# O3;"R4D0E"`, `IMS# T3`, `IMS# I A$` v programu |
| `zps/ZPS-2_MIKOS_V5_Minigraf.sapi` | zapisovač ARITMA Minigraf 0507 na desce DPP-1 (okno Minigraf) | `.G=1800` pero na počátek, `.G=1809` nakreslí znakovou sadu |
| `zps/ZPS-2_MIKOS_V5_XY4130.sapi` | grafická jednotka XY 4130 na X3 desky JPR-1, ovladač TESLA DIZ za MIKOSem V5 (okno zapisovače) | PEN v okně zapisovače, `.G=181E` nakreslí znakovou sadu |
| `plotter_ar1990.sapi` | plotter z AR 1990: JPR-1 s programem z přílohy AR řídí Minigraf 0507 (okno Minigraf) | READY (po zapnutí pípá), pak Ukázka nebo TEST; povely do řádku, např. `\x02`, `D400,0` |
| `zps/ZPS-2_MIKOS_K.sapi` | záznamník dat KZD-1 s MIKOSem/K V5.0 (kazeta v okně Média) | `.K` `I` připraví kazetu, `.K` `D` vypíše adresář |
| `rkd1_tesla_cpm.sapi` | CP/M 2.2 Tesla Elstroj 1987 z kazetového disku RKD-1 (CM 5403), shell SH | číslo uživatele (např. `1`) a název oblasti, pak `DIR A:`; pevný disk C:, D: naformátuje `FORMTRKD` (`2`, `3`, Enter, `a`) |
| `rmp1_jb_izot.sapi` | CP/M J. Bíby s magnetickou páskou RMP-1 (IZOT SM 5300) a ukázkovou páskou | `B:DIRMT 0` vypíše pásku, `B:MTDAT` kopíruje mezi páskou a disketou; okno Média ukáže mechaniku |

### Desky Libora Lasoty (moderní)

Nové desky pro SAPI-1 od Libora Lasoty.

| Sestava | Co v ní je | Jak začít |
|---|---|---|
| `jpr1s_bgp1s.sapi` | JPR-1S s grafikou BGP-1S (PMD-85) a MIKROBIOSem 1.02 | Soubor → Nahrát program `media/jpr-1s/manic_miner.hex`, pak `JUMP 4400` a Enter |
| `sapi1v.sapi` | Liborovy desky V: CP/M, barevná grafika CGA-1V, zvuk MPH-1V | `3` = CP/M z RAM disku A: a IDE disk B:, C: (volby 0–2 čekají na řadič disket, který sestava nemá, jako skutečná deska; pak `3` hned po resetu F12, nebo Ctrl+F12) |
| `mikromon.sapi` | MikroMon 1.3 (Libor Lasota) | `?` + Enter vypíše příkazy |

### Hry a dema (novodobé)

SAPI-1 nebyl herní počítač. Porty her a dema vznikly pro nové desky a běží i na skutečném hardwaru.

| Sestava | Co v ní je | Jak začít |
|---|---|---|
| `sapi1v_hry.sapi` | **hry a dema** na sestavě V s grafikou CGA-1V i DGD-1V: Bad Apple!!, FXS4, Flappy (CGA-1V a DGD-1V), Bomberman, Bratrstvo kočičí pracky, dema a hudba L. Lasoty | `3`, pak `DIR` a jméno programu (viz Vyzkoušejte) |
| `sapi1v_PoP.sapi` | sestava V s Prince of Persia (grafika a hudba DOS verze) na RAM disku | `3`, pak `POP` |
| `sapi1v_BomberNet.sapi` | síťová hra BomberNet 0.3.0 na sestavě V s kartou DPI-1L (NET přes relay `ws://api.mzpico.com/net`, do sítě až po volbě HOST nebo JOIN ve hře; SD karta ve složce `work/dpi-1l/sdcard`) | `3` = CP/M jako `sapi1v`; síťovou hru spustí BomberNet (`docs/desky/DPI-1L.md`) |
| `jpr1s_bgp1s.sapi` | Manic Miner na JPR-1S a BGP-1S (ve výběru „Manic Miner (JPR-1S + BGP-1S)“) | jako v Liborových deskách; Ctrl+F7 režim pro hry |

### Ostatní

Další sestavy ZPS a jejich varianty, CP/M s ECMONem, CP/M J. Bíby, RKD-1 s mechanikou CM 5400 a další sestavy
s Liborovými deskami.

| Sestava | Co v ní je | Jak začít |
|---|---|---|
| `zps/ZPS-1_JPR-1S_AND-1B.sapi` | ekvivalent ZPS-1 z Liborových desek: JPR-1S, AND-1B (VGA), ANK-1M2 | rovnou píšete v BASICu |
| `zps/ZPS-1_JPR-1X.sapi` | ekvivalent ZPS-1 s JPR-1X: MIKROBASIC 2.4 z horní banky ROM | rovnou píšete v BASICu |
| `zps/ZPS-2_MIKOS_D.sapi`, `zps/ZPS-2_MIKOS_V5_DGD-1V.sapi` | ZPS-2 s MIKOSem/D z 8" diskety, ZPS-2 s MIKOSem V5 a grafikou DGD-1V | monitor MIKOS |
| `zps/ZPS-3.sapi`, `zps/ZPS-3_CPM48K.sapi`, `_CPM48K_FDC-1`, `_CPM62K`, `_CPM62K_FDC-1`, `zps/ZPS-4_ELIS.sapi`, `zps/ZPS-4_FDC-1.sapi` | CP/M 2.2/P z 8" diskety (RPD-1, u `_FDC-1` řadič FDC-1), 48K a 62K, ELIS | naběhne CP/M |
| `zps/ZPS-3_CPM48K_IMA.sapi` | originální CP/M 2.2 46K Tesla IMA s MON V1.0-C1 (systém z MOVCPM48) | `DIR`, `DIR B:` |
| `cpm_zrmd.sapi` | CP/M 2.2 z RAM disku ZRMD-1 (ECMON), s tiskárnou | po startu stiskněte `3` (B:, C: bez IDE-1 ukáží XYXY, to je v pořádku), pak `DIR`; `^P` a `DIR` vytiskne výpis (okno Tiskárna) |
| `cpm_ide.sapi` | totéž s IDE diskem a PMD32-SD | po startu `3` |
| `cpm_zrmd_fdc1.sapi`, `cpm_zrmd_mfc1a.sapi` | totéž s řadičem disket FDC-1 nebo MFC-1A a 8" disketami | po startu `0` (FDC-1) nebo `2` (MFC-1A), pak `DIR B:` |
| `cpm_zrmd_dpp1.sapi` | `cpm_zrmd` s deskou DPP-1 a PMD32-SD | po startu `3`, pak `DIR D:`, `DIR E:` |
| `cpm_zrmd_rtc_dsp.sapi` | navíc deska reálného času RTC-1 a sériová DSP-1 (programy J. Bíby) | po startu `3`, `E:RTCZ` hodiny RTC-1 (ESC konec), `RTC421` hodiny ZRMD-1, `DOPCP soubor` pošle soubor XMODEMem do okna terminálu „DSP-1 X2 #48“ |
| `cpm_ec1045.sapi` | systém EC1045: FDC-1 s disketami PC 3,5" a 5,25" HD | `1` = 3,5", `2` = 5,25" (vložit disketu v okně Média), pak `DIR B:` |
| `cpm_boot_mfc1a.sapi`, `cpm_boot_fdc1.sapi`, `cpm_boot_rpd1.sapi` | CP/M J. Bíby (2011) bootovaný z 8" diskety | naběhne CP/M, na A: jsou zdrojáky systému |
| `cpm_boot_*_ss256.sapi`, `cpm_boot_mfc1a_rmd.sapi` | totéž ze systémových disket J. Bíby (A: programy, B: zdroje), C: ZRMD-1 nebo RMD-1 | naběhne CP/M (výzva `U0A>`) |
| `cpm_boot_fdc1_rmd.sapi`, `cpm_boot_rpd1_rmd.sapi` | CP/M J. Bíby s RAM diskem RMD-1 (FDC-1, RPD-1) | `B:IRMD40`, odpověď `A`, pak C: |
| `cpm_boot_fdc1_irmd.sapi` | CP/M J. Bíby, RAM disk RMD-1 si připraví BIOS sám (zjistí i velikost) | `SAVE 2 C:X.COM`, `B:STAT C:` |
| `cpm_boot_rpd1_mfc256.sapi`, `_mfc512`, `cpm_boot_rpd1_fdc1_rmd.sapi` | CP/M J. Bíby z RPD-1 s RMD-1 a druhým řadičem pro D:, E: (MFC-1A s 8" FM 14 × 256 nebo 8 × 512, FDC-1 8") | `B:IRMD40`, `A`, pak `SAVE 2 D:X.COM`, `DIR D:` |
| `cpm_boot_dlouhe_rpd1_fdc1.sapi` (`_35`: 3,5") | „dlouhé“ CP/M J. Bíby: A:, B: 8" (RPD-1), C: ZRMD-1, D:, E: 5,25" HD (FDC-1) | naběhne CP/M, zkuste `DIR B:`, `STAT D:` |
| `cpm_pmd_fdc1.sapi` | CP/M J. Bíby: FDC-1 a PMD32-SD přes DPP-1 | `DIR C:` (Turbo Pascal), `DIR D:` (WordStar) |
| `cpm_pmd_rpd1.sapi` | CP/M J. Bíby s PMD32-SD přes DPP-1 (C:, D: = 5,25" diskety J. Bíby na SD kartě) | `DIR C:` (Turbo Pascal), `DIR D:` (WordStar) |
| `cpm_term64k_fdc1.sapi` | CP/M 64K bez obrazovky, jen terminál | Zobrazení → Sériový terminál (nebo TeraTerm na 127.0.0.1:2323) |
| `rkd1_jb_cm5400.sapi` | CP/M J. Bíby s RKD-1 a bulharskou mechanikou CM 5400 (BIOS pro D: až G: vznikl v emulátoru) | `B:FORMRKD`, mechanika `F`, `O`, `A` naformátuje a zkontroluje kazetu (12 × 512 B), pak `DIR F:`, `B:PIP F:=B:*.COM` |
| `jpr1x_bgp1s.sapi` | JPR-1X (8085, TURBO) s RAM-1, BGP-1S a MIKROBIOSem 1.02 | jako `jpr1s_bgp1s`; TURBO tlačítkem na panelu ZDR nebo v menu Stroj |
| `libor_doma.sapi` | Liborova sestava na ImBUS12: desky V s DGD-1V i CGA-1V (střídají se podle MAP2), bez IDE | `3` = CP/M z RAM disku (disky B:, C: bez IDE-1 nefungují) |

## Ovládání

- **Psaní:** klikněte do okna obrazovky. Text jde na klávesnici SAPI (české znaky podle klávesnice sestavy). Dokud
  obrazovka klávesnici nemá a nic jste nenapsali (nebo se psaný znak právě zahodil), je dole na obrazu nápis
  „Klikněte sem a pište“. Zahozený znak ohlásí i stavový řádek. Po „Napsat do SAPI“ v průvodci, Spustit, Nahrát
  program, vložení média, Vložit text a po zavření průvodce nebo výběru sestavy dostane klávesnici zpět displej
  použitý naposledy. Po startu sestavy je to její hlavní výstup (u her CGA-1V nebo BGP-1S).
- **Klávesy emulátoru** (přehled v Nápověda → Klávesové zkratky; při psaní do textového pole se neuplatní):
  - F5 pauza;
  - **Ctrl+F5 snímek obrazovky** okna, které má fokus (konzole nebo grafika CGA-1V, BGP-1S, DGD…), do
    `work/snimky/<karta>_<datum-čas>.png`; pravé tlačítko na obraz nabízí i „Uložit snímek jako…“, „Kopírovat
    snímek“ (do schránky jako PNG i BMP) a „Snímek zvětšený 3× (jak je vidět)“, jinak přesné body emulace;
  - **Ctrl+Alt+F5 video se zvukem** (zapnout / vypnout, také pravé tlačítko na obraz a Zobrazení): MP4 do
    `work/videa/<karta>_<datum-čas>.mp4`, potřebuje **ffmpeg** (`winget install ffmpeg`, nebo cesta v nastavení
    `"video": { "ffmpeg": "…" }`). Snímky se berou podle času emulace, takže video odpovídá skutečné rychlosti
    SAPI i při turbu; pauza video pozastaví. Body jsou zvětšené 4× (u VGA 2×), nejostřejší je přehrání v nativní
    velikosti. Ve stavovém řádku běží „● REC m:ss“;
  - **Alt+Enter celá obrazovka** (také Zobrazení → Celá obrazovka a pravé tlačítko na obraz): displej použitý
    naposledy sám přes celý monitor, bez menu a ostatních oken. Ukončí ji zase Alt+Enter nebo menu pravého
    tlačítka, Esc patří SAPI. V menu pravého tlačítka jde přepnout na jiný displej sestavy, celočíselné zvětšení
    a poměr TV, pauza zůstane napsaná přes obraz;
  - F8 rychlost (100 / 200 / 400 % / maximum); pauza i nová rychlost se na chvíli ukážou přes obraz, při pauze
    je v titulku okna „[pauza]“;
  - F12 reset (paměť zůstane; u APS-5, která reset nemá, se emulátor nejdřív zeptá);
  - **Ctrl+F12 vypnout a zapnout** (Stroj → Vypnout a zapnout): jako síťový vypínač, paměť se ztratí, karty,
    přepínače a vložená média zůstanou. Pomůže, když program visí a reset nestačí (třeba volba 0–2 v ECMONu
    u sestavy V);
  - ladění jako ve Visual Studiu: F5 běh / pauza, F10 krok přes, F11 krok, **F9 breakpoint** v debuggeru (na řádku
    disassembleru pod myší, jinak na PC; bez otevřeného debuggeru jen připomene, že reset je F12);
  - **Ctrl+Shift+C** zkopíruje text obrazovky do schránky (také Zobrazení → Kopírovat text obrazovky a pravé tlačítko
    na obraz konzole; v sériovém terminálu kopíruje jeho text);
  - v oknech s otázkou Esc zruší nebo zavře a Enter stiskne zvýrazněné tlačítko (Nahrát, Vložit, Zavřít, u neuložených
    změn Zrušit); kde by šlo o něco nevratného (reset APS-5, Obnovit z originálu, zápis systému RKD-1), Enter nedělá nic;
  - **Tab a šipky** přecházejí v oknech emulátoru (dialogy, Nastavení, konfigurátor, přístroje) mezi prvky: rámeček
    ukáže až Tab, pak ho posouvají i šipky, mezerník a Enter stisknou prvek v rámečku a kliknutí myší rámeček skryje
    (šipky a mezerník bez rámečku nic nestisknou, třeba když hrajete a okno obrazovky zrovna nemá fokus); v okně
    s otázkou začíná Tab na bezpečném tlačítku (Zrušit). Ctrl+Tab přepíná okna (seznam otevřených oken je
    i v Zobrazení → Okna). V okně obrazovky SAPI a v sériovém
    terminálu patří šipky, mezerník, Enter, Esc i Alt počítači SAPI;
  - **Ctrl+F2 / Ctrl+F3** rychlé uložení a načtení stavu;
  - Ctrl+F7 režim pro hry u klávesnice ANK-1 / ANK-1M2 (klávesy podle polohy, Shift = SHIFT; Manic Miner);
  - **Ctrl+Shift+V** vloží text ze schránky na klávesnici SAPI (také Stroj → Klávesnice → Vložit text), třeba
    program v BASICu;
  - Ctrl+F8 joystick z klávesnice (šipky, mezerník);
  - Ctrl+F10 pustí zachycenou myš.
- **Sestava a karty:** Zobrazení → Konfigurátor sestavy…. Rám s panely karet, propojky karet, mapa portů a paměti. Panely jsou
  nakreslené podle modernizované skříně Libora Lasoty s popsanými panely, původní Tesla SAPI-1 měl holé karty za
  krycím panelem. Kartu změníte kliknutím na slot, na jiný slot ji přetáhnete myší (na obsazený se prohodí).
  Některé propojky platí hned („platí hned“), ostatní změny se projeví až tlačítkem „Restartovat s touto
  sestavou“. Co běžící stroj ještě nemá, je oranžové (i vpravo nahoře v hlavním okně). Při odchodu s neuloženými
  změnami se program zeptá: Uložit přepíše soubor sestavy (i dodanou v `machines\`), Ponechat pro příští spuštění
  nechá soubor být a změny použije při **každém** dalším spuštění sestavy, dokud je neuložíte nebo v konfigurátoru
  nestisknete Vrátit změny (stavový řádek to při startu řekne, výběr sestav ji označí `*`).
- **Zobrazení:** barva obrazu, zvětšení (pravé tlačítko na obraz; Zobrazení → Zvětšení obrazovky platí pro displej
  použitý naposledy, první volba „Přizpůsobit oknu“), celá obrazovka, celočíselné škálování, poměr stran TV,
  **Jazyk / Language** (čeština, angličtina; poprvé podle Windows). Okna displejů nejdou sbalit a všechna okna se
  posouvají jen za titulek (tažení po obrazu nebo panelu okno nehne).
- **Rozmístění oken:** při prvním spuštění zabere okno programu 88 % obrazovky. Hlavní výstup sestavy (konzole,
  u her CGA-1V nebo BGP-1S) je vlevo nahoře, další displeje pod ním nebo vedle něj. Samo rozmístění jim dá
  celočíselné zvětšení nejvýš asi 200 % (při měřítku Windows 125 a 150 % až 300 %, aby byl obraz zhruba stejně
  velký) a okno velikosti obrazu s malým okrajem. Další displej nebude o moc větší než hlavní výstup. Jiné zvětšení
  si zvolíte v menu. Průvodce je vedle displejů, přístroje, Média a debugger si najdou volné místo, dialogy jsou
  uprostřed. Když zbude dost místa (asi 704 × 704 bodů při 100 %, na notebooku ne), otevře se při prvním spuštění
  i **Konfigurátor sestavy**. Nebere klávesnici a okna otevřená později ho mohou zakrýt. Okna si pamatují, kam jste
  je dali; výchozí místa vrátí Zobrazení → Výchozí rozložení oken (konfigurátor, který otevřelo samo rozložení
  a vy jste ho nepoužili, se přitom znovu otevře, jen když je místo). Po spuštění jiné sestavy si okna displejů
  nechají místo a dostanou velikost nového obrazu s malým okrajem; zvětšení zvolené v menu zůstane, okno zvětšené
  v tomto běhu tažením za okraj si nechá velikost. Okno víc než napůl mimo hlavní okno (po jeho zmenšení, na menším
  monitoru) a dialog (Vybrat sestavu, Nastavení…) i jen částečně mimo se při startu a po změně velikosti hlavního
  okna vrátí dovnitř.
- **Okno schované za jiným:** Zobrazení → **Okna** vypíše všechna otevřená okna (displeje, přístroje, Média,
  průvodce, ladicí okna, konfigurátor…). U zakrytého je vpravo „zakryté“ a kliknutí ho dá dopředu (s klávesnicí; okno
  částečně mimo hlavní okno se posune dovnitř). Stejně fungují položky oken v Zobrazení a Ladění: zavřené okno
  otevřou, zakryté (zakryté jiným oknem, sbalené nebo nevybraná záložka) dají dopředu a teprve okno vpředu zavřou.
  Zaškrtnutí dál znamená, že je okno otevřené. Okno přilepené do zadokované plochy plovoucí okno před sebou
  nepřeskočí, zakryté je proto jen sbalené nebo jako nevybraná záložka. **Zobrazení → Přeskládat otevřená okna** dá
  otevřená okna na místa výchozího rozložení a zvětšení zvolené v menu nechá, zavřená okna si svá místa nechají
  (Výchozí rozložení oken je zapomene). V zadokovaném rozložení postaví dok znovu, i se zavřenými okny.
- **Okno přilepené ke kraji:** okno přetažené ke kraji hlavního okna se přilepí (zadokuje) a jeho velikost pak
  určuje přilepená plocha. Odlepí se tažením **za název** v záložce titulku, volbou zvětšení (100–400 %) nebo
  **Zobrazení → Výchozí rozložení oken**, které vrátí všechna okna na výchozí místa a velikosti. S podrženým
  Shiftem se okno při tažení nepřilepí.
- **Zadokovaná okna:** Zobrazení → Nastavení → Vzhled → **Okna: Zadokovaná** dá hlavní displej doprostřed a ostatní
  okna do sloupce u pravého kraje: nahoře další displeje, pod nimi okna sestavy (přístroje, Média, terminál,
  tiskárna), dole průvodce a ladicí okna, každá skupina jako záložky. Část bez otevřeného okna místo nezabírá. Dělicí
  čáry jde posouvat, záložku jde vytáhnout ven. Po spuštění jiné sestavy jde doprostřed její hlavní displej (displej,
  který tam byl, si s ním vymění místo), zbytek rozložení zůstane. Výchozí rozložení oken vrátí zadokované rozložení
  pro běžící sestavu.
- **Každé rozložení si pamatuje svá místa oken** (plovoucí v `imgui.ini`, zadokované v `imgui-docked.ini` vedle
  nastavení). Volba Plovoucí nebo Zadokovaná vrátí okna tam, kde jste je v tom rozložení nechali; poprvé dostanou
  jeho výchozí místa.
- **Média:** mechaniky jsou očíslované podle řadiče (0–3) a za číslem je jejich písmeno v CP/M (ze sestavy,
  nebo „asi B:“ zjištěné za chodu). Zobrazení → Média: kazety (WAV, MP3, nebo program HEX/BIN, který se převede na kazetu MIKOS či MIKROBASIC; „Uložit jako WAV…“ pro skutečné SAPI) pro DSM-1, diskety RPD-1, FDC-1 a MFC-1A (obrazy `.img` a
  ImageDisk `.imd`; nová disketa v libovolném formátu: 8", PC 3,5"/5,25" HD nebo DD; s příponou `.imd` ji
  pak program pro formátování naformátuje i jinak, třeba s jiným číslováním sektorů), IDE disk a PMD32-SD.
  Výměna média v okně Média se zapíše do sestavy (konfigurátor ji ukáže, sestava je pak neuložená). Tlačítko
  „…“ u média nabízí **Obnovit z originálu** (jen u média z `media\`): médium z `media\` se používá přes pracovní
  kopii ve `work\`, která se
  dá kdykoli vrátit do původního stavu. Změny na páskách, kazetách, disketách a RAM discích se do souboru zapisují
  průběžně: pár sekund po posledním zápisu, hned při pauze (F5) a při přepnutí do jiného okna Windows, jinak při
  vysunutí, vypnutí a ukončení. Soubor se přepisuje přes dočasný soubor, takže ani pád nenechá napůl zapsaný obraz.
  Špatný obraz se do mechaniky nevloží, zůstane v ní původní médium. První Vložit… nebo Nahrát program začne ve
  složce `media\`, pak v naposledy použité.
- **Chyby:** odmítnuté médium, nepovedená nová disketa nebo kazeta, nepovedené nahrání programu a nepovedené
  zapnutí sestavy ukáže okno Chyba, povedené nahrání stavový řádek („Nahráno 0100-1FFFh, 7936 B: …“). Upozornění
  ve stavovém řádku svítí 7 s a zůstanou v Logu. Červené **✖ Chyby: N** ve stavovém řádku znamená chyby zapsané do
  Logu od jeho posledního otevření; kliknutím se Log otevře (chyby červeně, varování žlutě).
- **Nahlásit chybu:** Nápověda → Nahlásit chybu… řekne, co do hlášení napsat a kam ho poslat (do zveřejnění
  cestou, kterou jste program dostali, potom přes GitHub Issues). Ukáže verzi, běžící sestavu, složky dat a práce,
  soubory nastavení a logu a systém. **Kopírovat diagnostiku** dá tohle a posledních 200 řádků logu do schránky
  (jsou v tom cesty na vašem PC), **Otevřít složku logu** ukáže `sapiemu.log`. **Po pádu** program spusťte znovu
  a pošlete i `sapiemu.prev.log` z téže složky: je to log spuštění, které spadlo (další spuštění ho přepíše).
- **Software z disket J. Bíby:** překladače a editory jsou v `media/sapiman/knihovna/` (Turbo Pascal, MBASIC,
  FORTH, Pascal/MT+, C, dBase II, WordStar…). Okno Média, Vložit… do B: a pak např. `B:MBAS`. Soupis všech
  disket je v `docs/software/SAPIMAN_diskety.md`.
- **Sériový terminál:** Zobrazení → Sériový terminál. Poslat soubor (tak, jak je), textový soubor, soubor XMODEM
  (např. do `DOCPM10` v CP/M), přijmout soubor XMODEM (z `DOPC10`), Kopírovat (Alt+C nebo Ctrl+Shift+C, výběr myší nebo celý text; výběr jde do schránky
  i sám po puštění tlačítka myši) a Vložit (Alt+V nebo Ctrl+Shift+V).
  `docs/seriova-linka.md`.
- **Deska diagnostiky:** u JPR-1S / JPR-1X s `k3` = `diag` (sestava `jpr1x_bgp1s`) Zobrazení → Deska
  diagnostiky (K3): dvě sedmisegmentovky a tlačítka − a +. Ukázkový čítač: Soubor → Nahrát program
  `media/jpr-1x/diag_int.hex`, ve Falconu `JUMP 4400`, ESC ho ukončí.
- **Tiskárna:** Zobrazení → Tiskárna ukáže vytištěný text. Bajty, jak přišly, jsou v `work/printer/*.prn`.
  Tiskárnu zapnete v konfigurátoru u procesorové karty (X3) nebo u DPP-1 (`docs/tiskarna.md`).
- **Programy do paměti:** Soubor → Nahrát program do paměti (Intel HEX `.hex`, `.ihx` i `.txt`) nebo binární soubor. V CP/M pak
  `SAVE n JMENO.COM` (okno počet stránek napoví).
- **Zvuk, joystick, myš:** Stroj → Zvuk, joystick a myš; hlasitost a citlivost myši v Zobrazení → Nastavení. Zvuk
  hraje jen při rychlosti 100 %.
- **Nastavení** (Zobrazení → Nastavení…): vzhled (světlý nebo tmavý, velikost písma, okna plovoucí nebo
  zadokovaná), jazyk, zvuk, myš, průvodci sestavami, server MCP pro vývoj s Claude, cesta k ffmpeg.
- **Nápověda:** Průvodce sestavou, První kroky (s anglickým rozhraním anglické `docs/getting-started.md`), Návody
  k obsluze (otevřou se v programu pro soubory .md), Klávesové zkratky (všechny na jednom místě), Nahlásit chybu…
  a O programu.
- **Stav stroje:** Soubor → Uložit / Načíst stav (`.sapistate`). Stav uloží celý počítač včetně RAM disků.

## Vyzkoušejte

### Základ: historické sestavy Tesla

Každá sestava Základu má průvodce (Nápověda → Průvodce sestavou). Pro začátek:

- **ZPS-1 a MIKROBASIC** (`zps\ZPS-1.sapi`, běží po prvním spuštění): `10 FOR I=1 TO 5`, `20 PRINT I,I*I`,
  `30 NEXT I`, `RUN` vypíše čísla 1 až 5 a jejich druhé mocniny; `LIST` ukáže program, `NEW` ho smaže.
- **CP/M Tesla IMA** (`zps\ZPS-3_CPM64K_IMA.sapi`): `DIR B:` ukáže programy na druhé disketě, `B:` a `MBASIC`
  spustí MBASIC (`PRINT 6*7`, `SYSTEM` vrátí do CP/M).
- **MIKOS a grafika** (`zps\ZPS-2_MIKOS_V5.sapi`):
  1. Soubor → Nahrát program → `media\mikos\BASIC_EXP_V5.0G.hex`;
  2. `B`;
  3. `CLSG:FOR I=0 TO 319 STEP 8:MOVE I,0:PLOT 319-I,239:NEXT I`.

### Přístroje a technologie

- **Měřicí jednotka APS-5** (`aps5.sapi`, původní firmware Tesla):
  - vedle obrazovky se otevře okno APS-5 s čelním panelem a osmi pecemi; klávesy lze mačkat myší, nebo psát
    na PC: číslice, `M` (Esc), `R`, Backspace = ←, Enter = →;
  - příkaz se zadá číslem a →, rezervované příkazy číslem, `R` a → (zámek musí být v poloze 2);
  - první zapnutí je studený start. Nejdřív kalibrace `6R` → `9990` →, pak pro každou pec `0` → číslo pece →
    a `8R` → typ termočlánku → (pec 1: 3 = ch-a);
  - program: `2` → `1` →, `3R` → `20` → `700` → (20 minut na 700 °C) a další segmenty, `M` → zpět;
  - pec 1: `4R` → `6` → (vzorkovací perioda 60 s), `9R` → `1` → (program 1), `1` → `1` → (zapnout regulaci);
  - `11` → je tabulka stavu pecí. Pec v okně začne žhnout, graf ukazuje průběh. Tlačítko „Výpadek sítě“ zkusí,
    jak si jednotka poradí.
  - **RESET u APS-5 nepoužívejte.** Skutečná jednotka ho nemá a firmware ho bere jako chybu: „CHYBA 0 /
    INICIALIZACE“, po M → studený start (nastavení se smaže). Emulátor se proto před resetem zeptá. Vypínejte
    vypínačem POWER, to proběhne jako výpadek sítě a nastavení zůstane.

  **Nejrychleji:** tlačítko **DEMO** v okně APS-5 vše odťuká samo a pece nahřeje (asi minuta).
  **Ukázka krok za krokem** (tři pece v regulaci na 700 °C, dvě ručně, jako na obrázku v README):
  `docs/aps5-ukazka.md`. Zapojení a firmware: `docs/desky/APS-5.md`.
- **Měřicí pracoviště IMS-2** (`zps/ZPS-2_DPS-1_IMS-2.sapi`, deska DPS-1, BASIMS V4.0):
  1. nad ostatní okna se otevře Měřicí pracoviště: voltmetr M1T 330 měří výstup I zdroje BS 525;
  2. BASIMS z kazety: `.K_L`, jméno `BASIMS`, v okně Média PŘEHRÁT, `Y`, pak `.J` (nebo Soubor → Nahrát program
     do paměti → `media\dps-1\BASIMS.hex` a `G4400`); za ním jsou na kazetě programy `PRIKLAD` a `SMYCKA`
     (`LOAD`, jméno, `Y`, `RUN`);
  3. program `10 IMS# N`, `20 IMS# O3;"R4D0E"`, `30 IMS# T3`, `40 IMS# I A$`, `50 PRINT A$` vypíše `V+1.256…E+1`;
  4. knoflíky zdroje se točí myší, monitor sběrnice ukazuje adresování a texty.

  Celý návod: `docs/navod-dps1.md`.
- **Zapisovač Minigraf 0507** (`zps\ZPS-2_MIKOS_V5_Minigraf.sapi`, okno Minigraf se otevře samo):
  1. `.G=1800` (pero na počátek), `.G=1809` nakreslí znakovou sadu;
  2. ukázka: Soubor → Nahrát program → `media\mikos\BASIC_EXP_V5.0.hex`, `.B`, v okně Minigraf Nový papír, obsah
     `media\mikos\MINIGRAF_DEMO.bas` zkopírovat a vložit Stroj → Klávesnice → Vložit text (Ctrl+Shift+V), `RUN`;
  3. kresba se uloží tlačítkem Uložit PNG nebo SVG (`docs/desky/Minigraf.md`);
  4. kreslení v milimetrech: `media\mikos\BASIC_EXP_V5.0G_SZ.hex`, `.B`, vložit `media\mikos\KRES_DEMO.bas`, `RUN`
     (příkaz `KRES`: kružnice, šrafování, text).

  Celý návod s příklady vlastních kreseb: `docs/navod-minigraf.md`.
- **Grafická jednotka XY 4130** (`zps\ZPS-2_MIKOS_V5_XY4130.sapi`, okno zapisovače se otevře samo):
  1. po startu se ovladač ohlásí `L=XY4130` a vozík dojede na levý doraz (rozsvítí se READY);
  2. v okně zapisovače stisknout **PEN** (po zapnutí nesvítí, bez něj se pero nespustí);
  3. `.G=181E` nakreslí znakovou sadu: ovladač napíše `ZALOZ PAPIR A STISKNI KLAVESU`, stačí mezerník;
  4. výpisy na tiskárnu L: (`LLIST` v BASICu EXP) se kreslí písmem; ukázka kresby `media\mikos\XY4130_DEMO.hex`
     (z BASICu `A=CALL(32768)`).

  Celý návod: `docs/navod-xy4130.md`.
- **Plotter z AR 1990** (`plotter_ar1990.sapi`, deska JPR-1 jako řadič Minigrafu, povely Sharp MZ-1P16):
  1. po zapnutí plotter pípá: tlačítko READY v okně Minigraf, vozík najede na počátek;
  2. TEST napíše a podtrhne „*** MZ-821 ***“, Celý test nakreslí znakovou sadu;
  3. Ukázka pošle `media/ar1990/ukazka.txt` (rámeček, osy, sinusovky, hvězda, texty, textový režim);
  4. vlastní povely do řádku: `\x02` grafický režim, `I` počátek, `D400,0` čára, `P` text, `\x01` zpět na text.

  Celý návod: `docs/navod-ar1990.md`.
- **Záznamník dat KZD-1** (`zps\ZPS-2_MIKOS_K.sapi`, kazeta v okně Média):
  1. `.K` `I`, název pásky (nová kazeta se musí připravit, chvíli to trvá);
  2. `.K` `S`, jméno, `=5000 53FF` a Enter uloží paměť;
  3. `.K` `D` vypíše adresář;
  4. `.K` `Z` převine na začátek, `.K` `L` a jméno soubor načte (návod v `docs/desky/KZD-1.md`);
  5. programy z kazet magnetofonu na KZD-1 a zpět zkopíruje program CKM (`.G=4400`), nejlépe opravená kopie
     `media\mikos\CKM_opraveny.hex` (originál `CKM.hex` po čtení z KZD-1 nastaví nulovou délku bloku);
  6. program `.hex` nebo `.bin` jde do KZD-1 vložit rovnou (Média → Vložit…), vznikne z něj kazeta jako od MIKOSu/K.

  Celý návod s výpisy obrazovky: `docs/navod-kzd1.md`.
- **Kazetový disk RKD-1** (`rkd1_tesla_cpm.sapi`, CP/M 2.2 Tesla Elstroj 1987 z kazety CM 5403):
  1. po startu číslo uživatele (`1`) a název oblasti; výzva `b1$` je disk B:, uživatel 1;
  2. `DIR A:` ukáže systémovou kazetu (shell, formátovač, zdrojáky systému);
  3. pevný disk je nový: `FORMTRKD`, klávesy `2` a `3` (C: a D:), Enter, `a`; F8 formátování zrychlí;
  4. `PIP C:=E:STAT.COM` zkopíruje program z diskety, `E:STAT` ukáže volné místo;
  5. kazetu měnit jen s Resetem (F12) a jen za systémovou kazetu; novou udělá tlačítko Systém… v okně Média
     (pomoc emulátoru, originální SYSGEN chybí) a soubory shellu se na ni zkopírují přes pevný disk C:;
  6. bulharská mechanika CM 5400: `rkd1_jb_cm5400.sapi`, formátovač `B:FORMRKD` J. Bíby, pak D: až G: v CP/M
     (BIOS vznikl v emulátoru, dobový se nenašel).

  Celý návod s výpisy obrazovky: `docs/navod-rkd1.md`.
- **Magnetická páska RMP-1** (`rmp1_jb_izot.sapi`, mechanika IZOT SM 5300, programy J. Bíby):
  1. Zobrazení → Média: mechanika s cívkami a kontrolkami, vpravo mapa pásky;
  2. `B:DIRMT 0` vypíše ukázkovou pásku SAPI01 (dva soubory);
  3. `B:MTDAT`, funkce 1 zkopíruje soubor z pásky na disketu (páska `SAPI01`, soubor `2`, blok `800`, záznam
     `80`, jméno `MTDATMAN`, cíl `A:MTDAT.MAN`), funkce 2 opačně;
  4. nová páska: OFF-LINE, Nová páska…, LOAD, ON-LINE, pak MTDAT funkce 3;
  5. kopie z pásky na pásku (jednotka 0 → 1): do B: vložit `media\rmp-1\a_lad_izo_opraveny.img` a spustit
     opravenou kopii `B:MTCOPYO` (původní MTCOPY zapíše do EOF1 o blok méně), postup krok za krokem v návodu.

  Celý návod s výpisy obrazovky: `docs/navod-rmp1.md`.

### Desky Libora Lasoty (moderní)

Programy pro desky V z `media\` si nahrajete takto:

- **Grafika CGA-1V** (sestava V, CP/M):
  1. Soubor → Nahrát program do paměti → `media\cga-1v\rembran1.hex`;
  2. v CP/M `SAVE 64 REMB1.COM`, pak `REMB1`.

  Obrázek se ukáže v okně CGA-1V. `castle.hex` je hrad, `rembran2.hex` (`SAVE 127`) střídá stránky.
- **Hudba MPH-1V** (sestava V):
  1. `media\mph-1v\splay.hex` + `SAVE 4 SPLAY.COM`;
  2. RAT PLAYER `media\mph-1v\rat_0_99_93.txt` + `SAVE 18 RAT.COM`.

  Skladby nejsou v `media\mph-1v` (jsou na RAM disku sestavy `sapi1v_hry`, viz Hry a dema), další RAD jsou na
  https://www.3eality.com/Public/Modules/RAD-v1/. Soubor RAD nahrajte
  binárně od 0100h a uložte příkazem, který okno nabídne.
- **Test karty MPH-1V:** `media\mph-1v\mph_test_0v99.hex`, `SAVE 31 MPH.COM`.
- **Rychlost RAM disku a CGA-1V** (sestava V): `media\zrd-1v\zrdbench.hex`, `SAVE 7 ZRDBENCH.COM`, `ZRDBENCH`.
  Ukáže takt CPU a takty na bajt, porovnání se skutečným SAPI-1 V je v `docs\desky\ZRMD-1.md`. Zkuste i
  s vypnutým TURBO (tlačítko na panelu ZDR). Rychlost videopaměti AND-1V měří stejně `media\and-1v\vrambench.hex`
  (`SAVE 6 VRAMBENCH.COM`, `VRAMBENCH`).

### Hry a dema (novodobé)

SAPI-1 nebyl herní počítač, hry a dema jsou novodobý doplněk na nových deskách.

**SAPI-1 V: hry a dema** (`sapi1v_hry.sapi`, start volbou `3`, pak jméno programu na RAM disku A:):
- `BADAPPLE` (demo Bad Apple!!; obraz místy rychle bliká, kdo je citlivý na blikání, ať ho nespouští), `FXS4`
  (Fuxoft Soundtrack IV, skladby klávesami A–Z);
- `FLAPPY` (Flappy na CGA-1V; šipky a mezerník, nebo joystick) a `FLAPDGD` (Flappy na DGD-1V, port L. Lasoty):
  ovládá se joystickem, Ctrl+F8 joystick z klávesnice, mezerník = palba (úvod, výběr scény, hra);
- `BOMBER` (Bomberman), `BKP` (textová hra Bratrstvo kočičí pracky, verze pro SAPI od L. Lasoty);
- dema L. Lasoty: `Y1STDEMO`, `SNDDEMON` (jen zvuk), obrázky `CASTLE`, `REMBRAN1`, `REMBRAN2`, testy `CGATEST`,
  `MPHTEST`;
- hudba: `SPLAY HELLFIRE.VGM` (dalších 10 skladeb VGM, `DIR *.VGM`) a `RAT SMJOOP.RAD` (15 modulů RAD, `DIR *.RAD`).
- Většina her a přehrávačů se vrací do CP/M klávesou ESC (Bad Apple!! kteroukoli klávesou). Program, který se
  sám neukončí, zastaví reset F12: ECMON si volbu 3 pamatuje a CP/M spustí znovu. Ovládání každé hry je v průvodci
  sestavou.

Další hry: Prince of Persia (`sapi1v_PoP.sapi`), síťový BomberNet (`sapi1v_BomberNet.sapi`) a Manic Miner
(`jpr1s_bgp1s.sapi`). Jak začít, je v tabulce sestav výše.

## Pro vývojáře

`sapiemu-cli.exe` běží bez okna (skripty, testy). Přehled voleb vypíše `sapiemu-cli --help`. Emulátor má
i MCP server pro práci s Claude, viz `docs\mcp.md`. Podrobnosti o deskách jsou v `docs\desky\`.

Pomůcky při vývoji programů:
- **Náhodný obsah pamětí po zapnutí** (konfigurátor, zaškrtávátko u názvu sestavy): RAM, videopaměti a palety
  nejsou po zapnutí vynulované, stejně jako na železe. Program, který spoléhá na nuly, se tak ukáže hned.
- **CGA-1V se kreslí s paprskem:** přepnutí stránky nebo zápis do palety v nevhodnou chvíli je vidět jako trhání,
  stejně jako na monitoru. MCP `machine_info` ukáže počet zápisů do palety během zobrazování a přepnutí
  uprostřed snímku s číslem řádku.
- **Přerušení:** MCP `int_log` zaznamená každé přijaté přerušení (karta, IM, vektor, obsluha). Vektor IM 2
  přečtený z videopaměti hlásí log vždy.
- MCP `quit` emulátor korektně ukončí (média se uloží).
