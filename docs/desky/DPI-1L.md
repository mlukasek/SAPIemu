# DPI-1L – síťová část (Repository API a NET)

Stav: **emulovaná jen síťová část** (typ `DPI-1L`, `src/cards/dpi1l.*`, spojení s relay `src/core/relay.*`) od
2026-10-16. Sestava `machines/sapi1v_BomberNet.sapi`.

DPI-1L je Martinova karta s RP2350 na sběrnici SAPI. Na skutečném železe umí víc: emuluje karty DPP-1, IDE-1,
RAM disk a další a má SD kartu. Do SAPIemu patří **jen síťová část**: Repository API (rozhraní kompatibilní
s Unicardem) s příkazy NET A0h–A9h pro síťové hry přes relay, hlavně **BomberNet**. Karty, které DPI emuluje,
má SAPIemu samo (rozhodnutí Martina 2026-10-16).

## Podklady

- **Zadání:** `..\SAPI_DPI-firmware\docs\sapiemu_dpi.md` (2026-10-16). Jsou v něm i referenční záznamy ze
  skutečné DPI-1L (§6).
- **Vzor je ověřený firmware DPI v0.2** (`..\SAPI_DPI-firmware`, stejný autor, GPLv3):
  - `common/repo.c` a `repo.h`: stavový automat Repository API;
  - `common/netgame.c`: NET A0–A9, relay a WebSocket;
  - `firmware/main.c`: reset SAPI.
- **Popis protokolu:** `docs/unicard.md` (§2 stav, §3 parametry, §7 NET, §8 detekce) a `docs/net.md` (relay,
  chyby MZPico, kterým se vyhnout).
- **Klient na Z80:** `..\BomberNet\c\common\netdev_card.c` (detekce, volání NET). Port pro SAPI vzniká v repu
  `SAPI-BomberNet` (`sapi/uc_sapi.c`).

## Porty

| Port | Zápis (OUT) | Čtení (IN) |
|---|---|---|
| báze (B0h) | příkaz | stav: 4 bajty, pak 00h |
| báze + 1 (B1h) | parametr | výstup |

- Báze je parametr `base`. Na skutečné kartě se nastavuje v `DPI.INI`, výchozí B0h. Zrcadla B2h/B3h karta nemá.
- Kódy A0–A9 jsou příkazy zapsané na B0h, ne porty. S RTC-1 na A0h tedy nekolidují.

## Protokol (podle `repo.c`)

- **Stav** (čtení B0h):
  - bajt 0 = příznaky: 01h PARAMRQ, 02h DOUTRQ, 40h IN_PROGRESS, 80h ERROR;
  - bajt 1 = poslední příkaz;
  - bajt 2 = chybový kód, nebo volné místo pro parametry (PARAMRQ), nebo zbývající bajty výstupu (DOUTRQ);
  - bajt 3 = výsledek FatFs, tady vždy 0.
- **Ukazatel stavu se vrací na bajt 0** každým zápisem na B0h a každým přístupem na B1h. STSR (03h) nedělá nic
  jiného, chybu nemaže.
- **Parametry:**
  - `B` = bajt, `S` = řetězec ukončený libovolným bajtem < 20h;
  - volné místo na začátku je 255 − počet řetězců;
  - po posledním parametru se příkaz hned vykoná.
- **Čtení výstupu:** po posledním bajtu je fáze DONE. Čtení, když není co číst, dá 00h a ERROR 4.
- **Chybové kódy:** 1 neznámý příkaz, 3 přetečení parametrů, 4 nic ke čtení nebo zápisu; u NET 6 build, 7 místnost,
  8 není v místnosti, 9 spojení, 10 parametr, 11 nejde odeslat.
- **Příkazy:**
  - 00 RESET (opustí místnost NET);
  - 01 ASCII a 04 STORNO nedělají nic, 02 SHASCII dá ERROR 1;
  - 05 REV `DPI-1L v0.2` + 0Dh, 06 REVD `00 02 44 01`;
  - 11 INTCALLER `00`, 93 WIFISTATUS `03` (připojeno), 95 INFO 16 B `01 01 04 09` + nuly (bit 08h = NET);
  - A0–A9 NET: STATUS, CREATE, JOIN, LEAVE, READY, SEND, POLL, HASH, MSG, RECV (tabulka v zadání §3);
  - ostatní (soubory 20–56h, RTC 60–63h, TCP/DNS 80–89h, BOOT 07h…) ERROR 1.
- **Detekce v BomberNetu:** REVD musí dát stav `02 06 04`; INFO musí být bez ERROR, s výstupem alespoň 16 B a s bitem
  08h v bajtu 3.

## Emulace

- **Parametry** (katalog, konfigurátor):
  - `base` (B0h);
  - `net.transport`: `ws` (WebSocket, výchozí), nebo `tcp` (řádky JSON);
  - `net.host` (`api.mzpico.com`), `net.port` (80).
  - Relay jde změnit i za chodu, karta přitom opustí místnost (jako příkaz `ngame relay` na konzoli DPI).
  - Pro lokální `relay.py` z repa BomberNet: `tcp` s portem 8766, nebo `ws` s portem 8765. `relay.py` poslouchá jen
    na 127.0.0.1.
