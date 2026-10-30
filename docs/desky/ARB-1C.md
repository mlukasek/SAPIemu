# ARB-1 / ARB-1C – sběrnice

Zdroje:
- [sapi.cz/sapi/arb-1.php](https://sapi.cz/sapi/arb-1.php), [sapi.cz/sapi/arb-1x.php](https://sapi.cz/sapi/arb-1x.php)
- netlist `..\SAPI-karty\ARB-1C_8slot\Eagle\ARB-1C_ZPC_8slot_cor.sch` (XML)
- `..\SAPI-karty\ARB-1C_8slot\ARB-1C_ZPC_8slot_cor_sch.pdf`

## Konektor FRB, 62 pinů

Pokud není uvedeno jinak, signály jsou aktivní v nule.

| Pin | Signál | Pin | Signál |
|---|---|---|---|
| 1 | /RTL (tlačítko reset) | 31 | D4 |
| 2 | /HOLD | 32 | D3 |
| 3 | RDY (aktivní v 1) | 33 | D6 |
| 4 | STSTB (aktivní v 1) | 34 | D5 |
| 5 | /HLDA | 35 | D2 |
| 6 | /M1 | 36 | D7 |
| 7 | /INTE (u JPR-1Z/ZI a JPR-1V: /REF, /RFSH) | 37 | D0 |
| 8 | /RES | 38 | D1 |
| 9 | /MR | 39 | A1 |
| 10 | /IOR | 40 | A0 |
| 11 | /MW | 41 | A3 |
| 12 | /IOW | 42 | A2 |
| 13 | /AEN | 43 | A5 |
| 14 | /DEN | 44 | A4 |
| 15–18 | +5 V | 45 | A7 |
| 19–20 | 0 V | 46 | A6 |
| 21 | S1 = MAP1 | 47 | S3 (−15 V) |
| 22 | S2 = MAP2 | 48 | S4 (zálohované napájení) |
| 23 | A9 | 49 | S5 (?) |
| 24 | A15 | 50 | S6 (+15/+30 V) |
| 25 | A11 | 51–52 | +12 V |
| 26 | A14 | 53–54 | 0 V |
| 27 | A13 | 55–56 | −5 V |
| 28 | A12 | 57–58 | −12 V |
| 29 | A10 | 59 | /INT1 |
| 30 | A8 | 60 | Φ2 (TTL hodiny) |
| | | 61 | /INTA |
| | | 62 | /INT0 |

## Důsledky pro emulaci

- **Paměť a I/O:** sběrnice je ve stylu 8080, má jen /MR, /MW, /IOR a /IOW (žádné MREQ ani IORQ). I/O adresu tvoří A0–A7.
- **Čekání:** samostatný WAIT na sběrnici není. Karta ho vyvolá stažením RDY do nuly (např. AND-1Z při přístupu do VRAM).
- **Přerušení:** jen /INT0 a /INT1 a k nim /INTA. Na JPR-1ZI vede INT0 na Z80 /INT a INT1 na /NMI.
- **MAP1 a MAP2:** vedení pro přemapování paměti. Budí je např. ZRMD-1 (port 63h, bity 6–7, přes JP2). Čtou je dekodéry RAM a AND-1Z.
- **Volná sběrnice:** emulátor vrací FFh. Je to předpoklad, na reálném HW ověřit.

## ARB-1C_8slot

Varianta desky ZPC (Martin), 8 slotů.

- **X1 je slot pro CPU.** Adresy AP0–AP15 jdou přes dva 8282 (STB připojen na H, tedy transparentní) na A0–A15 slotů X2–X8.
- **JP3** volí chování těchto bufferů:
  - 2-3: /OE bufferů je /AEN. Při DMA se adresy ze slotu 1 odpojí a sběrnici budí DMA master.
  - 1-2: buffery jsou trvale vypnuté. Používá se pro PKT-1 a SAPI-1.5, kdy CPU karta sedí mezi ostatními.
    Sestava `machines/zps/ZPS-6.sapi` (2026-10-07) tak nahrazuje sběrnici SBI-1 SAPI-1.5: X1 je volný, PKT-1 v X2
    si /AEN budí sama.
- Data a řídicí signály buffery nemají.
- **X1 nemá vývody S1–S6**, takže CPU slot nevidí MAP1 a MAP2.
- Na Φ2, /INTA, /INT0 a /INT1 jsou pull-upy 10k (R1–R4).
- RN3–RN8 (10k SIP) jsou v BOM, ale v netlistu nezapojené. **Nejasnost.**

V emulátoru může mít sběrnice libovolný počet slotů. Slot 1 je vyhrazen CPU kartě, pokud není JP3 v poloze 1-2.
