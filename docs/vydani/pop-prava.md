# Prince of Persia: porty, práva a doporučení (průzkum webu 2026-10-28)

> **Rozhodnutí autora (2026-10-28, po tomto průzkumu):** PoP zůstává v SAPIemu i ve veřejném vydání jako ukázka
> historické technologie staré 40 let, s upozorněním, že se na žádost nositelů autorských práv cokoli upraví nebo
> odstraní. Historie gitu se nepřepisuje. Repo SAPI-Prince-of-Persia zůstane bez hry v binární podobě a autor ho
> zveřejní. Doporučení v oddílu 5 jsou tím zčásti překonaná (`docs/vydani/souhlasy.md`).

Průzkum webu je z 2026-10-28 a zdroje jsem otevíral přímo. Nejde o právní radu. Co nemám ověřené z primárního zdroje, je označené jako odhad, můj výklad nebo neověřeno.

**Ve zkratce**
- **Zásahy:** proti žádnému fanouškovskému portu PoP1 jsem nenašel veřejně známý zásah. Platí to od ZX Spectra (1996) přes C64 (2011) po letošní porty. Platí to i pro projekty, které na GitHubu otevřeně šíří grafiku a hudbu DOS verze (SDLPoP od 2015, PrinceJS, Roku). Je to ale tolerance, ne povolení.
- **Mechner:** porty veřejně chválí. Formálně ale dovoluje jen „studying, modifying, attempting to run“ svého kódu pro Apple II a šířit hry PoP smí podle něj jen Ubisoft.
- **Data DOS:** jeho prohlášení se na ně nevztahuje, dělal je tým Brøderbundu. Varianta s grafikou a hudbou DOS je proto formálně nejslabší.
- **Past v SAPIemu:** hotový RAM disk s PoP je v historii gitu SAPIemu, verze z Apple od 2026-10-10 a verze DOS od 2026-10-28. Když repo otevřeš i s historií, data PoP zveřejníš bez ohledu na to, co dáš do vydání.

## 1. Jak to řeší jiné porty

**Porty nejbližší tvému:**
- **Commodore 64** (Andreas Varga alias Mr. SID, 2011-10-16, verze 1.1 2011-11-05)
  - Logika je z výpisu paměti Apple II, disassemblovaného podle Mechnerovy dokumentace. Grafiku autor převedl z DOS verze (jeho blog, díly 1, 5 a 10). Je to tedy stejná kombinace jako u tebe.
  - Šíří se zdarma jako obraz cartridge EasyFlash na CSDb a je ke stažení dodnes.
  - Mechner ještě týž den napsal pod oznámení „That's crazy! … I couldn't get anyone interested in doing a C64 port“ a „I'm amazed and humbled by the amount of work this must have taken“. Jestli si všiml, že grafika je z DOS verze, nevím.
  - O povolení autor nepíše nic. Blog apl2bits port tehdy označil za „unauthorized and likely infringes on the original author's rights“, zároveň ale za „a gray area“.
  - Zásah jsem nenašel.
- **BBC Master** (Kieran Connell / Bitshifters, 2018-03-30)
  - Převedl celý Mechnerův zdroják. Sprity jsou z verze pro Apple, překreslené. Hudbu z DOS verze nově zaranžoval Inverse Phase.
  - Inverse Phase píše: „Once Jordan told me that the DOS version reflected his dad's original vision, I got to work…“. Mechner tedy o projektu věděl (můj výklad), licence to ale není.
  - Zdroják je na GitHubu od 2017 (kieranhj/pop-beeb), obraz diskety na webu Bitshifters. Bez licence a bez prohlášení o povolení, ale s původním „Copyright 1989 Jordan Mechner“.
  - Repo se naposledy měnilo 2026-09-09. Zásah jsem nenašel.
- **Atari XL/XE 128K** (rensoup a tým asi deseti lidí, 2021-10-24)
  - Vychází z portu pro BBC. Disk i cartridge jsou ke stažení na fóru AtariAge.
  - Ve vlákně (viděl jsem jen výňatky ve vyhledávači) uživatelé píšou, že zdarma ano, prodávat ne. Jeden dodává, že bez licence je to technicky nelegální tak jako tak.
  - Zásah jsem nenašel.