- **Bez čekacích taktů.** Skutečná karta drží Z80 ve WAIT, dokud přístup neskončí. Emulátor odpoví hned.
- **CREATE a JOIN běží na pozadí:**
  - stav má bajt 0 = 40h, dokud relay neodpoví (`room`, `error`), nebo dokud nevyprší limit 10 s;
  - limit se měří časem PC, ne takty emulace, protože relay běží v reálném čase;
  - nový příkaz během čekání čekání zruší (jako firmware).
- **Spojení:**
  - každé CREATE a JOIN otevře nové spojení s relay ve vlastním vlákně;
  - řádky od relay čekají ve frontě bez omezení délky a karta je zpracuje při nejbližším přístupu Z80 na své porty;
  - Z80 nic jiného nevidí, výsledek je stejný, jako kdyby se zpracovaly hned;
  - LEAVE, RESET 00h i /RES pošlou `leave` a teprve pak spojení zavřou (FIN za odeslanými daty).
- **WebSocket je vlastní**, jako ve firmwaru:
  - handshake s `Host` bez portu, `Sec-WebSocket-Accept` se neověřuje;
  - rámce klienta maskované, ping → pong se stejným obsahem;
  - `wss://` nejde (SAPIemu nemá TLS), produkce má proto i `ws://api.mzpico.com/net`.
- **Uložený stav:** spojení se uložit nedá. Po načtení stavu je karta jako po RESET 00h: bez místnosti, stav
  `00 00 00 00`.
- **Bez sítě** se karta chová jako skutečná, které se nepodařilo spojit: CREATE a JOIN skončí chybou 9. S prázdným
  `net.host` hlásí STATUS stav 0 (NOLINK).
- **Konfigurátor** ukazuje za chodu stav sítě (např. „NET: running, room SYTN, slot 0, 2 members“). MCP
  `machine_info` vypisuje relay, stav, kód místnosti, slot a poslední chybu.

## Ověření

- **Test `test_dpi1l`:** malá falešná relay v testu (TCP řádky i WebSocket). Pokrývá:
  - záznamy ze skutečné karty bez místnosti (zadání §6) a detekci BomberNetu;
  - hru dvou hráčů ze záznamu §6 bajt po bajtu, přes TCP i WebSocket (s pingem);
  - JOIN jako divák a jako hráč, chyby relay 6 a 7, limit → chyba 9, nedostupnou relay;
  - ztrátu spojení ve hře (stav 5, chyba 9), `desync`, `dropped`;
  - LEAVE, RESET 00h, /RES a načtení stavu: vždy `leave` před koncem spojení;
  - zrušení CREATE novým příkazem.
- **Ručně 2026-10-16:** `sapiemu-cli --mcp` s kartou ovládanou přes `io_write`/`io_read` jako konzolí DPI a
  `tools/net_peer.py` z repa firmwaru jako druhý hráč. Všechny tři cesty daly stejné bajty jako záznam §6 (jiný byl
  jen kód místnosti a seed):
  - `relay.py`, karta přes TCP 8766, druhý hráč přes WebSocket 8765;
  - `relay.py` přes WebSocket;
  - produkční `ws://api.mzpico.com/net`.

## Rozdíly a otázky k firmwaru (2026-10-16)

Emulátor se ve všech bodech chová jako firmware v0.2. Zadání to místy popisuje jinak:

1. **/RES nechá stav Repository API, jak byl.** Firmware při resetu SAPI zavolá jen `netgame_reset()`: opustí
   místnost, ale fázi, poslední příkaz a chybu nemění. Když reset přijde během CREATE/JOIN, stav dál hlásí 40h, dokud
   program nezapíše nový příkaz. Zadání říká jen „/RES opustí místnost“. Programy po startu příkaz vždy zapíšou, vadit
   to nemá.
2. **Zrušené CREATE/JOIN místnost přesto otevře.** Nový příkaz během čekání zahodí jen výsledek. Spojení zůstane, a
   když pak `room` přijde, karta je v místnosti (STATUS ukáže IN_ROOM) a limit 10 s už neběží. Zadání: „pozdější
   odpověď relay se zahodí“.
3. **JOIN s kódem delším než 7 znaků projde.** Firmware čte nejvýš 7 znaků a zbytek zahodí, chyba 10 je jen pro
   prázdný kód a jiné znaky než písmena a číslice. Zadání: „1–7 znaků, jinak chyba 10“.
4. **Ostatní příkazy Repository API dají ERROR 1**, i ty, které skutečná karta umí bez SD karty: RTC 60–63h (hodiny
   RP2350) a BOOT 07h (reset SAPI přes /RTL). Soubory nad složkou PC jsou v zadání volitelná druhá část (§7), zatím
   nejsou.
