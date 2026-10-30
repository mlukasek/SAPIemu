# CP/M a monitory pro ZPS-3 (JPR-1A + RPD-1 nebo FDC-1)

**Stav (2026-10-06): běží i originální CP/M 2.2 od Tesly IMA, 64K a 48K** (sestavy `ZPS-3_CPM64K_IMA.sapi`
a `ZPS-3_CPM48K_IMA.sapi`, níže). Bootovatelná disketa 48K se nedochovala, její systémové stopy jsou vyrobené
originálním `MOVCPM48` a `SYSGEN`.

**Stav (2026-10-05): CP/M 2.2/P 48K a 62K běží s RPD-1 i s FDC-1.** Předkonfigurované sestavy podle podkladů
autora (`..\SAPI-sestavy\ZPS-3\`: ROM `MOR*.hex`, diskety `CPM*.img`; 48/62 = paměť, F = řadič FDC-1):

| Sestava | ROM (JPR-1A) | Disketa | Paměť, displej |
|---|---|---|---|
| `ZPS-3_CPM48K.sapi` | MOR48A | `media/rpd-1/CPM22P_48A_Perina.img` | RAM-1A 0000–BFFF, AND-1A E800 |
| `ZPS-3_CPM62K.sapi` | MOR62A | `media/rpd-1/CPM22P_62A_Perina.img` | dvě RAM-1A 0000–F7FF, AND-1A F800 |
| `ZPS-3_CPM48K_FDC-1.sapi` | MOR48AF | `media/fdc-1/CPM22P_48AF_Perina.img` | jako 48K, řadič FDC-1 |
| `ZPS-3_CPM62K_FDC-1.sapi` | MOR62AF | `media/fdc-1/CPM22P_62AF_Perina.img` | jako 62K, řadič FDC-1 |
| `ZPS-3_CPM48K_IMA.sapi` | MON V1.0-C1 (IMA) | `media/rpd-1/CPM48K_IMA_004.img`, B: `_003` | RAM-1A 0000–BFFF, AND-1A E800 |
| `ZPS-3_CPM64K_IMA.sapi` | MON64C3 (IMA) | `media/rpd-1/CPM64K_IMA_01.img`, B: `_02` | dvě RAM-1A 0000–EFFF + F800–FFFF, AND-1A F000 |

- MOR se po RESET zkopíruje z EPROM na B800 (48K) nebo F000 (62K) a zavede CP/M z mechaniky A.
- `CPM48A.img` od autora je shodný s dřívějším `CPM22P_48A_Perina.img`.
- **62K:** paměť do F7FF dávají dvě RAM-1A. První je standardní 48K na 0000–BFFF. Druhá je RAM-1/16K s PROM D1A
  upravenou tak, že S1 S2 = 10 dá C000–F7FF místo C000–FFFF (`RAM-1A_D1A_62K_74S287.bin`). To je **předpoklad**,
  jak to vypadalo na skutečné sestavě, nemáme ho doložený. AND-1A na F800 má domácí PROM D3A
  (`AND-1A_D3A_CPM62K_74S287.bin` podle `SAPIMAN\...\FDC_AND1A130711_RMD\dekoder AND1ACPM.txt`).
- **FDC-1** (`docs/desky/FDC-1.md`): tiskárna DPP-1 je na 18H–1BH místo 08H–0BH (neemulovaná). Na disketě FDC
  je `FDC.DOC`, `MOR24F.MAC` (zdroj MORxxF) a formátovač `FORM6.COM` (ověřeno: formátuje 8" SS SD na B:).
- Test `tests/test_zps3.cpp` projde všechny čtyři sestavy: boot, DIR, SAVE (data na disketě), ERA a nový boot
  po RESETu.
- **64K Tesla IMA** (2026-10-06): EPROM „JPR-1A 64K V C3“ (`roms/jpr-1a/MON64C3.bin` =
  `SAPI_DOKU\SAPI_EPROM\BOOT_pro_CPM\JPR1A_BOOT_64k.BIN`, zdroj `MON64C3.MAC`) se po RESET zkopíruje do RAM
  na F800 a `OUT 00` vypne EPROM. Pak zavede „JPR-1A 60K CPM V2.2, BIOS V2.2-C2“ (CCP V2.2-A1 TESLA ELTOS IMA
  PARDUBICE 15/11/1984). Video je na F000 (`videom equ 0F000h`), RAM tedy 0000–EFFF a F800–FFFF:
  - druhá RAM-1A (16K, S1 S2 = 10) s PROM D1A `RAM-1A_D1A_64K_74S287.bin`: C000–EFFF a F800–FFFF;
  - AND-1A s PROM D3A `AND-1A_D3A_CPM64K_74S287.bin`: položka EFh → F000 místo DFh → E800 (S1 sepnutá dál 3800);
  - **obě PROM jsou předpoklad** odvozený od standardních, originální 64K sestavu ani její PROM nemáme;
  - diskety `media/rpd-1/CPM64K_IMA_01–03.img` = `SAPI_DOKU_FTP\ZALOHY_8SD_EC1045.01\nove_a_vse_co_mám\DEX\
    SAPI_ZPS3\01–03.8SD`: 1 systém, ED, PIP, M80, L80, F80, ASM, SYSGEN, MOVCPM60; 2 STAT, FORMAT, MBASIC
    V5.21-A1, BASCOM, COBOL; 3 Pascal MT+. Soubory mají atribut R/O, na 1 zbývá 5K, na 2 2K. Kopie
    `!!!!_TRIDENO_!!!!\CPM64K\CPM64K_001–004.8SD` mají stejné systémové stopy a jiné soubory (004 se zdrojáky
    MON64C3, BIOS64C2, BOOT0C2, BOOT1C2);
  - ověřeno: DIR, SAVE, ERA, STAT, MBASIC (31227 BYTES FREE), nový boot po RESETu (test `ima64k` v `test_zps3`).
- **48K Tesla IMA, rekonstrukce** (2026-10-06): s ROM MON V1.0-C1 (monitor na B800) naběhne „JPR-1A 46K CPM
  V2.2, BIOS V2.2-C1“ (CCP V2.2-A1 TESLA ELTOS IMA PARDUBICE 15/11/1984). Disketa `media/rpd-1/CPM48K_IMA_004.img`:
  - **soubory jsou originální** (disketa „!CPMZPS3.004“ = `SAPI_DOKU_FTP\ZALOHY_8SD_EC1045.01\nove_a_vse_co_mám\
    !!!!_TRIDENO_!!!!\ZPS3\083B.8SD`: MOVCPM48, SYSGEN, M80, F80, ED, ASM, PIP, STAT, zdroje MON48C1, BIOS48C1,
    BOOT0C1, BOOT1C1), systémové stopy tam byly vyplněné 40h;
  - **systémové stopy vyrobil originální `MOVCPM48 46 *` (V1.4-C1, TE-IMA 2/86) a `SYSGEN`** spuštěné v emulátoru
    pod Peřinovým CP/M 48K s disketou v B:. MOVCPM48 bere 20 až 46, víc se pod monitor na B800 nevejde, 46K je
    tedy jediná plná velikost „48K“ systému;
  - **ověřeno proti zdrojům** (M80/L80 v `cpmrun`): BOOT0C1 se shoduje na bajt, kód BIOS48C1 také, liší se jen
    oblast DS (v originále nuly) a číslice „46“, které doplní MOVCPM. Obraz je přesná relokace `SYS48.BIN`
    (stopy 0–1 přeložené s bias 0, ve složce `083B`): všech 1145 rozdílů je +9Eh, kromě těch dvou číslic;
  - **diskety `083ASYS`, `038BSYS` (i s `_`)** v téže složce mají v systémových stopách IMA CP/M, ale
    nepřemístěný (bias 0, CCP na 0000, `JMP 1600h`), proto nenaběhnou. Je to pracovní výstup pro relokační
    bitmapu MOVCPM (zdroje se překládají dvakrát, BOOT0C1 s bias 0 a BOOT1C1 s bias 100H). Verze bez `_` se od
    `SYS48.BIN` liší jen číslicemi „48“ v hlášce BIOSu;
  - B: `media/rpd-1/CPM48K_IMA_003.img` = `CPMZPS3_003.IMG` (Pascal MT+, bez systému, shodná s `083A.8SD`);
  - test `ima48k` v `test_zps3`: boot, SAVE, ERA a znovu `MOVCPM48 46 *` + `SYSGEN` na B:, systémové stopy
    vyjdou stejné jako na A:.

**Stav (2026-10-03): ZPS-3 běží.** Sestava `machines/zps/ZPS-3.sapi`:
- **MON V1.0-C1 zavede z diskety Peřinův CP/M 2.2/P** (`media/rpd-1/CPM22P_48A_Perina.img`): banner
  `JPR-1A 48K MON V1.0-C1`, `CPM V2.2 V/P1`, `A>`.
- DIR, TYPE, SAVE a ERA fungují. Totéž s ROM MOR48A (`SAPI-1 ZPS3-48K`).
- S připojeným terminálem na DSM-1 (DCD) jde konzole na sériovou linku.
- AND-1A zobrazuje jen velká písmena a české znaky KOI-8 ukazuje jako jiné znaky MHB 2501 (např. W místo
  É). Tak to vypadá i na skutečném stroji.
- Test `tests/test_zps3.cpp`.

Desky: `docs/desky/JPR-1A.md`, `AND-1A.md`, `RAM-1.md`.

## Sestava ZPS-3 (manuál `BKP\pdf\zps3.pdf`, příloha 2 a 3)

- **Sloty:** X1 JPR-1A, X2 RAM-1A, X3 AND-1A, X4 DSM-1, X5 RPD-1A, X6 RPD-1B (kabel KB-17), X7/X8 volné.
  Dvě mechaniky JPD-1 8", klávesnice Consul 259.11, tiskárna Consul 2111.
- **Porty:** 00 JPR-1A (zápis = přepnutí ROM/RAM), 01–03 JPR-1A P0–P2, 10–13 DSM-1, 20–25 RPD-1A, 3C–3F RPD-1B.
  MOR* používá ještě 08–0B (tiskárna na DPP).
- **Paměť:**
  - po RESET: EPROM JPR-1A 0000–0FFF (jen čtení), RAM-1A 1000–BFFF, AND-1A E800–EFFF;
  - po `OUT 00h`: RAM-1A 0000–BFFF, AND-1A E800–EFFF;
  - s terminálem na DSM-1 smí být E800–EFFF RAM, ale pak nesmí být osazená AND-1A.

## Monitory (EPROM JPR-1A)

Tabulka je v `docs/desky/JPR-1A.md`. Standard je **MON V1.0-C1** (`B_V10_JPR-1A_RPD-1_ZPS-3.BIN`, zdroj
`MON48C1.MAC`).

## Obrazy disket (vše 256 256 B = 77 × 26 × 128)

- **cpm48a, cpm62a** (`BKP\data2\cpm48a_img.rar`, `cpm62a_img.rar`): Peřina CP/M 2.2/P pro RPD-1, bootovatelné,
  k ROM MOR48A/MOR62A. Na disketě jsou BIOS.MAC, BOOT.MAC, MOR24.MAC a CPM.DOC.
  - 48 = 48K, AND-1A na E800, monitor na B800.
  - 62 = 62K, AND-1A na F800, monitor na F000. Potřebuje dvě RAM-1A a nestandardní D3A.
- **Písmena:** `a` = AND-1A, `z` = AND-1Z, `f` = řadič FDC-1 místo RPD-1 (cpm48af, cpm62af). V repu jsou
  `media/rpd-1/CPM22P_48A_Perina.img`, `CPM22P_62A_Perina.img` a `media/fdc-1/CPM22P_48AF_Perina.img`,
  `CPM22P_62AF_Perina.img`.
- **cpmzps3_003/004.IMG** (`cpmzps3_img.rar`): navzájem shodné, systémové stopy vyplněné 40h, **nebootovatelné**.
  Soubory čtvrté diskety jsou v `cpmzps3_soubory.rar` (BIOS48C1.MAC, BOOT0C1/1C1.MAC, MON48C1.MAC,
  MOVCPM48.COM).
- **Originální Tesla IMA CP/M 64K, bootovatelný** („CCP V2.2-A1 TESLA ELTOS IMA PARDUBICE 15/11/1984“):
  `DEX\SAPI_ZPS3\01–03.8SD` v SD dumpu EC1045 Petra Všijanského (stejné jsou
  v `..\SAPI_DOKU_FTP\ZALOHY_8SD_EC1045.01\nove_a_vse_co_mám\DEX\SAPI_ZPS3`). Volá F821, patří
  k `JPR1A_BOOT_64k`. V repu jako `media/rpd-1/CPM64K_IMA_01–03.img`, sestava `ZPS-3_CPM64K_IMA.sapi`.
- **Diskety ze Slušovic** ve stejné složce `DEX\SAPI_ZPS3` (`04A`, `04B`: TNS CP/M V2.27-K 1986; `05`: MIC CP/M
  2.30 1987, OSBOOT-F) **nejsou pro SAPI-1**, ale pro počítače TNS z JZD AK Slušovice (Z80 s IM 2, displej FC00,
  klávesnice B8h, řadič disket na 60h–78h). Rozbor 2026-10-06: `docs/software/TNS_Slusovice.md`.
- **48K zdrojové diskety** bez systémových stop: `...\IMAGE_8\ZDROJ_STARE\15_a/b_zps3_48k.8sd`.
- `BKP\data\bios_lst.php` je BIOS MIKOS/D, ne CP/M ZPS-3. Zdroje CP/M BIOS: BIOS48C1.MAC, BIOS64C2.MAC,
  BIOS.MAC.

## Chybí

- **Bootovatelná originální systémová disketa IMA 48K ZPS-3** k MON V1.0-C1 se nedochovala. ~~Rekonstrukce~~
  hotová 2026-10-06 (originální MOVCPM48 a SYSGEN, výše), originál by se hodil jen pro porovnání.
