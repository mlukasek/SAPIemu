# SAPI-1.5: ROM BIOS PKT-1 a CP/M 2.2 62K (Melsoft)

Software pro sestavy SAPI-1.5 (ZPS-5 až ZPS-10, Tesla Liberec 1990). Emulované od 2026-10-07 v sestavě
`machines/zps/ZPS-6.sapi` (PKT-1, RSW-1, MFC-1A, DBS-1A), test `test_sapi15`. Desky: `docs/desky/PKT-1.md`,
`RSW-1.md`, `DBS-1A.md`, `MFC-1A.md`. **Melsoft** = Ing. Jan Meloun (autor 2026-10-30). Od něj má autor sestavu
SAPI-1.5 a diskety, poděkování je v README a v okně O programu.

## Sestavy SAPI-1.5 (Tesla, `..\SAPI.CZ\sapi\doku\sapi15.php`, `doku2\zps.php`)

| Sestava | Rám, sběrnice | Desky | Mechaniky | Dodávky |
|---|---|---|---|---|
| ZPS-5 | dvojitý, SBI-1 | PKT-1, RSW-1 | – | 1990 |
| ZPS-6 | dvojitý, SBI-1 | PKT-1, RSW-1, MFC-1A, DBS-1 | 1 × 5,25" | 1990 |
| ZPS-7 | dvojitý, SBI-1 | jako ZPS-6 | 2 × 5,25" | na poptávku |
| ZPS-8 | jednoduchý, SBK-1/7 | jako ZPS-6 | 1 × 5,25" | 1990 |
| ZPS-9 | jednoduchý, SBK-1/5 | jako ZPS-6 | 2 × 5,25" | na poptávku |
| ZPS-10 | jednoduchý, SBI-1/10 | PKT-1, RSW-1, RTI-1 | – | 1991 |

**ZPS-7 až ZPS-9 nemají vlastní sestavy:** v emulaci se od ZPS-6 neliší (rám a typ sběrnice SBI/SBK emulátor
nerozlišuje). ZPS-7 a ZPS-9 = ZPS-6 s druhou mechanikou (`drive_b` na MFC-1A), ZPS-8 = ZPS-6. Zvláštní software pro
ně nemáme (rozhodnuto s autorem 2026-10-07).

