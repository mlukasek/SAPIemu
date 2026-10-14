# JPR-1ZI – procesorová deska se Z80

Zdroje:
- `H:\SAPI-1\sapi.cz_BKP_2023-02-22\sapi\jpr-1z.php` (manuál Tesla včetně obsahu PROM a zapojení X1–X3) a `jpr-1zi.php`
- `..\SAPI-karty\JPR-1ZI\JPR-1ZI.pdf`, `JPR-1ZI_BOM.xlsx`
- PROM dumpy v `roms/jpr-1zi/`

## Obvody

- Z80 (D5B).
- EPROM 2764 (D4C).
- Dvě 74S287 jako dekodéry: D3C pro paměť, D6B pro I/O.
- Sedm 3212: šest pro I/O porty, D5C jako buffer dat z EPROM.
- 8286 jako budič dat, dále 74LS93, 74LS74 a 74LS123.
- **Na desce není RAM ani 8255, 8251 či 8253.**

## Hodiny

Krystal 16 MHz (oscilátor z hradel 74LS04) jde do 74LS93: QB = 8 MHz, QC = 4 MHz, QD = 2 MHz, QA = 1 MHz (QA vede jen na testovací pin JP1).

Hodiny se volí jumpery JP2/JP3, což jsou dvě lišty 1×4. Poloha n znamená propojení JP2.n–JP3.n:

| Poloha | Výsledek |
|---|---|
| 1 | 4 MHz → Φ2 sběrnice |
| 2 | 2 MHz → Φ2 sběrnice |
| 3 | 2 MHz → CPU CLK |
| 4 | 4 MHz → CPU CLK |

Ve schématu jsou osazeny polohy 2 a 3, tedy **sběrnice i CPU běží na 2 MHz**. Podle manuálu JPR-1Z je standard 2 MHz; kombinace sběrnice 2 MHz a CPU 4 MHz „je možná, nezkoušeno“.

Emulátor: `crystal_hz = 16000000`, `cpu_clock = 2|4 MHz`, `bus_clock = 2|4 MHz`.

## Paměť

Na desce není RAM. Signál **BOOT** (klopný obvod) se resetem nastaví na 1 (svítí LED D2) a zápisem `OUT (00h),x` se vynuluje. Znovu se nastaví jen resetem.

Dekodér D3C:
- Vstupy PROM: A0=A13, A1=A12, A2=GND, A3=A14, A4=A15, A5=A11, A6=BOOT, A7=GND. CE1=/AEN, CE2=GND.
- Používá se jen výstup O4 (bit 3) = ROM/RAM.

| Dump | BOOT=1 | BOOT=0 |
|---|---|---|
| `JPR-1ZI_D3C_2k.bin` | ROM 0000–07FF (bajt 0x40 = 08) | vše na sběrnici |
| `JPR-1ZI_D3C_8k.bin` | ROM 0000–1FFF (0x40, 0x42, 0x60, 0x62 = 08) | vše na sběrnici |

- Při ROM/RAM=1 se **čte z EPROM** a /MR se na sběrnici nepouští. **Zápis jde vždy na sběrnici**, takže RAM pod ROM lze zapisovat i při BOOT=1.
- Adresy EPROM A0–A12 jsou zapojené přímo. S dekodérem 2k je proto vidět jen offset 0–7FF.

## I/O

Dekodér D6B (`JPR-1ZI_D6B.bin`: bajty 07, 0B, 0D, 0E pro adresy 0–3, jinde 0F):
- Plně dekóduje A0–A7, takže porty nemají zrcadla.
- Je hradlován /AEN. A8–A15 se ignorují.

| Port | Čtení (/IOR) | Zápis (/IOW) |
|---|---|---|
| 00h | – (sběrnice) | BOOT := 0 |
| 01h | P0-IN | P0-OUT |
| 02h | P1-IN | P1-OUT |
| 03h | P2-IN | P2-OUT |

- Výstupy 3212 nuluje reset (/CLR).
- STB a /INT jednotlivých 3212 jsou vyvedeny jen na konektory, k CPU nevedou.
- Konektor X2 (klávesnice): P1-IN, P0-OUT, P0-IN, P0-STB, P1-STB a +5 V pro klávesnici.
- Konektor X3 (tiskárna): P2-IN, P2-OUT, P1-OUT. Emulace tiskárny Centronics (sekce `printer`, kabel podle
  návodu JPR-1Z: data P2-OUT, /STROBE P1-OUT1, BUSY P2-IN6, kolík 16 = P2-IN7): `docs/tiskarna.md`.
- Podrobnosti zapojení klávesnic jsou v [klavesnice.md](klavesnice.md).

## Sběrnice a řízení

