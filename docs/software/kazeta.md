# Kazeta DSM-1: formát MIKOS V4/V5 a MIKROBASIC 2.4, kazeta z programu (HEX, BIN)

Rozbor 2026-10-06 podle zdrojáků ROM ze sapi.cz a skutečných nahrávek. Podle něj emulátor vyrobí kazetu
z programu (`src/core/tapegen.*`, kapitola 4).

## 0. Co je ověřeno a jak

| Zdroj | Výsledek |
|---|---|
| Zdrojáky ze sapi.cz (`mi40f_asm`, `mi5_asm`, `m5f_asm`, `mb_24_asm`) | výpisy `*_lst` porovnány s ROM v `roms/`: MIKOS V4.0F 0800–0FFF 2043 B shodných, 0 rozdílů; monitor V4.0 2006/0; MIKOS V5.0 2041/0; monitor V5.0F 3050/1 (jen výplň 0BFD); MIKROBASIC 2.4 4043/0. Zdroják V4.0F je zpětně upravená V4.0H („V4.0H ZPETNE UPRAVENA NA V4.0F“, EC1045 2014), ale bajtově odpovídá ROM. |
| Nahrávky `tool_80.wav`, `tool_48.wav`, `BASIC EX V4.0.wav` | dekódováno (port fázového dekodéru emulátoru do Pythonu), 0 chyb rámce, všech 118 kontrolních součtů bloků sedí. |
| Round trip | z vytaženého obsahu a jména vygenerovaný proud bloků je **bajtově shodný** s nahrávkou u všech tří pásek (s výplní posledního bloku jako V4). |
| Obsah | TOOL 80 4400–5FF3 a BASIC EX 4400–5FFF jsou shodné s `tool80_hex`/`bex_hex` ze sapi.cz. TOOL 48 z pásky (EDIT V2.3, 4400–7C6F) je jiná verze než `tool48_hex`. |
| Emulátor (ROM běží v emulaci) | vygenerované pásky: V4.0F `.K_L` + `.J` → `* PEDIT V2.5 *`; V5.0 `.K_L` + `.J` i `.L` (autostart) → PEDIT; malé soubory (3, 246, 501 B) se nahrají bajtově přesně; MIKROBASIC LOAD + RUN a BASIC řádek + strojový kód (`CALL`). Test `test_tapegen`. |
| Generátor v emulátoru | WAV z `sapiemu-cli --make-tape` je bajtově shodný s prototypem v Pythonu, kterým se dělal round trip (TOOL 80: 30 bloků, 49,5 s). |

Odvozené (neověřené na skutečném HW): minimální délky tónů pro skutečný magnetofon a dekodér DSM-1 (kapitola 5).

## 1. Fyzická vrstva

- UART MHB1012 na DSM-1, kazeta: 2400 Bd, **1 start, 8 dat (LSB první), bez parity, 2 stop bity** = 11 bitů/bajt,
  4,583 ms/bajt. V nahrávkách jdou bajty bloku těsně za sebou (žádné mezery mezi rámci).
- Kódování: úroveň linky XOR hodiny 2400 Hz, jedna perioda na bit. Klid = 1 = souvislý tón 2400 Hz (průchody
  nulou po 208 µs); změna dat = interval 416 µs.
- Mezery mezi bloky jsou na originálech **tón v klidu (1), ne ticho**: celá nahrávka je jeden souvislý úsek
  (dekodér našel 1 „burst“ na celou délku). Motor se mezi bloky zastavuje, ale páska je až k zastavení
  nahraná tónem.
- Doporučení pro WAV generátoru: **48 000 Hz, 16 bit mono, 20 vzorků na bit** (přesně), obdélník ±12000
  (jako emulátor), v první polovině bitu úroveň linky, ve druhé její negace (`(level ^ clk)`, clk = 0 v první
  půlce – shodné s `Dsm1::tape_record`). Polarita je pro dekodér DSM-1 (měří intervaly) lhostejná. Všechno
  na mřížce bitů, **nikde ticho** (začátek úseku po tichu je u skutečného dekodéru nejasný, viz Nejasnosti
  DSM-1.md).

## 2. MIKOS V4.0F / V5.0: blok na pásce (275 bajtů)

