# Změny emulátoru podle měření na skutečném HW

Sem se zapisuje každá změna emulátoru, která vychází z měření na skutečné sestavě, a také to, co měření
potvrdilo beze změny. U každé změny jsou doklady (schéma, měření, testovací program), aby šla zopakovat.

**Postup (autor, 2026-10-11):** nález se nejdřív popíše s doklady a navrhne, autor ho schválí, teprve potom se
implementuje. Podezření bez potvrzení se do emulátoru nedávají, zapisují se do části „Podezření k dořešení“.

## Měřicí pracoviště

- Sestava V u autora: JPR-1V (Z80 4 MHz, TURBO), RAM-1V, AND-1V, CGA-1V, DSM-1V, ZRMD-1, MPH-1V.
- **PicoROM P28** (`SAPI_P28-01`) v patici ROM JPR-1V: testovací ROM se nahrává jen do RAM PicoROM (ve flash
  zůstává ECMON autora). Reset: výstup LOW PicoROM na /RTL, `picorom reset … low`, 0,3 s, `z` (kratší impuls
  reset nevyvolá). Komunikační kanál PicoROM (okno 0400h–07FFh) přes `pypicorom` s opravou knihovny `picolink`.
- **Sériová linka:** DSM-1V kanál A (10h), 9600 Bd, převodník MOXA na COM13.
- **Saleae Logic Pro 16**, Logic 2 v2.4.48, ovládání přes Automation API, vzorkování **125 MS/s** (8 ns) na všech
  16 kanálech. Zapojení: D0 /RES, D1 RDY, D2 /IOR, D3 /IOW, D4 TxD A, D5 RxD A, D6 /MR, D7 /MW, D8 STSTB, D9 /INT0,
  D10 MAP1, D11 MAP2, D12 CLK (Z80 vývod 6), D13 /M1 (Z80 vývod 27), D14 /CPU a D15 QA karty CGA-1V (IC7
  vývody 9 a 11; D14 je později na VSYNC).
  Podezřelé hrany se kontrolují i analogově.
- **Kanál PicoROM občas přidá falešné bajty** (na začátku relace `FBh 50h`, jednou `FEh FEh` a `FFh` mezi dvěma
  znaky; program takové adresy nečte). Data se proto posílají s rámcem (`*` a šestnáctkové číslice), PC ostatní
  bajty zahodí. Původ zatím nevíme.
- Programy a skripty: `tools/hwbench/` (popis v `tools/hwbench/README.md`), výstupy ve `work/hwbench/`.
- **Kontrola zapojení** (`wire.asm`, `wire_check.py`, 2026-10-11): všech 16 kanálů ukazuje přesně to, co program
  dělá (počty impulzů /IOW, /IOR, /MR, /MW, čekání RDY, MAP1/MAP2, text na TxD, příjem na RxD a /INT0,
  CLK 4,000 MHz, /M1, STSTB).

## Změny

### 2026-10-11 – DSM-1V: reset UARTu (MR) trvá do prvního čtení paměti na sběrnici

- **Na desce:** MR obvodu 6402 drží klopný obvod ze dvou hradel 74LS10 (IC27 kanál A, IC27 + IC28 kanál B).
  Nastaví ho /RES (/IRES) nebo zápis na základ+3 (/WR3), uvolní ho až /IMR = **/MR ze sběrnice** (přes 74LS244
  IC26). JPR-1V při čtení vlastní ROM /MR na sběrnici nedává. Popis desky to zmiňuje jako „prodloužení MR“.
- **Měření:**
  1. `hello.asm` bez čtení RAM: na TxD nic, TBRE čte pořád 1 (žádné čekání), COM13 nepřijme nic.
  2. `diag.asm`: stav po resetu 7Fh, modem 10h (kanál A s MOXA), po zápisu na +3 i po 8N1 stav 7Fh.
  3. `diag2.asm`: bajt 00h zapsaný před prvním čtením RAM chybí, všechny další (01h–27h, dávka, „XY“,
     „K“–„T“ s TBRE) dorazily jako v emulátoru.
  4. `mr.asm` se signály sběrnice: znak A hned po nastavení a znak B po čtení ROM (žádný /MR) se ztratily,
     jediný /MR (čtení 8000h) v 14,05 ms, hned za ním znak C, TxD od 14,17 ms; linka přenesla „CD“.
  5. `hello.asm` s jedním `LD A,(8000h)` po nastavení UARTu vysílá a čeká na TBRE normálně.
- **Změna:** `Dsm1` (jen DSM-1V): příznak `mr_held` nastaví reset a zápis na +3. Dokud trvá, zápis dat se
  zahodí, přijaté znaky se ztratí a stav se čte 7Fh. `Dsm1v::mem_read` (každé čtení paměti na sběrnici, i DMA)
  ho uvolní. Do uloženého stavu se nedává (trvá do prvního čtení RAM, staré stavy sestav V zůstanou čitelné).
- **Test:** `test_sapi1v` (`dsm1v_mr`): stejný program jako `mr.asm`, linka musí přenést „CD“. Emulátor
  s `emu_run.py` dává pro `mr`, `hello` a `diag2` totéž co deska.
