# Klávesnice (konektor X2 na JPR-1ZI)

Všechny klávesnice se připojují na porty 3212 desky JPR-1ZI: P0 je port 01h, P1 je port 02h.

Emulátor umí **EKL-1**, **Consul 262.3** (s úpravou 7474 i bez ní), **Consul 259.11** (KOI-7 i KOI-8 cs),
**ANK-1** a **ANK-1M2** (Libor Lasota, s konektorem joysticku). Klávesnice se volí v sestavě (`"keyboard": { "type": ... }` u JPR-1ZI), za běhu v menu
Stroj → Klávesnice, v MCP nástrojem `set_keyboard` a v `sapiemu-cli` volbou `--keyboard NAME`.
Vnitřek klávesnic (AT89C2051 v EKL-1, logika Consulů) se neemuluje, jen chování na portech.

## Přehled emulace

- **Bzučák klávesnice** (`core/beeper.*`) je zvukový zdroj sběrnice jako MPH-1V. Hraje přes zvuk PC (Stroj → Zvuk,
  joystick a myš, jen při 100 %) a nahrává se do WAV (`--audio-wav`, MCP `audio_record`):
  - EKL-1, Consul 262.3 a 259.11: aktivní bzučák na X2 pin 14 (P0-OUT1 = 0 zní, tón 2,4 kHz). U Consulů jde
    o předpoklad (nejasnost);
  - ANK-1: piezo za P0-OUT D6, zvuk dělá program;
  - parametr karty CPU „Klávesnice: bzučák“ (`keyboard.beeper`, výchozí zapnuto, platí hned) ho vypne;
  - **po resetu** je výstupní latch P0 (3212) vynulovaný, takže P0-OUT1 = 0 a bzučák EKL-1 pípne, než monitor
    zapíše `OUT 01h,02h`: MikroMon 25 ms, ECMON 48 ms. Emulace to dělá věrně (autor).

| Klávesnice | Kódy | Strobe | Čeká na ACK=0 | P1-IN7 | Kód (`src/input/`) |
|---|---|---|---|---|---|
| EKL-1 | ASCII, české C0–FF s prohozenou velikostí | drží do ACK | ano | data | `ekl1.*` |
| Consul 262.3 | 8bitové (LAT), kurzor C1–C4, numerika B0–B9 | drží do ACK (7474) | ne | data | `consul.*` |
| Consul 262.3 (bez 7474) | dtto | pulz 1 ms | ne | data | `consul.*` |
| Consul 259.11 | KOI-7 (ASCII) | drží do /AC | ano | /CLR | `consul.*` |
| Consul 259.11 (KOI-8 cs) | KOI-8 cs, české s prohozenou velikostí | drží do /AC | ano | BIT8 | `consul.*` |
| ANK-1 | matice 5×8 | — | — | INT | `ank1.*` |
| ANK-1M2 | matice 5×8, joystick nebo myš na P1-IN D5–D0 | — | — | INT | `ank1.*`, `joyport.*` |

Paralelní klávesnice (všechny kromě ANK-1) mají společný základ `parallel_kbd.*`. Volby v sestavě:

- `char_gap_us`: mezera mezi znaky, výchozí 1 ms, bez 7474 20 ms;
- `strict`: věrně jako hardware, bez pomoci pro text z PC (nečekat na ACK=0, neopakovat přehlédnutý pulz),
  znaky se mohou ztratit. V konfigurátoru „Klávesnice: věrně jako hardware“ u karty CPU, platí hned (i za chodu).
  ANK-1 tuto volbu nemá (drží klávesu, dokud ji program nepřečte);
- Consul 262.3: `latch_mod` (úprava 7474, výchozí `true`). Na PKT-1 (SAPI-1.5) se dává bez úpravy: strobe zachytí
  port B 8255 v režimu 1, kód se čte invertovaný jako na X2 JPR-1Z (`docs/desky/PKT-1.md`);
