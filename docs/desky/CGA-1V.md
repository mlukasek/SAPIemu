# CGA-1V – barevná grafika 320×200 s výstupem VGA (Libor Lasota)

Podklady (mimo repo): `..\CGA-1V\` – `CGA-1V_datasheet.pdf` (16 stran, registry jen jako obrázky na str. 6–7),
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
- RDY je nízko od /ISTSTB (začátek strojového cyklu s vybranou kartou) do okna CPU (IC9B). I/O cykly a /RES
  čekání ruší.
- **Emulace:** čeká se do konce aktuálního okna 318 ns (0–1 takt při 4 MHz). Je to přibližné, viz „Na
  promyšlení“ v SOUHRN.
- Řádek má 400 bodů MCLK (320 viditelných), 31,78 µs. Snímek má 525 řádků (400 viditelných), 16,68 ms (59,94 Hz).

## Emulace – poznámky

- Okno GUI „CGA-1V“ (`text_display() = false`). Snímek přes MCP `screenshot display=CGA-1V` nebo
  `sapiemu-cli --display CGA-1V --screenshot`.
- **Prolínání snímků** (`frame_blend`, výchozí zapnuto):
  - REMBRAN2 po každém VBI přepíná stránku A/B i paletu a teprve oko smíchá barvy obou snímků;
  - emulátor si na začátku VBI uloží, co se zobrazovalo (stránku, režim a barvy), a když se poslední dva
    snímky liší, kreslí jejich průměr;
  - jinak se kreslí živý stav, takže je vidět i zápis při zastaveném CPU v debuggeru.
- Po zapnutí jsou paleta i VRAM nulové (černá), COLMASK = FFh. Na skutečné desce je obsah po zapnutí náhodný.

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

- `..\CGA-1V\RADICROM.bin` (2 KB, sestupná tabulka FF FE FD…) se k CGA-1V nevztahuje (deska nemá PROM). Je
  to omylem, nebo k jiné desce?
- Přesné číslo čekacích taktů (synchronizace IC7A/B, IC9B) při 2 a 4 MHz.
- Kde přesně začíná VBI vůči STATUS (řádek 400 = začátek zatemnění, podle IC39B).
- Samostatný dokument „Programování karty“ (datasheet str. 7 na něj odkazuje) zatím nemáme.