| offset | délka | obsah |
|---|---|---|
| 0 | 3 | `10 10 10` preambule (PREAM) |
| 3 | 1 | typ (návěští bloku): `01` SOH hlavička souboru, `3A` `:` jméno pásky (volume), `41` `A` ASCII data, `42` `B` binární data, `04` EOT = poslední blok souboru (V4 „END OF TAPE“, ve skutečnosti uzavření souboru), V5 navíc `00` EOL „konec záznamů“ (`.K_K`) |
| 4 | 255 | data |
| 259 | 11 | jméno WRNM (kódy AND-1) |
| 270 | 1 | číslo bloku (binárně) |
| 271 | 1 | číslo souboru (binárně, `INR A`, žádné BCD) |
| 272 | 1 | kontrolní součet = **součet bajtů 3..271 (typ, 255 dat, 11 jméno, blok, soubor = 269 B) mod 256** |
| 273 | 2 | `10 00` postambule (SEROP + „SENTINEL“); čtečka je **nečte**, spolkne je SINC |

Ověřeno na všech 118 blocích tří nahrávek. Zápis `WRBLO` (V4 0EE6h): `B` se nuluje `LXI B,0000` až po
preambuli, `SEROUT` přičítá každý bajt k B, po `SEROUT B` (součet) následuje `SEROP`, `XRA A`, `SEROUT`.

```
0EE6 WRBLO: PUSH B / PUSH H / PUSH D
            CALL STARW          ; OUT 10h,30h (relé+KAZ, zápis), OUT 11h,1Fh
            MVI A,80 / CALL BTIME ; 800 ms, při návěští 'B' 80/8 = 100 ms
            CALL SEROP x3       ; 10 10 10
            LXI B,0000h         ; B = součet 0, C = 0 -> 256 bajtů
            CALL SEROM          ; typ + 255 dat z BOUT-1
            LXI H,WRNM / MVI C,11 / CALL SEROM
            POP D / MOV A,E / CALL SEROUT   ; číslo bloku
            MOV A,D / CALL SEROUT           ; číslo souboru
            MOV A,B / CALL SEROUT           ; součet
            CALL SEROP / XRA A / CALL SEROUT ; 10 00
            ... MVI A,1 / CALL TIME (10 ms), když návěští != 'B': STOP (OUT 10h,10h)
```

### 2.1 Jméno

- Hlavička (typ 01) má v datech **ASCII text** jména + komentáře (až 64 znaků, COMLE), za ním **jedna mezera**
  (RNAME zapíše `' '` na pozici dalšího znaku před čtením klávesy, takže zůstane na místě CR) a zbytek
  **nuly** (NULBF). Příklad TOOL 80: `54 4F 4F 4C 20 38 30 20 00 00 …`; BASIC EX: `42 41 53 49 43 20 00 …`.
  V5 (RNAME monitoru V5) převede na velká písmena, stejný tvar (v testu emulátoru `DATA` + 2 mezery + nuly).
- Pole WRNM v každém bloku = prvních 11 bajtů dat hlavičky `AND 3Fh`, nula → `20h` (`WRN1`, 0BB7h+).
  „TOOL 80“ → `14 0F 0F 0C 20 38 30 20 20 20 20`. Čtečka ho **jen zobrazuje** (V4 na první řádek obrazovky
  3800h, V5 do okna READNM); nekontroluje ho, ale je v kontrolním součtu.
- Hledání (`CMPNM` V4 0B45h, V5 0F4Fh/TESTJM): porovnává zadané jméno s **ASCII daty hlavičky**, znak po znaku,
  **jen do prvního oddělovače v zadaném jméně** (V4: mezera, `,`, TAB, CR; V5: mezera, CR, 00). Je to tedy
  shoda prefixu: `.K_L TOOL` najde první „TOOL…“, i „TOOL 80“ se porovná jen v části „TOOL“. `*` = každou
  hlavičku nabídne s dotazem Y/N (A/N). V4 hledá hlavičky typu 01 i `:`, V5 hlavičky `:` přeskakuje.
  → Generátor: jméno bez mezer, nejvýš 11 znaků (do WRNM se vejde 11), velká písmena, jedinečný prefix.

### 2.2 Číslování

