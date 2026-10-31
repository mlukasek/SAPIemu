# DSE-1: návod k obsluze simulátoru a programátoru EPROM

Deska **DSE-1** (Tesla, popsaná v AR B 6/1985) udělá ze SAPI-1 vývojový systém pro programy v EPROM. Má dvě části:
- **programátor EPROM 2716 a 2732:** kabel X2 s paticí, do které se zasune EPROM;
- **simulátor EPROM:** 4 KB RAM na desce. Kabel X3 se sondou se zasune do patice cílového zařízení místo EPROM
  a zařízení pak čte program z RAM desky. Program jde opravovat bez mazání a vypalování.

Ovládá ji program **DSE** od Ing. Petra Navrátila (Tesla Elstroj, „NAP051186“). Je v EPROM od 1000h a potřebuje
MONITOR V4. Všechny kroky jsou vyzkoušené v emulátoru (2026-10-30), výpisy obrazovky jsou opsané z něj. Jak deska
funguje a co je v emulaci odhad, je v `docs/desky/DSE-1.md`. Pod CP/M desku ovládá **EPROM.COM J. Bíby**, který umí
i EPROM 2708 (kapitola 11), a **DSE.COM J. Hlaváče** pro ZPS-3 (kapitola 12).

![Program DSE programuje prázdnou 2716, vpravo okno Programátor EPROM DSE-1: svítí PROG, Vpp 25 V, pulz 50 ms](img/navod-dse1.png)

## 1. Zapnutí a nabídka

1. Soubor → Vybrat sestavu… → Přístroje a technologie → **Programátor EPROM DSE-1**
   (`zps/ZPS-2_DSE-1.sapi`). Naběhne MIKOS a vedle obrazovky se otevře okno **Programátor EPROM DSE-1**:

   ```
   MIKOS-1 V4.0F +
   SAPI-1 V4.0
   .
   ```
2. Za tečku napište `G1000` a Enter. Program DSE vypíše nabídku:

   ```
        ******  D S E  ******

   ______________________________NAP051186


           NABIZIM TYTO VARIANTY:

   T ... TRANSFER  OBJIMKA --> RAM
   P ... PROGRAM   RAM --> OBJIMKA
   D ... DUMP      VYPIS RAM + HLAVICKA
   S ... SIMULATE  RAM --> DSE
   R ... RECALL    DSE --> RAM
   C ... COMPARE   RAM X OBJIMKA
   A ... ALTERNATE ZMENA RAM
   M ... MENU      ZOBRAZENI NABIDKY
   F ... FILL      KONSTANTA DO USEKU RAM
   E ... EXIT      NAVRAT DO MONITORU
   Z ... ZKOUSKA DESKY
   K ... KONTROLA EPROM


          STISKNI KLAVESU:
   ```

**Jak se ovládá:**
- Příkaz je jedno písmeno, **bez Enteru**.
- Adresy se píšou šestnáctkově a potvrzují Enterem (`0`, `7FF`, `5000`). „OBJIMKA“ je patice programátoru.
- Na `VLOZ EPROM` stačí libovolná klávesa (EPROM už v patici je).
- Na `2732? A=ANO` znamená `A` EPROM 2732, jiná klávesa (třeba `N`) 2716.
- Po `OK` a `STISKNI KLAVESU:` se píše další příkaz. `M` vypíše nabídku znovu.

## 2. Okno Programátor EPROM DSE-1

Okno ukazuje shora krabičku na konci programovacího kabelu X2: zelenou patici s pákou (patice s nulovou zasouvací
silou, podobná 3M TEXTOOL), EPROM v ní a dvě LED. Vpravo stojí UV lampa na mazání. Jak krabička Tesla vypadala,
nevíme (fotka chybí), kresba je volná. Pod kresbou jsou tlačítka a stav desky.

