# Otázky k ověření na skutečném HW

**Generováno** skriptem `python tools/hw_otazky.py` ze sekcí „Nejasnosti“ v `docs/` (needitovat ručně,
otázky se doplňují a odškrtávají v dokumentech desek a skript se spustí znovu). Vyřešené body (přeškrtnuté
nebo „vyřešeno“) jsou vynechané. Podklad pro společné dávkové ověřování na skutečném SAPI-1.

Otevřených bodů: **238** v 49 sekcích. Desky jsou první, pak ostatní témata (SOUHRN, software, tiskárna…).

## [ADP-1 – deska A/D převodníku](desky/ADP-1.md) – Nejasnosti (1)

- Doba převodu C 520 D na desce, hodnota bitů 5 a 6 stavového registru (emulace 0), co udělá /RES s registry
  74173 a s READY (emulace vše 0).


## [AND-1 – původní alfanumerický displej](desky/AND-1.md) – Nejasnosti (2)

- Skutečné skládání sloupců 32–63 do 1 KB (D4C) a frekvence blikání.

- Ověřit překreslený font na fotce obrazovky skutečné AND-1.


## [AND-1A – alfanumerický displej pro ZPS-3](desky/AND-1A.md) – Nejasnosti (4)

- Dump MHB 2501 (stejná mezera jako u AND-1, font je překreslený).

- Video RAM 1 KB, nebo 2 KB? Návod píše 2 KB, rozpiska má 2× 2114 (1 KB) a parametry 1 KB. Co se čte
  z E800–EFFF mimo viditelné buňky?

- PROM D3A pro F000, kterou potřebuje monitor IMA 64K (`videom equ 0F000h` v MON64C3), a pro F800 (CP/M 62K).
  Emulace má pro obě předpoklad (`AND-1A_D3A_CPM64K_74S287.bin`, `…_CPM62K_…`).

- „EB00H–EFFFH“ v návodu DGD-1 je nejspíš chyba OCR místo E800H.


## [AND-1B – náhrada AND-1 / AND-1A s výstupem VGA (Libor Lasota)](desky/AND-1B.md) – 9. Nejasnosti (8)

1. **Font ve skutečných deskách:** je vypálený `FontAND1B11.bmp.bin` (kompilátor „rev. 1.1“, deska rev 1.4)? Na
   webu jsou „Fonty“ bez odkazu. Fotky obrazovky [B7] se s fontem shodují (`¤` na 24h, podtržení, široké znaky).
   Autorova deska rev 1.4 má EPROM, kterou si autor vypálil sám z `FontAND1B11` (2026-10-13), fotka obrazovky
   s fontem sedí. Jak je to u desek od Libora, zůstává otevřené.

3. **Skládání sloupců 32–63:** na AND-1B ověřeno 2026-10-13 (`and1b.asm`, fotka obrazovky proti emulátoru):
   sloupce 40–47 i 56–63 zrcadlí 32–39 a řádky 24–31, sloupce 16–23 jsou sloupce 32–39 řádků 16–23, přesně podle
   vzorce v kap. 4. Otevřené zůstává, jestli se tak skládá i skutečná AND-1 (D4C), pak opravit i předpoklad
   v `And1::vram_index` (liší se jen neviditelné zrcadlení).

4. Polohu obrazu (okraje 40 bodů VGA) mám jen spočtenou z pipeline, mohou se lišit o pár bodů. Na monitoru se stejně
   automaticky upraví.

5. S3 (TS oscilátoru): na fotce rev 1.4 nepropojená (H/L). Pro emulaci nemá význam (L by oscilátor vypnula).
   starší než 1.4 s jinými propojkami (adresa, INT1, barva), zeptat se Libora.

6. Výchozí barva sestavy: autorova deska je GRN, AND-1V má v emulátoru výchozí bílou. Rozhodne autor.

7. INT1 50Hz režim dává nepravidelné intervaly (4× 16,6 ms + 1× 33,3 ms). Programy počítající čas (MIKROBASIC,
   hodiny) to vidí jako 50 Hz v průměru. **Změřeno na autorově desce 2026-10-13** (`and1b_int.py`): impulz 317,0 µs,
   intervaly 4× 16,641 ms a 1× 33,283 ms, přesně jako schéma a emulátor. Zbývá potvrdit, že Libor nic jiného
   nezamýšlel.

8. BOM uvádí BAT42, schéma BAT41 (bez vlivu).

9. Žádná propojka 20/24 řádků ani volba MHB 2502 (na rozdíl od AND-1/AND-1A): pevně 24 řádků, font jen
   z EPROM.


## [AND-1Z / AND-1Z-89 – alfanumerický displej](desky/AND-1Z-89.md) – Nejasnosti (ověřit na HW) (2)

- AND-1Z: vstup A6 DA1 je vyřešený, nastavuje ho přepínač S1 (viz výše).

- Zapojení neoznačených vstupů DD4 A7 a DA1 A6.


## [APS-5 – měřicí jednotka (regulace osmi pecí)](desky/APS-5.md) – Nejasnosti k ověření na HW (6)

- **P2-IN7 = /PFAIL ze zdroje ZDR-1A.**
  - Je odvozeno z firmwaru:
    - R7 = RST 0 vede přes 003Bh na uložení stavu;
    - po resetu firmware na 01FCh čeká na L, jinak hlásí CHYBA;
    - při povoleném přerušení musí být H.
  - Kudy signál vede (přes HRK-1B?), schéma kabelů neříká.

- **Kódy kláves:** překreslené schéma HRK-1B (EC1045) má na D11–D13 „<-“, „M“, „->“ a na D14–D16 R1–R3. Tak by
  ← = 0Ah, M = 0Bh, → = 0Ch. Firmware ale bere 0Dh jako ←, 0Fh jako → a 0Bh jako R. Emulace se řídí
  firmwarem, takže skupiny jsou prohozené (nebo MH 1KK1 kóduje jinak).

- Kabel RTC-1 XC2 na JPR-1 X3: OUT0 DD5 → P2-IN5, OUT1 → P2-IN4 (podle obsluh přerušení), CLK0/CLK1 DD5 =
  XC2/29 (podle frekvencí, které firmware předpokládá).

- /RTL z HRK-1B (XC2/20) vede asi na sběrnici, emulace resetuje celou jednotku.

- Doba převodu C 520 D (emulace 20 ms), bity 5 a 6 stavu ADP-1 (emulace 0), stav ADP-1 po /RES.

- Zesílení DMT-1 v emulaci odpovídá tabulce firmwaru (100, 199, 498, 1000). Skutečná deska se nastavuje trimrem RP3.