- `OPEN` (V4 0A95h): číslo souboru = číslo souboru posledního bloku nalezeného na pásce + 1, blok = 0;
  `WOB` před každým zápisem zvýší číslo bloku → **hlavička = blok 1, data od 2, EOT pokračuje**. TOOL 80:
  01 (hlavička), 02–1D (B), 1E (EOT) → „status 1E“ v dokumentaci je číslo posledního bloku, které čtečka
  zobrazí (`*"TOOL 80    " 1E`). Celkem 30 bloků včetně hlavičky.
- `INIT` (`.K_I`): blok `:` s číslem souboru 0, blok 1. Nahrávky TOOL 80 a BASIC EX mají soubor 01, TOOL 48 soubor 02.
- Čtení: `FIND` vezme číslo souboru D z nalezené hlavičky a nastaví BNI = (D, 1) bez ohledu na číslo bloku
  hlavičky. `RIB` před každým blokem BNI++ a `NUFIL` vyžaduje **přesně** blok BNI a soubor D. Jinak
  `ERROR/N?` (V5 `CHYBA CTENI: I/D/N/A=ZNOVA ?`). Typ datových bloků se nekontroluje (jen `:`/01 = konec souboru).
  → Generátor: soubor 1 (libovolné 1..255 stejné ve všech blocích), hlavička blok 1, data 2, 3, …

### 2.3 Binární soubor (`.K_S od do`, V4 SAVE 0E10h)

Proud bajtů zapisovaný přes PO do bufferu BOUT (255 B), plný buffer = blok `B`:

```
'<'  end_lo end_hi  start_lo start_hi  '>'  KS1  data[start..end]  KS2  1Ah
```

- Pořadí adres: **nejdřív konec, pak začátek** (EXPR uloží `od`, `do`; `POP H` vezme `do`). TOOL 80:
  `3C F3 5F 00 44 3E D4`.
- **KS1 = (end_lo + end_hi + start_lo + start_hi + 3Eh) mod 256** (B se nuluje až po `'<'`). TOOL 80: D4. ✓
- data = paměť start..end **včetně** (HILO: konec, když HL > DE).
- **KS2 = součet dat mod 256**. Za ním `1Ah`.
- Délka proudu = 9 + (end − start + 1). Prvních ⌊L/255⌋ úseků po 255 B jde do bloků `B`, zbytek do bloku
  `04` (EOT), který zapíše `CLOSE`.
- Výplň bloku EOT: **V4** buffer nemaže, za `1Ah` zůstanou bajty předchozího bloku (u velmi krátkého souboru
  zbytek textu hlavičky). Když proud přesně zaplní poslední blok `B`, V4 zapíše tentýž buffer ještě jednou
  jako EOT. **V5** maže buffer po každém bloku (NULBF 125Bh), `1Ah` zapíše `CLOSE0` (0E8Fh) a zbytek jsou nuly.
  Obě čtečky výplň ignorují (LOAD čte jen do KS2).
- TOOL 80: proud 7165 B = 28 × 255 + 25 → bloky 02–1D `B`, EOT s 25 platnými bajty (23 dat, KS2 6A, 1A).

### 2.4 Čtení (`.K_L jméno`, V4 LOAD 0DA7h)

```
0DA7 LOAD: CALL FIND            ; jméno, PLAY DONE?, hledání hlavičky, RIB = první datový blok
           MVI A,'B' / STA BIN-1 / CALL START
0DB2 LO1:  CALL RICH / ORA A / JZ LO1   ; nuly před '<' přeskočí
           SUI '<' / JNZ ERSTP / MOV B,A
           CALL RADR@ / SHLD EADR       ; 1. adresa = konec
           XCHG / CALL RADR@ / SHLD SADR ; 2. adresa = začátek
           CALL RICH / CPI '>' / JNZ ERSTP
           CALL CHECK                   ; KS1
           MVI B,0
0DD9 LO2:  CALL RICH / MOV M,A / CALL HILO / JNC LO2
           CALL CHECK / STA SUM         ; KS2 si pamatuje pro .J
```

