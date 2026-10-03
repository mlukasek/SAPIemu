# DSM-1V – dva sériové kanály (Libor Lasota, podle DSM-1, bez kazety)

Stav: **emulovaná** (2026-10-04), sestava `machines/sapi1v.sapi`, test `tests/test_sapi1v.cpp`.
Karta `DSM-1V` (`src/cards/dsm1.*`, dvě instance kanálu DSM-1 bez kazety):
- parametry `a` a `b` (`enabled`, `base`, `baud`, `connected`, …);
- výchozí podle fotky (`..\DSM-1V\Foto\mDSM-1V_rev.1.1.jpg`) a autora: **A = 10h, B = 14h, oba 115200 Bd**.
- Sériové kanály „DSM-1V #10“ a „#14“ jdou připojit přes TCP a COM jako u DMR-1.

Zdroje:
- schéma `..\DSM-1V\DSM-1V.pdf` (DPLA0089DSM rev 1.1, 27.06.2025);
- `..\DSM-1V\DESCRIPTION`, `info.txt`, `Baud DSM1V.xlsx`, `UART6402 typy.xlsx`;
- https://www.pmd85.cz/?page_id=2229.

**Bez PROM a GAL.**

## Zapojení

- **Dva shodné kanály A a B:** UART HD3-6402R-9 (rodina 6402, náhrada MHB1012/AY-5-1013), 75188/75189,
  konektor DB9M (K3 = A, K2 = B). Kanál A má navíc lištu TTL pro adaptér USB/FTDI. RI se nepoužívá.
- **Adresa** (74LS688, A4 = 1, A5–A7 = 0 napevno), propojky S3, osazená = 0:

  | S3 | Funkce |
  |---|---|
  | 1-2 | A2 kanálu A |
  | 3-4 | A3 kanálu A |
  | 5-6 | povolení kanálu A |
  | 7-8 | A2 kanálu B |
  | 9-10 | A3 kanálu B |
  | 11-12 | povolení kanálu B |

  Základ = 10h + 4 · (A2 rozpojená) + 8 · (A3 rozpojená). Potisk: DIS, 10h, 14h, 18h, 1Ch.
- **Registry** jako DSM-1 a DMR-1:

  | Port | Zápis | Čtení |
  |---|---|---|
  | +0 | b0 RTS, b1 DTR, b2 BRK, b5 „START“ (nevede nikam), b6 IET, b7 IER. **Bez RD, KAZ a kazety.** | b7 /CTS, b6 /DSR, b5 /DCD (0 = aktivní), b4 RXD, b3 PE, b2 FE, b1 OE, b0 INTR |
  | +1 | CRL: b0 PI, b1 SBS, b2 CLS2, b3 CLS1, b4 EPE | b7 DR, b6 TBRE, b5–b0 = 1 |
  | +2 | data k odeslání | přijatá data |
  | +3 | reset UART (MR prodloužený do dalšího /MR, nastaví ho i /RES) | nic (FFh) |

  - 0Dh = 8N1, tak ho používá MikroMon.
  - Bez kabelu je /DCD = 1, MikroMon pak kanál bere jako nepřipojený.
- **Přerušení:** /INT = NOT(DR·IER + TBRE·IET) pro každý kanál, oba kanály přes 74LS38 na **/INT0**, bez
  propojky.
- **Rychlost:**
  - oscilátor 3,6864 MHz → 74HC(T)40103 pro každý kanál (dělí P+1), výstup je 16× takt UART;
  - S2 = kanál A, S1 = kanál B, dvojice 15-16 = P0 … 1-2 = P7, osazená = 0;
  - **Baud = 230400 / (256 − N)**, N = kód z osazených propojek, rozsah 900 Bd (bez propojek) až 115200 Bd
    (N = FEh);
  - tabulka `Baud DSM1V.xlsx`. Libor s HD-6402B a rychlejším oscilátorem dosáhl 921,6 kBd.
- **Rev 1.1:** prodloužení /IOW nuluje /MR, prodloužení MR přes 74LS10.

## Autorova deska (fotka)

- Nejspíš A = 10h, B = 14h, k ověření.

## Plán emulace

- Dvě instance UART (`Mhb1012`) se sériovými kanály jako u DMR-1/DSM-1:
  - každý kanál má vlastní základ a povolení;
  - rychlost z kódu N;
  - přerušení na INT0.
- V panelu a v názvu kanálu „DSM-1V #10“ / „#14“.
