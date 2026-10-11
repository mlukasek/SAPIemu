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
  D10 MAP1, D11 MAP2, D12 CLK (Z80 vývod 6), D13 /M1 (Z80 vývod 27), D14 VSYNC CGA-1V (zatím nezapojeno).
  Podezřelé hrany se kontrolují i analogově.
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

## Ověřeno beze změny

- **JPR-1V nedává /MR při čtení ROM** (0000h–07FFh): při kontrole zapojení byl /MR jen u čtení RAM a AND-1V.
  Emulátor to tak dělá (`docs/desky/JPR-1V.md`).
- **Port 63h → MAP1, MAP2:** OUT 40h → MAP1, 80h → MAP2, C0h → oba, 00h → nic, aktivní v H. Shoduje se
  s nastavením RAM-1V v sestavě (bit 6 → MAP1, bit 7 → MAP2). Na 63h ale odpovídá i ZRMD-1, kdo čáry budí, se
  z tohoto měření nedá rozlišit (otázka JP2 u ZRMD-1 zůstává).
- **AND-1V čeká při čtení videopaměti:** 16 čtení F800h, 16 impulzů RDY (délky se zatím nevyhodnocovaly).
- **Časování CPU:** zpožďovací smyčka 65 536 × 24 taktů trvá 393,5 ms jako při 4,000 MHz bez čekání; emulátor
  dává totéž.

## Návrhy čekající na schválení

### DSM-1V: UART převezme znak do vysílacího registru až na hraně taktu 16× rychlosti

- **Měření** (`tbre.asm`, `tbre_run.py`, 2026-10-11, 9600 Bd, takt 16× = 153,6 kHz, perioda 6,51 µs):
  1. jeden zápis dat (/IOW 0,62 µs): start bit 0,9 µs po začátku zápisu, stav potom 7Fh;
  2. dva zápisy 2,7 µs po sobě: **na lince jen jeden znak**, start bit 6,4 µs po prvním zápisu, stav celých
     336 µs po zápisech 7Fh (TBRE = 1, žádný znak nečeká);
  3. začátky obou odeslaných znaků jsou od sebe 3,1835 ms = **přesně 489 period taktu 16×**: vysílání začíná
     vždy na hraně taktu.
- **Výklad:** zápis naplní vyrovnávací registr (TBRE = 0), do vysílacího registru se znak přesune až na nejbližší
  hraně taktu 16×. Druhý zápis před touto hranou první znak přepíše. Dřívější „XY“ (`diag2.asm`, zápisy 4,5 µs
  od sebe) prošlo, protože hrana mezi zápisy náhodou byla. Prodloužení /IOW to nevysvětluje (zápis trvá 0,62 µs).
- **Emulátor dnes:** znak převezme hned, druhý čeká ve vyrovnávacím registru (TBRE = 0 asi 1 ms) a odejdou oba
  (stejná ROM: „55 55 55“ proti „55 55“ na desce).
- **Návrh:** v modelu UARTu převzetí do vysílacího registru na hraně taktu 16× (mřížka od zapnutí, fázi skutečné
  desky neznáme), start bit od té hrany. Zápis rychleji než jedna perioda taktu (6,5 µs při 9600 Bd, 0,54 µs při
  115200 Bd) pak přepíše předchozí znak jako na desce. Zatím jen DSM-1V (6402). MHB1012 na DSM-1 a DMR-1 je
  nejspíš stejný (odvozený od AY-5-1013), ale neměřený.

## Podezření k dořešení

- Příjem (DR, přetečení) na DSM-1V zatím neměřen.
