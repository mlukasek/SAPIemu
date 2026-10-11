# CGA-1V – barevná grafika 320×200 s výstupem VGA (Libor Lasota)

Podklady (mimo repo): `..\SAPI-karty\CGA-1V\` – `CGA-1V_datasheet.pdf` (16 stran, registry jen jako obrázky na str. 6–7),
`schema\cga1v11(3).zip` (7 listů PNG, rev 1.1), `foto\cga1v_timing.png` (časové průběhy), fotka desky
`mCGA-1V rev. 1.1.jpg`, programy v `sw\` (Intel HEX, CP/M `.COM` od 0100h). Kopie programů jsou
v `media/cga-1v/`.

Emulace: `src/cards/cga1v.*`, typ karty `CGA-1V`, v sestavě `machines/sapi1v.sapi` na pozici X7. Test
`test_sapi1v` (CASTLE, REMBRAN2, VBI).

## Parametry

- 16 KB adresního prostoru včetně registrů, báze 0000/4000/8000/C000h.
- 320×200 bodů, 80 bajtů na řádek, 4 body na bajt, 16000 bajtů (0000–3E7F). D7 je levý bod.
- Dvě stránky videoram A a B (SRAM 62256). CPU a zobrazení si stránku volí nezávisle.
- RAMDAC Bt476: 256 barev z 262144 (6 bitů na R, G, B).
- Oscilátor 25,175 MHz, VGA 640×480/60 Hz. Každý bod je 2×2 body VGA, zobrazuje se 400 řádků z 525.
- Přerušení 60 Hz na začátku vertikálního zatemnění (VBI) na /INT0.

## Propojka S1 (vlevo dole)

Osazená propojka = 0, rozpojená = 1 (pull-up R6). Porovnávač 74LS85 (IC40) srovnává A15, A14, MAP1, MAP2 s propojkami.
/RES (vstup A=B) výběr vypíná.

| Propojky | Význam |
|---|---|
| /A15, /A14 | báze v negovaném tvaru: obě rozpojené = C000h, /A14 osazená = 8000h, /A15 = 4000h, obě = 0000h |
| M1EN, M1LO | rozpojené obě = MAP1 se ignoruje; M1EN = karta při MAP1 = H; M1EN + M1LO = při MAP1 = L; jen M1LO = karta vypnutá |
| M2EN, M2LO | totéž pro MAP2 |

- **Autorova deska** (fotka): osazené M1EN a M2EN, tedy C000h při MAP1 = MAP2 = H. Je to doporučené nastavení
  pro ZPS-3/4 a CP/M.
- Programy přepínají mapování přes RAM-1V `OUT 63h,C0h` a zpět `OUT 63h,00h`.
- RAM-1V má při MAP1 = H na C000–FFFF díru, AND-1V je proto na MAP1 = L.

## Registry (offset od báze)

Dekódování ze schématu (list 5):
- **IA6–IA13 = 1** znamená oblast registrů **3FC0–3FFF**.
- **A2 = 0**: CONFIG (zápis), A2 = 1: Bt476, RS0 = A0, RS1 = A1.
- Datasheet uvádí registry na 3FFB–3FFE. Ostatní adresy 3FC0–3FFF jsou jejich zrcadla.
- Oblast 3E80–3FBF je jen nezobrazená RAM.

| Offset | Zápis | Čtení |
|---|---|---|
| 3FFB (A2 = 0) | CONFIG: D0 zobrazená stránka (0 = A), D1 stránka pro CPU, D2 0 = EGA / 1 = CGA, D3 povolení IRQ, D7 = 1 kvituje VBI | STATUS: D7 = požadavek VBI |
| 3FFC | Bt476 adresa barvy (zápis) | |
| 3FFD | Bt476 data palety: R, G, B (6 bitů), pak adresa + 1 | |
| 3FFE | Bt476 COLMASK (AND na P7..P0) | |
| 3FFF | Bt476 adresa pro čtení (deska palety nečte, /RD nezapojen) | |

- **Zápis do oblasti registrů jde i do SRAM** (/WE se negeneruje podle adresy). Platí to pro stránku vybranou
  pro CPU.
- **Čtení z oblasti registrů:** D0–D6 jsou ze SRAM, D7 = požadavek VBI (READZ: IC42B/D, IC34 bit 7). STATUS je
  tedy čitelný na kterékoli adrese 3FC0–3FFF.
- **CONFIG** (IC17 74LS175) nuluje /RES. Klopný obvod VBI (IC9A) ani RAMDAC se resetem nenulují.
- **VBI** nastaví IC9A náběžnou hranou VZAT (řádek 400). Nuluje ho /IACK, což je zápis CONFIG s D7 = 1 (IC8A; pulz
  končí dalším /ISTSTB). /INT0 = IRQ AND IEN (74LS38, otevřený kolektor).
- CGATEST hlásí „60Hz, INT0, RST7“. Přerušení bez vektoru je v IM0 na sběrnici FF = RST 7.

## Obraz

Bajt videoram → vstupy RAMDAC P7..P0 (list 2: posuvné registry IC15, IC16 74LS194):

- **P7:P6:P5** = V256:V128:V64 = barvová doména `y >> 5` (6 domén po 32 řádcích, sedmá jen 8 řádků).
- **P0** = bit D7..D4 pro body 0..3 (IC15 posouvá doleva).
- **P4..P1:**
  - EGA: dolní půlbajt D3..D0, společný atribut čtveřice;
  - CGA: IC16 posouvá (S1 = LOAD OR CGA), takže P4 = D3..D0 pro body 0..3 a P3..P1 jsou vysunuté zbytky.
    Programy je maskují přes COLMASK (např. 11h).
- **Index barvy** = (P7..P0) AND COLMASK.
- Adresa bodu = `y * 80 + x / 4` (sčítačky IC23, IC18: řádek × 5 × 16), stránka podle CONFIG D0.

## Časování

- MCLK = 25,175/2 MHz (bod). Čtveřice bodů (QA, QB) se střídá: 2 takty VIDEO, 2 takty CPU (`cga1v_timing.png`),
  okno CPU je tedy každých 318 ns.
- RDY je nízko od /ISTSTB (začátek strojového cyklu s vybranou kartou) do konce okna CPU (IC9B). I/O cykly a /RES
  čekání ruší. Podle listu 4:
  - STSTB (na JPR-1V krátký impuls z 74LS123 na začátku /MREQ) se SEL nastaví IC9B, RDY = 0;
  - /READ nebo /WRITE převezme IC7B náběžnou hranou QA, tedy na začátku okna CPU;
  - IC7A okno ukončí hranou /QA o půl periody později, náběžná hrana /CPU vynuluje IC9B a RDY se uvolní.
- **Zápis čeká déle než čtení:** Z80 dává /RD (s /MREQ) na sestupné hraně T1, ale /WR až na sestupné hraně T2,
  kdy už poprvé vzorkuje /WAIT. Zápis tak dostane okno o takt později.
- **Emulace (od 2026-10-09, zpoždění od 2026-10-11):** přesně tento sled. /WAIT se vzorkuje na sestupné hraně
  T2 a každého Tw, čekání je v celých taktech CPU (i při 2 MHz na JPR-1V). Zpoždění jsou **změřená sondami na
  QA a /CPU (IC7) na desce autora** (`docs/zmeny-podle-hw.md`): /MR přijde 40–48 ns po sestupné hraně CLK, /MW
  32–40 ns, IC7B žádost vezme jen 24–32 ns před hranou QA, takže žádost platí **72 ns (čtení) a 64 ns (zápis)**
  po hraně CLK; RDY stoupne 200–216 ns po přidělující hraně QA (okno 159 ns + 55 ns včetně předstihu /WAIT
  u Z80). Dřív 40 / 50 ns podle ZRDBENCH, čtení při 4 MHz pak čekalo o 0,36 T méně a zápis při 2 MHz o 0,3 T.
- **Měření** smyčkami ZRDBENCH (`media/zrd-1v/ZRDBENCH.ASM`), 2026-10-11 v ROM na měřicím pracovišti
  (`tools/hwbench/zrdtimer.asm`: časovač 82C54 i analyzátor dávají totéž na tisícinu). Takty na bajt `LDIR`,
  bez čekání 21,176:

  | | železo 4 MHz | emulace 4 MHz | železo 2 MHz | emulace 2 MHz | emulace do 2026-10-10 |
  |---|---|---|---|---|---|
  | paměť → CGA-1V | 23,608 | 23,627 | 22,757 | 22,757 | 23,564 / 22,44 |
  | CGA-1V → paměť | 22,757 | 22,765 | 21,272 | 21,272 | 22,408 / 21,27 |

  Kratší a delší bloky (`cga1v.asm`, `cga1v_long.asm`) sedí na ±2 takty, 12 KB čtení na 0,04 %. ZRDBENCH
  8. 10. naměřil při 4 MHz čtení 22,44 se ZRD-1V místo ZRMD-1 a jiným pořadím karet: čtení je okrajové (o okně
  rozhoduje pár ns kolem QA), jiné zatížení sběrnice ho posune. Test `test_sapi1v` hlídá `LDIR` (`cga_waits`)
  i ZRDBENCH pod CP/M (`zrdbench`, tabulka v `docs/desky/ZRMD-1.md`).
- Řádek má 400 bodů MCLK (320 viditelných), 31,78 µs. Snímek má 525 řádků (400 viditelných), 16,68 ms (59,94 Hz).

## Emulace – poznámky

- Okno GUI „CGA-1V“ (`text_display() = false`). Snímek přes MCP `screenshot display=CGA-1V` nebo
  `sapiemu-cli --display CGA-1V --screenshot`.
- **Kreslení s paprskem** (`beam`, výchozí zapnuto, od 2026-10-09 podle podnětu z portu Prince of Persia):
  - obraz se kreslí po řádcích tak, jak je paprsek míjí (VGA řádky 2y a 2y+1 = řádek bodů y, snímek začíná
    řádkem 0, VBI na řádku 400), ze stavu karty v tu chvíli. Před každým zápisem do karty se dokreslí řádky,
    které paprsek už přejel;
  - přepnutí zobrazené stránky, režimu EGA/CGA, COLMASK nebo palety uprostřed snímku a kreslení do zobrazené
    stránky se tak projeví jako na monitoru (trhání, obraz napůl starý). Dřív se kreslil celý snímek z jednoho
    stavu a tyhle chyby byly v emulátoru neviditelné;
  - **pomůcka pro programátora** (MCP `machine_info`, údaje karty CGA-1V): `palette_writes_visible` (zápisy do palety Bt476
    během zobrazování, na skutečném monitoru rušené body), `palette_writes_visible_last_frame`,
    `mid_frame_switches` (posledních 16 přepnutí stránky, režimu nebo COLMASK uprostřed obrazu s číslem snímku
    a řádku), `beam_line`. První zápis do palety během zobrazování se jednou ohlásí v logu;
  - **„sníh“ Bt476** (`palette_snow`, výchozí vypnuto): zápis do palety během zobrazování nakreslí na místě
    paprsku bod v odstínu šedi podle zapsaného bajtu. Jak přesně rušení vypadá na skutečné kartě, nevíme, je to
    jen upozornění;
  - s `beam: false` se kreslí celý obraz z okamžitého stavu jako dřív.
- **Prolínání snímků** (`frame_blend`, výchozí zapnuto):
  - REMBRAN2 po každém VBI přepíná stránku A/B i paletu a teprve oko smíchá barvy obou snímků;
  - emulátor si na začátku VBI uloží, co se zobrazovalo (stránku, režim a barvy), a když se poslední dva
    snímky liší, kreslí jejich průměr;
  - prolínají se dva poslední dokončené snímky nakreslené s paprskem. Při zastaveném CPU (debugger) se po
    zápisu do karty obraz překreslí celý z okamžitého stavu, aby byl zápis vidět.
- Po zapnutí jsou paleta i VRAM nulové (černá), COLMASK = FFh. Na skutečné desce je obsah po zapnutí náhodný,
  s volbou sestavy `"power_on_memory": "random"` (`docs/sestavy.md`) je náhodná i VRAM a paleta.

## Programy (`media/cga-1v/`)

| Soubor | Co dělá | `SAVE` |
|---|---|---|
| `castle.hex` | obrázek hradu, EGA 32 barev (COLMASK 1Fh) | 64 |
| `rembran1.hex` | Rembrandt (stařec), EGA | 64 |
| `rembran2.hex` | Rembrandt (Syndikové), CGA, střídání stránek a palet po snímcích | 127 |
| `cga_test.hex` | CGATEST rev. 0.99: menu testů 2–7 (EGA, RAMDAC, videoram, VBI, STATUS, homogenní pole) | 18 |

- **Postup v CP/M** (sestava V):
  1. nahrát HEX (Soubor → Nahrát program do paměti, MCP `load_hex`);
  2. `SAVE n JMENO.COM`;
  3. spustit `JMENO`.
- Obrázky běží, dokud se nestiskne klávesa.
- CGATEST rev. 0.99 nemá v menu volbu 1 (test CGA): v kódu chybí `CP 31h`, nejde o chybu emulátoru.
- Ověřeno v emulátoru:
  - test videoram „16000 byte OK“;
  - VBI počítá přerušení;
  - STATUS kreslí značky VBI po 1/60 s;
  - EGA s doménami a RAMDAC (přechody) vypadají správně.

## Nejasnosti k ověření

- `..\SAPI-karty\CGA-1V\RADICROM.bin` (2 KB, sestupná tabulka FF FE FD…) se k CGA-1V nevztahuje (deska nemá PROM). Je
  to omylem, nebo k jiné desce?
- ~~Přesné číslo čekacích taktů při 2 a 4 MHz, zápis při 2 MHz (1,57 proti 1,27)~~: vyřešeno 2026-10-11
  změřenými zpožděními (sondy na QA a /CPU, viz Časování). Zápis při 2 MHz teď sedí bez zvláštního ladění.
- ~~Kde přesně začíná VBI vůči STATUS~~: změřeno 2026-10-11 (`tools/hwbench/vbi.asm`, `docs/zmeny-podle-hw.md`):
  STATUS D7 naskočí 50 řádků před sestupnou hranou VSYNC (VSYNC záporný, 2 řádky, 59,940 Hz, 525 řádků). Obraz
  je ve VGA časování uprostřed (řádky 40–439), VBI začíná řádkem 440 = řádek 400 karty; do dalšího obrazu zbývá
  125 řádků, jako v emulaci.
- **INTA s PC v okně karty** (L. Lasota 2026-10-10, ověřeno pokusem): STSTB z JPR-1V při INTA s adresou v okně
  nastaví IC9B a RDY se už neuvolní (žádné /READ ani /WRITE). RESET to nevyřeší (JPR-1V ho vzorkuje s /M1), viz
  `JPR-1V.md`. Emulace výběr karty při INTA nedělá. Ověřit po Liborově záplatě IC15 na JPR-1V.
- Samostatný dokument „Programování karty“ (datasheet str. 7 na něj odkazuje) zatím nemáme.