## [Plotter z AR 1990 – souřadnicový zapisovač řízený JPR-1](desky/AR1990.md) – Nejasnosti (5)

- **Manuál a program se rozcházejí:** manuál uvádí zrušení zmenšeného tisku 09H 09H **08H**, program porovnává
  s **0BH** (komentář „tentokrat $0B“). Emulace dělá to, co program. Přepis v archivu má 08H.

- **Doraz:** DRZ při pohybu vpravo testuje bit 1 (podle Tab. 1 levý doraz) a vlevo bit 0 (pravý). Buď je tabulka,
  nebo program obráceně. S Minigrafem (bez spínačů) se to neprojeví.

- **LED BUFF a READY:** komentář v BUFE říká „rozsviť diodu READY“ u bitu D6, Tab. 1 a manuál mají D6 = BUFF
  (svítí trvale = prázdný buffer) a D7 = READY. Emulace se drží tabulky a manuálu.

- Polarita RDP a RDA na straně počítače se volí přepínači. V emulaci je jen úroveň, kterou vidí program.

- Ruční oprava FAST ve skenu: kdo ji dopsal a jestli se dostala do nějaké EPROM.


## [BGP-1S – grafika kompatibilní s PMD-85 (Libor Lasota)](desky/BGP-1S.md) – 9. Nejasnosti (7)

1. Výchozí S1 (fotka: ACE) a S2 (fotka: kolíky u „INT0“): potvrdit.

2. Stav klopného obvodu přerušení IC14A po zapnutí (není nulovaný /RES). Emulace začíná bez požadavku.

3. Čte Manic Miner videoram (je stínová RAM C000–FFFF povinná)? Pro „auto“ ano.

4. Přesné vzorkování RDY u 8080 vs. okno s5 (počet čekacích taktů při 2 MHz): stačí model „čekat do konce s5“?

5. Je `.sch` v repu poslední uložení? PNG jsou z 05.03.2022 (C15 100n, nový list 6), `.sch` z 09.02.2022. Hodilo
   by se nahrát novější `.sch`.

6. List 6 schématu: popisek „143 ns (1/7 MHz)“ je ze starší revize (krystal je 6,5536 MHz), zůstal i v PNG
   z 05.03.2022.

7. Kdo napsal hru „auto“?


## [CGA-1V – barevná grafika 320×200 s výstupem VGA (Libor Lasota)](desky/CGA-1V.md) – Nejasnosti k ověření (3)

- `..\SAPI-karty\CGA-1V\RADICROM.bin` (2 KB, sestupná tabulka FF FE FD…) se k CGA-1V nevztahuje (deska nemá PROM). Je
  to omylem, nebo k jiné desce?

- **INTA s PC v okně karty** (L. Lasota 2026-10-10, ověřeno pokusem): STSTB z JPR-1V při INTA s adresou v okně
  nastaví IC9B a RDY se už neuvolní (žádné /READ ani /WRITE). RESET to nevyřeší (JPR-1V ho vzorkuje s /M1), viz
  `JPR-1V.md`. **Změřeno na desce autora 2026-10-11 a emulováno** (`docs/zmeny-podle-hw.md`). Zbývá ověřit po
  Liborově záplatě IC15 na JPR-1V.

- Samostatný dokument „Programování karty“ (datasheet str. 7 na něj odkazuje) zatím nemáme.


## [CRM-1 – deska zálohovaných pamětí](desky/CRM-1.md) – Nejasnosti k ověření na HW (3)

Autor desku nemá, ověření je možné jen na cizím kusu.

1. Skutečná délka impulzu DD6 (R10 5k6, C1 150 pF, tolerance 74121): 1, nebo 2 WAIT při 2 MHz?

2. Co přesně dělá SA1 na neopravené desce z výroby (blokuje jiný kilobajt, nic, nebo celé čtení)?


## [DBS-1A – deska barevné semigrafiky SAPI-1.5](desky/DBS-1A.md) – Nejasnosti k ověření na HW (6)

1. Pořadí R a G v atributu (atributy 02h a 04h na X2/16 a X2/20).

2. Blikání: bit 7 = 1 bliká a blikající buňka ukáže pozadí (ne prohození barev)?

3. Horní poloviny PROM DD2 a DD27 (C000–F000, E8–EF): tabulky propojek a výpisy PROM v návodu se liší; ideálně
   vyčíst DD2 („53“) a DD27 („61“).

4. XT7 38-39 = 8 bodů? XT5 29-32 (JP32 je VCC)? Je Y vždy bit 3 atributu?

6. Časování ROM BIOSu (15,0 kHz / 46,3 Hz): existovala DBS-1 s jiným krystalem, nebo to monitor snesl?

7. Kterou propojku XT5 předpokládá CP/M Melsoft (bit 3 atributu jako výběr sady potřebuje XT5 28-29 nebo 2764)?


## [DGD-1 – deska grafického displeje](desky/DGD-1.md) – Nejasnosti (4)

- Které vstupy má PROM D1A (A15, A14, MAP1, MAP2, SP1…)? Obsah dumpu nesedí na jednoduché přiřazení.

- Délka okna CPU v periodě 1 µs (počet čekacích stavů).

- Chování s mapováním (aktivní úrovně MAP1/MAP2 na sběrnici).

- Má autor DGD-1 nebo fotku obrazovky s grafikou pro porovnání (poměr stran, polarita)?


## [DGD-1V – grafický displej 320×240 s výstupem VGA (Libor Lasota)](desky/DGD-1V.md) – Nejasnosti (3)

- Přesný počet čekacích taktů (synchronizace IC4A/B).

- Výchozí adresa C000h jako DGD-1 (autor), sestava stejně.

- **INTA s PC v okně karty** (L. Lasota 2026-10-10): stejně jako u CGA-1V a AND-1V by STSTB při INTA
  nastavil RDY bez žádosti CPU a sběrnice by stála, viz `JPR-1V.md`. U CGA-1V a AND-1V změřeno 2026-10-11;
  emulace to dělá i pro DGD-1V (podle schématu stejné čekání). Na desce DGD-1V ještě ověřit. U původní DGD-1 se
  zatím neemuluje.


## [DPP-1 – deska paralelních portů](desky/DPP-1.md) – Nejasnosti k ověření na HW (2)

- Přepínače S2–S4 při připojení tiskárny: jaké nastavení a jaký kabel se používaly?

- Kanál 1: děrovač (`PUN:`), nebo tiskárna přes `UL1:`? Zapojení signálů (STROBE, BUSY, ACK) na 1PA/1PC
  (emulace: /OBF → /STROBE, /ACK → PC6, BUSY nezapojený).