- **Strobe:** Z80 strobe se převádějí na signály ve stylu 8080 (/MR, /MW, /IOR, /IOW, /INTA) hradly 74LS32 a budiči 74LS125. Data budí 8286.
- **/AEN, /HLDA:** /BUSAK přes invertor dává /AEN a z něj vzniká /HLDA. /AEN přepne budiče desky do třetího stavu.
- **RDY → Z80 /WAIT; /HOLD → /BUSRQ.**
- **STSTB:** pulz asi 100 ns z 74LS123 na začátku /MREQ.
- **Pin 7 = /REF** (/RFSH & /MREQ), potřebuje ho RAM-1Z s DRAM.
- **Reset:** tlačítko /RTL (X1 pin 1) jde přes monostabilní obvod, aby přečkal refresh DRAM. Výsledkem je /RES na sběrnici, který zároveň nuluje porty a nastavuje BOOT.

## Přerušení

- /INT0 (pin 62) → Z80 /INT. /INT1 (pin 59) → /NMI.
- **Proti JPR-1Z navíc (XINTA):** D4A (7432) a D2B (7400) způsobí, že po INTA cyklu se čtení z paměti mění na INTA cykly až do dalšího M1. Používá se **IM2** a hodnota registru I nevadí.
- **Postup s 8259:**
  1. Z80 při INTA přečte vektorový bajt, kterým je opkód CALL z 8259.
  2. Dvě následující „čtení tabulky vektorů“ vrátí 2. a 3. bajt INTA z 8259.
  3. CPU tak skočí na adresu, kterou poslal 8259.
- Na sapi.cz je jako první bajt uvedeno C9h. Správně má být CDh (CALL), jde nejspíš o překlep.
- Emulovat až s kartou s 8259.

## EPROM dostupné v `roms/`

- **MikroMon 1.3:** podrobnosti v [../software/MikroMon.md](../software/MikroMon.md).
- **ECMON 2k** („JPR-1Z 62K MON CZ“): kopíruje se na F000. Používá porty 01/02, DSM-1 na 10–17h a 28–2Fh (DPP-1).
- **ECMON 2k IDE-1:** totéž a navíc porty 5C–5Fh (PMD32-SD přes IDE-1).
- **ZZRMD 2k/8k:** přenos systému (Intel HEX ze sériové linky) na RAM disk ZRMD-1, **ne** boot. CP/M
  bootuje ECMON. Podrobnosti v [../software/ECMON.md](../software/ECMON.md).

## JPR-1V (modernější reinkarnace)

Zdroje: `..\SAPI-karty\JPR-1V\DESCRIPTION`, `schema\*.png`.

- **Hodiny:** oscilátor 8 MHz a 74HCT175. Jumper TURBO přepíná CPU mezi 2 a 4 MHz. Že sběrnice zůstává na 2 MHz, je jen odvozeno z popisu.
- **Reset:** MCP100.
- **I/O dekodér:** 74LS138. Port 0 zápis = BOOT off, porty 1–3 = 3212.
- **ROM:** 28C64, výběr přes 74LS688 jen pro 0000–07FF při /BOOT a /AEN. A11 EEPROM se volí jumperem BIOS (dvě 2K banky).
- **Bez helperu pro 8259.**
- **Známá vlastnost:** DGD-1 nejde použít jako RAM, protože RDY při BOOT visí.

## Nejasnosti

- **Čip Z80: NMOS, nebo CMOS?** Parametr `z80` (výchozí `nmos`). CMOS Z84C00 nepřijme přerušení hned po
  `LD A,I` / `LD A,R` (změřeno na JPR-1V 2026-10-13). Ověřit na autorově JPR-1ZI (`tools/hwbench/intblock.asm`)
  a podle nápisu na čipu.

- **Čtení prázdné sběrnice:** JPR-1V vrací poslední bajt datové sběrnice CPU (změřeno 2026-10-11). Jak je to
  u JPR-1ZI, JPR-1Z a JPR-1 (8080)? **Od 2026-10-14 emuluje JPR-1ZI i JPR-1Z poslední bajt předběžně** (autor si
  pamatuje, že neexistující IDE ukazovalo disky XYXY), parametr `open_bus` (`last` výchozí, `ff`). Změřit na
  skutečné JPR-1ZI (`tools/hwbench/openbus.asm`). JPR-1 (8080) dál FFh.

- Nedohledaný účel sítí „P1-/INT_“ a „P1-STB_“ ve schématu JPR-1ZI.
- ~~Hodiny sběrnice v režimu TURBO u JPR-1V.~~ Změřeno 2026-10-13 (`busclk.py`, Φ2 na DKS-1): při TURBO CPU
  4,000 MHz a Φ2 na sběrnici 2,000 MHz se střídou 50 %, obě hrany Φ2 0–8 ns po sestupné hraně CLK CPU; bez TURBO
  CPU i Φ2 2,000 MHz, Φ2 jde 8 ns za CLK CPU. Sběrnice má tedy vždy 2 MHz, jak píše schéma.
