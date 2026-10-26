# DPI-1L – Repository API (soubory na SD kartě, NET)

Stav: **emulované Repository API** (typ `DPI-1L`, `src/cards/dpi1l.*`, SD karta nad složkou PC `src/cards/dpi1l_fs.*`,
spojení s relay, TCP a DNS `src/core/relay.*`). Síťová část od 2026-10-16 ráno, soubory, nastavení, RTC, BOOT, TCP
a DNS od 2026-10-16 odpoledne. Sestava `machines/sapi1v_BomberNet.sapi`.

DPI-1L je Martinova karta s RP2350 na sběrnici SAPI. Na skutečném železe emuluje i karty DPP-1, IDE-1, RAM disk
a další. Ty do SAPIemu nepatří, SAPIemu je má samo (rozhodnutí Martina 2026-10-16). Emulované je **Repository API**
(rozhraní kompatibilní s Unicardem):
- soubory a adresáře SD karty nad **složkou PC**, aby šel v emulátoru vyvíjet software pro SAPI, který s DPI pracuje
  (například `DPI.COM`);
- nastavení v `/DPI/DPI.INI`, hodiny RTC a BOOT;
- jedno spojení TCP a DNS (80–89h) ze sítě PC;
- příkazy NET A0h–A9h pro síťové hry přes relay, hlavně **BomberNet**.

## Podklady

- **Zadání:** `..\SAPI_DPI-firmware\docs\sapiemu_dpi.md` (2026-10-16). Jsou v něm i referenční záznamy ze
  skutečné DPI-1L pro NET (§6) a odpovědi na otázky (§9).
- **Záznamy zbytku Repository API:** `..\SAPI_DPI-firmware\docs\sapiemu_dpi_repo.md` (2026-10-16, soubory, nastavení,
  RTC, BOOT, SDCARD, TCP a DNS, odpovědi na otázky). Firmware se při nich opravil (`f3e583b`), emulátor jde podle
  opraveného.
- **Vzor je ověřený firmware DPI v0.2** (`..\SAPI_DPI-firmware`, stejný autor, GPLv3):
  - `common/repo.c` a `repo.h`: stavový automat Repository API, bajty souboru, SEEK, TELL, SIZE, RTC;
  - `common/repo_fs.c`: soubory a adresáře (FatFs), dohledávání jmen, skryté soubory;
  - `common/koi8.c`: jména v KOI-8čs2 (převzato beze změny);
  - `common/config.c`, `common/ini.c`: `/DPI/DPI.INI`, GETCONFIG a SETCONFIG;
  - `common/net.c`: TCP a DNS;
  - `common/netgame.c`: NET A0–A9, relay a WebSocket;
  - `firmware/main.c`: reset SAPI, BOOT (pulz /RTL 100 ms).
- **Popis protokolu:** `docs/unicard.md` (§2 stav, §3 parametry, §4 příkazy, §6 záznam FILINFO, §7 NET, §8 detekce,
  §8a rozhodnutí pro DPI, §9 opravené chyby originálu), `docs/funkce/repository.md` a `docs/net.md`.
- **Klienti na Z80:** `cpm/dpi.com` (DPI.COM, z88dk) a `..\BomberNet\c\common\netdev_card.c`. Port BomberNetu pro SAPI
  je v repu `SAPI-BomberNet`.

## Porty

| Port | Zápis (OUT) | Čtení (IN) |
|---|---|---|
| báze (B0h) | příkaz | stav: 4 bajty, pak 00h |
| báze + 1 (B1h) | parametr, bajt do otevřeného souboru | výstup, záznam adresáře, bajt ze souboru |

- Báze je parametr `base`. Na skutečné kartě se nastavuje v `DPI.INI`, výchozí B0h. Zrcadla B2h/B3h karta nemá.
- Kódy A0–A9 jsou příkazy zapsané na B0h, ne porty. S RTC-1 na A0h tedy nekolidují.

## Protokol (podle `repo.c`)

