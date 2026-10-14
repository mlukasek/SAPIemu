# Otázky na HW podle důležitosti

Roztřídění seznamu `docs/hw-otazky.md` (2026-10-13, 238 otevřených bodů). Ten se generuje z Nejasností v dokumentech
desek a obsahuje všechno, co nevíme jistě, od zapojení po to, kdo napsal hru. **Žádný bod neblokuje vydání:**
emulátor se v těch místech řídí schématem, dumpem nebo firmwarem a programy v sestavách běží. Tady je jen pořadí,
ve kterém má smysl body řešit. Jednotlivé otázky jsou dál v dokumentech desek (odkazy), tento soubor se při změnách
Nejasností upravuje ručně.

Skupiny:

- **A** – odpověď může viditelně změnit chování programů a **jde ji ověřit na autorově pracovišti** (karty, které
  autor má: JPR-1V, JPR-1ZI, JPR-1X, CGA-1V, AND-1V, AND-1B, AND-1Z/89, BGP-1S, DSM-1V, DMR-1, ZRD-1V, RAM-1V,
  MPH-1V, IDE-1, EKL-1). Měřicí programy jsou v `tools/hwbench/`.
- **B** – může změnit chování, ale potřebuje desku, sestavu nebo člověka, které tu nejsou (ZPS-1 až ZPS-4,
  SAPI-1.5, řadiče disket a pásek, přístroje).
- **C** – na emulaci nemá vliv (původ souborů, dokumentace, vzhled panelů, historie), nebo jde o drobnost, kterou
  žádný známý program nepozná.

## A – ověřit na pracovišti (pořadí podle užitku)

1. **JPR-1ZI** ([JPR-1ZI.md](desky/JPR-1ZI.md), [JPR-1Z.md](desky/JPR-1Z.md)): co vrací prázdná sběrnice (JPR-1V
   vrací poslední bajt, a to změnilo start ECMONu na sestavách V; ZI a Z od 2026-10-14 předběžně také, podle
   paměti autora, ověřit), STSTB při INTA (zamrzání FXS4),
   RESET jen s /M1, čip Z80 NMOS/CMOS (`intblock.asm`). Týká se všech sestav s JPR-1ZI a CP/M.
2. **JPR-1X, procesor 8085** ([JPR-1X.md](desky/JPR-1X.md), [8085.md](8085.md)): příznaky V a K, nedokumentované
   instrukce, RIM, časování RST 7.5/TRAP, rezistor na D3 a prázdná sběrnice (MikroMon podle ní zjišťuje paměť),
   snese 4 MHz. Jeden test v ROM přes PicoROM rozhodne většinu bodů najednou.
3. **BGP-1S** ([BGP-1S.md](desky/BGP-1S.md)): počet čekacích taktů (rychlost her, Manic Miner), stav klopného
   obvodu přerušení po zapnutí (falešné přerušení po startu?), čte hra zpět videopaměť.
4. **DSM-1 a DMR-1** ([DSM-1.md](desky/DSM-1.md), [DMR-1.md](desky/DMR-1.md); DMR-1 až s přechodkou FRB → D-Sub):
   reset UARTu do prvního /MR a převzetí znaku na taktu 16× (na DSM-1V změřeno a emulováno, původní MHB1012 zatím
   bez toho), pořadí bitů délky slova v řídicím registru, propojka JP8, bity modemu bez terminálu.
5. **IDE-1** ([IDE-1.md](desky/IDE-1.md)): stav signálů IDE po resetu (8255 PC jako vstup, pull-upy R8/R9), obraz
   skutečného DOM/CF ze sestavy.
6. **EKL-1 a MikroMon** ([klavesnice.md](desky/klavesnice.md), `docs/SOUHRN.md` body 7, 9, 13, 25): tón bzučáku,
   MikroMon s UART (`sw i01`, kanály 10/14/18h, ozvěna bez USYNC), velké `Ž` po Alt-lock. Stačí MikroMon v PicoROM
   a EKL-1.
7. **DGD-1V**, pokud se najde ([DGD-1V.md](desky/DGD-1V.md)): čekací takty a zamrzání při INTA (emulováno podle
   schématu jako CGA-1V).
8. ~~**AND-1V**~~: hotovo (čekací takty a zamrzání při INTA 2026-10-11, rev 2.3 s fontem typ 1 2026-10-13).
9. **Desky V, odpovědi pohledem** (SOUHRN bod 22): revize RAM-1V (1.1, nebo 1.2), propojky DSM-1V, Timekeeper
   v patici C.

## B – může změnit chování, potřebuje jiné desky nebo lidi

Seřazeno podle toho, jak viditelný by rozdíl byl.

- **Barvy a obraz:** DBS-1A pořadí R a G v atributu, blikání, PROM DD2/DD27 ([DBS-1A.md](desky/DBS-1A.md));
  AND-1, AND-1A skládání sloupců 32–63 a font MHB 2501 ([AND-1.md](desky/AND-1.md), [AND-1A.md](desky/AND-1A.md);
  AND-1B je změřená, emulátor AND-1 zatím počítá trochu jinak, liší se jen neviditelné buňky); DGD-1 PROM D1A,
  okno CPU ([DGD-1.md](desky/DGD-1.md)).