- Consul 259.11: `code` (`koi7` nebo `koi8cs`);
- ANK-1: `press_ms` (nejkratší doba stisku i uvolnění, výchozí 40 ms), `software` (`mikrobasic`, výchozí, nebo
  `mikos`: jinak se mapují klávesy PC na klávesy matice). Preset „ANK-1“ je pro MIKROBASIC, „ANK-1 (MIKOS)“ pro
  MIKOS V4 (i MIKROBIOS JPR-1S).
- **ANK-1 a ANK-1M2 z klávesnice PC (od 2026-10-06):**
  - **Držíš = drží:** klávesa matice je stisknutá, dokud držíš klávesu na PC. Žádná fronta, autorepeat Windows
    se nepoužívá (opakuje si to program sám jako na skutečném stroji, třeba MIKROBIOS).
  - **Psaní podle znaku:** znak z PC se přeloží na klávesu ANK-1 (české rozložení: „,“ = SHIFT + N). SHIFT jde
    dolů dřív a klávesa až potom, co program přečetl řádek se SHIFT; při puštění jde nahoru nejdřív klávesa a
    SHIFT až potom. Žádné skenování tak nenajde N bez SHIFT. Psané klávesy jdou po sobě: překrývající se klávesy
    rychlého psaní čekají, až program přečte předchozí klávesu puštěnou (MIKROBASIC jinak další klávesu
    zahodí). Každá klávesa je stisknutá aspoň `press_ms` a dokud ji program nepřečte, krátký ťuk se neztratí.
  - **Režim pro hry** (Ctrl+F7, Stroj → Klávesnice, ve stavovém řádku „režim pro hry“): písmena podle popisu,
    číslice podle polohy (horní řada, i v českém rozložení), Shift na PC = SHIFT, mezerník a Enter. Klávesy
    stisknuté naráz jsou stisknuté naráz (Manic Miner: O/P a SHIFT). Symboly jako na ANK-1 (SHIFT + klávesa).
  - **Vkládání textu a MCP** (`type_text`) jdou přes frontu: každý znak jako ťuk stejným mechanismem (i SHIFT
    předem). Dřív MIKROBIOS při psaní „,“ občas dostal i „N“ (stisk uprostřed skenování matice).

Emulace (kromě „strict“) nepošle další znak, dokud program nevrátí ACK do 0, aby se znaky z PC neztrácely.
U pulzního STROBE (Consul 262.3 bez 7474) pošle klávesu znovu, když program během pulzu port 01h vůbec nečetl
(např. zrovna ukládal soubor). Na skutečném stroji by se taková klávesa ztratila, program to nijak nepozná.

### Mapování kláves PC

Text (písmena, číslice, znaky) jde přes textový vstup PC, takže funguje české rozložení Windows. Klávesy
bez textu:

| PC | EKL-1 | Consul 262.3 | Consul 259.11 | ANK-1 |
|---|---|---|---|---|
| Enter | 0D | 0D (Ctrl+Enter = LF 0A) | 0D | CR |
| Esc | 1B | 1B | 1B | LF (v MIKOS = ESC) |
| Backspace | 08 | 08 | 08 | Shift+T (08) |
| Tab | 09 | 09 | 09 | Shift+SP |
| Delete | 0B (Shift 1A) | 7F | 7F | Shift+P (7F) |
| Šipky | WordStar (05/18/13/04) | C1 ↑, C2 ↓, C3 →, C4 ← | ← 08, ↓ 0A | — |
| Home | 1E | 8D (klávesa 79, šipka vlevo nahoru) | — | — |
| PgUp / PgDn | 12 / 03 | — | 0B (VT) / 0C (FF) | — |
| Numerika 0–9, `.` | text | B0–B9, AE (Shift+0 = 80) | text | text |
| F1 / F2 / F3 | — | ROL 80/81, COPY 82/83, BREAK 84/85 (se Shiftem druhý kód) | DC1 11 / DC2 12 / — | — |
| F4 / F6 / F7 | — | klávesy 61/62/63 = D0/D1/D2 | — | — / INT / T |
| Pause | — | — | CLEAR (P1-IN7, drží 100 ms) | — |
| Ctrl+písmeno | 01–1A | 01–1A | 01–1A | Ctrl+Q/W/A/S/X = Shift+klávesa |