- **Stav** (čtení B0h):
  - bajt 0 = příznaky: 01h PARAMRQ, 02h DOUTRQ, 04h otevřený adresář (nebo GETCONFIG), 08h soubor pro čtení,
    10h soubor pro zápis, 20h konec souboru, 40h IN_PROGRESS, 80h ERROR;
  - bajt 1 = poslední příkaz (F0h čtení a F1h zápis bajtu souboru, po NEXT 41h/42h);
  - bajt 2 = chybový kód, nebo volné místo pro parametry (PARAMRQ), nebo zbývající bajty výstupu (DOUTRQ), nebo
    zbývající bajty záznamu adresáře;
  - bajt 3 = výsledek FatFs posledního příkazu SD karty (4 soubor není, 5 cesta není, 6 neplatné jméno, 7 odepřeno,
    8 existuje, 16 soubor je otevřený…).
- **Ukazatel stavu se vrací na bajt 0** každým zápisem na B0h a každým přístupem na B1h. STSR (03h) nedělá nic
  jiného, chybu nemaže.
- **Parametry:** `B` = bajt, `S` = řetězec ukončený libovolným bajtem < 20h. Volné místo na začátku je
  255 − počet řetězců. Po posledním parametru se příkaz hned vykoná.
- **Datový port bez příkazu:** čtení dá další bajt záznamu adresáře, jinak bajt otevřeného souboru (za koncem 00h bez
  chyby, konec ukazuje bit 5), jinak 00h a ERROR 4. Zápis jde do otevřeného souboru, jinak ERROR 4.
- **Chybové kódy:** 1 neznámý příkaz, 2 špatný parametr, 3 přetečení parametrů, 4 nic ke čtení nebo zápisu (nebo
  soubor otevřený jinak), 5 zaneprázdněno, 6 chyba FatFs (kód v bajtu 3); u NET 6 build, 7 místnost, 8 není
  v místnosti, 9 spojení, 10 parametr, 11 nejde odeslat.
- **Příkazy:**
  - 00 RESET: opustí místnost NET, zapíše a zavře soubor, zavře adresář, CWD = `/`;
  - 01 ASCII a 04 STORNO nedělají nic, 02 SHASCII dá ERROR 1 (jen Sharp);
  - 05 REV `DPI-1L v0.5.1` + 0Dh (14 B), 06 REVD `00 05 44 01` (od 2026-10-26 jako skutečná karta s firmwarem
    0.5.1, REVD stejné od 0.5.0 z 25. 10.; do té doby REVD `00 04 44 01`: 22. 10. večer 0.4.2, ráno 0.4.1, od 2026-10-20 0.4.0; REVD nese jen hlavní a vedlejší číslo; protokol Repository API a NET je stejný
    jako ve v0.2, dřív `DPI-1L v0.2` a `00 02 44 01`),
    07 BOOT (reset SAPI přes /RTL);
  - 11 INTCALLER `00`, 93 WIFISTATUS `03` (připojeno), 95 INFO 16 B `01 01 04 09` + nuly (bit 08h = NET);
  - 20 GETFREE, 21 CHDIR, 22 GETCWD, 30 STAT, 31 UNLINK, 34 RENAME, 40 MKDIR, 41 READDIR, 42 FILELIST, 43 NEXT;
  - 50 OPEN, 51 SEEK, 52 TRUNC, 53 SYNC, 54 CLOSE, 55 TELL, 56 SIZE;
  - 60–63 RTC (datum D, M, R−1980; čas H, M, S);
  - 92 GETCONFIG (záznamy po 80 B: klíč 16 B, hodnota 64 B), 99 SETCONFIG (sekce, klíč, hodnota; prázdná maže);
  - B0 SDCARD (FFh stav, 1 půjčit kartu PC, 0 vrátit);
  - 80 TCPSTATUS, 81 TCPIP, 82 TCPPORT, 83 TCPOPEN, 84 TCPCLOSE, 85 TCPSEND, 86 TCPRECV, 88 DNSREQ, 89 DNSRES;
  - A0–A9 NET: STATUS, CREATE, JOIN, LEAVE, READY, SEND, POLL, HASH, MSG, RECV (tabulka v zadání §3);
  - 32 CHMOD a 33 UTIME po parametrech ERROR 1 (DPI má FatFs bez `FF_USE_CHMOD`).
- **Detekce v BomberNetu:** REVD musí dát stav `02 06 04`; INFO musí být bez ERROR, s výstupem alespoň 16 B a s bitem
  08h v bajtu 3.

## Emulace: SD karta nad složkou PC

