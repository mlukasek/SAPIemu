# Hodnocení SAPIemu: přehlednost, chování, vzhled, přístupnost

> Hodnocení z 2026-10-28 (verze 0.7.0-alpha, debug build). Na autorovu žádost („nadhled“, obava z molochu a z
> „AI slop“) ho připravilo 7 nezávislých pohledů, 3 oponenti a souhrn. Snímky, na které odkazuje, jsou jen na
> autorově PC v `out\ux-review-20261028\` (mimo git); znovu je vyrobí `out\ux-review-20261028\ux_gallery.py`.

> **Podklady.** Vycházel jsem z 26 snímků galerie z čisté instalace a z dalších snímků, které pořídili recenzenti. Program posuzovalo 7 recenzí z různých pohledů: nováček, informační architektura, vzhled, přístupnost, chování, vnímání „AI slop“ a srovnání s jinými emulátory. Tři oponenti potom každé zjištění znovu ověřili ve zdrojácích a na snímcích. Použil jsem jen zjištění, která oponenti ponechali nebo upravili. Sloučená zjištění uvádím jednou.
>
> **Značení.** [11] je snímek galerie `11_…png`, [new_02] je extra snímek, `soubor:řádek` je místo ve zdrojáku. Plné cesty ke snímkům jsou v Podkladech na konci.
>
> **Omezení.** Snímky vznikají ve skrytém okně, proto stavový řádek ukazuje „emulace -- %“. Ve skutečném běhu je tam rychlost. Všechny snímky jsou při škálování 100 %.

## 1. Celkový dojem

Proti světovým standardům stojí SAPIemu rozpolceně. Obsahem je nad úrovní běžných hobby emulátorů a v několika věcech nemá obdobu: kreslený rám ARB-1 s čelními panely a propojkami, přístroje (APS-5 s pecemi, pracoviště IMS-2, Minigraf), okno Vybrat sestavu s náhledy, průvodci s tlačítkem „Napsat do SAPI“ a ověřování desek V na skutečném SAPI-1 V logickým analyzátorem. Nic z toho MAME, 86Box ani openMSX v téhle podobě nemají.

Povrchem ale program působí jako vývojářský nástroj. Má nezměněný styl Dear ImGui s průsvitnými okny, přes která prosvítá text, ovládací prvky s kontrastem 1,41:1 a okna, která se při prvním spuštění rozmístí podle pevných pixelů. Chybí i to, co lidé od emulátoru čekají v první minutě: celá obrazovka, obraz, který roste s oknem, a přetažení souboru.

Obava z molochu je oprávněná napůl. Rozsah je opravdu velký: 60 sestav, z toho jen 17 s názvem pro nováčky, 18 nástrojových oken a README o 2 642 slovech. Cesta pro nováčka je ale promyšlená. Dojem molochu dělají hlavně výchozí hodnoty, struktura menu a prezentace, ne koncepce. Skoro všechno jde spravit malou prací a bez ústupku věrnosti hardwaru.

Nováček má prvních šedesát sekund dobrých. Otevře se Vybrat sestavu, za ním už běží ZPS-1 a s průvodcem napíše program v BASICu do minuty. Pak si vybere to nejlákavější, Hry a dema nebo Prince of Persia, a narazí:
- Ve výchozím okně 1100×760 hru nevidí, protože okno CGA-1V se otevře pod spodním okrajem [new_02].
- Menu ECMONu mu česky nabídne volby 0–2, na kterých stroj čeká navždy.
- Boot hlásí „PMD32-SD nenalezen“.
- Klávesy po kliknutí na tlačítko v průvodci mizí.
- Při ukončení se ho program ptá na uložení konfigurátoru, který nikdy neotevřel.

Tady dnes cizí člověk odchází. Tvoji kamarádi tyhle díry přeskočí, protože vědí, co dělat, a jejich chvála je proto neodhalí.

## 2. Co je dobré a stojí za to zachovat

- **Vybrat sestavu jako vstupní brána.** Okno se otevře při prvním startu a nabízí:
  - kategorie, skutečné náhledy a krátký text česky i anglicky;
  - hledání a poslední sestavy;
  - spuštění dvojklikem a běžící sestavu zeleně [01] [02] [26].

  Za oknem už běží ZPS-1 s MIKROBASICem (`app.cpp:1443-1454`). Je to na úrovni nového správce strojů 86Boxu a bohatší než výběr v openMSX. Skupina „Ostatní“ se 43 variantami je sbalená, a to je správně.
- **Průvodci sestavou.** Mají očíslované kroky, bloky kódu s tlačítky „Napsat do SAPI“ a „Kopírovat“ a odkazy. Jsou pro 16 sestav česky i anglicky [04] [10] (`browser.cpp:314-343`). Žádný ze srovnávaných emulátorů nemá u každého stroje přímo v programu návod, co vyzkoušet.
- **Kreslený hardware je tvoje identita.** Patří sem:
  - rám s čelními panely, konektory a LED [11–13];
  - IMS-2 se sedmisegmentovým M1T 330, ručkovými měřidly, knoflíky a kabely [21];
  - APS-5 s klávesnicí, klíčkem, pecemi a grafem [20];
  - deska K3 [23], papír Minigrafu [22] a panel RMP-1 (`media.cpp:1190-1235`).

  Je to nejsilnější vizuální argument proti „AI slop“.
- **Konfigurátor.**
  - Drží každou propojku.
  - Karty jdou přetáhnout; přetažení dává zelenou nebo červenou odezvu a Esc ho zruší.
  - Změny čekající na restart jsou oranžové, s tooltipem a připomínkou ve stavovém řádku.
  - Mapa portů a paměti ukazuje kolize pro každý stav MAP (`rack.cpp:1118-1120, 1188-1252, 1458-1466`; `app.cpp:1126-1136`).

  Tooltipy tlačítek ZDR vysvětlují anglicky české nápisy na panelu (`rack.cpp:1326-1335`). To je vzor pro ostatní přístroje.
- **Pixelově přesný obraz.** Používá nearest neighbour, celočíselné zvětšení a přichycení k mřížce pixelů (`app.cpp:1170-1192`). Text v [05], [14] a [25] je ostrý.
- **Promyšlené zkratky.**
  - Ctrl+písmena zůstávají SAPI, emulátor má F-klávesy.
  - Ctrl+Shift+V vkládá, terminál má Alt+C/Alt+V jako TeraTerm a Ctrl+F10 uvolní myš jako v DOSBoxu.
  - Při psaní do textového pole se zkratky vypnou (`app.cpp:415`).
  - Všechno je v jedné tabulce [19].
- **Stav je vidět.** Herní režim, joystick z klávesnice, zachycená myš a REC ukazují i klávesu, která je vypne (`app.cpp:1093-1115`). Myš se při ztrátě fokusu uvolní a držené klávesy se pustí.
- **Robustnost.**
  - Nastavení se ukládá každé 3 s a poškozený soubor nastavení se ignoruje.
  - Okno se obnoví jen na připojeném monitoru.
  - Okna mají stálá `###id` s migrací `imgui.ini`.
  - Výchozí rozložení oken slouží jako záchranná brzda (`app.cpp:266-312, 1381-1383, 1708-1711`).
- **Ochrana médií.** Program pracuje s pracovními kopiemi a nabízí „Vysunout bez uložení“. „Obnovit z originálu“ se zeptá a ukáže obě cesty (`media.cpp:179-257`). Dialog nahrávání ukáže rozsah adres, velikost i přesný příkaz `SAVE n JMENO.COM` (`media.cpp:664-669, 686-689`).
- **Laskavé chyby při startu.** Spuštění ze zipu poradí rozbalit celý archiv (`app.cpp:1331-1335`). SmartScreen je vysvětlený v `docs/prvni-kroky.md:9-10`. Když se sestava nenačte, spustí se MikroMon.
- **Jazyky.**
  - První start se řídí jazykem Windows.
  - „Jazyk / Language“ je dvojjazyčně v menu i v Nastavení.
  - Jazyk se přepne za běhu a rozložení oken zůstane.
  - Všech 536 klíčů `tr()` je přeložených.
- **Čitelnost textu splňuje WCAG 2.2 AA.** Tělo textu má 19,2:1, tlačítka 9,6:1 a popisky přístrojů 7–11:1. UI nebliká a kontrolky přístrojů se liší jasem, takže je přečtou i barvoslepí.
- **Důvěra.** MCP je v nové instalaci vypnutý a poslouchá jen na 127.0.0.1. Program nemá telemetrii a README má jasnou sekci „Síť a soukromí“ (README:209-225).
- **Poctivá dokumentace.**
  - Nikde netvrdí „cycle-exact“ ani „100 %“.
  - Otevřené otázky jsou zveřejněné (`docs/hw-otazky.md`).
  - `docs/zmeny-podle-hw.md` obsahuje 11 datovaných změn podle měření na skutečném SAPI-1 V.
  - V textu nejsou typické fráze jazykových modelů; recenzent je cíleně hledal.
  - Jádra CPU jsou z ověřených knihoven a procházejí testy ZEXDOC, ZEXALL, 8080EXM a 8085EXM. Před vydáním běží `check_machines.py`.

## 3. Top 10 doporučení

Pořadí je podle dopadu a pracnosti. Body 1–6 a S části bodů 7 a 8 patří do bety. Bod 9 popisuje, jak betu vést. Bod 10 je potřeba před zveřejněním repa.

### 1. Rozmístění oken při prvním použití: nic mimo obraz, nic přes výstup

**Co.** Jeden pomocník pro umístění, který platí pro všechna okna bez záznamu v `imgui.ini` a pro Zobrazení → Výchozí rozložení oken:
- Pracovní plocha je viewport bez menu.
- Hlavní výstup jde vlevo nahoru v největším celočíselném zvětšení, které se vejde do asi 65 % šířky.
- U sestav, kde hlavním výstupem je grafika (`sapi1v_hry`, `sapi1v_PoP`, `jpr1s_bgp1s`), dostane velké místo CGA-1V nebo BGP-1S. Stačí na to volitelný klíč vedle `windows` v `.sapi` nebo v `_vyber.json`.
- Ostatní displeje a nástrojová okna jdou do sloupce vpravo, každé do vlastního slotu: průvodce, Média, zapisovač, APS-5, IMS-2 a deska diagnostiky.
- Každý obdélník se ořízne do pracovní plochy; to nahradí `app.cpp:1254-1255`. Okno mimo plochu se při startu a změně velikosti vrátí dovnitř.
- Hlavní okno se při prvním startu nastaví podle `SDL_GetDisplayUsableBounds` na 85–90 % pracovní plochy a nikdy nebude vyšší. Totéž platí při obnově uložené velikosti.

Docking jako výchozí stav nedoporučuji, protože odporuje tvému rozhodnutí z 2026-10-10 (`rozhodnuti.md:1545-1548`). Do poznámek k vydání napiš testerům alfy, ať jednou použijí Výchozí rozložení oken, protože jejich `imgui.ini` drží staré pozice. Otestovat 1100×760, 1280×720, 1920×1080 a škálování 100/125/150 %.

**Proč.**
- Ve výchozím okně 1100×760 (`app.cpp:1387`) dává `app.cpp:1252-1257` grafiku pod konzoli na y = 630, kdykoli je šířka pod 1390 px. Z Flappy jsou vidět jen titulky DGD-1V a CGA-1V [new_02] [09]. Prince of Persia, hlavní lákadlo bety, se otevírá stejně.
- Průvodce zakryje třetinu konzole [new_03] nebo hru [10].
- APS-5 zakryje vlastní displej, přes který se ovládá, a mezi pecemi prosvítá text průvodce [ia_08].
- Deska diagnostiky nemá pozici, sedne na (60,60) přes Manic Miner [23] (`diag.cpp:53`).
- IMS-2 je vysoké 896 px a je useknuté [21] (`ims2.cpp:470`).
- Průvodce a zapisovač dostanou stejný obdélník [ia_07]. Paměť a Média sdílejí slot (`debugger.cpp:225`, `media.cpp:1396`).
- Okno má 1100×760 krát měřítko displeje (`app.cpp:1340, 1387`):
  - Při 125 %, běžném na noteboocích s 1080p, má 1375 px, tedy pod hranicí 1390.
  - Při 150 % má 1650×1140 a je vyšší než obrazovka.
  - Na displeji 1366×768 je moc vysoké už při 100 %.

