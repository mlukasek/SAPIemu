# RSW-1 – RAM, sériový port a watchdog SAPI-1.5

Stav: **emulovaná** (typ `RSW-1`, `src/cards/rsw1.*`, USART `src/chips/i8251.*`) od 2026-10-07. Sestava
`machines/zps/ZPS-6.sapi`, test `test_sapi15`. Tesla Liberec 1990, 6XK 199 244, deska 6XB 007 26, návod
6XV 123 43.

## Podklady

- **Návod Tesla** (sken): `..\SAPI_DOKU\SAPI-1.5_dokumentace\RSW-1\TMP48..78.TIF` (strana N = TMP(N+48), schéma
  TMP75–78). OCR a **dumpy PROM z desky**: `..\SAPI.CZ\sapi\rsw-1.php`.
- Replika EC1045 rev 1.0 (2010): `..\SAPI-karty\RSW-1\RSW-1.sch` a PDF.
- Software se sériovou linkou: Melsoft disketa 7 (`CONNECT.MAC`, `INTER.MAC`, `SAPI.MAC`), `docs/software/SAPI-1.5.md`.
- Rozbor: `work/sapi15/report_pkt1_rsw1.md` (mimo git).

## Zapojení

- **RAM 64 KB** (8 × 4164), obnova jen RAS ze sběrnicového /REF, takže **jen s procesorovými deskami Z80**
  (PKT-1, JPR-1Z). Linky BOOT, RDY, /M1, /INTA na desce nejsou: stínění EPROM funguje tím, že PKT-1 v okně EPROM
  vypne svůj datový buffer.
- **Dekodér RAM DD1** (74S571, 512 × 4): vstupy A0–A5 = A10–A15 (bloky 1 KB), A6/A7/A8 z pole propojek **XT1**
  (každý L, H, MAP1 nebo MAP2). Dump z desky (= tabulka návodu str. 17), skupina A8 A7 A6:

| A8 A7 A6 | RAM |
|---|---|
| 000, 010 | 0000–FFFF |
| 001, 101 | 0000–F7FF |
| 011, 111 | 0000–BFFF |
| 100 | 0000–7FFF + C000–FFFF |
| 110 | 0000–AFFF + C000–FFFF (**díra B000–BFFF**) |

- **Tabulka propojek XT1 v návodu (str. 15) i seznam propojek ZPS-8 dumpu odporují.** Mapa paměti ZPS-8
  (MAP2 = 1 uvolní B000–BFFF pro DBS-1) sedí jen při **A6 = L, A7 = H, A8 = MAP2**, což je výchozí nastavení
  emulace (předpoklad).
- **Porty** (PROM **DD2**, dump z desky, /AEN): **18h data 8251, 19h příkaz/stav 8251, 1Ah čítač 2, 1Bh řídicí slovo
  8253, 1Ch čítač 0, 1Dh čítač 1**, 1Eh maska přerušení a BREAK, 1Fh watchdog. Tabulka v návodu (str. 13) má
  18h/19h a čítače přehozené; schéma i software (Melsoft: `usd 18h`, `usc 19h`, `tcw 1Bh`, `t0 1Ch`) odpovídají
  dumpu.
- **8251:** TxC = RxC = OUT0 8253; čítače běží z 12,288 MHz / 10 = **1,2288 MHz**. Software Melsoft: 8253 čítač 0
  režim 3, dělitel 8 (153,6 kHz), 8251 režim CEh (×16, 8 bitů, 2 stop) = **9600 Bd**, příkaz 37h.
- **Registr 1Eh:** D0 povolení TxRDY, D1 RxRDY, D3 TxE, D2 = BREAK. Součet povolených požadavků jde přes **XT3
  13-14** na /INT0 (otevřený kolektor, bez vektoru).
- **Watchdog:** 1Fh D0 = 1 otevře GATE1/GATE2 čítačů 1 a 2 (režim 2, čítač 2 taktovaný OUT1, nejvýš asi
  58 minut). Obnovení zápisem 0 a 1. Doběhnutí nastaví klopný obvod, který přes **XT2 11-12** stáhne /RTL:
  PKT-1 resetuje celou sestavu, /RES obvod nuluje. XT2 v dodávce ZPS-8 osazená není (watchdog je vypnutý).
- **XP2** (číslování DB-25 V.24): 2 TD, 3 RD (i 15), 4 RTS, 5 CTS, 6 DSR, 7 SG, 20 DTR, 9/10 SYNDET, 16 pomocný
  výstup, 19/21 a 23/24 proudová smyčka DPK-1.

## Emulace

- Parametry: `decoder` (DD1), `xt1_a6`, `xt1_a7`, `xt1_a8` (L/H/MAP1/MAP2, výchozí L, H, MAP2), `io_decoder` (DD2),
  `int0` (XT3), `watchdog_reset` (XT2, výchozí ne), terminál (`connected`, `cts`, `serial.tcp`, `serial.com`
  jako u DMR-1).
- 8251 jen asynchronně na úrovni znaků se správným časováním, rychlost z děliče čítače 0 (čítač 0 se netaktuje
  po jednom, čte se jen jeho dělitel). Synchronní režim, BREAK a proudová smyčka se neemulují.
- Čítače 1 a 2 se taktují, jen když watchdog běží. Sériový kanál se jmenuje „RSW-1 #18“ (okno terminálu, MCP).

## Nejasnosti k ověření na HW

1. Skutečné propojky XT1 na deskách ZPS-7/8 a polarita MAP (schéma Tesla píše /MAP1, /MAP2).
2. Analogová zem (kolíky 53/54) proti GND (na replice nespojené).
3. Obnovení watchdogu: stačí zápis 0 a 1 do 1Fh (předpoklad z návodu „H→L→H na hradle“)?