- **Parametry** (katalog, konfigurátor):
  - `sd.folder`: složka PC, kořen `/` karty, výchozí `work/dpi-1l/sdcard`. Složka ve `media/` se použije přes
    pracovní kopii ve `work/` jako u PMD32-SD. Jiná cesta (i absolutní, třeba výstup překladu) se použije přímo.
  - `sd.sectors`: velikost karty v sektorech po 512 B, jak ji ukazuje GETFREE (FatFs: clustery krát sektory
    v clusteru). Výchozí 62 309 376 je Martinova SanDisk 32 GB (FAT32, cluster 32 KB, GETFREE `03B6C400h`). Clustery:
    do 128 MB po 2 KB, do 32 GB po 32 KB, nad tím 128 KB. Volné místo = celek minus soubory ve složce (zaokrouhlené
    na clustery), adresáře a kořen po clusteru.
  - `sd.flush_on_exit`: odchylka, výchozí vypnutá (viz stránka níže).
- **Při prvním spuštění** se složka založí se složkou `/DPI` a souborem `/DPI/DPI.INI` s výchozím obsahem, stejným jako
  ve firmwaru (`create_default`).
- **Jména:** Z80 posílá a dostává KOI-8čs2, na PC jsou UTF-8 (`koi8.c` z firmwaru beze změny).
  - Co KOI-8čs2 nemá, dostane rozumnou ASCII podobu, jinak `_`.
  - Hledání jména jde jako ve firmwaru (`look_up`): přesné jméno bez ohledu na velikost písmen (jako FatFs, tedy
    i na Linuxu), pak podoba v KOI-8čs2, nakonec pro jméno v ASCII bez diakritiky, když pasuje jediný soubor. Víc
    shod = FatFs 6.
  - Záznam FILINFO má jméno nejvýš 31 bajtů. Soubor jde najít i podle takto zkráceného jména.
- **Chování FatFs, které program vidí:**
  - jeden otevřený soubor a jeden adresář; READDIR zavře soubor, OPEN zavře adresář a předchozí soubor zapíše;
  - výsledky FR_* v bajtu 3 (OPEN adresáře = 4, mazání neprázdného adresáře nebo CWD = 7, mazání nebo přejmenování
    otevřeného souboru = 16 LOCKED, STAT kořene = 6…);
  - OPEN s režimy FatFs (01 READ, 02 WRITE, 04 CREATE_NEW, 08 CREATE_ALWAYS, 10 OPEN_ALWAYS); bit 20h (SharpASCII) se
    ignoruje; soubor v režimu 0Ah je jen pro zápis (čtení dá ERROR 4 a 00h), číst i zapisovat jde v režimu 0Bh;
  - SEEK od začátku, od konce, vpřed a vzad, před začátek = 0; TRUNC na pozici (dál než konec soubor prodlouží);
  - CHDIR na soubor = 5, STAT se jménem zakončeným tečkou nebo mezerou najde jméno bez nich.
- **Stránka 4 KB jako ve firmwaru:**
  - bajty souboru se čtou a zapisují ve stránce 4 KB;
  - na disk PC jdou až při změně stránky, SYNC, CLOSE, TRUNC, RESET, READDIR nebo OPEN;
  - velikost v adresáři (STAT, READDIR) se mění až při SYNC a CLOSE, jako u FatFs;
  - /RES soubor nezavře a stránku nezapíše, stav Repository API přežije reset;
  - vypnutí emulátoru (i restart sestavy) rozepsanou stránku ztratí jako skutečná karta. `sd.flush_on_exit` ji
    zapíše (odchylka pro pohodlí, výchozí vypnutá).
- **Skryté soubory:** `/DPI/SECRET.INI` a starý `/DPI.INI` v kořeni READDIR nevypíše, STAT ani OPEN je nenajde (zápis
  = odepřeno), nejdou smazat ani přejmenovat. Složku `/DPI` nejde smazat ani přejmenovat.
- **GETCONFIG a SETCONFIG** pracují s `/DPI/DPI.INI` jako firmware (stejné čtení a přepis řádků, limity délek).
  Emulátor se ale tímto souborem neřídí: relay a port jsou parametry karty. Na skutečné kartě SETCONFIG stejně platí
  až po dalším startu.
- **RTC 60–63h:** hodiny PC posunuté o to, co nastavil program. Posun se pamatuje v uloženém stavu.
- **BOOT 07h:** /RTL na 100 ms jako `rtl_pulse` ve firmwaru. JPR-1V a ostatní procesorové karty s /RTL resetují celé
  SAPI.
