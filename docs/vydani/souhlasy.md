# Software ve vydání a souhlasy

Podklad pro žádosti o souhlas před veřejným vydáním (2026-10-17). Ke každému autorovi nebo zdroji je uvedené, co
ve vydání je, odkud to je a jaký souhlas už máme. Podrobnosti o souborech jsou v `roms/README.md` a
`media/README.md`.

Autor (Martin) rozhodl: historii repa neřešit, staré CP/M programy (i komerční, často dnes uvolněné) nechat,
**Prince of Persia** zůstává v emulátoru i ve veřejném vydání (autor 2026-10-28, po průzkumu webu): je to ukázka
historické technologie staré 40 let s upozorněním pro nositele práv (průvodce, popis sestavy, výběr sestav, README),
že se na jejich žádost cokoli upraví nebo odstraní. Jde o emulátor velmi specifického, většinou průmyslového
československého počítače, autor nečeká velké rozšíření ani dopady. Repo SAPI-Prince-of-Persia zůstane bez hry
v binární podobě (jen kód, informace a snímky) a autor ho zveřejní.

**Historie gitu se nepřepisuje** (autor): obrazy RAM disku s PoP v ní zůstanou (`media/zrmd-1/pop-ramdisk.bin` od
2026-10-10 verze z Apple, od 2026-10-28 verze DOS; smazaný `ramdisk_pop-fxs4-flappy-bomberman-bkp.bin` také). Průzkum,
jak to řeší jiné porty, postoj Jordana Mechnera, práva a doporučení: `docs/vydani/pop-prava.md`.

## Libor Lasota: souhlas pro veřejné vydání máme (2026-10-30)

**Souhlas v plném rozsahu** (odpověď na betu 0.7.0, 2026-10-30): „ohledně SW použij v plném rozsahu vše, co
potřebuješ“. Platí pro všechno níže i pro veřejné vydání. Dřív byl souhlas zapsaný jen u MikroMonu a MIKROBIOSu
(„použito s jeho souhlasem“, `roms/README.md`, `docs/rozhodnuti.md`). Emulátor vyzkouší asi o víkendu, až přejde
z Windows 7 (SAPIemu chce Windows 10 nebo 11). Otevřené zůstává jen, jestli pošle novější verze svého softwaru,
Jeho software ve vydání:

- MikroMon 1.3 (`roms/mikromon/`);
- MIKROBIOS 1.0, 1.02 a 2.1 (`roms/jpr-1s/`), MikroBIOS 3.0 v bance JPR-1V (`roms/jpr-1v/`), MIKROBIOS +
  MIKROBASIC pro JPR-1X (`roms/jpr-1x/`);
- fonty AND-1B a AND-1V (`roms/and-1b/`, `roms/and-1v/`);
- programy pro CGA-1V (`media/cga-1v/`: test, obrázky Rembrandt, hrad) a MPH-1V (`media/mph-1v/`: MPH-TEST,
  SPLAY, RAT PLAYER, Y1STDEMO);
- programy pro JPR-1S a BGP-1S, které poslal (`media/jpr-1s/`: hra „auto“ a další). Manic Miner je port ViLiSoft
  a DaJeSoft 1985 pro PMD-85, na SAPI-1 s BGP-1S ho převedl Libor (autor 2026-10-30).
- Flappy pro DGD-1V (FLAPDGD), SNDDEMON, BKP (verze pro SAPI, „-LAILA-“ = Libor), skladby VGM převedené pro SPLAY
  a moduly RAD pro RAT na RAM disku herní sestavy (`docs/vydani/sestavy.md`, oddíl „Co je Liborovo“).

## Petr Všijanský (EC1045, sapi.cz)

Byl mezi příjemci soukromých alf, výslovný souhlas pro veřejné vydání zapsaný není. **Autor 2026-10-30:** klíčový
byl souhlas Libora, Petrovy věci a programy na webu jsou většinou převzaté ze starých a historických zdrojů (Tesla
a další níže), jeho souhlas proto vydání nepodmiňuje. Poděkování zůstává první v pořadí. Co ve vydání je z jeho
zdrojů:

- výpisy a dumpy převzatými ze sapi.cz: MIKOS, MIKROBASIC, MOR*, ELIS, APS-5, DPS-1, PKT-1 a RSW-1, BASIMS,
  disketa MIKOS/D a další (`roms/`, `media/`, vždy se zdrojem v README);
