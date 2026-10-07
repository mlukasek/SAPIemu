# SAPI-1.5: ROM BIOS PKT-1 a CP/M 2.2 62K (Melsoft)

Software pro sestavy SAPI-1.5 (ZPS-5 až ZPS-10, Tesla Liberec 1990). Emulované od 2026-10-07 v sestavě
`machines/zps/ZPS-6.sapi` (PKT-1, RSW-1, MFC-1A, DBS-1A), test `test_sapi15`. Desky: `docs/desky/PKT-1.md`,
`RSW-1.md`, `DBS-1A.md`, `MFC-1A.md`.

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
| 3 (CPM62-5.003) | MBASIC, BASCOM + BASLIB, COBOL + OVR + COBLIB; **jeden sektor BDOS poškozený** (C0 H0 R14), proto `SAPI15_3_opr.sp2` se systémovou stopou z diskety 1 |
| 4 (CPM62-5.004) | Pascal MT+, DIS8080 |
| 7 | zdrojáky a aplikace: ROMBIOS.MAC, SYSTEM.MAC, CONNECT, INTER, SAPI (RSW-1), SIMPROG, TAPEREC, DOCTOR, TABULKA |
| 8 | ROMBIOS.MAC/PRN/REL, TAPEREC, ASM, LOAD, DUMP, FORMAT |

Diskety 5 a 6 se nedochovaly. Všechny soubory mají atribut R/O.

- **Systémová stopa** (válec 0) = `system1.bin`: boot sektor (EF00h, „JPR-1Z 62K CP/M VERS. 2.2“, čte 26 sektorů do
  D400h a skočí na EA00h), CCP D400h, BDOS DC00h (DRI 2.2 s mírnými úpravami „(C)-JBPG“/„(C)-PGJB“), CBIOS EA00h.
- **Mechaniky CBIOS:** A:, B: 5,25" DS/DD 312 KB (SPT 64, blok 2 KB, 128 položek adresáře, OFF 1, CP/M stopa =
  válec s oběma stranami, prokládání po 3 střídavě stranami), C: 5,25" SS/DD 148 KB (OFF 3), D: 8" SS/SD (IBM).
  BIOS+33h = SETATR (barevný atribut).
- **SETDBS1.COM:** nastavení barev popředí a pozadí (ukáže i celou znakovou sadu). SDT40: debugger Tesla Eltos.
  FORMAT V1.1 formátuje 5,25" DS/DD, 5,25" SS/DD a 8" SS/SD.
- **Melsoft disketa 7, RSW-1:** `CONNECT.MAC`, `INTER.MAC` programují 8253 (36h, dělitel 8) a 8251 (CEh, 37h) na 9600 Bd.

## Co chybí

- **ZPS-5:** žádný firmware se nenašel. ZPS-5 (PKT-1 + RSW-1 bez displeje a disket) byla zřejmě řídicí sestava
  s EPROM od zákazníka. Známý ROM BIOS bez MFC-1A a DBS-1 nenaběhne. Sestava ZPS-5 počká na nalezený software.
- **Znakový generátor DBS-1** (`docs/desky/DBS-1A.md`), Melsoft diskety 5 a 6, úplný návod k CP/M SAPI-1.5
  (zachoval se jen `CPM62-5.TXT`).
- Další zdroje: `CPM\BIOS\BIOS_ZPS7\BIOS7.asm` (2010–13, rozhraní ROM BIOSu pro SAPI-1 s konzolí na sériové lince
  DSM-1), ovladač DBS-1A J. Bíby z roku 2012 na H: (`H:\SAPI-1\Petr_repliky_k_vyrobe\PMD32-SD\SD_EC1045_2019-01-31\
  SAPI1_MAP\4B_DBSC_311212\311212C.2MB`).
