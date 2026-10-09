# RKD-1 – řadič kazetového disku (CM 5403 / CM 5400)

Stav: **neemulovaný**, podklady a zjištění pro emulaci (průzkum 2026-10-09). Dvoudeskový řadič RKD-1A („deska
řízení“) a RKD-1B („deska dat“) pro 1 až 4 mechaniky kazetového disku. Software se zdrojáky je k dispozici
(Tesla Elstroj a JaMeSoft 1987, J. Bíba 1987), takže emulace jde ověřit originálními programy.

Zdroje:
- [R1] `..\SAPI_DOKU\SAPIMAN\Scany dokumentací\RKD1.pdf`: původní předvýrobní dokumentace, 38 stran skenu
  (jen obrázky). Strany 1–12 popis, 13–16 konektory X2/X3 a kabel SM 5400, 17–19 rozpisky, 20–23 desky,
  24–38 schémata RKD-1B (listy 1–4) a RKD-1A (listy 1–3);
- [R2] `..\SAPI.CZ\sapi\rkd-1.php`: OCR stran 1–12 a konektorů [R1] (pozor: v tabulce bitů má prohozené sloupce
  portu B a C, správně je to podle ovladače níže). Schémata překreslená v `..\SAPI.CZ\sapi\pdf\rkd-1a.pdf`,
  `rkd-1b.pdf`, repliky v Eagle `..\SAPI_DOKU\sapi_eagle\REPLIKY\RKD-1\`;
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

## Plán emulace (návrh)

1. Karta `RKD-1` (obě desky jako jedna karta s parametrem typu mechanik `cm5403` / `cm5400`), 8255 a 8253 už
   máme v `src/chips/`. Vyrovnávací RAM 1 KB, sektorový čas z otáček (25 ms / 12 nebo 24).
2. Mechanika: obraz kazety (výměnný disk) a obraz pevného disku zvlášť, 2 povrchy × 204 stop × sektory.
   Uložit raději po bajtech stopy (hlavička, součet), aby šly emulovat chyby MARK_ERR a SUM_ERR a formátování
   oběma formáty.
3. Okno Média: vložit a vyjmout kazetu, ochrana zápisu (WPP / WPD).
4. Sestava s CP/M Tesla Elstroj: přeložit `CBES.MAC` a `ROMES.MAC`, zformátovat obraz `FORMTRKD`, systém zapsat
   na stopy 0 a 1 (nástroj v emulátoru, nebo SYSGEN, pokud se najde verze pro RKD).
5. Test: formátování, zápis a čtení souborů, boot z kazety.

## Nejasnosti

- **Pevný a výměnný disk:** CBIOS Tesly [R4] bere médium 0 a 1 (PC6 = 0) jako výměnný disk a bootuje z něj.
  FORMRKD J. Bíby [R7] má na médiu 0 a 1 pevný disk. Liší se polarita signálu DISK mezi CM 5403 a CM 5400, nebo
  se jeden zdroj mýlí?
- **12 nebo 24 sektorů:** dávala mechanika počet sektorových impulzů podle kazety (sektorové značky na disku),
  nebo podle přepínače? Šla kazeta 24 × 256 přečíst na jiné mechanice?
- **Kabel SM 5400 posunutý o bit** ([R1] str. 15): T00 na adresu cylindru 2⁰ a návrat nezapojený. Má SM 5400
  ve vysoké hustotě 406 (nebo víc) cylindrů a řadič používá jen sudé? Jak pak funguje návrat na stopu 0
  (`RK_HOME` posílá T00)? Byla tabulka úprava pro konkrétní sestavu?
- **Dekodér PROM MH 74188 (A6 na RKD-1A):** dump není, adresy 40h–4Fh jsou z popisu. Lze přepnout základ?
- **Skutečné kazety:** existuje někde obraz nebo kazeta s daty (CP/M Tesla Elstroj, data ze SMEP)? Emulace
  zatím začne s prázdnými kazetami.
- **Používal někdo RKD-1 se SAPI-1 v provozu** (Tesla Elstroj, JZD Slušovice)? Na disketách TNS jsou zmínky o
  jiném modulu „KDP – řadič kazetových disků“ (KDP-723), bez podrobností.