**Náročnost:** M. Oříznutí do plochy a pozice grafiky a diagnostiky jsou S.

### 2. Jeden blok stylu: neprůhledná okna a viditelné ovládací prvky

**Co.** Hned za `StyleColorsDark()` (`app.cpp:1431`) nastavit:
- `WindowBg.w = PopupBg.w = 1.0f`. Pak můžou pryč výjimky `SetNextWindowBgAlpha` v `browser.cpp:215` a `help.cpp:114`.
- `FrameBorderSize = 1` a `Border` s alfou 1.
- `TextDisabled` asi na 0,60 šedé.
- `ScrollbarGrab` s kontrastem aspoň 3:1.
- Světlý `CheckMark`.
- Výraznější vybranou záložku a řádek a `TabBarOverlineSize = 2`.
- Aktivní titulek výraznější než dnes.
- Poli ffmpeg viditelný popisek.

Hodnoty jsou v kapitole 5.2.

**Proč.**
- Přes okna prosvítá text: „62K MON CZ 05082015JB“ a „rtin“ za cestou v konfigurátoru [11–13], BASIC v kontextovém menu [07], text průvodce mezi pecemi APS-5 [ia_08]. Vypadá to rozbitě a oprava stojí dva řádky.
- Nezaškrtnutý checkbox, rádio a prázdné pole mají vůči oknu 1,41:1, WCAG 2.2 SC 1.4.11 chce 3:1. Placeholder „Hledat“ má 3,4:1 [16] [14] [15] [20].
- Pole ffmpeg má jen placeholder, takže po vyplnění neříká, co je zač (`app.cpp:883-884`).

**Náročnost:** S. Jsou to 2 plus asi 10 řádků, potom znovu pořídit snímky 07, 11, 15, 16 a 20.

### 3. Klávesnice patří obrazovce a je vidět, kam se píše

**Co.**
- Vrátit fokus hlavnímu displeji po „Napsat do SAPI“, po Spustit, po zavření průvodce nebo výběru, po Nahrát program a po Vložit…. Mechanismus `focus_screen` už existuje (`app.cpp:606-631, 1586-1589`).
- Kolem displeje s fokusem kreslit tenký rámeček v barvě akcentu.
- Když fokus nemá žádný displej, ukázat přes obraz tlumený nápis „Klikněte sem a pište“. Kreslit ho do draw listu okna, ne do textury, aby se nedostal do snímků.
- Při zahozené tisknutelné klávese jednou upozornit.

Posílat klávesy do SAPI plošně nedoporučuji. Při prvním startu má fokus Vybrat sestavu a písmena by šla do běžícího BASICu.

**Proč.** `handle_key` a `handle_text` zahodí každou klávesu, když displej nemá fokus (`app.cpp:473, 516`). „Napsat do SAPI“ píše přes `type_text` a fokus nechá v průvodci (`browser.cpp:330-335`). Další krok průvodce proto nic neudělá:
- LIST v ZPS-1 [04];
- „klávesa přeskočí intro“ v Prince of Persia;
- Enter po JUMP 4400 v Manic Miner.

Deset průvodců to obchází větou „Klikněte do okna obrazovky…“. Jediný signál fokusu je barva titulku s kontrastem 2,22:1.

**Náročnost:** S.

### 4. Cesta ke hrám bez slepých uliček

**Co.**
- V průvodcích Her a dem a Prince of Persia dát „3“ do bloku s „Napsat do SAPI“ a přidat větu: „Volby 0–2 čekají na řadič disket, který tahle sestava nemá. Reset volbu nezmění, použij Stroj → Vypnout a zapnout.“
- Názvy programů (FLAPPY, BOMBER…) psát jako bloky kódu, aby měly tlačítko. Ke každé hře připsat ovládání.
- Přidat řádek „Normální při startu: PMD32-SD nenalezen znamená, že sestava nemá SD kartu; YXYX… znamená, že nemá IDE disk“. V anglických průvodcích přeložit i „Zadej 0-3“.
- Přidat položku Stroj → Vypnout a zapnout na jedno kliknutí, například Ctrl+F12.
- U APS-5, která reset nemá, se F12/F9 a RESET na panelu ZDR nejdřív zeptají. To je blíž hardwaru.
- Sestavám s ECMONem ve skupině Ostatní (`cpm_zrmd`, `cpm_ide`…) dát do souhrnu „po zapnutí stiskni 3“.

**Proč.**
- První obrazovka sestavy her je menu ECMONu, česky i v anglickém UI [new_05].
- Volby 0–2 visí navždy (`docs/software/ECMON.md:56`). Reset znovu spustí uloženou volbu, pokud do 1,3–3 s nestiskneš klávesu (`ECMON.md:40-42`).
- `sapi1v_hry.en.md:7` a `sapi1v_PoP.en.md:9` varování nemají, `sapi1v.en.md:8-9` ano.
- Boot hlásí „PMD32-SD nenalezen“ a „INIT HDD YXYX…“ [08] [26].
- Názvy her jsou inline kód, ze kterého renderer udělá prostý text (`browser.cpp:276-300`). U FLAPPY chybí ovládání úplně.
- F12 na APS-5 smaže nastavení (`docs/prvni-kroky.md:240-242`), přitom skutečná jednotka reset nemá.

**Náročnost:** S. Jde hlavně o texty průvodců a jednu položku menu.

### 5. Konfigurátor: propojky vidět bez posouvání

**Co.**
- Měřítko rámu omezit i výškou. Rám zabere nejvýš 40–55 % výšky obsahu; stačí jedno `min()` v `draw_frame` a nižší `kmin`.
- Po kliknutí na slot posunout nastavení do pohledu. Volitelně přidat zapamatovaný „Kompaktní rám“.
- Pevné záložky „Sestava a sběrnice | <karta ve vybraném slotu> | Mapa portů a paměti“. Nastavení sběrnice dnes otevře jen kliknutí na prázdný rám.
- Nabídku Nová ▸ naplnit názvy a kategoriemi z `_vyber.json` místo 60 cest.

Každá propojka zůstane.

**Proč.**
- Ve výchozím okně 1100×760 není po kliknutí na X7 CGA-1V vidět ani jeden řádek nastavení, jen žlutá hláška a záložky na spodním okraji [chl_01].
- Při 1280×720 to platí také [ia_06].
- Při 1600×900 jsou vidět 3–4 řádky a pátý je useknutý [11] [12].
- Měřítko se počítá jen podle šířky (`rack.cpp:1078-1088`) a nastavení dostane, co zbude (`rack.cpp:2241`).

Nováček klikne na kartu a zdá se mu, že se nic nestalo. A to u nejcennější části programu.

**Náročnost:** S. Tabulka „popisek | hodnota“ a seznam slotů ovladatelný klávesnicí jsou M, ty můžou počkat.

### 6. Chyby nahlas, data v bezpečí

**Co.**
- (a) Výsledky akcí uživatele posílat přes existující `state_notice`:
  - odmítnuté médium i s důvodem;
  - chybu nové diskety či kazety;
  - úspěch HEX/BIN, například „Nahráno 0100–1FFFh, 7936 B“.

  Selhané Zapnout ukázat v okně Chyba, když je konfigurátor zavřený. Řádky LOGE a LOGW v Logu obarvit a přidat červený segment „Chyby: N“, který Log otevře.
- (b) Textová pole a pole cest v konfigurátoru potvrzovat až Enterem nebo opuštěním, jako to už dělá `PK_HEX` (`rack.cpp:1501-1510`). Médium nikdy nezakládat z rozepsaného jména.
- (c) Změněná média zapsat pár sekund po posledním zápisu, při pauze a při ztrátě fokusu, přes dočasný soubor a přejmenování. Odpovídá to skutečnému disku i RAM zálohované baterií.
- (d) „Otevřít v konfigurátoru“ posílat přes `request(PEND_OPEN)` stejně jako Otevřít… v konfigurátoru.

**Proč.**
- (a) Chyby médií a nahrávání jdou jen do logu (`media.cpp:153, 308-311, 344-347, 378-380, 697-699`). Špatný obraz diskety nejdřív vysune starý obraz a mechanika zůstane bez hlášky prázdná (`fdc8272.cpp:61-73`).
- (b) Psaní cesty mění médium běžícího stroje po každé klávese (`rack.cpp:1533-1536`). Test prokazatelně založil soubor `work/kzd-1/beh_w` [beh_04]. U RKD-1 by vznikaly vícemegové prázdné kazety a vyměňovaly by se pod běžícím CP/M (`rkd1.cpp:38-51, 116-124`).
- (c) Diskety, RAM disky a kazety RKD-1 se zapíšou jen při vysunutí, vypnutí a čistém ukončení (`machine.cpp:25-29, 428-431`; `zrmd1.cpp:340-356`). Pád nebo odhlášení z Windows zahodí všechno od zapnutí.
- (d) `editor_open` přepíše neuložené úpravy bez dotazu (`rack.cpp:160-174, 2094-2098`). To je proti tvému pravidlu v `rozhodnuti.md:503-508`.

**Náročnost:** S, každá část zvlášť.

### 7. Celá obrazovka a obraz, který se přizpůsobí oknu

**Co.**
- Alt+Enter přepne na celou obrazovku bez rámu. Ukáže displej, který má fokus, v celočíselném zvětšení a ostatní okna skryje. Další Alt+Enter vrátí okno; Esc zůstává SAPI. Stejná funkce bude v Zobrazení → Celá obrazovka, v pravém tlačítku na obraze a v tabulce zkratek.
- Při prvním použití dát každému displeji největší celočíselný zoom, který se vejde do asi 70 % plochy. CGA-1V a DBS-1A tak dostanou 2×.
- V menu zoomu dát „Přizpůsobit oknu“ jako první položku a zaškrtnout skutečný stav. Zobrazení → Zvětšení obrazovky ať platí pro naposled aktivní displej, ne jen pro konzoli.
- Displejům nastavit `ImGuiWindowFlags_NoCollapse` a `io.ConfigWindowsMoveFromTitleBarOnly = true`.
- Později (M) přidat ostré bilineární škálování pro obraz, který se nevejde ve 2×. Poměr stran jako na TV pro jednotlivé karty je na tvém rozhodnutí.

**Proč.**
- Konzole je pevně 820×560 (`app.cpp:1201`), grafika 500×400 (`app.cpp:1257`) a zoom se zaokrouhluje dolů (`app.cpp:1180`).
- V okně 1600×900 jsou proto asi dvě třetiny prázdné [05].
- Flappy běží na 100 % [08], zatímco náhled ve výběru je dvakrát větší [26].
- Text ZPS-6 je na 100 % sotva čitelný [24].
- V `src` není `SDL_SetWindowFullscreen`.
- Dvojklik na titulek, ve Windows reflex pro maximalizaci, obraz sbalí a sbalení se uloží do `imgui.ini` (`imgui.cpp:8234-8245`).
- Menu zoomu nic nezaškrtne, i když titulek říká 200 % [07].
- MAME má Alt+Enter, openMSX (také Dear ImGui) má celou obrazovku v nastavení a emulátor PMD 85 ji má také.

**Náročnost:** M. Přizpůsobit oknu, zaškrtnutí a NoCollapse jsou S.

### 8. „Neuložené změny“ srozumitelně, dodané sestavy nepřepisovat

**Co.** Tvůj model, ve kterém je vyměněné médium součástí sestavy (rozhodnutí z 2026-10-10), bych nechal. Opravil bych komunikaci:
- Dotaz vyjmenuje skutečné rozdíly, například „Mechanika 0: CPM22P_48A_Perina.img“, „TURBO zapnuto“ nebo „Barva AND-1V“.
- Nabídne „Ponechat pro příští spuštění“ a „Zapomenout“.
- U sestav z `machines/` uloží Uložit v release buildu kopii do kategorie „Moje sestavy“ a nabídne „Obnovit původní“. Debug build ti nechá dnešní Uložit.
- Dotaz po načtení stavu jiné sestavy pojmenuje skutečnou příčinu, ne „MCP“.

