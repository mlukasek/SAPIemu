# RKD-1 – řadič kazetového disku (CM 5403 / CM 5400)

Stav: **emulovaná** (2026-10-09), karta `RKD-1` (`src/cards/rkd1.*`) s mechanikou CM 5403 nebo CM 5400, sestavy
`machines/rkd1_tesla_cpm.sapi` (CM 5403, CP/M Tesly) a `machines/rkd1_jb_cm5400.sapi` (CM 5400, FORMRKD J. Bíby),
test `tests/test_rkd1.cpp`. Dvoudeskový řadič RKD-1A („deska řízení“) a RKD-1B („deska dat“) pro 1 až 4 mechaniky
kazetového disku. Emulace je ověřená originálním softwarem Tesla Elstroj a JaMeSoft 1987 (formátovač, generátor
systému, boot ROM, CBIOS), který na ní běží beze změn. **Návod k obsluze** (start, shell SH, formátování,
kopírování, výměna kazet): `docs/navod-rkd1.md`.

Zdroje:
- [R1] `..\SAPI_DOKU\SAPIMAN\Scany dokumentací\RKD1.pdf`: původní předvýrobní dokumentace, 38 stran skenu
  (jen obrázky). Strany 1–12 popis, 13–16 konektory X2/X3 a kabel SM 5400, 17–19 rozpisky, 20–23 desky,
  24–38 schémata RKD-1B (listy 1–4) a RKD-1A (listy 1–3);