- Čtení bloku `RBLOK` (0E7Eh): `START` (OUT 10h,38h; OUT 11h,1Fh), `SINC` čeká, až po dobu **10 ms nepřijde
  žádný znak** (IN 12h; TIME 10 ms; test DR; opakuje), pak vyžaduje **tři 10h po sobě** (jinak znovu SINC),
  přečte 256 + 11 + 2 bajty, porovná součet. Po bloku, který není `B`, zastaví motor (`#VERS "E"#`).
- Mezi bloky musí čtečka zpracovat 255 bajtů bufferu (LOAD přes RI→GET@) a pak najít 10 ms klidu.
  Měřeno v emulátoru s ROM V4.0F: **mezera 70 ms a víc projde, 60 ms a méně ne** (ERROR na některém bloku),
  stejně mezera před EOT (80 ms OK, 60 ms ne). ROM sama píše ≥100 ms (+ režie), skutečně ~210 ms.
- Hlavička nemusí mít konkrétní číslo bloku, svazek `:` ani blok EOT nejsou potřeba, pokud KS2 leží v
  bloku `B` (LOAD skončí po KS2). EOT je potřeba, když v něm leží konec dat (běžný případ) → vždy psát.
- Celý soubor musí ležet v RAM mimo systémovou oblast: V4 má v 4000–42FAh zásobník, proměnné a buffery
  (BOUT 40F1h, BIN 41F1h, WRNM 42F0h, `ORG @R@+400H` = uživatelské programy od **4400h**); V5 buffer 4159h–4258h.
  Nahrání přes ně rozbije čtení. → Generátor: povolit jen start ≥ 4400h (pod 4000h je ROM a 3800h video).

### 2.5 `.J` (V4 JUMP 08C7h, V5 monitor 0A26h)

```
08C7 JUMP: LHLD EADR / XCHG / LHLD SADR
           MOV A,L / ORA H / JZ ERROR     ; SADR = 0 -> nic nenahráno
           PUSH H                         ; návratová adresa = start
           MVI B,0
     J1:   MOV A,M / ADD B / MOV B,A / CALL HILO / JNC J1   ; součet SADR..EADR v paměti
           LDA SUM / CMP B / JNZ ERROR    ; = KS2 z posledního LOAD
           RET                            ; skok na SADR
```

- `.J` (monitor V4: neznámý příkaz → EXROM → `EP2+6` = EXMON) bere SADR/EADR/SUM z posledního `.K_L`;
  znovu spočte součet v paměti, při shodě **skočí na SADR = nejnižší adresu souboru** (druhá adresa v `<…>`).
  Pod startem zůstane na zásobníku návrat do monitoru, program může skončit `RET`.
- **Vstupní bod = první bajt souboru.** Když má HEX jiný vstup (záznam typu 03/05 nebo dohoda), musí generátor
  buď odmítnout, nebo přidat před data trampolínu `C3 lo hi` (start := min − 3, pokud je tam volno a ≥ 4400h).
- V5 navíc `.L` (LOADB 09D1h): nahraje **první soubor na pásce** (prázdné jméno `"`), a **když je na SADR `C3`**,
  spustí: SADR ≠ 4400h → JUMP (součet + skok); SADR = 4400h → podle 4403h (`C3` → PEDIT, jinak BASIC, se
  studeným startem, LOAD V5 při SADR = 4400h maže příznaky @BT/@PED). Jinak jen vypíše adresy.
  Trampolína `C3` na začátku tedy dává i autostart ve V5.

### 2.6 Časování (ROM, naměřené, doporučené)

TIME: `LXI B,800+255` smyčka 25 taktů × 800 = 20 000 taktů = 10 ms při 2 MHz.

| místo | ROM V4.0F | ROM V5.0 | originální nahrávky | emulátor (ROM V4/V5) | min. v emulátoru | **generátor** |
|---|---|---|---|---|---|---|
| úvodní tón před hlavičkou | 800 ms (BTIME), motor | 800 ms | 1,97–2,04 s | 0,75 s + 50 ms ticho | 100 ms OK | **2000 ms** |
| hlavička → 1. blok B | ≥ 800 ms (SAVE: STARW + TIME 80) + 100 ms + start motoru | +800 ms dozvuk hlavičky | 6,0–7,8 s (obsluha) | 0,97 s / 1,99 s | 30 ms OK | **2000 ms** |
| mezi bloky B | 100 ms + režie | 100 ms + režie | **209,6–211,7 ms** | 170 / 180 ms | 70 ms | **210 ms** (nikdy < 150) |
| před EOT | 800 ms + režie | 800 ms | 983–1034 ms | 806 / 812 ms | 80 ms | **1000 ms** |
| za EOT | 800 ms (CLOSE: STARW, TIME 80, STOP) | 800 ms (ZASTAV) | 1,5–2,4 s | | | **1000 ms** |

