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
- **Analyzátor ovlivní čtení z CGA-1V při 4 MHz** (2026-10-11): s kartou, přes kterou je analyzátor připojený
  ke sběrnici, dává smyčka ZRDBENCH CGA → paměť 22,757 T na bajt, bez ní 22,513 a 22,523 (dvakrát, i bez sond
  D12–D15 na IC7). Zápis do CGA-1V (23,61) a obě hodnoty při 2 MHz (22,75 / 21,27) se nezmění. Čtení leží při
  4 MHz na hraně okna, pár ns zátěže sběrnice ho posune, stejně tak jiné karty nebo jejich pořadí. Měření čekání
  s analyzátorem proto u čtení CGA-1V při 4 MHz neodpovídá stroji bez něj; bez karty analyzátoru se měří jen
  časovačem (`zrdtimer_manual.py`, RESET autor zmáčkne ručně, protože /RTL vede ta karta).
- **Kanál PicoROM občas přidá falešné bajty** (na začátku relace `FBh 50h`, jednou `FEh FEh` a `FFh` mezi dvěma
  znaky; program takové adresy nečte). Data se proto posílají s rámcem (`*` a šestnáctkové číslice), PC ostatní
  bajty zahodí. Původ zatím nevíme.
- **Past kanálu PicoROM:** PicoROM sleduje jen adresu v okně ROM, ne druh cyklu. `IN A,(n)` (horní bajt = A),
  `IN r,(C)`, `INI`, `INIR` (horní bajt = B) s horním bajtem 04h–07h je pro něj čtení komunikačního okna
  0400h–07FFh: 07xxh pošle bajt, 06xxh vezme vstup. Při rychlém sledu takových čtení se firmware zasekne
  („USB send timeout“, pomůže odpojit USB). Testovací ROM proto drží horní bajt I/O mimo 04h–07h (2026-10-13).
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
  paměť 22,757; 2 MHz 22,757 a 21,272 T na bajt (bez čekání 21,176). **Bez karty analyzátoru** (`zrdtimer_manual.py`)
  4 MHz 23,607–23,609 a **22,513–22,523**, 2 MHz 22,750–22,752 a 21,272–21,273.
- **Změna (autor schválil):** CGA-1V žádost 72 ns při čtení, 64 ns při zápisu, uvolnění 55 ns. Emulátor proti
  desce: 2 MHz LDIR 256 B 0 / 0 taktů, samostatné přístupy +1 / +2; 4 MHz 256 B +2 / 0, 12 KB +6 / +108 (0,04 %);
  smyčky ZRDBENCH 4 MHz 23,627 / 22,765. Starý model měl při 4 MHz čtení −4504 taktů na 12 KB a při 2 MHz zápis
  −84 na 256 B (stará Nejasnost 1,57 proti 1,27 je tím vyřešená, bez zvláštního ladění).
- **Úprava podle měření bez analyzátoru (autor schválil):** žádost při čtení **63 ns** místo 72, ať emulátor
  odpovídá stroji bez měřicí karty. V modelu dává smyčka čtení při 4 MHz pro 60 / 62 / 63 / 64 / 65 / 66–88 ns
  22,39 / 22,44 / 22,50 / 22,53 / 22,63 / 22,76 T na bajt, při 2 MHz v celém rozsahu 21,27, zápis se nemění.
  Emulátor teď: ZRDBENCH 4 MHz 23,61 / 22,50, 2 MHz 22,76 / 21,27; proti záznamům s analyzátorem (`cga1v.asm`,
  `cga1v_long.asm`) zápisy a samostatné přístupy ±10 taktů, čtení 256 B −67, 12 KB −3115 taktů (−0,25 T na bajt,
  to je vliv analyzátoru).
- **Rozdíl proti 8. 10.:** ZRDBENCH tehdy dal čtení při 4 MHz 22,44 (v modelu 62 ns). Sestava byla jiná: ZRD-1V
  místo ZRMD-1, jiné pořadí karet (autor kvůli kartě s připojením analyzátoru posunul RAM-1V, CGA-1V a MPH-1V).
  Čtení je okrajové (rozhoduje pár ns kolem hrany QA), zápis i 2 MHz se tehdy i dnes shodují. Jiné karty v rámu
  ho tedy snadno posunou v rozmezí asi 22,4–22,8 T na bajt.
- **Testy:** `test_sapi1v` (`cga_waits`, `zrdbench`) hlídají dnešní hodnoty s tolerancí ±0,1 T.

### 2026-10-11 – ZRMD-1: /RES nuluje jen latch 63h (A16–A21)