Krok 2 k diskusi: média a přepínače panelu si pamatovat automaticky pro každou sestavu a ptát se jen na úpravy karet a propojek.

**Proč.** Vložení kazety, TURBO nebo Zobrazení → Barva změní popis sestavy i v konfigurátoru, který uživatel nikdy neotevřel (`rack.cpp:2140-2146, 501-507`). Konec, zavření okna i spuštění jiné sestavy se pak ptá na „Neuložené změny“ [beh_04] (`rack.cpp:241-244, 259-306`). První tlačítko Uložit přepíše `machines/*.sapi` v instalaci (`rack.cpp:176-190`) a cesta zpět k původní sestavě neexistuje.

**Náročnost:** S pro krok 1, M pro krok 2.

### 9. Beta jako skutečný test, ne zdvořilostní kolečko

**Co.**
- (0) Zajistit kanál pro hlášení. Repo je soukromé, takže odkazy na Issues v README:47, v `prvni-kroky.md:4` i v O programu → GitHub skončí pro lidi mimo spolupracovníky na 404. Testery přidej jako spolupracovníky, nebo dej do zipu e-mail.
- (1) Třem kamarádům dát 5 konkrétních úkolů zhruba na 20 minut:
  - spustit ZPS-1 a program z průvodce;
  - najít a pustit Bad Apple!! a FXS4;
  - změnit jednu propojku a restartovat;
  - uložit a načíst stav;
  - po přečtení stránky na GitHubu říct jednou větou, co SAPIemu je.

  K tomu 3 otázky: co je mátlo, kde se zasekli a co by vyhodili. Ke každému problému chtít `sapiemu.log`.
- (2) Dva lidi mimo SAPI, třeba z PMD-85 nebo ZX scény na ByteFESTu, 10 minut pozorovat a nepomáhat.
- (3) Desetivteřinový test stránky GitHubu („co to je, kdo to dělá, stáhl bys to?“) před přepsáním README a po něm.
- (4) Šablona issue (sestava, kroky, log; česky nebo anglicky). Nápověda → Nahlásit chybu… ukáže verzi, sestavu a cesty k datům a logu a nabídne „Kopírovat diagnostiku“ a „Otevřít složku logu“.

**Proč.** Alfy šly třem zasvěceným (`docs/SOUHRN.md:592-593`). Repo nemá `.github`, šablony ani CONTRIBUTING. Zasvěcení přeskočí přesně to, na čem nováček odpadne: vědí, že se mačká 3 a kliká do obrazovky. Celé tohle hodnocení je expertní, ne test s lidmi; potvrdí ho nebo vyvrátí až beta s úkoly.

**Náročnost:** S.

### 10. Přední strana pro veřejnost: tvůj hlas, přiznání AI, důkaz z hardwaru, krátké README, video

**Co.** Podrobně v kapitole 6. Pro betu stačí anglický „Getting started“ v zipu a vyřazení MCP z prvního odstavce. Zbytek stihneš před zveřejněním repa.

**Proč.**
- README má 2 642 slov. První odstavec jmenuje asi 15 kódů desek a končí MCP serverem s Claude (README:5-11).
- Anglická část je seznam zkratek (README:13-26).
- V zipu je jen český PRVNI-KROKY.md (`tools/package.py:74`).
- Nikde není věta v první osobě, fotka skutečného SAPI ani snímek z analyzátoru.
- Přiznání AI chybí, přestože 424 z 425 commitů nese `Co-Authored-By: Claude`.

**Náročnost:** S pro texty, M pro video a přestavbu README.

## 4. Rychlé výhry

Kromě S bodů z Top 10 jde každá z těchto věcí udělat za den, většinou rychleji:

- **Velikost písma 100–200 %** v Nastavení přes `style.FontScaleMain` (ImGui 1.92). Při `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` přestavět styl. Dnes je písmo pevných 16 px krát měřítko primárního monitoru, načtené jednou (`app.cpp:1340, 1433-1437`).
- **Pauza a rychlost na obraze.** Při F5, F8, Ctrl+F7 a Ctrl+F8 ukázat přes obraz na 2–3 s nápis, například „Rychlost 200 % – bez zvuku (F8)“, „Pozastaveno (F5)“ nebo „Joystick z klávesnice zapnut“. Při pauze nechat odznak „POZASTAVENO – F5“ a „[pauza]“ v titulku okna. F8 leží hned pod Ctrl+F8, které průvodci her radí, a tiše zdvojnásobí rychlost a vypne zvuk (`app.cpp:429-432, 1548-1549`).
- **Stavový řádek.**
  - Nekreslit ho přes menu; dnes přepisuje Nápovědu jako „Náp●Sestava změněna…“ [beh_03]. Když chybí místo, nejdřív vypustit upozornění, pak zkrátit režimy na odznaky.
  - Psát „rychlost 100 %“ místo „emulace 100 %“.
  - Upozornění ukazovat 6–8 s, klikací (otevře složku) a zapsané do Logu.
  - Přidat Zobrazení → Otevřít složku snímků a videí. Dnes ji nic neotevře (`shot.cpp:59`, `video.cpp:333`).
- **Pravé tlačítko na obraze:** přidat „Vložit text (Ctrl+Shift+V)“ a „Kopírovat text obrazovky (Ctrl+Shift+C)“ přes existující `screen_text()` (`headless.cpp:12`).
- **Vybrat sestavu.**
  - Oddělit Přístroje od Her a dem, nebo skupinu aspoň přejmenovat na „Hry, dema a přístroje“.
  - Popis kategorie ukázat viditelně, ne jen v tooltipu.
  - Duplicitní JPR-1S ve skupině Her přejmenovat na „Manic Miner (JPR-1S + BGP-1S)“.
  - Spustit udělat hlavním tlačítkem a náhled dát do rámečku 4:3.
  - U běžící sestavy ukázat „Restartovat“ a značku „(běží)“, ne jen zelenou barvu.

  Většinou jde o `_vyber.json`. Seskupení z 2026-10-17 je tvoje rozhodnutí.
- **Průvodce.**
  - Sám se otevře jen po výslovném spuštění. Dnes se otevírá po každém vypnutí a zapnutí, restartu i načtení stavu (`browser.cpp:435-454`), což odporuje tvému zápisu v `rozhodnuti.md:1765-1766`.
  - V průvodci jedno zaškrtávátko „Tohoto průvodce už sám neotevírat“, globální volba jen v Nastavení.
  - Když se na prvním startu zavře výběr, otevřít průvodce ZPS-1.
- **První dialog Vložit… a Nahrát program** otevřít ve složce aktuálního média nebo v `media/`. Dnes se otevírá ve `work/` (`media.cpp:69`).
- **Názvy.**
  - „Sestava…“ přejmenovat na „Konfigurátor sestavy…“ a okno na „Konfigurátor – SAPI-1 V“.
  - „Otevřít soubor sestavy…“ přejmenovat na „Spustit sestavu ze souboru…“.
  - Otevřít… v konfigurátoru přejmenovat na „Otevřít k úpravám…“.
  - Anglicky „Configurator…“, „Start machine from file…“ a „Open for editing…“; slovo „set“ jen v historických názvech.
  - Opravit pár řádků dokumentace.
- **Okna přístrojů v menu** povolovat podle typu přístroje. `dynamic_cast` už je v `aps5.cpp:355` a `ims2.cpp:462`. Hlášku s JSONem (`ims2.cpp:477`) přepsat na „Tato sestava nemá …, otevři Vybrat sestavu → Přístroje“.
- **Modální okna:** Esc znamená Zrušit, Enter bezpečnou výchozí volbu; u „Neuložené změny“ je to Zrušit. Enter ve Vybrat sestavu sestavu spustí.
- **Port 63h u desek V:** sdílený zápis ukázat šedě jako záměr, ne žlutě jako problém; katalog karet ho označí. Dnes žlutou hlášku vidí každý, kdo otevře konfigurátor vlajkové sestavy [11–13].
- **Jména souborů místo plných cest** v Médiích a Tiskárně; plnou cestu dát do tooltipu [14] [25].
- **Angličtina z dat.**
  - Pomocník `machine_title()` s `title_en` pro titulek okna OS, Poslední sestavy a konfigurátor.
  - `tr()` pro „APS-5 klíč“, „PORUCHA“, „Pec %d“, filtry dialogů (`media.cpp:24-37`) a názvy funkcí v hlášeních mapy.
  - ctest na chybějící a nepoužité klíče.
- **Pomocník `tip()`** se zalomením na 35u místo 97× `SetTooltip`. Dnes jsou tooltipy až 1470 px na jednom řádku.
- **Rozbalovací sekce, které si pamatují stav.**
  - APS-5: „Simulace poruch“ pro přerušený termočlánek a vadné topení. LI a/b jsou skutečné vstupy, ty nech vidět.
  - IMS-2: monitor sběrnice přesunout pod přístroje a sbalit.
  - Terminál: „Připojení (TCP, COM)“ a „Přenos (XMODEM)“.
  - Nastavení: MCP a ffmpeg pod „Pokročilé (pro vývojáře)“.
- **Debugger.**
  - Registry podle skutečného CPU: na 8080/8085 bez AF', IX, IY, I, R a IM a s příznaky S Z AC P CY.
  - `AlignTextToFramePadding` pro popisky.
  - „.“ místo „�“ v textovém sloupci paměti [vis_02] [vis_01].
- **Stavové barvy s ikonou nebo slovem:** ⚠ s „Upozornění:“, ✖ s „Chyba:“. Nadpisy odlišit velikostí a tučností místo žluté, kterou dnes sdílejí s varováními.
- **Hledání ve výběru bez diakritiky.** Dnes „zapisovac“ nenajde Zapisovač (`browser.cpp:85-93`). K tomu opravit „paketů: %zu“.
- **Důvěra v programu:** šedý řádek „Desky V ověřené měřením na skutečném SAPI-1 V“ u sestav s deskami V a položka Nápověda → Jak ověřujeme.
- **Pro betu.**
  - Vyřadit MCP z prvního odstavce README.
  - Obrázky v README přegenerovat bez „emulace -- %“. V režimu snímku rychlost vynechat, nepředstírat 100 %.
  - Zmenšit `docs/img/minigraf-ukazka.png`, které má 3,87 MB.
  - Přidat k zipu SHA-256.
  - Do průvodce her přidat větu, že některá dema rychle blikají (Bad Apple!!).

## 5. Větší témata

### 5.1 Struktura a menu

**Diagnóza.** Menu kopírují implementaci, ne úkoly uživatele (`app.cpp:889-1071`, `help.cpp:35-46`):
- Dostat software do SAPI je rozdělené mezi Soubor (Nahrát program), Zobrazení → Média a Stroj → Klávesnice (Vložit text).
- Zobrazení má 18 položek čtyř druhů: okna přístrojů, volby obrazu, záznam a nastavení aplikace.
- Editor konfigurace je „Sestava…“ v Zobrazení a v češtině se plete s „Vybrat sestavu…“ a „Průvodce sestavou“ (`i18n.cpp:381, 400`).
- Pauza je ve Stroji, ale Krok v Ladění.
- Soubor → Otevřít soubor sestavy… sestavu spustí, kdežto Otevřít… v konfigurátoru ji jen načte k úpravám.

**Doporučení: přidávat a přejmenovávat, nepřesouvat.** Cesty v menu cituje asi 77 řádků ve 26 souborech: průvodci česky i anglicky, První kroky, návody a README. Přestavba ve stylu VirtualBoxu (Soubor, Stroj, Zobrazení, Vstup, Média, Ladění, Nápověda) by je všechny rozbila. Tu nech až po zveřejnění a jen tehdy, když si o ni řeknou testeři.

Navrhovaný strom. `+` znamená novou položku, `←` přejmenování, ostatní zůstává:

```
Soubor
  Vybrat sestavu…
  Poslední sestavy ▸
  Spustit sestavu ze souboru…          ← Otevřít soubor sestavy…
  ─────
  Nahrát program do paměti (Intel HEX)…
  Nahrát binární soubor do paměti…
  ─────
  Uložit stav… · Načíst stav…
  Rychle uložit stav Ctrl+F2 · Rychle načíst stav Ctrl+F3
  ─────
  Konec
Stroj
  Zapnout / Vypnout
+ Vypnout a zapnout                    Ctrl+F12
  Reset                                F12
  Pauza                                F5
  Rychlost emulace ▸
  TURBO, BIOS                          jen karty, které je mají
  ─────
  Klávesnice ▸                         modely, Vložit text Ctrl+Shift+V, režim pro hry Ctrl+F7
  Zvuk, joystick a myš ▸
+ Média…                               totéž co v Zobrazení
Ladění                                 beze změny, zůstává viditelné
Zobrazení
  Konfigurátor sestavy…                ← Sestava…
  ─────
  Média
  Sériový terminál · Tiskárna · Zapisovač · Měřicí pracoviště IMS-2
  Deska diagnostiky (K3) · APS-5: panel a pece
                                       povolené jen ty, které sestava má
+ Obrazovky ▸                          skrýt nebo ukázat jednotlivé displeje, výchozí všechny
  ─────
+ Celá obrazovka                       Alt+Enter
  Zvětšení obrazovky ▸                 + Přizpůsobit oknu, 100–400 %, pro aktivní displej
  Barva ▸ · Celočíselné zvětšení · Poměr stran jako na TV · Vyhlazování
  ─────
  Snímek obrazovky                     Ctrl+F5
+ Kopírovat text obrazovky             Ctrl+Shift+C
  Nahrávat video se zvukem             Ctrl+Alt+F5
+ Otevřít složku snímků a videí
  ─────
  Výchozí rozložení oken
  Jazyk / Language ▸
  Nastavení…
Nápověda
  Průvodce sestavou
  Vybrat sestavu…
  ─────
  První kroky                          v okně programu, ne .md mimo program
  Návody k obsluze ▸
  Klávesové zkratky
+ Jak ověřujeme                        měření na SAPI-1 V
+ Nahlásit chybu…
  ─────
  O programu

Pravé tlačítko na obraze
+ Přizpůsobit oknu · 100 % · 200 % · 300 % · 400 %   zaškrtnuté, co platí
+ Celá obrazovka                       Alt+Enter
+ Vložit text                          Ctrl+Shift+V
+ Kopírovat text obrazovky             Ctrl+Shift+C
  Uložit snímek · Uložit snímek jako… · Kopírovat snímek · Nahrávat video
```

Dvě věci narážejí na tvá zapsaná rozhodnutí, proto je ve stromu nemám:
- **Konfigurátor ve Stroji.** `rozhodnuti.md:1549` říká: „je to zapnutí a vypnutí okna… ne ve Stroji“. Oponent kvůli tomu zamítl i BEN-08. Doporučuji jen přejmenovat a do Stroje ho nedávat, dokud testeři neukážou, že ho tam hledají.
- **F9.** Reset na F9 je tvoje rozhodnutí kvůli ladění ve VS (`rozhodnuti.md:137-143`). Jestli vůbec něco měnit, tak jen v release buildu a jen při otevřeném debuggeru: F9 = breakpoint.

Nastavení podle sekcí:

```
Obecné      Jazyk / Language · Průvodci se otevírají sami · + Velikost písma 100–200 %
Obraz       Celočíselné zvětšení · Poměr stran jako na TV · Vyhlazování (zrcadlí menu)
Zvuk        Zvuk · Hlasitost
Ovládání    Citlivost myši · Myš PC jako myš SAPI (jen do ukončení)
Záznam      Snímek zvětšený 3× · ffmpeg: [cesta] · + složka a „Otevřít složku“
▸ Pokročilé (pro vývojáře)   Server MCP          sbalené
```

Uvnitř oken platí stejný princip postupného odhalování: rozbalovací sekce v APS-5, IMS-2, terminálu a Nastavení (viz Rychlé výhry). Menu Ladění zůstane viditelné. Debugger je pro retro programátory lákadlo a MCP je už teď vypnutý.

K tomu jednostránkový slovníček v `docs`, česky i anglicky. Česky: sestava = stroj, konfigurátor sestavy = editor. Anglicky: machine a configurator, slovo „set“ jen v historických názvech.

### 5.2 Vizuální styl