Měřeno od konce 2. stop bitu bajtu `00` po start bit první `10h`. V5: `CAS` je v ROM text `'PERINA'`
(0C24h), první bajt `'P'` = 50h = 80 → 800 ms; `.Z n` (CASK ≠ 0) mezery zdvojí.

### 2.7 Minimální páska, kterou ROM přijme (V4 i V5, `.K_L jméno` + `.J`)

```
tón ≥ ~0,1 s (doporučeno 2 s)
blok 01: data = ASCII jméno + 20h + nuly, WRNM, blok 1, soubor F
tón (doporučeno 2 s)
bloky 42 'B': po 255 B proudu, bloky 2, 3, …, soubor F, mezi nimi tón 210 ms
tón 1 s
blok 04: zbytek proudu (1..255 B, končí 1Ah), výplň nulami, blok n, soubor F
tón 1 s
```

Je-li proud ≤ 255 B, je jen hlavička + EOT (ověřeno: 3 a 246 B). Svazek `:`, `INIT`, blok EOL nejsou potřeba.
Generátor v režimu „V4 přesně“ (`v4pad`) vyrobí bajtově totéž co `.K_A`/`.K_S` V4 (round trip shodný).

## 3. MIKROBASIC 2.4 (ZPS-1)

Zdroják `mb_24_asm` (sapi.cz) = ROM `MIKROBASIC_2.4_ZPS-1.bin`. RAM: TXTUNF 4024h, STACK 40B1h,
**TXTBGN = 40EFh**, TXTE 47ABh (konec programové oblasti bez `RAM`), BUFA 47ADh (vstupní řádek), BRST 47EEh (RST 1/2).

### 3.1 SAVE (0BB3h) a LABEL (0BF4h)

```
0BF4 LABEL: tisk "NAME ", GETLN s výzvou '=' (řádek do BUFFER, končí CR)
            MVI A,30h / OUT 10h ; relé + KAZ, zápis
            MVI A,0FFh / OUT 11h
            CALL SEC3           ; 110 x 76,4 ms = 8,40 s tónu
            MVI A,'B' / CALL TOUT
            CALL DBUFF / MVI B,0 ; součet od jména
     LAB1:  LDAX D / CALL TOUT / CPI 0Dh / RZ / INX D / JMP LAB1   ; jméno + CR
0BB3 SAVE:  CALL LABEL
            LHLD TXTUNF / MOV A,L / CALL TOUT / MOV A,H / CALL TOUT ; konec, lo hi
            XCHG / LXI H,TXTBGN
     SAVE1: CALL COMP / JZ SAVE2 / MOV A,M / CALL TOUT / INX H / JMP SAVE1  ; 40EFh..TXTUNF-1
     SAVE2: MOV A,B / CALL TOUT  ; součet
            CALL SEC1            ; 20 x 76,4 ms = 1,53 s tónu
     SAVE3: MVI A,10h / OUT 10h  ; motor stop
0B97 TOUT:  ADD B do součtu, OUT 12h, čeká na TBMT (IN 11h, bit 6)
```

Záznam (bajty těsně za sebou):

```
'B'  jméno (0..n znaků, jak napsáno)  0Dh  end_lo end_hi  data[40EFh .. end-1]  CS
end = TXTUNF = 40EFh + délka dat
CS = (součet jména + 0Dh + end_lo + end_hi + data) mod 256      ('B' se nepočítá)
```

Ověřeno na pásce z emulátoru (`10 PRINT "HELLO"`, `20 PRINT 6*7`, jméno PROG):
`42 50 52 4F 47 0D 0B 41 | 0A 00 "PRINT \"HELLO\"" 0D 14 00 "PRINT 6*7" 0D | 72`.