- **Měření** (`zrmd.asm`, `zrmd_run.py`, 2026-10-11, dvakrát se stejným výsledkem): fáze A přečte po
  4 bajtech z 8 adres (každý z registrů 61h, 62h, 63h nula nebo 10h / 41h / 03h; obsah disku sedí s obrazem
  v `media/zrmd-1`), nastaví adresu 03:41:10 bez čtení; po druhém resetu čte fáze B z 60h bez nastavení adresy
  a dostane data z **00:41:10**: A16–A21 vynulované, A8–A15 a čítače A0–A7 zůstaly. Na disk se nezapisovalo.
- **Emulátor do změny:** reset nuloval všechny tři (předpoklad v Nejasnostech).
- **Změna (autor schválil):** `Zrmd1::reset` nuluje jen `a_hi` (latch 63h, s ním MAP1/MAP2 přes JP2).
- **Test:** `test_sapi1v` (`zrmd1_reset`).

### 2026-10-11 – JPR-1V: čtení, na které nikdo neodpoví, vrací poslední bajt datové sběrnice CPU

- **Měření** (`openbus.asm`, `openbus_run.py`, hodnoty kanálem PicoROM): IN A,(9Ah) / (A0h) / (C5h) → 9Ah, A0h,
  C5h (operand); IN A,(C) → 78h, IN B,(C) → 40h (druhý bajt instrukce); LD A,(C000h) a (C123h) v díře RAM-1V
  (MAP1 = H, MAP2 = L) → C0h, C1h (horní bajt adresy); LD A,(HL) → 7Eh (kód instrukce); IN A,(9Ah) hned po čtení
  5Ah z RAM → 9Ah. Čtení nedekódovaných 61h–64h, 6Fh u ZRMD-1 totéž (číslo portu). Platí i pro bajty z ROM na
  JPR-1V, která jde mimo sběrnici: hodnota drží na datových vodičích procesoru.
- **Emulátor do změny:** FFh.
- **Změna (autor schválil):** `Bus::open_value` (výchozí FFh) vrátí čtení paměti a I/O, na které žádná karta
  neodpoví. JPR-1V si pamatuje poslední bajt datové sběrnice (výběr instrukce, čtení, zápis, INTA) a před každým
  čtením ze sběrnice ho tam nastaví. INTA bez karty, která dá vektor, zůstává FFh (neměřeno). Ostatní procesorové
  karty FFh (Nejasnost v `docs/desky/JPR-1ZI.md`). Autor potvrdil i ze softwaru: CP/M s ovladačem IDE-1 bez
  karty dává disky „XYXY…“ (porty 58h, 59h).
- **Test:** `test_sapi1v` (`open_bus`: 9Ah, 78h, C1h, 7Eh jako deska); `openbus_run.py`: deska a emulátor
  stejně ve všech 10 čteních.
- **Dopad:** ECMON na sestavách V volbou 0–2 (FDC8, RPD8, MFC8) bez příslušného řadiče visí v BIOSu na stavu
  řadiče (`IN A,(3Ch)` nebo `IN A,(0Ch)` vrací číslo portu), jako by to bylo na desce. Na všech pěti sestavách
  s JPR-1V naběhne jen volba 3 (HDD), i bez IDE-1. Testy a návody (`prvni-kroky.md`, `build.md`, `ECMON.md`)
  přepnuté na 3.

### 2026-10-11 – MPH-1V: YM3812 bere zápisy až 35,8 µs po resetu

- **Měření** (`ymrst.asm`, `ymrst_run.py`, 4 MHz): po prodlevě od konce /RES zapíše časovač T1 = FFh a spustí
  ho, za 1 ms přečte stav. První zápis 12,8 / 24,0 / 30,8 µs po konci /RES: příznak T1 se nenastaví (stav 00h,
  zápisy ztracené); 37,3 / 40,5 / 43,8 µs a dál: nastaví se (C0h). Práh leží mezi 31 a 37 µs po konci /RES (impuls
  /RES 6,2 µs), schéma: /RST drží 74LS164 8 taktů 14,31818 MHz / 64 = 35,8 µs.
- **Emulátor do změny:** YM bral zápisy hned.
- **Změna (autor schválil):** MPH-1V ignoruje zápisy do YM 35,8 µs po /RES (`ym_rst_until`, mimo uložený stav).
- **Test:** `test_mph1v` (`ym_reset_delay`: zápis 25 µs po resetu se ztratí, 40 µs projde).