- **Netýká se:** původní DSM-1 a DMR-1 (MHB1012) zůstávají beze změny, jejich schéma se na to ještě neprošlo
  (Nejasnosti v `docs/desky/DSM-1.md`).

### 2026-10-11 – DSM-1V: UART převezme znak do vysílacího registru až na hraně taktu 16×

- **Měření** (`tbre.asm`, `tbre_run.py`, 2026-10-11, 9600 Bd, takt 16× = 153,6 kHz, perioda 6,51 µs):
  1. jeden zápis dat (/IOW 0,62 µs): start bit 0,9 µs po začátku zápisu, stav potom 7Fh;
  2. dva zápisy 2,7 µs po sobě: **na lince jen jeden znak**, start bit 6,4 µs po prvním zápisu, stav celých
     336 µs po zápisech 7Fh (TBRE = 1, žádný znak nečeká);
  3. začátky obou odeslaných znaků jsou od sebe 3,1835 ms = **přesně 489 period taktu 16×**: vysílání začíná
     vždy na hraně taktu.
- **Výklad:** zápis naplní vyrovnávací registr (TBRE = 0), do vysílacího registru se znak přesune až na nejbližší
  hraně taktu 16×. Druhý zápis před touto hranou první znak přepíše. Dřívější „XY“ (`diag2.asm`, zápisy 4,5 µs
  od sebe) prošlo, protože hrana mezi zápisy náhodou byla. Prodloužení /IOW to nevysvětluje (zápis trvá 0,62 µs).
- **Emulátor před změnou:** znak převzal hned, druhý čeká ve vyrovnávacím registru (TBRE = 0 asi 1 ms) a odejdou oba
  (stejná ROM: „55 55 55“ proti „55 55“ na desce).
- **Změna (autor schválil):** `Mhb1012::clock_grid` (zapnuté jen u DSM-1V): zápis dat si spočítá nejbližší hranu
  taktu 16× (mřížka od času 0, fázi skutečné desky neznáme), převzetí a start bit jsou na té hraně nebo na konci
  předchozího znaku. Zápis rychleji než jedna perioda taktu (6,5 µs při 9600 Bd, 0,54 µs při 115200 Bd) přepíše
  předchozí znak jako na desce. Čas hrany se neukládá do stavu (nejvýš jedna perioda).
- **Test:** `test_sapi1v` (`uart_clock_grid`): dva zápisy v jedné periodě pošlou jen druhý znak, start na hraně,
  znak zapsaný během vysílání čeká; bez mřížky odejdou oba. ROM `tbre.asm` v emulátoru pošle dva nebo tři znaky
  podle toho, kam padne hrana mezi zápisy 11 taktů od sebe (perioda 26 taktů), stejně jako by to na desce
  záviselo na fázi děliče.
- **Netýká se:** MHB1012 na DSM-1 a DMR-1 je nejspíš stejný (odvozený od AY-5-1013), ale neměřený, zůstává beze
  změny (Nejasnosti `docs/desky/DSM-1.md`). Autor ho zkusí změřit na skutečné DSM-1.

### 2026-10-11 – CGA-1V: zpoždění žádosti a uvolnění podle sond na oknu karty

- **Sondy:** D15 na QA (IC7 vývod 11, začátek okna CPU, 3,1469 MHz = 25,175 MHz / 8), D14 na Q IC7B („/CPU“,
  vývod 9). Okno od náběžné hrany QA do sestupné (159 ns), na každý přístup dvě hrany /CPU (`cga_window.py`).
- **Řetěz po přístupech** (`cga1v.asm`, 4 MHz): /MW 32–40 ns po sestupné hraně CLK (T2), /MR 40–48 ns (T1);
  IC7B žádost vezme jen 24 ns (zápis) a 24–32 ns (čtení) před hranou QA, žádost tedy platí asi **64 a 72 ns**
  po hraně CLK (model měl 40); RDY (1,65 V) stoupne 200–216 ns po přidělující hraně QA; Z80 udělá takt navíc,
  když RDY stoupne méně než 10 ns před svou sestupnou hranou (předstih /WAIT a pomalá hrana RDY, 140 ns).
- **Smyčky ZRDBENCH na desce** (`zrdtimer.asm`, 82C54 i analyzátor shodně): 4 MHz paměť → CGA 23,608, CGA →
  paměť 22,757; 2 MHz 22,757 a 21,272 T na bajt (bez čekání 21,176).
- **Změna (autor schválil):** CGA-1V žádost 72 ns při čtení, 64 ns při zápisu, uvolnění 55 ns. Emulátor proti
  desce: 2 MHz LDIR 256 B 0 / 0 taktů, samostatné přístupy +1 / +2; 4 MHz 256 B +2 / 0, 12 KB +6 / +108 (0,04 %);
  smyčky ZRDBENCH 4 MHz 23,627 / 22,765. Starý model měl při 4 MHz čtení −4504 taktů na 12 KB a při 2 MHz zápis
  −84 na 256 B (stará Nejasnost 1,57 proti 1,27 je tím vyřešená, bez zvláštního ladění).