- [R2] `..\SAPI.CZ\sapi\rkd-1.php`: OCR stran 1–12 a konektorů [R1] (pozor: v tabulce bitů má prohozené sloupce
  portu B a C, správně je to podle ovladače níže). Schémata překreslená v `..\SAPI.CZ\sapi\pdf\rkd-1a.pdf`,
  `rkd-1b.pdf`, repliky EC1045 (rev 2.0, 2011) v Eagle `..\SAPI_DOKU\sapi_eagle\REPLIKY\RKD-1\` (`RKD-1A.pdf` je
  vektorové, spoje jdou dohledat podle úseček; `eaglecon` na starém `.sch` čeká na dialog);
- [R3] ovladač `RKD.MAC` 87-02-13 (Tesla Elstroj & JaMeSoft): `..\SAPI_DOKU\SAPIMAN\Driver RKD-1\drv_rkd1.txt`,
  také na disketách `..\SAPI_DOKU\SAPIMAN\PMD32-SD_IMAGE8\ZDROJ_STARE\a_mercl.8sd` a `b_mercl.8sd`;
- [R4] CBIOS CP/M `CBES.MAC` 87-02-18: `..\SAPI_DOKU\SAPIMAN\HDD projekt\PJRC\PROGRAM\BIOSRKD_macra.txt`
  (a `BIOSRKD.txt`, `...\zdrojemercl\cbes.txt`);
- [R5] boot ROM `ROMES.MAC` 87-02-10: `...\HDD projekt\PJRC\PROGRAM\BOOTRKD.txt`; na `a_mercl.8sd` je novější
  překlad `ROMES.COM` 87-02-13;
- [R6] formátovač `FORMTRKD.PAS` (Turbo Pascal, rozhraní `TIRKD.MAC`) 87-02-12: `...\zdrojemercl\formrkdpas.txt`,
  `tirkd.txt`;
- [R7] FORMRKD v1.1 J. Bíby (20. 10. 1987, „Formátování a kontrola disku SM 5400 (203*12*512)“):
  `..\SAPI_DOKU\SAPIMAN\SAPI1_ARCHIV\ze SAPI\ZDRMOJE\formrkd.txt`, `FORMRKD.MAC` na `a_zdrmoje_rom.8sd`,
  `FORMRKD.COM` na `SYSPROG\a_sluzebni CPM.8sd` a dalších (`docs/software/SAPIMAN_diskety.md`);
- [W1] historie SMEP (VÚVT Žilina): https://vystava.sav.sk/wp-content/uploads/SMEP.pdf;
- [W2] ruské zdroje o СМ 5400: https://nevlabs.ru/about/museum/magnetic-disks/,
  https://huntersfoto.livejournal.com/16638.html, https://zx-pk.ru/threads/12561-zapusk-periferii-sm-evm/page10.html.

## Mechaniky

- **CM 5403, Zbrojovka Brno:** kazetová disková paměť z programu SMEP. Zbrojovka ji předložila na mezinárodní
  zkoušky SMEP I v Moskvě 1977–78 [W1]. Technický popis mechaniky se nenašel. Kabel RKD-1 má „stejné obsazení
  špiček konektorů jako je tomu u paměti CM 5403 se Zbrojovky Brno … nebo použít přímo kabely, které jsou
  s pamětí dodávány“ [R2].
- **CM 5400, Bulharsko (IZOT):** 14" mechanika s jedním pevným a jedním výměnným diskem, kazeta se zakládá
  shora (styl IBM 5440). Asi 2,45 MB na disk, celkem 4,9 MB [W2]. V ČSSR v podsystému kazetových diskových
  pamětí SM 3-20 a SM 4-20 („CM 5400 s kapacitou 5 MB“), magnetické diskové paměti se pro SMEP dovážely z BĽR
  [W1]. Předchůdce IZOT 1370 byl podle [W2] kopie IBM 5440. Ruská fóra uvádějí programovou kompatibilitu
  s DEC RK05/RK11 (nedoložené). Otáčky [W2] 4500/min si odporují s [R2] (2400/min), věřit [R2].
- **Společné (z popisu rozhraní [R1]):**
  - v jednotce jsou dva disky, pevný a výměnný (signál DISK, hladinový), každý se dvěma povrchy (HLAVA): 4
    „média“ na jednotku, až 4 jednotky (SEL0–3), celkem 20 MB;
  - 204 stop (0–203), „dvěstě a čtyři reservní“. Číslo větší než 203 disk odmítne (NSA);
  - otáčky 2400/min (25 ms na otáčku). Disk je napevno sektorovaný: sektorový impulz SEK 40 µs „12x nebo 24x
    za otáčku“, číslo sektoru na S1–S16. Index se nepoužívá;
  - separátor dat a hodin je v mechanice (RD, RC); zápis WD „společně s hodinami“, bit 400 ns (2,5 Mbit/s,
    3,2 µs na bajt);
  - časový limit vystavení hlav 0,5 s (STO);
  - všechny signály rozhraní jsou aktivní v nule.
- **Rozdíl Zbrojovka / BLR [R2]:** signál mazání ERS „není u disků ze Zbrojovky a mazání se odvozuje ze zápisu.
  U disků z BLR je tento signál povinný“. Signály BLR, které RKD-1 nepoužívá: vysoká hustota záznamu, nízké
  napájení, +5 V pro zakončovací odpory (jen u poslední jednotky).
- **Geometrie a DEC RK05:** formát 12 × 512 B „kompatibilní se SMEP“ odpovídá RK05 (203 cylindrů, 12 sektorů po
  256 slovech, IBM 2315). RK05 se ale točí 1500/min, tady 2400/min, mechanicky to tedy přímý klon není.

## Kabel SM 5400 ([R1] str. 15, ručně psaná tabulka „SM 5400 – RKD-FRB“)

Konektory mechaniky 01–12 po 6 kolících (ruské názvy), vpravo signál a kolík RKD-1A (A) / RKD-1B (B):

| SM 5400 | kolík | RKD | | SM 5400 | kolík | RKD |
|---|---|---|---|---|---|---|
| výběr jednotky 0–3 | 02-1…4 | SEL0–3 A1, A2, A3, A5 | | adresa sektoru 2⁰–2⁴ | 01-1…5 | S1–S16 B6, B7, B9, B10, B11 |
| výběr disku | 02-5 | DISK A18 | | povel „čtení“ | 08-1 | RD A22 |
| výběr hlavy | 02-6 | HLAVA A19 | | povel „ochrana“ | 08-2 | WPP A23 |
| **adresa cylindru 2⁰** | 04-1 | **T00 A17** | | konec vystavení | 08-3 | SRDY B13 |
| adresa cylindru 2¹–2⁵ | 04-2…6 | T1–T16 A6, A7, A9, A10, A11 | | adresa potvrzena | 08-4 | APT B17 |
| adresa cylindru 2⁶, 2⁷ | 06-1, 06-2 | T32, T64 A13, A14 | | neplatná adresa | 08-5 | NSA B18 |
| **adresa cylindru 2⁸** | 05-2 | **T128 A15** | | index | 08-6 | – |
| strob cylindru | 06-3 | STB A27 | | sektor | 10-1 | SEK B21 |
| **návrat (восстановление)** | 06-4 | **–** | | chyba přístupu | 10-2 | STO B14 |
| povel „zápis“ | 06-5 | WR A21 | | porucha | 10-3 | ERR B19 |
| povel „mazání“ | 06-6 | ERS A26 | | jednotka připravena | 10-4 | UNR B15 |
| vysoká hustota | 05-3 | – | | stav „ochrana“ | 10-5 | WPD B23 |
| nízké napájení | 01-6 | – | | pozornost 0–3 | 10-6, 12-2…4 | ATN0–3 B1, B2, B3, B5 |
| napájení zakončení | 05-6, 09-1 | nezapojovat | | data zápisu | 12-1 | WD A29 |
| | | | | data čtení / synchro | 12-5, 12-6 | RD B29, RC B27 |

**Zvláštnost:** adresa cylindru je u SM 5400 devítibitová a kabel ji posouvá o bit: T00 (u Zbrojovky
bezadresový návrat na stopu 0) jde na 2⁰, T1 na 2¹ … T128 na 2⁸, a návrat mechaniky není zapojený. Číslo stopy
z řadiče N tak dá cylindr 2N + T00. Viz Nejasnosti.

## Řadič

- **RKD-1A** [R1 str. 18]: MHB 8255A, KR580VI53 (8253), PROM MH 74188 (dekodér adres, dump není), 74LS138,
  74LS174, 2× MH 3216, 9× 75452 (budiče linek), MHB 8286, 74145, konektory X1 (sběrnice), X2 (disk), X3 (k RKD-1B).
- **RKD-1B** [R1 str. 17]: krystal 10 MHz, 2× MHB 2114 (1 KB vyrovnávací RAM pro jeden sektor), 74164 (posuvný
  registr), 2× 74LS374, 5× MH 7493A, 74123, 74192, MH 74151, 74153, 74LS25x (v rozpisce nečitelné), 74LS139, MHB 8286.
- **Porty** (základ 40h, [R2], [R3]):

  | Port | Funkce |
  |---|---|
  | 40h–42h | 8253 čítače 0–2; 43h řízení 8253 (14h, 5Ah, B0h) |
  | 44h | 8255 PA: číslo stopy T1–T128 |
  | 45h | 8255 PB: stav (vstup) |
  | 46h | 8255 PC: řízení (výstup) |
  | 47h | řízení 8255 (82h: PA, PC výstup, PB vstup) |
  | 48h | zápis: STB (strob adresy stopy, 500 ns) |
  | 49h | zápis: RUN (start čtení nebo zápisu sektoru) |
  | 4Ah | zápis: D7 = nulování ukazatele RAM, D0–D4 = číslo sektoru **inverzně** |
  | 4Bh | zápis: D1 = WPP (zapnout ochranu zápisu), D0 = T00 (bezadresový návrat na stopu 0) |
  | 4Ch | data vyrovnávací RAM (čtení i zápis, ukazatel se posouvá sám) |
  | 4Dh–4Fh | obsazené, nepoužité |

- **PB (45h)** [R3]: bit 0 BUSY (řadič čte nebo zapisuje), 1 WPROTECT, 2 DATA_CNT_OK (počet bajtů přenesen), 3
  TRACK_NO_ACK (APT). Bity 4–7 podle PC7: PC7 = 1 → SEEK_RDY, SEEK_TOUT, UNIT_RDY, ERROR (aktivní v nule),
  PC7 = 0 → ATN0–3.
- **PC (46h)** [R3]: PC0 povolení budičů čísla stopy, PC1–PC2 výběr jednotky (ovladač Tesly je nechává 0 = jen
  jednotka 0), PC3 RD_ENBL, PC4 WR_ENBL, PC5 hlava, PC6 disk, PC7 přepínač stav / ATN.
- **8253:** čítač 0 dělí 2 MHz dvěma (krok 1 µs), čítač 1 (režim 5) zpoždění od konce sektorového impulzu: zápis
  N = 3, čtení N = 20; čítač 2 (režim 0) počet bajtů. RUN se zruší resetem, dopočítáním bajtů, chybou, nebo
  „nezkomparuje-li se sektor ani po 256 příchodech sektorového impulsu“.

## Formát sektoru [R1], [R3]

- 26 × 00h, 80h (synchronizace), 2 bajty hlavičky (vyšší první), data (512 nebo 256 B), 2 bajty součtu jen dat
  (16 bitů, nižší první), 2–3 × 00h. Čtení přeskočí nuly a 80h a uloží hlavičku, data a součet (len + 4).
- Hlavička [R3]: bit 15 velikost (0 = 256 B, 1 = 512 B), 14 médium (disk), 13 povrch, 12–5 stopa 0–203,
  4–0 sektor 0–11 nebo 0–23.
- Dva formáty:
  - **12 × 512 B „kompatibilní se SMEP“** (FORMRKD J. Bíby pro SM 5400 [R7]);
  - **24 × 256 B** (systém Tesla Elstroj [R4]–[R6]: „2x2x203x24x256“). Celý zápis sektoru 290 B × 3,2 µs = 0,93 ms
    se vejde do 1,04 ms mezi impulzy.

## Software

- **CP/M 2.2 Tesla Elstroj** [R4]: „A:..D: RKD-1 kazetový disk 2.37 Mbyte 2x2x203x24x256“, každé médium jedna
  jednotka CP/M, stopa CP/M = fyzická stopa, sektor = logický / 2. DPB: výměnný disk (A:, B:) 204 stop, 2 systémové,
  1212 KB; pevný (C:, D:) 1224 KB, bez systémových stop. Studený start z média 0, stopy 0 a 1 [R5].
- **Boot ROM** [R5]: nahraje 1800h bajtů ze stopy 0 a zbytek ze stopy 1, skočí do BIOSu (systém 62 KB).
  `ROMES.COM` na `a_mercl.8sd`. Na téže disketě jsou `MOVCPM.COM` a `STARTSYS.SUB` (generování systému).
- **Formátovače:** `FORMTRKD` [R6] (24 × 256, výplň E5h) a `FORMRKD` [R7] (12 × 512, SM 5400; pevný disk D: dolní,
  E: horní, výměnný F:, G:).
- Zdrojáky jdou přeložit M80/L80 přes `cpmrun` (`docs/build.md`), jako u ECMONu.

## Emulace (2026-10-09)

- **Karta `RKD-1`** zabírá dva sloty (desky A + B), porty 40h–4Fh (dekodér PROM MH 74188 nemáme, adresy jsou
  z popisu a ovladače). 8255 a 8253 jsou čipy emulátoru, 1 KB RAM s ukazatelem, porovnání čísla sektoru (zapsané
  inverzně), RUN, STB, T00, WPP. Bity PB jsou surové stavy linek rozhraní, jak je čte ovladač. V konfigurátoru má
  každá deska dva FRB 30: horní X3 propojuje desky kabelem (jako u RPD-1), dolní X2 vede k mechanice („K2 DISK“).
- **T00 je hladina, ne pulz** (schéma repliky, list RKD-1A): klopný obvod D7CA bere DB0 při zápisu na 4Bh a nuluje
  ho invertované PC0 (D4BA). T00 a adresa stopy (budiče povolené PC0) se proto vylučují: `RK_HOME` pošle T00 se
  STB, `RK_SEEK` nastaví PC0 a tím T00 smaže. WPP je pulz zápisu na 4Bh s DB1, STB pulz zápisu na 48h.
- **Dvě mechaniky** (volba `drive` u každé jednotky, `CM 5403` výchozí):
  - **CM 5403** (Zbrojovka): DISK neaktivní = kazeta, T00 = bezadresový návrat na stopu 0 (nuluje chybu), stopa
    řadiče = cylindr. Pro ni je CP/M Tesly.
  - **CM 5400** (IZOT) na kabelu SM 5400 podle [R1] str. 15: DISK neaktivní = **pevný** disk (FORMRKD J. Bíby: D:, E:
    pevný, F:, G: výměnný), T00 na bitu 2⁰ adresy cylindru a T1–T128 na 2¹–2⁸, takže stopa N je cylindr 2N
    a `RK_HOME` (jen T00) najede na cylindr 1; návrat mechaniky zapojený není. Mechanika bere cylindry do 407
    (předpoklad, viz Nejasnosti), vystavení přes celý disk trvá stejně jako u CM 5403 (0,185 ms na cylindr). Nové
    talíře mají 12 × 512 B (SMEP). Lichý cylindr (jen po `RK_HOME`, ovladač před přenosem vždy vystavuje) se bere
    jako stopa pod ním.
- **Mechanika (společný model):** 2400 ot./min, 12 nebo 24 sektorových impulzů (vlastnost talíře, kazeta si ji
  nese v obrazu), 204 stop, 2 povrchy, kazeta a pevný disk. Vystavení hlav je odhad podle DEC RK05 téže třídy:
  10 ms + 0,37 ms na stopu (přes celý disk asi 85 ms), stejná stopa 1 ms. Návrat T00 nuluje chybu mechaniky.
  Na chráněný disk mechanika nezapisuje (jako rodina RK05: ochrana jen vypne zápisový proud), řadič bajty odpočítá a
  chyba nevznikne. Ovladač Tesly WPROTECT nečte, CP/M zápis bere jako hotový a kazeta zůstane beze změny (viz
  Nejasnosti). Původní odhad „ochrana = porucha ERR“ systém zasekl: WBOOT ani boot ROM nevolají `RK_HOME`, které
  jediné poruchu maže, a `RK_SEEK` se při chybě vrací s `PUSH BC` na zásobníku (chyba originálního ovladače).
- **Časování RUN:** porovnání čeká na impulz zadaného sektoru, pak 40 µs impulz, zpoždění čítače 1 (µs) a bajty
  čítače 2 po 3,2 µs. Při čtení datový separátor přeskočí nuly a 80h. Sektor, který se neobjeví (číslo větší než
  počet sektorů) nebo nemá data (nenaformátovaný), ukončí RUN až po 256 sektorových impulzech bez DATA_CNT_OK, jak
  píše popis řadiče. Ovladač stihne jeden sektor na otáčku (formátovač zapisuje 1:1), takže formátování jednoho
  povrchu trvá asi dvě minuty.
- **Obraz kazety `.rkd`:** hlavička `SAPIRKD1`, počet sektorů (12/24), 204 stop, 2 povrchy, pak pro každý sektor
  délka a bajty, které do něj řadič zapsal (26 × 00h, 80h, hlavička, data, součet). Zachová se tak formát obou
  druhů, rozbitý sektor i zápis přes konec sektoru (zničí následující sektor). 12 sektorů: 636 B na sektor,
  24 sektorů: 312 B (čas mezi impulzy). Soubor má asi 3 MB.
- **Kazeta a pevný disk:** u CM 5403 DISK neaktivní (PC6 = 0) = kazeta, aktivní = pevný disk. Polarita je podle
  CBIOSu Tesly, který bootuje z média 0 a má na A:, B: systémové stopy (DPB výměnného disku). U CM 5400 obráceně
  (FORMRKD). Pevný disk je soubor
  `work/rkd-1/unitN_fixed.rkd`, vytvoří se nenaformátovaný. Bez kazety mechanika neběží (UNR neaktivní).
- **Okno Média:** jednotky s typem mechaniky, kazeta (Vložit, Nová kazeta s volbou 24 × 256 nebo 12 × 512,
  Vysunout, Ochrana proti zápisu), pevný disk. Nová kazeta je nenaformátovaná: formátuje se v CP/M programem
  `FORMTRKD` (Tesla, 24 × 256) nebo `FORMRKD` (J. Bíba, 12 × 512).
- **Pomoc emulátoru „Systém…“ (SYSGEN, 2026-10-09):** u kazety 24 × 256 v CM 5403. Originální SYSGEN pro RKD-1 se
  nenašel, a bez něj nejde udělat novou systémovou kazetu (WBOOT i boot ROM čtou systém ze stop 0 a 1 média 0, SH
  potřebuje své soubory na A:). Tlačítko zkopíruje stopy 0 a 1 prvního povrchu ze systémové kazety
  `media/rkd-1/tesla_cpm_system.rkd` bajt po bajtu (záznamy sektorů včetně hlaviček), soubory na A: a B: nechá.
  Volitelně předtím kazetu naformátuje stejnými bajty, jaké zapisuje FORMTRKD přes `RKD.MAC` (výchozí u
  nenaformátované kazety; test to porovná se sektory systémové kazety). V dialogu je to označené jako pomoc
  emulátoru. Postup s kopírováním SH přes pevný disk je v `docs/navod-rkd1.md`.
- **Pohodlí:** `spinup_s` (rozběh mechaniky po vložení kazety, výchozí 0 = hned; skutečnou dobu neznáme).
- **MCP:** `machine_info` ukáže jednotky, stopu, kazetu, pevný disk, formát a ochranu.

## Sestava a systémová kazeta

- **`machines/rkd1_tesla_cpm.sapi`:** JPR-1Z s boot EPROM `roms/jpr-1z/ROMES_RKD1_870210.bin`, RAM-1Z 62 KB, AND-1Z,
  Consul 259.11, RPD-1 (E: a F: služební diskety J. Bíby s PIP, STAT, FORMRKD), RKD-1 s kazetou
  `media/rkd-1/tesla_cpm_system.rkd`. Po startu: „Boot ROM pro RKD-1“, „Cbios 87-02-13“, shell SH se `STARTSYS.SUB`
  („zadej prosím své číslo uživatele“), výzva `b1$`.
- **Boot EPROM:** `ROMES.COM` na disketě `a_mercl.8sd` (87-02-13) **není obraz EPROM**: je přeložený na 0100h
  (`JP 0176h`, kód v souboru od 0076h), spustitelný pod CP/M jako zavaděč systému z RKD-1. EPROM proto vznikla
  překladem zdrojáků z téže diskety (ROMES 87-02-10, AND1Z, C25911, NOMOTOFF, RKD) přes M80/L80 (`cpmrun`,
  `L80 /P:0,/D:F700,ROMES,AND1Z,C25911,NOMOTOFF,RKD,ROMES0/N/X/E`): kód 0000h–0550h, data F700h (komentář „/d: pro
  l80“ ve zdrojáku). Binárka 87-02-13 má na začátku dvě volání navíc, novější zdroják se nenašel.
- **Výroba kazety** (opakovatelná: `RKD_MAKE=media/rkd-1/tesla_cpm_system.rkd test_rkd1`), vše originálními
  programy kromě kroku 3:
  1. CP/M J. Bíby z RPD-1, B: = `a_mercl.8sd`, RKD-1 s novou kazetou; `B:FORMTRKD` naformátuje A: a B: (E5h);
  2. `B:ES 59` (MOVCPM Tesly s CBIOS 87-02-13) postaví 59K systém a skočí do něj; v tu chvíli (CBIOS na E400h) je
     v CE00h–F6FFh přesně systém, jak ho boot ROM nahrává;
  3. **náš SYSGEN** (originální pro RKD se nenašel): systém se zapíše na médium 0, stopa 0 sektory 0–23
     (CE00h–E5FFh) a stopa 1 sektory 0–16, přesně odkud ho čtou ROMES a WBOOT CBIOSu;
  4. boot z kazety přes ROMES, `F:PIP A:=E:*.*` (soubory `a_mercl.8sd`: zdrojáky, SH, STARTSYS, FORMTRKD, ES,
     MOVCPM, WM…) a `PIP.COM`.
- **Sestava `machines/rkd1_jb_cm5400.sapi`:** CP/M J. Bíby z 8" diskety (RPD-1, MON 271111), C: RAM disk ZRMD-1,
  B: služební disketa s `FORMRKD.COM` v1.1, RKD-1 s CM 5400 (nová nenaformátovaná kazeta a pevný disk). CP/M pro
  RKD-1 s CM 5400 (12 × 512) jsme nenašli, FORMRKD je jediný program, který ji používá.
- **FORMRKD.COM má starší ovladač než `RKD.MAC` 87-02-13:** jeho `PREP_MARK` jen posune stopu o 5 bitů (hlavička
  = stopa × 32), číslo sektoru, médium ani příznak 512 B do ní nedá a zapisuje nižší bajt první. Zjištěno z bajtů,
  které FORMRKD zapsal, a z binárky (`PREP_MARK` na 1052h). Při čtení porovnává stejnou hlavičku, takže jeho
  formátování a kontrola fungují. Ovladač Tesly by takové sektory odmítl (chybné záhlaví): disky FORMRKD a Tesly
  nejsou záměnné ani při stejném počtu sektorů.
- **Test** (`test_rkd1` ve třech částech, ctest je pouští souběžně: `test_rkd1`, `test_rkd1_cm5400`,
  `test_rkd1_tesla`):
  - nad kazetou z `media/`: start, SH, `FORMTRKD` na pevném disku C:, soubor na C:, zápis na chráněnou kazetu (nic
    se nezapíše, systém běží dál), Reset s kazetou bez systému („Chyba čtení disku A: při boot“) a návrat
    systémové kazety, nová systémová kazeta přes „Systém…“ (soubory SH přes C:, Reset, SH se ptá na uživatele);
  - CM 5400: FORMRKD naformátuje a přečte kazetu F: a naformátuje pevný disk D:, kontrola bajtů a cylindru;
  - výroba systémové kazety výše (když je `..\SAPI_DOKU`).
- **Výměna kazety za běhu** (pokus 2026-10-09): CBIOS drží poslední sektor ve vyrovnávací paměti a výměnu nepozná.
  Po výměně ukázal `DIR B:` adresář staré kazety a další `SAVE` ho zapsal na novou; BDOS výměnu A: poznal (R/O).
  Shell SH bez svých souborů na A: hlásí u každého příkazu chybu čtení A:. Návod proto radí měnit kazetu jen
  s Resetem a pracovat jen se systémovými kazetami (`docs/navod-rkd1.md`).

## Nejasnosti

Autor ani EC1045 nikoho, kdo RKD-1 provozoval, neznají. Co šlo, je rozhodnuté z podkladů (viz Emulace), zbytek
čeká na náhodný nález.

- **Pevný a výměnný disk:** CBIOS Tesly [R4] bere médium 0 a 1 (PC6 = 0) jako výměnný disk a bootuje z něj.
  FORMRKD J. Bíby [R7] má na médiu 0 a 1 pevný disk. Emulace to bere jako rozdíl mechanik (CM 5403 a CM 5400,
  kabel na str. 15 vede DISK přímo), ověřit to nejde.
- **12 nebo 24 sektorů:** dávala mechanika počet sektorových impulzů podle kazety (sektorové značky na disku),
  nebo podle přepínače? Šla kazeta 24 × 256 přečíst na jiné mechanice?
- **Kabel SM 5400 posunutý o bit** ([R1] str. 15): T00 na adresu cylindru 2⁰ a návrat nezapojený. Protože T00 je
  hladina, kterou PC0 maže (schéma), dá řadič s tímto kabelem cylindr 2N pro stopu N a `RK_HOME` najede na
  cylindr 1 (bez skutečného návratu). Vychází to, jen pokud SM 5400 bere aspoň 408 cylindrů (signál vysoké hustoty
  kabel nezapojuje), jinak by FORMRKD (stopy 0–203) skončil chybou na stopě 102. Emulace proto bere 408 cylindrů
  (J. Bíba by program na půlku disku nepsal). CM 5400 má podle [W2] 2,45 MB na disk, tedy jen 203 cylindrů po
  12 × 512 B na dvou plochách: buď je tabulka kabelu pro jinou mechaniku, nebo adresa mechaniky znamená něco jiného.
- **Ovladač ve FORMRKD.COM:** proč má starší `PREP_MARK` (hlavička jen se stopou) než zdroják `RKD.MAC`
  87-02-13, když je FORMRKD v1.1 z 20. 10. 1987? Měl J. Bíba vlastní verzi ovladače, nebo systém pro CM 5400
  s touto hlavičkou?
- **Ochrana proti zápisu:** co udělala CM 5403 se zápisem na chráněný disk? Tiše ho neprovedla (emulace, jako
  RK05), nebo hlásila poruchu ERR / chybu přístupu STO? Ovladač Tesly bit WPROTECT nečte a s poruchou by se systém
  zasekl (viz Emulace), takže chráněnou kazetu Tesla zřejmě nepoužívala.
- **Dekodér PROM MH 74188 (A6 na RKD-1A):** dump není, adresy 40h–4Fh jsou z popisu. Lze přepnout základ?
- **Skutečné kazety:** existuje někde obraz nebo kazeta s daty (CP/M Tesla Elstroj, data ze SMEP)? Emulace
  zatím začne s prázdnými kazetami.
- **Používal někdo RKD-1 se SAPI-1 v provozu** (Tesla Elstroj, JZD Slušovice)? Na disketách TNS jsou zmínky o
  jiném modulu „KDP – řadič kazetových disků“ (KDP-723), bez podrobností.