- **SDCARD B0h:** emulátor nemá USB, kam by kartu půjčil. Stav (FFh) i vrácení (0) dají 00h = karta v DPI, půjčení (1)
  zavře soubor a adresář a vrátí chybu 5 jako DPI bez připojeného PC.
- **Uložený stav** drží CWD, otevřený soubor s pozicí a nezapsanou stránkou, otevřený adresář (při načtení se
  vypíše znovu, jak je teď), proud GETCONFIG a posun RTC. Soubory samotné jsou ve složce.
- **Pořadí položek** v READDIR a FILELIST je jako na FAT pořadí vytvoření: podle času vytvoření na PC. Co vytvořila
  karta (OPEN, MKDIR, RENAME, `/DPI` při prvním spuštění), jde v pořadí vytvoření i v rámci jednoho tiku hodin PC;
  na Windows emulátor čas vytvoření o zlomek posune, aby pořadí platilo i po restartu.
- **Krátká jména 8.3** (`altname` v záznamu FILINFO) dělá emulátor jako FatFs R0.15 s kódovou stránkou 437 (`ff.c`
  `create_name` a `gen_numname`): é → E, Ü zůstane, ž → `_`, mezery a další tečky vypadnou, `~1` až `~5`, pak hash.
  `altname` je prázdné u jména, které je čisté velké 8.3. Krátké jméno najde soubor i při hledání.
- **Rozdíly proti skutečné kartě, které zůstávají:**
  - FAT zaplní místo po smazané položce novou, emulátor ji dá na konec;
  - čas souborů je čas PC, karta bere čas z RTC (NTP);
  - `/DPI/LOG.TXT` (záznamy firmwaru o startu a SETCONFIG) emulátor nepíše, `DPI.INI` má výchozí obsah;
  - atributy: adresář 10h, soubor 20h, soubor jen pro čtení na PC 01h.

## Emulace: TCP a DNS (podle `net.c`)

- **Jedno spojení klienta** přes síť PC, ve vlastním vlákně (`TcpLink` v `src/core/relay.*`).
  - TCPSTATUS 10 B: bajt 0 připojeno (0/1), bajt 1 stav (0 nečinné, 1 připojuje, 2 připojeno, 3 selhalo, 4 zavřela
    protistrana), WORD bajtů k přečtení, IP (první oktet první), port.
  - TCPOPEN se dokončí na pozadí; když se do 10 s (času PC) nepřipojí, TCPSTATUS ukáže 3.
  - Příjem čeká ve frontě 16 KB. Plná fronta přestane číst, okno TCP pak zdrží odesílatele, nic se neztratí.
  - TCPRECV vrátí nejvýš 253 bajtů (WORD počet, pak data). Po zavření protistranou (stav 4) jde přijaté dočíst.
  - TCPSEND s délkou nad 252 B = chyba 2 už při parametrech. Bez spojení se data tiše zahodí (jako Unicard). Když
    odeslání nejde, chyba 5.
  - RESET 00h spojení zavře a DNS zapomene odpověď (`f3e583b`, jako místnost NET); adresa a port zůstanou. /RES
    spojení nezavře. TCPCLOSE ho zavře, přijaté bajty jdou dočíst.
  - Spojení na port, kde nic neposlouchá, ohlásí karta hned (stav 3), PC na Windows až po asi 2 s pokusů.
  - Cíl TCPOPEN, dokud ho program nenastaví: parametr `net.tcp_default` (`a.b.c.d:port`, jako `[net] tcp_default`
    v `DPI.INI`), výchozí 0.0.0.0:0.
- **DNS:** DNSREQ jméno (číselná adresa je hotová hned), DNSRES stav (0 nic, 1 hledá, 2 nalezeno, 3 neznámé jméno,
  4 špatný parametr) a IP. Hledání běží ve vlastním vlákně (jen IPv4), po 5 s bez odpovědi je výsledek 3. Každý
  DNSREQ IP vynuluje, neúspěch tedy ukáže `03 00 00 00 00` (`f3e583b`).
- **Bajt na B1h, který nic nepřijme** (žádné parametry, žádný soubor), nastaví ERROR 4, jen když ještě žádná chyba
  není: TCPSEND s délkou 253 nechá chybu 2 i po zbytku dat (`f3e583b`).