**Diagnóza.** SAPIemu má dvě tváře. Kreslené přístroje jsou řemeslně udělané. Všechno kolem je nezměněný `StyleColorsDark()` (`app.cpp:1431-1434`):
- Okna (#0F0F0F) jsou tmavší než plocha (#181818) a neaktivní titulky mají vůči ploše 1,11:1.
- Titulky, hlavičky i tlačítka jsou sytě modré, rohy bez zaoblení a průsvitnost je 0,94.
- Program má jedno písmo, jednu velikost a žádnou tučnou.
- Nadpisy a varování mají stejnou žlutou.

Obojí uzavři do jedné funkce `ui_theme_apply()` s konstantami v `ui_theme.h`. Pořadí: nejdřív neprůhlednost, pak kontrast, pak paleta.

Upravil jsem dvě hodnoty z návrhu recenzenta a poměry jsem přepočítal:
- **TitleBgActive.** #22303F by dal jen 1,24:1, dnes je 2,22:1, a titulek je jediný signál, kam jde klávesnice. Proto #3D6A99 (2,96:1).
- **ScrollbarGrab.** #3A4048 by dal 1,60:1, proto #646C77 (3,15:1).

```
// povrchy
clear colour (app.cpp:1614)        #121417
WindowBg                           #1B1E22   alfa 1.0
PopupBg                            #23272D   alfa 1.0
ChildBg                            průhledné; kód a terminál #15171A
MenuBarBg                          #16181C
TitleBg / TitleBgActive            #1B1E22 / #3D6A99   aktivní vs neaktivní 2,96:1
Border                             #646C77   3,15:1, FrameBorderSize 1
// text
Text                               #E6E9ED   13,7:1
TextDisabled                       #9AA3AE   6,6:1 na okně, 5,3:1 v poli
// ovládací prvky
FrameBg / Hovered / Active         #2A2F36 / #333A43 / #3B444F
Button / Hovered / Active          #2F353D / #3A424C / #46505C
hlavní tlačítko (Spustit, Restartovat)  #3D6A99 / #4A7AAD / #5689BF, bílý text 5,7:1
CheckMark, TextLink, NavCursor     #8FB8E0   6,5:1 v poli
SliderGrab                         #6F95BC
Header (výběr) / Hovered / Active  #3D6A99 / #34536F / #4A7AAD   výběr 2,96:1, text na něm 4,6:1
CollapsingHeader a kategorie       push #262B31, akcent nese jen výběr
Tab / TabSelected                  #23272D / #2C4763, TabSelectedOverline #8FB8E0 (7,2:1)
Separator                          #3A4048
ScrollbarGrab / Hovered / Active   #646C77 / #7A838F / #8FB8E0
ResizeGrip                         průhledné, hover ponechat
TableRowBgAlt #FFFFFF08 · TextSelectedBg #3D6A998C · ModalWindowDimBg #00000080
okna displejů                      WindowBg #101010, okraj splyne s obrazem
// stavové barvy na #1B1E22, vždy s ikonou nebo slovem
OK #4CC38A 7,6:1 · WARN #E3B341 8,6:1 · ERROR #F06A60 5,5:1 · PENDING #F0883E 6,6:1
// rozměry (ScaleAllSizes(dpi))
WindowPadding 10,10 · FramePadding 8,4 · ItemSpacing 8,6 · ItemInnerSpacing 6,4 · CellPadding 6,3
IndentSpacing 18 · ScrollbarSize 12 · GrabMinSize 10
Window/Child/Popup/FrameBorderSize 1 · TabBorderSize 0 · TabBarOverlineSize 2
SeparatorTextBorderSize 1 · SeparatorTextPadding 12,4
WindowRounding 6 · PopupRounding 6 · ChildRounding 4 · FrameRounding 4 · TabRounding 4
ScrollbarRounding 6 · GrabRounding 3
WindowMenuButtonPosition ImGuiDir_None · DisabledAlpha 0.5
```

**Akcent.** Doporučuji ocelově modrou odvozenou z barvy rámu ARB-1 (`rack.cpp:39`). Jantarová z AND-1V by splynula s varovnou #E3B341.

**Písmo.** Přidat DejaVu Sans Bold; má stejnou licenci jako DejaVu, které už v repu je. Velikosti přes `PushFont(font, size)` z ImGui 1.92:

```
Nadpis    Bold 20 px   název sestavy ve výběru a v průvodci, nadpisy oken
Sekce     Bold 16 px   SeparatorText, CollapsingHeader
Tělo      16 px        beze změny
Drobné    13 px        X1…, cesty, jednotky, nápovědné řádky
Mono      15 px        beze změny
O programu  28 px tučně v barvě textu, verze 13 px
```

**Identita z hardwaru.**
- Tiskárně dát světlý papír #F4F1E8 s tmavým textem #222 (14,1:1), jako má Minigraf.
- Čelní panely mechanik a terminál ve stylu VT až později (M).
- Logo v O programu exportovat s průhledností a odkazy kreslit přes `TextLinkOpenURL` místo tlačítek.

**Po změně zkontrolovat** místa, která si nastavují vlastní barvy: klávesnici APS-5 (`aps5.cpp:76-78`), oranžové Restartovat (`rack.cpp:2198-2200`) a blok kódu v průvodci (`browser.cpp:317`).

**Drobnosti v kresbách:**
- Na ose APS-5 sedí „0“ na „-60 min“.
- Posuvník baterie je SliderFloat vedle dvou DragFloatů.
- Kabely IMS-2 vedou přes popisky.
- Výška IMS-2 má být nejvýš výška pracovní plochy.

### 5.3 Onboarding a průvodci

- **Vybrat sestavu.**
  - Přidat nahoru skupinu „Začněte zde“ s Prince of Persia, Hrami a demy a ZPS-1 s BASICem. Rejstřík už umí jednu sestavu ve dvou kategoriích.
  - Oddělit Přístroje. Tvůj `docs/vydani/sestavy.md` je tak už člení v oddílech 3a–3c.
  - Anglické názvy kategorií mají říct, co si vybrat: „historic Tesla configurations (ZPS)“, „Libor Lasota's modern boards“.
  - V anglickém UI přesunout český technický název do technického popisu.
- **Průvodci jako akce.**
  - Všechno, co se píše, dát do bloků kódu.
  - Odkazy typu `sapi:type:…` a `sapi:load:…` obsloužit v `open_link` (`browser.cpp:302-312`) a po akci vrátit fokus obrazovce.
  - Ke každé hře připsat ovládání a řádek „Normální při startu“.
- **Později (M).**
  - Spouštění her jedním tlačítkem: napíše 3, počká na A>, napíše FLAPPY, zapne joystick z klávesnice a dá fokus CGA-1V. Je to stejná konvence jako MAME `-autoboot_command` nebo „phantom typist“ ve Fuse.
  - Knihovna asi 15 vybraných programů z `media/` s akcí „vlož do B:“ nebo „nahraj HEX a spusť“.
  - Přetahování souborů na okno:
    - `.sapi` sestavu spustí, `.sapistate` načte stav;
    - `.hex` a `.bin` otevřou dialog nahrávání;
    - `.img` a `.imd` jdou do volné mechaniky, `.wav` a `.mp3` do magnetofonu.

    86Box i openMSX to umějí.
- **Nápověda v programu.** První kroky a krátký anglický „Getting started“ ukázat ve vlastním okně rendererem průvodců doplněným o jednoduché tabulky. Druhá možnost je HTML z `package.py`. Po zveřejnění přidat odkaz na stránku, kterou vykreslí GitHub.
- **Důvěra přímo v programu.** Viz Rychlé výhry. Později stav u každé karty: měřeno / podle schématu a PROM / vypínatelná odchylka / N otevřených otázek. Funguje to jako barvy working a imperfect v MAME.

### 5.4 Přístupnost a jazyky

- **Kontrast.** Text projde (19,2:1), ovládací prvky ne (1,41:1). Řeší to blok z 5.2 (WCAG 2.2 SC 1.4.11 a 1.4.3).
- **Velikost.**
  - Chybí nastavení velikosti písma.
  - DPI se čte jednou z primárního monitoru a změna měřítka se neobsluhuje.
  - Asi 25 rozměrů je v pixelech (`debugger.cpp`, `terminal.cpp`, `media.cpp`, `app.cpp:1201, 1254-1257`).

  Řešení: Velikost písma 100–200 %, přestavba stylu při změně měřítka a rozměry v jednotkách písma (SC 1.4.4). Před betou vyzkoušej první start při 125 a 150 %.
- **Klávesnice.** Nejdřív Enter a Esc v modálních oknech. Plnou navigaci ImGui (`NavEnableKeyboard` s `NoNavInputs` na displejích a terminálu) zkoušej až opatrně, protože Alt, AltGr a šipky patří SAPI (SC 2.1.1, 2.4.7).
- **Barva jako jediný nositel významu (SC 1.4.1):**
  - Běžící sestava je jen zelená.
  - V grafu APS-5 jsou pro deuteranopy Pec 4 a Pec 5 prakticky stejné (ΔE2000 0,3–0,6) a DEMO topí právě v obou. Pomůže číslo pece na konci čáry, hodnoty pod kurzorem a zvýraznění při najetí.
  - Režim pásky je jen ve výplni tlačítka; přidat text PLAY/REC.
  - Chyba a varování se liší jen červenou a žlutou; přidat „Chyba:“ a „Upozornění:“.
- **Tooltipy (SC 1.4.13).** Některé významy jsou jen v nich: „…“ = vysunout bez uložení, LO1/LO2, LI a/b. Pomůže `tip()` se zalomením a viditelný popisek u klíčových věcí.
- **Čtečky obrazovky.** S Dear ImGui nefungují, protože ImGui nevytváří strom UI Automation, a v rámci tvých omezení to nezměníš. Napiš to poctivě do README a nabídni textové cesty: Kopírovat text obrazovky, `sapiemu-cli --print-screen` a sériový terminál.
- **Jazyky.** Navrhuji pravidlo: popisky natištěné na přístroji zůstanou, jak jsou, a dostanou anglický tooltip (vzor ZDR). Všechno, co píše emulátor, se překládá. Dnes to neplatí:
  - IMS-2 má VOLTAGE, CURRENT, PROTECTION a MAINS přeložené, ale SÍŤ, AUTO, FILTR, CHYBA a MĚŘENÍ ne. PROTECTION navíc naráží na MAINS [a11y_21].
  - Anglické UI ukazuje „APS-5 klíč 2, PORUCHA“, „Pec 1–8“ a český titulek okna [a11y_20] [26].
  - V českém UI je v hlášce mapy „MAP register“ [11].
  - Filtry dialogů jsou česky i v angličtině.
  - Chyby jádra míchají češtinu s angličtinou („Stav nejde načíst: not a SAPIemu state file“).

  Náhledy sestav jsou snímky českého UI; oříznout je jen na obraz, aby nezávisely na jazyce. Desetinná čárka nebo tečka v číslech emulátoru je na tobě.

### 5.5 Chování

- **Fokus a klávesnice:** bod 3. **Chyby a data:** bod 6. **Neuložené změny:** bod 8. **Slepé uličky ECMONu:** bod 4.
- **Načtení stavu.** Když se načtení nepovede, běžící stroj už je pryč (`host.cpp:52-65, 249-253`).
  - Verzi zapsat do hlavičky a nesouhlas odmítnout ještě před `load_config` (S).
  - Zálohovat stav v paměti a starý stroj vypnout až po postavení nového (M). Tím zmizí i nekonečný výpadek napájení HRK-1 po nepovedeném přepnutí (`hrk1.h:35-36`).
- **Vypnout a zapnout není odchylka od hardwaru,** je to vypínač. Případnou nápovědu „CPU čeká na port 3Ch, žádná karta neodpovídá“ dělat jen jako označenou, vypínatelnou pohodlnost a až později.
- **Konec relace.** Spustit u běžící sestavy ji tiše restartuje (`browser.cpp:100-110, 175`). Stačí popisek „Restartovat“ a dotaz u dvojkliku. Automatický stav před přepnutím s tlačítkem „Vrátit“ na 30 s a „Pokračovat, kde jsem skončil“ jen jako vypnutá a označená pohodlnost, později.
- **Konzistence.**
  - Stroj → Klávesnice ▸ vymění klávesnici mimo konfigurátor a po restartu je zpět stará (`machine.cpp:417-426`). Má jít přes `rack_set_live_param` jako Barva.
  - Poslední sestavy se plní jen z okna výběru (`browser.cpp:70-83`).
  - „Myš PC jako myš SAPI“ se záměrně neukládá; přidat štítek „(jen do ukončení)“.
- **Dvě instance nad stejnými daty** si přepisují nastavení i RAM disky. Stačí upozornění při startu druhého GUI, bez tvrdého zámku, protože `sapiemu-cli` běží souběžně běžně.

## 6. Obava z „AI slop“

### Poctivě

**Obsahem je SAPIemu opak AI slopu.**
- Tvrzení jsou konkrétní a ověřitelná.
- Text nemá typické fráze: recenzent hledal seamless, robust, comprehensive, delve, leverage, powerful a intuitive i české komplexní, robustní a intuitivní a nenašel nic.
- Nic se nepřehání a otevřené otázky jsou zveřejněné.
- Za vším je měření na skutečném stroji.

**Povrch ale nese tři „prototypické vlastnosti“ slopu**, které kritici hledají (Kommers et al. 2026, citováno v hesle Wikipedie „AI slop“):
1. **Masová produkce.** 425 commitů od 3. 10. 2026, z toho 424 s `Co-Authored-By: Claude`, asi 47 500 řádků C++ a 185 000 slov dokumentace v 96 souborech.
2. **Nepoměr úsilí.** README o 2 642 slovech, první odstavec s 15 kódy desek a poznámky, které o tobě mluví ve třetí osobě („autor schválil“ 17×).
3. **Žádný viditelný člověk ani důkaz.** Chybí fotka skutečného SAPI-1, snímek z analyzátoru i věta v první osobě. Claude se objevuje jen jako funkce MCP.

**Česká a slovenská scéna** (sapi.cz, pmd85.cz, oldcomp.cz, ByteFEST, Vrbice): tam tě ponese tvoje jméno, články na 8bity.cz o deskách SAPI-1 od roku 2024 a účast Petra Všijanského a Libora Lasoty. Podmínkou je, že AI zmíníš sám dřív, než ji někdo najde v historii commitů. Větší riziko je tam „je toho moc, kde začít“.

**Mezinárodně** (r/emulation, Hacker News, Hackaday) by dnes výchozí dojem byl „vibe-coded emulátor obskurního stroje za zdí českého textu“. V roce 2026 sklízejí nepřátelské komentáře i dobře otestované projekty s AI. Respekt tam přináší to, co SAPIemu má, ale schovává: měření na hardwaru, konkrétního člověka, otevřené přiznání a stručnou úvodní stránku.

**K obavě, že kamarádi chválí bez testování:** je to pravděpodobné z podstaty věci, ne jejich chybou. Vědí, že se mačká 3 a kliká do obrazovky, takže díry z kapitoly 1 nevidí. Řeší to bod 9.

### Co udělat, v tomhle pořadí

1. **Přiznat, že program vzniká s AI.** Stačí před zveřejněním repa; kamarádi z bety to vědí. Vlastními slovy, věcně, bez omlouvání i bez obhajoby. Tvrdit jen to, co platí: měřené jsou desky V, desky Tesla vycházejí z dokumentace a dumpů. Návrh pro README hned za úvod:

   > **Jak SAPIemu vzniká.** Emulátor vyvíjím od října 2026 s AI asistentem Claude Code (Anthropic). Kód a většinu poznámek v docs/ píše Claude podle mého zadání a pod mou kontrolou. Já stavím a měřím desky SAPI-1, rozhoduji, co a jak se emuluje, a chování desek V ověřuji na svém SAPI-1 V logickým analyzátorem (docs/zmeny-podle-hw.md). Původní desky Tesla vycházejí z dokumentace, schémat a dumpů ze sapi.cz (Petr Všijanský) a od Libora Lasoty; co ještě nebylo ověřeno na skutečném hardwaru, je v docs/hw-otazky.md. Proto má každý commit řádek Co-Authored-By: Claude. Najdete-li rozdíl proti skutečnému SAPI-1, napište do Issues.

   > **How SAPIemu is made.** I have been developing SAPIemu since October 2026 with the AI coding assistant Claude Code (Anthropic). Claude writes the code and most of the notes in docs/ to my brief and under my review. I build and measure SAPI-1 boards, decide what is emulated and how, and check the behaviour of the V boards on my own SAPI-1 V with a logic analyser (docs/zmeny-podle-hw.md). The original Tesla boards follow the documentation, schematics and ROM dumps from sapi.cz (Petr Všijanský) and from Libor Lasota; what has not been checked on real hardware yet is listed in docs/hw-otazky.md. That is why every commit carries Co-Authored-By: Claude. If you find a difference from a real SAPI-1, please open an issue.

   Do O programu pod Autora přidat jeden řádek: „Vyvíjeno s AI asistentem Claude Code; chování desek V ověřuji měřením na skutečném SAPI-1 V.“ / „Developed with the AI coding assistant Claude Code; the behaviour of the V boards is checked by measurements on a real SAPI-1 V.“ Jednu větu dát i do poznámek k prvnímu veřejnému vydání a do prvního odstavce každého oznámení. `Co-Authored-By` ponechat a historii nepřepisovat. Lutris v březnu 2026 atribuci odstranil a po kritice ji vracel.
2. **Ukázat důkaz.** Sekce „Měřeno na skutečném SAPI-1 V“ hned za úvodem README:
   - fotka SAPI-1 V s PicoROM a Logic Pro 16, případně fotka z tvého článku na 8bity.cz;
   - jeden snímek z Logic 2 s dvouřádkovým popiskem ve tvaru „emulátor se mýlil, deska ukázala proč, teď sedí“. Nejlépe DSM-1V, kde dva zápisy dávají jeden znak (emulátor posílal 55 55 55, deska 55 55), nebo zamrznutí FXS4 v cyklu INTA;
   - dvě až tři čísla, například ZRDBENCH při 4 MHz: deska 23,61/22,51 T na bajt, emulátor 23,61/22,50;
   - řádek o testech: jádra floooh/chips z80.h a superzazu/8080 s měřenými úpravami, ZEXDOC, ZEXALL, 8080EXM a 8085EXM, `check_machines.py`;
   - odkazy na `zmeny-podle-hw.md`, články na 8bity.cz a `hw-otazky-priorita.md` jako „co jsme ještě neověřili“.
3. **Tvůj hlas.** 120–200 slov, která napíšeš sám; Claude opraví jen překlepy. Obsah:
   - kdo jsi: stavíš desky SAPI-1 a píšeš na 8bity.cz;
   - proč: ověřit emulaci proti vlastním deskám a dát SAPI lidem bez hardwaru;
   - co je zvláštní: karty a propojky jako skutečné, přístroje, měření na desce V;
   - co je poctivé: Claude a alfa verze.

   Anglicky raději krátký odstavec vlastními slovy než uhlazený překlad. Přidat řádek „Kdo za tím je“ s odkazy na 8bity.cz, sapi.cz a pmd85.cz.
4. **Kratší README, asi 100–120 řádků.** Pořadí:
   1. titulek „SAPIemu: emulátor Tesla SAPI-1“ a logo;
   2. jedna věta, co to je: „Emulátor československého modulárního počítače Tesla SAPI-1 (1983–1990): sestavíte ho z karet v rámu jako skutečný a spustíte MIKROBASIC, MIKOS nebo CP/M“;
   3. tvůj odstavec a odstavec o AI;
   4. GIF;
   5. „Stáhnout a spustit“ ve třech krocích s poznámkou o SmartScreenu;
   6. „Co vyzkoušet“: pět sestav s náhledy (ZPS-1 BASIC, ZPS-3 CP/M, hry na SAPI-1 V, APS-5, IMS-2);
   7. sekce s měřením;
   8. pět odkazů a „všechny dokumenty“;
   9. zkrácené soukromí, poděkování a licence.

   Celé „Co umí“ beze změny přesunout do `docs/funkce.md`. MCP a `sapiemu-cli` přesunout do sekce „Pro vývojáře“. Anglicky `README.en.md` se stejnou kostrou, začít tím, co SAPI-1 bylo. Vždy psát „Tesla SAPI-1“, protože samotné SAPI koliduje s Microsoft Speech API. Vyplnit popis a topics repa.
5. **Video.**
   - Nahoře GIF 8–10 s, pod 5 MB: Vybrat sestavu → Spustit ZPS-1 → BASIC běží, nebo rám s LED a pak Bad Apple!!.
   - Jedno video na 2–3 minuty, které natočíš ty: skutečný SAPI-1 V na stole a SAPIemu na notebooku se stejným programem vedle sebe, pak konfigurátor, APS-5 a IMS-2. Odpoví zároveň na „je to skutečné?“, „funguje to?“ i „kdo to dělá?“.
   - Část z emulátoru nahraješ přes Ctrl+Alt+F5 s čistým rozložením oken.
6. **Drobnosti, kterých si všímá každý, kdo AI slop hledá.**
   - Obrázky v README přegenerovat ze současného GUI. Pět ze šesti ukazuje „emulace -- %“ a `sapi1v-rat-sestava.png` má staré tlačítko „Zapnout s touto sestavou“.
   - Zmenšit `minigraf-ukazka.png`.
   - Verzi v README srovnat při vydání 0.7.0.
   - Do kontrolního seznamu vydání dát bod „obrázky aktuální, verze sedí“.
   - Neprůhledná okna a vlastní akcent (5.2), aby sdílené snímky nevypadaly jako ukázka ImGui.
7. **Úklid repa.**
   - Do `SOUHRN.md`, `rozhodnuti.md` a `hw-otazky*.md` dát dvouřádkovou hlavičku „Pracovní poznámky z vývoje, vede je Claude Code; „autor“ = Martin Lukášek“.
   - Z verzovaného `CLAUDE.md` vyndat, co cizí nepotřebuje: jméno tvého serveru se sdílenou složkou a cesty na `H:`. Soubor nechat veřejný.
   - `zadani.md` přesunout do `docs/vznik.md` s jednou větou úvodu. Je to nejlepší doklad, že cílem bylo ověřování proti skutečným deskám.
   - `hw-otazky-priorita.md` přejmenovat na pozvánku „Hledáme pomoc: otázky k ověření na skutečném HW“.
8. **Commity.**
   - Předměty kolem 60–72 znaků, podrobnosti do těla. Dnes je průměr 181 znaků.
   - Psát neutrálně místo „the author“.
   - `Co-Authored-By` ponechat; pravidlo dát do `CLAUDE.md`.
   - Do odstavce o AI přidat jednu větu o tempu: „Git historie začíná 3. 10. 2026; na deskách SAPI-1 pracuji od roku 2024 (8bity.cz).“
9. **Beta jako test:** bod 9 v Top 10.
10. **Kde to oznámit.**
    - **Nejdřív česká a slovenská scéna**, přes lidi, kteří tě znají:
      - článek na 8bity.cz tvým hlasem s fotkami a videem;
      - vlákno na oldcomp.cz (Československé počítače → Emulace na moderních strojích);
      - zpráva nebo odkaz na sapi.cz a pmd85.cz, ideálně s větou od Petra a Libora;
      - živá ukázka na ByteFESTu nebo ve Vrbici se skutečným SAPI-1 V vedle notebooku;
      - otázky z `hw-otazky-priorita.md` jako výzva „Hledáme pomoc“.
    - **Mezinárodně až s `README.en.md` a videem.** Nejdřív fórum VCFed a r/retrocomputing (lidé od hardwaru), potom tip pro Hackaday, Show HN nakonec.
    - **Začínat příběhem hardwaru**, například „emulátor československého modulárního počítače z 80. let, ověřovaný proti skutečnému stroji logickým analyzátorem“, ne Claudem a MCP. AI zmínit jednou větou v prvním odstavci a na dotazy odpovídat měřením, ne debatou o AI obecně.
    - K vydání přiložit kontrolní součty SHA-256.

### Zdroje

Tyto zdroje našli recenzenti vnímání a srovnání. Případ Lutris ověřil i oponent, ostatní citace znovu ověřené nejsou. Recenzentům brzy došlo vyhledávání na webu, proto stránky stahovali přímo.

- Wikipedia, „AI slop“ (Kommers et al. 2026, „superficial competence“): https://en.wikipedia.org/wiki/AI_slop
- Simon Willison, „Slop“ (2024, „unreviewed content“): https://simonwillison.net/2024/May/8/slop/
- GamingOnLinux, Lutris skryl atribuci Claude a pak ji vracel (12. 3. 2026): https://www.gamingonlinux.com/2026/03/lutris-now-being-built-with-claude-ai-developer-decides-to-hide-it-after-backlash/
- Ghostty, AI_POLICY.md („All AI usage in any form must be disclosed“): https://github.com/ghostty-org/ghostty/blob/main/AI_POLICY.md
- Hackaday, port na SNES s Claude a komentáře (11. 7. 2026): https://hackaday.com/2026/07/11/micropython-is-this-summers-hottest-title-for-the-snes-thanks-to-claude-fable/
- Hacker News, vlákno o AI v retro emulaci (MAME): https://news.ycombinator.com/item?id=49778613
- Pravidla Show HN: https://news.ycombinator.com/showhn.html
- QEMU, code provenance (odmítá příspěvky odvozené z AI): https://www.qemu.org/docs/master/devel/code-provenance.html
- GitHub Docs, About READMEs: https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-readmes
- Tvůj článek o analyzátoru pro sběrnici SAPI-1 (28. 7. 2024): https://www.8bity.cz/2024/48-channel-logic-analyzer-for-sapi-1-bus/
- Konvence ostatních emulátorů:
  - 86Box, správce strojů: https://86box.readthedocs.io/en/latest/usage/manager.html
  - 86Box, stavový řádek s médii: https://86box.readthedocs.io/en/latest/usage/statusbar.html
  - MAME, Alt+Enter: https://docs.mamedev.org/usingmame/defaultkeys.html
  - MAME, `-autoboot_command`: https://docs.mamedev.org/commandline/commandline-all.html
  - MAME, výběr strojů a stav emulace: https://docs.mamedev.org/usingmame/ui.html
  - openMSX, GUI v ImGui: https://github.com/openMSX/openMSX/tree/master/src/imgui
  - Fuse: https://fuse-emulator.sourceforge.net/
  - VICE, autostart: https://vice-emu.sourceforge.io/vice_4.html
  - Altirra: https://www.virtualdub.org/altirra.html
  - Mesen, štítky v debuggeru: https://www.mesen.ca/docs/debugging/debuggerintegration.html
  - Emulátor PMD 85: https://pmd85.borik.net/wiki/Emulator

## 7. Příloha: všechna potvrzená zjištění

Každé zjištění má tvar *závažnost, název — doklad → doporučení (náročnost)*. Závažnosti jsou kritické, závažné, menší a kosmetické; náročnost S, M nebo L. Předpony ID: NEW = nováček, IA = informační architektura, VIS = vzhled, A11Y = přístupnost, BEH = chování, PER = vnímání, BEN = srovnání. MISS, REV a MIS doplnila oponentura. Sloučená zjištění jsou uvedena jednou.

**Rozvržení oken a obraz**
- [IA-01, NEW-01, VIS-01, BEN-01, BEH-01, MIS-03] kritické, Okna při prvním použití skrývají nebo zakrývají hlavní obsah — [new_02] [09] [10] [new_03] [ia_07] [ia_08] [14] [20] [21] [23], `app.cpp:1252-1257, 1387`, `diag.cpp:53`, `ims2.cpp:470` → jeden pomocník umístění z viewportu s ořezem, hlavní výstup podle sestavy, sloupec slotů vpravo, bez výchozího dockingu, test při 100/125/150 % (M, jádro S)
- [MISS-02, A11Y-02, BEH-17] závažné, Škálování Windows a velikost hlavního okna — 1100×760 × měřítko bez ořezu (`app.cpp:1340, 1387`), změna měřítka se neobsluhuje (`app.cpp:1503-1515`), asi 25 rozměrů v pixelech → ořez na `SDL_GetDisplayUsableBounds` (S), velikost písma (S), přestavba stylu (S), rozměry v jednotkách písma (M)
- [NEW-02, IA-07, BEN-02, BEH-09, REV-03] závažné, Chybí celá obrazovka a obraz neroste s oknem — [05] [08] [24] [26], `app.cpp:1201, 1257`, žádné `SDL_SetWindowFullscreen` → Alt+Enter, zoom při prvním použití podle plochy; docking jen volitelně (M)
- [VIS-03, NEW-11 zoom, IA-22] závažné, Malý obraz ve velkém černém rámu, menu zoomu nic nezaškrtne a platí jen pro konzoli — [24] [07], `app.cpp:1180, 675, 1031-1036` → Přizpůsobit oknu a zaškrtnutí (S), zoom pro aktivní displej (S), ostré bilineární škálování (M), poměr TV pro jednotlivé karty probrat
- [MIS-01, MISS-04] menší, Displej se chová jako nástrojové okno: dvojklik ho sbalí natrvalo, tah po obraze posune okno, v menu chybí Vložit — `app.cpp:1211, 1260`, `imgui.cpp:8234-8245`, [07] → NoCollapse, ConfigWindowsMoveFromTitleBarOnly, Vložit text (S)
- [IA-14] menší, Titulky podle typu karty, titulek OS česky, displej nejde skrýt — [08] (prázdné DGD-1V), `app.cpp:331, 1211, 1260` → titulek z `title_en`, Zobrazení → Obrazovky ▸ (S)

**Klávesnice a vstup**
- [MISS-01, REV-01, BEH-02] závažné, Bez fokusu displeje klávesy mizí; „Napsat do SAPI“ nechá fokus v průvodci — `app.cpp:473, 516`, `browser.cpp:330-335`, [04] [19] → vracet fokus, rámeček, „Klikněte sem a pište“, jednorázové upozornění (S)
- [MISS-03, REV-04, MIS-04] menší, F8 vedle Ctrl+F8 tiše zrychlí a ztlumí; pauza je jen slovo v přeplněném řádku — `app.cpp:429-432, 1548-1549, 417-421, 1087` → nápis přes obraz, odznak při pauze, „[pauza]“ v titulku (S)
- [IA-12, BEH-13, A11Y-03] menší, Žádná navigace klávesnicí, dialogy nereagují na Enter a Esc — `app.cpp:1429`, `rack.cpp:259-306`, `app.cpp:1291-1308` → Enter a Esc v modálních oknech a Enter ve výběru (S), NavEnableKeyboard s NoNavInputs na displejích jako pozdější pokus (M)
- [IA-11] menší, F9 resetuje, ačkoli F5, F10 a F11 kopírují Visual Studio — `app.cpp:438-439` → při otevřeném debuggeru F9 = breakpoint, debug build F9 = reset; pozor, F9 jako reset je tvoje rozhodnutí (`rozhodnuti.md:137-143`), BEH-11 proto oponent zamítl (S, nízká priorita)
- [IA-18] menší, Stroj → Klávesnice ▸ vymění klávesnici mimo konfigurátor a restart vrátí starou — `machine.cpp:417-426, 527-533` → přes `rack_set_live_param` jako Barva (S)

**Hry, onboarding a průvodci**
- [NEW-03] závažné, Slepé uličky věrné hardwaru nemají východ: ECMON 0–2, Reset drží volbu, F12 na APS-5 — [new_05], `ECMON.md:40-42, 56`, `prvni-kroky.md:240-242` → „3“ jako blok s varováním, Vypnout a zapnout (Ctrl+F12), reset APS-5 s dotazem, souhrny sestav s ECMONem (S)
- [NEW-05, BEN-01 texty a spouštění, BEN-06] menší, Ke hře vedou kroky, které je třeba znát zpaměti — `sapi1v_hry.en.md:16-19`, `browser.cpp:276-300, 314-343`, [new_08], `media.cpp:69` → první dialog v `media/`, odkazy `sapi:type:` a `sapi:load:`, ovládání her; spouštění her a Knihovna později (S; M)
- [BEN-03, BEH-14, NEW-05 bod 4] menší, Nejde přetáhnout soubor, Média ukazují dlouhé cesty pracovních kopií — `app.cpp:1503-1515`, [14] → obsluha přetažení podle přípony přes existující dialogy, jméno souboru s cestou v tooltipu; nedávné obrazy později (M)
- [NEW-06, PER-11 hlášky] menší, Boot vypadá jako chyba — [08] [26], [new_05] („Uzivatel: Martin“ je z dumpu ECMON, ROM neměnit), [new_06] (`hrk1.cpp:161`), [new_08] („LED SHIFT ERR“) → řádek „Normální při startu“, přeložit stav APS-5, LED klávesnice jako kontrolky (S)
- [NEW-07, IA-09, BEN-09, VIS-09] menší, Vybrat sestavu: v Hrách 6 přístrojů, JPR-1S dvakrát, 43 sestav s českými technickými jmény, náhled bez rámu, tři rovnocenná tlačítka — `_vyber.json:23, 26-34`, `browser.cpp:171, 175-187, 247`, [01] [02] [26] → oddělit Přístroje, „Začněte zde“, „Manic Miner (JPR-1S + BGP-1S)“, viditelné popisy kategorií, náhled 4:3, Spustit jako hlavní tlačítko; seskupení z 2026-10-17 je tvoje volba (S)
- [NEW-08, IA-08, IA-21, BEN-05, BEH-22] menší, Nápověda otevírá surový .md mimo program, dvě zaškrtávátka, po zavření výběru žádný průvodce — `help.cpp:12-18, 38-44`, `browser.cpp:446-447, 488-495`, `app.cpp:1462`, [04] → První kroky v okně programu s tabulkami, anglický Getting started, jedno zaškrtávátko, průvodce ZPS-1 (S)
- [BEH-10] menší, Průvodce se znovu otevírá po každém vypnutí, restartu a načtení stavu — `browser.cpp:435-454`, `host.cpp:68, 82`; odporuje `rozhodnuti.md:1765-1766` → jen po výslovném spuštění, ruční zavření si pamatovat (S)
- [BEN-10] menší, Ověření na hardwaru není v programu vidět — `browser.cpp:151-193` → šedý řádek u sestav s deskami V a Nápověda → Jak ověřujeme; stav každé karty později (S; M)
- [BEH-15] menší, Spustit u běžící sestavy ji tiše restartuje — `browser.cpp:100-110, 175` → „Restartovat“ a dotaz u dvojkliku; stav pro „Vrátit“ volitelně později (S)

**Menu, názvy a nastavení**
- [IA-02, NEW-09] menší, Menu kopírují implementaci — `app.cpp:889-1071`, `help.cpp:35-46` → přidávat a přejmenovávat, nepřesouvat (cesty cituje asi 77 řádků ve 26 souborech); konfigurátor do Stroje jen při změně `rozhodnuti.md:1549`, kvůli tomu oponent zamítl BEN-08 (S)
- [IA-05] menší, Ústřední pojem má čtyři jména — `i18n.cpp:21, 193, 229, 231, 233, 381, 400, 450`, [01] [11] → slovníček, „Konfigurátor sestavy…“, „Spustit sestavu ze souboru…“, „Otevřít k úpravám…“; „Stroj“ ponechat (S)
- [IA-03, VIS-12] menší, Expertní ovládání překáží nováčkovi, formuláře nemají jednotné rozložení — [20] (32 checkboxů), [21] (monitor sběrnice), [15] (7 řádků ovládání), [16], `app.cpp:851` → rozbalovací sekce s pamětí, MCP a ffmpeg pod Pokročilé, jeden pomocník pro formuláře; Ladění zůstává (S)
- [IA-13, BEN-07, BEH-21] menší, Nastavení je roztroušené, chybí velikost písma, „Myš PC“ se jako jediná volba neukládá — `app.cpp:814-887, 166-167, 1433-1437`, [16] [17] → velikost písma 100–200 %, „(jen do ukončení)“, sekce Obraz a Záznam; přemapování joysticku a „vzhled monitoru“ (vypnutý, označený) později (S; M)
- [IA-10] menší, Položky přístrojů jsou povolené i pro sestavu, která přístroj nemá, a hláška obsahuje JSON — `app.cpp:1026, 1028`, `aps5.cpp:372`, `ims2.cpp:477`, [ia_04] → povolovat podle typu přístroje, srozumitelná věta (S)
- [IA-16] menší, Chybí Nahlásit chybu a cesta k logu — `help.cpp:35-46, 163-169` → okno s verzí, sestavou a cestami, Kopírovat diagnostiku a Otevřít složku logu; Issues až po zveřejnění (S)
- [BEH-19] kosmetické, Poslední sestavy se plní jen z okna výběru, a to se zavře i při zrušeném startu — `browser.cpp:70-83`, `rack.cpp:330-331` → aktualizovat po každém úspěšném startu a po Uložit jako (S)

**Konfigurátor**
- [BEN-04, IA-04, VIS-04] závažné, Rám odsune propojky pod okraj okna — [chl_01] (1100×760: žádné nastavení), [ia_06], [11] [12], `rack.cpp:1078-1088, 2241, 1338-1340, 2247` → měřítko i podle výšky, posunout na nastavení, pevné záložky, Nová ▸ s názvy; tabulka popisek | hodnota a seznam slotů později (S; M)
- [BEH-03] závažné, Psaní cesty média mění běžící stroj po každé klávese a zakládá prázdná média — `rack.cpp:1533-1536` proti `1501-1510`, `kzd1.cpp:211-219`, `rkd1.cpp:38-51`, soubor `work/kzd-1/beh_w` [beh_04] → potvrzovat Enterem nebo opuštěním pole, médium zakládat jen tlačítkem Nová (S)
- [NEW-04, IA-06, BEH-04] závažné (tvé rozhodnutí z 2026-10-10), Kazeta, TURBO nebo Barva vyvolají „Neuložené změny“ a Uložit přepíše dodanou sestavu — `media.cpp:147-154`, `rack.cpp:176-190, 241-244, 259-306, 501-507, 2140-2146`, [beh_04] → dotaz vyjmenuje změny, Ponechat/Zapomenout, v release kopie do „Moje sestavy“ a Obnovit původní; automatická paměť médií k diskusi (S; M)
- [BEH-07] menší, „Otevřít v konfigurátoru“ zahodí neuložené úpravy bez dotazu — `browser.cpp:184-187`, `rack.cpp:160-174, 2094-2098` → přes `request(PEND_OPEN)` (S)
- [NEW-11 port, IA-17, REV-02, BEH-20] menší, Dodaná SAPI-1 V hlásí žlutě zápis do dvou karet na portu 63h, přestože je to záměr — [11] [12] [13], `docs/img/sapi1v-rat-sestava.png`, `machine_map.cpp:165-173`, `RAM-1V.md:9-10` → katalog označí záměrné sdílení, v mapě šedá informace (S)

**Chování, chyby a data**
- [BEH-05] závažné, Chyby (a některé úspěchy) končí jen v logu — `media.cpp:153, 308-311, 344-347, 378-380, 697-699, 749-750`, `fdc8272.cpp:61-73`, `rack.cpp:192-202`, `app.cpp:1281-1285` → `state_notice`, okno Chyba pro Zapnout, barevný Log, „Chyby: N“ (S)
- [BEH-08] závažné, Diskety, RAM disky a kazety RKD-1 se zapíšou až při vysunutí, vypnutí nebo čistém ukončení — `machine.cpp:25-29, 428-431`, `zrmd1.cpp:340-356`, `floppy.cpp:352-363` → zápis chvíli po posledním zápisu, při pauze a ztrátě fokusu, přes dočasný soubor (S)
- [BEH-06] menší, Nepovedené načtení stavu poškodí běžící relaci — `host.cpp:52-65, 249-253`, `machine.cpp:577, 590-592`, `hrk1.h:35-36` → verze v hlavičce a odmítnutí předem (S), záloha v paměti a starý stroj vypnout až po postavení nového (M)
- [BEH-12, IA-15, VIS-11, MIS-02, A11Y-11] menší, Stavový text přepisuje menu, „emulace“ neříká, že jde o rychlost, složku snímků nic neotevře, upozornění zmizí za 3 s a do logu se nezapíše — [beh_03], `app.cpp:1074-1143`, `shot.cpp:59`, `video.cpp:333` → nepřekrývat menu, „rychlost 100 %“, klikací upozornění, Otevřít složku, 6–8 s a zápis do Logu; spodní stavový řádek s LED mechanik později (S; M)
- [BEH-18] kosmetické, Dvě instance si přepisují data — `app.cpp:1708-1714`, `zrmd1.cpp:340-349` → upozornění při startu druhého GUI, žádný zámek (S)

**Vzhled**
- [A11Y-12, IA-20, VIS-02 průhlednost, PER-11 průhlednost] závažné, Průsvitná okna: prosvítá text oken pod nimi — [07] [10] [11–13] [23] [ia_08] [a11y_20], `app.cpp:1431`, `browser.cpp:215`, `help.cpp:114` → `WindowBg.w = PopupBg.w = 1.0` (S)
- [A11Y-01, VIS-05, A11Y-10] závažné, Ovládací prvky mají kontrast 1,41:1, placeholder 3,4:1, pole ffmpeg nemá popisek — [16] [14] [15] [20], `app.cpp:883-884` → FrameBorderSize 1, Border s alfou 1, TextDisabled 0,60, světlý CheckMark, popisek (S)
- [VIS-02 paleta] menší, Výchozí vzhled Dear ImGui působí jako vývojářský nástroj — `app.cpp:1431-1434` → `ui_theme_apply()` s grafitovou paletou z 5.2, akcent z rámu ARB-1, TitleBgActive #3D6A99 (S)
- [VIS-06] menší, Jedno písmo, jedna velikost a žádná tučná, takže chybí hierarchie — `app.cpp:1437-1438`, `help.cpp:143` → DejaVu Sans Bold, 20/16/13 px přes PushFont (S)
- [VIS-08, A11Y-05] menší, Barvy mění význam a nesou ho samy: nadpis vypadá jako varování, výběr jako kategorie, běžící sestava je jen zelená, Pec 4 a 5 jsou pro deuteranopy stejné, stav pásky je jen ve výplni — `browser.cpp:99-105, 168`, `rack.cpp:61-62, 2243`, `aps5.cpp:46-50`, `media.cpp:434` → pojmenované stavové barvy, ⚠/✖ a slova, „(běží)“, čísla pecí u čar, PLAY/REC textem (S)
- [VIS-07] menší, Popisky jsou převysvětlené: 146 z 610 českých řetězců obsahuje závorku — [11] [16] [20] → zkrátit tam, kde je vysvětlení vedlejší, vysvětlení dát na zalomený řádek nebo do (?) přes `tip()`, „platí hned“ jako odznak (M)
- [VIS-10, NEW-11 cesty] menší, Kolem kreslených panelů jsou obyčejná okna: plné cesty, tmavý „papír“ tiskárny, 32 checkboxů pod pecemi — [14] [25] [20], `media.cpp:862`, `printer.cpp:42-47` → jména souborů, sbalitelná „Simulace poruch“ (LI a/b nechat), světlý papír, panel přístroje do README; čelní panely mechanik později (S; M)
- [VIS-13, BEN-11] menší, Debugger na 8080 ukazuje registry Z80, popisky nejsou zarovnané, v paměti je „�“ — [vis_01] [vis_02], `debugger.cpp:26-27, 61-81, 122-124`, `charset.cpp:80` → registry a příznaky podle CPU, zarovnání, sloupec breakpointů, „.“; mnemoniky Intel, symboly a step out později (S; M)
- [VIS-14] kosmetické, Rámečky oken jsou plné šumu — `app.cpp:537-542`, `rack.cpp:2153, 2181` → WindowMenuButtonPosition None, průhledné úchyty, titulek konfigurátoru z rejstříku, pryč s „|“; zoom v titulku zatím nechat (S)
- [VIS-15] kosmetické, V O programu je logo v rámečku a odkazy vypadají jako tlačítka — [18], `help.cpp:101-105, 143-148`, `browser.cpp:423` → logo s průhledností, TextLinkOpenURL, 28 px tučně (S)
- [VIS-17] kosmetické, Drobné kolize v kresbách — [20] (osa a baterie), [21] (kabely přes popisky, výška) → posunout „0“, DragFloat, pořadí kreslení, výška nejvýš pracovní plocha (S)

**Přístupnost a jazyky**
- [A11Y-06, IA-19, BEH-16] menší, Anglické UI ukazuje češtinu: titulek okna, Poslední sestavy, podtitulek, „APS-5 klíč“, „PORUCHA“, „Pec 1–8“, náhledy, filtry dialogů, chyby jádra — `app.cpp:331, 898`, `browser.cpp:171`, `hrk1.cpp:161`, `furnaces.cpp:24-31`, `media.cpp:24-37`, [26] [a11y_20] [a11y_21] → `machine_title()`, `tr()` v GUI, pravidlo „natištěné jak je + anglický tooltip“ (S; chyby jádra M)
- [A11Y-08] menší, IMS-2 je přeložené napůl, PROTECTION naráží na MAINS, combo je uříznuté — `ims2.cpp:218-240, 327, 339-355, 529`, [a11y_21] → popisky jako na přístroji s tooltipem, šířka podle textu (S)
- [A11Y-09] menší, Česká hlášení obsahují anglické kousky a překlady hlídá jen test duplicit — `machine_map.cpp:145-173`, `ram1v.cpp:164`, 13 nepoužívaných položek → překládat `what` v GUI, ctest na chybějící a nepoužité klíče (S)
- [A11Y-04] menší, Čtečky obrazovky nedostanou nic a text obrazovky nejde zkopírovat — [07], `headless.cpp:12` → Kopírovat text obrazovky (Ctrl+Shift+C), poctivá věta v README (S)
- [A11Y-07] menší, Důležitá vysvětlení jsou jen v tooltipech, ty jsou na jednom řádku až 1470 px — 97× `SetTooltip`, `app.cpp:980, 1141`, `debugger.cpp:160` → `tip()` se zalomením, viditelné popisky klíčových věcí (S)
- [A11Y-13] kosmetické, Hledání vyžaduje přesnou diakritiku, chybný tvar „%zu paketů“ — `browser.cpp:85-93`, `i18n.cpp:639` → hledat bez diakritiky, „paketů: %zu“; desetinná čárka je tvoje volba (S)
- [A11Y-14] kosmetické, Chybí upozornění na blikání v demech — `sapi1v_hry.en.md:14` → jedna věta česky a anglicky (S)

**Prezentace, README a vnímání**
- [PER-01] kritické před zveřejněním, Nikde veřejně není, že program vzniká s AI, přestože ji uvádí skoro každý commit — README:10-11 a 19, [18], `git log` → odstavec tvými slovy, řádek v O programu, v poznámkách k vydání a v oznámeních; `Co-Authored-By` ponechat (S)
- [PER-02] závažné, Nejsilnější důkaz, měření na skutečném hardwaru, není vidět — README:9-10, 194; `docs/img` nemá fotky → sekce „Měřeno na skutečném SAPI-1 V“ s fotkou, snímkem z Logic 2 a čísly (S)
- [PER-03] závažné, Přední stránce chybí lidský hlas — README a První kroky bez první osoby, motivace je jen v `zadani.md` (dnes `docs/vznik.md`) → 120–200 slov sám, „Kdo za tím je“ (S)
- [PER-04, BEN-12, NEW-10] závažné, README je katalog o 2 642 slovech, anglická část je seznam zkratek a v zipu je jen český PRVNI-KROKY.md — README:5-11, 13-26, 49-156, `package.py:74` → asi 110 řádků, „Co umí“ do `docs/funkce.md`, `README.en.md`, „Tesla SAPI-1“, anglický Getting started; snímky nechat za odkazem na stažení (M)
- [PER-05] závažné, Žádné video ani GIF — README:28-38 → GIF nahoře, 2–3 minuty skutečného SAPI-1 V vedle emulátoru (M)
- [PER-07] závažné, Chvála přátel není test a testeři nemají kam hlásit (soukromé repo, Issues vrací 404) — `SOUHRN.md:592-593`, README:47, `prvni-kroky.md:4`, `help.cpp:163` → kanál pro hlášení, úkoly a otázky, dva lidé zvenku, desetivteřinový test, šablona issue (S)
- [PER-06] menší, Chybí plán oznámení → nejdřív česká a slovenská scéna, pak VCFed, r/retrocomputing, Hackaday, Show HN; SHA-256 (S)
- [PER-08] menší, Vývojové poznámky působí jako zápisník AI a `CLAUDE.md` obsahuje soukromé cesty — „(autor)“ asi 80×, „autor schválil“ 17×, `CLAUDE.md:30-35, 104, 135-138` → hlavička poznámek, z `CLAUDE.md` vyndat jméno sdílené složky a cesty na H:, `docs/vznik.md`, „Hledáme pomoc“ (S)
- [PER-09] menší, MCP je v prvním odstavci README — README:10-11, 19 → přesunout do „Pro vývojáře“ (S)
- [PER-10] menší, Obrázky v README ukazují „-- %“ a staré tlačítko, minigraf-ukazka.png má 3,87 MB — `docs/img/*` → přegenerovat, rychlost v režimu snímku vynechat (nepředstírat), zmenšit, bod v kontrolním seznamu (S)
- [PER-11 snímky] menší, Sdílené snímky ukazují výchozí ImGui a zbytečné chyby — [11] [12] [10] [20] [23] [08] → průhlednost a styl jako výše, připravené snímky s čistým rozložením (S)
- [PER-12] menší, Historie commitů vypadá strojově: průměrný předmět 181 znaků, „the author“ → krátké a neutrální předměty, pravidlo v `CLAUDE.md`, historii nepřepisovat (S)

## Co tahle revize neprověřila

- **Běh v reálném čase:** zvuk (latence, praskání, ztlumení mimo 100 %), gamepady, odezva vstupu, zatížení CPU a baterie notebooku, dlouhé sezení. Snímky běží skryté a rychlost neměří.
- **Škálování nad 100 % a víc monitorů:** jen čtením kódu a dokumentace SDL, nikdo to nespustil. Proto je test součástí bodu 1.
- **Menu a debugger:** menu nešla na snímku otevřít, posuzovala se jen podle kódu. Debugger má dva extra snímky.
- **Linux (WSL):** písma, dialogy pro soubory, DPI.
- **Balíček a první spuštění ze zipu:**
  - velikost a struktura zipu;
  - SmartScreen a antiviry;
  - zápis do `%LOCALAPPDATA%`, když se do složky programu zapisovat nedá;
  - přechod z alfy s existujícím `imgui.ini` a nastavením.
- **Systémové pomůcky:** skutečná čtečka obrazovky, režim vysokého kontrastu Windows a systémová velikost textu.
- **Sestavy:** recenzenti prošli hlavně 17 sestav s názvem, ne 43 ve skupině Ostatní.
- **Funkčně nevyzkoušené:**
  - sériový terminál (TCP, COM, XMODEM);
  - export z tiskárny a zapisovače;
  - nahrávání videa, včetně otázky, odkud se bere ffmpeg;
  - síťové funkce DPI-1L (relay, Repository API) z pohledu souhlasu a soukromí.
- **Test se skutečnými lidmi neproběhl.** Všechno výše je expertní odhad, potvrdí ho až beta podle bodu 9.

## Podklady

Popis galerie: out\ux-review-20261028\gallery.md

Citované snímky galerie:
- [01] out\ux-review-20261028\01_first_start_cs.png
- [02] out\ux-review-20261028\02_first_start_en.png
- [04] out\ux-review-20261028\04_zps1_guide_cs.png
- [05] out\ux-review-20261028\05_zps1_main_cs.png
- [07] out\ux-review-20261028\07_zps1_screen_menu_cs.png
- [08] out\ux-review-20261028\08_hry_flappy_cs.png
- [09] out\ux-review-20261028\09_hry_flappy_1280_cs.png
- [10] out\ux-review-20261028\10_hry_guide_en.png
- [11] out\ux-review-20261028\11_rack_sapi1v_cs.png
- [12] out\ux-review-20261028\12_rack_cga_en.png
- [13] out\ux-review-20261028\13_map_cs.png
- [14] out\ux-review-20261028\14_media_fdc1_cs.png
- [15] out\ux-review-20261028\15_terminal_cs.png
- [16] out\ux-review-20261028\16_settings_cs.png
- [17] out\ux-review-20261028\17_settings_en.png
- [18] out\ux-review-20261028\18_about_en.png
- [19] out\ux-review-20261028\19_shortcuts_cs.png
- [20] out\ux-review-20261028\20_aps5_cs.png
- [21] out\ux-review-20261028\21_ims2_cs.png
- [22] out\ux-review-20261028\22_minigraf_cs.png
- [23] out\ux-review-20261028\23_manic_diag_cs.png
- [24] out\ux-review-20261028\24_zps6_cs.png
- [25] out\ux-review-20261028\25_cpm_printer_cs.png
- [26] out\ux-review-20261028\26_browser_hry_en.png

Citované extra snímky:
- [new_02] out\ux-review-20261028\extra\new_02_hry_1100x760_cs.png
- [new_03] out\ux-review-20261028\extra\new_03_hry_guide_1100x760_en.png
- [new_05] out\ux-review-20261028\extra\new_05_hry_poweron_en.png
- [new_06] out\ux-review-20261028\extra\new_06_aps5_poweron_en.png
- [new_08] out\ux-review-20261028\extra\new_08_jpr1s_guide_poweron_en.png
- [ia_04] out\ux-review-20261028\extra\ia_04_ims2_machine_with_aps5_window_en.png
- [ia_06] out\ux-review-20261028\extra\ia_06_rack_1280_cs.png
- [ia_07] out\ux-review-20261028\extra\ia_07_minigraf_with_guide_cs.png
- [ia_08] out\ux-review-20261028\extra\ia_08_aps5_default_window_1100_cs.png
- [beh_03] out\ux-review-20261028\extra\beh_03_status_overlap_1100_cs.png
- [beh_04] out\ux-review-20261028\extra\beh_04_kzd_path_live_en.png
- [chl_01] out\ux-review-20261028\extra\chl_01_rack_default_1100x760_cs.png
- [vis_01] out\ux-review-20261028\extra\vis_01_debugger_zps1_cs.png
- [vis_02] out\ux-review-20261028\extra\vis_02_debugger_only_zps1_cs.png
- [a11y_20] out\ux-review-20261028\extra\a11y_20_aps5_en.png
- [a11y_21] out\ux-review-20261028\extra\a11y_21_ims2_en.png

Výpočet kontrastů palety z 5.2: out\ux-review-20261028\contrast.py

Testy recenzentů nechaly v gitignorované kopii dat dva soubory. Zmizí spolu s celou složkou náhledu:
- E:\SAPI_GIT\SAPIemu\out\previews\20261028-150939\work\kzd-1\beh_w (28 B)
- E:\SAPI_GIT\SAPIemu\out\previews\20261028-150939\work\snimky\AND-1_20261028-152131.png

Repo jsem jen četl, nic jsem v něm neměnil.