- **Rozdíl proti 8. 10.:** ZRDBENCH tehdy dal čtení při 4 MHz 22,44. Sestava byla jiná: ZRD-1V místo ZRMD-1,
  jiné pořadí karet (autor kvůli kartě s připojením analyzátoru posunul RAM-1V, CGA-1V a MPH-1V). Čtení je
  okrajové (rozhoduje pár ns kolem hrany QA), zápis i 2 MHz se tehdy i dnes shodují.
- **Testy:** `test_sapi1v` (`cga_waits`, `zrdbench`) hlídají dnešní hodnoty s tolerancí ±0,1 T.

## Ověřeno beze změny

- **JPR-1V nedává /MR při čtení ROM** (0000h–07FFh): při kontrole zapojení byl /MR jen u čtení RAM a AND-1V.
  Emulátor to tak dělá (`docs/desky/JPR-1V.md`).
- **STSTB na JPR-1V** (`ststb.asm`, `ststb_run.py`, 2026-10-11, deska autora bez Liborovy záplaty): kladný
  impuls 88–96 ns na začátku **každého paměťového cyklu včetně čtení ROM**, **každého I/O cyklu** a **při INTA**,
  **ne při refreshi** (8× NOP: 9 výběrů instrukce + 1 operand = 10 STSTB; 4× IN (n): 14 = 10 paměťových + 4 I/O).
  Při INTA (/M1 1000 ns místo 500 ns) přijde STSTB 0,66 µs po začátku /M1. Emulátor STSTB bez refreshe
  předpokládá; výběr karty při INTA (zablokování RDY s PC v okně grafické karty) zatím nemodeluje, to je
  poslední krok měření (zamrzání FXS4).
- **AND-1V při IN/OUT s A8–A15 = F8h nečeká** (OUT (C) a IN (C) s B = F8h, 4× každé: žádný impuls RDY), jak
  plyne ze schématu (IC7A nuluje /IOR, /IOW) a jak to emulátor dělá.
- **Čekání AND-1V při 4 MHz** (`and1v.asm`, `and1v_run.py`, takty CLK mezi značkami, 2026-10-11): LDIR 256 B do
  karty 6134 taktů (emulátor 6136), z karty 5730 (5731), 16 samostatných zápisů 1385 (1386), čtení 1360 (1360).
  Čekací takty zápisu 2–3, čtení 1–2. Model AND-1V (převzatý z CGA-1V) při 4 MHz sedí.
- **Zápisy do CGA-1V při 4 MHz** (`cga1v.asm`, `cga1v_long.asm`): LDIR 12 KB do karty 288 097 taktů, starý model
  287 319 (−0,27 %, nový viz Změny), samostatné zápisy na takt přesně. Čekací takty 2–3, průměr 2,44 na bajt.
- **RDY je otevřený kolektor přes diodu s pull-upem:** náběžná hrana 10–90 % asi 140 ns, úrovně 0,3 V a 4,8 V
  (analogově, `rdy_analog.py`). Krátké „impulzy“ RDY v digitálním záznamu (8 ns) jsou jen šum při pomalém
  průchodu prahem, analogově je hrana plynulá.
- **Měření ZRDBENCH je přesné** (`zrdtimer.asm`, `zrdtimer_run.py`: jeho smyčky i výpočet z 82C54 v ROM, testy
  z RAM jako pod CP/M, ve stejném běhu analyzátor): časovač a analyzátor se shodují na tisícinu (paměť → paměť
  21,176, paměť → CGA 23,608, CGA → paměť 22,757; CPU podle kalibrace 3,9999 MHz). Metoda je tedy přesná,
  rozdíl proti 8. 10. (22,44) je na straně sestavy: tehdy byla v něčem jiná (karty, jejich pořadí, teplota).
  Čtení z CGA-1V je okrajové, o okně rozhoduje pár ns.
- **Čekání AND-1V při 2 MHz** (TURBO vypnuté): LDIR 256 B do karty 5873, z karty 5562 taktů, emulátor totéž;
  samostatné přístupy −1 / 0. Při 4 i 2 MHz tedy model AND-1V sedí.
- **Port 63h → MAP1, MAP2:** OUT 40h → MAP1, 80h → MAP2, C0h → oba, 00h → nic, aktivní v H. Shoduje se
  s nastavením RAM-1V v sestavě (bit 6 → MAP1, bit 7 → MAP2). Na 63h ale odpovídá i ZRMD-1, kdo čáry budí, se
  z tohoto měření nedá rozlišit (otázka JP2 u ZRMD-1 zůstává).
- **AND-1V čeká při čtení videopaměti:** 16 čtení F800h, 16 impulzů RDY (délky se zatím nevyhodnocovaly).
- **Časování CPU:** zpožďovací smyčka 65 536 × 24 taktů trvá 393,5 ms jako při 4,000 MHz bez čekání; emulátor
  dává totéž.

## Podezření k dořešení

