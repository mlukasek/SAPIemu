# SAPIemu – první kroky

Emulátor počítače Tesla SAPI-1. Verze **alfa**: hodně věcí funguje, ale počítejte s chybami. Budeme rádi za každou
zprávu, co nefunguje (GitHub Issues: https://github.com/mlukasek/SAPIemu/issues).

## Spuštění

1. Rozbalte zip kamkoli (třeba `C:\SAPIemu`). Exe najde ROM, média a sestavy ve složkách vedle sebe.
2. Spusťte **`sapiemu.exe`**. Windows u nepodepsaného programu ukážou varování SmartScreen: „Další informace“
   → „Přesto spustit“.
3. Nic se neinstaluje. Nastavení se ukládá vedle exe (`sapiemu-settings.json`, `imgui.ini`). Pracovní kopie
   disků a RAM disků vznikají ve složce `work\`. Originály v `media\` zůstávají netknuté.

Potřebujete Windows 10 nebo 11, 64 bit.

## Sestavy

Soubor → **Otevřít sestavu** nabídne připravené sestavy (`machines\`). Po prvním spuštění běží MikroMon.

| Sestava | Co v ní je | Jak začít |
|---|---|---|
| `mikromon.sapi` | MikroMon 1.3 (Libor Lasota) | `?` + Enter vypíše příkazy |
| `cpm_zrmd.sapi` | CP/M 2.2 z RAM disku ZRMD-1 (ECMON), s tiskárnou | po startu stiskněte `1`, pak `DIR`; `^P` a `DIR` vytiskne výpis (okno Tiskárna) |
| `cpm_ide.sapi` | totéž s IDE diskem a PMD32-SD | po startu `3` |
| `cpm_zrmd_fdc1.sapi`, `cpm_zrmd_mfc1a.sapi` | totéž s řadičem disket FDC-1 nebo MFC-1A a 8" disketami | po startu `0` (FDC-1) nebo `2` (MFC-1A), pak `DIR B:` |
| `cpm_ec1045.sapi` | systém EC1045: FDC-1 s disketami PC 3,5" a 5,25" HD | `1` = 3,5", `2` = 5,25" (vložit disketu v okně Média), pak `DIR B:` |
| `cpm_boot_mfc1a.sapi`, `cpm_boot_fdc1.sapi`, `cpm_boot_rpd1.sapi` | CP/M J. Bíby (2011) bootovaný z 8" diskety | naběhne CP/M, na A: jsou zdrojáky systému |
| `cpm_boot_*_ss256.sapi`, `cpm_boot_mfc1a_rmd.sapi` | totéž ze systémových disket J. Bíby (A: programy, B: zdroje), C: ZRMD-1 nebo RMD-1 | naběhne CP/M (výzva `U0A>`) |
| `cpm_boot_fdc1_rmd.sapi`, `cpm_boot_rpd1_rmd.sapi` | CP/M J. Bíby s RAM diskem RMD-1 (FDC-1, RPD-1) | `B:IRMD40`, odpověď `A`, pak C: |
| `cpm_boot_fdc1_irmd.sapi` | CP/M J. Bíby, RAM disk RMD-1 si připraví BIOS sám (zjistí i velikost) | `SAVE 2 C:X.COM`, `B:STAT C:` |
| `cpm_boot_rpd1_mfc256.sapi`, `_mfc512`, `cpm_boot_rpd1_fdc1_rmd.sapi` | CP/M J. Bíby z RPD-1 s RMD-1 a druhým řadičem pro D:, E: (MFC-1A s 8" FM 14 × 256 nebo 8 × 512, FDC-1 8") | `B:IRMD40`, `A`, pak `SAVE 2 D:X.COM`, `DIR D:` |
| `cpm_pmd_fdc1.sapi` | CP/M J. Bíby: FDC-1 a PMD32-SD přes DPP-1 | `DIR C:` (Turbo Pascal), `DIR D:` (WordStar) |
| `cpm_term64k_fdc1.sapi` | CP/M 64K bez obrazovky, jen terminál | Zobrazení → Terminál (nebo TeraTerm na 127.0.0.1:2323) |
| `cpm_pmd_rpd1.sapi` | CP/M J. Bíby s PMD32-SD přes DPP-1 (C:, D: = 5,25" diskety J. Bíby na SD kartě) | `DIR C:` (Turbo Pascal), `DIR D:` (WordStar) |
| `cpm_zrmd_dpp1.sapi` | `cpm_zrmd` s deskou DPP-1 a PMD32-SD | po startu `1`, pak `DIR D:`, `DIR E:` |
| `cpm_boot_dlouhe_rpd1_fdc1.sapi` (`_35`: 3,5") | „dlouhé“ CP/M J. Bíby: A:, B: 8" (RPD-1), C: ZRMD-1, D:, E: 5,25" HD (FDC-1) | naběhne CP/M, zkuste `DIR B:`, `STAT D:` |
| `sapi1v.sapi` | Liborovy desky V: CP/M, barevná grafika CGA-1V, zvuk MPH-1V | `1` = CP/M z RAM disku A:, `3` = i s IDE diskem B:, C: |
| `zps/ZPS-1.sapi` | ZPS-1 s MIKROBASICem 2.4 a membránovou klávesnicí ANK-1 | rovnou píšete v BASICu |
| `zps/ZPS-2*.sapi` | ZPS-2 s MIKOSem V4/V5, grafika DGD-1, MIKOS/D z diskety | monitor MIKOS; BASIC EXP je v `media\mikos` (viz níže) |
| `zps/ZPS-3*.sapi`, `zps/ZPS-4*.sapi` | CP/M 2.2/P z 8" diskety (RPD-1, u `_FDC-1` řadič FDC-1), 48K a 62K, ELIS | naběhne CP/M |
| `zps/ZPS-3_CPM48K_IMA.sapi` | originální CP/M 2.2 46K Tesla IMA s MON V1.0-C1 (systém z MOVCPM48) | `DIR`, `DIR B:` |
| `zps/ZPS-3_CPM64K_IMA.sapi` | originální CP/M 2.2 64K Tesla IMA (B: MBASIC, COBOL) | `B:`, `MBASIC` |

## Ovládání

- **Psaní:** klikněte do okna obrazovky. Text jde na klávesnici SAPI (české znaky podle klávesnice sestavy).
- **Klávesy emulátoru:**
  - F5 pauza;
  - F8 rychlost (100 / 200 / 400 % / maximum);
  - F9 nebo F12 reset;
  - F10 / F11 krok v debuggeru;
  - **Ctrl+F2 / Ctrl+F3** rychlé uložení a načtení stavu;
  - Ctrl+F8 joystick z klávesnice (šipky, mezerník);
  - Ctrl+F10 pustí zachycenou myš.
- **Sestava a karty:** Stroj → Konfigurace. Panely karet jako na skutečném počítači, propojky karet, mapa portů
  a paměti. Kartu změníte kliknutím na slot. Některé propojky platí hned („platí hned“).
- **Zobrazení:** barva obrazu, zvětšení (pravé tlačítko na obraz), celočíselné škálování, poměr stran TV,
  **Jazyk / Language** (čeština, angličtina; poprvé podle Windows).
- **Média:** mechaniky jsou očíslované podle řadiče (0–3) a za číslem je jejich písmeno v CP/M (ze sestavy,
  nebo „asi B:“ zjištěné za chodu). Zobrazení → Média: kazety (WAV, MP3) pro DSM-1, diskety RPD-1, FDC-1 a MFC-1A (obrazy `.img` a
  ImageDisk `.imd`; nová disketa v libovolném formátu: 8", PC 3,5"/5,25" HD nebo DD; s příponou `.imd` ji
  pak program pro formátování naformátuje i jinak, třeba s jiným číslováním sektorů), IDE disk a PMD32-SD.
- **Software z disket J. Bíby:** překladače a editory jsou v `media/sapiman/knihovna/` (Turbo Pascal, MBASIC,
  FORTH, Pascal/MT+, C, dBase II, WordStar…). Okno Média, Vložit… do B: a pak např. `B:MBAS`. Soupis všech
  disket je v `docs/software/SAPIMAN_diskety.md`.
- **Tiskárna:** Zobrazení → Tiskárna ukáže vytištěný text. Bajty, jak přišly, jsou v `work/printer/*.prn`.
  Tiskárnu zapnete v konfigurátoru u procesorové karty (X3) nebo u DPP-1 (`docs/tiskarna.md`).
- **Programy do paměti:** Soubor → Nahrát program do paměti (Intel HEX) nebo binární soubor. V CP/M pak
  `SAVE n JMENO.COM` (okno počet stránek napoví).
- **Zvuk, joystick, myš:** Stroj → Zvuk, joystick a myš. Zvuk hraje jen při rychlosti 100 %.
- **Stav stroje:** Soubor → Uložit / Načíst stav (`.sapistate`). Stav uloží celý počítač včetně RAM disků.

## Vyzkoušejte

**Disk C:** sestavy V (start volbou `3`, pak `C:`) má už programy připravené:
- SPLAY s HELLFIRE.VGM (`SPLAY HELLFIRE.VGM`);
- RAT PLAYER se skladbami RAD (`RAT SMJOOP.RAD`, VOIDRAST, VOIDTERR, ADLIB2, BLUEAD);
- MPH-TEST, Y1STDEMO, SNDDEMON, ZRDTEST;
- hra Flappy `FLAPDGD` pro sestavu `sapi1v_DGD_Flapy.sapi` (grafika DGD-1V).

Další programy z `media\` si nahrajete takto:

- **Grafika CGA-1V** (sestava V, CP/M):
  1. Soubor → Nahrát program do paměti → `media\cga-1v\rembran1.hex`;
  2. v CP/M `SAVE 64 REMB1.COM`, pak `REMB1`.

  Obrázek se ukáže v okně CGA-1V. `castle.hex` je hrad, `rembran2.hex` (`SAVE 127`) střídá stránky.
- **Hudba MPH-1V** (sestava V):
  1. `media\mph-1v\splay.hex` + `SAVE 4 SPLAY.COM`;
  2. RAT PLAYER `media\mph-1v\rat_0_99_93.txt` + `SAVE 18 RAT.COM`.

  Skladby nejsou součástí balíku (RAD z https://www.3eality.com/Public/Modules/RAD-v1/). Soubor RAD nahrajte
  binárně od 0100h a uložte příkazem, který okno nabídne.
- **Test karty MPH-1V:** `media\mph-1v\mph_test_0v99.hex`, `SAVE 31 MPH.COM`.
- **MIKOS a grafika** (`zps\ZPS-2_MIKOS_V5.sapi`):
  1. Soubor → Nahrát program → `media\mikos\BASIC_EXP_V5.0G.hex`;
  2. `B`;
  3. `CLSG:FOR I=0 TO 319 STEP 8:MOVE I,0:PLOT 319-I,239:NEXT I`.

## Pro vývojáře

`sapiemu-cli.exe` běží bez okna (skripty, testy). Přehled voleb vypíše `sapiemu-cli --help`. Emulátor má
i MCP server pro práci s Claude, viz `docs\mcp.md`. Podrobnosti o deskách jsou v `docs\desky\`.
