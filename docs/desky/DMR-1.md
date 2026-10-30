# DMR-1 – sériové rozhraní + RAM

DMR-1 vznikla sloučením a zjednodušením desek [DSM-1](DSM-1.md) a [RAM-1Z](RAM-1Z.md).

Zdroje:
- `..\SAPI-karty\DMR-1\DMR-1_sch.pdf`
- [sapi.cz/sapi/dmr-1a.php](https://sapi.cz/sapi/dmr-1a.php)
- `roms/dmr-1/DMR-1_D2C.bin`

## Sériová část

- **UART:** MHB1012, kompatibilní s AY-5-1013 / TR1602. **Není to 8251 a není tu ani 8253.**
- **Baud rate:** krystal 4,9152 MHz, dělení 3× 7493. Jumper JP8 volí 75, 150, 300, 600, 1200, 2400, 4800, 9600, 19200 nebo 38400 Bd.
- **Bázová adresa:** jumper JP2 volí 10h, 14h, 18h nebo 1Ch. Dekódování zajišťuje 3205 z A2–A7.
- **Registry:** stejné jako u DSM-1, viz tabulka níže.
- **Přerušení:** výstup na INT0 (X1 pin 62) přes 7438 s otevřeným kolektorem.
- **Kazeta a dálnopis:** nejsou.

### Registry (stejné jako DSM-1)

| Port | Čtení | Zápis |
|---|---|---|
| base+0 | Stav modemu: b7 CTS, b6 DSR, b5 DCD, b4 RI (surový vstup), b3 PE, b2 FE, b1 OE, b0 INTR | Řízení modemu: b7 IER, b6 IET, b5 relé kazety, b4 KAZ, b3 RD, b2 BRK, b1 DTR, b0 RTS |
| base+1 | Stav UART: b7 DR (data připravena), b6 TBRE (vysílač prázdný), b5–0 = 1 | Řízení UART: b0 PI (1 = bez parity), b1 SBS (1 = 2 stop bity), b3–2 délka slova, b4 EPS (1 = sudá parita) |
| base+2 | Přijatý znak (čtení nuluje DR) | Vysílaný znak (zápis nuluje TBRE) |
| base+3 | – | Libovolný zápis resetuje UART |

- **Přerušení:** INT0 = (DR & IER) | (TBRE & IET).
- **Jak to používá MikroMon:**
  - Zapisuje `OUT base,03` (RTS+DTR), `OUT base+1,0Dh` (8N1) a `OUT base+3` (reset).
  - Kanál považuje za přítomný, když `IN base` má bit 5 (DCD) = 0. Nepřítomný kanál čte FFh.
  - Chyby testuje maskou 06h (FE, OE) na base+0.
  - Na base+1 čeká na b7 (příjem) a b6 (vysílač prázdný).
- **Emulace:**
  - Modemové vstupy jsou konfigurovatelné. Výchozí hodnota je „připojeno“, takže CTS, DSR a DCD čtou 0.
  - Znaky se vysílají a přijímají tempem podle nastaveného baud rate (bity na znak podle módu).

## Paměťová část

- **Paměť:** SRAM 62010 128 KB, zálohovaná obvodem MAX691 a baterií nebo akumulátorem. Jumper JP7 je pro baterii.
- **Dekodér PROM D2C** (74S571, `DMR-1_D2C.bin`, 512 B):
  - Vstupy: A0..A5 = A10..A15 sběrnice (granularita 1 KB). A6, A7 a A8 přicházejí z jumperů JP3: A6 = MAP1 nebo pevná úroveň, A7 = MAP2 nebo pevná úroveň, A8 pevně.
  - Web píše JP7, schéma JP3. Platí schéma.
  - Výstupy: D0 = /CER (0 = RAM vybrána), D1 = SRAM A16, D3 = blokování zápisu.
  - V tomto dumpu jsou D1 a D3 vždy 0, takže se používá jen spodních 64 KB.

Konfigurace podle vstupů A8, A7, A6:

| Konfigurace | Obsazená RAM |
|---|---|
| 0 | celých 64 KB |
| 1 | 0000–F7FF (MikroMon / CP/M s AND-1Z na F800) |
| 2 | 0000–EFFF |
| 3 | žádná |
| 4 | 4000–FFFF |
| 5 | 4000–F7FF |
| 6 | 4000–EFFF |
| 7 | žádná |

Emulátor řeší konfiguraci jumperem `ram_config` 0–7, případně připojením A6 a A7 na MAP1 a MAP2. Baterie (JP7, parametr `battery`, výchozí zapnuto): obsah 128 KB SRAM přežije vypnutí počítače i ukončení
emulátoru, drží se v souboru `sram_file` (výchozí `work/dmr-1/sram.bin`, uloží se při vypnutí a ukončení). Bez baterie
je RAM po zapnutí vynulovaná. Více sestav se stejným souborem se chová jako jedna fyzická karta přenášená mezi nimi.
Reset (/RES) obsah RAM nemění. Autor baterii na svých kartách standardně nemá, sestavy v `machines/` proto mají
`"battery": false` (2026-10-05, jinak ECMON po zapnutí nenabídne volbu systému, `docs/software/ECMON.md`).

Karet DMR-1 může být v sestavě víc. Další se používají jako sériové porty: RAM se vypne (parametr `ram`) a UART se
nastaví na jiný port (JP2). Sériový kanál se jmenuje podle portu, např. „DMR-1 #14“.
