# SAPIemu – souhrn a předávka

Tento soubor je vstupní bod pro pokračování na jiném počítači nebo v nové konverzaci. Udržovat ho aktuální.

## Kde jsme skončili (2026-10-09) a co dál

- **Stav:**
  - Všechno je commitnuté na `main` a pushnuté na `origin/main` (repo je neveřejné), push jen na vyžádání.
  - Testy: 43 v debug i v release (2026-10-09, s `test_kzd1`); release `full` naposledy 45 včetně ZEX, 8080EXM a 8085EXM, Linux ve WSL 41 (2026-10-07, GCC 13 bez varování ve vlastním kódu). Vše prochází. Kdy které testy:
    `docs/build.md`.
  - Podklady mimo repo: `..\SAPI-karty\ZRMD-1\SRC\` (zdrojáky ECMON + CP/M, `build.py`, od 2026-10-06 dává L80
    příkaz v příkazové řádce a na „move anyway“ odpovídá N).
- **Alfa 0.1.0-alpha:**
  - Zip `out/package/SAPIemu-0.1.0-alpha-win64.zip` (vzniká `python tools/package.py`, přibalí i autorův IDE
    disk) poslal autor soukromě Liborovi Lasotovi, Petrovi Všijanskému a Michalovi Medkovi.
  - Čeká se na jejich zpětnou vazbu a souhlas s obsahem.
  - Tag `v0.1.0-alpha` a veřejný release zatím **nejsou**. Úkoly před veřejným vydáním jsou v bloku níže.
- **Alfa 0.2.0-alpha (připravená 2026-10-06):** `out/package/SAPIemu-0.2.0-alpha-win64.zip` (263 souborů, 21,7 MB,
  s IDE diskem jako minule), poznámky `docs/release-notes/v0.2.0-alpha.md`. Release build prošel všemi 35 testy
  (i ZEX a 8080EXM), ze zipu naběhne MikroMon, dlouhé CP/M, ZPS-4 s FDC-1 i tisk v `cpm_zrmd`. Je opět jen pro
  kamarády (soukromě), tag a veřejný release zatím ne. Obsahuje knihovnu disket J. Bíby (komerční software).
- **Alfa 0.3.0-alpha (přebalená 2026-10-07 večer, odeslaná kamarádům):** `out/package/SAPIemu-0.3.0-alpha-win64.zip`
  (342 souborů, 24,0 MB, s IDE diskem `work/ide` jako minule), poznámky `docs/release-notes/v0.3.0-alpha.md`. Obsahuje
  i SAPI-1.5 (ZPS-6 s fontem EPROM „59“), snímky a video, CRM-1, RTC-1, DSP-1. Release `full` prošel všemi 45 testy
  (ZEX, 8080EXM, 8085EXM), Linux ve WSL 41. Z rozbaleného zipu naběhne MikroMon, `jpr1x_bgp1s`, `jpr1s_bgp1s`, obě
  ZPS-1 s JPR-1X a JPR-1S (MIKROBASIC), sestava V (menu ECMONu), `cpm_ec1045`, `ZPS-3_CPM64K_IMA` a `zps/ZPS-6`
  (`A>`). Zase jen pro kamarády (soukromě), tag a veřejný release ne. (SOUHRN v zipu je o jeden commit starší.)
- **Hotové v této sérii (2026-10-04/05), podrobnosti v blocích níže a v `docs/rozhodnuti.md`:**
  - karty: MPH-1V (zvuk, 82C54, joystick, myš), ZRD-1V rev 2.1 (zámek zápisu, MAX187);
  - emulace: ukládání stavu (`docs/stav.md`), bzučák klávesnice a míchání zvuku na sběrnici;
  - GUI: nahrávání binárních souborů a HEX s dotazem, větší pole Popis;
  - nástroje: ZRDTEST (`media/zrd-1v`), assembler `tools/asm8080.py`, balík a obrázky do README.
- **Hotové 2026-10-05 odpoledne:** řadič **FDC-1** (8272 + DMA 8257, `docs/desky/FDC-1.md`) a sestavy ZPS-3
  s CP/M 2.2/P 48K a 62K pro RPD-1 i FDC-1 (`machines/zps/ZPS-3_CPM*.sapi`, `docs/software/CPM_ZPS-3.md`).
  Podklady od autora: `..\SAPI-sestavy\ZPS-3\`, `..\SAPI-karty\FDC-1\`. Nové otázky na HW jsou v bodě 26.
- **Hotové 2026-10-05 večer:** řadič **MFC-1A** (`docs/desky/MFC-1A.md`), sestavy `cpm_zrmd_fdc1.sapi` a
  `cpm_zrmd_mfc1a.sapi` (ECMON varianty 0 a 2), test `test_fdc_ecmon`, celkem 27 testů. Registr 0FH podle
  originálního schématu Tesla (`..\SAPI_DOKU\MFC-1A\`); **překreslení EC1045 se od něj liší** (D4/D5 prohozené,
  polarita D6), je jako volba `wiring = ec1045`. Sestava `cpm_zrmd_mfc1a.sapi` má `ec1045`, reset podle schématu
  a RAM disk s opraveným BIOSem ECBIM8 (`media/zrmd-1/..._fixMFC.bin`). Původní BIOS bez `RET` v `INIT_MFC` jde
  jen s odchylkou `reset_mode = pulse`, viz bod 27 Nejasností.
- **Hotové 2026-10-05 večer:** **zdrojáky systému JPR-1Z / ZRMD-1** (ECMON + CP/M se 4 variantami CBIOS)
  v `..\SAPI-karty\ZRMD-1\SRC\`, překlad přes M80/L80 pod novým nástrojem **`cpmrun`** (`docs/build.md`). Překlad
  souhlasí s EPROM i obrazem RAM disku. ECBIM8 je ve zdrojáku opravený a s opravou MFC-1A funguje i s resetem podle
  schématu (`docs/software/ECMON.md`). Baterie DMR-1 je ve všech sestavách vypnutá (autor ji nemá), takže ECMON
  po zapnutí nabídne menu. Ze zdrojáků je přeložené všechno včetně CCPZ V4.1 (stažený z sydneysmith.com)
  a BDOS DRI. Sestava `cpm_zrmd_mfc1a.sapi` má opravený RAM disk `..._fixMFC.bin` a reset 8272 podle schématu.
- **Hotové 2026-10-05 v noci (`docs/software/CPM_SAPIMAN.md`):** průzkum podkladů J. Bíby (SAPIMAN) a EC1045,
  sestavy s bootem CP/M z 8" diskety (`cpm_boot_mfc1a/fdc1/rpd1.sapi`, Consul 259.11), formáty disket PC (MFM,
  HD/DD, dvě strany) pro FDC-1 a MFC-1A, sestava EC1045 (`cpm_ec1045.sapi`, 3,5"/5,25" HD), okno Média zakládá
  diskety v libovolném formátu.
- **Hotové 2026-10-06 v noci:** **JPR-1X** (Libor Lasota, 80C85): procesor 8085 jako režim jádra 8080
  (`docs/8085.md`, 8085EXM s CRC skutečného čipu projde), deska jako varianta `Jpr1` (`docs/desky/JPR-1X.md`),
  sestavy `jpr1x_bgp1s` a `zps/ZPS-1_JPR-1X`. Debugger ukazuje instrukce 8085. Otázky na HW v Nejasnostech desky.
  Deska diagnostiky na K3 (rev 2.0, okno Zobrazení → Deska diagnostiky, MCP `diag_board`) s ukázkovými čítači
  `media/jpr-1x/diag_int.hex` (přerušení) a `diag_poll.hex`.
- **KZD-1** (2026-10-09, přání autora, `docs/desky/KZD-1.md`): kazetopáskový záznamník dat na DPP-1 X2 s MIKOSem/K
  V5.0 (originální EPROM 6. 4. 88), sestava `zps/ZPS-2_MIKOS_K.sapi`, test `test_kzd1`.
  - Model signálů rozhraní: dvě stopy, impulsy 40 µs, rozběh motorů, přetížení na konci pásky, zoubky proti zápisu.
  - Kazety `.kzd` (obě strany), okno Média s kreslenou kazetou a cívkami, MCP `kzd1`.
  - MIKOS/K: K_I, K_S, K_D, K_Z, K_L ověřené. Po K_R a K_D hlásí K_L `NENI KONEC SOUBORU`: chyba originálu
    (stejně i verze 18. 5. 88), ponecháno věrně, správně je K_Z nebo `.JMÉNO`.
  - Kopírovací program CKM (originál ze sapi.cz, `media/mikos/CKM.hex`): BASIC EXP z kazety MIKOS přešel na KZD-1
    a odtud naběhl.
  - Program `.hex`/`.bin` jde vložit rovnou do KZD-1 (okno Média, MCP `kzd1 program`): vznikne kazeta, jakou by
    zapsal MIKOS/K (`kzd_from_program`, sdílí `tape_load_image` a `tape_mikos_file` s magnetofonem).
  - Otázky na HW v Nejasnostech `KZD-1.md`.
- **APS-5** (2026-10-08, přání autora: „ať je co měřit a na co koukat“, `docs/desky/APS-5.md`): měřicí jednotka
  Tesla s původním firmwarem (7 × 2716 ze sapi.cz) v sestavě `machines/aps5.sapi`, test `test_aps5`.
  - Nové desky: ADP-1, DMT-1 a DRV-1A.
  - Panel HRK-1 jako klávesnice JPR-1 (klávesy, zámek, watchdog, logické vstupy a výstupy, /PFAIL).
  - RTC-1 má kabel XC2 (`xc2` = `aps5`).
  - Technologie: osm pecí s termočlánky, okno „APS-5: panel a pece“ (žhnoucí pece, graf), MCP `plant`.
  - Vypnutí sestavy proběhne jako výpadek sítě s varováním.
  - Tlačítko DEMO (2026-10-09) odťuká ukázku `docs/aps5-ukazka.md` na panelu a pece zrychleně nahřeje.
    Autor vyzkoušel v GUI („paráda“).
  - Firmware měří s chybou 0–3 °C a reguluje adaptivním PSD (pece 1, 2 a 5 drží 700 °C).
  - Při tom se opravila logika 3214 na JPR-1/JPR-1X (`docs/desky/JPR-1.md`).
- **Sestava `libor_doma`** (2026-10-08, zadání autora): ImBUS12 s deskami V, AND-1V F800h při MAP1 = L, DGD-1V
  a CGA-1V na C000h při MAP1 = H (MAP2 = L / H), RAM-1V s dírami 2 KB a 16 KB, bez IDE-1. CP/M naběhne.
- **Sběrnice ImBUS9 a ImBUS12** (2026-10-08, Libor Lasota, `docs/desky/ImBUS.md`): typy sběrnice s 9 a 12 pozicemi,
  X1 jen pro CPU (adresy přes MH3216 z K1), bez JP3.
- **Sestava `cpm_zrmd_rtc_dsp`** (2026-10-07 v noci): `cpm_zrmd_dpp1` + RTC-1 (A0h) + DSP-1 (48h, TCP 2324), E: =
  disketa s `RTCZ.COM`; ověřené z cli (`RTCZ`, ESC, `DOPCP`). Jméno podle autora: se ZRMD funguje skoro všechno.
- **Hotové 2026-10-07 večer: DSP-1** (`docs/desky/DSP-1.md`, test `test_dsp1`): dva kanály 8251, porty podle SA1/SA2
  (výchozí jako J. Bíba 48h, 4Ah, 4Ch), rychlost SA3/SA4 z 12,288 MHz, maska přerušení na INT0. Programy XMODEM
  J. Bíby (`media/dsp-1`) pod CP/M přenesou soubor do PC i zpět (test proti XMODEMu emulátoru).
- **Hotové 2026-10-07 večer: RTC-1** (`docs/desky/RTC-1.md`, test `test_rtc1`) podle návodu, originálního schématu
  (sken J. Bíby) a repliky EC1045: čítač reálného času (výchozí podle hodin PC, po prvním zapnutí čas od pondělní
  půlnoci), dva 8253, přerušení kabelem z XC2/14, watchdog na /RTL. /RTL teď berou i procesorové karty SAPI-1
  (dřív jen PKT-1), karty dostávají Φ2 sběrnice (`Bus::phi2_hz`). Opravený BCD v 8253 (zapsaný počet se nepřeváděl
  z BCD) a hromadné odpočítávání `I8253::ticks()`. Ověřeno `RTCZ.COM` J. Bíby pod CP/M (den a čas PC).
  Jeho `RTC421.COM` pro hodiny na ZRMD-1 odhalil chybu emulace 72421: v HOLD registry neukazovaly aktuální čas
  (opraveno, test v `test_zrmd`).
- **Odpovědi EC1045 2026-10-07** (`..\Dotazy\EC1045_2026-10-07\`), zapracované téhož dne:
  - **dump EPROM „59“ znakového generátoru DBS-1:** je teď výchozím fontem DBS-1A (`roms/dbs-1a/DBS-1A_CharGen_59.bin`),
    potvrdil adresování i semigrafiku 2 × 3, navíc má rámečky a řecká písmena (`docs/desky/DBS-1A.md`);
  - **originální MFC-1A (v. č. 0124) odpovídá jeho překreslení** (DD11, D4/D5): výchozí `wiring` je od teď `ec1045`,
    schéma Tesla je chybné (`docs/desky/MFC-1A.md`);
  - **DPP-1:** ostřejší fotka, přepínače rozepsané v `docs/desky/DPP-1.md` (28h, X3 = kanál 2 obousměrný pro
    PMD32-SD, emulace sedí); tabulky 7 a 9 návodu mají chybu u obousměrného portu;
  - **FDC-1:** SA2 `11000011` je souměrné, SA1 na FDC-1B nahrazený propojkou; **tiskárna:** v jeho kabelu plotteru
    vede X3/16 na BUSY (DSUB25/11, zapojení jako LPT PC); **ZPS-5:** dvojitá vana, dlouhá sběrnice, bez MFC-1A;
  - pro Petra je připravená oprava BIOSu MFC8 (`..\Dotazy\EC1045_2026-10-07\pro_Petra\`: opravené ECBIM8.MAC
    a ECBIM8_cd.MAC a obraz RAM disku s přepsanými dvěma bajty, v emulátoru ověřený); poslat ji musí autor.
- **Hotové 2026-10-07 večer: CRM-1** (zálohovaná CMOS RAM 4 KB, `docs/desky/CRM-1.md`, test `test_crm1`) podle
  návodu Tesla; autor desku nemá, software pro ni neznáme. ZPS-7 až ZPS-9 jako samostatné sestavy nebudou (od ZPS-6
  se v emulaci neliší), poznámka v `docs/software/SAPI-1.5.md`.
- **Hotové 2026-10-07 večer: snímky obrazovek** (`src/frontend/shot.cpp`): Ctrl+F5 (jako DOSBox; F12 je reset),
  pravé tlačítko na obraz (uložit, uložit jako, do schránky PNG + BMP, volba zvětšený 3× s poměrem TV), Zobrazení →
  Snímek obrazovky. Výchozí jsou přesné body emulace (autor). **Video se zvukem** (`src/frontend/video.cpp`):
  Ctrl+Alt+F5, MP4 (H.264 + AAC) přes ffmpeg do `work/videa`, 50 snímků za sekundu emulovaného času
  (`EmuHost::tick_hook` dělí běh na snímky), zvuk ze sběrnice (`Bus::audio_record`), na konci spojení obou stop.
  Ověřeno `--ui-video`: ZPS-6 1536 × 720, sestava V se SPLAY 1280 × 960 s hudbou, obě stopy stejně dlouhé. Autor
  vyzkoušel, funguje. Večer zvětšení změněné na sudé (4×, VGA 2×, crf 14), protože 3× s barvou po blocích 2 × 2
  rozmazávalo barevné hrany (`docs/rozhodnuti.md`).
- **Hotové 2026-10-07 odpoledne: SAPI-1.5, sestava ZPS-6** (`machines/zps/ZPS-6.sapi`, test `test_sapi15`,
  `docs/software/SAPI-1.5.md`): karty **PKT-1**, **RSW-1**, **DBS-1A** (`docs/desky/PKT-1.md`, `RSW-1.md`, `DBS-1A.md`)
  podle dumpů PROM ze sapi.cz, schémat a návodů Tesla. ROM MON-V100690 (dump EPROM, i opravená verze Melsoft)
  nabootuje CP/M 2.2 62K z disket Melsoft `.sp2` (`media/sapi15`). Znakový generátor DBS-1 nejdřív jako náhrada
  z fontu AND-1Z, večer dump EPROM „59“ od EC1045. ZPS-5 čeká na firmware. Rozbory ve `work/sapi15/report_*.md` (mimo git).
- **Hotové 2026-10-07** (podrobnosti v `docs/rozhodnuti.md`):
  - **konfigurátor:** karty jde myší přetáhnout do jiného slotu (přesun, prohození se stejně širokou kartou).
    Sloty sběrnice jsou rovnocenné, takže se přesun provede rovnou i v běžícím stroji a sestava je jen neuložená
    („Vrátit změny“ vrátí karty i v běžícím stroji). Snímek `--ui-drag Z:DO[:drop]`;
  - **terminál:** výběr myší (tažení, dvojklik slovo, trojklik řádek), po puštění tlačítka rovnou do schránky
    (volba, výchozí zapnuto), Kopírovat bez výběru bere celý text. Snímek `--ui-terminal`, `--ui-term-text`,
    `--ui-term-sel`;
  - **odpověď EC1045** zapracovaná (`docs/software/CPM_SAPIMAN.md`), odpověď Petrovi s popisem chyby `RET`
    v ECBIM8 odeslaná; **FDC-1 `motor_hl`**: MOTOR ON mechanik z /HL 8272 jako u EC1045 (sestava `cpm_ec1045`);
  - **úklid:** varování GCC pryč, `test_tapegen` a `test_jpr1x` prošly i ve WSL; release buildy bez záchranné cesty
    ke zdrojákům (`SAPIEMU_SOURCE_FALLBACK`, `docs/build.md`), `package.py` to hlídá;
  - `jpr1s_bgp1s` má na K3 desku diagnostiky (autor).
- **Hotové 2026-10-06 večer** (podrobnosti v `docs/rozhodnuti.md`):
  - **kazeta z programu** HEX/BIN (i `.txt`) pro MIKOS a MIKROBASIC, „Uložit jako WAV…“, `--make-tape`
    (`docs/software/kazeta.md`);
  - **konfigurátor:** co běžící stroj ještě nemá, je oranžové (tlačítko „Restartovat s touto sestavou“, sloty,
    parametry, stavový řádek), „Vrátit změny“ vrací i na běžící stroj, TURBO z MCP se už nevrací;
  - **ANK-1 / ANK-1M2 z klávesnice PC:** držíš = drží (bez fronty a autorepeatu), SHIFT před klávesou (konec
    „N,“ v MIKROBIOSu), režim pro hry Ctrl+F7 (Manic Miner), v liště „LED SHIFT“ (`docs/desky/klavesnice.md`);
  - diskety ze Slušovic: TNS, ne SAPI (`docs/software/TNS_Slusovice.md`).
- **Hotové 2026-10-06** (podrobnosti v `docs/rozhodnuti.md` a v uvedených souborech):
  - **okno Média:** výběr formátu nové diskety jednou nad mechanikami, širší okno; snímky `--ui-media`,
    `--ui-printer`;
  - **originální programy J. Bíby** (formátování, číslování sektorů) na FDC-1 a MFC-1A: HD, DD, FM i 8" fungují.
    Doraz hlavy je teď stopa 82, protože programy formátují stopy 0–80. Stopa za koncem obrazu `.img` zůstane
    v paměti. Nová disketa `.imd` je předformátovaný ImageDisk (`docs/software/CPM_SAPIMAN.md`);
  - **MFC-1A proti výpisům z reálných desek** (`SAPI15\MFC_TEST_CPM`, `docs/desky/MFC-1A.md`): s `rdy_ground`
    se shoduje. Změna READY v 8272 nepřepíše čekající stav, `rdy_ground` nezávisí na D6. Sestavy SAPIMANa
    (`cpm_boot_mfc1a*`) mají RDY trvale (rozhodnutí autora);
  - **`cpmrun`:** stav konzole hlásí stdin, test `test_cpmrun`. „L80 nebere `/P:0`“ byl převod cest v Git Bash
    a odpověď Y na „move anyway“ (`docs/build.md`);
  - **motory mechanik** FDC-1 a MFC-1A (`motor_line`, `spinup_ms`), zapnuté v sestavách s mechanikami PC
    (`cpm_ec1045`, dlouhé CP/M); 8" Consul se točí stále;
  - **všechny bootovací diskety J. Bíby** ze `SAPIMAN\PMD32-SD_IMAGE8` vyzkoušené; nová sestava jen
    `cpm_boot_fdc1_irmd` (BIOS si RAM disk připraví sám), výsledky ostatních v `CPM_SAPIMAN.md`;
  - **okno terminálu:** Poslat soubor, Poslat textový soubor, Poslat a Přijmout soubor XMODEM (`core/xmodem`,
    testy s DOCPM10 a DOPC10), Kopírovat a Vložit (Alt+C, Alt+V jako TeraTerm), `docs/seriova-linka.md`;
  - **desky Libora Lasoty k původním sestavám** (z posledních revizí, fakta z netlistů EAGLE): **JPR-1S** rev 2.2
    (8080, banky RAM, MIKROBIOS 1.0/1.02/2.1), **BGP-1S** rev 4.2 (grafika PMD-85, Colorace, TV), **AND-1B** rev 1.4
    (VGA náhrada AND-1/AND-1A), redukce **Redukce34KB01** (sériová linka pro BOOT); sestavy `jpr1s_bgp1s` (Manic
    Miner odpovídá fotce ze skutečného stroje) a `zps/ZPS-1_JPR-1S_AND-1B`; testy `test_and1b`, `test_bgp1s`;
  - **klávesnice ANK-1M2** (Libor Lasota): ANK-1 s konektorem joysticku (joystick z šipek + mezerníku nebo
    gamepadu, myš Atari ST / Amiga), společný kód joyportu s MPH-1V, `docs/desky/klavesnice.md`;
  - **testy paralelně a po úrovních** (`docs/build.md`, „Kdy které testy“): debug 25 s místo 100 s, release bez
    ZEX 9 s, `tools\build.cmd release full` se ZEX asi 3 min, `tools\build.cmd debug test "zps3|rpd1"` jen vybrané;
  - **Linux build ve WSL** (Ubuntu 24.04, GCC 13): přeloží se, všech 33 testů projde, GUI přes WSLg; opravy pro
    GCC (`static constexpr`, CR v řetězci), Windows build beze změny chování (`docs/build.md`);
  - **přepínání jazyka GUI** (čeština, angličtina): Zobrazení → Jazyk / Language, výchozí podle jazyka Windows, `--lang cs|en`, pevná ID oken (rozložení zůstane), test `test_i18n`;
  - **ZPS-3 s originálním CP/M 2.2 64K od Tesly IMA** (`machines/zps/ZPS-3_CPM64K_IMA.sapi`, ROM MON64C3,
    diskety z archivu EC1045 `DEX\SAPI_ZPS3\01–03`; PROM D1A a D3A pro F000 jsou předpoklad), test `ima64k`;
  - **ZPS-3 s originálním CP/M IMA 48K (46K)** (`ZPS-3_CPM48K_IMA.sapi`, MON V1.0-C1): disketa se nedochovala,
    systémové stopy vyrobilo originální `MOVCPM48 46 *` a `SYSGEN` v emulátoru, ověřeno proti zdrojům BOOT0C1
    a BIOS48C1; test `ima48k`;
  - **RPD-1 s druhým řadičem** (diskety J. Bíby `r2_*`, `r3_*`): `cpm_boot_rpd1_mfc256`, `_mfc512` (MFC-1A s 8"
    FM 14 × 256 a 8 × 512, ne DD) a `cpm_boot_rpd1_fdc1_rmd`;
  - **ZPS-4 s FDC-1** (`machines/zps/ZPS-4_FDC-1.sapi`, ROM MOR62ZF, disketa CPM62ZF ze sapi.cz). Varianty 48Z
    a 48ZF se nedělají (rozhodnutí autora, jen studijní význam), zůstávají v záloze sapi.cz;
  - **mechaniky v okně Média** jsou očíslované podle řadiče (0–3) s písmenem v CP/M ze sestavy nebo zjištěným za
    chodu (BIOS SELDSK + čtení sektoru), test `test_drive_letters`;
  - **tiskárna Centronics** na X3 karet JPR a na DPP-1 (8255 mód 1), okno Tiskárna, MCP `printer_output`, test
    `test_printer`, sestavy `cpm_zrmd` a `cpm_zrmd_dpp1` (`docs/tiskarna.md`). DPP-1 kanál 1 je v ECMONu `PUN:`.
- **Možné další kroky (autor vybírá; doporučení 2026-10-06: c) sestavy a diskety SAPIMANa, pak DUP):**
  0. **Diskety a CP/M, dodělávky z 2026-10-05** (podrobně v `docs/software/CPM_SAPIMAN.md`, „Dodělávky“):
     a) ověřit FDC-1 a MFC-1A, i DD a formátování, originálními programy SAPIMANa (`CPM_PROGRAMY\CISLOVANI
        SEKTORU`, `FORMATOVANI`, `Format_FDC_8`) a `MFC1.ASM`; **většinou hotové 2026-10-05**: formátování FDC-1 HD
        a 8", MFC-1A DD, FM a 8", číslování sektorů (doraz hlavy 82, nová disketa jako `.imd`), `mfc1test` se shoduje s výpisy z reálných
        MFC-1A (`docs/desky/MFC-1A.md`; sestavy SAPIMANa mají `rdy_ground = true`), DUP-80 (fyzická kopie) hotové 2026-10-06, test `dup8`; **bod a) je celý hotový**;
     b) ~~motory mechanik~~ hotové 2026-10-06 (`motor_line`, `spinup_ms`; sestavy s mechanikami PC);
     c) ~~sestavy s RAM diskem RMD-1 a s PMD32-SD na FDC-1~~ hotové 2026-10-06 (`cpm_boot_fdc1_rmd`,
        `cpm_boot_rpd1_rmd`, `cpm_pmd_fdc1`; tiskárna na X3 ve všech sestavách ECMONu a J. Bíby); knihovna jeho
        disket hotová (soupis 180 disket a výběr 16 v `media/sapiman/knihovna/`,
        `docs/software/SAPIMAN_diskety.md`); další bootovací diskety ze soupisu jsou v `CPM_SAPIMAN.md`;
     d) ~~`cpmrun`: test a L80 `/P:0`~~ hotové 2026-10-06 (`test_cpmrun`, `docs/build.md`);
     e) ~~vzhled okna Média s výběrem formátu~~ hotové 2026-10-05 (výběr jednou nad mechanikami, `--ui-media`);
     f) otázky na EC1045 (Petr Všijanský) odeslané 2026-10-06, **odpověď přišla týž večer** (souhrn v
        `docs/software/CPM_SAPIMAN.md`, e-mail a přílohy v `..\Dotazy\EC1045_2026-10-06\`): zdrojáky nic nového,
        ECMON IDE-1 nemá; JB rozchodil MFC-1A na jeho replice; 5,25" HD na FDC-1 přímým kabelem s MOTOR ON
        z /HL; PMD-32SD na horním konektoru DPP-1;
     g) případně opravit `RET` v ECBIM8 i v autorově pracovní kopii RAM disku (`work/zrmd-1`, jen se souhlasem).
  0b. **Z archivu EC1045 v `..\SAPI_DOKU_FTP`** (2026-10-05, `docs/software/CPM_SAPIMAN.md`):
     1) ~~obrazy ImageDisk (`.imd`) přímo v mechanikách~~ hotové 2026-10-05 (test `test_floppy_imd`; `JILEK\04`
        jsou diskety MS-DOS se soubory SAPI, čitelné, ale ne CP/M);
     2) ~~sestavy podle systémových disket `PETR\SAPI_SYS`~~ hotové 2026-10-05 (`cpm_boot_*_ss256`, `cpm_boot_mfc1a_rmd`,
        `cpm_term64k_fdc1`); „dlouhé CP/M“ RPD-1 + FDC-1 5,25" HD hotové
        (`cpm_boot_dlouhe_rpd1_fdc1.sapi`, bootovací disketa poskládaná ze zdrojáků); i pro 3,5" (`…_35.sapi`);
        RPD 170212 + PMD32-SD hotové (`cpm_pmd_rpd1.sapi`, nová karta **DPP-1**);
     3) ~~obrazy `PETR\2MB` jako disky PMD32-SD~~ hotové (`media/pmd32sd/16xx.2MB`, sestavy `cpm_pmd_rpd1`,
        `cpm_zrmd_dpp1` s DPP-1). ~~Tiskárna~~ hotová 2026-10-06: Centronics na X3 karet JPR (tam ji mají
        ECMON i J. Bíba) a na DPP-1 (mód 1, `LST: = UL1:`), okno Tiskárna, MCP `printer_output`
        (`docs/tiskarna.md`). Kanál 1 DPP-1 je v ECMONu `PUN:`, ne tiskárna.
  0c. **ZPS-3** (2026-10-06, `docs/software/CPM_ZPS-3.md`): ~~originální CP/M 64K Tesla IMA~~ hotové;
     ~~rekonstrukce IMA CP/M 48K~~ hotová (originální MOVCPM48 a SYSGEN, sestava `ZPS-3_CPM48K_IMA`);
     ~~diskety ze Slušovic~~ rozebrané 2026-10-06: **nejsou pro SAPI**, jsou pro počítače TNS JZD Slušovice
     (Z80, IM 2, displej FC00, řadič KORD4 / FD17xx na 60h–78h), `docs/software/TNS_Slusovice.md`. Obecné programy
     (Turbo Pascal, dBASE II, WordStar…) by šly přenést na disketu CP/M pro Z80 SAPI.
  1. Otázky k ověření na HW: **jen průběžně sbírat** do „Nejasností“. Autor je bude testovat mnohem později,
     možná na společné akci (2026-10-05). ZRD-1V se ZRDTEST.
  2. Neemulované karty: DPP-1, ~~DSP-1~~ (2026-10-07), ~~DBS-1A~~ (2026-10-07), AND-30C, ~~CRM-1~~ (2026-10-07), RAM-64, RPD-1Z, ~~RTC-1~~
     (2026-10-07). Podle
     podkladů, které autor má. ~~DPP-1~~ hotová 2026-10-05 (PMD32-SD, od 2026-10-06 i tiskárna).
  3. „Na promyšlení“ níže: čekací stavy AND-1V, STSTB JPR-1V, klávesnice ELIS v3, časování RMD-1.
  4. ~~Převod HEX na kazetu (WAV) pro DSM-1~~ hotový 2026-10-06: program HEX/BIN se při vložení do magnetofonu
     převede na kazetu MIKOS (`.K_L`, `.J`) nebo MIKROBASIC (LOAD), „Uložit jako WAV…“, cli `--make-tape`
     (`docs/software/kazeta.md`). Otázky na HW: přehrání WAV z PC do skutečné DSM-1, minimální mezery.
  5. ~~Linux build~~ ověřený ve WSL 2026-10-06 (`docs/build.md`, všech 33 testů a GUI přes WSLg); CI
     (GitHub Actions) zatím ne, nejdřív až s veřejným repem nebo jen s linuxovým během.
  6. ~~Přepínání jazyka UI~~ hotové 2026-10-06 (Zobrazení → Jazyk / Language, výchozí podle Windows, `--lang`,
     `docs/build.md`).
  7. ~~PKT-1~~ hotová 2026-10-07 se sestavou ZPS-6 (a RSW-1, DBS-1A). Zbývá ZPS-5 (firmware se nenašel). ~~Dump
     znakového generátoru DBS-1~~ od EC1045 2026-10-07. ZPS-7 až ZPS-9 jen poznámkou (`docs/software/SAPI-1.5.md`).
  8. Příprava veřejného vydání (níže).
  9. ~~Okno terminálu: kopírování jen výběru myší~~ hotové 2026-10-06 (tažení, dvojklik slovo, trojklik řádek,
     bez výběru celý text; `docs/seriova-linka.md`); ~~diagnostická deska na K3~~ hotová 2026-10-06
     (rev 2.0, JPR-1S i JPR-1X, okno, MCP `diag_board`, čítače `media/jpr-1x`).
  10. ~~Uklidit varování GCC~~ hotové 2026-10-06 (čisté sestavení ve WSL bez varování ve vlastním kódu, všech 37
     testů včetně `test_tapegen` a `test_jpr1x` prochází).
  11. **Navržené další kroky (2026-10-08):**
     a) souhrnný seznam otázek k ověření na HW podle desek, které autor má (teď rozházené po „Nejasnostech“
        `docs/desky/*` a v bodech 1–32 níže), jako podklad na společnou akci;
     b) další várka otázek pro EC1045: software nebo ROM k sestavě **SAPI-IPS** (JPR-1Z, AND-30C, RAM-64, RFD-Z),
        software k RPD-1Z, pořadí A5/A6 na SA2 u DSP-1, kam vedl kabel /INT z XC2/14 u RTC-1;
     c) **SAPI-IPS** na sapi.cz (`..\SAPI.CZ\sapi\and-30c.php`, `ram-64.php`, `rfd-z.php`): schémata ze zpětného
        inženýrství a dumpy PROM dekodérů jsou. AND-30C: 6845, 80 × 21, font jako AND-1Z (2716/2732), dekodér PROM
        po 2 KB. RAM-64: 8 × 6264 se zálohou, PROM 74188 (stránkování). RFD-Z: WD2797 bez DMA, konektory pro 8"
        i 5,25"/3,5", adresace v portech i v paměti. Bez softwaru (ROM, systém) zatím není na čem emulaci ověřit;
     d) RPD-1Z (8271 + 8257, AR B 6/1985, replika EC1045): podle EC1045 pod CP/M náhodně chybuje, software chybí.
     e) **Průzkum desek ze `sapi.cz/sapi/desky.php`** (2026-10-08), které mají podklady i software:
        1. ~~**APS-5**~~ hotové 2026-10-08 (výše). Původní poznámka: (měřicí ústředna Tesla, regulace pecí s termočlánky): ADP-1 (A/D C520D), DMT-1 (zesilovač
           s reléovým multiplexem), DRV-1A (relé, 77h), HRK-1 (14klávesová klávesnice, watchdog). **Celý originální
           firmware** (7 × 2 KB INTELHEX) je v `..\SAPI.CZ\sapi\doku\aps-5.php`, ostatní desky sestavy (JPR-1, REM-1,
           AND-1, CRM-1, RTC-1) už emulujeme. Potřeba jednoduchý model pece a termočlánků;
        2. **RKD-1** (řadič kazetového disku CM 5403, 8255 + 8253, 40h): ovladač, BIOS CP/M, boot ROM a formátovač
           se zdrojáky (Tesla Elstroj 1987) na `..\SAPI_DOKU\SAPIMAN\PMD32-SD_IMAGE8\ZDROJ_STARE\a_mercl.8sd`,
           `FORMRKD.COM` na `a_sluzebni CPM.8sd`; návod `rkd-1.php`, sken `SAPIMAN\Scany dokumentací\RKD1.pdf`;
        3. **DSE-1** (simulátor a programátor EPROM 2716/2732, F0–F4h): programy pro MONITOR V4 (`SAPI_EPROM\ZPS-2\V4\
           DSE-1_1000H_programator.BIN`), CP/M `DSE.COM` (`PERINA\dse.8sd`) a `SIMPROG` (`SAPI15\SimProg_opr`);
        4. **DPS-1** (sběrnice IMS-2 / IEEE-488, 30–37h): BASIMS4.SYS je už v `media/rpd-1/MIKOS_D_V5.0_system.img`,
           chtělo by to virtuální přístroj (voltmetr M1T330 z návodu);
        5. **RMP-1** (řadič magnetické pásky IZOT): sada pro CP/M (DIRMT, MTCOPY, MTDAT…) na `a_lad_izo.8sd`;
        6. BWP-1 + DTO-1 (tester obvodů autora sapi.cz, 38–3Bh) s programem IOT se zdrojáky.
        Bez softwaru: ROM-1, MTS-1, PDS-1Z, UDR-1, AND-1 úprava, DOV-1, DRV-1, DAP-1, ADC/DAC, desky CHP 3000
        (stránky `z-1xx`, Chemoprojekt). DMR-1A je nejspíš naše DMR-1. Vedlejší nález: programy pro plottery na DPP-1
        (Aritma Minigraf 0507/0517, XY4130, plotter z AR 1990). Výpisy a extrakty v pracovní složce průzkumu (mimo git).
     f) **Zásobník nápadů (2026-10-09, autor: „všechno jsou dobré nápady, vrátíme se k nim“):**
        1. model plotteru na DPP-1 (okno s kreslením, export PNG/SVG; ovladače Minigraf 0507/0517, XY 4130,
           AR 1990; ovladač XY 4130 je za MIKOSem V5 v `_ROM\MIKOS v5\MIKOS5.BIN`);
        2. RKD-1 (kazetový disk CM 5403, CP/M ze zdrojáků Tesla Elstroj);
        3. DSE-1 (simulátor a programátor EPROM);
        4. DPS-1 (IMS-2) s virtuálním voltmetrem, BASIMS4 už máme;
        5. souhrnný seznam otázek k ověření na HW (bod a);
        6. další várka otázek pro EC1045 (bod b);
        7. alfa 0.4.0 pro kamarády (APS-5 s DEMEM, CRM-1, RTC-1, DSP-1, ImBUS, nové sestavy).
     g) **KZD-1** (kazetopáskový záznamník dat ZPA Košíře 1986, k SAPI s MIKOSem/K V5.0 TESLA ELTOS DIZ 1988).
        **Hotovo 2026-10-09:** `docs/desky/KZD-1.md`, sestava `zps/ZPS-2_MIKOS_K.sapi`, test `test_kzd1`. Dál se nabízí
        kopírovací program CKM (magnetofon ↔ KZD-1, zdroják `CKM.MAC`) a převod programu na kazetu `.kzd`. Podklady:
        - `..\SAPI_DOKU\KZD-1\` (sken návodu `kzd1.pdf`, 29 stran);
        - MIKOS/K V5.0 (6. 4. 88 a 18. 5. 88, zdroje, PRN, HEX) v `..\SAPI_DOKU\SAPI_EPROM\ZPS-2\V5\`
          a `MIKOS5_K\`; kopírovací program CKM (magnetofon ↔ KZD-1);
        - ovladač z PMD 85-3 upravený pro MIKROBASIC na SAPI (port 2C00h, `KZD_SAPI.txt`);
        - návod MIKOS V5 na sapi.cz (`doku2/v5.php`): kabel KZD-1 → DPP-1 X2, 8255 na 30h/31h/33h, formát
          záznamu (dvě stopy, asi 10 kbit/s, bloky se synchronizací FBh);
        - wiki https://pmd85.borik.net/wiki/KZD-1.
- **Pracovní pravidla, která se osvědčila:**
  - Skripty s `\n`, `\r` a `\b` psát nástrojem Write do souboru, ne přes heredoc v bashi. Shell zpětná lomítka
    rozbije a do souborů se pak dostanou řídicí znaky.
  - Po commitu obnovit CRLF smazáním a checkoutem commitnutých souborů, jen když commit prošel.
  - GUI ověřovat přes `--ui-snapshot`, stav emulace přes `sapiemu-cli`, ne snímky plochy.
  - `sapiemu-cli` z Git Bash občas nevypíše nic: pustit z PowerShellu. PowerShell 5.1 v argumentech zahodí uvozovky
    (`--type 'PRINT "X"'`), na text s uvozovkami použít Python.
  - Sestavy ukládat zapisovačem konfigurátoru: `sapiemu-cli --mcp`, nástroj `set_machine_config` se `save_as`.
  - Výstup `sapiemu-cli` spolehlivě přesměrovat do souboru (`> soubor.txt 2>&1`) a ten přečíst.
  - Snímky GUI se změnou v konfigurátoru: `--ui-set SLOT:KLÍČ=HODNOTA` / `SLOT:remove`, tažení karty
    `--ui-drag Z:DO[:drop]` (simulované události myši ImGui); okna `--ui-media`, `--ui-printer`, `--ui-diag`, `--ui-aps5`,
    `--ui-terminal` (s `--ui-term-text`, `--ui-term-sel`); stav se scénou `--ui-state` (stav připravit přes
    `sapiemu-cli --save-state`).
  - Jiná sezení Claude (např. SAPI-Flappy) pouštějí `sapiemu-cli` z tohoto repa přes MCP, jejich uložené sestavy
    pak končí v `machines/`. Necommitnuté změny, které jsem nedělal, ukázat autorovi a nepřidávat.
  - Před každým pushem projít commity od `origin/main` a zaktualizovat všechny dotčené `.md` (CLAUDE.md, Git).
  - Rozbor nových desek: netlist z EAGLE (`eaglecon`, CLAUDE.md), u procesorů zdroje s citacemi a testy proti CRC
    skutečného čipu (`docs/8085.md`). Pracovní soubory rozborů jsou mimo git ve `work/` (`work/jpr1x`,
    `work/i8085`, `work/tapefmt`).

## Stav k 2026-10-04

**Před veřejným vydáním (autor, v klidu):** zip 0.1.0-alpha teď jde jen soukromě (Libor Lasota, Petr Všijanský,
Michal Medek), repo zůstává neveřejné, dokud to neodsouhlasí. Pak:
- vybrat a pečlivě připravit sestavy `.sapi` (názvy, popisy, které vůbec vydat);
- připravit vydávací obsah disků (IDE, RAM disk) místo autorova pracovního `work/ide`;
- rozhodnout o právech: hudba RAD/VGM na disku, programy třetích stran (komerční software z 80. let na disketách
  `media/sapiman/knihovna`, `media/pmd32sd`: Turbo Pascal, dBase II, WordStar…);
- zvolit výchozí sestavu pro první spuštění;
- veřejný zip pak případně `python tools\package.py --no-ide`.

**Příprava první alfa verze 0.1.0-alpha:**
- README s obrázky (`docs/img/`), návod `docs/prvni-kroky.md`, poznámky `docs/release-notes/v0.1.0-alpha.md`;
- balík `python tools/package.py` (postup v `docs/build.md`, „Vydání“);
- statický runtime MSVC;
- snímky GUI se stavem (`--ui-state`, `--ui-size`).

**Hotové: bzučák klávesnice** (`docs/desky/klavesnice.md`).
- EKL-1 a Consuly: aktivní bzučák 2,4 kHz na P0-OUT1 = 0, i krátké pípnutí po resetu (3212 se nuluje).
- ANK-1: piezo za P0-OUT D6, tón dělá program (MIKROBASIC asi 750 Hz).
- Zvuk se míchá na sběrnici se zvukem MPH-1V (`Bus::audio_add`, `audio_collect` po každém běhu), přepínač
  `keyboard.beeper`.

**Hotové: ukládání stavu stroje** (`docs/stav.md`): Soubor → Uložit / Načíst stav, rychlý slot Ctrl+F2 / Ctrl+F3
(`work/state/`), MCP `save_state` / `load_state` (i do paměti jako checkpointy), CLI `--save-state` / `--load-state`.
- Ve stavu je CPU, paměti včetně RAM disků (autor), všechny karty, klávesnice a sériové kanály.
- IDE disk, SD karta, diskety a kazety jsou jen odkazem s varováním, když se soubor změnil.
- `test_state` ověří u každé sestavy z `machines/`, že běh z načteného stavu je bajt po bajtu stejný jako
  původní. **Nová karta musí mít `state()`**, jinak to test pozná.

**Hotové: ZRD-1V rev 2.1** podle nového schématu (`..\SAPI-karty\ZRD-1V\ZRD-1V_rev20`): parametr `revision` (2.1 / 1.1).
- Zámek zápisu: zápis na 60h jen do 20 ms po přístupu na 62h, jinak se karta zamkne do resetu (tak ji zamyká
  MikroMon).
- LED podle desky: ZAM červená = zamčeno, DSK zelená = přístup.
- Měření baterie MAX187 (čtení 61h, D5, `battery_mv`) a vedlejší účinek čtení 61h–63h.
- Zámek se neukládá do sestavy (autor), konfigurátor ho ukazuje živě. Popis v `docs/desky/ZRMD-1.md`.
- Autor má rev 1.1: sestavy V jsou na 1.1, výchozí v katalogu je 2.1. Test karty `media/zrd-1v/ZRDTEST`
  (CP/M), assembler `tools/asm8080.py`.

**Hotové: MPH-1V** (Liborova zvuková a joysticková karta: YM3812, 8253, joystick) v sestavě V na X8, báze 50h.
- YM3812 na jádru Nuked-OPL3 (third_party, LGPL) s časovači a stavem OPL2 a bitem WSE, 8253 (režimy 0–5,
  kaskáda), výstupní filtr karty. Zvuk v GUI (Stroj → Zvuk a joystick, jen při 100 %), joystick z gamepadu
  a volitelně šipky + mezerník, `sapiemu-cli --audio-wav`, MCP `audio_record` a `joystick`.
- Podle **schématu hotové desky** (`..\SAPI-karty\MPH-1V\Eagle`, 6. 9. 2025, autor ho sehnal později): souhlasí s tím,
  co emulace dělala podle MPH-test 0.99 (STATUS `F2 F1 T2 T1 T0 0 0 YM`, F1 z OUT1). Časovač je **82C54**
  (kusovník), emulace umí jeho read-back. SPLAY, RAT PLAYER, y1stdemo i MPH-test fungují.
- Programy v `media/mph-1v/`, hudba (VGM, RAD) v repu není, test ji bere z `..\SAPI-karty\MPH-1V`.
- Soubor → Nahrát binární soubor do paměti… (adresa, velikost, počet stránek pro `SAVE`), `sapiemu-cli --load-bin`.
- **Myš** na K2 (výchozí, Atari ST nebo Amiga), konektory K4/K2 jako parametry karty (nic / joystick / myš, platí
  hned), dva gamepady. GUI myš zachytí kliknutím do okna obrazovky po zapnutí v menu (výchozí vypnuto), prostřední tlačítko /
  Ctrl+F10 ji pustí.
  Nahrání Intel HEX se ptá stejně (rozsah adres ze souboru, od 0100h navržený příkaz `SAVE`), zapíše až po potvrzení. Autor ověřil hudbu i joystick
  v GUI.

**Hotové: DGD-1V** (Liborova grafika 320×240 s VGA, varianta DGD-1) a sestava `machines/zps/ZPS-2_MIKOS_V5_DGD-1V.sapi`.
- S2 jako u CGA-1V (báze 0000/4000/8000/C000h, MAP1/MAP2), S1 barva jako AND-1V, čekání na okno CPU.
- BASIC EXP V5.0/G kreslí, autorův TEST.BAS vypadá jako fotka obrazovky. Popis v `docs/desky/DGD-1V.md`.
- **Panely:** port u sériového konektoru, FRB (zelený, zlaté piny) u desek původní konstrukce podle Liborových
  štítků, D-sub u desek V, IDE-1 (IDC 40 a D-sub 15), RPD-1 (kabel KB-17 mezi deskami). Karty bez konektoru
  podle autora: ZRMD-1, RAM, REM. Další karty se projdou postupně (`docs/sestavy.md`).
- LED na panelu JPR svítí podle emulace (BOOT, u JPR-1 P1-OUT D7), `Card::panel_leds()`.
- **Rychlost a takt rozdělené:** TURBO a BIOS na panelu ZDR ovládají kartu CPU (JPR-1V), rychlost emulace je
  F8 / menu Stroj → Rychlost emulace (100/200/400 %/max). Stavový řádek „CPU … MHz  emulace … %“, displej
  FREKVENCE CPU efektivní takt (od 100 bez desetin).

**Hotové: jedna barva obrazu a okna podle karet.**
- Barva jednobarevného obrazu je parametr karty v sestavě: AND-1V propojky S1 (`color`), ostatní parametr
  `monitor` (luminofor). Menu Zobrazení → Barva i konfigurátor mění totéž a hned, uloží se se sestavou. Globální
  „Luminofor“ v nastavení GUI zmizel. Sestava V má AND-1V jantarovou (autor).
- Okna obrazovek se jmenují podle karty („AND-1V“, „CGA-1V“, „DGD-1“) a ukazují zvětšení v procentech
  („200 %“). Pravé tlačítko na obraz (a Zobrazení → Zvětšení obrazovky) nastaví 100/200/300/400 %. Rozložení okna se
  převede.
- Oprava: konfigurátor po otevření jiné sestavy zůstával na předchozí, když v ní byly neuložené změny (třeba
  barva z menu). Teď se před nahrazením neuložené sestavy zeptá: Uložit / Uložit jako… / Zahodit / Zrušit.
- Popis v `docs/sestavy.md` („Okna obrazovek a barva obrazu“).

**Hotové: CGA-1V** (barevná grafika 320×200, VGA, Libor Lasota) v sestavě V na X7.
- Nastavení: C000h při MAP1 = MAP2 = H (`OUT 63h,C0h`), jumpery podle fotky.
- Registry jsou ze schématu (CONFIG/STATUS, Bt476), EGA i CGA, barvové domény, dvě stránky, VBI na /INT0
  a čekací stavy.
- Liborovy obrázky CASTLE, REMBRAN1 a REMBRAN2 se zobrazí, CGATEST projde. REMBRAN2 střídá stránky po snímcích,
  proto emulátor prolíná dva poslední snímky.
- **TURBO JPR-1V** jde přepínat za chodu (tlačítko TURBO na panelu ZDR, menu Stroj, MCP `cpu_turbo`).
- Popis v `docs/desky/CGA-1V.md`, programy v `media/cga-1v/`.

**Hotové: desky V Libora Lasoty** (`machines/sapi1v.sapi`).
- JPR-1V (banky EEPROM MikroBIOS 3.0 / ECMON), AND-1V rev 2.3 (VGA 64x20), RAM-1V (generátor MAP na 63h,
  díry, volitelné RTC M48T35), DSM-1V (10h a 14h, 115200 Bd).
- ZRD-1V bez MAP, reset nenuluje adresu.
- ECMON + CP/M z RAM disku běží, MikroBIOS se ohlásí.
- Test `test_sapi1v`, celkem 24 testů. Propojky podle fotek autorových desek.

**Hotové: ZPS-4** (`machines/zps/ZPS-4.sapi`, `ZPS-4_ELIS.sapi`).
- Nové karty JPR-1Z (typ JPR-1ZI s 2716), RAM-1Z (dump D3B, přepínače S1) a RAM disk RMD-1 (256 KB / 1 MB).
- CP/M 2.2/P (Peřina) s RAM diskem E: i ELIS 61k CP/M s RAM diskem C: běží. ELIS v3.00 má nevyjasněnou
  klávesnici.
- Test `test_zps4`, celkem 23 testů. Popis v `docs/software/CPM_ZPS-4.md`.


**Hotové: řadič FDC-1 a CP/M 48K/62K pro ZPS-3** (2026-10-05).
- Karta `FDC-1` (desky A + B, dva sloty): 8272 (`src/chips/i8272.*`), DMA 8257 (`i8257.*`), dekodér portů podle dumpu
  PROM D5B, registry FDC-1B 0EH/0FH, přepínače SA2. Mechanika 8" a obraz diskety jsou společné s RPD-1
  (`src/chips/floppy.*`, `FloppyDrive`), okno Média obslouží každou kartu s mechanikami.
- DMA bere CPU 4 takty na bajt (`Bus::hold`, spotřebuje CPU karta).
- Sestavy `ZPS-3_CPM48K`, `ZPS-3_CPM62K` (RPD-1) a `ZPS-3_CPM48K_FDC-1`, `ZPS-3_CPM62K_FDC-1` s Peřinovými MOR48A,
  MOR62A, MOR48AF, MOR62AF a disketami od autora. 62K: druhá RAM-1A na C000–F7FF (předpoklad), AND-1A na F800.
- FORM6.COM z diskety FDC formátuje 8" disketu na B:, test `test_zps3` projde všechny čtyři sestavy.

**Hotové: ZPS-3** (`machines/zps/ZPS-3.sapi`).
- Nové karty JPR-1A (varianta JPR-1: BOOT, porty 01–03, D2C), AND-1A (D3A, E800) a RAM-1A (D1A,
  0000–BFFF).
- Originální 48K MON V1.0-C1 zavede z diskety CP/M 2.2/P (Peřina) a funguje DIR, TYPE, SAVE, ERA.
- Dekodéry RAM-1, RAM-1A, JPR-1A a AND-1A jsou soubory PROM v `roms/`, jiná adresace = jiný soubor
  (rozhodnutí autora).
- Test `test_zps3`, celkem 22 testů. Popis v `docs/software/CPM_ZPS-3.md`.

**Hotové: řadič pružných disků RPD-1 a MIKOS/D V5.0** (`machines/zps/ZPS-2_MIKOS_D.sapi`).
- Systém se zavede z diskety, funguje DIR, SAVE/LOAD a BASIC z diskety.
- Diskety 8" jako `.img` v okně Média (vložit, nová, vysunout, ochrana proti zápisu).
- Registry RPD-1 vycházejí z obrázků návodu a z rozboru EBIOS. Popis v `docs/desky/RPD-1.md`, test
  `test_rpd1`, celkem 21 testů.
- V záloze sapi.cz jsou i obrazy CP/M disket pro ZPS-3/4 (`data2/cpm48*`, `cpm62*`, `cpm64k*`).

**Hotové: BASIC EXP V5.0 a V5.0/G pod MIKOSem V5, nová karta DGD-1 (grafika 320×240).**
- `media/mikos/*.hex`, v GUI Soubor → Nahrát program do paměti (Intel HEX), `.B`.
- DGD-1 má vlastní okno „DGD-1“, snímek přes MCP `screenshot display=DGD-1` a `sapiemu-cli --display`.
- Popis v `docs/desky/DGD-1.md` a `docs/software/MIKOS.md`.

**Hotové: MIKOS V5.0 na ZPS-2 s klávesnicí Consul** (`machines/zps/ZPS-2_MIKOS_V5.sapi`, ROM Monitor V5.0F/E
+ MIKOS V5.0). Naběhne, klávesnice funguje, `.K_S`/`.K_L` funguje, BASIC EX V4.0 z kazety běží. BDK-1 je
prázdná karta bez funkce. Popis v `docs/software/MIKOS.md`.

**Hotové: ZPS-2 s MIKOSem V4.0F.** Nová karta RAM-1 (16/32/48 KB, S1/S2 podle tabulky 1, `docs/desky/RAM-1.md`),
ROM Monitor V4.0 + MIKOS 4.0F/4.0D a V4.1 pro Consul v `roms/mikos/`. `machines/zps/ZPS-2.sapi` naběhne
s `MIKOS-1 V4.0F +`. Z kazety se nahraje TOOL 80 (PEDIT) a BASIC EX (WAV i MP3) a spustí se `.J`. Ukládání `.K_A`
a `.K_S` funguje a jde zpět načíst. Magnetofon zapisuje při rozběhu motoru krátké ticho. Popis
v `docs/software/MIKOS.md`, test `test_mikos`, celkem 20 testů.

**Hotové: DSM-1 s magnetofonem.** Terminál jako u DMR-1 (TCP, COM), kazety WAV i MP3 odkudkoli, tlačítka v okně
Média a v MCP. MIKROBASIC SAVE a LOAD fungují. Fázový dekodér čte skutečné nahrávky TOOL 48, TOOL 80 a BASIC EX ve
WAV i MP3 bez chyby. Popis v `docs/desky/DSM-1.md`.

**Hotové: ZPS-1 s MIKROBASICem.** Nové karty JPR-1 (8080, porty v paměti 2400/2800/2C00, 3214), REM-1
(EPROM a RAM, propojky E/F/R) a AND-1 (40×24, režimy znaků, překreslený font MHB 2501/2502). ANK-1 s volbou
programu (MIKROBASIC/MIKOS). Sestavy `machines/zps/ZPS-1.sapi`, `ZPS-2.sapi` (MIKROBASIC 2.4) a
`MIKROBASIC_AR1983.sapi` (verze z AR/B 1983). Jádro 8080 `third_party/i8080`, test `test_i8080`, integrační test
`test_mikrobasic`. IDE-1 odpovídá na portech vždy. Popis v `docs/desky/JPR-1.md`, `REM-1.md`, `AND-1.md`,
`docs/software/MIKROBASIC.md`.

**Hotové: mapa portů a paměti s kolizemi** (konfigurátor, záložka „Mapa portů a paměti“; MCP `machine_map`;
popis v `docs/sestavy.md`). Vychází z dekodérů karet (PROM, jumpery). Kolize na čtení sestavu nezapne.

**Hotové: sestavy s příponou `.sapi`, NAPÁJENÍ jako vypínač s aretací, baterie DMR-1 (JP7, SRAM přežije
vypnutí).**

**Hotové: pamatování nastavení GUI** (`sapiemu-settings.json` vedle exe, popis v `docs/build.md`): poslední
sestava, okno, otevřená okna, zobrazení, terminál, rychlost. Opravené pády v menu (výběr sestavy, výměna
klávesnice) a ztráta psaní po použití textového pole ImGui. F8 přepíná rychlost emulace 100/200/400 %/max
(2026-10-04, dřív TURBO 2 / 4 MHz / max).

**Hotové: konfigurátor sestavy (Stroj → Sestava…), návod `docs/sestavy.md`.**
- Čelní pohled na rám (X1 vpravo, panely SPN a ZDR). Kliknutím na slot se karta vloží, vymění nebo nastaví
  (jumpery, ROM, obrazy).
- Katalog karet (`src/cards/catalog.cpp`) zná i karty, které zatím nejsou emulované. Ty jsou šedé a na sběrnici
  neodpovídají.
- Ukládání sestav jako komentovaný soubor `.sapi` kamkoli. Šablony ZPS-1 až ZPS-4 v `machines/zps/`.
- Změna se projeví po vypnutí a zapnutí (tlačítko NAPÁJENÍ na panelu ZDR).
- MCP: `card_catalog`, `get_machine_config`, `set_machine_config`. Sběrnice podle typu (ARB-1 8, ARB-1C 14,
  ARB-1C-8 8 pozic).
- `sapiemu --ui-snapshot` vykreslí GUI do souboru (kontrola vzhledu bez snímku plochy).
- Testy `test_machine_cfg` a rozšířený `test_mcp`, celkem 16 testů.
- **Do budoucna:** fotky panelů karet (PNG pro každou kartu, kreslený panel zůstane jako náhrada), mapa portů
  a paměti sestavy s kontrolou kolizí z PROM dekodérů, přesouvání karet tažením.

**Hotové: výběr klávesnice (F5).** EKL-1, Consul 262.3 (výchozí s úpravou 7474, volitelně původní pulz
1 ms), Consul 259.11 (KOI-7 s klávesou CLEAR, volitelně KOI-8 cs pro ECMON) a ANK-1 (matice 5×8).
- **Volba:** v sestavě (`keyboard` u JPR-1ZI), menu Stroj → Klávesnice, MCP `set_keyboard`,
  `sapiemu-cli --keyboard NAME`.
- **Klávesy PC** bez textu se mapují podle klávesnice (šipky, numerika B0–B9, F1 ROL, F2 COPY, F3 BREAK,
  F4/F6/F7 klávesy D0–D2, Pause CLEAR); tabulka je v `docs/desky/klavesnice.md`. MCP `press_key`.
- **Stav LED ANK-1** ve stavovém řádku a v `machine_info`.
- **EKL-1 posílá češtinu s prohozenou velikostí** jako jeho firmware, MikroMon proto po `ž` ukáže `Ž`
  (nejasnost 13).
- **Test** `test_keyboards`: porty všech klávesnic a MikroMon s pěti paralelními variantami. Celkem 15 testů.
- ANK-1 zatím nemá software pro JPR-1ZI (MIKOS a MIKROBASIC jsou pro paměťově mapovanou JPR-1).

**Hotové: sériová linka na straně PC (první část F5).** Ověřeno autorem 2026-10-03: TeraTerm přes TCP,
konzole na obrazovce (DCD vypnuté) a přenos souboru DOPC10 → XMODEM do TeraTermu (obsah souboru korektní).

- **TCP server pro každý sériový kanál** (`src/serial/bridge.*`):
  - TeraTerm se připojí přímo (`127.0.0.1:2323`), telnet nebo raw, rozpozná se automaticky;
  - DCD může sledovat připojení klienta, takže ECMON po resetu běží na TeraTermu;
  - ověřeno: menu ECMON, boot CP/M a `DIR D:` přes TCP.
- **COM port PC** (Windows `COMx`, Linux `/dev/tty*`), případně virtuální pár com0com.
- **Volby kanálu:**
  - „Bez přetečení“ (`pacing`, výchozí zapnuto): PC posílá až po přečtení předchozího znaku;
  - řízení toku RTS/CTS (`flow_control`).
- **Ovládání** v okně Sériový terminál (spustit/zastavit TCP, otevřít COM). Stav připojení je i v MCP
  `machine_info`.
- **Sestavy:** CP/M sestavy mají TCP 2323 zapnuté; u MikroMonu je příklad zakomentovaný.
- **DCD a CTS jsou samostatné** („Terminál připojen (DCD)“ volí konzoli, „Připraven (CTS)“ povoluje
  vysílání s řízením RTS/CTS). Přijímaný znak má dobu přenosu po lince.
- **XMODEM:** DOPC10.COM přenese soubor do PC (TeraTerm) i s konzolí na obrazovce.
- **Testy:** `test_serial_tcp` (raw i telnet přes IPv4/IPv6, DCD, MikroMon s USYNC) a `test_xmodem`
  (DOPC10 → přijímač XMODEM). Celkem 14 testů.
- **Návod:** `docs/seriova-linka.md`.

Fáze F4 (IDE-1 s IDE diskem a PMD32-SD, okno Média) je hotová a ověřená autorem.

Dřív hotové fáze:
- **F4:** IDE-1 (IDE disk, PMD32-SD), sestava `cpm_ide.sapi`, okno Média.
- **F3:** ZRMD-1 + RTC, boot CP/M z RAM disku, sériový terminál s emulací SAPI/ANSI.
- **F2:** MCP server (`docs/mcp.md`), debugger, sériový terminál, `EmuHost`.
- **F0:** kostra, build a dokumentace z průzkumu.
- **F1:** jádro, karty JPR-1ZI, DMR-1, AND-1Z-89 a EKL-1, GUI, `sapiemu-cli`. ZEXDOC prošel.

## Plán fází

| Fáze | Obsah | Stav |
|---|---|---|
| F0 | Kostra repa, build, dokumentace z průzkumu, ROM a média | ✅ hotovo |
| F1 | Jádro, JPR-1ZI, ARB-1C, DMR-1, AND-1Z-89, EKL-1, Intel HEX, režim bez okna, minimální GUI. **MikroMon naběhne a reaguje** | ✅ hotovo |
| F2 | MCP server (Streamable HTTP), okno terminálu, debugger (registry, disassembler, paměť, breakpointy, I/O log) | ✅ hotovo |
| F3 | ZRMD-1 + RTC, boot CP/M z RAM disku (ECMON) | ✅ hotovo |
| F4 | IDE-1 (8255, IDE `.img`) + PMD32-SD (protokol, `images.cfg`), ROM ECMON IDE, boot volbou 3 | ✅ hotovo |
| F5 | Sériová linka přes TCP a COM; klávesnice Consul 262.3, Consul 259.11, ANK-1; konfigurátor sestav a mapa portů | ✅ hotovo |
| F6 | ZPS-1 (JPR-1 8080, REM-1, AND-1, MIKROBASIC), ZPS-2 (RAM-1, DSM-1 s kazetou, MIKOS V4 a V5, BASIC EX/EXP, DGD-1), RPD-1 + MIKOS/D | ✅ hotovo |
| F7 | **ZPS-3:** JPR-1A, AND-1A, RAM-1A, CP/M 2.2/P z diskety přes RPD-1. 62K a FDC-1 (2026-10-05), originální IMA 64K a 48K (2026-10-06, 48K z MOVCPM48) | ✅ hotovo |
| F8 | **ZPS-4:** JPR-1Z, RAM-1Z, AND-1Z, RMD-1, CP/M 2.2/P (MOR62Z, s FDC-1 MOR62ZF) a ELIS | ✅ hotovo (originál Tesla IMA chybí) |
| F9 ✅ | **Liborovy desky V (hotovo):** JPR-1V (Z80, rychlejší, až 4 MHz), RAM-1V (silně konfigurovatelná, mapování MAP), AND-1V (VGA), DSM-1V (dva sériové kanály, bez kazety), ZRD-1V (zálohování tužkovými AAA, bez RTC a bez mapování). Podklady v `..\SAPI-karty\JPR-1V` atd. (PDF schémata) a na https://www.pmd85.cz/?page_id=292 | ✅ hotovo |
| F10 ✅ | **Diskové řadiče a systémy J. Bíby a EC1045** (2026-10-05/06): FDC-1 (8272 + DMA 8257), MFC-1A (zapojení Tesla i EC1045), diskety PC HD/DD a ImageDisk, motory mechanik; zdrojáky ECMON + CP/M (`cpmrun`); sestavy z disket SAPIMANa (8", dlouhé CP/M, EC1045), DPP-1 s PMD32-SD a tiskárnou, tiskárna na X3. Ověřeno originálními programy a výpisy z reálných MFC-1A (`docs/software/CPM_SAPIMAN.md`) | ✅ hotovo |
| F11 ✅ | **Desky Libora Lasoty k původním sestavám** (2026-10-06): klávesnice ANK-1M2 (joystick, myš), AND-1B (VGA), BGP-1S (grafika PMD-85, Colorace), JPR-1S (banky RAM, MIKROBIOS), JPR-1X (8085, TURBO), redukce Redukce34KB01 (BOOT po sériové lince); sestavy `jpr1s_bgp1s` (Manic Miner) a `zps/ZPS-1_JPR-1S_AND-1B`. Okno terminálu: posílání souborů, XMODEM tam i zpět, schránka. Kazeta z programu (HEX/BIN → MIKOS, MIKROBASIC, WAV) | ✅ hotovo |
| F12 ✅ | **SAPI-1.5** (2026-10-07): PKT-1, RSW-1 (8251, watchdog), DBS-1A (6845, font z dumpu EPROM „59“), sestava ZPS-6 s CP/M 2.2 62K z disket Melsoft (`.sp2`) | ✅ hotovo (bez ZPS-5) |
| F10+ | ~~PKT-1~~ (hotovo); ~~dokončení ZPS-3~~ (62K a FDC-1 hotové 2026-10-05, IMA 64K a 48K 2026-10-06); ~~ukládání stavu~~ (hotovo); ~~přepínání jazyka UI~~ (hotovo); zvuk bzučáku; ~~převod HEX → kazeta~~ (hotovo); CI (~~Linux~~ ve WSL hotovo) | |

Po každé fázi následuje commit, shrnutí, aktualizace tohoto souboru a zastavení, aby autor mohl výsledek ověřit.

## Na promyšlení (nemodelované věci a vylepšení)

- **AND-1V:** čekací stavy (RDY, jedno okno CPU na znak, 0–1 takt při 2 MHz, žádné při I/O).
- **JPR-1V:** STSTB bez refreshe (u JPR-1ZI je STSTB jen z /MREQ, což u AND-1Z může při refreshi s I = F8–FF
  spustit čekání). Visení RDY u DGD-1 během BOOT.
- **ZPS-4 / ELIS v3.00:** obsluha klávesnice (dvojité nebo nekonečné opakování kláves).
- **RMD-1:** časování (INIR, NOP po `OUT 43`).
- **Písmena mechanik podle hlášky po startu** (nápad autora 2026-10-06, zatím odložený): monitory J. Bíby a ECMON
  vypisují `A,B,D,E: FDC1…`, `B,C: MFC1`, `C:PMD32-SD/2M`. Emulátor by po resetu mohl číst obrazovku (u systémů
  jen s terminálem i sériovou linku) a přidělit písmena jednotkám popořadě, jako třetí zdroj mezi sestavou a
  odhadem ze SELDSK (zobrazit „podle hlášky“). Proti: jen jedna rodina systémů, pořadí u PMD32-SD je odvozené,
  obrazovka může odrolovat. Sestavy už písmena mají a SELDSK je dodá při prvním čtení.
- **CGA-1V:**
  - čekací stavy jsou přibližné (do konce okna CPU 318 ns);
  - prolínání snímků je náhrada za oko (REMBRAN2);
  - paleta a VRAM jsou po zapnutí černé, ne náhodné.

## Nejasnosti k ověření na reálném HW (sbírat a řešit dávkově)

1. **AND-1Z-89:** 20 nebo 24 řádků? (PROM a manuál říkají 20.) Délka řádku 107 nebo 108 znaků.
2. **DMR-1:** skutečné nastavení JP8 (baud rate) na autorově sestavě. Stav modemových bitů při odpojeném terminálu.
3. **Volná sběrnice:** čte se opravdu FFh?
4. **Consul 262.3:** polarita, délka strobe; má vaše JPR-1Z/ZI úpravu se 7474? (Kódy už jsou z manuálu.)
5. **ANK-1:** bity LED a bzučáku na P0-OUT (emulace podle MIKOS: D7 ERR, D6 bzučák, D5 SHIFT). Je piezo
   pasivní (sleduje D6, MIKROBASIC ho přepíná), nebo s oscilátorem?
6. **ZRMD-1:** ~~rozsahy S1–S4~~ (vyřešeno z GAL). ~~Jak MikroMon „zamyká“ ZRD-1V?~~ (rev 2.1: zápis na 60h
   déle než 20 ms po 62h, `docs/desky/ZRMD-1.md`). Nuluje /RES adresní čítače?
   Které bity 63h jdou na MAP1/MAP2?
7. **MikroMon 1.3:** posílá opravdu „sw i01“ a skenuje jen kanály 10, 14 a 18h?
8. **IDE-1:** stav IDE signálů po resetu, když je PC 8255 nastavený jako vstup.
9. **MikroMon bez USYNC:** ztrácí ozvěna na UART znaky i na skutečném HW? Emulace: po startu přijde „Z4kn 3“.
10. **MHB1012:** pořadí bitů délky slova (b3/b2) v řídicím registru.
11. **Reálná sestava CP/M:** je na DMR-1 připojený terminál (DCD), nebo se používá obrazovka? Kterou volbu
    0–3 v ECMON používáte (předpoklad: 3 = HDD)? Kterou EPROM máte v JPR-1ZI (ECMON s IDE-1)?
12. **IDE disk:** máte obraz (dump) skutečného DOM/CF z vaší sestavy? Emulátor zatím začíná s prázdným
    diskem vyplněným E5h.
13. **EKL-1 a čeština v MikroMonu:** ukáže MikroMon na skutečném HW po napsání `ž` (Alt-lock) velké `Ž`?
    Firmware EKL-1 posílá české znaky s prohozenou velikostí, ECMON ji vrací, MikroMon ne.
15. **JPR-1 a ZPS-1:**
    - Které RST dává 3214 pro kterou úroveň (předpoklad RST 7 − úroveň) a význam bitů zápisu na 3000h.
    - Co CPU čte z prázdných patic JPR-1 a REM-1 (emulace: FFh).
    - AND-1: skládání sloupců 32–63 do 1 KB VRAM (D4C), frekvence blikání, fotka obrazovky pro kontrolu
      překresleného fontu MHB 2501.
    - Kterou verzi MIKROBASICu má tvoje ZPS-1 a v jakém pořadí jsou karty ve slotech.
14. **Sestavy ZPS:** v jakém pořadí byly karty ve slotech u ZPS-1 až ZPS-4 (šablony jsou odhad)? Kolik pozic
    mají ARB-1A/Au (odhad 13) a ARB-1B (odhad 14, ARB-1C má X1–X14)? Mají všechny karty čelní panel se zkratkou
    (DMR, ZRMD, IDE…)? Panely jsou kreslené podle Liborových štítků. AND-1Z-89 má zatím panel AND-1V (K2 VGA),
    jak vypadá panel originálních karet a DMR-1, ZRMD-1, IDE-1?

16. **ZPS-2 a MIKOS:**
    - Byl MIKOS v ZPS-2 ve 4× 2708 (dump 1. KB tomu odpovídá), nebo ve 2× 2716? Emulace: 2708.
    - Dump PROM D1A z RAM-1 (emulace generuje obsah z tabulky 1).
    - Jsou v ZPS-2 opravdu rozpojené propojky přerušení (AND-1 49–50, JPR-1 10–11), jak radí manuál?
    - Bere MIKOS na skutečném HW klávesu stisknutou hned po vypsání otázky? (Emulace: ztratí se.)
    - MIKOS V5: kterou verzi monitoru (V5.0E nebo F) a kterou klávesnici máš? Byl v EPROM i ovladač XY 4130?

17. **DGD-1:** zapojení vstupů PROM D1A, okno CPU (čekací stavy), mapování. Máš desku nebo fotku grafiky?

18. **RPD-1 a MIKOS/D:** čítač 2 a OVFL, doba ustálení hlavy, 4 nebo 5 nul před datovou značkou, TOUT.
    Jak rychle se na skutečném stroji nahraje BAS5C.SYS (emulace 17 s)? Funguje LOAD bez přípony?
    Máš 8" mechaniku a disketu MIKOS/D?

19. **ZPS-3:**
    - Máš bootovatelnou originální systémovou disketu IMA CP/M 48K pro ZPS-3 (k MON V1.0-C1)? Emulace má
      rekonstrukci z originálního MOVCPM48 (46K), originál by sloužil jen k porovnání.
    - Ve které patici JPR-1A je boot EPROM (D3B nebo D4B)? Jaké propojky máš na JPR-1A, AND-1A a RAM-1A?
    - Jsou na AND-1A D3A vstupy A6/A7 (G/H) na +5 V?
    - Dumpy PROM D2C, D3A, D1A pro potvrzení přepsaných tabulek.
    - Má AND-1A 1 KB, nebo 2 KB video RAM?
    - Pro 62K a 64K: PROM D3A pro F800 a F000 a jak se dvě RAM-1A vyhnou displeji. Emulace 62K: druhá RAM-1A
      s upravenou D1A (S1 S2 = 10 → C000–F7FF) a domácí D3A pro F800; 64K: D1A C000–EFFF + F800–FFFF a D3A pro
      F000 (obojí předpoklad, `docs/software/CPM_ZPS-3.md`).

20. **ZPS-4** (odpovědi autora 2026-10-04):
    - **RMD-1 PROM:** bit 2 (O3) = port 40 (řada), bit 3 (O4) = 43 (příprava čtení)? Schéma má popisky
      obráceně. Autor desku nemá, zeptá se kamaráda. Emulace zatím: bit 2 řada, bit 3 příprava čtení.
    - **Originální boot EPROM Tesla IMA a systémová disketa ZPS-4:** autor je nemá, zeptá se po nich.
    - AND-1Z vstup A6 DA1: vyřešeno ze schématu, nastavuje ho přepínač S1 (`docs/desky/AND-1Z-89.md`).
    - Která ROM ELIS (v3.00/v7.01) patří k disketě: autor neví, vyzkoušet v emulátoru.
    - RMD-1 časové limity: autor neví.
    - Pořadí karet je celkem jedno. Klávesnice spíš **Consul 262.3** (ZPS-4 je novější).
    - **ELIS v3.00:** s Consulem 262.3 bere klávesy dvakrát (pulz STROBE) nebo donekonečna (7474). Jak
      monitor v3.00 čte klávesnici? (v7.01 funguje.)
    - **Pozorování autora (2026-10-04, GUI, ZPS-4):** jednou se systém zasekl a ve stavovém řádku přibývaly
      neobsloužené klávesy. Mohl to být i software. Zatím nevyhodnocovat, potřebuje to víc testování.

**Odpovědi autora k deskám V (2026-10-04)**, platí pro body 21 a 22:
- Do složek s podklady (`..\SAPI-karty\JPR-1V` atd.) doplnil **fotky hotových desek s výchozími propojkami** a do
  `..\SAPI-karty\ZRD-1V` **PDF schématu**.
- **JPR-1V:** 28C64 nemá, programuje si ho dynamicky (PicoROM). Rozhodnutí: **banka 0 = MikroBIOS 3.0,
  banka 1 = ECMON IDE-1**.
- **AND-1V:** rev 2.3, font neví. U něj je nastavená na **MAP1 = L** kvůli CGA-1V (deska přijde později).
- **RAM-1V:** rev 1.1, patice C prázdná. RTC Timekeeperu emulovat **volitelně** (parametr).
- **DSM-1V:** kanály na **10h a 14h, oba 115200 Bd**.
- **ECMON ani CP/M mapování nepoužívají.**
- **MikroBIOS (binárka) a fonty AND-1V smí do repa**, zdroje ne.
- Drobnosti (TURBO, oscilátor, bit 5 DSM-1V, polovina fontu rev 1.1) neví.

21. **Desky V – JPR-1V a AND-1V** (rozbor `docs/desky/JPR-1V.md`, `AND-1V.md`):
    - Který obraz EEPROM máš v JPR-1V a ve které bance (propojka BIOS osazená/rozpojená) je ECMON IDE-1 a
      MikroBIOS 3.0? Šel by dump 28C64?
    - Kterou revizi AND-1V má sestava V (fotky ukazují v1.1 i v2.3) a který font je vypálený (10×24 typ 1/typ 2,
      u rev 1.1 kterou sadu volí S1 9-10)?
    - Smí MikroBIOS 3.0 (HEX) a fonty AND-1V do `roms/`? Zdroje ne, jako u MikroMonu?
    - Lišta TURBO: tlačítko mezi piny 1 a 3 (pin 2 = LED)? Podle schématu dává 4 MHz pin 3 na zemi.
    - Oscilátor 8 MHz? Jede něco rychleji (10/5 MHz)?
    - Používá ECMON nebo CP/M na deskách V AND-1V mapovanou přes MAP1 (port a bit RAM-1V), nebo vždy F800
      s MAP1OFF?
    - Ověřit: s JPR-1V AND-1V při IN/OUT na porty F8–FF nečeká? Visí DGD-1 při BOOT opravdu?
    - Rev 1.1: která polovina fontu (A12 = 0/1) je standardní?

22. **Desky V – RAM-1V, DSM-1V, ZRD-1V** (rozbor `docs/desky/RAM-1V.md`, `DSM-1V.md`, `ZRMD-1.md`):
    - **ZRD-1V:** PDF schématu (vytisknout `ZRD-1V.sch` z Eagle jako u ostatních). Nuluje /RES čítače a
      registry adresy? Jsou 61h–63h opravdu nečitelné? Má hlídání MAX808 nějaký vliv viditelný pro software?
    - ZRD-1V: kterou revizi máš (fotka 1.1)? ~~Zámek zápisu a MikroMon~~: vyřešeno schématem rev 2.1, otevřené
      body jsou v `docs/desky/ZRMD-1.md` („Nejasnosti rev 2.1“).
    - RAM-1V: revize 1.1, nebo 1.2? Emulovat podle schématu 1.1 (1.2 nemá MAP2 u díry 16 KB)?
    - RAM-1V, patice C: je prázdná. Jaký Timekeeper se tam dává (M48T35/DS1644?) a emulovat jeho RTC?
    - DSM-1V: propojky na tvé desce (adresy A = 10h, B = 14h?, rychlosti obou kanálů).
    - DSM-1V b5 („START“): nemá funkci? Čte +3 FFh?
    - DGD-1/BGP-1S „paralelně s RAM-1V“: obě karty odpovídají na stejné adresy. BGP-1S se od 2026-10-06 hlásí jen
      pro zápis (nečte), takže kolize není; DGD-1 to dál potřebuje.

23. **CGA-1V** (`docs/desky/CGA-1V.md`):
    - `..\SAPI-karty\CGA-1V\RADICROM.bin` k desce nepatří. Je tam omylem?
    - Je k dispozici dokument „Programování karty“ a novější CGATEST, který má i test CGA (volba 1)?
    - Liborovy programy smí do repa jako binárky (autor), jsou v `media/cga-1v/`.

24. ~~**MPH-1V**~~: vyřešeno schématem hotové desky (`..\SAPI-karty\MPH-1V\Eagle`), viz `docs/desky/MPH-1V.md`.

25. **Bzučák** (`docs/desky/klavesnice.md`):
    - Mají Consul 262.3 a 259.11 bzučák na X2 pin 14 (P0-OUT1)? Emulace předpokládá stejný jako EKL-1.
    - Jaký tón má bzučák EKL-1 (SG1 F/CM12P)? Emulace: 2,4 kHz.
    - Pípne skutečný SAPI po resetu? Podle zapojení ano (3212 se nuluje, P0-OUT1 = 0, 25–50 ms do zápisu
      monitoru).

26. **FDC-1** (`docs/desky/FDC-1.md`):
    - Polarita D0 (DD/SD) přepínačů SA2 při čtení 0EH a k čemu je dvoupólový přepínač SA1 (u EC1045 nahrazený
      propojkou; SA2 má `11000011`, souměrně).
    - Hlásí 8272 po resetu „ready changed“ pro všechny 4 jednotky (SENSE INTERRUPT STATUS po resetu)?
    - Kolik taktů CPU zabere přenos DMA (emulace 4) a doba RQM po bajtu povelu (emulace 12 µs).
    - Registr 0FH: vedly /M0–/M3 z X3 k mechanikám PC? Nuluje ho /RES? Rozběh mechanik (emulace 500 ms)?
      EC1045 je nepoužíval, MOTOR ON měl z /HL (přímý kabel jako 8").
    - Kam až dojede hlava mechanik (8" Consul, 5,25" a 3,5" PC)? Emulace: stopa 82, programy SAPIMANa formátují 0–80.

27. **MFC-1A** (`docs/desky/MFC-1A.md`):
    - ~~Které zapojení registru 0FH mají vyrobené desky?~~ **Vyřešeno 2026-10-07:** originál v. č. 0124 odpovídá
      překreslení EC1045 (D5 reset, D4 INT, D6 = 1 zapnuté), schéma Tesla je chybné. Výchozí `wiring = ec1045`.
    - Bit resetu podle schématu drží 8272 v resetu. Fungoval BIOS ECBIM8 (ECMON varianta 2) s MFC-1A? Autor neví,
      desku zatím nemá, plánuje repliku. Oprava BIOSu: `RET` za SPECIFY v `INIT_MFC` (`0E 00` → `C9 00`), je ve
      zdrojáku `..\SAPI-karty\ZRMD-1\SRC\ECBIM8C.MAC` a v obrazu `..._fixMFC.bin`. Sestava s ní běží s resetem
      podle schématu, původní obraz jen s odchylkou `pulse`.
    - RDYn (XP2-19) v příkladech připojení mechanik v návodu zapojený není. Jaké mechaniky se připojovaly?
      Výpisy `SAPI15\MFC_TEST_CPM` (2010) ukazují stálé RDY a desku „Original MFC1A“, která zapínala mechaniky
      s D6 = 1 (jako překreslení EC1045). Kdo testy dělal a s jakým `DRVON`?

28. **Tiskárna** (`docs/tiskarna.md`):
    - Co je v kabelu na kolíku 16 X3 (P2-IN7)? ECMON při 1 netiskne, emulace ho drží v 0. U plotteru EC1045
      vede na BUSY (DSUB25/11); s ovladačem ECMONu by to zahazovalo znaky, pro tiskárnu dál nejasné.
    - Bere tiskárna data při sestupné, nebo náběžné hraně /STROBE (emulace: náběžná)?
    - DPP-1 kanál 1: děrovač, nebo tiskárna přes `UL1:`? Zapojení /OBF, /ACK, BUSY a přepínače S2–S4 pro tiskárnu
      (pro PMD32-SD vyřešeno 2026-10-07, `docs/desky/DPP-1.md`).

29. **Desky Libora Lasoty k původním sestavám** (2026-10-06; podrobně v sekcích „Nejasnosti“ `docs/desky/JPR-1S.md`,
    `BGP-1S.md`, `AND-1B.md`, `klavesnice.md`):
    - JPR-1S: aktuální propojky a BIOS v PicoROM (emulace: všechny banky, INT1, RST7, BIOS 1.02)? Odkud přichází
      /RTL (reset)? Převodník a program na straně PC pro BOOT?
    - BGP-1S: stav klopného obvodu přerušení po zapnutí; čte Manic Miner zpět videoram? Je `.sch` v repu poslední
      uložení (PNG a kusovník jsou novější)? Kdo napsal hru s autem?
    - AND-1B: je ve vyráběných deskách font `FontAND1B11`? Střída blikání 75 : 25, nerovnoměrný INT1 50 Hz; jsou
      v oběhu desky starší než rev 1.4 s jinými propojkami?
    - ANK-1M2: který program používá joyport (myš)? Má JPR-1 na P1-IN pull-upy?
    - JPR-1X: obsah ROM (dump obou bank), stav propojek BIOS/TURBO/RSTN/7, rezistor na D3, příznaky V/K a nedok.
      instrukce 8085 na skutečném čipu (`docs/desky/JPR-1X.md`, `docs/8085.md`).

30. **SAPI-1.5** (2026-10-07; podrobně v Nejasnostech `docs/desky/PKT-1.md`, `RSW-1.md`, `DBS-1A.md`):
    - PKT-1: polarita MAP1/MAP2/BOOT na sběrnici, /MR při čtení vnitřní EPROM, hodnota sběrnice v okně EPROM a při INTA,
      propojky XT4/XT5 na ZPS-5/6/7, BUSY tiskárny na PC3 nebo PC6, EKL-1 s /KACK = IBF.
    - RSW-1: skutečné propojky XT1 (emulace A6 = L, A7 = H, A8 = MAP2) a polarita MAP, obnovení watchdogu.
    - DBS-1A: pořadí R a G v atributu, blikání, horní poloviny PROM DD2/DD27, XT5/XT7, ~~dump znakového
      generátoru~~ (EC1045 2026-10-07; znak 98h je v něm kopií 86h), časování ROM BIOSu (15,0 kHz / 46,3 Hz).
    - ~~MFC-1A: byly desky Tesla zapojené jako překreslení EC1045?~~ Ano (v. č. 0124, EC1045 2026-10-07).
    - ZPS-5: jaký firmware měla? Podle EC1045 dvojitá vana, dlouhá sběrnice, bez MFC-1A, terminál nebo
      zobrazovadlo na míru.

32. **DSP-1** (2026-10-07, `docs/desky/DSP-1.md`): pořadí bitů A5/A6 na SA2, výstup 75154 bez kabelu (/CTS),
    nastavení desky v. č. 44127 z fotky.

31. **RTC-1** (2026-10-07, `docs/desky/RTC-1.md`): čtení PA v módu 1 s propojkami z výroby (/STB A nezapojený),
    kabel /INT z XC2/14 do počítače, sepnuté SA na skutečných deskách, XC2/29 (OUT2) jako vstup podle návodu.

33. **APS-5** (2026-10-08, `docs/desky/APS-5.md`, `ADP-1.md`, `DMT-1.md`): P2-IN7 = /PFAIL ze zdroje (odvozeno
    z firmwaru), kódy kláves HRK-1 proti překreslenému schématu, kabel RTC-1 XC2 → JPR-1 X3, /RTL z HRK-1B,
    doba převodu C 520 D a nevyužité bity stavu ADP-1. Logika 3214 (stav ~D & 7, D3 = vše) by šla ověřit na
    skutečné JPR-1 krátkým programem.

## Jak pokračovat

1. `git pull`.
2. Přečíst `CLAUDE.md`, tento soubor a `docs/rozhodnuti.md`.
3. Sestavit přes `tools\build.cmd debug test`. Na PC s pomalým síťovým diskem použít lokální preset `local-debug`
   (soubor `CMakeUserPresets.json` se necommituje, vzor je v `docs/build.md`). Na autorově hlavním PC je repo od
   2026-10-04 na lokálním `E:\SAPI_GIT\SAPIemu` (dřív `H:\SAPI-1\GIT\SAPIemu`), stačí preset `windows-debug`.
   Po přesunu je sestavení čisté (2026-10-06: 35 testů v debug, v release navíc ZEX a 8080EXM). Autor ověřil překlad debug i release
   ve VS 2022.
4. Pokračovat další fází z tabulky. **Hotovo (2026-10-04): F9 desky V včetně CGA-1V.** Původní poznámky k F9:
   Výsledek
   a otázky pro autora se zapíšou do `docs/desky/JPR-1V.md`, `RAM-1V.md`, `AND-1V.md`, `DSM-1V.md` a
   `ZRMD-1.md` (ZRD-1V). ZPS-1 až ZPS-4 běží (2026-10-04).
   - Pokud se rozbor ztratí, zopakovat ho:
     - schémata `..\SAPI-karty\JPR-1V\JPR-1V.pdf`, `..\SAPI-karty\AND-1V\AND-1V_sch.pdf` (+ `rev2.3\Popis-AND-1V-rev.-2.3.pdf`,
       `FontROM\`), `..\SAPI-karty\RAM-1V\RAM-1V.pdf` (+ `Popis-RAM-1V-rev.-1.1.pdf`), `..\SAPI-karty\DSM-1V\DSM-1V.pdf` (+ xlsx
       s rychlostmi);
     - `..\SAPI-karty\ZRD-1V\DESCRIPTION` (PDF chybí, Eagle je binární);
     - stránka https://www.pmd85.cz/?page_id=292.
   - Porovnat s emulovanými JPR-1ZI/1Z, AND-1Z-89, RAM-1/1A, DSM-1 a ZRMD-1. Výsledek a otázky zapsat do
     `docs/desky/`.
   - Autor: na deskách V běží systém ze ZRMD-1 (ECMON + CP/M) a funguje s nimi boot ROM i IDE-1. Mapování
     dělá RAM-1V, ZRD-1V ho nemá.
   - **Rozbor je hotový** (`docs/desky/JPR-1V.md`, `AND-1V.md`, `RAM-1V.md`, `DSM-1V.md`, `ZRMD-1.md`) a autor
     odpověděl (viz „Odpovědi autora k deskám V“ v nejasnostech).
   - **Hotovo:** JPR-1V (typ JPR-1ZI, banky EEPROM propojkou BIOS, osc 8 MHz, TURBO), `roms/jpr-1v` (banka 0
     MikroBIOS 3.0, banka 1 ECMON IDE-1), `roms/and-1v` (fonty). Ověřeno: obě banky naběhnou se ZRMD-1.
   - **Další krok:**
     - z fotek desek přečíst výchozí propojky (běží agent), z PDF ZRD-1V ověřit reset a čitelnost 61h–63h;
     - implementovat JPR-1V, AND-1V (rev 2.3), RAM-1V (s generátorem MAP a volitelným RTC v C), DSM-1V,
       opravit ZRD-1V (pryč `map_jp2`);
     - sestava V s ECMON a CP/M ze ZRD-1V a IDE-1.
   - Zdroje MikroBIOSu jsou v `H:\SAPI-1\LiborLasota\JPR-1V\@orig\JPR-1V_monitor.zip` (do repa jen HEX/BIN). Otevřené otázky ke ZPS-3/4 jsou v bodech
   19 a 20. Originální IMA CP/M 48K je rekonstruovaný (MOVCPM48), 62K a 64K jsou hotové. Podklady jsou v `docs/software/CPM_ZPS-3.md`.