### 2026-10-11 – MPH-1V: stav YM3812 má dolní bity 0

- **Měření** (`ymstat.asm`): 32 čtení stavu před spuštěním časovače 00h, po přetečení T1 C0h, dolní bity vždy
  00000 (`INIR`). Jednotlivé `IN A,(56h)` dvakrát z 11 dalo C4h (bit 2): zákmit? Prázdná sběrnice by dala
  u `IN A,(56h)` 10110, YM tedy dolní bity budí.
- **Emulátor do změny:** dolní bity 110 („jako skutečný YM3812, detekce AdLib“).
- **Změna (autor schválil):** `Ym3812::status` bez dolních bitů. Občasný bit 2 jen v Nejasnostech MPH-1V.
- **Test:** `test_mph1v` (`adlib_detect`: přesně 00h a C0h).

### 2026-10-11 – JPR-1V + CGA-1V / AND-1V: přerušení s PC v okně karty zablokuje sběrnici (zamrzání FXS4)

- **Měření** (`freeze.asm`, `freeze_run.py`, 4 MHz, deska autora bez Liborovy záplaty IC15): smyčka `JR $`
  ve videopaměti CGA-1V na C100h (MAP1 = MAP2 = H), IM 1, přerušení od přijímače DSM-1V. Od výběru `JR`:
  /INT0 klesne v +0,54 µs; INTA začíná v +3,55 µs a **/M1 už se nezvedne**; STSTB v +4,16 µs (adresa = PC
  v okně CGA); **RDY klesne v +4,26 µs a zůstane v L**, /MR, /MW, /IOR ani okno /CPU už nepřijdou; CLK běží.
  Obsluha na 38h neproběhla (na lince nic).
- **AND-1V stejně** (`freeze_and.asm`: smyčka ve videopaměti na F900h, MAP1 = L): /INT0 v 12,3896 ms, INTA
  (/M1 trvale v L) v 12,3902 ms, STSTB a **RDY trvale v L** v 12,3909 ms, žádné /MR ani /IOR. Čekání AND-1V ruší
  jen /MR, /MW, /IOR, /IOW a /RES; INTA nedá nic z toho.
- **RESET:** `picorom reset low/z` (/RTL) během zaseknutí nevyvolal žádný impulz /RES (1,5 s záznamu: žádný
  /M1, RDY trvale v L), u CGA-1V i AND-1V. Reset je synchronizovaný s /M1. Pomůže jen vypnutí (autor po každém
  pokusu vypnul a zapnul).
- **Emulátor do změny:** obsluhu provedl („NOFREEZE“), výběr karty při INTA nedělal, reset fungoval vždy.
- **Změna (autor schválil):** `Card::inta_holds_rdy` (CGA-1V, AND-1V a DGD-1V se zapnutými čekacími stavy, adresa
  v okně podle MAP). JPR-1V při INTA (`ststb_inta`) zjistí, jestli některá karta RDY zablokuje; pak CPU zůstane
  v cyklu INTA (`hung`, čas i ostatní karty běží), log to vysvětlí. `CpuCard::reset_blocked`: `Machine::reset`
  (tlačítko, GUI, MCP) pak nic neudělá, pomůže vypnout a zapnout. Mimo uložený stav. JPR-1Z (Libor: stejně)
  a Liborova záplata IC15 zatím ne.
- **Test:** `test_sapi1v` (`fxs4_freeze`: s čekáním CGA-1V zamrzne a RESET nepomůže, bez čekání obsluha proběhne);
  `emu_freeze.py`: `freeze` i `freeze_and` v emulátoru zamrznou jako na desce.

## Ověřeno beze změny

- **Boot ROM JPR-1V neodpovídá na I/O čtení** (`iorom.asm`, `iorom_run.py`, 2026-10-13): /CE EEPROM vybírá jen
  adresa (IC12), ale `IN A,(C)` i `IN A,(n)` s horním bajtem 00h–03h (okno ROM) čtou totéž jako s 08h, 10h, 80h,
  FFh: port 56h (YM3812) 00h, port 9Ah (nikdo) poslední bajt na sběrnici (78h, 9Ah). Na sběrnici se ROM při I/O
  nedostane, emulátor se tak chová.