Časování: WAIT2 (09F4h) = 1527 taktů na H, SEC31 = 152 735 taktů = 76,4 ms při 2 MHz.
Úvodní tón **SEC3 = 8,40 s**, dozvuk **SEC1 = 1,53 s** (emulátor: 8,35 s + 50 ms ticho, 1,52 s).

### 3.2 LOAD (0B23h)

```
0B23 LOAD:   MVI A,38h / OUT 10h / MVI A,0FFh / OUT 11h
     LOAD40: IN 12h / MVI B,0
     LOAD30: IN 11h / ANI 80h / JNZ LOAD40      ; přišel znak -> znovu
             B != FFh: CALL SEC1 (1,53 s), B = FFh, JMP LOAD30
     LOAD20: '*' na obrazovku, PIN, CPI 'B', JNZ LOAD40
             MVI B,0 / PIN / LOAD5: CO, PIN, dokud není 0Dh   ; jméno se jen vypíše
             PIN -> E, PIN -> D                  ; konec
             LXI H,TXTBGN
     LOAD4:  CALL COMP / JZ LOAD3 / PIN / MOV M,A / INX H / JMP LOAD4
     LOAD3:  součet, JNZ TPERR ("ERROR"), SHLD TXTUNF, motor stop
```

- Potřebuje **okno 1,53 s bez jediného přijatého znaku** (souvislý tón) a hned první znak potom musí být `'B'`.
  Ověřeno v emulátoru: úvodní tón 2,0 s projde, **1,0 s ne** (čeká dál). ROM píše 8,4 s → generátor 8,4 s
  (minimum ~2 s; počítá se od zapnutí motoru příkazem LOAD).
- **Jméno se nekontroluje**, LOAD vezme první záznam. Prázdné jméno (jen CR) je platné.
- Data jdou **vždy od 40EFh**, délka = end − 40EFh. LOAD **nekontroluje meze**: end < 40EFh → čte 64 KB;
  end > 47ABh přepíše vstupní buffer a BRST (a za 47FFh není na ZPS-1 RAM). Po LOAD je TXTUNF = end.
- Řádek programu: číslo řádku **2 bajty lo, hi**, text tak, jak byl napsán (klíčová slova se netokenizují,
  mezera za číslem se nezapisuje), `0Dh`.
- MIKOS V4 `.K_B adr` (BB 0FA8h) čte tentýž formát do libovolné adresy (adr .. adr + end − 40F0h), před
  `'B'` chce 500 ms klidu; v V5 tento příkaz není.

### 3.3 Má smysl „MIKROBASIC páska z HEX“ pro strojový kód?

Jen omezeně:
- Kód musí být přeložený **pro absolutní adresy od 40EFh** a celý se vejít do 40EFh–47AAh (1724 B, se
  `RAM n` a pamětí navíc i výš). Nic pod 40EFh (proměnné BASICu, zásobník) přenést nejde.
- Po LOAD BASIC považuje oblast za program (TXTUNF = end): `LIST` vypíše nesmysly, editace řádků posune kód.
- Ověřený použitelný tvar (emulátor): **BASIC stub + kód**:
  `01 00 "CALL nnnnn" 0D  02 00 "END" 0D  <strojový kód>`, kde nnnnn = 40EFh + délka stubu dekadicky
  (stub s pětimístným číslem má 19 B → kód od 4102h, tedy `1 CALL 16642` / `2 END`). `RUN` skočí přes
  `CALL` (BCALL 09A4h: PUSH návratu, PCHL), kód může skončit `RET`, pak `END` → READY. Přímý příkaz
  `CALL 16623` funguje taky (kód přímo od 40EFh, bez stubu, ale pak `RUN` spadne).
- Kód nesmí obsahovat odkazy pod 40EFh kromě systémových vektorů ZPS-1 (CO 4007h …).

## 4. Kazeta z programu v emulátoru (`src/core/tapegen.*`)

- **Vstup:** Intel HEX (`.hex`, `.ihx`, `.txt`, adresy ze záznamů) nebo binární soubor (`.bin`, adresa se zadá). Díry mezi
  záznamy HEX se vyplní nulami (při nahrání se tedy přepíšou nulami).