- **MSX2+ / V9990** (bokor, 2026-10-19, zdarma)
  - Autor píše „directly from the Apple II source code“, podle jiného uživatele je převedený pomocí AI.
  - Tvému portu se podobá nejvíc, je ale moc čerstvý na jakékoli závěry.

**Projekty, které šíří přímo data DOS verze:**
- **SDLPoP** (David Nagy, od 2015-05-02, GPL-3.0, asi 1 300 hvězd)
  - Přepis DOS verze podle disassembleru. Data DOS verze má v repu na GitHubu od prvního commitu.
  - Hotová sestavení s daty jsou na popot.org. Tam je i celá původní DOS hra (verze 1.0–1.4) a její hudba v MP3, FLAC a OGG.
  - README řeší jen licenci programu, o datech mlčí.
  - RPM Fusion ho kvůli datům dává do „nonfree“ („It is a reverse engineered version of PoP including original graphics“). Guix a Homebrew ho balí i s daty bez komentáře.
  - Verze pro Dreamcast, Xbox, RP2350 a Recalbox data přibalují. Verze pro PicoCalc (2026-09) je naopak vědomě bez dat: „You must obtain the original game data yourself“ a „source-code licenses do not grant rights to the original game data“.
  - Za 11 let jsem žádný zásah nenašel.
- **PrinceJS** (Oliver Klemenz, 2021, hraje se na princejs.com)
  - Obsahuje sprity DOS, titulní obrazovku DOS i s „©Copyright 1990 Jordan Mechner“ a 22 skladeb z DOS verze v MP3.
  - Kód je pod Unlicense, bez výjimky pro data.
  - Měl velký ohlas v médiích (Gizmodo 2022-01-12). Zásah jsem nenašel.
- **Roku** (Marcelo Lv Cabral, od 2016, sprity DOS a Mac, hudba v repu)
  - Mechner o něm ve své knize The Making of Prince of Persia (2020) otiskl celou stránku.
  - README zakazuje komerční použití i obchody s aplikacemi.
  - Pro srovnání: stejnému autorovi v roce 2017 Nintendo přes DMCA zrušilo Donkey Konga pro Roku, kdežto repo s PoP běží od 2016.

**Další porty:** Wikipedie uvádí ještě ZX Spectrum (1996), BK-0011M, HP48, TI-89/92, Enterprise 128, Plus/4 (2024), VIC-20 (2024) a další. U žádného neuvádí reakci Mechnera, Ubisoftu ani Brøderbundu. Výjimkou je MSX2 port Davida Heremanse (2026): zatím vyšel jen fyzicky a cartridge se prodávala. O reakci zatím nevím.

**Co mají porty společné** (pozorování):
- jsou zdarma, neprodávají se a nejsou v obchodech s aplikacemi;
- uvádějí Mechnera a nechávají původní copyrighty;
- licenci dávají jen na vlastní kód, ti opatrnější výslovně píšou, že ne na data;
- část projektů data vůbec nedává.

## 2. Postoj Jordana Mechnera

**Formálně** platí README jeho repa se zdrojáky pro Apple II. Znění je stejné od 2012-04-17 a při úpravě v září 2024 ho nechal:

> „As the author and copyright holder of this source code, I personally have no problem with anyone studying it, modifying it, attempting to run it, etc. Please understand that this does NOT constitute a grant of rights of any kind in Prince of Persia, which is an ongoing Ubisoft game franchise. Ubisoft alone has the right to make and distribute Prince of Persia games.“

- LICENSE jen odkazuje na README. Otevřená licence neexistuje.
- Právní dotazy posílá „to the community at large“.
- Materiály v knihovně jeho webu jsou „All rights reserved“.

**Na co se prohlášení vztahuje:**
- Výslovně jen na „this source code“, tedy kód pro Apple II.
- Že platí i pro data Apple v repu (obrázky IMG.*, úrovně LEVEL0–14), je rozumný výklad, doslova to tam ale není.
- Grafika a hudba DOS verze v repu nejsou a prohlášení se jich netýká.