## [DPS-1 – deska přístrojové sběrnice IMS-2](desky/DPS-1.md) – Nejasnosti (6)

- **Předpoklad LIST/TALK → vlastní NDAC** (viz Emulace): jak je to na skutečné desce zapojené? Schéma Tesly
  i EC1045 vede READ (PC7 D2C) jen do D2B. Funguje replika EC1045 s BASIMS jako mluvčí (`IMS# O`)?

- **/OBF B a /ACK B:** nuluje MHB8255A /OBF úrovní /ACK (datasheet Intel), nebo hranou? Při úrovni se ERR nenahodí
  nikdy a zápis bez posluchače zmizí. Jak se deska chová bez přístroje na sběrnici?

- **Bajt 00h s ATN** mezi zápisy B0h a 03h do 37h (viz Emulace): je ho na skutečné sběrnici vidět (logický
  analyzátor na DAV)?

- **M1T 330:**
  - odpověď končí CR LF s EOI na LF (odhad podle BM 595 [V1]; BASIC to snese);
  - tvar odpovědi na Q (emulace: `%+d.ddddE±e`, chyba v %);
  - přetečení (emulace: `V+9.9999E+9`, displej OL);
  - doby měření, autokalibrace a rozsahů;
  - chování REP v dálkovém režimu;
  - popisky tlačítek podle fotografie (SÍŤ, AUTO, FILTR, START REP/1x, CHYBA);
  - zda `R4` opravdu jede od nejmenšího rozsahu („přepnutí z minimálního na maximální rozsah“ [V1]), emulace mění
    rozsah po krocích od posledního.

- **BS 525:** rozmístění ovládacích prvků na panelu (nakreslené podle popisu v návodu, ne podle fotografie).

- **PROM D3D:** obsah z návodu je zjevně jen ukázka. Měl někdo skutečné kódy zpráv (UNL, DCL…)?


## [DSM-1 – sériové rozhraní s kazetou](desky/DSM-1.md) – Nejasnosti (7)

- Pořadí bitů délky slova (b3–2) v registru řízení UART. OCR uvádí „10“ = 6 bitů.

- **Reset UARTu (MR):** DSM-1V ho drží do prvního čtení paměti na sběrnici (/MR), změřeno 2026-10-11
  (`docs/zmeny-podle-hw.md`). Má podobné prodloužení MR i původní DSM-1 a DMR-1 (MHB1012)? Ve schématech
  zkontrolovat, emulace je zatím bez něj.

- **Převzetí znaku do vysílacího registru:** 6402 na DSM-1V ho dělá až na hraně taktu 16× (změřeno 2026-10-11).
  MHB1012 (AY-5-1013) nejspíš stejně, změřit na skutečné DSM-1 (autor: TTL signály jen na čipu UART).

- Stav po resetu: dokumentace uvádí „DR=1“, což je pravděpodobně chyba OCR.

- Jak přesně skutečný obvod rozhoduje bity a jak se chová na začátku úseku (emulace: fázový dekodér, viz výše).

- Co dělá výstup do magnetofonu, když je KAZ = 0 (emulace: klidový tón).

- Kazeta z programu přehraná z PC do skutečné DSM-1: stačí obdélník z WAV, jaké jsou minimální mezery pro
  skutečný magnetofon (`docs/software/kazeta.md`, kapitola 5)?


## [DSM-1V – dva sériové kanály (Libor Lasota, podle DSM-1, bez kazety)](desky/DSM-1V.md) – Nejasnosti (1)

- Modem (+0) podle stavu linky: s otevřeným COM13 (MOXA drží DTR, RTS) 10h, se zavřeným F0h.


## [DSP-1 – deska dvou sériových kanálů](desky/DSP-1.md) – Nejasnosti k ověření na HW (3)

1. Pořadí bitů A5/A6 na přepínači SA2 (návod a replika se liší).

2. Úroveň výstupu 75154 při nezapojeném vstupu (/CTS, /DSR, RxD bez kabelu): emulace bez kabelu `cts = false`.

3. Deska v. č. 44127 na fotce má SA1 2, 7, 8 a SA2 jinak než J. Bíba: s čím byla používaná?


## [FDC-1 – řadič pružných disků (FDC-1A + FDC-1B)](desky/FDC-1.md) – Nejasnosti k ověření na HW (7)

- Polarita D0 přepínačů SA2 (DD/SD) při čtení 0EH a k čemu je dvoupólový přepínač SA1 na FDC-1B (návod mluví
  o jednom „DIL přepínači“ čteném na D0 a D1). **EC1045 (2026-10-07):** na FDC-1A/B je jediný osmipólový DIP, SA2
  na FDC-1B. SA1 (DIP2) je na jeho FDC-1B nahrazený propojkou.

- Hlásí 8272 na FDC-1 po resetu „ready changed“ pro všechny 4 jednotky? (MOR to snese, bylo by dobré to ověřit
  sekvencí SENSE INTERRUPT STATUS po resetu.)

- Kolik taktů CPU zabere jeden přenos DMA (emulace 4).

- Registr 0FH (motory, hlavy) a výběr 0EH D1 D0 vedou jen na X3 FDC-1B. Motory zapínají BIOSy pro mechaniky PC
  (EC1045, dlouhé CP/M J. Bíby), formátovací programy SAPIMANa a FORM6. Vedly /M0–/M3 z X3 k mechanikám PC
  (MOTOR ON), a jak? Nuluje /RES registr 0FH (emulace ano)? Jak dlouho se mechaniky rozbíhají (emulace 500 ms)?
  **EC1045 (2026-10-06):** mechaniky 5,25" HD připojoval přímým kabelem k FDC-1A jako 8": MOTOR ON mechaniky
  z /HL (přiklopení hlavy 8272), READY mechaniky nezapojený (/RDY uzemněný), /M0–/M3 nepoužité. Motor se u něj
  tedy točí, jen když 8272 přiklopí hlavu. Emulace (`motor_line`) zapíná motory registrem 0FH, jak to dělají
  BIOSy J. Bíby a EC1045; jeho zapojení je od 2026-10-07 volba `motor_hl` (sestava `cpm_ec1045`).

- Doba RQM po bajtu povelu (emulace 12 µs).