| Prvek | Význam |
|---|---|
| Patice s pákou | Prázdná patice má páku zvednutou, s EPROM je páka sklopená. Páka se neovládá, EPROM se vkládá tlačítky pod kresbou. Bublina nad paticí řekne, jak je kabel zapojený (pro 2716, pro 2732, dvě patice, pro 2708). |
| EPROM | Keramické pouzdro s křemenným okénkem: 2716 má potisk TESLA 2716, 2708 TESLA 2708 (Tesla vyráběla MHB2708C), 2732 Intel 2732 (Tesla 2732 nevyráběla), všechny jen ilustrační. Prázdná EPROM (všechny bajty FFh) ukáže okénkem čip. Naprogramovaná má přes okénko papírový štítek s názvem souboru, jak se lepil na vypálené EPROM. Štítek se přilepí, až programování skončí; během něj je okénko vidět. |
| LED PROG (červená), MAP (zelená) | LED1 „indikace progr.“ a LED2 „indikace map.“ v krabičce kabelu. Program DSE rozsvítí PROG při programování. Barvy vybral autor, skutečné nejsou známé. |
| UV lampa (na podstavci „UV · SMAZAT“) | Smaže EPROM v patici (kapitola 4). Pohodlí emulátoru: skutečná EPROM jde z patice pod UV lampu na 15–20 minut. Vypnutá lampa nemá paprsky, pod myší ukáže slabé (jde na ni kliknout). |
| Karta | Jen v sestavě s víc deskami DSE-1: vybere desku, jejíž kabel okno ukazuje (i pro soubor přetažený na okno). |
| EPROM v patici | Soubor EPROM v patici a její typ (2708, 2716, 2732). Žlutě upozornění, když typ nesedí se zapojením kabelu, a pomůcky emulátoru (kapitoly 9 a 11). |
| Vložit… | Vloží jinou EPROM: soubor `.bin` s 1024 B (2708), 2048 B (2716) nebo 4096 B (2732). Soubor jde také přetáhnout myší na okno. |
| Nová 2708, Nová 2716, Nová 2732 | Vloží novou prázdnou EPROM (všechny bity 1, jako po smazání UV světlem) `work\dse-1\eprom_N.bin`. Prázdný soubor téže velikosti, který není v patici, se použije znovu, nový vznikne, jen když žádný není. |
| Vysunout | EPROM vytáhne z patice. |
| … → Obnovit z originálu | Pracovní kopii přepíše originálem. |
| Uložit RAM simulátoru… | 4 KB RAM simulátoru do souboru `.bin`. Pomoc emulátoru, skutečná deska to neumí. |
| Vpp 0 / 5 / 25 V | Napětí na vývodu Vpp patice: 5 V pro čtení, 25 V (červeně) pro programování. |
| ON LINE / OFF LINE | ON LINE: RAM simulátoru patří sondě X3. OFF LINE: zapisuje a čte ji počítač. |
| Poslední programovací pulz | Délka posledního pulzu v ms. Během programování „pulz běží“ a počet pulzů. |

Vpp, ON LINE a délka pulzu jsou údaje emulátoru, krabička sama má jen dvě LED.

V sestavě je prázdná 2716. Emulátor pracuje s její pracovní kopií `work\dse-1\eprom_2716.bin`, originál
v `media\dse-1` se nemění. Naprogramované bajty se do kopie ukládají průběžně, jako u ostatních médií. Štítek
naprogramované EPROM nese jméno souboru (`eprom_2716`, u Nové 2716 `eprom_1` a dál).

## 3. Kontrola smazání: K

Před programováním se vyplatí ověřit, že je EPROM prázdná (všechny bajty FFh):

1. `K`, na `VLOZ EPROM` libovolná klávesa, na `2732? A=ANO` klávesa `N`.
2. `DOLNI ADRESA EPROM=` `0` a Enter, `HORNI ADRESA EPROM=` `7FF` a Enter (celá 2716).

```
       STISKNI KLAVESU:K
VLOZ EPROM
2732? A=ANO N
DOLNI ADRESA EPROM=0

HORNI ADRESA EPROM=7FF

CISTA  0000 - 07FF


OK
```