- ECMON verze EC1045 (`roms/jpr-1zi/JPR-1ZI_ECMON_2k_EC1045.bin`), RAM disk `*_sysEC1045.bin`;
- dumpem znakového generátoru DBS-1A (`roms/dbs-1a/DBS-1A_CharGen_59.bin`);
- obrazy disket z archivu EC1045 (CP/M Tesla IMA 48K a 64K, diskety SAPI-1.5 Melsoft);
- uvedením jeho jména v poděkování (první v pořadí).

## Jiří Bíba (SAPIMAN)

Podle dosavadních zpráv není k zastižení (zapsáno). Ve vydání jsou jeho monitory a systémy CP/M (`roms/jpr-1zi/
JPR-1ZI_MON*_JB_*.bin`), diskety `media/sapiman/` a knihovna disket, programy RMP-1 a DSP-1. Navrhuji ponechat
s poděkováním a poznámkou, že jde o historický archiv. Kdyby se ozval, vyžádat souhlas dodatečně.

## Michal Medek

Byl mezi příjemci soukromých alf. Žádný jeho software ve vydání nenacházím. Stačí informovat o zveřejnění.

## Tesla a další historické zdroje (bez žijícího držitele práv, jen uvést)

- Tesla ELTOS IMA, DIZ, Liberec, Elstroj: MIKROBASIC, MIKOS V4/V5, MIKOS/D, MIKOS/K, monitory ZPS-3, APS-5, RKD-1,
  PKT-1, ovladače Minigrafu a XY 4130, BASIC EXP, CKM, BASIMS, výpisy PROM.
- R. Peřina (1988): CP/M MOR48A, MOR62A, MOR62Z a jejich diskety.
- ELIS (*JME/JPE), Melsoft (1990, Ing. Jan Meloun: od něj má autor sestavu SAPI-1.5 a diskety, poděkování v README a O programu), V. Julius OK1IVJ (plotter z AR 1990, přepsaný z výpisu v AR).
- Komerční software 80. let na disketách (Turbo Pascal, dBase II, WordStar, MBASIC, Pascal/MT+…): podle rozhodnutí
  autora ponechat.

## Autorovy porty a dema (Martin rozhodne sám)

| Program | Původní autor | Kde ve vydání | Poznámka |
|---|---|---|---|
| Bad Apple!! | hudba ZUN (Team Shanghai Alice), video fanouškovské; port Martin Lukášek | `media/zrmd-1/ba-ramdisk.bin` | repo SAPI-Bad-Apple má sekci Autorská práva |
| Fuxoft Soundtrack IV (FXS4) | František Fuka (Fuxoft), skladby různých autorů | RAM disk herní sestavy | port Martin, repo SAPI-FXS4 |
| Flappy | dB-SOFT 1984 (Sharp MZ-800) | RAM disk herní sestavy | port Martin (CGA-1V), FLAPDGD port Libor (DGD-1V) |
| Bomberman | Hudson Soft 1983 (Sharp MZ-700) | RAM disk herní sestavy | port Martin, repo SAPI-Bomberman |
| BomberNet | Martin Matyáš (MZPico), podle Bomber Man | **ano** (autor 2026-10-28: „Herní, i ve vydání“) | port 0.3.0, sestava `sapi1v_BomberNet` |
| Prince of Persia | Jordan Mechner, Broderbund (DOS verze Broderbund) | **ano**, s upozorněním pro nositele práv (autor 2026-10-28) | verze s grafikou a hudbou DOS |
| Hudba: 11 skladeb VGM z her (Hellfire, Tyrian, Monkey Island…) a 15 modulů RAD (3eality.com) | různí autoři | RAM disk herní sestavy | autor rozhodl: ponechat obojí |

## Postup

1. ~~Martin odsouhlasí rozřazení sestav (`docs/vydani/sestavy.md`) a obsah herní sestavy.~~ Hotovo 2026-10-17.
2. ~~Podle tohoto souboru napíše Liborovi a Petrovi s konkrétním seznamem.~~ Betu poslal 2026-10-30, Libor souhlasil
   v plném rozsahu týž den, Petrův souhlas vydání nepodmiňuje (autor).
3. Souhlasy se zapíšou sem a do `roms/README.md` a `media/README.md` (sloupec Zdroj), poděkování do README a okna
   O programu.
