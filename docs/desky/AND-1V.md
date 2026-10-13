# AND-1V – alfanumerický displej s výstupem VGA (Libor Lasota, podle AND-1Z)

Stav: **emulovaná** (2026-10-04), sestava `machines/sapi1v.sapi`, test `tests/test_sapi1v.cpp`.
Karta `AND-1V` (`src/cards/and1v.*`), rev 2.3:
- parametry `base`, `map` (`always`, `map1_h`, `map1_l`, `never`), `color` (propojky S1: white, green, amber, red,
  blue, cyan, yellow, magenta; mění se i za běhu přes Zobrazení → Barva, viz `docs/sestavy.md`), `font`;
- čekací stavy podle schématu (od 2026-10-09, parametr `wait_states`, výchozí zapnuté), viz „Čekání (RDY)“.
- **Výchozí jako autorova deska** (fotka `..\SAPI-karty\AND-1V\mAND-1V rev. 2.3.jpg`, autor): F800, **MAP1 = L** (kvůli
  budoucí CGA-1V). Barvu si každý nastaví podle sebe (autor), výchozí je bílá, sestava V má jantarovou
  (autor). Deska autora je **rev 2.3** s fontem **typ 1** (`font_and1v22_1024.bin` z
  `..\SAPI-karty\AND-1V\rev2.3\font_and1v22`), jako výchozí v emulátoru: ověřeno 2026-10-13 fotkou obrazovky se
  všemi 256 znaky (`tools/hwbench/and1vfont.asm`): A se špičkou, T s patkou, přeškrtnutá 0, d s ocáskem (typ 2 má
  A zaoblené, T bez patky, 0 nepřeškrtnutou). Typ 2 se od typu 1 liší ve 139 znacích, většinou o pár bodů.

Zdroje:
- [S5] https://www.pmd85.cz/?page_id=1924 (rev 2.x);
- [S6] https://www.pmd85.cz/?page_id=1765 (rev 1.x);
- [S7] `..\SAPI-karty\AND-1V\rev2.3\Popis-AND-1V-rev.-2.3.pdf` (strany 6–12 = schéma rev 2.3, listy 1/7–7/7);
- [S8] `..\SAPI-karty\AND-1V\AND-1V_sch.pdf` = **rev 1.1** (2022), `AND-1V_BOM.csv` je také rev 1.1 (15,36 MHz, 28C64,
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
- **Čekání (RDY), list 4/7 „Synchronizace sběrnicových operací“** (stejné schéma jako u CGA-1V):
  - STSTB se SEL (IC23D) nastaví IC7A, jeho /Q přes diodu D1 stáhne RDY do nuly.
  - Žádost CPU: /MR nebo /MW se SEL (IC22A, IC22B, IC5C). Vzorkuje ji IC16B sestupnou hranou VIDEO, tedy
    na začátku poloviny znaku pro CPU (VIDEO = 0 přepne multiplexery IC25, IC26, IC29 na adresu CPU). Během té
    poloviny IC30A s QA dá zápisový pulz /WE, nebo zachytí data pro čtení (/STROBE do IC27, IC28).
  - Náběžná hrana VIDEO (konec poloviny) překlopí IC16A, ten přes PRE vrátí IC16B. Náběžná hrana IC16B Q
    taktuje IC7A (D = 0) a RDY se uvolní. IC16A drží „hotovo“, dokud žádost CPU neskončí.
  - Okno je jednou za znak (10 bodů při 25,175 MHz = 397 ns) a je otevřené 5 bodů (199 ns).
  - IC7A nulují /IOR, /IOW a /RES (IC30B, IC30C), takže **IN/OUT s A8–A15 = F8–FF nečeká**.
  - Data jsou zachycená v 74LS374/373, obraz proto nezrní.
  - **Emulace (od 2026-10-09):** stejný výpočet jako u CGA-1V (`Bus::window_waits`). /RD jde dolů na sestupné
    hraně T1, /WR až na sestupné hraně T2, takže zápis dostane okno o takt později. Z80 vzorkuje /WAIT na
    sestupné hraně T2 a každého Tw. Zpoždění hradel a klopných obvodů (40 ns k žádosti, 50 ns k uvolnění) je
    převzaté z CGA-1V, kde je změřené. Na AND-1V zatím změřené není (Nejasnosti).
  - Průměrné čekání na bajt `LDIR` (bez čekání 21,18 T):

    | | 4 MHz | 2 MHz |
    |---|---|---|
    | paměť → AND-1V | +2,81 T (23,99) | +1,79 T (22,97) |
    | AND-1V → paměť | +1,23 T (22,41) | +0,58 T (21,76) |

    Měří to program VRAMBENCH (`media/and-1v`) a hlídá `test_sapi1v` (`and1v_waits`, `vrambench`). Dřívější
    odhad „0–1 takt při 2 MHz, 0–2 při 4 MHz“ platil jen pro čtení. Zápis čeká déle kvůli pozdější /WR.
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
- Dvě sady po 256 znacích: `..\SAPI-karty\AND-1V\FontROM\FontAND1V.bmp.bin` (alternativní a standardní).

## Plán emulace

- Nová karta (nebo `And1z` s parametrem `revision: 2|1`):
  - základ z pěti propojek, režim MAP (vždy / H / L / nikdy), barva ze čtyř propojek;
  - soubor fontu, u rev 1 i sada fontu;
  - vykreslení 640×480 (rev 2) nebo 384×480 zdvojeně (rev 1), VRAM 32 řádků;
  - ~~čekání jako u AND-1Z, ale bez čekání při I/O~~ hotovo 2026-10-09 podle schématu (viz Čekání).
- **Soubory do `roms/and-1v/`:**
  - z `..\SAPI-karty\AND-1V\rev2.3\font_and1v22\`: `font_and1v22_1024.bin` (typ 1), `newfont_and1v22_1024.bin` (typ 2),
    `font_and1v22_1012.bin` (rev 2.1);
  - z `..\SAPI-karty\AND-1V\FontROM\`: `FontAND1V.bmp.bin` (rev 1.x).
  - Stejné kopie jsou v `H:\SAPI-1\Klasika\AND-1V\`.

## Nejasnosti k ověření

- ~~**Čekací stavy na skutečné desce**~~: změřeno 2026-10-11 na měřicím pracovišti (`tools/hwbench/and1v.asm`,
  LDIR 256 B do karty a z karty, samostatné přístupy, takty CLK mezi značkami): při 4 i 2 MHz sedí emulace
  na 0–2 takty (4 MHz 6134 / 5730, 2 MHz 5873 / 5562). Zpoždění převzatá z CGA-1V (40 / 50 ns) tu platí;
  v okně AND-1V se uvolnění RDY vůči CLK rozkládá rovnoměrně, proto přesné hodnoty nevadí
  (`docs/zmeny-podle-hw.md`).
- **Vyřešeno 2026-10-10 (L. Lasota): STSTB při refreshi** JPR-1Z a JPR-1V nedávají díky záplatě na deskách.
  Obdobný problém s INTA (PC v okně karty) popisuje `JPR-1V.md` (STSTB). Původní otázka: pokud JPR-1V dá STSTB
  i v cyklu refreshe a registr I ukazuje do F8–FF, nastaví
  IC7A RDY bez žádosti /MR, /MW. Podle schématu ho pak uvolní jen dokončený přístup do karty, IN/OUT nebo
  /RES, a protože CPU s RDY dole stojí, zůstal by stát. Stane se to na skutečné sestavě (třeba s I = F8h),
  nebo JPR-1V při refreshi STSTB nedává? Emulace STSTB při refreshi nedává.