- **Sestavy ZPS-1 až ZPS-4** (SOUHRN body 14–20): pořadí RST z 3214 na JPR-1, MIKOS ve 2708, PROM D1A RAM-1,
  RMD-1 význam bitů O3/O4 PROM ([RMD-1.md](desky/RMD-1.md)), boot EPROM a propojky JPR-1A ([JPR-1A.md](desky/JPR-1A.md)),
  ELIS a Consul 262.3, AND-1A 1 KB / 2 KB, prázdné patice JPR-1 a REM-1.
- **SAPI-1.5** ([PKT-1.md](desky/PKT-1.md), [RSW-1.md](desky/RSW-1.md)): polarita MAP1/MAP2/BOOT, /MR při čtení vnitřní
  EPROM, propojky XT4/XT5 a XT1, BUSY tiskárny, EKL-1 s /KACK, watchdog.
- **Disky a pásky:** RPD-1 (rychlost MIKOS/D, čítač 2, TOUT; [RPD-1.md](desky/RPD-1.md)), FDC-1 (8272 po resetu,
  DMA, motory, SA2; [FDC-1.md](desky/FDC-1.md)), MFC-1A ([MFC-1A.md](desky/MFC-1A.md)), RKD-1 ([RKD-1.md](desky/RKD-1.md)),
  RMP-1 a SM 5300 ([RMP-1.md](desky/RMP-1.md)), KZD-1 ([KZD-1.md](desky/KZD-1.md)), kazeta DSM-1 na skutečném
  magnetofonu ([kazeta.md](software/kazeta.md)).
- **Ostatní karty:** DSP-1 (bity SA2, 75154 bez kabelu), RTC-1 (PA v módu 1, kabel /INT), CRM-1 (délka WAIT, SA1),
  DPP-1 (kanál 1, tiskárna), RAM-1Z, ZRD-1V rev 2.1 (okno 74LS123, AIN), JPR-1S (propojky, /RTL), AND-1B starší
  revize, AND-1Z neoznačené vstupy DD4 A7 a DA1 A6, AND-1V rev 1.1 (polovina fontu; autor má rev 2.3).
- **Přístroje:** APS-5 (/PFAIL, kódy kláves HRK-1B, ADP-1), DPS-1 a IMS-2 (NDAC, /OBF, bajt 00h s ATN, odpovědi
  M1T 330), plotter AR 1990 (doraz, LED, kód zrušení zmenšení), Minigraf (rychlost, pero po zapnutí), XY 4130,
  tiskárna (kolík 16 X3, hrana /STROBE).
- **CGA-1V a JPR-1V:** chování po Liborově záplatě IC15 (STSTB jen od /MREQ) a příčina poškození paměti ve FXS4
  (čeká se na Liborův výpis).

## C – bez vlivu na emulaci

Tyto body se mohou vyřešit kdykoli, nebo nikdy:

- původ a verze souborů: `RADICROM.bin` u CGA-1V, poslední `.sch` BGP-1S, popisek 143 ns, kdo napsal hru „auto“,
  ruční oprava FAST v plotteru AR 1990, verze ovladače ve FORMRKD, zdroje OFFSHDD.COM a BIOSu 2015, test TSX 10,
  kdo dělal testy MFC_TEST_CPM;
- dokumentace a tisky: OCR „EB00H“ u DGD-1, BAT41/BAT42, IC4 74HCT04/14 u JPR-1S, prázdný `TEST AND1` v BIOSu 2.1,
  dokument „Programování karty“ CGA-1V, PROM D3D DPS-1 (jen ukázka), zapojení lišty SV2 IDE-1;
- vzhled: panely karet a přístrojů (ZPS, SM 5300, BS 525), poloha obrazu AND-1B o pár bodů, S3 AND-1B, výchozí
  barva AND-1B, směr os myši MPH-1V (žádný program myš nepoužívá), joyport ANK-1M2 (žádný známý program);
- historie: kdo a s čím provozoval RKD-1, XY 4130, DSP-1 v. č. 44127, mechaniky u MFC-1A, firmware ZPS-5, sítě
  „P1-/INT_“ a „P1-STB_“ ve schématu JPR-1ZI;
- drobnosti, které žádný známý program nepozná: přesná zesílení DMT-1 (trimr), parametry SM 5300, převíjení KZD-1,
  RDY při převíjení RMP-1, mezera mezi bloky kazety 210 / 170 ms.

## Hotovo na pracovišti (2026-10-11 a 2026-10-13)

Změny i potvrzení beze změny jsou v `docs/zmeny-podle-hw.md`: DSM-1V (reset UARTu, takt 16×), CGA-1V a AND-1Z/89
(čekací takty), AND-1V (čekání), JPR-1V (prázdná sběrnice, STSTB, zamrzání FXS4, CMOS Z80, takt sběrnice, INTA),
ZRMD-1 a ZRD-1V (reset, čtení 61h–63h, bity MAP), MPH-1V (YM3812 po resetu, stav), CGA-1V (VBI), AND-1B (INT1,
skládání sloupců, blikání), AND-1Z/89 (20 řádků, 109 znaků, čekání), AND-1V (rev 2.3, font typ 1).