U naprogramované EPROM vypíše program každý bajt, který není FFh (adresa a obsah), a na konci `NECISTA`.

## 4. Data do RAM a programování: F, D, P

Program DSE programuje z RAM počítače. Na zkoušku naplníme 256 bajtů hodnotou 55h:

1. `F` a `5000,50FF,55` a Enter (od, do, obsah):

   ```
          STISKNI KLAVESU:F

   OD:  DO:  OBSAH:
   =5000,=50FF,=55


   OK
   ```
2. `D` a `5000,501F` a Enter vypíše kus RAM:

   ```
          *  MEMORY DUMP  *
   _______________________________DSE-NAP

   OD:  DO:
   =5000,=501F

   5000 55 55 55 55 55 55 55 55  UUUUUUUU
   5008 55 55 55 55 55 55 55 55  UUUUUUUU
   5010 55 55 55 55 55 55 55 55  UUUUUUUU
   5018 55 55 55 55 55 55 55 55  UUUUUUUU

   OK
   ```
3. `P`, `DOLNI ADRESA RAM=` `5000` Enter, `HORNI ADRESA RAM=` `50FF` Enter, `DOLNI ADRESA EPROM=` `0` Enter,
   libovolná klávesa a `N`. Program vypíše `PROGRAMUJI`:
   - v okně programátoru svítí červená **PROG**, Vpp je **25 V**, poslední pulz **49.97 ms** (obrázek nahoře);
   - každý bajt dostane jeden pulz 50 ms, 256 bajtů tedy trvá asi 13 s;
   - okénkem EPROM je během programování vidět čip;
   - pak program celý rozsah porovná a na okénko se přilepí štítek se jménem souboru (EPROM už není prázdná):

   ```
          STISKNI KLAVESU:P
   DOLNI ADRESA RAM=5000

   HORNI ADRESA RAM=50FF

   DOLNI ADRESA EPROM=0

   VLOZ EPROM
   2732? A=ANO N

            PROGRAMUJI

   SHODNY OBSAH  0000 - 00FF


   OK
   ```

V okně Log (Ladění → Log) je shrnutí programování:
`DSE-1: programming: 256 bytes, 1.00 pulses per byte, pulses 49.97-49.97 ms`.

- **Celá 2716** (2048 bajtů) trvá asi 100 s. Čekání zkrátí F8 (Stroj → Rychlost emulace → Maximum). Délka pulzu se
  měří v čase emulovaného počítače, takže zůstane 50 ms.
- **Programování mění bity jen z 1 na 0.** Do naprogramovaného bajtu jde dopsat jen to, co z něj ubírá jedničky.
  Jinak je potřeba EPROM smazat (UV lampa níže) nebo vzít novou.

