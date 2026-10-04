# AND-1V – alfanumerický displej s výstupem VGA (Libor Lasota, podle AND-1Z)

Stav: **emulovaná** (2026-10-04), sestava `machines/sapi1v.sapi`, test `tests/test_sapi1v.cpp`.
Karta `AND-1V` (`src/cards/and1v.*`), rev 2.3:
- parametry `base`, `map` (`always`, `map1_h`, `map1_l`, `never`), `color`, `font`;
- čekací stavy se neemulují.
- **Výchozí jako autorova deska** (fotka `..\AND-1V\mAND-1V rev. 2.3.jpg`, autor): F800, **MAP1 = L** (kvůli
  budoucí CGA-1V). Barvu si každý nastaví podle sebe (autor), výchozí je bílá. Font vypálený
  v desce autor neví, výchozí je typ 1.

Zdroje:
- [S5] https://www.pmd85.cz/?page_id=1924 (rev 2.x);
- [S6] https://www.pmd85.cz/?page_id=1765 (rev 1.x);
- [S7] `..\AND-1V\rev2.3\Popis-AND-1V-rev.-2.3.pdf` (strany 6–12 = schéma rev 2.3, listy 1/7–7/7);
- [S8] `..\AND-1V\AND-1V_sch.pdf` = **rev 1.1** (2022), `AND-1V_BOM.csv` je také rev 1.1 (15,36 MHz, 28C64,
  6116).

## Rev 2.3 (aktuální)

- **Obraz:**
  - 64×20 znaků, znak 10×24 px, VGA 640×480/60 Hz, čtvercové pixely;
  - jednobarevně, 1 z 15 barev inkoustu na černé;
  - **bez atributů, kurzoru, blikání a inverze**.
- **Znaková sada** kompatibilní s AND-1Z [S7 str. 4]: 00–1F rámečky, ASCII, 80–BF semigrafika 2×3, C0–FF
  KOI-8čs2.
- **VRAM 2 KB:** offset = řádek·64 + sloupec. Řádků je 32, viditelné 0–19, řádky 20–31 jsou obyčejná RAM
  pro CPU. Žádné porty I/O ani přerušení.
- **Dekódování bez PROM** (dva 74LS85, propojky S2, 7 poloh, osazená = 0):
  - /A11–/A15: základ v krocích po 2 KB (osazená = 0, tedy invertovaně), pro F800 všech pět rozpojených.
  - **MAP1OFF** (11-12) a **H/L** (13-14):
    - MAP1OFF osazená = vybrána vždy;
    - MAP1OFF rozpojená a H/L osazená = při MAP1 = 1;
    - obě rozpojené = při MAP1 = 0;
    - obě osazené = nikdy.
  - /RES výběr vypne.
  - Výchozí stav: F800, MAP1OFF osazená, H/L rozpojená [S7 str. 5].
- **Čekání (RDY):**
  - STSTB se SEL nastaví klopný obvod a RDY jde do nuly. Uvolní se po synchronizaci s oknem CPU (jedno okno
    na znak, 10 bodů při 25,175 MHz ≈ 397 ns), tedy 0–1 čekací takt při 2 MHz a 0–2 při 4 MHz.
  - Klopný obvod nuluje /IOR, /IOW a /RES, takže **IN/OUT s A8–A15 = F8–FF nečeká**.
  - Data jsou zachycená v 74LS374/373, obraz proto nezrní.
- **Časování:** bodová frekvence 25,175 MHz, poloviny znaků po 5 bodech, 160 polovin = 800 bodů (standardní
  VGA).
- **Znakový generátor** (27C256/28C256, list 2/7, `font_and1v22\main.c`):
  - A0–A3 = L1, L2, L4, L8, A4–A11 = kód, A12 = pravá polovina, A13 = L0 (lichý mikrořádek), A14 = 1.
  - Mikrořádek m = 0..23: line = m >> 1, index = line < 6 ? line + 2 : line + 4. Indexy 0, 1, 8, 9 jsou 0.
  - Body: D3 vlevo, pak D4–D7, D0–D2 nepoužité.
  - 32KB soubory: stránky 0 = 2 a 1 = 3 (ověřeno MD5).
  - `font_and1v22_1012.bin` (8 KB) je font 10×12 pro rev 2.1 s 28C64 (bez A13, řádky zdvojené). Rozpiska rev
    2.3 uvádí AT27C256.
- **Barva:** S1 = 4 propojky (/BLU, /RED, /GRH, /GRL), osazená = vypnuto, vše rozpojené = bílá. Zelená má
  2 bity.

## Rev 1.x

- 15,36 MHz, znak 6×12 se zdvojenými řádky, fyzicky 384×480. Pixely jsou asi 5:3 široké.
- Dekódování: S2 pin 14 = MAP1, pin 12 = GND, legenda „MAP1 IGNORE / MAP1=H / MAP1=L / DISABLE“ [S8 list 4].
- 28C64: A0–A3 = řádek, A4–A11 = kód, **A12 = sada fontu z S1 9-10**. 16 B na znak (řádky 0–5 a 8–13), body
  D7 (vlevo) až D2, stejně jako AND-1Z.
- Dvě sady po 256 znacích: `..\AND-1V\FontROM\FontAND1V.bmp.bin` (alternativní a standardní).

## Plán emulace

- Nová karta (nebo `And1z` s parametrem `revision: 2|1`):
  - základ z pěti propojek, režim MAP (vždy / H / L / nikdy), barva ze čtyř propojek;
  - soubor fontu, u rev 1 i sada fontu;
  - vykreslení 640×480 (rev 2) nebo 384×480 zdvojeně (rev 1), VRAM 32 řádků;
  - čekání jako u AND-1Z, ale bez čekání při I/O.
- **Soubory do `roms/and-1v/`:**
  - z `..\AND-1V\rev2.3\font_and1v22\`: `font_and1v22_1024.bin` (typ 1), `newfont_and1v22_1024.bin` (typ 2),
    `font_and1v22_1012.bin` (rev 2.1);
  - z `..\AND-1V\FontROM\`: `FontAND1V.bmp.bin` (rev 1.x).
  - Stejné kopie jsou v `H:\SAPI-1\Klasika\AND-1V\`.