F5 a F8–F12 patří emulátoru (běh, turbo, krokování, reset). Když je zaměřené okno sériového terminálu,
klávesy jdou do terminálu (kurzorové klávesy v kódech WordStar jako u EKL-1). V MCP jsou klávesy bez textu
v nástroji `press_key`.

## EKL-1

Zdroje:
- `..\SAPI-karty\EKL-1\Firmware\EKL-1_SOFT_updated\KEYB_REPAIR.a51` (rutina OUTPUT a tabulky kódů na řádcích ~860–905)
- `..\SAPI-karty\EKL-1\EKL-1.pdf`

### Signály

- **Data:** firmware provádí `CPL A` a `SWAP A`. SWAP opravuje prohození nibblů na DPS. Výsledkem je **invertované ASCII** na P1-IN0..7, tedy port 02h.
- **STROBE** (P3.3) vede na X2 pin 10 (P1-STB) i pin 30 (P0-IN0). V klidu je v 1, při připraveném znaku jde do 0.
- **Čekání na ACK:** firmware pak **čeká, dokud ACK (P3.7 = X2 pin 13 = P0-OUT0) nečte 1**, a teprve potom vrátí STROBE do 1. Když program nepotvrdí, EKL-1 visí.
- **Bzučák:** SG1 F/CM12P zapojený přímo mezi VCC a X2 pin 14 (P0-OUT1), bez obvodů klávesnice. Zní při 0.
  Software jen drží úroveň: ECMON při BEL dá `XOR A; OUT (01h),A`, čeká asi 160 ms (BC = 3000h) a pak
  `OUT (01h),02h`. Bzučák má tedy vlastní oscilátor (aktivní). Emulace: tón 2,4 kHz, dokud je P0-OUT1 = 0.

### Kódy

- **Rozložení:** US. Alt-lock přepne na české rozložení s kódy C0–FFh (KOI-8čs) a prohozenými velkými a malými písmeny.
- **Ctrl+písmeno** dává řídicí kódy. Esc = 1Bh, Enter = 0Dh, BS = 08h, Tab = 09h.
- **Kurzorové klávesy ve stylu WordStar:**

| Klávesa | Kód | Klávesa | Kód |
|---|---|---|---|
| ↑ | 05 | ↓ | 18 |
| ← | 13 | → | 04 |
| Home | 1E | End | 11 |
| PgUp | 12 | PgDn | 03 |
| Ins | 16 | Del | 0B |

- Varianty se Shiftem jsou v tabulce ve firmwaru.
- **Scroll Lock:** přímé zadání kódu dvěma hex číslicemi.
- **Prohozená velikost českých písmen:** firmware posílá české znaky jako KOI-8 cs XOR 20h (malé `ž` jako FAh,
  což je v KOI-8 cs `Ž`). ECMON a ovladače CP/M (C25911) XOR 20h vracejí. MikroMon kódy ukládá a zobrazuje,
  jak přišly, takže v emulaci po napsání `ž` ukáže `Ž`. Emulátor se chová jako firmware (viz Nejasnosti
  v `docs/SOUHRN.md`).

### Protokol čtení (MikroMon `kbd.asm`, ověřeno v binárce 1.3)

1. `IN (01h)` bit 0 = 0 znamená, že je klávesa připravena.
2. `IN (02h)` a `CPL` dá kód ASCII.
3. `OUT (01h),03h` nastaví ACK.
4. Čeká se, až `IN (01h)` bit 0 = 1.
5. `OUT (01h),02h` zruší ACK. P0-OUT1 zůstává 1, bzučák je potichu.

