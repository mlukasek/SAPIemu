# JPR-1X – procesorová deska 80C85 (Libor Lasota, rev 1.0, 2025)

**Stav (2026-10-06): emulovaná** (typ `JPR-1X`, třída `Jpr1` v `src/cards/jpr1.*`, procesor v režimu 8085 jádra
`third_party/i8080`, popis procesoru v `docs/8085.md`, test `test_jpr1x`). Podklady: `..\SAPI-karty\JPR-1X\`
(schéma, EAGLE, kusovník, fotky autorovy desky), článek https://www.8bity.cz/2025/sapi-1-jpr-1-board-replacements-jpr-1s-jpr-1x.
Nic neověřeno na HW.

## Emulace

- **Mapa paměti jako JPR-1:**
  - 0000–1FFF ROM, jedna ze dvou bank po 8 KB;
  - 2000–23FF RAM 6116 (1 KB, po zapnutí šum);
  - 2400/2800/2C00 porty P0/P1/P2, každý zrcadlený přes 1 KB;
  - 3000 stav 3214 (jen zápis).
- **Sběrnice:** každý zápis jde i na sběrnici, jako u JPR-1S. Čtení z 3000–FFFF jde ze sběrnice.
- **Procesor 80C85:** takty podle 8085, RIM/SIM, nedokumentované instrukce, příznaky V a K (`docs/8085.md`).
  TRAP a RST 5.5/6.5/7.5 jsou uzemněné, SID je propojený se SOD (RIM bit 7 vrací SOD).
- **TURBO** (propojka, za chodu; tlačítko TURBO na panelu ZDR, menu Stroj, MCP `cpu_turbo`): CPU 4 MHz, jinak
  2 MHz. Sběrnice je vždy 2 MHz. Časová základna emulace je 4 MHz, při 2 MHz trvá takt CPU dvě jednotky.
- **BIOS** (propojka, zachycená na konci RESETu): osazená = dolní banka. Změna za chodu (tlačítko BIOS na panelu
  ZDR) se projeví po RESETu.
- **Obraz ROM** (`rom`):
  - 27C256/28C256 (32 KB): banky na 4000h a 6000h čipu;
  - 27C128 (16 KB): banky na 0000h a 2000h;
  - 28C64 (8 KB): jedna banka pro obě polohy propojky.
  - Výchozí `roms/jpr-1x/JPR-1X_MIKROBIOS_MIKROBASIC.bin` (16 KB, autor): dolní banka MIKROBIOS 1.02 s Falcon
    Monitorem 6.1L (`roms/jpr-1s`), horní banka MIKROBASIC 2.4 (`roms/mikrobasic`).
  - Autor zvláštní ROM pro JPR-1X nemá, používá tytéž BIOSy jako pro JPR-1S a banku volí propojkou BIOS podle
    potřeby (2026-10-06). V nabídce jsou proto i obrazy MIKROBIOSů z `roms/jpr-1s` (8 KB = obě banky).
- **Přerušení:** 3214 má INTE natvrdo v 1, takže povolený požadavek (zápisem stavu na 3000h) se hned zachytí do
  3212 IC7, i při DI. IC7 drží INTR až do INTA a 3214 čeká na další zápis stavu.
  - Stav: /B0–/B2 a /SGS (IC8.1–4, D0–D3) jsou aktivní v nule. Zápis D2–D0 dá stav `~D & 7`, projde úroveň
    vyšší než stav, D3 = 1 pustí všechny úrovně. Je to stejný model jako u JPR-1, odvozený z firmwaru APS-5
    (2026-10-08). Ukázka `diag_int` proto zapisuje 08h (dřív 00h).
  - Vektor: RSTN/7 rozpojená = vždy RST 7, osazená = RST (7 − úroveň).
  - Požadavky: R0 = /INT0, R1 = /INT1 (zároveň P2-IN0/1), klávesa INT na R7 (11-10), R6 (10-R6) nebo nikam
    (z výroby).
- **Parametry:** `rom`, `bios`, `turbo`, `ram`, `int_vector`, `int_key`, `osc_hz` (16 MHz), `keyboard`,
  `k2_adapter` (Redukce34KB01 jako u JPR-1S), `k3` (`diag` = deska diagnostiky rev 2.0, popis v `JPR-1S.md`).
- **Deska diagnostiky na K3:** tlačítko „+“ je P2-IN0, tedy týž vodič jako /INT0 sběrnice (R0), „−“ je P2-IN7 (R7).
  Karta, která táhne /INT0 (BGP-1S se S2 = INT0), se s tlačítkem „+“ plete. V sestavě `jpr1x_bgp1s` je proto
  přerušení BGP-1S vypnuté (nic ho nepoužívá). Ukázkový čítač `media/jpr-1x/diag_int.hex` obsluhuje tlačítka
  přerušením (3214, RSTN/7 rozpojená = RST 7 → MIKROBIOS skočí na 2000h, kam si program dá svou obsluhu).
- **DMA:** emulátor DMA řeší časem (`bus->hold`), na paměť desky DMA nevidí, což odpovídá odpojení desky při HLDA.
- **Sestavy** (TURBO zapnuté jako na autorově desce, jde vypnout za chodu):
  - `machines/zps/ZPS-1_JPR-1X.sapi`: ekvivalent ZPS-1, MIKROBASIC 2.4 z horní banky, REM-1 jen s RAM, AND-1,
    DSM-1, ANK-1;
  - `machines/jpr1x_bgp1s.sapi`: MIKROBIOS 1.02 z dolní banky, RAM-1/48K, BGP-1S, ANK-1M2 s redukcí. Manic Miner
    se nahraje jako u JPR-1S.
- **Ověřeno** (`test_jpr1x`):
  - MIKROBASIC počítá;
  - smyčka FOR je s TURBO zhruba dvakrát rychlejší;
  - MIKROBIOS dává stejný obraz jako na JPR-1S;
  - přerušení INT1 z AND-1 zachycené pod DI skočí po EI na RST 7.
  - Jádro 8085: `test_i8080` a `test_8085exm` (CRC skutečného D8085AH-1).
- **Debugger:** kód ukazuje mnemotechnikou Z80 jako u JPR-1. RIM, SIM a nedokumentované instrukce 8085 mají
  jména Intel (DSUB, LDHI…). U 8080 jsou ty opkódy aliasy (`NOP*`, `JP*`, `CALL*`, `RET*`).

Rozbor pro emulaci (2026-10-06). Moderní klon JPR-1 s 80C85, mapa paměti a porty jako JPR-1.

Zdroje (vše jen ke čtení, pracovní kopie v `work\jpr1x\`):
- [SCH] `..\SAPI-karty\JPR-1X\JPR-1X_sch.pdf` (4 listy, DPLA0083JPX rev 1.0, 13. 4. 2025);
- [NET] netlist a partlist z kopie `JPR-1X.sch` přes `eaglecon` (`work\jpr1x\n.txt`, `p.txt`, `pins.txt`);
- [BRD] `JPR-1X.brd` převedený na XML (`work\jpr1x\xb.brd`, souřadnice plošek skriptem `brd.py`);
- [DESC] `DESCRIPTION` (cp1250);
- [BOM] `kusovník JPR-1X rev. 1.0.xlsx`, `JPR-1X_Mouser_parts.xlsx`;
- [FOTO] `Foto\JPR-1X_Martin_front.jpg`, `_back.jpg` (autorova osazená deska), výřezy `work\jpr1x\c_*.png`;
- pro srovnání `docs/desky/JPR-1.md`, `JPR-1S.md`, `JPR-1V.md`, `ARB-1C.md`, `src/cards/jpr1.*`, manuál JPR-1
  `..\SAPI.CZ\sapi\jpr-1.php`.

Zápis: `síť` = jméno sítě z netlistu, `ICn.p` = vývod p obvodu ICn.

## 1. Obvody

| Pozice | Obvod | Funkce |
|---|---|---|
| IC13 | 80C85 (foto: Mitsubishi M5M80C85AP-2, BOM: OKI M80C85AH) | CPU |
| IC26 | 74LS373 | latch A0–A7 (ENC = ALE, OC = GND) |
| IC14 | 74LS245 | oddělovač dat CPU (strana B, sítě DI0–DI7) ↔ sběrnice (strana A, D0–D7) |
| IC27, IC10 | 74LS138 | dekodéry adres |
| IC11 | 27C256 / 28C256 (BOM: AT28C256) | ROM 0000–1FFF, dvě banky |
| IC9 | 6116 (foto: UM6116-2) | RAM 1 KB |
| IC1–IC6 | 3212 (MH3212) | porty P0–P2 |
| IC7 | 3212 | vektor přerušení |
| IC8 | 3214 (MH3214) | řadič priority přerušení |
| IC15 74LS32, IC17 74LS00, IC18/IC12 74LS04 | | řídicí signály |
| IC16 | 74LS112A | generátor STSTB |
| IC19, IC20, IC24 | 74LS125 | budiče řídicích signálů sběrnice |
| IC21 | MAX707 | reset |
| QG1, IC22 74HCT175, IC23 74LS00, IC25 74LS74 | | hodiny, TURBO, latch BIOS |
| Q1, D1 | BC547, LED | LED na P1-OUT7 |

Na fotce jsou IC10 v patici pod IC11 a IC12, IC14 pod patici CPU (potisk „IC11/IC10“, „IC13/IC12“, „/IC14“).

## 2. Řídicí logika CPU (list 1)

- `/IMR` = `/RD` OR `IO/M` (IC15.13, IC15.12 → IC15.11): čtení paměti.
- `/IMW` = `/WR` OR `IO/M` (IC15.4, IC15.5 → IC15.6): zápis do paměti.
- `/IIOR` = NAND(`IRD`, `IO/M`) (IC17.9, IC17.10 → IC17.8), `IRD` = NOT `/RD` (IC18.9 → 8).
- `/IIOW` = NAND(`IO/M`, `IWR`) (IC17.4, IC17.5 → IC17.6), `IWR` = NOT `/WR` (IC18.5 → 6).
- `STATUS11` = NAND(S0, S1) (IC17.1, IC17.2 → IC17.3); `/IM1` = `STATUS11` OR `IO/M` (IC15.9, IC15.10 → IC15.8).
  **/M1 je aktivní během celého strojového cyklu výběru operačního kódu** (S1 = S0 = 1, IO/M = 0). Při INTA
  (IO/M = 1) /M1 aktivní není (u 8080 je M1 i ve stavovém slově INTA).
- `/READ` (síť pojmenovaná zavádějícím způsobem) = NAND(`/IINTA`, `/RD`) (IC17.12, IC17.13 → IC17.11) → IC14.1 DIR.
  Výstup je 1 při čtení (`/RD` = 0) nebo INTA (`/IINTA` = 0). 74LS245 DIR = 1 je A → B, tedy **sběrnice → CPU**.
  Jindy (DIR = 0) budí CPU sběrnici (adresa v T1, zápis, nečinnost).
- IC14.19 /OE = `HLDA`: buffer je trvale zapnutý, vypne ho jen HLDA.
- Low adresa: IC26 má vstupy 1D–8D na **sběrnicové** straně (sítě D0–D7), ENC = ALE (IC26.11), OC = GND (IC26.1).
  Během ALE je DIR = 0 a AD0–AD7 jdou přes IC14 na D0–D7 a do latche. Spodní bajt adresy se tedy v T1 objeví na
  datové sběrnici K1. **IC26 budí A0–A7 trvale, i při HLDA.**
- A8–A15 jdou z CPU (IC13.21–28) přímo na K1 bez budiče. 80C85 je při HLDA odpojí.

### Budiče sběrnice (74LS125, aktivní /G = 0)

| Signál K1 | Pin K1 | Zdroj | /G | Při HLDA |
|---|---|---|---|---|
| /MR | 9 | `/IMR` (IC19.5 → 6) | HLDA | 3. stav |
| /MW | 11 | `/IMW` (IC19.12 → 11) | HLDA | 3. stav |
| /IOR | 10 | `/IIOR` (IC19.9 → 8) | HLDA | 3. stav |
| /IOW | 12 | `/IIOW` (IC19.2 → 3) | HLDA | 3. stav |
| /AEN | 13 | `HLDA` (IC20.2 → 3), **neinvertováno**: /AEN = 1 při DMA | GND | budí |
| /RES | 8 | `/CLR` (IC20.5 → 6) | GND | budí |
| /HLDA | 5 | `/IHLDA` = NOT HLDA (IC18.11 → 10, IC20.9 → 8) | GND | budí |
| STSTB | 4 | `ISTSTB` (IC20.12 → 11), aktivní v 1 | GND | budí |
| /INTA | 61 | `/IINTA` = IC13.11 (IC24.2 → 3) | GND | budí |
| PHI2TTL | 60 | `BUSCLK` (IC24.5 → 6) | GND | budí |
| /M1 | 6 | `/IM1` (IC24.9 → 8) | GND | budí (IO/M má při HOLD pull-up → /M1 = 1) |

IC24D (IC24.12 = GND, /G = GND, výstup IC24.11 nezapojen) je nevyužité hradlo.

### Ostatní piny K1 (TY 517 62 11)

- 1 `/RTL` → IC21.1 /MR (MAX707, vnitřní pull-up);
- 2 `/HOLD` (pull-up R7.3 10k) → IC12.13 → IC12.12 `HOLD` → IC13.39;
- 3 `READY` (RDY, pull-up R7.2 10k) → IC13.35 přímo;
- 7: ve schématu popsán „/RFSH“, ale **nezapojen** (žádná síť). [DESC]: „Na pinu 7 sběrnice není signál INTE“.
  /RFSH ani INTE deska nedává;
- 14 (/DEN), 21–22 (MAP1, MAP2), 47–52, 55–58: nezapojeno;
- 15–18 +5 V, 19–20, 53–54 GND;
- 23–30 A9 A15 A11 A14 A13 A12 A10 A8, 31–38 D4 D3 D6 D5 D2 D7 D0 D1, 39–46 A1 A0 A3 A2 A5 A4 A7 A6 (jako ARB-1);
- 59 `/INT1` = síť `P2IN1`, 62 `/INT0` = síť `P2IN0` (pevně, bez propojky).

Pull-upy R7 (SIL6 10k): R7.2 READY, R7.3 /HOLD, R7.4 /RD, R7.5 /WR, R7.6 IO/M (při HOLD drží /IMR, /IMW, /IIOR,
/IIOW neaktivní).

### STSTB (IC16 74LS112A)

- IC16A: CLK = ALE (IC16.1, sestupná hrana), J = VCC, K = GND, Q = `ISTSTB` (IC16.5), /R = `STSTB_NULL` (IC16.15).
- IC16B: CLK = CLKOUT (IC16.13), J = `ISTSTB`, K = `/ISTSTB` (funguje jako D), /Q = `STSTB_NULL` (IC16.7).
- Sestupná hrana ALE nastaví ISTSTB = 1, nejbližší sestupná hrana CLKOUT ho přes /R shodí. **STSTB je kladný
  impuls kratší než 1 takt CLKOUT na začátku každého strojového cyklu s ALE** (paměť, I/O, INTA).

## 3. Dekódování adres a paměť (list 2)

- **IC27** 74LS138: A = A13 (IC27.1), B = A14 (IC27.2), C = A15 (IC27.3), G1 = VCC, **G2A = A15** (IC27.4),
  **G2B = HLDA** (IC27.5). Funguje jen pro A15 = 0 a mimo DMA. Bez IO/M.
  - Y0 (IC27.15) = `/ROMCS` 0000–1FFF, Y1 (IC27.14) = `/2000CS` 2000–3FFF, Y2/Y3 (4000–7FFF) nezapojeny.
- **IC10** 74LS138: A = A10, B = A11, C = A12, G1 = VCC, G2A = G2B = `/2000CS`.
  - Y0 (IC10.15) `/RAMCS` 2000–23FF, Y1 (IC10.14) `/S0` 2400–27FF, Y2 (IC10.13) `/S1` 2800–2BFF,
    Y3 (IC10.12) `/S2` 2C00–2FFF, Y4 (IC10.11) `/3000CS` 3000–33FF, Y5–Y7 (3400–3FFF) nezapojeny.
- **ROM IC11:** A0–A12 z adresy, **A13 (IC11.26) = `/BIOS`**, **A14 (IC11.27) = VCC**, VPP (IC11.1) = VCC,
  /CE = `/ROMCS`, /OE = `/IMR`.
  - U 27C256/28C256 se tedy používají **horní dvě banky**: A13 = 0 → offset 4000–5FFF, A13 = 1 → 6000–7FFF.
    Poznámka ve schématu: „možno osadit 27C128, 27C256, 28C64, 28C256 (u 27C256 a 28C256 naprogramovat
    poslední 2 banky) (u 28C64 nelze přepínat alternativní banky)“.
  - 27C128: vývod 26 = A13, vývod 27 = /PGM (VCC) → banky na 0000 a 2000.
  - 28C64: vývod 26 NC, 27 /WE na VCC → jedna banka, propojka BIOS nemá vliv. /WE je u všech typů na VCC,
    **ROM se za provozu nedá přepsat**.
- **RAM IC9 6116:** /CS = `/RAMCS`, **A10 (IC9.19) je spojený s `/RAMCS`** → při výběru vždy 0. Využit je jen
  **1 KB** (dolní polovina 6116), na 2000–23FF bez zrcadlení. /OE = `/IMR`, /WE = `/IMW`. Adresní a datové vodiče
  6116 jsou přeházené (A0 → IC9 A7 atd.), na funkci to nemá vliv.
- Všechny výběry na desce jsou hradlované /IMR nebo /IMW (IO/M = 0), I/O cykly a INTA desku nevyberou.

### Mapa paměti s chováním čtení a zápisu

Klíčové: obvody desky sedí **na sběrnicové straně** IC14 (sítě D0–D7 = K1). CPU čte vždy přes IC14 ze sběrnice
K1. Co nebudí deska, budí sběrnice. **Každý zápis jde na sběrnici** (IC14 směrem ven, /MW na K1 vždy), stejně
jako u JPR-1S. /MR a /MW jdou na K1 i pro adresy na desce (nejsou hradlované dekodérem).

| Adresa | Čtení | Zápis |
|---|---|---|
| 0000–1FFF | ROM IC11 (banka podle BIOS) | jen sběrnice (ROM nemá /WE) |
| 2000–23FF | RAM 6116 (1 KB) | RAM + sběrnice |
| 2400–27FF | P0-IN (IC6), zrcadleno po 1 KB | P0-OUT (IC5) + sběrnice |
| 2800–2BFF | P1-IN (IC4), zrcadleno | P1-OUT (IC3) + sběrnice |
| 2C00–2FFF | P2-IN (IC2), zrcadleno | P2-OUT (IC1) + sběrnice |
| 3000–33FF | **sběrnice** (3214 se číst nedá, deska nic nebudí) | stav 3214 (/ECS) + sběrnice |
| 3400–37FF | sběrnice („nevyužito“) | sběrnice |
| 3800–3FFF | sběrnice (AND-1 2 KB) | sběrnice |
| 4000–FFFF | sběrnice (48 KB) | sběrnice |
| I/O 00–FF | sběrnice (na desce žádný I/O port) | sběrnice |
| INTA | vektor z IC7 (na desce), /INTA jde i na K1 | – |
| při HLDA | deska nevybrána, IC14 odpojen | – |

- Čtení z adres desky budí datovou sběrnici K1 obvodem desky. Pokud by na téže adrese odpovídala i karta na
  sběrnici (např. RAM přes celých 64 KB), byl by to konflikt. Karta se sběrnice čtení vidí (/MR).
- Bez osazené ROM nebo RAM (prázdná patice) přijdou data ze sběrnice (např. REM-1 v ZPS-1).
- Emulace JPR-1 dnes pro čtení 3000–33FF vrací FFh. U JPR-1X má jít na sběrnici.
- **Úprava na autorově desce:** na zadní straně je připájený rezistor (modrý, hodnota z fotky nečitelná)
  mezi IC14.20 (VCC) a nejspíš IC14.5 (A4 = síť **D3**), tedy pull-up datového bitu D3 sběrnice. Určeno z výřezu
  `c_res2.png` a souřadnic plošek [BRD]. Vývod 5 je odhad, může to být sousední 4 (D2) nebo 6 (D4). **Ověřit
  s autorem, k čemu slouží.**

## 4. Porty P0–P2 (list 3)

3212: /DS1 + DS2 = výběr, MD = 1 výstupní režim, MD = 0 vstupní (STB = 1 → latch průchozí, sestupná hrana STB
zachytí data a nastaví /INT = 0, výběr ho shodí). /CLR všech 3212 = `/CLR` (reset → výstupy 00h).
Bity jsou rovně (Dn ↔ bit n portu), páry vývodů 22↔21, 20↔19, 3↔4, 5↔6, 9↔10, 18↔17, 16↔15, 7↔8.

| Port | Adresa | IC | /DS1 | DS2 | MD | STB | Data | /INT |
|---|---|---|---|---|---|---|---|---|
| P0 OUT | 2400 W | IC5 | `/S0` | `IMW` | `P0_STB` (pull-up R5.2) | `P0_STB` (= MD, 1) | K2 | `P0INTO` K2-12 |
| P0 IN | 2400 R | IC6 | `/S0` | `IMR` | GND | `P0_STB_OUT` (pull-up R5.4, **na konektor nevede**) → průchozí | K2 | nezapojeno |
| P1 OUT | 2800 W | IC3 | `/S1` | `IMW` | `LOG1_3212A` (R4.2) | `P1STBO` K3-19 (pull-up R3.11) | K3 | `P1INTO` K3-22 |
| P1 IN | 2800 R | IC4 | `/S1` | `IMR` | GND | `P1STBI` **K2-10** (pull-up R4.11) | K2 | `P1INTI` K2-1 |
| P2 OUT | 2C00 W | IC1 | `/S2` | `IMW` | `LOG1_3212A` | `P2STBO` K3-9 (pull-up R3.2) | K3 | `P2INTO` K3-1 |
| P2 IN | 2C00 R | IC2 | `/S2` | `IMR` | GND | `LOG1_3212A` (1) → průchozí | K3 | nezapojeno |

- P0-IN a P2-IN jsou průchozí, P1-IN se zachytává signálem P1-STB z K2-10 (bez něj pull-up → průchozí).
- /INT výstupy 3212 vedou jen na konektory, ne do CPU ani do 3214.
- Pull-upy 4k7: R5 = P0-IN0..7, P0_STB, P0_STB_OUT; R4 = P1-IN0..7, P1-STB (K2-10), LOG1_3212A; R3 = P2-IN0..7,
  P2-STB (K3-9), P1-STB (K3-19). Nezapojený vstup čte 1.
- Výstupní registry nejdou číst (čtení adresy dává vstupní 3212).
- **K2 a K3 jsou kolík po kolíku shodné s JPR-1S** (tabulky v `JPR-1S.md`): K2 = P1-IN, P0-OUT, P0-IN, K3 =
  P2-OUT, P2-IN, P1-OUT. K2-11, 21 a K3-11, 21 = GND.
- **K2-22 = +5 V (KBDPWR)** [DESC]. U původní JPR-1 je X2-22 **P0-STB** (vstup STB portu P0, manuál
  `jpr-1.php`). Na zadní straně jsou potiskem označené body „K2/22 (KBDPWR)“ (ploška K2-22 se značkou
  přerušitelného spoje na cestě +5 V) a „P0-STB“ (šipka na plošku **R5.4 = síť `P0_STB_OUT` = STB vstupního
  3212 IC6**). Pro původní zapojení se přeruší cesta KBDPWR a P0-STB se drátem propojí na K2-22. Na autorově desce
  žádný drát není, tedy výchozí stav: P0-IN průchozí, K2-22 = +5 V.
- **LED D1:** `P1OUT7` → R1 10k → báze Q1 BC547 (emitor GND), kolektor → katoda D1, anoda → R2 1k → VCC.
  **P1-OUT7 = 1 svítí** (po resetu 3212 vynulovaný → nesvítí). Stejně jako JPR-1S.

## 5. Přerušení (list 3)

### 3214 IC8

| Vstup | Síť | Zdroj |
|---|---|---|
| /R0 (IC8.15) | `P2IN0` | K1-62 **/INT0**, K3-20, P2-IN0 |
| /R1 (IC8.16) | `P2IN1` | K1-59 **/INT1**, K3-17, P2-IN1 |
| /R2–/R5 (IC8.17–20) | `P2IN2`–`P2IN5` | K3-12, 14, 13, 18 |
| /R6 (IC8.21) | `/R6` | střed propojky P2IN6-R6-10 |
| /R7 (IC8.22) | `P2IN7` | K3-16, propojka 11-10-R6 (pozice 11) |
| /B0–/B2 (IC8.1–3) | D0–D2 | stav při zápisu na 3000h |
| /SGS (IC8.4) | D3 | |
| /ECS (IC8.23) | `/ECS` = `/IMW` OR `/3000CS` (IC15.1, IC15.2 → IC15.3) | zápis do 3000–33FF |
| CLK (IC8.6) | `CLKOUT` (IC13.37) | 2 nebo 4 MHz podle TURBO |
| INTE (IC8.7), ETLG (IC8.13) | `LOG1A` (pull-up R6.5) | **trvale 1** (80C85 nemá výstup INTE) |
| ENLG (IC8.14) | – | nezapojen |
| /ELR (IC8.11) | `RST7`: pull-up R6.8, propojka RSTN/7 na GND | |
| A0–A2 (IC8.8–10) | `A0_3214`–`A2_3214`, pull-upy R6.6, R6.3, R6.2 | → IC7 |
| /INT (IC8.5) | `/I3214`, pull-up R6.10 | → IC23A (IC23.1, 2 → 3) → `I3214OUT` → IC7.11 STB |

### Propojky zdrojů R6 a R7 (pájecí SJ2W na zadní straně)

- **„11-10-R6“**: vývod 1 = `P2IN7` (= /R7, „11“ = kolík 11 původní JPR-1), vývod 2 (střed) = `P1IN7`
  (K2-7, klávesa INT, „10“), vývod 3 = `PROPOJ_R6` („R6“).
  - 11-10 = klávesa INT na R7 (jako propojka 10-11 původní JPR-1, ta se dodávala propojená);
  - 10-R6 = klávesa INT na spojku k druhé propojce.
- **„P2IN6-R6-10“**: vývod 1 = `PROPOJ_R6` („10“), vývod 2 (střed) = `/R6`, vývod 3 = `P2IN6` (K3-15).
  - R6-P2IN6 = /R6 z P2-IN6 (K3-15);
  - R6-10 = /R6 ze spojky (tedy z klávesy INT při 10-R6).
- Schéma: „**PROPOJE Z VÝROBY: R6-P2IN6**. DOPORUČENO: R6-10 (P2IN6-R6-10), 10-R6 (11-10-R6)“. Z výroby je tedy
  /R6 = P2-IN6 a klávesa INT není připojená k žádnému požadavku (11-10-R6 rozpojená). Doporučené: klávesa INT na R6.
- /R6 nemá vlastní pull-up. Při rozpojené P2IN6-R6-10 visí ve vzduchu (vstup TTL → čte se jako 1, bez požadavku).
- [DESC]: „Radič přerušení 3214 … včetně výrobcem doporučených úprav“. Které úpravy to jsou (pull-upy A0–A2 a /INT,
  INTE na 1, přesun klávesy INT na R6?), není napsané.

### Vektor IC7 (3212) a cesta do CPU

- IC7: /DS1 = `/IINTA` (IC7.1), DS2 = `LOG1A` (IC7.13), MD = GND (vstupní režim), STB = `I3214OUT` (IC7.11),
  /CLR = `/CLR`.
- Vstupy: I0, I1, I2, I6, I7 = 1 (`LOG1A`), I3 = `A0_3214`, I4 = `A1_3214`, I5 = `A2_3214`.
  Vektor = **11 A2 A1 A0 111 = RST n**, n = A2A1A0 z 3214.
- **RSTN/7** (JP1Q, potisk „RST n/7“): osazená → /ELR = 0 → 3214 dává A0–A2 → RST podle úrovně; rozpojená →
  /ELR = 1 → výstupy A0–A2 (otevřený kolektor) vypnuté, pull-upy dají 111 → **vždy RST 7** (0038h).
  [DESC]: „Možnost volby vektoru přerušení podle zdroje nebo nucené RST7“.
- Podle emulace JPR-1 (předpoklad, tamtéž v Nejasnostech) dává 3214 A = doplněk úrovně, tedy RST (7 − úroveň):
  R0 (/INT0) → RST 7, R1 (/INT1) → RST 6, R6 → RST 1, R7 → RST 0. /INT0 tak skončí na 0038h v obou polohách RSTN/7.
- `/INTR` = IC7.23 /INT → IC12.1 → IC12.2 `INTR` → IC13.10 INTR.
- Průběh (podle zapojení 3214 + 3212 z MCS-80): 3214 dá krátký impuls /INT (synchronní s CLK) a nastaví si vlastní
  zákaz přerušení. STB IC7 = negovaný /INT: po dobu impulsu je latch průchozí, sestupná hrana STB zachytí vektor a
  nastaví /INT IC7 = 0 → INTR = 1. CPU při INTA přečte vektor (IC7 vybraný /INTA, IC14 DIR = sběrnice → CPU) a
  výběr IC7 shodí jeho /INT. 3214 přijme další požadavek až po zápisu stavu na 3000h (/ECS).
- **Rozdíl proti JPR-1 (8080):** INTE 3214 je trvale 1. Požadavek se proto zachytí (vektor v IC7, INTR drží
  IC7) i když má CPU přerušení zakázané. Po EI se přijme, i kdyby zdroj mezitím zmizel. Emulace JPR-1 dnes
  vyhodnocuje požadavky až v okamžiku přijetí.
- Vektor ze sběrnice nejde: IC7 odpovídá na každé INTA (DS2 = 1). /INTA jde ale i na K1-61, karta na sběrnici
  by s IC7 kolidovala.
- **80C85:** TRAP (IC13.6), RST7.5 (7), RST6.5 (8), RST5.5 (9) jsou na GND (nepoužité). **SID (IC13.5) je spojený
  se SOD (IC13.4)** (síť `SID_SOD`): RIM bit 7 vrací stav výstupu SOD, jinam SOD nevede.

## 6. HOLD/HLDA (DMA), READY, reset

- **HOLD:** K1-2 `/HOLD` (pull-up R7.3) → IC12F → `HOLD` IC13.39.
- **HLDA (IC13.38)** vypne:
  - IC27 G2B → žádný výběr na desce (ROM, RAM, porty, /ECS);
  - IC14 /OE → datový buffer;
  - IC19 /G → /MR, /MW, /IOR, /IOW do třetího stavu;
  - na K1 dá `/HLDA` = 0 (IC18E, IC20C) a `/AEN` = 1 (IC20A).
- **Nevypne:** IC26 (OC = GND) dál budí A0–A7, IC24 budí /M1, /INTA, PHI2TTL, IC20 /RES, STSTB. Adresy A0–A7 by
  kolidovaly s DMA masterem, pokud sběrnice neodpojí slot CPU (ARB-1C JP3 2-3: /AEN vypne buffery 8282 slotu 1,
  viz `ARB-1C.md`). [DESC] tvrdí „plně funkční DMA“. **Nejasnost.**
- **READY:** K1-3 RDY přímo na IC13.35, pull-up R7.2. Čekací stavy dává jen sběrnice, **propojku pro vlastní
  čekací stav deska nemá** [DESC] (JPR-1: 12-14).
- **Reset:** MAX707 IC21: /MR (1) = K1-1 `/RTL`, PFI (4) = GND (/PFO nevyužit), /RST (7) → `/RSTI` IC13.36.
  Power-on a /RTL dají impuls cca 200 ms. RSTO (IC13.3, síť `CLR`) → IC18F → `/CLR`: /CLR všech 3212, /RES na
  K1-8 (IC20B), CLK latche BIOS (IC25B.11).

## 7. Hodiny, TURBO, BIOS (list 4)

- QG1 16 MHz (BOM MXO45HST-3C-16.0, „není třeba 3State“) → `OSC` IC22.9 CLK. IC22 74HCT175 (/CLR = LOG1C),
  „nutno dodržet HCT, jinak nepojede CPU“ [BOM].
- Johnsonův čítač: D2 (IC22.5) = /Q3, D3 (IC22.12) = Q2 → Q2, Q3 = 4 MHz.
- **Sběrnice:** `BUSCLK_X2` = /Q2 (IC22.6) → IC25A CLK, IC25A D = /Q (dělička 2) → `BUSCLK` (IC25.5) 2 MHz →
  IC24B → **PHI2TTL K1-60 = trvale 2 MHz**.
- **CPU:** D4 (IC22.13) = `/TURBO` (pull-up R6.11, propojka TURBO na GND), Q4 = `175.NT`, /Q4 = `175.T`.
  - IC23B: `CPU4M` = NAND(NT, Q3); IC23D: `CPU8M` = NAND(T, /Q1); IC23C: D1 = NAND(CPU4M, CPU8M).
  - TURBO rozpojená (NT = 1): D1 = Q3 → Q1 = 4 MHz → X1 80C85 → **CLK 2 MHz**.
  - TURBO osazená (T = 1): D1 = /Q1 → Q1 = 8 MHz → **CLK 4 MHz**.
  - `CPUCLK_X2` = Q1 (IC22.2) → IC13.1 X1, X2 (IC13.2) nezapojen.
  - TURBO se vzorkuje každou hranou 16 MHz → **přepíná se za chodu**, synchronně [DESC: „Přepínání frekvence
    procesoru 2MHz/4MHz za chodu, frekvence sběrnice trvale 2MHz“]. Stejné jako JPR-1V.
- **BIOS:** `/SELBIOS` (pull-up R6.9, propojka BIOS na GND) → IC25B D (IC25.12), CLK = `/CLR` (vzestupná hrana =
  konec resetu), Q (IC25.9) = `/BIOS` → ROM A13. **Zachytí se při resetu**, změna za chodu se projeví až po
  RESETu. Osazená → A13 = 0 → u 27C256/28C256 offset 4000h (předposlední banka), rozpojená → A13 = 1 → 6000h.
- **PIN1** (SJ2W, zadní strana, potisk „LO – HI“): vývod 1 = VCC (HI), 2 = `PIN1PWR` → QG1.1 (NC/TS),
  3 = GND (LO). Pro oscilátory s vývodem 1 = povolení výstupu. Osazený MXO45HST ho nepotřebuje. Na emulaci vliv nemá.

## 8. Stav propojek na autorově desce (fotky)

| Propojka | Kde | Fotka | Význam |
|---|---|---|---|
| BIOS | přední, lišta u IC24/IC25 | **nejspíš osazená** (čepička) | banka A13 = 0 |
| TURBO | přední, nad BIOS | **nejspíš osazená** (čepička s delším úchytem?) | CPU 4 MHz |
| RSTN/7 („RST n/7“) | přední, mezi IC7 a IC8 | **nejspíš rozpojená** (vidět dva kolíky, bez čepičky) | vždy RST 7 |
| 11-10-R6 | zadní, u K3 | vypadá nepropojená (z výroby) | klávesa INT nikam |
| P2IN6-R6-10 | zadní | vypadá jako z výroby | /R6 = P2-IN6 |
| PIN1 LO/HI | zadní, roh | nepropojená | oscilátor vývod 1 volný |
| K2/22, P0-STB | zadní | beze změny | K2-22 = +5 V |

Rozlišení fotky na jistotu nestačí, BIOS a TURBO tvoří jeden dlouhý černý celek přes obě lišty. **Ověřit
s autorem.** BOM uvádí jen dvě lišty JP1x2 (S1, S2), schéma ale má tři JP1Q (BIOS, TURBO, RSTN/7).

## 9. Srovnání s JPR-1, JPR-1S, JPR-1V

**Shodné s JPR-1 (8080) a emulací `Jpr1`:**
- mapa paměti 0000–1FFF ROM, 2000–23FF RAM 1 KB, P0/P1/P2 na 2400/2800/2C00 zrcadlené po 1 KB, 3214 na
  3000–33FF jen zápis (zrcadlený), 3400–3FFF a 4000–FFFF sběrnice, I/O jen sběrnice;
- 3214 + 3212 s RST n, R0 = /INT0, R1 = /INT1, P2-IN = požadavky, propojka klávesy INT na R7 („11-10“ = 10-11);
- obsazení K2/K3 jako X2/X3 JPR-1 (kromě K2-22), LED na P1-OUT7;
- instrukční sada 8080 (8085 je nadmnožina).

**Odlišné:**
1. **CPU 80C85:** jiné počty taktů (MOV r,r 4 místo 5, INX/DCX 6, PUSH 12, CALL 18, Rcc 6/12, …), RIM/SIM
   (SID = SOD), nedokumentované instrukce 8085 (DSUB, ARHL, RDEL, LDHI, LDSI, SHLX, LHLX, JNK/JK, RSTV na
   08, 10, 18, 28, 38, CB, D9, DD, ED, FD; na 8080 jsou to aliasy NOP/JMP/CALL/RET), příznaky V a K v F. Jádro
   `third_party/i8080` 8085 neumí.
2. **ROM** 27C256/28C256 se dvěma 8KB bankami (BIOS, latch při resetu) místo 2708/2716 v paticích.
3. **RAM** 6116 (1 KB využit) místo 2× 2114.
4. **Hodiny** 16 MHz: CPU 2/4 MHz (TURBO za chodu), sběrnice 2 MHz. JPR-1: 18 MHz / 9 = 2 MHz.
5. **Bez čekacího stavu** (JPR-1 12-14).
6. **RSTN/7:** volba nuceného RST 7. **INTE 3214 = 1**, požadavek se zachytí i při DI. K1-7 INTE chybí.
7. **Klávesa INT** přes 11-10-R6 + P2IN6-R6-10 na R7 nebo R6, z výroby nikam (JPR-1 z výroby 10-11 propojené).
8. **DMA** s odpojením desky, /AEN = HLDA, /HLDA; 74LS125 na řídicích signálech.
9. **Data:** obvody desky na straně sběrnice, všechny zápisy jdou na sběrnici (jako JPR-1S). Čtení 3000–33FF ze
   sběrnice.
10. **K2-22 = +5 V** místo P0-STB (s možností vrátit), P0-IN průchozí.
11. **/M1** = úroveň po celý cyklus výběru operačního kódu (ne při INTA). STSTB z ALE.
12. Reset MAX707 (bez RC).

**Proti JPR-1S:** společné K2/K3, +5 V na K2-22, P2-IN s pull-upy, LED, zápisy na sběrnici, 8KB ROM. JPR-1S navíc
banky RAM 4000–FFFF, nemá 3214 (INT_SRC/INT_ADR, 3000–3FFF sběrnice), nemá DMA, 8080.

**Proti JPR-1V:** TURBO stejné (74HCT175, za chodu, sběrnice 2 MHz, ale oscilátor 16 MHz a CPU = osc/8 nebo osc/4).
BIOS: JPR-1V vede propojku na A11 přímo, JPR-1X ji zachytí při resetu (výsledek stejný: projeví se po RESETu).

## Nejasnosti k ověření (HW / autor)

1. ~~Stav propojek~~ (autor 2026-10-06): TURBO zapnuté, BIOS podle potřeby, RSTN/7 (dvoukolíková lišta
   s potiskem „RST n/7“ na přední straně mezi IC7 3212 a IC8 3214, vpravo) rozpojená = vždy RST 7, jako
   v sestavách. Pájecí propojky 11-10-R6, P2IN6-R6-10 na zadní straně z výroby (klávesa INT nikam, R6 =
   P2-IN6), jako v sestavách (`int_key` = `none`).
2. ~~Obraz ROM~~ (autor): žádná zvláštní ROM, tytéž BIOSy jako pro JPR-1S.
3. Rezistor na zadní straně: podle článku na 8bity.cz pull-up 4k7 na D3, protože některé Mitsubishi 8085 při čtení
   prázdné adresy vracely dříve zapsaná data (jiní výrobci spodní bajt adresy) a Monitor pak špatně zjišťoval
   paměť. Emulátor vrací z prázdné sběrnice FFh. Ověřit vývod a hodnotu.
4. Pořadí RST pro úrovně 3214 (předpoklad RST 7 − úroveň), význam /B0–/B2 a /SGS (aktivní v 0?) – společné s JPR-1.
5. Je /INT 3214 skutečně krátký impuls (zachycení v IC7) a ovlivňuje /ELR jen výstupy A0–A2, nebo i /INT?
6. DMA: IC26 budí A0–A7 i při HLDA. Funguje DMA jen s ARB-1C (JP3 2-3, /AEN odpojí slot CPU)?
7. Kam vede /RTL K1-1 v autorově sestavě (tlačítko reset)?
8. Které „výrobcem doporučené úpravy“ 3214 [DESC] myslí a proč je doporučená klávesa INT na R6 místo R7
   (R7 = RST 0 = restart?).
9. Snese M5M80C85AP-2 4 MHz (X1 8 MHz)? Článek: všechny vyzkoušené 8085 (Toshiba, Intel, OKI, Mitsubishi) běží na
   4 MHz spolehlivě.
10. Procesor 8085: příznaky V a K, DSUB/ARHL a časování přijetí RST 7.5/TRAP (`docs/8085.md`, Nejasnosti).