- **Stav YM3812 na MPH-1V v ustáleném stavu** (`ymstat2.asm`, `ymstat2_run.py`, 2026-10-13): 16 384 jednotlivých
  `IN A,(56h)` s náhodnými mezerami při stojícím časovači 100 % 00h, po přetečení T1 100 % C0h, 16 384 čtení
  bloky INI také 100 % C0h. Občasné C4h z 11. 10. (`ymrst`) se nezopakovalo. **Za resetu YM** (`ymres.asm`,
  `ymres_run.py`, čtení po 4–10 µs od RESET, třikrát) čte stav 10h až do 22–35 µs, pak 00h (konec /RST
  z 74LS164); jednou 38h (`iorom`). Emulátor vrací i za resetu 00h; nikdo stav YM v prvních 36 µs nečte, beze
  změny.
- **Takt sběrnice Φ2 na JPR-1V** (`busclk.py`, sonda na DKS-1, 2026-10-13): při TURBO CPU 4,000 MHz a Φ2
  2,000 MHz, střída 50 %, obě hrany 0–8 ns po sestupné hraně CLK CPU (Johnsonův čítač, jak píše schéma).

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
  Čtení z CGA-1V je okrajové, o okně rozhoduje pár ns; i samotný analyzátor ho posune (bez něj 22,52, viz
  Měřicí pracoviště).
- **Čekání AND-1V při 2 MHz** (TURBO vypnuté): LDIR 256 B do karty 5873, z karty 5562 taktů, emulátor totéž;
  samostatné přístupy −1 / 0. Při 4 i 2 MHz tedy model AND-1V sedí.
- **VBI CGA-1V proti VSYNC** (`vbi.asm`, `vbi_run.py`, D14 na VSYNC, VGA vývod 14): VSYNC 59,940 Hz, 525 řádků
  po 31,778 µs, záporný impuls 2 řádky. STATUS D7 (VBI) naskočí 49,6–49,8 řádku (plus nejvýš 0,24 řádku latence
  čtecí smyčky), tedy 50 řádků před VSYNC: obraz 400 řádků je uprostřed (VGA řádky 40–439), VBI na začátku
  řádku 440. Od VBI do dalšího obrazu 125 řádků, emulátor stejně (VBI na řádku 400, snímek 525).
- **ZRD-1V rev 1.1** (autor kartu dal místo ZRMD-1; `zrmd.asm`, `zrdrd.asm`, jen čtení): **/RES adresu nemaže**
  (po resetu čtení z 03:41:10 jako před ním), čtení 61h, 62h a 63h vrátí **FFh** a FFh zapíše do registru adresy
  (pak 03:41:FF, 03:FF:10 a 3F:41:10), 64h a 6Fh nedekódované (prázdná sběrnice: číslo portu). Přesně jako
  v emulátoru podle schématu.
- **Port 63h → MAP1, MAP2:** OUT 40h → MAP1, 80h → MAP2, C0h → oba, 00h → nic, aktivní v H. Shoduje se
  s nastavením RAM-1V v sestavě (bit 6 → MAP1, bit 7 → MAP2). Na 63h ale odpovídá i ZRMD-1, kdo čáry budí, se
  z tohoto měření nedá rozlišit (otázka JP2 u ZRMD-1 zůstává).
- **AND-1V čeká při čtení videopaměti:** 16 čtení F800h, 16 impulzů RDY (délky se zatím nevyhodnocovaly).
- **Časování CPU:** zpožďovací smyčka 65 536 × 24 taktů trvá 393,5 ms jako při 4,000 MHz bez čekání; emulátor
  dává totéž.

## Podezření k dořešení

- **Co čte INTA, když vektor nikdo nedává** (`intavec*.asm`, IM 2 s tabulkou, kde každý bajt vede na vlastní
  obsluhu; JPR-1V, 4 MHz): přerušení od DSM-1V po smyčce NOP 00h, `LD A,(9876h)` (5Ah) 5Ah, `LD (9876h),A`
  (A5h) A5h, `INC BC` 03h: poslední bajt na sběrnici, jako u ostatních prázdných čtení. Za instrukcí s delší
  vnitřní částí bajt odplyne k FFh: `JR $` (5 vnitřních taktů) FFh, `ADD HL,BC` (7) FDh, FFh, F9h. Přerušení
  z YM3812 přišlo hned po `EI` a dalo B7h (`OR A`, další instrukce). Nevysvětlené: smyčka `LD A,I` dává vždy D6h
  (posunutí `JR` na jejím konci), jako by se přerušení po `LD A,I` nepřijímalo. Emulátor vrací při INTA FFh.
  Bajt zřejmě drží kapacita sběrnice asi 0,5 µs a během 1–2 µs se rozplyne, je to analogové a závisí na kartách.
  Přesnější odpověď dá jen záznam datové sběrnice analyzátorem (D0–D7). Do té doby beze změny.