Stejně čtou klávesnici ECMON i ovladače CP/M, takže s tímto programem fungují všechny paralelní klávesnice
(EKL-1, oba Consuly). Ověřeno testem `test_keyboards` (MikroMon se všemi pěti variantami).

### Emulace

Fronta znaků z PC klávesnice. Stavový automat:

1. Klid: STROBE=1.
2. Je-li znak ve frontě, data se nastaví a STROBE=0.
3. Čeká se na ACK=1, pak STROBE=1.
4. Čeká se na ACK=0 a pokračuje se dalším znakem.

Malé prodlevy mezi kroky odpovídají rychlosti firmwaru.

## Consul 262.3

Zdroj: `jpr-1z.php`, manuál 864.795 (skeny `H:\SAPI-1\SAPI_DOKU_2024-01-03\skeny_dokumentace_bez_retusi_Petr\CONSUL_262.3_4\`).

- **Zapojení:** DAT0–7 → P1-IN0..7 (X2 piny 6, 4, 3, 5, 2, 8, 9, 7). STROBE → X2 pin 30 (P0-IN0). Napájení +5 V na pinu 22.
- **STROBE se nezachytává:** jde o pulz asi 1 ms, takže při pollingu lze klávesu přehlédnout. Úprava se 7474
  (na JPR-1Z) STROBE zachytí a nuluje ho až potvrzení programu P0-OUT0 = 1. Emulátor ji má ve výchozím stavu
  (`latch_mod: true`), původní chování je preset „Consul 262.3 (bez 7474)“.
- **JPR-1V úpravu 7474 nemá** (P0-IN průchozí, data zachytává P1-STB), sestava V má proto `latch_mod: false`.
  MikroBIOS 3.0 klávesu nepotvrzuje přes ACK: vezme ji na pulz STROBE (`IN 01h` bit 0 = 0, `IN 02h`) a pak čeká,
  až je STROBE 500 čtení po sobě neaktivní. S úpravou 7474 by STROBE zůstal držený a další klávesa by nepřišla.
- **Polarita** (strobe aktivní v 0, data invertovaná) je **jen odvozená z MikroMonu**.
- **Kódy (verze LAT, finál 800.567):** 8bitové, ASCII pod 80h, CTRL = písmeno AND 1Fh. Zvláštní klávesy:
  - kurzor C1 ↑, C2 ↓, C3 →, C4 ←;
  - numerická klávesnice B0–B9, tečka AE;
  - ROL 80/81, COPY 82/83, BREAK 84/85;
  - prázdné klávesy 61–63 = D0–D2 (Monitor V5.0/F je používá jako háček, čárku a přepínač CS);
  - klávesa 79 (šipka vlevo nahoru) = 8Dh.
- Web: https://sapi.cz/prislusenstvi/c262-3v2.php

## Consul 259.11

Zdroj: `prislusenstvi\c259-11.php` (kabel ANK-1 KB-13).

- **Kódy:** 7bitový KOI-7 (ASCII, na 24h je ¤). Tabulka je na stránce. Modré klávesy DC1, DC2, HT, VT, ESC,
  FF, CR; černé DEL, ← (BS), ↓ (LF).
- **Zapojení:**
  - Data /D1–/D7, aktivní v 0 → P1-IN0..6.
  - /SC (strobe) → P0-IN0.
  - Klávesa /CLR → P1-IN7.
  - /AC (potvrzení) ← P0-OUT0.
  - /ZVUK ← P0-OUT1 (manuál JPR-1Z chybně uvádí P0-OUT0).
  - /A0 → GND.
- **Chování:** kód drží, dokud ho počítač nepotvrdí (LED „P“ svítí). Autorepeat 10 znaků/s po 1,4–1,6 s.
- **Protokol** je stejný jako u EKL-1. P1-IN7 nese /CLR (v klidu 1), po `CPL` je tedy bit 7 kódu 0.
- **Varianta KOI-8 cs:** ECMON má ovladač „CONSUL 259.11 KOI8 CS2“, kde P1-IN7 nese osmý bit kódu místo /CLR.
  Češtinu posílá s prohozenou velikostí jako EKL-1. V emulátoru je to preset „Consul 259.11 (KOI-8 cs)“
  (`code: "koi8cs"`), klávesa CLEAR v něm není.

## ANK-1

Zdroj: `sapi\ank-1.php`, příloha VII manuálu (`ank-1pri07.png`), tabulky MIKOS V4.0.

- **Matice:** membránová matice 5×8, dekódování dělá software.
- **Zapojení:** P0-OUT D4–D0 vybírají řádky (0 = aktivní, může jich být víc najednou), P0-IN čte 8 sloupců
  (stisknutá klávesa na aktivním řádku = 0).
- **Další výstupy P0-OUT:** D7 LED ERR, D6 bzučák, D5 LED SHIFT (1 = svítí). Pozice jsou podle MIKOS V4.0,
  viz Nejasnosti. Stav LED ukazuje stavový řádek GUI („LED SHIFT“, „LED ERR“, bublina vysvětlí) a MCP
  `machine_info` (`keyboard_status`). LED svítí, když byla rozsvícená kdykoli od minulého pohledu: MIKROBIOS ji
  nechává svítit trvale (MaskLED 20h), Manic Miner s ní přepíná zvuk (`XRI 60h`), na stroji by žhnula.
- **P1-IN:** D7 = klávesa INT, D6 = klávesa T (0 = stisknuto), D5–D0 = 1. RES vede na reset.
- **Matice** (sloupec = bit P0-IN):

| Řádek | b7 | b6 | b5 | b4 | b3 | b2 | b1 | b0 |
|---|---|---|---|---|---|---|---|---|
| D4 | 0 | P | CR | LF | 1 | Q | A | SHIFT |
| D3 | 9 | O | L | SP | 2 | W | S | Z |
| D2 | 8 | I | K | M | 3 | E | D | X |
| D1 | 7 | U | J | N | 4 | R | F | C |
| D0 | 6 | Y | H | B | 5 | T | G | V |

- **Znaky se SHIFT** (MIKOS V4.0): 1 `!`, 2 `?`, 3 `"`, 4 `$`, 5 `^`, 6 `'`, 7 `<`, 8 `>`, 9 `(`, 0 `)`,
  T 08h, Y `/`, U `:`, I `@`, O `#`, P 7Fh, H `+`, J `-`, K `*`, L `=`, B `;`, N `,`, M `.`, G `_`, R `%`,
  D `&`, F `\`, C `[`, V `]`, Q 11h, W 17h, A 10h, S 13h, X 03h. SHIFT+LF = 0Ah, SHIFT+SP = Tab.
- **Emulace:** klávesy PC drží klávesy matice, text z fronty se převádí na ťuky (SHIFT + klávesa pro horní
  znaky, jen velká písmena), každá klávesa aspoň 40 ms stisknutá a 40 ms uvolněná kvůli odrušení zákmitů
  v programu. Režim pro hry (Ctrl+F7) drží klávesy podle polohy. Podrobně v kapitole o volbách výše.
- **Adresy:** u paměťově mapované JPR-1 byly porty na 2400h/2800h, u JPR-1A/Z/ZI jde o I/O porty 01h/02h.
- **Software:** veškerý známý software pro ANK-1 (MIKROBASIC, MIKOS V4) je pro paměťově mapovanou JPR-1,
  na JPR-1ZI tedy zatím nepoběží. Emulace ANK-1 je ověřená testem na úrovni portů (`test_keyboards`).
- **Kompatibilita:** nefunguje s CP/M ani s MikroMonem (jiný protokol).

### ANK-1 schéma

Překreslená schémata EC1045.01 (2010): `BKP\obrazky\ank-1_logika.png` a `ank-1_tlacitka.png` (`BKP` =
`H:\SAPI-1\sapi.cz_BKP_2023-02-22\sapi`), význam bitů v příloze VII `ank-1pri07.png`.
- P0-OUT5 → 7405 → LED SHIFT (zelená), P0-OUT7 → LED ERR (rudá), P0-OUT6 → piezo (zní při 1).
- **Bzučák ANK-1:** MIKROBASIC 2.4 tón dělá sám: po stisku klávesy přepíná D6 asi 64× (asi 750 Hz, 65–80 ms).
  Piezo tedy sleduje úroveň bitu (pasivní měnič). Emulace je stejná: zvuk je průběh D6 (`core/beeper`, druh
  SPEAKER). MIKOS V5 po klávese nepípá.
- Tlačítka INT a T mají RS klopné obvody proti zákmitům (7400) a vedou na P1-IN7 a P1-IN6 (0 = stisknuto).
  RES vede přímo na reset.
- Legendy se SHIFT na klávesách odpovídají tabulce výše. Na T je „←“ (MIKOS: 08h), na P „DEL“.

## ANK-1M2 (Libor Lasota)

**ANK-1M5 = ANK-1M2** (autor): ANK-1M5 (2021) je předchůdce ANK-1M2 (2024), proč má novější verze nižší číslo,
ví jen Libor. Kde se v podkladech, sestavách nebo popisech objeví ANK-1M5 (např. BIOS JPR-1S v
`..\SAPI-karty\JPR-1S\BIOS\bios_jpr1s.txt`: „JPR-1S, BGP-1, ANK-1M5“), bere se jako ANK-1M2. Emulátor přijme
`"type": "ANK-1M5"` jako ANK-1M2, v nabídkách je jen „ANK-1M2“.

Podklady: `..\SAPI-karty\ANK-1M2\` (schéma rev. 1.2 `ank1m2r12_sch.png`, 14. 10. 2024; rev. 1.0 v `ANK-1M2.pdf`;
návod k sestavení, kusovník, 3D tisk krytu). Předchůdce **ANK-1M5** (2021) je popsaný na
<https://www.pmd85.cz/?page_id=1527> (dokumentace `ank1m5.zip`). Liborův komentář tam z 9. 5. 2024 slibuje vrátit
boční tlačítka pro MIKROBASIC, a ta ANK-1M2 má (INT a „T“).

- **Náhrada ANK-1** se 40 spínači Cherry MX a dvěma tlačítky APEM (INT, „T“). Matice, LED, bzučák i tlačítka jsou
  zapojené kolík po kolíku jako u ANK-1, programy pro ANK-1 (MIKROBASIC, MIKOS) fungují beze změny.
- **Připojení:** IDC34 na klávesnici, plochý kabel, na kartě JPR redukce FRB30 na IDC34 do konektoru K2 (X2).
  Kolíky K3 klávesnice = kolíky K2 karty, jen +5 V (K2-22) je v redukci přesměrované na kolíky 33 a 34 (zkraty
  levných samořezných konektorů s GND na kolíku 21).
- **Matice:** výstupy K3-17, 19, 20, 14, 13 = P0-OUT D4–D0 (přes diody BAT41), vstupy K3-25, 24, 26, 23, 29, 27, 28, 30
  = P0-IN D7–D0. Rozložení kláves je stejné jako v tabulce ANK-1.
- **LED a bzučák:** K3-15 (P0-OUT D5) zelená, K3-18 (D7) červená, K3-16 (D6) přes 74LS06 a PNP tranzistor
  magnetický měnič KSSG1201 s potenciometrem hlasitosti (zní při 1, průběh dělá program jako u ANK-1).
- **INT, „T“:** RS klopné obvody (74LS00) a 74LS06 na K3-7 (P1-IN7) a K3-9 (P1-IN6).
- **Konektor joysticku** (DSUB9, Atari): pin 1 → K3-4 (P1-IN1), 2 → K3-6 (IN0), 3 → K3-5 (IN3), 4 → K3-3 (IN2),
  6 → K3-2 (IN4), 9 → K3-8 (IN5), pin 7 +5 V přes vratnou pojistku BK60-030, pin 8 GND. P1-IN D5–D0 jsou tedy
  FR FL vlevo vpravo nahoru dolů (aktivní 0), **stejně jako konektory MPH-1V**. Přiřazení K2 → P1-IN podle
  tabulky Consulu 259.11 (P1-IN0–7 = K2 kolíky 6, 4, 3, 5, 2, 8, 9, 7). Libor uvádí, že port čte i paralelní myš
  s kvadraturními výstupy a dvěma tlačítky.
- **Emulace** (typ `ANK-1M2`, kód ANK-1 s joyportem): parametr `joyport` = `joystick` (výchozí), `mouse_atari`,
  `mouse_amiga` nebo `none`; v konfigurátoru „Klávesnice ANK-1M2: konektor joysticku“, platí hned i za chodu.
  Joystick je první hostitelský ovladač: gamepad, nebo šipky + mezerník
  (Stroj → Zvuk, joystick a myš, **Ctrl+F8**; šipky a mezerník pak nejdou do klávesnice). Myš z myši PC jako
  u MPH-1V (zachycení kliknutím do obrazovky). Joystick i myš jsou společný kód `input/joyport.*` s MPH-1V.
  Volba `software` (`mikrobasic`, `mikos`) platí jako u ANK-1, v menu je jen jedno jméno „ANK-1M2“ (autor).
- **Test:** `test_keyboards` (`test_ank1m2`): MIKROBASIC na ZPS-1 s ANK-1M2, joystick, myš Atari ST, zapojení za chodu.
- **Redukce Redukce34KB01** (`..\SAPI-karty\Redukce34`, rev 2.0, 2022, „Redukce kabelu klávesnice ANK-1M5“):
  FRB30 na kartě JPR ↔ IDC34 klávesnice, navíc vstup RS-232 přes optočlen 6N135 na K2-7 (P1-IN7); K2-1, 9, 10, 12
  nezapojené, takže klávesy INT a T s ní nefungují. Pro BOOT MIKROBIOSu na JPR-1S a JPR-1X, v emulátoru parametr karty CPU
  `k2_adapter` (`docs/desky/JPR-1S.md`). ANK-1M2 má podle návodu obyčejnou redukci (jen +5 V na kolíky 33/34).

### ANK-1 a programy

- **MIKROBASIC 2.4** (tabulky 0DFA/0E22): klávesa vedle CR je BS (08h), SHIFT+P = DEL (7Fh), SHIFT+písmeno =
  stejné písmeno. Backspace na PC = tato klávesa. Esc a Tab MIKROBASIC nemá.
- **MIKOS V4.0:** klávesa vedle CR je LF (ESC), SHIFT+LF = 0Ah, SHIFT+T = 08h, SHIFT+Q/W/A/S/X = 11h/17h/10h/13h/03h.
- Znaky se SHIFT u číslic a ostatních kláves (!?"$^'<>() /:@# +-*=;,._%&\[]) jsou v obou programech stejné.
- Ověřeno s MIKROBASICem na emulované JPR-1 (`tests/test_mikrobasic.cpp`).

## Nejasnosti ANK-1M2

- Který program joyport ANK-1M2 používá (hry, myš)? Emulace je jen podle schématu.
- Má JPR-1 na vstupech P1-IN pull-upy (bez zapojeného joysticku se čte 1)? Emulace: 1.

## HRK-1 (panel APS-5)

Membránová klávesnice HRK-1A s elektronikou HRK-1B (MH 1KK1, 14 kláves 0–9, ←, M, →, R, zámek, watchdog) z měřicí
jednotky APS-5. V emulátoru je to „klávesnice“ `HRK-1` na JPR-1, která zastupuje celé kabely APS-5 na X2 a X3
(logické vstupy a výstupy, tiky z RTC-1, /PFAIL, /RTL). Kódy, mapování kláves PC a zapojení: `APS-5.md`.
