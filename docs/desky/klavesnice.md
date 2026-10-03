# Klávesnice (konektor X2 na JPR-1ZI)

Všechny klávesnice se připojují na porty 3212 desky JPR-1ZI: P0 je port 01h, P1 je port 02h.

Výchozí klávesnicí emulátoru je **EKL-1** (PS/2 → SAPI) a chová se navenek jako **Consul 262.3**. Později přibudou
Consul 259.11 a ANK-1. Vnitřek EKL-1 (AT89C2051) se neemuluje, jen chování na portech.

## EKL-1

Zdroje:
- `..\EKL-1\Firmware\EKL-1_SOFT_updated\KEYB_REPAIR.a51` (rutina OUTPUT a tabulky kódů na řádcích ~860–905)
- `..\EKL-1\EKL-1.pdf`

### Signály

- **Data:** firmware provádí `CPL A` a `SWAP A`. SWAP opravuje prohození nibblů na DPS. Výsledkem je **invertované ASCII** na P1-IN0..7, tedy port 02h.
- **STROBE** (P3.3) vede na X2 pin 10 (P1-STB) i pin 30 (P0-IN0). V klidu je v 1, při připraveném znaku jde do 0.
- **Čekání na ACK:** firmware pak **čeká, dokud ACK (P3.7 = X2 pin 13 = P0-OUT0) nečte 1**, a teprve potom vrátí STROBE do 1. Když program nepotvrdí, EKL-1 visí.
- **Bzučák:** X2 pin 14 (P0-OUT1), zní při 0.

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

### Protokol čtení (MikroMon `kbd.asm`, ověřeno v binárce 1.3)

1. `IN (01h)` bit 0 = 0 znamená, že je klávesa připravena.
2. `IN (02h)` a `CPL` dá kód ASCII.
3. `OUT (01h),03h` nastaví ACK.
4. Čeká se, až `IN (01h)` bit 0 = 1.
5. `OUT (01h),02h` zruší ACK. P0-OUT1 zůstává 1, bzučák je potichu.

### Emulace

Fronta znaků z PC klávesnice. Stavový automat:

1. Klid: STROBE=1.
2. Je-li znak ve frontě, data se nastaví a STROBE=0.
3. Čeká se na ACK=1, pak STROBE=1.
4. Čeká se na ACK=0 a pokračuje se dalším znakem.

Malé prodlevy mezi kroky odpovídají rychlosti firmwaru.

## Consul 262.3

Zdroj: `jpr-1z.php`.

- **Zapojení:** DAT0–7 → P1-IN0..7 (X2 piny 6, 4, 3, 5, 2, 8, 9, 7). STROBE → X2 pin 30 (P0-IN0). Napájení +5 V na pinu 22.
- **STROBE se nezachytává:** jde o pulz asi 1 ms, takže při pollingu lze klávesu přehlédnout. Existuje úprava se 7474, která STROBE zachytí.
- **Polarita** (strobe aktivní v 0, data invertovaná) je **jen odvozená z MikroMonu**.
- **Tabulka kódů v textové podobě chybí**, existují jen skeny: `H:\SAPI-1\SAPI_DOKU_2024-01-03\skeny_dokumentace_bez_retusi_Petr\CONSUL_262.3_4\`.
- Web: https://sapi.cz/prislusenstvi/c262-3v2.php

## Consul 259.11

Zdroj: `prislusenstvi\c259-11.php` (kabel ANK-1 KB-13).

- **Kódy:** 7bitový KOI-7 (ASCII, na 24h je ¤). Tabulka je na stránce.
- **Zapojení:**
  - Data /D1–/D7, aktivní v 0 → P1-IN0..6.
  - /SC (strobe) → P0-IN0.
  - Klávesa /CLR → P1-IN7.
  - /AC (potvrzení) ← P0-OUT0.
  - /ZVUK ← P0-OUT1 (manuál JPR-1Z chybně uvádí P0-OUT0).
  - /A0 → GND.
- **Chování:** kód drží, dokud ho počítač nepotvrdí (LED „P“ svítí). Autorepeat 10 znaků/s po 1,4–1,6 s.

## ANK-1

Zdroj: `sapi\ank-1.php`.

- **Matice:** membránová matice 5×8, dekódování dělá software.
- **Zapojení:** P0-OUT D4–D0 vybírají řádky (0 = aktivní), P0-IN čte 8 sloupců.
- **Další výstupy P0-OUT:** dva bity pro LED (ERR, SHIFT) a jeden pro bzučák. **Přesné pozice bitů jsou jen v obrazové příloze VII, zatím neznámé.**
- **P1-IN:** D7 = klávesa INT, D6 = klávesa T. RES vede na reset.
- **Adresy:** u paměťově mapované JPR-1 byly porty na 2400h atd., u JPR-1A/Z/ZI jde o I/O porty 01h/02h.
- **Kompatibilita:** nefunguje s CP/M a pravděpodobně ani s MikroMonem.
