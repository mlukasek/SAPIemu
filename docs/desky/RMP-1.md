# RMP-1 – řadič magnetické pásky (IZOT SM 5300)

Stav: **emulovaná** (2026-10-10), karta `RMP-1` (`src/cards/rmp1.*`) s 1 až 3 mechanikami IZOT SM 5300, sestava
`machines/rmp1_jb_izot.sapi` (CP/M J. Bíby, programy pro pásku na B:, ukázková páska), test `tests/test_rmp1.cpp`.
Emulace je ověřená originálním softwarem: ovladač `RMP.MAC` Tesly Elstroj 1985 a programy J. Bíby (MTDAT, DIRMT,
MTZAP, MTCTE, MTCOPY) na ní běží beze změn. **Návod k obsluze** (ukázková páska, výpis, kopírování mezi páskou
a disketou, nová páska): `docs/navod-rmp1.md`.

Zdroje:
- [R1] uživatelská dokumentace 6XV 122 14: sken `..\SAPI_DOKU\SAPIMAN\Scany dokumentací\RMP1.pdf`, OCR
  `..\SAPI.CZ\sapi\rmp-1.php` (popis, porty, konektory X1–X3, kabel k jednotce IZOT, rozpiska), schéma
  `..\SAPI.CZ\sapi\pdf\rmp-1.pdf` a `rmp-1_cb.pdf`, replika v Eagle `..\SAPI_DOKU\sapi_eagle\REPLIKY\RMP-1\`;
- [R2] ovladač `RMP.MAC` (Jan Mercl, Tesla Elstroj 1985), knihovna `RMP.REL` pro M80/L80:
  `..\SAPI_DOKU\SAPIMAN\CPM_PROGRAMY\Prenos_CPM_JSEP_MT\rmp.txt` (také `...\zdrojemercl\rmp.txt`);
- [R3] programy J. Bíby pro přenos dat JSEP ↔ CP/M: disketa
  `..\SAPI_DOKU\SAPIMAN\PMD32-SD_IMAGE8\ZDROJ_STARE\a_lad_izo.8sd` (v repu `media/rmp-1/a_lad_izo.img`, stejná je
  i v kopii z FTP), zdroje jako text v `...\CPM_PROGRAMY\Prenos_CPM_JSEP_MT\` (`mtkan.txt`, `mtdat.txt`,
  `mtdatman.txt`, `dirmt.txt`, `mtcopy.txt`…);
- [W1] robotrontechnik.de, magnetopáskové jednotky: https://www.robotrontechnik.de/html/zubehoer/mbe.htm
  (fotografie CM 5300: https://www.robotrontechnik.de/bilder/Grossrechner/K1600/CM5300_k.jpg,
  https://www.robotrontechnik.de/bilder/Grossrechner/K1600/CM5300_offen_k.jpg);
- [W2] A.Museum, ИЗОТ 5003: https://www.amuseum.bg/katalog/mspdml-izot-es9003;
- [W3] 9stopá páska obecně (formát, značky BOT/EOT, NRZI): https://en.wikipedia.org/wiki/9-track_tape;
- [W4] Pertec, mechaniky stejného druhu (synchronní, čtení po zápisu):
  https://bitsavers.org/pdf/pertec/tape/100605_PEC_3000_4000_Series_Synchronous_Write_Tape_Transport_Operating_and_Service_Manual_1969.pdf.

## Mechanika IZOT SM 5300

Technický popis mechaniky se nenašel. Z [W1], [W2] a [R1]:

- bulharská (IZOT) magnetopásková jednotka z programu SMEP, verze **CM 5300.01** je zásuvka asi 30 kg pro 19"
  skříň, používala se i u K1600, A5220/A5130 a R4201 (NDR);
- páska 1/2", cívky do 216 mm (8,5"), asi 350 m pásky, 9 stop, 800 bpi (32 b/mm) NRZI, **12,5 palce/s**
  (31,75 cm/s), 10 KB/s, asi 10 MB na pásku, převíjení asi 5 minut;
- tři motory a dvě tahová ramena (bez vakuových komor), cívka navíjecí je pevná: pásku jde sundat až po
  převinutí na začátek;
- [R1]: řadič je „doporučený pro IZOT SM 5300“, výrobce ho testoval s SM 5300.00 programem TSX 10 (nenašel se).
  Jednotka musí mít propojkami zapojený signál RDS (logický součet stop). Zakončovací odpory smí být jen
  v jedné jednotce.
- Program MTDAT píše „Dej paskovou mechaniku do "DISTANC"“: tlačítko dálkového ovládání (ON-LINE).

## Porty (C0H–CBH, [R1] tab. 1)

| port | obvod | směr | význam |
|---|---|---|---|
| C0H | D1B 8255 PA | W | data k zápisu (WD0 = D7 … WD7 = D0, bajt tak, jak je) |
| C1H | D1B PB | R | přečtená data (RD0 = D7 …), strobováno RDS při RE |
| C2H | D1B PC | R | OBF (PC7), WDSE (PC5), WDP (PC4), IBF (PC1) |
| C3H | D1B CWR | W | A6H = mód 1, PA ven, PB dovnitř; 08/09 = WDP 0/1, 0A/0B = WARS/WDS |
| C4H | D2B PA | W | 7 WE, 6 RE, 5 TE, 4 OFC, 3 RWC, 2 SWS, 1 SRC (vzad), 0 SFC (vpřed) |
| C5H | D2B PB | R | 6 PON, 5 RDY, 4 WPT, 3 BOT, 2 ONL, 1 RWS, 0 EOT vybrané jednotky |
| C6H | D2B PC | R/W | čtení PC7 OUT2, PC6 OUT0, PC5 LPP, PC4 PAE; zápis PC3 RPON, PC2–0 výběr S2–S0 |
| C7H | D2B CWR | W | 8AH = mód 0 |
| C8H–CAH | D2A 8253 | R/W | čítače 0–2, všechny hodiny Φ2 (2 MHz) |
| CBH | D2A CWR | W | řídicí slovo čítačů |

Čítače: Č0 hradlo TE, výstup OUT0 na PC6 a na zápisový monostabil D7C (vzestupná hrana s WE a plným PA vyšle
WDS nebo WARS podle WDSE a pak ACK). Č1 hradlo = „hodiny dat“ z D4C (impulz při každém RDS), Č2 hradlo = OUT1.
Ovladač dává Č1 mód 5 na 200 µs a Č2 mód 1 na 400 µs: OUT2 v nule znamená, že data chodí, v jedničce mezeru.
PON (D7A) se nastaví resetem a dokud ho program nezruší bitem RPON, nejdou povely k pohybu.

## Záznam (ČSN 36 9464, [R1], [R2])

- Řádka = 8 bitů dat + paritní stopa, **lichá parita** (řádka samých nul má paritu 1, takže každá řádka má
  aspoň jednu jedničku a RDS je logický součet stop). NRZI: jednička = změna magnetizace.
- Blok 18 až 2048 znaků. Za poslední řádkou dat je po 3 prázdných řádkách **CRC** (algoritmus normy, `comp.crc`
  v [R2]), po dalších 3 **LRC**: WARS vynuluje zápisové klopné obvody, tím vznikne řádka, která doplní každou
  stopu na sudý počet jedniček.
- **Značka pásky (TM, FM)** = 13H, 7 prázdných řádek, 13H.
- Mezery píše ovladač časem: 33 ms před blokem, 368 ms před TM, od BOT 570 ms (blok) a 855 ms (TM), po bloku
  dojede 5 ms. Při čtení čeká od BOT 260 ms bez čtení a jinak 20 ms, kdy nesmí přijít data („data v mezeře“).

## Emulace

- **Model mechaniky:** poloha pásky v jednotkách 1/160000" (jeden takt Φ2 při 12,5 palce/s), na pásce jsou
  řádky s polohou. Čtecí hlava je 0,15" za zápisovou (čtení po zápisu jako u mechanik Pertec [W4]; `RMP.MAC`
  čeká na přečtení zapsaného do 32 ms). Zápis maže od zápisové hlavy dál.
- **Rozběh a doběh:** plná rychlost za 32,8 ms, dráha rozběhu i doběhu 0,2" (ECMA-62 chce pro 800 bpi 0,19").
  Je to nutné: po `mt.bsr` (přeskoč blok vzad) ovladač zastaví jen 9 ms za blokem a při čtení vpřed čeká 20 ms
  bez dat. Bez doběhu DIRMT za prvním souborem hlásí „Chyba cteni“.
- **BOT:** odrazná značka 28 mm (1,1"), signál drží, dokud je pod snímačem. `RMP.MAC` se na BOT ptá až po
  rozjetí pásky, kratší značka by vedla k zápisu bez dlouhé mezery od BOT.
- **EOT** 13 450" (asi 342 m) od BOT, konec pásky 13 600". Převíjení 50 palců/s (350 m asi 4,6 min).
- **Obraz pásky:** formát SIMH `.tap` (záznamy s délkou, 0 = TM, chybný záznam s příznakem 0x80000000). Při
  načtení se řádky rozloží s mezerami, jak je píše `RMP.MAC`, a CRC a LRC se dopočítají. Při uložení se ze
  řádek složí záznamy znovu (parita, CRC a LRC se kontrolují). Páska z `media/` se používá přes kopii ve `work/`.
- **Ovládací panel:** okno Média kreslí mechaniku s cívkami, kontrolky PON (stav řadiče, svítí jen u vybrané
  jednotky), ONL, RDY, BOT, EOT, RWS, WPT a tlačítka LOAD, ON-LINE/OFF-LINE, REWIND, UNLOAD. Skutečný panel SM 5300
  neznáme, rozmístění je kreslené. Vpravo je mapa pásky (návěští JSEP, bloky, TM, žlutá šipka = hlava, mapa
  není v měřítku) a výpis bloku u hlavy (hex a EBCDIC).
- **Pohodlí (přiznaná odchylka):** `ready_at_start` (výchozí zapnuto) založí pásku a stiskne LOAD a ON-LINE už
  při zapnutí. Na železe to dělá obsluha. Vypnuté: po zapnutí je páska jen nasazená, LOAD a ON-LINE se mačkají
  v okně Média.
- **Kroužek zápisu** (`write_ring`): bez něj jednotka hlásí WPT a nezapíše. V okně Média jde změnit jen
  u nezavedené pásky.
- **Nemodelováno:** šikmost pásky (skew) a rozdílné časy stop, výpadky, stárnutí pásky, PAE jen z parity řádky.

## Software

- **`RMP.MAC`** [R2] (Jan Mercl, Tesla Elstroj 1985): knihovna funkcí `mt.clr` (inicializace), `mt.sel`
  (výběr jednotky), `mt.sts` (stav), `mt.rrf` (čti blok), `mt.fsr`/`mt.bsr` (blok vpřed/vzad), `mt.fsf`/`mt.bsf`
  (za TM vpřed/vzad), `mt.wcc` (zapiš blok), `mt.wfm` (zapiš TM), `mt.gap` (mezera), `mt.rew` (převiň),
  `mt.rwo` (převiň a OFF-LINE). Všechno časuje programem a čítačem Č0 (smyčky po 1 ms).
- **MTKAN** [R3] (J. Bíba): kanálový program nad `RMP.MAC` pro soubory JSEP (návěští VOL1, HDR1/HDR2,
  EOF1/EOF2, kód EBCDIC, pevné záznamy v blocích), návod `MTKAN.MAN` na disketě.
- **MTDAT** V070588JB: menu 1 = páska → disketa, 2 = disketa → páska, 3 = inicializace pásky (VOL1), 4 = konec,
  5 = výpis obsahu. Textové soubory převádí ASCII ↔ EBCDIC, tabulka je 7bitová: horní bit českých písmen v KOI8
  se ztratí (zkoušeno na `MTDAT.MAN`). Návod `MTDAT.MAN`.
- **DIRMT** V141287: výpis pásky (`B:DIRMT` a číslo jednotky 0–2 nebo `B:DIRMT 0`).
- **MTCOPY** V041287JB: páska → páska, páska ↔ disketa, inicializace.
- **MTZAP / MTCTE:** zkušební zápis 255 bloků s větou „Toto je prvni zapsany a precteny blok dat pocitacem
  SAPI-1/CPM na magneticke pasce IZOT CM 5300.01“ a TM, MTCTE je přečte. Bíbova první zkouška pásky.
- **Ukázková páska** `media/rmp-1/sapi01_ukazka.tap`: páska SAPI01 se dvěma soubory, MTZAP (MTZAP.MAC) a MTDATMAN
  (MTDAT.MAN), blok 800, záznam 80. Vyrobila ji MTDAT v emulátoru (`RMP_MAKE=soubor test_rmp1`).

## Nejasnosti

- **Parametry SM 5300:** vzdálenost čtecí a zápisové hlavy (emulace 0,15"), čas a dráha rozběhu a doběhu
  (emulace 32,8 ms a 0,2"), délka značky BOT (emulace 28 mm), poloha EOT a rychlost převíjení (emulace
  50 palců/s). Ověřit měřením nebo v manuálu SM 5300, pokud se najde.
- **Panel SM 5300:** jaká tlačítka a kontrolky skutečně má (LOAD, ON-LINE, REWIND, UNLOAD?) a jak se jmenují
  (MTDAT píše „DISTANC“). Fotografie [W1] panel nečitelně ukazují.
- **OFC a RWC:** jsou to impulzy (podle [R1] ano). Emulace bere náběžnou hranu bitu v PA. Vrací se jednotka
  po OFC sama do ON-LINE jen tlačítkem?
- **RDY při převíjení:** emulace RDY během převíjení shazuje (`wait.rewind` v [R2] čeká na RWS, ne na RDY).
- **Test TSX 10** výrobce se nenašel.