- Volí přepínač SA2 S1–S4 („8"“) hodiny 8272 (8 MHz pro 8" a mechaniky HD, 4 MHz pro 5,25" DD)? **EC1045
  (2026-10-06):** pro mechaniky HD má SA2 stejně jako pro 8", `11000011` (krajní dva na obou stranách sepnuté),
  signál hustoty (kolík 2 mechaniky) nezapojený. **Upřesnění 2026-10-07:** sepnuté jsou dva krajní spínače
  na každé straně, zbytek rozepnutý. Nastavení je souměrné, na směru počítání tedy nezáleží. S naším přiřazením (S1–S4 = 8" pro A–D, S5–S8 = DD) to je A, B
  = 8" SD, proto má `cpm_ec1045` od 2026-10-07 `sa2_a`/`sa2_b` = `8sd`. Jeho BIOS port 0EH nečte. Které číslo
  přepínače patří které mechanice, je dál odhad.

- Kam až dojede hlava použitých mechanik (8" Consul, 5,25" a 3,5" PC)? Emulace má doraz na stopě 82. Programy
  SAPIMANa formátují stopy 0–80.


## [IDE-1 – IDE disk + připojení PMD32-SD](desky/IDE-1.md) – Nejasnosti (3)

- Po resetu je PC 8255 vstup a vstupy 7406 mohou „plavat“ do H, takže by IDE signály byly aktivní. Není jisté, zda R8 a R9 jsou pull-upy. Emulátor zatím bere signály jako neaktivní.

- Zapojení lišty SV2 je jen odečtené z PDF.

- Zdroje OFFSHDD.COM a BIOSu 2015 („BI-3K V050815JB“) nejsou k dispozici.


## [JPR-1 – původní procesorová deska (8080)](desky/JPR-1.md) – Nejasnosti (4)

- Které RST dá která úroveň 3214 (odvozeno `RST 7 − úroveň`; APS-5 to nerozliší, všechny vektory jdou na
  jednu obsluhu). Význam D3 = 1 (emulace: všechny úrovně) firmware APS-5 nepoužívá.

- Manuál uvádí 2. pozici 2708 na 0400, dekodér pro 2716 dává 0800 (emulace: 2708 po 1 KB, 2716 po 2 KB).

- Co CPU čte z prázdných patic a z 3000h (emulace: prázdné patice = sběrnice, 3000h = FFh).

- Polarita LED na P1-OUT D7 (emulace: 1 = svítí, LED na panelu v konfigurátoru).


## [JPR-1A – procesorová deska 8080 pro ZPS-3](desky/JPR-1A.md) – Nejasnosti (3)

- Fyzický dump PROM D2C (zatím tabulka z návodu).

- Rozpiska osazuje jen **D4B**, ale OE1 pro 0000 vybírá **D3B**. Ve které patici je boot EPROM (fotka nebo HW)?

- Jak jsou nastavené propojky na skutečné desce (JP1, přerušení)?


## [JPR-1S – moderní reinkarnace procesorové karty JPR-1 (Libor Lasota)](desky/JPR-1S.md) – 8. Nejasnosti (8)

1. Propojky na autorově desce: fotka rev. 2.2 ukazuje RAM_EN 1+2+3 osazené, INT_SRC = INT1, INT_ADR = RST7. Je to
   aktuální stav? Je sestava JPR-1S + BGP-1S na C000 (+ AND-1V)? Který BIOS je teď v PicoROM?

2. Redukce: ~~dvě různé desky, nebo dva názvy?~~ Vyřešeno z čísel desek: první Redukce34KB01 (DPLA0014A5R) byla
   prostá, Redukce34REV (DPLA0026COM) a Redukce34KB01 rev 2.0 (DPLA0045R34) mají sériový vstup. Zbývá: jaký
   převodník se používá na straně PC (COM, USB-RS232) a jaký program posílá soubor?

3. Deska diagnostiky: zapojení je vyřešené (segmenty a–g, dp = bity 0–7). Otázka je jen, zda ji emulovat (autor
   ji asi má, fotka `diagnostika.jpg`).

4. Manic Miner: ovládání je odvozené z kódu (O/P, SHIFT, S pauza, CR, SHIFT+LF konec; v emulátoru s režimem
   pro hry Ctrl+F7, Shift na PC = SHIFT). Jestli čte zpět videoram
   (tj. jestli je RAM pod BGP-1S povinná), se nezjistilo. S RAM pod kartou v emulátoru běží.

5. Kam vede K1 /RTL na skutečné sestavě (tlačítko reset)? Na desce tlačítko reset není, ANK-1 RES na K2 nevede.

6. IC4: 74HCT04 (2021) / 74LS04 (2.2) vs. 74HCT14 [WEB]. Jen nesoulad v dokumentaci, pro emulaci nepodstatné.

7. Starší revize 2021: podle netlistu z eaglecon logicky shodná (DIP SW1 místo propojek). Potisk „2000h PORT2“
   na malé fotce `mJPR-1S.jpg` je nejspíš jen nečitelné „2C00h“ (osazovák 2021 má 2C00h).

8. `TEST AND1/AND1A/AND1B` je v 2.1 prázdný (`ret`). Asi záměr.


## [JPR-1V – procesorová deska Z80 (Libor Lasota, upgrade JPR-1Z)](desky/JPR-1V.md) – Nejasnosti k ověření (2)

- **Záplata STSTB při INTA** (L. Lasota, 2026-10-10): přesné zapojení (přeškrab, IC15 piny 3–4) a které karty
  STSTB při I/O potřebují. Podle [S4] je STSTB z /IORQ oprava pro mapování, po záplatě se to musí ověřit
  s RAM-1V (registr MAP 63h), DSM-1V a ostatními.

- **FXS4 a přepisované definice znaků:** na Liborově sestavě se po čase přepisuje font FXS4 a nakonec se program
  zasekne. Podle Libora se v 1.0.1 přerušení přijalo s PC > BFFFh. V SAPIemu PC při INTA nad 904Fh nebylo
  (9. 10.). Zkouška fontu: viz `docs/SOUHRN.md` (2026-10-10).


## [JPR-1X – procesorová deska 80C85 (Libor Lasota, rev 1.0, 2025)](desky/JPR-1X.md) – Nejasnosti k ověření (HW / autor) (8)

3. Rezistor na zadní straně: podle článku na 8bity.cz pull-up 4k7 na D3, protože některé Mitsubishi 8085 při čtení
   prázdné adresy vracely dříve zapsaná data (jiní výrobci spodní bajt adresy) a Monitor pak špatně zjišťoval
   paměť. Emulátor vrací z prázdné sběrnice FFh. Ověřit vývod a hodnotu.

4. Pořadí RST pro úrovně 3214 (předpoklad RST 7 − úroveň), význam /B0–/B2 a /SGS (aktivní v 0?) – společné s JPR-1.

5. Je /INT 3214 skutečně krátký impuls (zachycení v IC7) a ovlivňuje /ELR jen výstupy A0–A2, nebo i /INT?

6. DMA: IC26 budí A0–A7 i při HLDA. Funguje DMA jen s ARB-1C (JP3 2-3, /AEN odpojí slot CPU)?

7. Kam vede /RTL K1-1 v autorově sestavě (tlačítko reset)?

8. Které „výrobcem doporučené úpravy“ 3214 [DESC] myslí a proč je doporučená klávesa INT na R6 místo R7
   (R7 = RST 0 = restart?).

9. Snese M5M80C85AP-2 4 MHz (X1 8 MHz)? Článek: všechny vyzkoušené 8085 (Toshiba, Intel, OKI, Mitsubishi) běží na
   4 MHz spolehlivě.

10. Procesor 8085: příznaky V a K, DSUB/ARHL a časování přijetí RST 7.5/TRAP (`docs/8085.md`, Nejasnosti).


## [JPR-1Z – procesorová deska Z80 pro ZPS-4](desky/JPR-1Z.md) – Nejasnosti (4)

- Originální boot EPROM ZPS-4 (Tesla IMA) a systémová disketa.

- **Čip Z80: NMOS, nebo CMOS?** Parametr `z80` (výchozí `nmos`, v ZPS-4 nejspíš NMOS U880 nebo Z80A). Viz JPR-1V.

- **STSTB při INTA a RESET jen s /M1** (L. Lasota 2026-10-10 píše o JPR-1Z/1V): dává JPR-1Z STSTB i od /IORQ
  jako JPR-1V a je RESET synchronizovaný s /M1? Záplata pro refresh na JPR-1Z podle Libora je. Popis v
  `JPR-1V.md`. U JPR-1V změřeno a emulováno (2026-10-11), JPR-1Z v emulaci zatím ne.

- Která ROM ELIS patří k disketě ELIS (v3.00 má C262.3, disketa hlásí C259.11)?


## [JPR-1ZI – procesorová deska se Z80](desky/JPR-1ZI.md) – Nejasnosti (3)

- **Čip Z80: NMOS, nebo CMOS?** Parametr `z80` (výchozí `nmos`). CMOS Z84C00 nepřijme přerušení hned po
  `LD A,I` / `LD A,R` (změřeno na JPR-1V 2026-10-13). Ověřit na autorově JPR-1ZI (`tools/hwbench/intblock.asm`)
  a podle nápisu na čipu.

- **Čtení prázdné sběrnice:** JPR-1V vrací poslední bajt datové sběrnice CPU (změřeno 2026-10-11). Jak je to
  u JPR-1ZI, JPR-1Z a JPR-1 (8080)? Emulace u nich zatím vrací FFh.

- Nedohledaný účel sítí „P1-/INT_“ a „P1-STB_“ ve schématu JPR-1ZI.


## [Klávesnice (konektor X2 na JPR-1ZI)](desky/klavesnice.md) – Nejasnosti ANK-1M2 (2)

- Který program joyport ANK-1M2 používá (hry, myš)? Emulace je jen podle schématu.

- Má JPR-1 na vstupech P1-IN pull-upy (bez zapojeného joysticku se čte 1)? Emulace: 1.


## [KZD-1 – kazetopáskový záznamník dat](desky/KZD-1.md) – Nejasnosti k ověření na HW (5)

- Co dávají čtecí výstupy DATA 0 / DATA 1 během záznamu (emulace: nic)? V obr. 4 je hradlo MH8438 na výstupu
  spojené s linkou ZÁZNAM, není čitelné.

- Za jak dlouho po zastavení pásky na konci se ohlásí přetížení (emulace 30 ms) a jak rychle zmizí po rozjezdu
  opačným směrem?

- Délka průhledného zavaděče skutečných datových kazet (emulace 0,4 m) a kde po K_R přesně stojí hlava.

- Skutečná rychlost převíjení s plnou a prázdnou cívkou (emulace stálá 1,44 m/s).

- Chová se K_L po K_D a K_R na skutečném KZD-1 stejně (chyba `NENI KONEC SOUBORU`)?


## [MFC-1A – řadič minidisket (Mini Floppy Controller)](desky/MFC-1A.md) – Nejasnosti k ověření na HW (4)

- Fungoval BIOS ECBIM8 s MFC-1A na skutečném stroji? (EC1045 na to neodpověděl, MFC-1A s mechanikami sám
  nerozchodil; nejspíš ho nikdo nezkoušel.) Podle obou schémat by se zasekl (chybí `RET` v `INIT_MFC`)
  a na originálu by navíc D6 = 1 mechaniky vypínal. Jaké MSR vrací 8272 v resetu?

- RDYn (XP2-19) není v příkladech připojení mechanik zapojený. Jakou úroveň pak vidí 8272 na RDY? Výpisy
  `SAPI15\MFC_TEST_CPM` (sekce výše) ukazují stálé RDY (`rdy_ground = true`). Sestavy SAPIMANa
  (`cpm_boot_mfc1a*`) ho mají od 2026-10-06 (rozhodnutí autora), `cpm_zrmd_mfc1a` ne (ECBIM8 čte z jednotky 3).

- Kdo dělal testy `SAPI15\MFC_TEST_CPM` (2010) a s jakým `DRVON` (40h, nebo 00h) běžel test na „Original MFC1A“?

- Které mechaniky se k MFC-1A u SAPI skutečně připojovaly (5,25" s READY, nebo 8" Consul)?


## [ARITMA Minigraf 0507 – souřadnicový zapisovač (plotter)](desky/Minigraf.md) – Nejasnosti k ověření na HW (4)

- Ovladač po nastavení počátku krokuje až asi 780 kroků/s, leták uvádí nejvýš 400 kroků/s (u modelů od roku 1986
  o 40 % víc). Stačil skutečný Minigraf, nebo ztrácel kroky? (Kresby na sapi.cz vypadají v pořádku.)

- Pero po zapnutí: port B DPP-1 je po resetu vstup a budiče 3216 (nastavené na výstup) pak dávají H. Spustí se
  pero, dokud program nezapíše řídicí slovo?

- Originální ovladač posouvá papír při nastavení počátku o 212 mm a TEST o 437 mm. Kreslilo se na A4, nebo
  na pás papíru? Jak se papír zakládal?

- Přepínače DPP-1 pro Minigraf: stránka píše „2PB“, kabel je na X2 (1PB).


## [MPH-1V – zvuková a joysticková karta (Libor Lasota)](desky/MPH-1V.md) – Nejasnosti (1)

- Směr os myši (která strana je „+“) jsme neověřili na skutečné myši s programem. Zatím žádný program pro SAPI
  myš nepoužívá, MPH-test jen ukazuje bity.


## [PKT-1 – procesorová deska SAPI-1.5 (Procesor, Klávesnice, Tiskárna)](desky/PKT-1.md) – Nejasnosti k ověření na HW (7)

1. Polarita linek MAP1/MAP2/BOOT na sběrnici (replika: aktivní H; čísla vývodů DD7 v originálním schématu
   nesedí s replikou).

2. Blokuje se sběrnicové /MR při čtení vnitřní EPROM (text návodu ano, netlist repliky ne)?

3. Hodnota na datové sběrnici při zápisu do okna EPROM a při INTA bez odpovídající karty (emulace FFh).

4. XT4 a XT5 na skutečných ZPS-5/6/7 (ZPS-8: 2 MHz, čekání jen z RDY).

5. Zapojení BUSY tiskárny na dochovaných deskách (PC6 původně, nebo upravené na PC3).

6. Funguje EKL-1 s /KACK = IBF?

7. Kde se použil dekodér DD8 s oknem 8 KB a 2864 z fotky (`FOTO_poklady_petr`)?


## [RAM-1 – deska dynamické paměti RAM](desky/RAM-1.md) – Nejasnosti (2)

- Dump PROM D1A (RAM-1 i RAM-1A), hlavně jestli výrobní obsah opravdu odpovídá tabulce 1 i pro neúplně osazené
  desky.

- Co CPU čte z neosazené řady.


## [RAM-1Z – dynamická RAM 64 KB](desky/RAM-1Z.md) – Nejasnosti (1)

- Potvrdit nastavení přepínačů pro 60 KB na skutečné desce.


## [REM-1 – deska pamětí RAM a EPROM](desky/REM-1.md) – Nejasnosti (1)

- Co CPU čte z prázdných patic EPROM a RAM (FFh, nebo plovoucí sběrnice).


## [RKD-1 – řadič kazetového disku (CM 5403 / CM 5400)](desky/RKD-1.md) – Nejasnosti (10)

Autor ani EC1045 nikoho, kdo RKD-1 provozoval, neznají. Co šlo, je rozhodnuté z podkladů (viz Emulace), zbytek

čeká na náhodný nález.

- **Pevný a výměnný disk:** CBIOS Tesly [R4] bere médium 0 a 1 (PC6 = 0) jako výměnný disk a bootuje z něj.
  FORMRKD J. Bíby [R7] má na médiu 0 a 1 pevný disk. Emulace to bere jako rozdíl mechanik (CM 5403 a CM 5400,
  kabel na str. 15 vede DISK přímo), ověřit to nejde.

- **12 nebo 24 sektorů:** dávala mechanika počet sektorových impulzů podle kazety (sektorové značky na disku),
  nebo podle přepínače? Šla kazeta 24 × 256 přečíst na jiné mechanice?

- **Kabel SM 5400 posunutý o bit** ([R1] str. 15): T00 na adresu cylindru 2⁰ a návrat nezapojený. Protože T00 je
  hladina, kterou PC0 maže (schéma), dá řadič s tímto kabelem cylindr 2N pro stopu N a `RK_HOME` najede na
  cylindr 1 (bez skutečného návratu). Vychází to, jen pokud SM 5400 bere aspoň 408 cylindrů (signál vysoké hustoty
  kabel nezapojuje), jinak by FORMRKD (stopy 0–203) skončil chybou na stopě 102. Emulace proto bere 408 cylindrů
  (J. Bíba by program na půlku disku nepsal). CM 5400 má podle [W2] 2,45 MB na disk, tedy jen 203 cylindrů po
  12 × 512 B na dvou plochách: buď je tabulka kabelu pro jinou mechaniku, nebo adresa mechaniky znamená něco jiného.

- **Ovladač ve FORMRKD.COM:** proč má starší `PREP_MARK` (hlavička jen se stopou) než zdroják `RKD.MAC`
  87-02-13, když je FORMRKD v1.1 z 20. 10. 1987? Měl J. Bíba vlastní verzi ovladače, nebo systém pro CM 5400
  s touto hlavičkou?

- **Ochrana proti zápisu:** co udělala CM 5403 se zápisem na chráněný disk? Tiše ho neprovedla (emulace, jako
  RK05), nebo hlásila poruchu ERR / chybu přístupu STO? Ovladač Tesly bit WPROTECT nečte a s poruchou by se systém
  zasekl (viz Emulace), takže chráněnou kazetu Tesla zřejmě nepoužívala.

- **Dekodér PROM MH 74188 (A6 na RKD-1A):** dump není, adresy 40h–4Fh jsou z popisu. Lze přepnout základ?

- **Skutečné kazety:** existuje někde obraz nebo kazeta s daty (CP/M Tesla Elstroj, data ze SMEP)? Emulace
  zatím začne s prázdnými kazetami.

- **Používal někdo RKD-1 se SAPI-1 v provozu** (Tesla Elstroj, JZD Slušovice)? Na disketách TNS jsou zmínky o
  jiném modulu „KDP – řadič kazetových disků“ (KDP-723), bez podrobností.


## [RMD-1 (RMD-1Z) – RAM disk](desky/RMD-1.md) – Nejasnosti (2)

- **PROM O3/O4:** HEX z návodu dává 40 → bit 2 (O3) a 43 → bit 3 (O4). Schéma ale pojmenovává O3 (pin 10)
  „43H“ a O4 „40H“. Software potřebuje 40 = řada a 43 = příprava čtení. Do ověření platí bit 2 → řada,
  bit 3 → příprava čtení.

- Časové limity: INIR při 2/4 MHz, je NOP po `OUT 43` nutný?


## [RMP-1 – řadič magnetické pásky (IZOT SM 5300)](desky/RMP-1.md) – Nejasnosti (5)

- **Parametry SM 5300:** vzdálenost čtecí a zápisové hlavy (emulace 0,15"), čas a dráha rozběhu a doběhu
  (emulace 32,8 ms a 0,2"), délka značky BOT (emulace 28 mm), poloha EOT a rychlost převíjení (emulace
  50 palců/s). Ověřit měřením nebo v manuálu SM 5300, pokud se najde.

- **Panel SM 5300:** jaká tlačítka a kontrolky skutečně má (LOAD, ON-LINE, REWIND, UNLOAD?) a jak se jmenují
  (MTDAT píše „DISTANC“). Fotografie [W1] panel nečitelně ukazují.

- **OFC a RWC:** jsou to impulzy (podle [R1] ano). Emulace bere náběžnou hranu bitu v PA. Vrací se jednotka
  po OFC sama do ON-LINE jen tlačítkem?

- **RDY při převíjení:** emulace RDY během převíjení shazuje (`wait.rewind` v [R2] čeká na RWS, ne na RDY).

- **Test TSX 10** výrobce se nenašel.


## [RPD-1 – řadič pružných disků (RPD-1A + RPD-1B)](desky/RPD-1.md) – Nejasnosti (5)

- Taktuje čítač 2 opravdu přístup CPU na 24h (N+1 přenosů)? Nechává OVFL bity D7–D5? (EBIOS to vyžaduje.)

- Doba ustálení hlavy (HLR) a zda opakovaný zápis na 3Dh při přiklopené hlavě drží HLR = 1.

- Bere RAM jen FE a RDM jen FB/F8? Kdy přesně se otevře hradlo zápisu a kterému bajtu se vynechají hodiny?
  Proč EBIOS posílá 4 nuly, když návod říká 5?

- Je TOUT výstup čítače 0, nebo i čítače 1 při čekání na datovou značku?

- **Rychlost:** `BAS5C.SYS` (10 KB) se v emulátoru čte asi 17 s. MIKOS/D po každém sektoru tráví tolik času, že
  další sektor (skew 6) mine a čeká otáčku. Jak dlouho to trvá na skutečném stroji?


## [RSW-1 – RAM, sériový port a watchdog SAPI-1.5](desky/RSW-1.md) – Nejasnosti k ověření na HW (3)

1. Skutečné propojky XT1 na deskách ZPS-7/8 a polarita MAP (schéma Tesla píše /MAP1, /MAP2).

2. Analogová zem (kolíky 53/54) proti GND (na replice nespojené).

3. Obnovení watchdogu: stačí zápis 0 a 1 do 1Fh (předpoklad z návodu „H→L→H na hradle“)?


## [RTC-1 – deska reálného času](desky/RTC-1.md) – Nejasnosti k ověření na HW (4)

1. Čtení PA v módu 1 (0B3h) s propojkami z výroby, kdy /STB A (PC4) nikam nevede: dává 8255 aktuální stav, nebo
   zamrzlou hodnotu? (Emulace: aktuální stav, podle funkčního `RTCZ`.)

2. Kterým kabelem a na který vstup (INT0, INT1, 3214 na JPR-1) se /INT z XC2/14 připojoval?

3. Které SA bylo sepnuté na deskách v sestavách (Bíba A0h; na Petrově fotce je jiný jen horní přepínač)?

4. Výstup XC2/29 je v originále i replice spojený s OUT2 DD6, návod ho uvádí jako vstup CLK.


## [XY 4130 – grafická jednotka (souřadnicový zapisovač)](desky/XY4130.md) – Nejasnosti k ověření na HW (3)

- Jaký zapisovač XY 41x0 kdo k SAPI připojoval, jakým kabelem a s jakým programem (ovladač v EPROM, nebo `KRES`)?

- Pracovní plocha, dorazy, krok 0,1 mm (z konstanty `KRES`)?

- Byl ovladač v `MIKOS5.BIN` někdy použitý (sapi.cz píše „nezkoušen“)? Proč je v EPROM za MIKOSem V5?


## [ZRMD-1 / ZRD-1V – zálohovaný RAM disk](desky/ZRMD-1.md) – Nejasnosti (1)

- Zamykání ZRD-1V z MikroMonu (viz výše).


## [ZRMD-1 / ZRD-1V – zálohovaný RAM disk](desky/ZRMD-1.md) – Nejasnosti rev 2.1 (4)

- Autorova deska je **rev 1.1** (autor), sestavy V to mají. Rev 2.1 se zatím nedá ověřit na skutečné desce.

- Okno 74LS123: schéma píše 20 ms, vzorec 0,37–0,45 · R · C dává 25–31 ms. Emulace: 20 ms.

- AIN přes 24 MΩ: kolik ukazuje skutečná deska proti napětí baterie (vstupní proud MAX187)? Emulace: přesně
  `battery_mv`.

- Čte se volná vnitřní sběrnice (61h–63h) opravdu jako FFh? Na rev 1.1 ano (změřeno 2026-10-11, `zrdrd.asm`,
  čtení zároveň nahraje FFh do registru adresy, jak emulátor dělá). Na rev 2.1 neověřeno.


## [Procesor Intel 8085 v emulátoru (režim 8085 jádra `third_party/i8080`)](8085.md) – 7. Nejasnosti k ověření na HW (8085 v SAPI) (10)

Krátký test na reálném čipu: pro každou instrukci POP PSW s F = 00h a F = FFh, instrukce, PUSH PSW a výpis F.

Rozhodne všechna místa označená [S] a [?]:

1. Zapisují INR/DCR, logické operace, RLC/RAL/RRC/RAR a DAA V i K podle 3.2?

2. Mění DAD V (16bitové přetečení) a K?

3. Mění ARHL V a K? A RDEL K?

4. DSUB: P a AC z horního bajtu? Sémantika AC?

5. CMA a STC/CMC: opravdu nemění V/K (MAME u CMA nastavuje V)?

6. PUSH PSW: bit 3 je vždy 0? POP PSW s bitem 3 = 1 a pak PUSH PSW.

7. RIM: jsou I5.5/I6.5 surová úroveň pinu i při masce a DI?

8. Časování přijetí RST 7.5/TRAP: 12 T (Intel obr. 2-19) proti 11 v MAME. Lze změřit čítačem taktů nebo
   z porovnání s instrukční smyčkou.


## [Kazeta DSM-1: formát MIKOS V4/V5 a MIKROBASIC 2.4, kazeta z programu (HEX, BIN)](software/kazeta.md) – 5. Nejasnosti (k ověření na skutečném HW) (5)

- Jak se skutečný dekodér DSM-1 chová na začátku úseku po tichu. Generátor proto nikde ticho nedává.

- Minimální délky tónů pro skutečný magnetofon (rozběh motoru po zastavení za hlavičkou, AGC). Doporučené časy
  jsou blízko originálu; minima (MIKOS mezi bloky 70 ms, MIKROBASIC úvod asi 2 s) jsou jen z emulátoru, čtecí
  rutina ale běží v ROM se stejnými takty.

- Proč je skutečná mezera mezi bloky `B` 210 ms a v emulátoru 170 ms (100 ms TIME + režie): pomalejší skutečný
  stroj (čekací stavy?) nebo jiná režie. Pro formát nepodstatné.

- V5 ve výchozím stavu počítá se SP-210 (TSP = 0, port 34h–37h 8255), s K-10 (relé) přes `.K_X`. Formát stejný.

- Přehrání obdélníku z PC do skutečného vstupu DSM-1: stačí přímo, nebo je lepší zaoblit (filtr)? Originální
  nahrávky z magnetofonu jsou 96 kHz.


## [SAPIemu – souhrn a předávka](SOUHRN.md) – Nejasnosti k ověření na reálném HW (sbírat a řešit dávkově) (41)

1. **AND-1Z-89:** ~~20 nebo 24 řádků? Délka řádku 107 nebo 108 znaků.~~ 20 řádků, 109 znaků (změřeno 2026-10-13,
   `docs/desky/AND-1Z-89.md`).

2. **DMR-1:** skutečné nastavení JP8 (baud rate) na autorově sestavě. Stav modemových bitů při odpojeném terminálu.

3. **Volná sběrnice:** ~~čte se opravdu FFh?~~ Na JPR-1V ne: vrací poslední bajt datové sběrnice CPU
   (změřeno a emulováno 2026-10-11, `docs/zmeny-podle-hw.md`). JPR-1ZI, JPR-1Z a JPR-1 jsou otevřené
   (`docs/desky/JPR-1ZI.md`).

4. **Consul 262.3:** polarita, délka strobe; má vaše JPR-1Z/ZI úpravu se 7474? (Kódy už jsou z manuálu.)

5. **ANK-1:** bity LED a bzučáku na P0-OUT (emulace podle MIKOS: D7 ERR, D6 bzučák, D5 SHIFT). Je piezo
   pasivní (sleduje D6, MIKROBASIC ho přepíná), nebo s oscilátorem?

6. **ZRMD-1:** ~~rozsahy S1–S4~~ (vyřešeno z GAL). ~~Jak MikroMon „zamyká“ ZRD-1V?~~ (rev 2.1: zápis na 60h
   déle než 20 ms po 62h, `docs/desky/ZRMD-1.md`). ~~Nuluje /RES adresní čítače?~~ Jen latch 63h.
   ~~Které bity 63h jdou na MAP1/MAP2?~~ b6 a b7 (obojí změřeno 2026-10-11, `docs/zmeny-podle-hw.md`).

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
    - ~~Ověřit: s JPR-1V AND-1V při IN/OUT na porty F8–FF nečeká?~~ Nečeká (změřeno 2026-10-11,
      `docs/zmeny-podle-hw.md`). Visí DGD-1 při BOOT opravdu?
    - Rev 1.1: která polovina fontu (A12 = 0/1) je standardní?

22. **Desky V – RAM-1V, DSM-1V, ZRD-1V** (rozbor `docs/desky/RAM-1V.md`, `DSM-1V.md`, `ZRMD-1.md`):
    - **ZRD-1V:** PDF schématu (vytisknout `ZRD-1V.sch` z Eagle jako u ostatních). ~~Nuluje /RES čítače a
      registry adresy? Jsou 61h–63h opravdu nečitelné?~~ Rev 1.1: /RES adresu drží, 61h–63h čtou FFh a FFh
      nahrají do registru (změřeno 2026-10-11). Má hlídání MAX808 nějaký vliv viditelný pro software?
    - ZRD-1V: ~~kterou revizi máš (fotka 1.1)?~~ Rev 1.1 (autor). ~~Zámek zápisu a MikroMon~~: vyřešeno schématem rev 2.1, otevřené
      body jsou v `docs/desky/ZRMD-1.md` („Nejasnosti rev 2.1“).
    - RAM-1V: revize 1.1, nebo 1.2? Emulovat podle schématu 1.1 (1.2 nemá MAP2 u díry 16 KB)?
    - RAM-1V, patice C: je prázdná. Jaký Timekeeper se tam dává (M48T35/DS1644?) a emulovat jeho RTC?
    - DSM-1V: propojky na tvé desce (adresy A = 10h, B = 14h?, rychlosti obou kanálů).
    - DSM-1V b5 („START“): nemá funkci? ~~Čte +3 FFh?~~ Ano, FFh (změřeno 2026-10-11).
    - DGD-1/BGP-1S „paralelně s RAM-1V“: obě karty odpovídají na stejné adresy. BGP-1S se od 2026-10-06 hlásí jen
      pro zápis (nečte), takže kolize není; DGD-1 to dál potřebuje.

23. **CGA-1V** (`docs/desky/CGA-1V.md`):
    - `..\SAPI-karty\CGA-1V\RADICROM.bin` k desce nepatří. Je tam omylem?
    - Je k dispozici dokument „Programování karty“ a novější CGATEST, který má i test CGA (volba 1)?
    - Liborovy programy smí do repa jako binárky (autor), jsou v `media/cga-1v/`.

25. **Bzučák** (`docs/desky/klavesnice.md`):
    - Mají Consul 262.3 a 259.11 bzučák na X2 pin 14 (P0-OUT1)? Emulace předpokládá stejný jako EKL-1.
    - Jaký tón má bzučák EKL-1 (SG1 F/CM12P)? Emulace: 2,4 kHz.
    - ~~Pípne skutečný SAPI po resetu?~~ Ano: s testovacími ROM, které `OUT 01h` nezapíšou, EKL-1 na pracovišti
      po resetu piská pořád (autor ji proto odpojil, 2026-10-11). Emulace to dělá stejně.

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


## [Tiskárna](tiskarna.md) – Nejasnosti k ověření na HW (3)

- Co je v kabelu k tiskárně na kolíku 16 X3 (P2-IN7)? ECMON při 1 netiskne, emulace ho drží v 0. (EC1045 neví,
  na X3 měl jen plotter MINIGRAF přes rozhraní z AR 1988 s přepínači polarity STB a ACK.) **EC1045 (2026-10-07):**
  v jeho rozhraní pro plotter vede X3/16 na kolík 11 konektoru DSUB25, tedy **BUSY**. Konektor odpovídá LPT
  z PC. Ovladač `lst1` ECMONu by při BUSY na P2-IN7 znaky zahazoval (bit 7 = 1 → `RC`), plotter tedy měl buď vlastní
  ovladač, nebo BUSY s opačnou polaritou. Pro tiskárnu s ECMONem zůstává kolík 16 nejasný.

- Bere tiskárna (LQ-1070, K6313) data při sestupné, nebo náběžné hraně /STROBE? Emulace je bere při náběžné.

- Tiskárna na DPP-1: jak byl zapojený kabel (/OBF → /STROBE, /ACK → PC6, BUSY?) a přepínače S2–S4? Tiskl
  někdo přes `UL1:`, nebo tam byl jen děrovač? (Přepínače desky EC1045 s PMD32-SD a programátorem PROM jsou
  rozepsané v `docs/desky/DPP-1.md`, tiskárnu na DPP-1 neměl.)