- **Uložený stav** drží adresu, port a stav. Spojení ani hledání se uložit nedá: po načtení je spojení zavřené
  (stav 4, nebo 3 při připojování) a hledání skončí jako neznámé jméno.

## Emulace: NET

- **Parametry:** `net.transport` (`ws` WebSocket, výchozí, nebo `tcp` řádky JSON), `net.host` (`api.mzpico.com`),
  `net.port` (80).
  - Relay jde změnit i za chodu, karta přitom opustí místnost (jako příkaz `ngame relay` na konzoli DPI).
  - Pro lokální `relay.py` z repa BomberNet: `tcp` s portem 8766, nebo `ws` s portem 8765. `relay.py` poslouchá jen
    na 127.0.0.1.
- **Bez čekacích taktů.** Skutečná karta drží Z80 ve WAIT, dokud přístup neskončí. Emulátor odpoví hned.
- **CREATE a JOIN běží na pozadí:**
  - stav má bajt 0 = 40h, dokud relay neodpoví (`room`, `error`), nebo dokud nevyprší limit 10 s;
  - limit se měří časem PC, ne takty emulace, protože relay běží v reálném čase;
  - nový příkaz během čekání čekání zruší a zavře spojení, /RES čekání ukončí (stav 00h bez chyby).
- **Spojení:**
  - každé CREATE a JOIN otevře nové spojení s relay ve vlastním vlákně;
  - řádky od relay čekají ve frontě bez omezení délky a karta je zpracuje při nejbližším přístupu Z80 na své porty;
  - Z80 nic jiného nevidí, výsledek je stejný, jako kdyby se zpracovaly hned;
  - LEAVE, RESET 00h i /RES pošlou `leave` a teprve pak spojení zavřou (FIN za odeslanými daty).
- **WebSocket je vlastní**, jako ve firmwaru:
  - handshake s `Host` bez portu, `Sec-WebSocket-Accept` se neověřuje;
  - rámce klienta maskované, celá zpráva jedním zápisem, ping → pong se stejným obsahem;
  - `wss://` nejde (SAPIemu nemá TLS), produkce má proto i `ws://api.mzpico.com/net`.
- **Uložený stav:** spojení se uložit nedá. Po načtení stavu je karta mimo místnost a čekající CREATE/JOIN skončí.
- **Bez sítě** se karta chová jako skutečná, které se nepodařilo spojit: CREATE a JOIN skončí chybou 9. S prázdným
  `net.host` hlásí STATUS stav 0 (NOLINK).
- **Konfigurátor** ukazuje za chodu stav sítě (např. „NET: running, room SYTN, slot 0, 2 members“). MCP
  `machine_info` vypisuje relay, stav, kód místnosti, slot, poslední chybu, složku SD karty, CWD a otevřený soubor.

## Ověření

- **Test `test_dpi1l`:** NET s malou falešnou relay v testu (TCP řádky i WebSocket):
  - záznamy ze skutečné karty bez místnosti (zadání §6) a detekci BomberNetu;
  - hru dvou hráčů ze záznamu §6 bajt po bajtu, přes TCP i WebSocket (s pingem);
  - JOIN jako divák a jako hráč, chyby relay 6 a 7, limit → chyba 9, nedostupnou relay;
  - ztrátu spojení ve hře (stav 5, chyba 9), `desync`, `dropped`;
  - LEAVE, RESET 00h, /RES a načtení stavu: vždy `leave` před koncem spojení;
  - zrušení CREATE novým příkazem (i s `room`, který Z80 ještě nepřečetl), /RES během CREATE, kód JOIN delší než
    7 znaků.
- **Test `test_dpi1l_files`:** SD karta nad čistou složkou `work/test/`: adresáře, zápis a čtení, stránka 4 KB a
  velikost v adresáři, SEEK, TRUNC, /RES s otevřeným souborem, READDIR a FILELIST (dlouhé a české jméno, krátké
  jméno), NEXT, dohledávání jmen (bez diakritiky, víc shod, zkrácené jméno), RENAME, UNLINK, LOCKED, skryté soubory,
  GETCONFIG a SETCONFIG, RTC, BOOT, SDCARD, uložený stav s otevřeným souborem, RESET, plná karta 1 MB.