Mechaniky 5,25" DS/DD, CP/M 2.2 na 5,25" disketě s možností práce s 8" disketou. Dokumentace Tesla k PKT-1,
RSW-1, SBK-1, ZDR-1A, ZPS-8 a systémovým parametrům: `..\SAPI_DOKU\SAPI-1.5_dokumentace\`.

## ROM BIOS PKT-1 „PKT-1 62K MON-V100690*LCH“

- **Dva 2KB obrazy, oba se ze zdrojáků přeloží bajt po bajtu** (M80 pod `cpmrun`):
  - `roms/pkt-1/PKT-1_MON-V100690_EPROM.bin` = dump EPROM (`PKT1_ZPS-7.BIN`, MD5 79205be1…), zdroják
    `..\SAPI-karty\PKT-1\System\pkt1.asm`. Výchozí v sestavě. Chyba: LISTST vrací stav klávesnice a tisk čeká na
    BUSY = 0 věčně;
  - `roms/pkt-1/PKT-1_ROMBIOS_MELSOFT.bin` = opravená verze (`pkt1-rombios.bin`, zdroják `pkt1-rombios.asm` =
    Melsoft `ROMBIOS.MAC`, originální výpis `DISK8\ROMBIOS.PRN`): skutečný test BUSY, časový limit tisku, PC4,
    R2 = 4Ah. Nevíme, jestli se vypalovala.
- **Start:** EPROM na 0000h (BOOT), kopie na F000h, `OUT 0Eh,01h`, SP = EEFBh, 8255 87h, inicializace 6845 a MFC-1A,
  hlavička „TERMINAL DBS-1 + CONSUL 262.3 / LST: CENTRONICS“. Pak čte C0 H0 R1 do EF00h a skočí tam (bez chybové
  hlášky opakuje).
- **Vstupy F000–F02Ah:** GCOL0, CONS0, CONI0, CONO0, PRNS0, PRNO0, DSEL0, DHOM0, DSTR0, DSSC0, DRDS0, DWRS0,
  GWBT0 (čtení skupiny sektorů), DFOR0 (zápis skupiny), CSAT0 (barevný atribut). SYSGEN hlásí „Works only with ROM
  MON V060988LCh and later“, existovala tedy i starší ROM z roku 1988.
- **Konzola DBS-1** (64 × 20, VRAM B000h + 2 · ((pozice + rolování) mod 2048), znak a atribut), řídicí kódy: 01h další
  znak doslova, 04h/05h rolování zap./vyp., 08h, 09h (po 8), 0Ah, 0Dh, 1Bh ESC řádek+20h sloupec+20h, 1Dh home,
  1Eh smazat do konce řádku, 1Fh smazat obrazovku. Atribut po startu 07h.
- **Klávesnice Consul 262.3** na portu B 8255 (režim 1, data invertovaná): 80h ROLL = numerický blok, 81h Shift+ROLL
  = čeština (tabulka CTAB1: é ě š č ř ž ý á í), mrtvé klávesy DCh a E0h, numerický blok s ROLL dává řídicí kódy
  WordStaru.
- **MFC-1A:** SPECIFY 03 2F 28 (DMA), čtení C6h (MT + MFM), zápis C5h, SEEK a SENSE INTERRUPT STATUS dotazováním,
  10 pokusů, zpoždění pro 2 MHz. **Registr 0Fh: `40h | mechanika<<2 | mechanika` při práci, 00h po nečinnosti**,
  D4 a D5 nikdy. Software SAPI-1.5 z roku 1990 tedy počítá s **D6 = 1 = mechaniky zapnuté** jako překreslení
  EC1045, ne jako schéma Tesla (`docs/desky/MFC-1A.md`). Sestava ZPS-6 má `wiring = ec1045`.
- RSW-1 (porty 18h–1Fh) ROM BIOS ani CBIOS nepoužívají.

## EPROM pro sestavu bez DBS-1 (AND-1Z, AND-1V)

Nový ROM BIOS pro ty, kdo mají PKT-1 a RSW-1, ale ne DBS-1 (2026-11-08): `roms/pkt-1/PKT-1_ROMBIOS_AND.bin`, skript
`tools/pkt1_and.py`, ovladač `tools/pkt1_and.mac`, sestavy `machines/sapi15_and1z.sapi` a `sapi15_and1v.sapi`, test
`test_sapi15`. Na skutečném HW zatím neběžel (Nejasnosti v `docs/desky/RSW-1.md`).

- **Z čeho vychází:** verze Melsoft (`pkt1-rombios.asm`), vyměněný je jen ovladač konzoly. Diskety Melsoft zůstávají
  beze změny, CBIOS volá jen vektory F000–F02Ah.
- **Video RAM** F800–FCFF, adresa = F800h + řádek · 64 + sloupec, bez atributů a bez stránkování MAP. Zápis do portu
  0Eh zůstal jen na startu (BOOT).
- **Řídicí kódy** jsou stejné jako u DBS-1. Test posílá stejnou sekvenci přes vektor F009h do ZPS-6 a do obou nových
  sestav a porovnává obrazovky.
- **Kurzor** je znak 127 ve video RAM jako v ECMONu, bliká z CONS0 (po 4096 voláních). **Rolování** kopíruje 19 řádků
  instrukcí LDIR, asi 15 ms na AND-1Z při 2 MHz. DBS-1 měnila jen začátek obrazu v 6845.
- **CSAT0 (F02Ah)** nedělá nic. SETDBS1 běží bez chyby, ale barvy se nezmění. Semigrafika DBS-1 v AND není.
- **RSW-1:** XT1 A6 = H, A7 = L, A8 = L nebo MAP2 (obojí dá podle dumpu DD1 RAM 0000–F7FF), nad ní je video RAM.
  Kód a proměnné ROM BIOSu jsou v F000–F7FF, CP/M 62K se proto nemění.
- **Karty:** AND-1Z bez MAP1 (S1), AND-1V s propojkou MAP1OFF (vždy aktivní, nezávisí na polaritě MAP1). Obě na
  F800h. Řadič MFC-1A zůstává.

## CP/M 2.2 62K (CBIOS V050690*LCH, Melsoft)

- **Diskety** `media/sapi15/SAPI15_{1,2,3,4,7,8}.sp2` (jen v kopii `..\SAPI_DOKU_FTP\SAPI15\MELSOFT_DISKETY`,
  v SAPI_DOKU z Dropboxu jsou jen rozbalené soubory `DISKn`). Formát **.sp2** = prostý výpis C/H/R:
  **40 stop × 2 strany × 16 sektorů × 256 B** (327 680 B), MFM 250 kbit/s, ID sektorů R = 1–16 na obou stranách bez
  prokládání, bez hlavičky (sektory za sebou: válec, strana, sektor). Přípona pochází z programu CpmImgMngr
  (`..\SAPI_DOKU\SAPI15\MELSOFT_DISKETY\CpmImgMngr.ini`, správce obrazů disket CP/M), kde sekce `[SP2]` popisuje
  právě tento formát (DPB a prokládání mechanik A:, B: z CBIOS) a `[SP1]` jednostrannou variantu pro mechaniku C:
  (40 × 1 × 16 × 256 = 163 840 B, CP/M stopa = strana, OFF 3). Význam zkratky nikde zapsaný není, nejspíš „SAPI,
  2 strany“. Emulátor oba formáty pozná podle velikosti (geometrie „5.25" DS DD SAPI-1.5 (16x256)“ a „SS DD“,
  i pro novou disketu; okno Média je nabízí v dialogu a nová disketa bez přípony dostane `.sp2` nebo `.sp1`).

| Disketa | Obsah |
|---|---|
| 1 (CPM62-5.001) | systém, CPM62-5.TXT, FORMAT, PIP, SDT40, SETDBS1, STAT, SYSGEN |
| 2 (CPM62-5.002) | ASM, ED, M80, L80, LIB, CREF80, F80 + FORLIB, WM, SORT, DDUMP, SUBMIT, XSUB |
| 3 (CPM62-5.003) | MBASIC, BASCOM + BASLIB, COBOL + OVR + COBLIB; **poškozený sektor BDOS** (C0 H0 R14) a sektory souborů, opravená kopie `SAPI15_3_opr.sp2` |
| 4 (CPM62-5.004) | Pascal MT+, DIS8080 |
| 7 | zdrojáky a aplikace: ROMBIOS.MAC, SYSTEM.MAC, CONNECT, INTER, SAPI (RSW-1), SIMPROG (programátor DSE-1, build pro 4 MHz, `docs/desky/DSE-1.md`), TAPEREC, DOCTOR, TABULKA |
| 8 | ROMBIOS.MAC/PRN/REL, TAPEREC, ASM, LOAD, DUMP, FORMAT |

Diskety 5 a 6 se nedochovaly. Všechny soubory mají atribut R/O.

**Poškozené sektory na disketách 2, 3 a 4** (nalezeno 2026-11-08). Dochované obrazy mají vadná místa: sektor sedí
s neporušenou kopií souboru do některého bajtu a odtud do konce je nesmyslný, nebo má jeden až několik chybných bajtů
(většinou překlopený bit). Vadná místa leží skoro vždy na stejných číslech sektoru na sousedních válcích (R3 a R4 na
disketách 2 a 4, R2 a R12 na disketě 4, R14 na disketě 3). Vypadá to na fyzickou vadu médií nebo chybu při čtení, ne na
úpravu programů.

| Disketa | Sektorů | Soubory |
|---|---|---|
| 2 | 41 | FORLIB.REL, GENHEX, L80, LIB, LOAD, M80, PIP, SAMPLE.DAT, SDT, SORT, STAT, SUBMIT, WM, WM.HLP |
| 3 | 6 a sektor BDOS | BASLIB.REL, COBOL3.OVR, MBASIC, PIP, STAT |
| 4 | 27 | MTPLUS.000, .001, .002, .003, .004, .006, MTPLUS.COM, PASLIB.ERL, PIP, RANDOMIO.ERL, STAT |

Všechny soubory těchto tří disket mají kopii jinde v archivech, takže se daly zkontrolovat celé. Na disketách 1, 7 a 8
se nic takového nenašlo, porovnat ale šly jen soubory s kopií jinde (ne např. SETDBS1 a většina zdrojáků na disketě 7).

**Opravené kopie** `media/sapi15/SAPI15_{2,3,4}_opr.sp2` vyrábí `tools/sapi15_oprava.py` (potřebuje archivy
`..\SAPI_DOKU_FTP` a `..\SAPI_DOKU`). Vadný sektor nahradí obsahem, na kterém se shodne většina kopií souboru, a výsledek
musí být shodný s některou skutečnou kopií. Originály zůstávají vedle, v katalogu MFC-1A se nabízejí opravené. Beze změny
zůstal SYSGEN (vlastní verze Melsoftu) a velikost obrazovky ve WM.

**Vyzkoušené programy** (ZPS-6, 2026-11-08, u každého základní použití, z originálů selžou M80, Pascal MT+ a WM):

| Disketa | Co běželo a dalo správný výsledek |
|---|---|
| 2 | ASM + LOAD, M80 + CREF80 + LIB + L80, F80 s FORLIB, ED, WM i s nápovědou, SDT, DDUMP (čtení), DUMP, GENHEX, GENMOD, SORT (SAMPLE.DAT, 645 záznamů), SUBMIT + XSUB, FORMAT a SYSGEN na B:, STAT, PIP s ověřením |
| 3 | MBASIC se soubory, BASCOM s BASLIB, COBOL s překryvy a COBLIB, přilinkovaný CRTDRV, STAT, PIP s ověřením |
| 4 | Pascal MT+ (překryvy .000–.006) a LINKMT s PASLIB, FPREALS, BCDREALS, TRANCEND (ukázka CALC.SRC), RANDOMIO, FULLHEAP, DEBUGGER; DIS8080, LIBMT, STAT, PIP s ověřením |

FORMAT a SYSGEN jsou na všech třech disketách stejné. Nevyzkoušené zůstaly opravy sektorů přes DDUMP, ROVLMGR (překryvy
Pascalu) a IOERR.SRC, který čeká soubor v cestě `80rtp/`. Návod k SORT V1.50 se nedochoval, zadání funguje takto:
`SORT SAMPLE.DAT`, `INPUT 100 CR-DELIMITED`, `OUTPUT X.DAT`, `KEY 1 40`, `GO`. V souboru `.BLD` pro LIBMT je na prvním
řádku výstupní knihovna.

**Proč WM (WordMaster V1.07A-A1) z originálu diskety 2 hlásí `BDOS ERR ON P: SELECT`:** poškozené jsou adresy 0245–02FF,
122E–12FF, 21E9–21FF a 223A–22FF, od neporušené kopie se liší jinak jen velikost obrazovky na 01B9h a 01BAh (14h, 40h =
20 × 64). Vstup 0269h leží v poškozeném místě, z 0297h se zavolá 9579h, běh pak projde prázdnou RAM až do CCP s registrem
C = FFh. CCP to bere jako uživatele 15 a mechaniku P:. Emulátor za to nemůže.

- **Systémová stopa** (válec 0) = `system1.bin`: boot sektor (EF00h, „JPR-1Z 62K CP/M VERS. 2.2“, čte 26 sektorů do
  D400h a skočí na EA00h), CCP D400h, BDOS DC00h (DRI 2.2 s mírnými úpravami „(C)-JBPG“/„(C)-PGJB“), CBIOS EA00h.
- **Mechaniky CBIOS:** A:, B: 5,25" DS/DD 312 KB (SPT 64, blok 2 KB, 128 položek adresáře, OFF 1, CP/M stopa =
  válec s oběma stranami, prokládání po 3 střídavě stranami), C: 5,25" SS/DD 148 KB (OFF 3), D: 8" SS/SD (IBM).
  BIOS+33h = SETATR (barevný atribut).
- **SETDBS1.COM:** nastavení barev popředí a pozadí (ukáže i celou znakovou sadu). SDT40: debugger Tesla Eltos.
  FORMAT V1.1 formátuje 5,25" DS/DD, 5,25" SS/DD a 8" SS/SD.
- **Melsoft disketa 7, RSW-1:** `CONNECT.MAC`, `INTER.MAC` programují 8253 (36h, dělitel 8) a 8251 (CEh, 37h) na 9600 Bd.

## Co chybí

- **ZPS-5:** žádný firmware se nenašel. Podle EC1045 (2026-10-07) je ZPS-5 z toho, co se dochovalo, dvojitá vana
  s dlouhou sběrnicí bez MFC-1A, nejspíš pro průmyslové použití s obsluhou přes terminál nebo zobrazovadlo na míru.
  Byla to tedy řídicí sestava s EPROM od zákazníka. Známý ROM BIOS bez MFC-1A a DBS-1 nenaběhne. Sestava ZPS-5
  počká na nalezený software.
- ~~Znakový generátor DBS-1~~ (dump EPROM „59“ od EC1045 2026-10-07, `docs/desky/DBS-1A.md`), Melsoft diskety 5
  a 6, úplný návod k CP/M SAPI-1.5
  (zachoval se jen `CPM62-5.TXT`).
- Další zdroje: `CPM\BIOS\BIOS_ZPS7\BIOS7.asm` (2010–13, rozhraní ROM BIOSu pro SAPI-1 s konzolí na sériové lince
  DSM-1), ovladač DBS-1A J. Bíby z roku 2012 v SD dumpu EC1045 Petra Všijanského
  (`SAPI1_MAP\4B_DBSC_311212\311212C.2MB`).
