# ECMON a ZZRMD (EPROM JPR-1ZI)

## ECMON – „JPR-1Z 62K MON CZ 05082015JB“

Soubory `roms/jpr-1zi/JPR-1ZI_ECMON_2k_Martin.bin` a `JPR-1ZI_ECMON_2k_IDE-1_Martin.bin`. Druhá varianta
podporuje PMD32-SD přes IDE-1 (porty 5C–5Fh), první přes DPP-1 (28–2Fh).

- **Start:** zkopíruje se na F000 a vypíše hlavičku a „Uzivatel: Martin“. Pak nabídne volbu systému,
  jednu ze 4 kopií systému na RAM disku:

  ```
  0 - A: ZRMD-1, B,C: FDC8, D,E: PPMD32SD
  1 - A: ZRMD-1, B,C: RPD8, D,E: PPMD32SD
  2 - A: ZRMD-1, B,C: MFC8, D,E: PPMD32SD
  3 - A: ZRMD-1, B,C: HDD , D,E: PPMD32SD
  Zadej 0-3:
  ```
- **Řídicí kódy konzole** (ovladač obrazovky ECMON, stejné posílá i na sériovou linku):

  | Kód | Funkce | Kód | Funkce |
  |---|---|---|---|
  | 07 | zvonek (P0-OUT0 pulz) | 1B r c | kurzor na řádek r−20h, sloupec c−20h |
  | 08 | kurzor vlevo | 1D | kurzor domů |
  | 09 | tabulátor (po 8) | 1E | smazat do konce řádku |
  | 0A | nový řádek (rolování) | 1F | smazat do konce obrazovky |
  | 0B | kurzor dolů | 02 | vložit řádek |
  | 0C | smazat obrazovku | 03 | smazat řádek |
  | 0D | návrat na začátek řádku | 04, 05 | nastavení / zrušení příznaku (F78Dh), význam nezjištěn |
  | 18 | kurzor vpravo | 1A | kurzor nahoru |

  Konec řádku posílá ECMON jako `0D 0D 0D 0A`. Opakované CR jsou nejspíš prodleva pro pomalé terminály.
- **Konzole:** podle DCD na DMR-1/DSM-1 (port 10h, bit 5). Při aktivním DCD (terminál připojen) běží
  na sériové lince, jinak na AND-1Z a klávesnici. V emulátoru to řídí volba „Terminál připojen (DCD)“
  (`connected` v konfiguraci DMR-1, okno terminálu, MCP `serial_set_connected`).
- **Boot:** po volbě načte systém z RAM disku ZRMD-1 a spustí CP/M 2.2 („JPR-1Z 62K CP/M V2.2Z,
  BIOS-3K V050815JB“).

## Volba systému (autor, 2026-10-05)

- **Menu s volbou se zobrazí jen po prvním zapnutí.** Volba se uloží do RAM a každý další RESET rovnou spustí
  zvolenou konfiguraci. Aby se menu ukázalo znovu, musí se počítač vypnout tak, aby se smazal obsah RAM. Menu
  i některé ovladače jsou v ROM.
- **Jak to ECMON dělá** (ROM, kód od F668h, rozebráno 2026-10-05): po hlavičce otestuje podpis **AA 55 00 na
  F792h–F794h**.
  - Podpis chybí (RAM po zapnutí bez zálohy): vypíše menu a čeká na klávesu 0–3, volbu uloží na **003Bh**.
  - Podpis je: asi 1,3–3 s (BC = 5000h nebo B000h průchodů) testuje, jestli je stisknutá klávesa. Když ano,
    vypíše menu a stisknutou klávesu rovnou bere jako volbu. Když ne, spustí variantu uloženou na 003Bh.
  - Pak načte 128 B zavaděče z RAM disku (porty 60h–63h, oblast podle varianty) na 0080h, zapíše podpis a skočí
    na 0080h.
- **V emulátoru drží RAM DMR-1 baterie** (výchozí `battery = true`, soubor `work/dmr-1/sram.bin`, sdílený všemi
  sestavami jako jedna fyzická karta, `docs/desky/DMR-1.md`). Podpis i volba proto přežijí vypnutí i ukončení
  emulátoru a menu se po zapnutí neukáže. Menu vyvolá stisk klávesy hned po zapnutí (během prodlevy), nebo
  sestava s `"battery": false` u DMR-1, nebo smazání `sram.bin` při vypnutém emulátoru. Menu i volba jsou celé
  v ROM, na obrazu RAM disku nezáleží.
- Testy mají soubory baterií vypnuté (RAM po zapnutí vynulovaná), číslo varianty píšou hned po zapnutí. Klávesnice
  ho podrží, takže funguje s podpisem i bez něj.

## Chování volby systému v emulátoru (sestava `machines/cpm_zrmd.sapi`)

| Volba | Výsledek |
|---|---|
| 0 (FDC8) | BIOS ECBIF8 inicializuje řadič FDC-1 (8272, porty 0Ch/0Dh). Bez karty čte FFh a skončí na `HALT` (E6D0). **S FDC-1** (`cpm_zrmd_fdc1.sapi`) nabootuje, B: a C: jsou 8" diskety („B,C: FDC1; 8" SS,SD“). |
| 1 (RPD8) | **Nabootuje až do `A>`**, `DIR` vypíše RAM disk. Hlásí „PMD32-SD nenalezen“. |
| 2 (MFC8) | Bez karty jako 0, skončí na `HALT` při inicializaci řadiče. **S MFC-1A** (`cpm_zrmd_mfc1a.sapi`) nabootuje („B,C: MFC1; 8" SS,SD“), jen se zapojením podle překreslení EC1045 (D6 = 1 mechaniky zapnuté) a s bitem resetu 8272 jako pulz (odchylka); BIOS ECBIM8 při inicializaci kvůli chybějícímu `RET` zapíše 0FH = FFh (`docs/desky/MFC-1A.md`). |
| 3 (HDD) | „INIT HDD“ chvíli hledá IDE disk, pak hlásí „INIT HDD - nenalezen!“ a nabootuje do `A>` (ověřil autor na GUI). |

S IDE-1 a ROM `ECMON_2k_IDE-1` (sestava `machines/cpm_ide.sapi`) najde volba 3 PMD32-SD i IDE disk:
„PMD32-SD OK“, „INIT HDD SAPIemu IDE disk“, „OFFSET B: 0000H, C: 0200H“. B: a C: jsou na IDE, D: a E:
na PMD32-SD.

## ZZRMD – „přenos systému na RAM disk“

Soubory `roms/jpr-1zi/JPR-1ZI_ZZRMD_epr2k.bin` a `_epr8k.bin`. **Nejde o zavaděč CP/M**:

1. Inicializuje DSM-1 na 10h a 14h a nabídne port RAM disku (60h, nebo 68h).
2. Přijme po sériové lince Intel HEX do paměti od 2000h.
3. Zapíše 256 × 128 B (32 KB systémové oblasti) na RAM disk, pak `OUT (00h)` a smyčka.

Text zapisuje na F880, a s `OUT 63h,40h` (MAP1 z ZRMD-1) i na C080. Počítá tedy i se zobrazovací kartou
přemapovanou přes MAP na C000.
