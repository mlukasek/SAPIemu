# FDC-1 – řadič pružných disků (FDC-1A + FDC-1B)

Stav: **emulovaná** (typ `FDC-1`, `src/cards/fdc1.*`, obvody `src/chips/i8272.*`, `i8257.*`, mechanika
`src/chips/floppy.*`), ověřeno se CP/M 2.2/P 48K a 62K pro FDC-1 (Peřina) v ZPS-3.

## Podklady

- `..\SAPI-karty\FDC-1\` (autor):
  - `fdc-1a.pdf`, `fdc-1b.pdf`, `FDC-1A.png`, `FDC-1B.png`, Eagle `.sch`/`.brd`: schémata překreslená EC1045.01
    (rev. 1.0, 13. 2. 2011);
  - `navod-scan\1.jpg`–`13.jpg`: návod (strany 2–27): instalace, programování, popis 8272, konfigurace mechanik,
    rozdíly proti RPD-1, zapojení Consul 7113, rozpisky, rozložení součástek;
  - `PROM_D5B.hex`: dump PROM D5B (dekodér portů);
  - `FDC-1AB.xlsx`, `.csv` (rozpisky), `Kabel_pro_FDD.xls`, fotky desek `Foto\`;
  - `sw\`: zdroje BIOSů EC1045 (`ECBIF*.MAC`, ovladač „FDC-1 V050290JB“) a `info-z-emailu.txt`.
- Systémové diskety CP/M 2.2/P pro FDC-1 (`..\SAPI-sestavy\ZPS-3\CPM48AF.img`, `CPM62AF.img`, v repu
  `media/fdc-1/`). Na nich je `FDC.DOC` (popis změn proti RPD-1), `FDC.INC`, zdroj monitoru `MOR24F.MAC` a
  formátovač `FORM6.COM` (AGRODAT Znojmo 1988).

### Návod (sken `navod-scan`, strany 2–27)

- 3 Instalace, 4 Popis funkce, 5 Programování (registry 0EH/0FH, dekodér, adresy), stručný popis 8272 (vývody,
  MSR, ST0–ST3, povely SPECIFY, RECALIBRATE, SEEK, READ ID, READ DATA, WRITE DATA, FORMAT, SENSE INTERRUPT STATUS,
  READ/WRITE DELETED DATA, READ TRACK, SENSE DRIVE STATUS, INVALID, SCAN), 6 Testování (ve výrobě speciální
  testovací program), 7 Údržba, 8 Všeobecné údaje, 9 Konfigurace mechanik (SA2), 10 Rozdíly v zapojení proti RPD-1,
  zapojení Consul 7113, 11/12 rozpisky součástek, 13/14 rozložení součástek.
- Návod k 8272 upozorňuje, že povely SCAN „se v běžných programech (BIOS CP/M) neuplatňují“ a u FDC rumunské
  výroby nelze předpokládat lepší výsledky.
- Rozpiska FDC-1A: 74LS04, 74LS08, 74LS14, MH7438, 74LS51, 74LS74, UCY74145, UCY74153, 74125, 74LS175, MH74S287,
  MHB8282, 2× MHB8286, 8257 (KR580IK57), NEC 765A (CM609), NE555, KC507. FDC-1B: 74LS02, 74LS04, 74LS08, 74LS14,
  74LS32, MH7438, UCY7473, 74LS74, MH7493A, 74S124 (K531GG1), 74125, 74145, 3× UCY74153, 3× 74LS175, MHB4503,
  krystal 8 MHz, přepínače DIL TS 501 8181 (8 pólů, SA2) a TS 501 2121 (2 póly, SA1).
- Tabulka GPL (8"): FM 128 B N = 00, GPL 07, při formátování 1B; 256 B 0E/2A; 512 B 1B/3A; 1024 B 47/8A.
  MFM 256 B 0E/36, 512 B 1B/54, 1024 B 47/74. 5,25": FM 128 B 07/09 …

## Zapojení

- **Dvě desky** propojené plochým kabelem stejně jako RPD-1 (konektor X2): FDC-1A (8272, DMA, dekodér portů,
  rozhraní mechanik X3) a FDC-1B (separátor dat, krystal 8 MHz, registry 0EH/0FH, přepínače SA1 a SA2, vlastní
  výstupy SEL, HL a MOT pro mechaniky na X3).
- **FDC-1A:** 8272 (NEC 765A, uPD765), DMA 8257 (KR580IK57, na schématu „8257 (8237-B)“), oddělovače 8282/8286,
  PROM D5B (74S287) na A0–A7, monostabil 555.
- Řadič umí až 4 mechaniky, 8" i 5,25", SS i DS, SD i DD (návod). **Emulace má 4 mechaniky, formát podle
  velikosti obrazu** (`src/chips/floppy.h`): 8" SS SD (IBM 3740, 77 × 26 × 128 B, jako u RPD-1) a formáty PC
  3,5" HD 1,44 MB, 5,25" HD 1,2 MB, DD 720 a 360 KB (MFM, 512 B, dvě strany). **Obrazy ImageDisk (`.imd`)**
  s libovolnou geometrií po stopách (FM/MFM, délka a číslování sektorů, deleted, chyby CRC; `docs/rozhodnuti.md`). PC mechaniky HD používal EC1045
  (BIOSy `ECBIF3`, `ECBIF5`, sestava `machines/cpm_ec1045.sapi`).
- Od roku 1989 dodávaný místo RPD-1 (FDC.DOC). Tiskárnu DPP-1 je kvůli kolizi portů nutné přeadresovat z 08H na 18H.
- **Propojení desek:** horní konektory (X2) obou desek spojuje stejný plochý kabel jako u RPD-1 (KB-17, návod:
  „Obě desky řadiče jsou propojeny stejnou propojkou jako u řadiče RPD-1“). Signály X2: /CK, /WC, /WD, /RD, /RES,
  /VCO, 4M, RD, /S1, /IE, /MFM a země.
- **Konektor mechanik X3 na FDC-1A** (FRB 30, klíč A-5) má stejné rozložení jako K2 FDD na RPD-1B s rozdíly (návod
  kap. 10):

| Kolík | RPD-1B | FDC-1A | | Kolík | RPD-1B | FDC-1A |
|---|---|---|---|---|---|---|
| 2 | /HL | /HL | | 18 | /IX | /IX |
| 4 | /ST | /ST | | 20 | /WF | /RDY (z výroby na zem, Consul 7113; pro 5,25" přerušit) |
| 6 | /SEL0 | /SEL0 | | 22 | /TR43 | /TR43 |
| 8 | /RD | /RD | | 23 | zem | /SS1 (výběr hlavy) |
| 10 | /WG | /WG | | 24 | /SEL1 | /SEL1 |
| 12 | /WD | /WD | | 25 | zem | /SEL2 |
| 14 | /TR00 | /TR00 | | 26 | /FR | /TWO SIDE |
| 16 | /WP | /WP | | 27 | zem | /SEL3 |
| 1, 3, 5 … 21 | zem | zem | | 28 | /DIR | /DIR |
| | | | | 29, 30 | zem, rezerva | /WF, /FR |

- **Konektor X3 na FDC-1B** (klíč B-5): samostatné signály /M0–/M3 (kolíky 2, 4, 6, 8), /H0–/H3 (10, 12, 14, 16)
  a /S0–/S3 (18, 20, 22, 24) z registrů 0FH a 0EH. Tabulka v návodu (str. 21) má nadpis „FDC-1A, X3“, text nad
  ní ale tyto signály přiřazuje konektoru X3 na FDC-1B.
- **Kabel k mechanikám Consul** (FDC.DOC, příloha): signály T43, FR, HL, WR, WD, STEP, DIR, S0, S1, RDD, T00,
  INDX, WF a jejich zemní vodiče, pro C7113 (přímý konektor), C7123 (RFT 34, dvě mechaniky), RPD-1B a FDC-1A
  (FRB 30). Rozdíly mezi RPD-1B a FDC-1A: FR na kolíku 26 / 30, WF na 20 / 16 (u FDC-1A je 20 RDY a „pro FDC-1A
  uzemnit!“). Zapojení Consul 7113 v návodu: STEP 4, SEL0 6, SEL1 24, RD 8, WG 10, WD 12, TR00 14, WP 16,
  INDX 18, DIR 28, TWO SIDE 26, RDY 20 (na FRB 30), napájení +5 V, +24 V A 0,5 A, +24 V B 1,5 A.

## Porty (PROM D5B, výchozí)

| Port | Obvod | Význam |
|---|---|---|
| 04–0BH | 8257 | 04 adresa kanálu 2, 05 počet kanálu 2, 06/07 kanál 3, 08 režim (W) / stav (R) |
| 0CH | 8272 | hlavní stav MSR (R) |
| 0DH | 8272 | data: povely, výsledky, data v režimu bez DMA |
| 0EH | FDC-1B | W: D1 D0 = výběr S0–S3, D2 = 1 reset 8272, D3 povolení INT0; R: přepínače SA2 vybrané mechaniky |
| 0FH | FDC-1B | W: D3–D0 motory M3–M0, D7–D4 přiklopení hlav H3–H0 (signály na X3 FDC-1B) |

- Kanály 0 a 1 osmice 8257 (00–03H) jsou zakryté porty JPR (PROM pro ně nic nevybírá). Návod píše „8257 na 04 až 08“.
- **PROM D5B:** výstupy D0 = /CS 8257 (Y0), D1 = /CS 8272 (Y1), D2 = /MR a /MW obou (Y2), D3 = FDC-1B (Y3).
  Dump: 04–0BH FAh, 0CH–0DH F9h, 0EH–0FH F7h, jinde FFh. Vstupy jsou přímo A0–A7.
- **8272** je připojený na DMA kanál 2 (DREQ2, DACK2, TC z 8257). INT přes hradlo s povolením D3 (0EH) na /INT0.
- **Čtení 0EH (SA2):** FORM6 zapíše na 0EH číslo mechaniky a v D1 čeká 1 pro 8" formáty. D1 = 8" (S1–S4
  „funkční a identifikační“), D0 = DD (S5–S8 jen identifikační, **polarita je odhad**). Návod: „IN 0EH lze číst
  stav DIL přepínače na bitech D0 a D1“.
- **READY:** na dodávaných deskách je /RDY (X3-20) spojený se zemí pro Consul 7113 (návod, FDC.DOC). 8272 pak
  považuje všechny 4 jednotky za připravené.

## Software

- **Diskety pro FDC-1** (`media/fdc-1`, obsah stejný pro 48K i 62K): BIOS2.MAC, BOOT.MAC, CPM.DOC, CPMH.DOC,
  CPM48A/48Z/62A/62Z.COM, FDC.DOC (popis změn proti RPD-1 a tabulka kabelu), FDC.INC, FORM6.COM, MODIF1.COM,
  MOR24F.MAC (zdroj MORxxF), MOR48AF/48ZF/62AF/62ZF.HEX (EPROM pro JPR-1A a JPR-1Z), RSYS48A/62A.COM, RTEST.COM,
  SYSGEN.COM, WM.COM, WM.HLP, W16/40/64/80.COM, X40/64/80.COM. Moduly pro FDC-1 mají za jménem „F“, jinak je systém
  stejný jako pro RPD-1 (FDC.DOC).
- **Podklady EC1045 v `sw\`:** BIOSy pro JPR-1Z/ZI se ZRMD-1 a druhým řadičem FDC-1 (`ECBIF3`, `ECBIF5`, `ECBIF8`,
  `ECBIF8P`, `ECBIHD`, `ECBIM8`, `ECBIR8`, `ECMON`; varianty `_cd` s příkazy CD a MOUNT pro PMD32-SD). V systému
  EC1045 je systémový řadič RPD-1 s 8" a FDC-1A/B připojuje 5,25" mechaniky (`info-z-emailu.txt`). `mount.cfg` je
  konfigurace programu MOUNT (https://pmd85.borik.net/wiki/MOUNT) pro AND-1Z/V a TeraTerm.

- **MOR48AF / MOR62AF** (Peřina 1988, „SAPI-1 ZPS3-48K/FDC-1“, zdroj `MOR24F.MAC`):
  - init: `OUT 0EH,04` a `OUT 0EH,00` (reset 8272), `OUT 08H,00`, SPECIFY 03 0F FE (SRT 16 ms, HUT 240 ms,
    HLT 254 ms, DMA);
  - čte a zapisuje po jednom sektoru: 8257 kanál 2 (adresa, počet 7Fh + 40h čtení / 80h zápis, režim 44h =
    kanál 2 a stop po TC), READ/WRITE DATA s EOT 26, GPL 7, DTL FFh;
  - po SEEK/RECALIBRATE volá SENSE INTERRUPT STATUS, dokud nedostane ST0 se SE pro svou mechaniku (80h = ještě
    ne, jiná mechanika = znovu, chybové bity = chyba).
- **ECBIF8.MAC** (EC1045, ovladač „FDC-1 V050290JB“): stejné porty, SPECIFY 03 7F 30.
- **FORM6.COM** („FORMAT ZNK, AGRODAT Znojmo V1.0 090388ZF“):
  - nabídka typů A–I: A = SS SD 8" 0–76, 26 × 128 B; B = SS DD 5,25" 0–39, 16 × 256; C = SS DD 8", 8 × 1024;
    D, E, F = SS DD 5,25" 9 × 512, 8 × 512, 5 × 1024; G = DS DD 5,25" 9 × 512; H = DS DD 8" 8 × 1024;
    I = DS DD 5,25" 0–79, 9 × 512;
  - pak „FORMATED DRIVE:“ (A–D) a „ARE YOU SURE ??? (Y/N)“ (řádkový vstup BDOS, tedy Y a Enter);
  - `OUT 0EH,mechanika`, `IN 0EH`, D1 porovná s typem (1 = 8"), jinak „IT IS NOT ALLOWED FORMAT FOR THIS DRIVE“;
  - `OUT 0FH,0FH` (motory) a čekání, SPECIFY 03 7F 80, RECALIBRATE, pro každou stopu SEEK a FORMAT A TRACK
    (8257: počet 8067h = 104 bajtů, 26 × C H R N), výsledek do „ERROR ON TRACK“ / „O.K.“.
  - V emulátoru naformátuje 8" SS SD disketu na B: (asi minuta) a CP/M na ni pak zapisuje.

## Emulace

- **8272:** povely READ DATA, READ DELETED DATA, WRITE DATA, WRITE DELETED DATA, READ TRACK, READ ID, FORMAT,
  RECALIBRATE, SEEK, SENSE INTERRUPT STATUS, SENSE DRIVE STATUS, SPECIFY. SCAN se neemuluje (odpověď jako na
  neplatný povel, varování v logu). Režim bez DMA (ND = 1) je emulovaný, software ho nepoužívá.
- **Hodiny 8272 a rychlost:** přepínač SA2 S1–S4 mechaniky (8") dává 8 MHz: FM 250, MFM 500 kbit/s (8" a mechaniky
  PC HD), 5,25" dává 4 MHz: FM 125, MFM 250 kbit/s (DD). **Přiřazení přepínače k hodinám je odhad.** 8272 vidí
  značky ID jen tehdy, když disketa má jeho záznam (FM/MFM), stranu a rychlost.
- **Časování:** disketa se točí svými otáčkami a rychlostí: 8" FM bajt 32 µs, 360 ot/min, stopa 5208 bajtů se
  stejnými mezerami jako u RPD-1; MFM HD bajt 16 µs, stopa 12 500 (3,5", 300 ot/min) nebo 10 416 (5,25", 360);
  rozložení MFM podle IBM System 34; krok
  16 − SRT ms, přiklopení hlavy HLT × 2 ms, odklopení HUT × 16 ms po povelu; RQM se vrací 12 µs po bajtu povelu
  nebo výsledku (odhad). Sektor se hledá podle polohy disku, chyba (ND, MA) po druhém indexu.
- **Přerušení:** jeden stav na jednotku (jako MAME): konec seeku přepíše starší stav téže jednotky, SENSE
  INTERRUPT STATUS bere jednotky 0–3 popořadě. Po resetu 8272 hlásí pro každou připravenou jednotku „ready
  changed“ (ST0 C0h + jednotka); nový SEEK/RECALIBRATE stav své jednotky smaže. Bez toho MOR po RESETu s hlavou
  na stopě 0 hlásil BOOT ERROR.
- **Bez diskety** a s uzemněným /RDY 8272 čeká na index věčně (jako skutečný čip).
- **DMA:** bajt se přenese v okamžiku, kdy ho 8272 přečte. Každý přenos ubere CPU 4 takty (HOLD, `Bus::hold`).
  Nevyřízený DRQ (kanál vypnutý) je přetečení (OR).
- **Parametry:** `drive_a`–`drive_d` (obraz, ochrana), `working_copy`, `decoder` (PROM D5B), `rdy_ground`,
  `sa2_a`–`sa2_d` (přepínače SA2: 8sd, 8dd, 5sd, 5dd).
- Okno Média ukazuje mechaniky A–D (vložit, nová, vysunout, ochrana), stejně jako u RPD-1.
- **Motory** (parametr `motor_line`, výchozí ne): s ním se disketa točí, jen když má mechanika zapnutý motor
  bitem M3–M0 registru 0FH, a čte se až `spinup_ms` (výchozí 500 ms) po zapnutí. Bez otáčení nejsou indexové
  pulzy ani značky a 8272 čeká, dokud motor nenaběhne. Bez parametru se disketa točí stále (8" Consul se
  střídavým motorem). Zapnuté v sestavách s mechanikami PC (`cpm_ec1045`, `cpm_boot_dlouhe_rpd1_fdc1*`):
  jejich BIOS motor zapne při výběru mechaniky a monitor ho po nečinnosti vypne (časovače `TIM_Mx` v CONST).
  Monitor MON130711 pro 8" motory nezapíná, s `motor_line` by nenabootoval (test `test_sapi_sys`).
- **Doraz hlavy** je stopa 82 (`FloppyDrive::LAST_TRACK`, předpoklad). Stopa naformátovaná za koncem obyčejného
  obrazu zůstane jen v paměti, ImageDisk ji uloží.
- **Ověřeno formátovacím programem SAPIMANa** `fd5p_512_30` (5,25" HD PC, MFM 2 × 15 × 512, stopy 0–80, kontrola
  READ TRACK) v sestavě `cpm_boot_dlouhe_rpd1_fdc1` (2026-10-05, `docs/software/CPM_SAPIMAN.md`).

- **ECMON (JPR-1ZI) varianta 0 „FDC8“** s BIOSem ECBIF8 funguje s FDC-1 i na Z80 (`machines/cpm_zrmd_fdc1.sapi`,
  test `test_fdc_ecmon`): B: a C: jsou 8" diskety na mechanikách A a B řadiče.
- Společnou část s MFC-1A (mechaniky, 8272, 8257, DMA, stav) má třída `Fdc8272Card` (`src/cards/fdc8272.*`).

## Nejasnosti k ověření na HW

- Polarita D0 přepínačů SA2 (DD/SD) při čtení 0EH a k čemu je dvoupólový přepínač SA1 na FDC-1B (návod mluví
  o jednom „DIL přepínači“ čteném na D0 a D1).
- Hlásí 8272 na FDC-1 po resetu „ready changed“ pro všechny 4 jednotky? (MOR to snese, bylo by dobré to ověřit
  sekvencí SENSE INTERRUPT STATUS po resetu.)
- Kolik taktů CPU zabere jeden přenos DMA (emulace 4).
- Registr 0FH (motory, hlavy) a výběr 0EH D1 D0 vedou jen na X3 FDC-1B. Motory zapínají BIOSy pro mechaniky PC
  (EC1045, dlouhé CP/M J. Bíby), formátovací programy SAPIMANa a FORM6. Vedly /M0–/M3 z X3 k mechanikám PC
  (MOTOR ON), a jak? Nuluje /RES registr 0FH (emulace ano)? Jak dlouho se mechaniky rozbíhají (emulace 500 ms)?
- Doba RQM po bajtu povelu (emulace 12 µs).
- Volí přepínač SA2 S1–S4 („8"“) hodiny 8272 (8 MHz pro 8" a mechaniky HD, 4 MHz pro 5,25" DD)? Jak EC1045
  připojoval mechaniky PC HD (přepínače, /RDY, signál HD/DD mechaniky)?
- Kam až dojede hlava použitých mechanik (8" Consul, 5,25" a 3,5" PC)? Emulace má doraz na stopě 82. Programy
  SAPIMANa formátují stopy 0–80.