- **`DPI.COM` z repa firmwaru v CP/M v emulátoru (2026-10-16):** kopie `sapi1v_BomberNet.sapi` (disky bez ukládání),
  `DPI.COM` nahraný do paměti a uložený příkazem `SAVE 50 DPI.COM`. INFO (`DPI-1L v0.2`, `31 MB free of 31 MB`),
  DIR, GET a TYPE, MD, CD, PUT (12800 B), REN, DEL, DEL neexistujícího (`file not found (FatFs 4)`), PWD a SDCARD PC
  (`busy (error 5)`) dělají, co mají.
- **TCP a DNS v `test_dpi1l`:** falešný server v testu: stav, adresa a port, připojení, odeslání, příjem po
  částech, TCPSEND 253 B, RESET spojení nezavře, zavření protistranou, odeslání bez spojení, nedostupný port, DNS
  číselné, `localhost`, neznámé jméno a prázdné jméno.
- **TCP ručně 2026-10-16:** přes MCP: DNSREQ `example.com`, TCPOPEN na port 80, `GET / HTTP/1.0`, příjem celé
  odpovědi (`HTTP/1.1 200 OK`, 891 B) až do zavření protistranou.
- **NET ručně 2026-10-16:** `sapiemu-cli --mcp` s kartou ovládanou přes `io_write`/`io_read` jako konzolí DPI a
  `tools/net_peer.py` z repa firmwaru jako druhý hráč. Všechny tři cesty daly stejné bajty jako záznam §6 (jiný byl
  jen kód místnosti a seed): `relay.py` přes TCP 8766 s druhým hráčem přes WebSocket 8765, `relay.py` přes WebSocket,
  produkční `ws://api.mzpico.com/net`.

## Otázky k firmwaru a odpovědi (2026-10-16)

První verze emulátoru se řídila firmwarem v0.2 i tam, kde se lišil od zadání. Autor rozdíly probral se session
firmwaru. Body 1–3 byly odchylky firmwaru, firmware je opravený (`b72a8d0`, ověřeno na kartě) a emulátor se chová
podle zadání (`..\SAPI_DPI-firmware\docs\sapiemu_dpi.md` §9):

1. **/RES během CREATE/JOIN** opustí místnost a stav opustí IN_PROGRESS: fáze DONE bez chyby, stav `00 A1 00 00`.
   Ostatní stav Repository API (poslední příkaz, chyba) zůstává.
2. **Zrušené CREATE/JOIN** (nový příkaz během čekání) zavře spojení. Pozdější `room` kartu do místnosti nedá. Platí
   to i pro `room`, který už přišel, ale Z80 si ho nepřečetl, protože mezitím zadal jiný příkaz.
3. **JOIN s kódem delším než 7 znaků** dá chybu 10 (`"ABCDEFGH"` → `80 A2 0A 00`).
4. **RTC 60–63h a BOOT 07h** mohly zůstat ERROR 1. S druhou částí (soubory) jsou hotové podle firmwaru.

## Porovnání se záznamy ze skutečné karty (2026-10-16)

Záznamy `sapiemu_dpi_repo.md` (A–G) jsou v testu `test_dpi1l_files` bajt po bajtu, jen datum a čas souborů v záznamu
FILINFO se nesrovnávají. Podle nich se upravilo:
- velikost karty (`sd.sectors`, GETFREE celkem stejné jako karta);
- pořadí položek (pořadí vytvoření) a krátká jména podle FatFs s kódovou stránkou 437 (`DLOUHE~1.TXT`, `_ABA~1.TXT`,
  `_ABA~2.TXT`, `ÜNICODE.TXT`);
- opravy firmwaru `f3e583b`: RESET zavře TCP a zapomene DNS, DNSREQ vynuluje IP, chyba odmítnutých parametrů zůstane.

Souhlasilo bez úprav: stavové bajty všech příkazů, stránka 4 KB a velikost v adresáři až po CLOSE, /RES s otevřeným
souborem (B.3), režim 0Ah jen pro zápis (B.2), hledání jmen (C.3), skryté soubory, GETCONFIG a SETCONFIG, RTC,
BOOT (100 ms), SDCARD, TCP (F). TCP v testu `test_dpi1l` jde proti falešnému serveru, F samo proti `example.com` je
ručně ověřené výše.