- **Zvuk:** 48 kHz, 20 vzorků na bit, obdélník ±12000 jako záznam z emulované DSM-1, mezery jsou tón, nikde ticho.
- **MIKOS** (`.K_L jméno`, pak `.J`; V5 i `.L`):
  - program musí ležet od 4400h výš (4000–43FF je RAM MIKOSu);
  - `.J` skočí na nejnižší adresu. Má-li program jiný vstup (start), dá se před něj `C3 lo hi` (JMP) a soubor
    začíná o 3 bajty dřív (musí to být pořád ≥ 4400h). Ve V5 pak `.L` program i sám spustí;
  - hlavička (blok 1), bloky `B` po 255 bajtech proudu, zbytek v bloku EOT, výplň nulami (jako V5), soubor 1;
  - tóny: úvod 2 s, za hlavičkou 2 s, mezi bloky `B` 210 ms, před EOT 1 s, na konci 1 s (jako originální
    nahrávky).
- **MIKROBASIC** (`LOAD`, jméno se nekontroluje):
  - program v BASICu od 40EFh (řádky `číslo_lo číslo_hi text 0Dh`) jde na kazetu tak, jak je;
  - strojový kód od 4102h výš: před něj se přidá řádek `1 CALL start` a `2 END` (19 B od 40EFh), mezera do
    kódu se vyplní nulami. `RUN` kód spustí, `RET` vrátí do BASICu. `LIST` pak ukáže nesmysly;
  - program mezi 40F0h a 4101h nejde (nevejde se před něj řádek s CALL);
  - nad 47AAh jen s `RAM n` a pamětí navíc (generátor upozorní, LOAD meze nekontroluje);
  - tóny: úvod 8,4 s a na konci 1,53 s jako SAVE.
- **Jméno na kazetě:** výchozí podle jména souboru, velká písmena, znaky 20h–5Fh, nejvýš 11 znaků (pole WRNM).
- **Ovládání:**
  - okno Média → Vložit… → soubor HEX/BIN → dialog „Program na kazetu“ (formát, jméno, u BIN adresa, start);
  - tlačítko „Uložit jako WAV…“ uloží kazetu (i nahranou nebo z MP3) jako WAV, např. pro přehrání do
    skutečného SAPI z PC;
  - MCP `tape_insert` (`format`, `name`, `address`, `entry`), `tape_control` `export` s `path`;
  - `sapiemu-cli --make-tape PROG.hex KAZETA.wav` (převod bez spuštění stroje), `--tape SOUBOR` (vloží a stiskne
    PŘEHRÁT), `--tape-export`, volby `--tape-format`, `--tape-name`, `--tape-addr`, `--tape-entry`;
  - parametr sestavy `tape` DSM-1 bere i `.hex` (kazeta MIKOS s výchozím jménem).
- Kazeta z programu je **jen pro čtení** (nedá se na ni nahrávat a nikdy se nezapíše zpět do souboru). Uložený
  stav si pamatuje soubor, formát, jméno, adresy a pozici a kazetu po načtení vyrobí znovu.

## 5. Nejasnosti (k ověření na skutečném HW)

- Jak se skutečný dekodér DSM-1 chová na začátku úseku po tichu. Generátor proto nikde ticho nedává.
- Minimální délky tónů pro skutečný magnetofon (rozběh motoru po zastavení za hlavičkou, AGC). Doporučené časy
  jsou blízko originálu; minima (MIKOS mezi bloky 70 ms, MIKROBASIC úvod asi 2 s) jsou jen z emulátoru, čtecí
  rutina ale běží v ROM se stejnými takty.
- Proč je skutečná mezera mezi bloky `B` 210 ms a v emulátoru 170 ms (100 ms TIME + režie): pomalejší skutečný
  stroj (čekací stavy?) nebo jiná režie. Pro formát nepodstatné.
- V5 ve výchozím stavu počítá se SP-210 (TSP = 0, port 34h–37h 8255), s K-10 (relé) přes `.K_X`. Formát stejný.
- Přehrání obdélníku z PC do skutečného vstupu DSM-1: stačí přímo, nebo je lepší zaoblit (filtr)? Originální
  nahrávky z magnetofonu jsou 96 kHz.