**Osobně je portům nakloněný:**
- 2011: komentáře k portu pro C64 (viz výše).
- 2012-03-29 na blogu zmínil „the hardy soul who ported POP to the Commodore 64 from an Apple II memory dump“.
- 2020: stránka o portu pro Roku v jeho knize.
- 2025-09-25 v blogu „A platform-jumping prince“: „an unofficial, fan-made C64 port was finally done in 2011, over 20 years later, and a Commodore Plus/4 port just last year. I hope my Apple II source code was helpful.“ U portu pro Plus/4 odkazuje přímo na stránku ke stažení.
- Na stránce o PoP píše, že hra je „long out of print and not currently available for purchase“, a radí ji hrát „for free in your browser and under emulation at the Internet Archive. (Don't tell anyone I told you.)“. V roce 2025 doporučil hrát právě DOS verzi.

**K DOS verzi:**
- Udělal ji interní tým Brøderbundu: program Lance Groody, grafika Leila Joslyn, zvuk Tom Rettig a Mechner jako režisér.
- Hudbu pro MIDI přearanžoval jeho otec Francis Mechner.
- V roce 2012 napsal „I don't have the PC source code“.

**Shrnutí (můj výklad):** osobně je pro. Formálně ale pustil jen studium, úpravy a spouštění kódu, šíření her nechává Ubisoftu. „Don't tell anyone I told you“ naznačuje, že ví, že neoficiální šíření není schválené.

## 3. Kdo drží práva dnes a jestli zasahují

**Ochranná známka:**
- V USA ji vlastní Waterwheel Licensing LLC (registrace č. 7149670 z 2023-08-29, podle výpisu z dat USPTO na Trademarkia).
- Ubisoft ve svých hrách uvádí „Prince of Persia is a trademark of Waterwheel Licensing LLC … used under license“.
- Že Waterwheel patří Mechnerovi, je dobře podložený odhad. V roce 2010 na ni přešly jeho registrace a drží i známku KARATEKA. Výslovně to ale nikde nestojí.
- Registrace v EU jsem neověřoval.

**Ubisoft:**
- Má výhradní celosvětovou licenci na počítačové a videohry, včetně práva vymáhat známku vlastním jménem. Vyplývá to z nesporných tvrzení ve sporu o doménu princeofpersia.com (2010-01-28).
- Mechner to v roce 2003 popsal jako „the exclusive worldwide license to develop and publish Prince of Persia video games“.
- Ubisoft se k PoP dostal v roce 2001, kdy koupil zábavní divizi The Learning Company. Řetězec vlastníků Brøderbundu: The Learning Company (1998) → Mattel (1999) → Gores (2000) → zábavní divize Ubisoftu (2001).

**Autorská práva:**
- Ke kódu pro Apple II je má Mechner, sám se tak označuje.
- U DOS verze uvádějí titulní obrazovka a manuál z roku 1992 „©Copyright … Jordan Mechner“.
- Manuál DOS verze z roku 1990 má ale navíc „©Copyright 1989, 1990 Brøderbund Software, Inc.“ a manuály z let 1992 a 1995 uvádějí společný copyright Brøderbundu a Mechnera. Smlouvy veřejné nejsou.
- Podíl Brøderbundu na grafice a hudbě DOS nejspíš přešel řetězcem k Ubisoftu (odhad, neověřeno). U dat DOS tak můžou být dva držitelé práv.
- Kdyby někdo zasáhl, čekal bych spíš Ubisoft než Mechnera (můj výklad).

**Zásahy:**
- Ve veřejném archivu výzev DMCA na GitHubu (github/dmca, stav k 2026-10-27, včetně smazaných) není nic o PoP, Mechnerovi, SDLPoP ani Brøderbundu.
- Ubisoft tam má šest výzev a všechny se týkají jeho kódu nebo her, které tehdy prodával či provozoval (můj výklad):
  - skripty Far Cry 3 (2014);
  - klony Rocksmithu (2016);
  - jeho kód v knihovně behaviac (2016);
  - soukromý server Growtopie (2020);
  - uniklé zdrojáky rozpracované hry (2022);
  - nástroj Slopsmith k Rocksmith 2014 (2026-06-15). GitHub tehdy zablokoval celou síť 131 repozitářů včetně forků.
- Jediné veřejné pravidlo Ubisoftu k jeho datům je Video Policy (aktualizace 01/2016). Uvádí „Do not take assets from our games (e.g. voice, music, items) and distribute them separately“ a „this Policy is not meant to allow you to upload our full game, or even a majority of it“. Týká se videí a fanouškovské hry ani porty nezmiňuje. Zásady pro fanouškovské hry jsem u Ubisoftu nenašel.
- Meze průzkumu: vidět jsou jen veřejné výzvy na GitHubu. Soukromé dopisy ani výzvy jiným hostingům vidět nejsou a databázi Lumen jsem otevřít nemohl. Že jsem nic nenašel, neznamená povolení.

## 4. Právní rámec v ČR/EU stručně

Vycházím z autorského zákona 121/2000 Sb. (znění od 2025-07-01) a trestního zákoníku 40/2009 Sb.

- **Co je chráněné:** počítačový program je dílo (§ 2 odst. 2, § 65). Grafika a hudba hry jsou samostatná díla výtvarná a hudební (§ 2 odst. 1).
- **Převod 6502 → Z80** je zpracování nebo překlad. Vzniká tím tvoje dílo, ale „tím není dotčeno právo autora zpracovaného nebo přeloženého díla“ (§ 2 odst. 4). Bez svolení smí dílo užít jiný jen tam, kde to stanoví zákon (§ 12 odst. 1).
- **Doba ochrany:** život autora a 70 let po jeho smrti (§ 27).
  - Výjimku pro „abandonware“, neprodávané hry ani fanouškovské porty zákon nezná (podle mého čtení).
  - Že je port zdarma, snižuje pravděpodobnost i rozsah sporu, legální ho to ale nedělá.
- **Osobní potřeba:** u počítačových programů se za užití počítá i užití pro osobní potřebu (§ 30 odst. 3). Ani model „každý si data vyrobí ze své kopie“ tedy není úplně čistý. Riziko ale nese uživatel a je zanedbatelné (odhad).
- **Následky:**
  - Občanskoprávně jde o zákaz zásahu a odstranění následků (§ 40 odst. 1), dále náhradu škody a bezdůvodné obohacení. Při užití bez licence se obohacení počítá jako dvojnásobek obvyklé odměny (§ 40 odst. 4).
  - Trestný je jen zásah „nikoli nepatrný“ (§ 270 odst. 1 TZ, až dva roky).
  - U bezplatného fanouškovského projektu je reálným prvním krokem výzva k odstranění (odhad).
- **EU:** autorské právo je sjednocené směrnicemi a principy jsou stejné. Je to obecná znalost, texty směrnic se mi tentokrát otevřít nepodařilo.
- **GitHub** jako americký hosting jedná podle DMCA a výzvu umí uplatnit na celou síť forků.
- **Název hry** je ochranná známka. U nekomerčního portu je hlavní otázka autorskoprávní, jen port nevydávej za oficiální. To je moje doporučení, známkové právo EU jsem neověřoval.

## 5. Doporučení pro SAPIemu a SAPI-Prince-of-Persia

**Jak to máš dnes** (ověřil jsem v gitu):
- **SAPI-Prince-of-Persia** už je varianta (a).
  - ..\SAPI-Prince-of-Persia\.gitignore vylučuje originální data.
  - V žádné větvi ani v historii nejsou soubory DAT, obrazy disket ani hotové RAM disky, jen kód, nástroje, dokumentace a 4 snímky obrazovky.
  - README má oddíl Autorská práva, soubor LICENSE chybí.
- **SAPIemu** má data PoP v gitu.
  - media\zrmd-1\pop-ramdisk.bin (obsahuje POP.DAT o velikosti 3,2 MB) je v gitu od 2026-10-10 ve verzi z Apple a od 2026-10-28 ve verzi DOS.
  - Smazaný `ramdisk_pop-fxs4-flappy-bomberman-bkp.bin` (PoP 1.0.0) v historii zůstal.
  - Rozhodnutí „historii repa neřešit“ v docs\vydani\souhlasy.md by proto znamenalo zveřejnit data PoP.

**Varianty** (míra rizika je můj odhad):

| Varianta | Riziko | Hlavní důvod |
|---|---|---|
| (a) jen kód, data dodá uživatel | nejnižší | odpovídá Mechnerovu „studying, modifying, attempting to run“, stejně to dělá port pro PicoCalc |
| (b) port z Apple s daty Apple | nízké | data jsou z velké části přímo v Mechnerově repu a podobných portů je od 2011 hodně bez zásahu |
| (c) grafika a hudba DOS | formálně nejvyšší, prakticky nízké | Mechnerovo prohlášení je nepokrývá a podíl Brøderbundu má nejspíš Ubisoft; přesto se to toleruje (SDLPoP, PrinceJS, Roku) |

Co k variantám ještě patří:
- **(a):** hru podle Mechnera nejde koupit, takže legální vlastní kopii má málokdo.
- **(b):** šíření hratelné hry je formálně vyhrazené Ubisoftu. Obrázky úvodu a hudbu navíc tvůj build bere z obrazu originální diskety.
- **(c):** převod do CGA ve 4 barvách ani hraní MIDI na YM3812 na tom podle mě nic nemění, pořád jde o tatáž díla.

**Soukromá beta (teď):** je v pořádku tak, jak je. Okruh testerů je uzavřený, repo soukromé a nic se neprodává. Do machines\guides\sapi1v_PoP.md nebo do poznámek k betě bych přidal jednu větu: „Neoficiální nekomerční port; Prince of Persia © Jordan Mechner, DOS verze Brøderbund, práva ke hrám má Ubisoft; na data PoP se licence SAPIemu nevztahuje; prosím nešířit dál.“

**Veřejné vydání doporučuju takhle:**
1. **SAPIemu vydat bez dat PoP.** Sestava, náhled a návod můžou zůstat s odkazem, kde RAM disk vzít nebo jak si ho vyrobit. Případná výzva pak zasáhne jen PoP, ne celý emulátor se souhlasy Libora a Petra.
2. **Před otevřením repa SAPIemu odstranit z historie oba RAM disky PoP**, a to všechny jejich verze (např. `git filter-repo --invert-paths --path …` v čerstvém klonu). Změní se tím hashe commitů a bude potřeba force push. Zbytek historie můžeš dál neřešit.
3. **SAPI-Prince-of-Persia zveřejnit tak, jak je, tedy jen kód.**
   - Na převedený Mechnerův kód nedávej GPL ani jinou otevřenou licenci. Nemůžeš udělit práva, která sám nemáš (můj názor).
   - Licenci můžeš dát jen na vlastní nástroje v `tools/`.
   - Do README doplň větu jako u PicoCalc, že licence kódu se netýká dat hry.
4. **Napsat Mechnerovi** a podle odpovědi:
   - **Nebude proti:** hotový RAM disk verze z Apple dát do Releases repa PoP, ne do SAPIemu. Verzi DOS nechat jen na vlastní sestavení, nebo ji vydat hotovou, pokud výslovně souhlasí i s ní. Ani jeho souhlas formálně nepokryje podíl Brøderbundu/Ubisoftu, prakticky to ale odpovídá SDLPoP a PrinceJS.
   - **Neodpoví:** (b) vydat podle precedentů, (c) jen na vlastní sestavení.
   - **Bude proti:** zůstat u (a).
5. **Kdyby přišla výzva k odstranění, data hned stáhnout.** Obvykle to tím skončí (odhad).

Další možnosti:
- **Vydat všechno včetně RAM disku DOS jako SDLPoP.** Podle 15 let precedentů je to málo rizikové, ale i tak bych to dal mimo repo SAPIemu.
- **PoP veřejně nevydat vůbec.** Je to nejbezpečnější, ale přijdeš o nejlepší ukázku.

**Zeptat se Mechnera?** Ano, jen s realistickým očekáváním.
- **Co čekat:** formální povolení nedá, sám píše, že šířit hry smí jen Ubisoft. Přátelské „nemám námitek“ ale má cenu a může ti poradit, jestli se ptát Ubisoftu.
- **Kudy:** přes kontaktní formulář na jordanmechner.com/en/about/ (nabízí typy zpráv „Press/Speaking Inquiries“ a „Website Related“), nebo přes Bluesky či Mastodon (@jmechner). Píše, že se snaží číst co nejvíc, ale neodpovídá na všechno.
- **Co neposílat:** podle jeho FAQ „For legal and logistical reasons, I can't receive or read unpublished work“. Míní tím hlavně nápady, ale pro jistotu neposílej soubory ani přístup do soukromého repa, jen odkaz na veřejné video ze hry.
- **Co napsat** (anglicky, krátce):
  - co je SAPI-1 a SAPIemu;
  - že jsi logiku hry strojově převedl z jeho zdrojáků 6502 na Z80. Jeho názor na převody pomocí AI jsem nehledal, takže bych to řekl rovnou;
  - že máš dvě varianty, s daty Apple a s grafikou a hudbou DOS;
  - že je to zdarma, nic se neprodává, zdroják je bez dat a autoři jsou uvedení;
  - otázky: Vadí mu veřejné vydání? Je pro něj rozdíl mezi daty Apple a DOS? Máš se ptát Ubisoftu? Jak chce být uveden?
- **Odpověď** si zapiš do docs\vydani\souhlasy.md.
- **Ubisoftu bych se sám neptal** (můj názor). U žádného nalezeného projektu jsem povolení od Ubisoftu nenašel. Formální dotaz spíš skončí mlčením nebo odmítnutím, a po odmítnutí by se pokračovalo hůř. Zeptal bych se jen, kdyby to doporučil Mechner.


## 6. Zdroje

**Mechner:**
- https://github.com/jmechner/Prince-of-Persia-Apple-II (README, LICENSE)
- https://www.jordanmechner.com/en/latest-news/#a-platform-jumping-prince (2025-09-25)
- https://www.jordanmechner.com/en/archive/ (2012-03-29, komentář 2012-05-03)
- https://www.jordanmechner.com/en/games-movies/prince-of-persia/
- https://www.jordanmechner.com/en/library
- https://www.jordanmechner.com/en/about/

**Porty:**
- https://popc64.blogspot.com/2011/10/prince-of-persia-for-commodore-64128.html
- https://popc64.blogspot.com/2011/10/part-five-painting-pretty-dungeon.html
- https://csdb.dk/release/?id=102248
- https://www.engadget.com/2011-10-17-prince-of-persia-leaps-to-commodore-64.html
- https://www.apl2bits.net/2011/10/17/prince-of-persia-64/
- https://github.com/kieranhj/pop-beeb
- https://bitshifters.github.io/posts/prods/bs-pop-beeb.html
- https://inversephase.bandcamp.com/album/prince-of-persia-un-official-bbcost
- https://lunduke.substack.com/p/prince-of-persia-ported-to-the-atari
- https://forums.atariage.com/topic/298914-unicorns-season-prince-of-persia-for-the-a8/
- https://www.msx.org/nl/node/67512
- https://retrovault.ch/news/prince-of-persia-msx2-portage-heremans-2026
- https://github.com/NagyD/SDLPoP
- https://www.popot.org/get_the_games.php?game=SDLPoP
- https://www.popot.org/get_the_games.php?game=1
- http://lists.rpmfusion.org/archives/list/rpmfusion-developers%40lists.rpmfusion.org/thread/MYWXCCCAWIF46J6VJB5MI54EGCPV6OJS/
- https://github.com/kwi-software/picocalc_sdlpop
- https://github.com/oklemenz/PrinceJS
- https://gizmodo.com/you-can-play-the-original-prince-of-persia-on-your-appl-1848344033
- https://github.com/lvcabral/Prince-of-Persia-Roku
- https://github.com/github/dmca/blob/master/2017/2017-06-22-Nintendo.md
- https://en.wikipedia.org/wiki/Prince_of_Persia_(1989_video_game)
- http://plus4world.powweb.com/software/Prince_of_Persia

**Práva a zásahy:**
- https://www.adrforum.com/domaindecisions/1298869.htm
- https://www.worthplaying.com/article/2003/3/25/interviews/9037-prince-of-persia-the-sands-of-time-interview/
- https://store.steampowered.com/app/2751000
- https://www.trademarkia.com/prince-of-persia-97510236
- https://gamesfirst.com/articles/2001/ubi-soft-acquires-the-learning-companys-entertainment-division/
- https://archive.org/details/prince-of-persia_202506 (manuál DOS 1990)
- https://archive.org/details/prince-of-persia-cd-collection-manual (1995)
- https://github.com/github/dmca
- https://github.com/github/dmca/blob/master/2026/06/2026-06-15-ubisoft.md
- https://www.ubisoft.com/en-us/videopolicy.html

**Zákony:**
- https://www.zakonyprolidi.cz/cs/2000-121
- https://www.zakonyprolidi.cz/cs/2009-40

**Lokálně:**
- git log repa SAPIemu
- ..\SAPI-Prince-of-Persia\README.md a ..\SAPI-Prince-of-Persia\.gitignore