**Smazání UV lampou:** klik na lampu „UV · SMAZAT“ vpravo v okně programátoru. Naprogramovaná EPROM se nejdřív zeptá
(celý obsah se ztratí), prázdná se maže hned. Všechny bajty jsou hned FFh, štítek zmizí a okénko asi 1,5 s svítí
fialově. Smazaný obsah se uloží do souboru EPROM jako programování (u EPROM z `media\` nebo `roms\` do pracovní kopie,
originál zůstane a „…“ → Obnovit z originálu ho vrátí). `K` pak vypíše `CISTA`. Skutečná EPROM by šla z patice pod UV lampu na
15–20 minut, v emulátoru je to pohodlí. Když je na patici napětí (Vpp 5 nebo 25 V, program právě čte nebo programuje),
lampa nesvítí a okno řekne, ať program nejdřív skončí.

## 5. Porovnání a změna RAM: C, A

`C` porovná EPROM s RAM: `DOLNI ADRESA EPROM=` `0`, `HORNI ADRESA EPROM=` `FF`, `DOLNI ADRESA RAM=` `5000`,
klávesa a `N`. Vypíše `SHODNY OBSAH  0000 - 00FF`.

Teď změníme v RAM dva bajty. `A` a `5010` a mezerník: program ukáže obsah `55-`. Napište `12` a mezerník (další
bajt), `34` a Enter (konec):

```
       STISKNI KLAVESU:A
OD: =5010 55-12 55-34
OK
```

Znovu `C` se stejnými adresami vypíše rozdíly (adresa a bajt v RAM, adresa a bajt v EPROM):

```
2732? A=ANO N
5010 12   0010 55
5011 34   0011 55
ROZDILNY OBSAH  0000 - 00FF


OK
```

## 6. EPROM do RAM: T

`T` přenese obsah EPROM do RAM: `DOLNI ADRESA EPROM=` `0`, `HORNI ADRESA EPROM=` `FF`, `DOLNI ADRESA RAM=`
`6000`, klávesa a `N`. Každý bajt hned zkontroluje a vypíše jen `OK`. `D` a `6000,601F` ukáže samé 55h.

Takhle se načte cizí EPROM: vložit ji do patice (Vložit… v okně programátoru, nebo soubor přetáhnout na okno), `T`
celý rozsah (`0`, `7FF`) do RAM, v RAM upravit a naprogramovat do nové EPROM (Nová 2716).

## 7. Simulátor: S a R

`S` zkopíruje 2 KB z RAM počítače do RAM simulátoru a přepne desku do **ON LINE**:

1. `S`, `DOLNI ADRESA RAM=` `5000` a Enter.
2. Na `EPROM 4K- A=ANO` klávesa `N` (simulovat 2716).

```
       STISKNI KLAVESU:S
DOLNI ADRESA RAM=5000

EPROM 4K- A=ANO  N

OK
```

- Okno programátoru ukáže **ON LINE**. Na skutečné desce by teď zařízení se sondou X3 v patici četlo místo EPROM
  obsah 5000–57FF. **V emulátoru sonda není nikam zasunutá**, ON LINE je jen vidět.
- 2716 leží v RAM simulátoru na **800–FFFh** (tak je zapojený kabel X3 z výroby), 2732 (`A`) na 000–FFFh.
- Obsah RAM simulátoru uloží do souboru tlačítko **Uložit RAM simulátoru…** v okně programátoru. Soubor má 4 KB,
  2716 je v něm od 800h. Jde ho vložit do jiné sestavy jako obsah EPROM (pomoc emulátoru).

`R` přenese RAM simulátoru zpět do RAM počítače: `DOLNI ADRESA DSE=` `800`, `HORNI ADRESA DSE=` `8FF`,
`DOLNI ADRESA RAM=` `7000`. Program každý bajt zkontroluje:

```
       STISKNI KLAVESU:R
DOLNI ADRESA DSE=800

HORNI ADRESA DSE=8FF

DOLNI ADRESA RAM=7000

SHODNY OBSAH  0800 - 08FF


OK
```

`D` a `7000,701F` ukáže data z kroku 5 i se změněnými bajty `12 34` na 7010h.

Původní návod DSE.DOC radí mít při `S` a `R` patici programátoru prázdnou. V emulátoru `S` a `R` projdou i s EPROM
v patici, do logu ale jde jednou varování („…DSE.DOC asks for an empty socket during S and R…“). Skutečná deska by
mohla číst špatně (`docs/desky/DSE-1.md`, Nejasnost 3). Prázdnou patici dá Vysunout v okně programátoru (páka se
zvedne).

## 8. Zkouška desky a návrat: Z, E

- `Z` vypíše `TEST DESKY`, zkopíruje ROM počítače 0000–0FFF do RAM simulátoru a zkontroluje ji. Vypíše jen `OK`.
  RAM simulátoru se tím přepíše.
- `E` se vrátí do monitoru (`SAPI-1 V4.0` a tečka). Program DSE zase spustí `G1000`.

## 9. EPROM 2732

**Kabel X2 se dodával zapojený pro 2716.** Pro 2732 se v jeho konektoru přepojovaly tři vodiče (DSE.DOC). V emulátoru:

1. Zobrazení → Konfigurátor sestavy…, karta DSE-1 ve slotu X6 (s 2732 v patici rovnou tlačítko **Změnit kabel X2…**
   v okně programátoru), **Programovací kabel X2** → „pro 2732, upravený
   podle DSE.DOC“. Pohodlnější je „dvě patice 2716 + 2732 (replika)“: EPROM si podle typu vybere svou patici.
2. **Restartovat s touto sestavou.** Obsah RAM se ztratí, pracovní kopie EPROM zůstanou.
3. V okně programátoru **Nová 2732** (na pouzdře je Intel 2732).
4. `G1000`, `K`, klávesa, `A` (2732), `0` a `FFF`:

   ```
   2732? A=ANO A
   DOLNI ADRESA EPROM=0

   HORNI ADRESA EPROM=FFF

   CISTA  0000 - 0FFF
   ```
5. `F` `5000,50FF,AA`, pak `P` z `5000`–`50FF` do EPROM od `900`, klávesa a `A`:

   ```
   DOLNI ADRESA EPROM=900

   VLOZ EPROM
   2732? A=ANO A

            PROGRAMUJI

   SHODNY OBSAH  0900 - 09FF
   ```

**Na otázku `2732?` odpovězte A.** S odpovědí N program 2732 naprogramuje (v kabelu pro 2732 jde PC6 na /CE a 25 V
na OE/Vpp), ale kontrola čte s 5 V na OE/Vpp: výstupy 2732 jsou vypnuté, čte se FFh a vyjde `ROZDILNY OBSAH`, i když
data v EPROM jsou. Okno programátoru pak žlutě upozorní „2732 se čte jako 2716: na otázku „2732?“ odpovězte A“
(pomůcka emulátoru, skutečná krabička nic neřekne).

V logu je u 2732 navíc `1 pulse shorter than 1 ms did not program`. To je krátký pulz s daty FFh těsně před
programováním (program Tesla zapne 25 V dřív než řídicí slovo), nic nezmění.

**Pozor na 2732 v kabelu pro 2716.** Okno programátoru upozorní žlutě „2732 v patici zapojené pro 2716“, bublina
vysvětlí proč a tlačítko **Změnit kabel X2…** vedle otevře konfigurátor na kartě DSE-1. Taková EPROM se nečte ani
neprogramuje, jak má. `K` sice vypíše `CISTA`, ale `P` nic nenaprogramuje a kontrola vypíše samé rozdíly
a `ROZDILNY OBSAH`. Na vývod A11 přitom jde 25 V. Log to hlásí („25 V on A11 (pin 21) of the 2732…“), skutečnou EPROM
by to mohlo poškodit.

## 10. Vlastní program do EPROM

1. Program do RAM počítače: Soubor → Nahrát program do paměti (Intel HEX)…, nebo Nahrát binární soubor do
   paměti…. Data patří od 4400h výš, jako programy pro MIKOS: pod tím jsou proměnné monitoru a programu DSE (4100h).
2. `G1000`, `K` (prázdná?), `P` s adresami programu, `C` pro kontrolu.
3. Soubor EPROM je pracovní kopie ve `work\dse-1\` (název je v okně programátoru a na štítku EPROM). Je to
   obyčejný obraz 1, 2 nebo 4 KB, jde použít jinde, třeba jako EPROM jiné karty.

MCP nabízí totéž bez okna: nástroj `dse1` (stav, `insert`, `new` 2708, 2716 nebo 2732, `eject`, `erase`, `sim_read`,
`sim_save`; `docs/mcp.md`).

## 11. EPROM 2708: EPROM.COM J. Bíby

Tesla 2708 na DSE-1 neprogramovala: návod desky i program DSE znají jen 2716 a 2732. Umí to **EPROM.COM J. Bíby**
(verze V130786, CP/M se Z80) jako typ `0`, s adaptérem, který 2708 dodá +12 V a −5 V a na vývod 18 přivede
programovací napětí. Žádný popis adaptéru se nedochoval, emulátor ho má zrekonstruovaný z toho, co EPROM.COM dělá
(`docs/desky/DSE-1.md`, oddíl Programovací kabel X2). EPROM.COM je na služební disketě J. Bíby. Vyzkoušeno
v emulátoru 2026-10-31, výpisy jsou opsané z něj.

1. Soubor → Vybrat sestavu… → Ostatní → CP/M J. Bíby (SAPIMAN) → **CP/M 2.2 z 8" diskety: RPD-1, BIOS V271111JB**
   (`cpm_boot_rpd1.sapi`).
2. Zobrazení → Konfigurátor sestavy…, klik na volný slot **X7**, karta **DSE-1**. U ní:
   - **JP1** rozpojit (zrušit zaškrtnutí). EPROM.COM svítí bitem D5 na LED a s JP1 by signál MAP1 pod CP/M
     přepnul paměť DMR-1;
   - **Programovací kabel X2** → „pro 2708, adaptér podle EPROM.COM J. Bíby“;
   - **EPROM v patici X2** → „Prázdná 2708“ (nebo později v okně programátoru **Nová 2708**).

   Pak **Restartovat s touto sestavou**. Uložit zapíše vaši kopii sestavy (Moje sestavy).
3. Zobrazení → Média, mechanika **B:** → Vložit… → `media\sapiman\knihovna\sysprog_a_sluzebni_CPM.img`.
4. `B:EPROM` a Enter. EPROM.COM vypíše nabídku a `STISKNI KLÁVESU (? = nabídka):`.
5. `F` vyplní úsek RAM: `2000` Enter, `203F` Enter, `55` Enter:

   ```
       STISKNI KLÁVESU (? = nabídka): F

   Od adresy: =2000
   Do adresy: =203F
   Konstanta: =55


   OK
   ```
6. `K` zkontroluje, že je EPROM prázdná: typ `0`, `0` Enter, `3FF` Enter, `A`:

   ```
       STISKNI KLÁVESU (? = nabídka): K
   Typ paměti?  2708 ... 0
                2716 ... 1
                2732 ... 2
                2764 ... 3     0

   Dolní adresa EPROM=0

   Horní adresa EPROM=3FF

   Vlož EPROM - údaje jsou spravné ?  (A=ano)A
   ČISTÁ  0000 - 03FF
   ```
7. `P` naprogramuje RAM 2000–203F do EPROM od 0: typ `0`, `2000`, `203F`, `0` (každé s Enter), `A`. Program dělá 129
   průchodů celým úsekem a za každým kromě posledního napíše hvězdičku. 64 bajtů trvá asi 16 s:

   ```
       STISKNI KLÁVESU (? = nabídka): P
   Typ paměti?  2708 ... 0
                2716 ... 1
                2732 ... 2
                2764 ... 3     0

   Dolní adresa RAM=2000

   Horní adresa RAM=203F

   Dolní adresa EPROM=0

   Vlož EPROM - údaje jsou spravné ?  (A=ano)A

       PROGRAMUJI
   ****************************************************************
   ****************************************************************

   SHODNÝ OBSAH  0000 - 003F
   ```

Při programování svítí PROG i MAP (EPROM.COM zapíná obě LED), Vpp je 25 V jen na 0,83 ms pulzu každého bajtu, okno
proto ukazuje většinou 0 V. V logu je `DSE-1: programming: 64 bytes, 129.00 pulses per byte, pulses 0.83-0.83 ms`.
To je pravidlo z katalogů 2708: pulzy 0,1–1 ms postupně na každé slovo, N průchodů, N × pulz aspoň 100 ms (tady
106,7 ms). Celá 2708 (0–3FF) trvá asi 4 minuty času stroje.

- **Jiný typ než 0 s 2708:** program pro 2716 drží 25 V přes všechny adresy a pulzy jsou kolem 50–100 ms. Emulátor
  2708 i tak naprogramuje, ale log varuje (pulz nad 1 ms, změna adresy během pulzu, součet) a okno programátoru žlutě
  napíše „2708 dostává pulzy delší než 1 ms: EPROM.COM typ 0“. Totéž udělá program Tesla v sestavě
  `zps/ZPS-2_DSE-1` s kabelem pro 2708. Jak by to snesla skutečná MHB2708C, nevíme.
- **2708 v kabelu pro 2716 nebo 2732** nedostane +12 V a −5 V a čte FFh. Okno programátoru upozorní „2708 v patici
  zapojené pro 2716“ a log varuje, že 5 V nebo 25 V na jejím vývodu VBB by skutečný čip mohlo poškodit. Obráceně
  2716 v kabelu pro 2708 dostane +12 V na A10 a −5 V na vývod 21.
- Typ 1 (2716) a 2 (2732) EPROM.COM fungují s kabelem pro 2716 (2732), stejně jako v testu `test_dse1_eprom`
  (pulzy 98 ms na 2 MHz, log varuje mimo 45–55 ms).

## 12. DSE.COM pod CP/M (ZPS-3)

**DSE.COM** napsal J. Hlaváč (1988) pro ZPS-3 s CP/M, návod DSECPM.DOC je od TESLA ELTOS DIZ (15. 3. 1989). Pracuje
s bufferem v RAM počítače a se soubory Intel HEX, umí 2716 a 2732. Disketa `media\sapiman\knihovna\perina_dse.img`
(ze sbírky SAPIMAN, složka PERINA) má i DSE.DOC (DSE-1 pod MONITOREM V5), DSECPM.DOC (návod k DSE.COM, vypíše ho
`TYPE B:DSECPM.DOC`), výpis adresáře D.COM a souborový správce X80.COM (pro 80 sloupců, na AND-1A s 64 sloupci možná
vypadá špatně, nezkoušeno). Vyzkoušeno v emulátoru 2026-10-31, výpisy jsou opsané z něj.

1. Soubor → Vybrat sestavu… → Základ: historické sestavy Tesla → **ZPS-3 s CP/M 64K Tesla IMA** (původní CP/M
   Tesly, pro kterou DSECPM.DOC píše).
2. Zobrazení → Konfigurátor sestavy…, klik na volný slot **X8**, karta **DSE-1** (výchozí nastavení: porty F0h, kabel
   pro 2716, prázdná 2716), **Restartovat s touto sestavou**. JP1 nevadí, DSE.COM bit MAP nepoužívá.
3. Zobrazení → Média, mechanika **B:** → Vložit… → `media\sapiman\knihovna\perina_dse.img` (místo diskety 2 IMA).
   Na B: DSE.COM nepíše. **A: musí zůstat zapisovatelná:** příkazy R a W bez jednotky pracují s A:, i když DSE běží
   z B:.
4. `B:DSE` a Enter, na otázku typu `2716` a Enter:

   ```
   **************************************
   *      PROGRAMATOR PAMETI EPROM      *
   *         PRO ZPS-3 S DESKOU         *
   *                                    *
   *             D S P - 1              *
   *                                    *
   **************************************

   PRIPOJITELNE EPROM : 2716, 2732

   ZADEJ TYP EPROM (2716,2732) :
   VYBRANY TYP PAMETI :  2716
   PORADOVE CISLO ROM : 00
   PRACOVNI CAST BUFFERU : 0000 - 0000
   ```

   („D S P - 1“ je v programu chybou místo DSE-1.) Pak asi 10 s vypisuje nápovědu a čeká s `00*`.
5. **Hned po startu `E 0` a Enter.** Pracovní část bufferu je po startu `0000 - 0000` (program ji spočítá dřív, než
   nastaví délku typu), `E 0` ji nastaví na celou EPROM:

   ```
   00*E 0
   PORADOVE CISLO ROM : 00 (0000 - 07FF)
   ```
6. Data do bufferu, výpis a test, že EPROM data snese (`CISTA -`: všude FFh):

   ```
   00*F 0 3F 55
   00*F 10 1F AA
   00*D 0 3F
   0000 55 55 55 55 55 55 55 55
   0008 55 55 55 55 55 55 55 55
   0010 AA AA AA AA AA AA AA AA
   0018 AA AA AA AA AA AA AA AA
   0020 55 55 55 55 55 55 55 55
   0028 55 55 55 55 55 55 55 55
   0030 55 55 55 55 55 55 55 55
   0038 55 55 55 55 55 55 55 55
   KONTROLNI SOUCET RAM (0000 - 003F) = 90
   00*T 0 3F 0
   TESTOVANI  2716
   CISTA -
   ```
7. `P 0 3F 0` naprogramuje buffer 0–3F do EPROM od 0, na otázku `A`:

   ```
   00*P 0 3F 0
   PROGRAMOVANI  2716
   URCITE PROGRAMOVAT ? (A/N)ANO
   PROGRAMOVANI SKONCENO OK
   KONTROLNI SOUCET ROM (0000 - 003F) =90
   ```

   **Pak počítač asi 32 s pípá** (program pošle 200 znaků BEL), teprve potom se vrátí `00*`.
8. `V 0 3F 0` porovná buffer s EPROM, `W 0 3F` zapíše buffer do souboru HEX (`A:PROG` → `A:PROG.HEX`), `R` ho
   načte zpět:

   ```
   00*V 0 3F 0
   VERIFIKACE  2716
   SHODNY OBSAH
   KONTROLNI SOUCET ROM (0000 - 003F) =90
   00*W 0 3F
   ZAPIS DAT DO SOUBORU TYPU HEX
   ZADEJ JMENO SOUBORU : A:PROG
   00*R
   CTENI DAT ZE SOUBORU TYPU HEX
   ZADEJ JMENO SOUBORU : PROG
   CTENI SKONCENO OK
   HEX 0000 - 003F
   ```
9. `Q`, na `KONEC ? (A/N)` `A`: návrat do CP/M.

| Příkaz | Význam (parametry šestnáctkově) |
|---|---|
| `H`, `N` | nápověda; jiný typ EPROM (2716, 2732) |
| `B adresa` | začátek bufferu jinam (logická adresa 0 je 2000h, buffer sahá pod BDOS, tady `0000 - B5FF`) |
| `C` | buffer celý FFh (zeptá se A/N) |
| `D od do` | výpis bufferu |
| `M`, `T`, `V`, `L` `EPROM-od EPROM-do RAM-od` | EPROM do bufferu; test, jestli EPROM data snese; porovnání; výpis EPROM |
| `P`, `A` `RAM-od RAM-do EPROM-od` | programování; buffer do RAM simulátoru a simulace (`SPUSTENA SIMULACE !`, konec ^B; sonda X3 v emulátoru není nikam zasunutá) |
| `E číslo`, `+`, `-` | výchozí úsek EPROM (2716 0000–07FF, 0800–0FFF…) |
| `S adresa`, `F od do hodnota` | změna bajtů (konec tečkou), vyplnění |
| `R`, `W od do` | čtení a zápis Intel HEX (přípona `.HEX` se doplní, výchozí jednotka A:; `R` bere i posun) |
| `^P`, `^B`, `Q`, `^C` | tisk zároveň na tiskárnu, přerušení, konec, reset disků |

Program přizpůsobuje pulzy: bajt FFh přeskočí, bajt, který už sedí, dostane 5 ms, jiný 20 ms a podle kontroly ještě
10 ms (40 ms, když po 20 ms nesedí). V logu je `programming: 64 bytes, 2.00 pulses per byte, pulses 10.12-20.22 ms`
a varování, že 30,35 ms na bajt je mimo katalogových 50 ms ± 5 (naprogramuje se stejně). Program assemblovaný
na disketě 1 (`ASM PROG` dá `PROG.HEX`) jde do bufferu příkazem `R`, program pro jinou adresu posune `B` nebo posun
u `R`. Pro 2732 kabel X2 pro 2732 (kapitola 9) a typ `2732`.
