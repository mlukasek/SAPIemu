# MPH-1V – zvuková a joysticková karta (Libor Lasota)

Podklady (mimo repo): `..\MPH-1V\`:
- **`Eagle\`: schéma hotové desky rev 1.0** (6. 9. 2025): `MPH-1V.pdf` (4 listy), `schema_MPH1V_1.png` až `_4.png`,
  `MPH-1V.sch`, `.brd` a `DESCRIPTION` („YM3812, i82C54, připojení 2× Atari JOY nebo JOY/MOUSE a blok řízení
  přerušení, báze 00h–F8h po 8, výchozí 50h“). Podle něj je emulace.
- starší rozpracované verze: `schema_mph1v_rev10_1.png` a `_2.png` (10. 8. 2025), `schema_mph1v_rev10_3.png`
  (6. 9. 2025, list 3 je shodný s hotovou deskou), `MPH-1V.pdf` z `MPH-1V_2025-08-24\MPH-1V.sch` (24. 8. 2025);
- kusovník `kusovník MPH-1V rev. 1.0.xlsx` (IC6 = CP82C54-10);
- programy: `MPH-test` (test karty 0.99), `SPLAY` (přehrávač VGM a 11 skladeb), `RATplayer` (RAT PLAYER 0.99.93
  a tři skladby RAD), `y1stdemo_2025-10-26`.

Emulace: `src/cards/mph1v.*`, obvody `src/chips/i8253.*` (8253 / 82C54) a `src/chips/ym3812.*` (jádro Nuked-OPL3), typ karty
`MPH-1V`, v sestavě `machines/sapi1v.sapi` na pozici X8 s bází 50h. Test `test_mph1v`. Programy jsou
v `media/mph-1v/`, hudba v repu není.

## Obvody

- **YM3812** (OPL2) s DAC YM3014B, takt 3,579545 MHz, jeden vzorek na 72 taktů = 49716 vzorků/s.
- **82C54** (IC6, CP82C54-10; ve schématu značka 8253): CLK0 = CLK2 = 55930,4 Hz (14,31818 MHz / 256),
  CLK1 = OUT0 (kaskáda). Proti 8253 má příkaz read-back (řídicí slovo s SC = 11, stavový bajt s OUT, NULL
  COUNT a naprogramovaným režimem). Emulace ho umí.
- Oscilátor QG1 14,31818 MHz (propojka S2 QG1EN), děliče 74LS393: /4 pro YM, /256 pro 82C54, /64 pro 74LS164.
- /RST YM3812 drží 74LS164 ještě 8 taktů po /IRES (asi 36 µs). Emulace resetuje YM přímo.
- Výstup: Sallen-Key dolní propust (R4 = R5 = 2k2, C9 = 6n8, C10 = 3n3: 15,3 kHz, Q 0,72), oddělení C8 10 µF
  s R3 47k (horní propust 0,34 Hz). Mono na oba kanály jacku K3. Emulace oba filtry počítá.
- Konektory (štítek i schéma): K4 OVL (joystick), K3 ZVUK (jack 3,5 mm), K2 MYŠ. K4 a K2 jsou zapojené stejně
  (Atari DB9): 1 nahoru, 2 dolů, 3 vlevo, 4 vpravo, 6 palba (FL), 9 druhé tlačítko (FR), 7 +5 V přes polyswitch
  BK60-030, 8 zem, 5 nezapojený. /IRES vypíná porovnávač adresy (karta během resetu neodpovídá).

## Porty (báze S1, 74LS688 na A3–A7, výchozí 50h)

Bity registrů jsou podle schématu hotové desky (list 2 z 6. 9. 2025). Shodují se s testovacím programem karty
MPH-test 0.99 i s tím, jak je používají SPLAY, RAT PLAYER a y1stdemo.

| Port | Čtení | Zápis |
|---|---|---|
| +0 … +2 | 82C54 čítač 0–2 | 82C54 čítač 0–2 |
| +3 | STATUS | 82C54 řídicí slovo |
| +4 | JOY (K4) | IEN |
| +5 | MOUSE (K2) | IACK |
| +6 | YM3812 stav | YM3812 adresa registru |
| +7 | (YM nevysílá, FFh) | YM3812 data |

| Registr | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |
|---|---|---|---|---|---|---|---|---|
| STATUS (čtení +3) | F2 | F1 | T2 | T1 | T0 | 0 | 0 | YM |
| IEN (zápis +4) | F2 | F1 | G2 | G1 | G0 | – | – | YM |
| IACK (zápis +5) | F2 | F1 | – | – | – | – | – | – |
| JOY, MOUSE | 1 | 1 | FR | FL | vlevo | vpravo | nahoru | dolů |

- **T0–T2:** úrovně OUT0–OUT2 82C54 (IC19 74LS244, D2 a D1 jsou na zemi).
- **F2, F1:** klopné obvody 74LS74 (IC9B, IC9A, D = 0, příznak je /Q) nastavené náběžnou hranou OUT2 a OUT1. Nastavují se i bez povolení
  přerušení (y1stdemo čeká na F2 s IEN = 20h). IACK s D7 / D6 = 1 je nuluje. 74LS175 drží nulování do dalšího
  STSTB, takže jde o krátký impulz.
- **YM:** požadavek přerušení YM3812 (negovaný /IRQ, aktivní v 1). SPLAY na něj čeká.
- **IEN** (IC11 74LS174, /RES nuluje): G0–G2 jsou hradla čítačů. F2, F1 a YM povolují přerušení. D2 a D1 se
  neukládají.
- **/INT0** = (F2 a IEN.D7) nebo (F1 a IEN.D6) nebo (YM a IEN.D0), 74LS38 s otevřeným kolektorem.
- **JOY/MOUSE:** aktivní v 0, pull-upy 4k7. D7 a D6 jsou jen na pull-upech (čtou 1). Pro myš mají D3–D0
  význam YA, YB, XB, XA (kvadratura, MPH-test: „Joystick B / Myš“ na dolním konektoru).
- Časovače YM3812: T1 po 80 µs (288 taktů), T2 po 320 µs. Stav: D7 IRQ, D6 T1, D5 T2, dolní bity 110
  (jako skutečný YM3812, detekce AdLib).
- WSE (registr 01h bit 5): bez něj hraje každý operátor sinus. Jádro OPL3 to v režimu OPL2 nedělá, řeší to
  obal `Ym3812`.

## Jak programy kartu používají

- **MPH-test 0.99:** volba 3 nastaví čítač 0 na 2 Hz (režim 3, 27965), čítač 1 = T0/4, čítač 2 na 1 Hz
  (55930) a časovač T2 YM na 12 Hz. Klávesy 7–0 přepínají bity IEN, 9–8 bity IACK, a počítá přerušení INT0.
- **SPLAY:** `OUT 54h,0` (bez přerušení), časovač T2 YM, čeká na STATUS D0 a kvituje YM registrem 04h = 80h.
- **RAT PLAYER:** čítač 2 v režimu 2 na 50 Hz (1119), IEN = A0h, čeká na STATUS D7 (F2) a kvituje zápisem
  přečteného stavu na IACK. Při startu změří periodu (hlášení „chyba casovace MPH-1V“).
- **y1stdemo:** čítač 2 v režimu 0, IEN = 20h, čte T2 a F2.

## Emulace

- 82C54 počítá takty z bázového času sběrnice (zbytky se přenášejí, žádný drift), YM3812 generuje vzorky
  podle emulovaného času. Zvuk jde do `Bus::audio` (mono, 49716 Hz).
- GUI hraje zvuk jen při rychlosti emulace 100 % a mimo pauzu (Stroj → Zvuk a joystick: zapnutí, hlasitost).
  Latence je asi 60 ms. Při zpoždění se nejstarší vzorky zahodí, po vyprázdnění se zvuk znovu nabuferuje.
- **Co je na konektorech** (parametry `k4`, `k2`, platí hned i za chodu): `none`, `joystick`, `mouse_atari`,
  `mouse_amiga`. Výchozí K4 = joystick, K2 = myš Atari ST (podle štítků OVL a MYŠ).
- **Joystick:** první konektor s joystickem dostane první gamepad, druhý druhý (SDL3: křížový ovladač nebo levá
  páčka, A/X = FL, B/Y = FR). Volitelně šipky a mezerník (FL) jako první joystick, které pak nejdou do klávesnice
  SAPI. Zapíná a vypíná se v menu nebo **Ctrl+F8** (autor: na psaní rychle vypnout). Stavový řádek ukazuje, když je
  zapnutý. Obě volby (joystick z klávesnice, myš PC) jsou výchozí vypnuté a GUI si pamatuje poslední stav.
- **Myš Atari ST:** pin 2 XA, 1 XB, 4 YB, 3 YA (= D0 XA, D1 XB, D2 YB, D3 YA, popisky MPH-test), levé tlačítko
  pin 6 (D4), pravé pin 9 (D5). **Amiga:** pin 2 H, 1 V, 4 HQ, 3 VQ (D0 H, D1 V, D2 HQ, D3 VQ), prostřední
  tlačítko (pin 5) karta nemá zapojené.
- Kvadratura: fáze 00, 10, 11, 01 (A, B), doprava a dolů vede A. Karta posílá nejvýš 1000 kroků za sekundu na
  osu (emulovaný čas) jako skutečné kolečko myši. Program, který port jen čte, by rychlejší kroky přeskočil.
  Pohyb, který nestihne, se drží nejvýš 250 ms, pak se zahodí.
- **Zachycení v GUI:** nejdřív se zapne Stroj → Zvuk, joystick a myš → Myš PC jako myš SAPI (výchozí vypnuto,
  pamatuje se). Pak kliknutí do kteréhokoli okna obrazovky (AND-1V, CGA-1V, DGD-1V…) myš zachytí, když je nějaká
  připojená. Kurzor zmizí a relativní pohyb jde do SAPI. Pustí ji prostřední tlačítko, Ctrl+F10 nebo
  ztráta fokusu okna. Citlivost (kroků na bod myši PC) je ve Stroj → Zvuk, joystick a myš. Absolutní pozice
  nejde: kde je kurzor, ví jen program.
- `sapiemu-cli --audio-wav soubor.wav` nahraje zvuk od místa v akcích do konce. MCP `audio_record` (WAV, RMS,
  špička), `joystick` a `mouse`.

## Nejasnosti

- ~~Rozložení bitů STATUS/IEN/IACK, zdroj F1, IACK D2/D1, STATUS D2/D1, JOY D7/D6, oscilátor~~: vyřešeno
  schématem hotové desky (`..\MPH-1V\Eagle`, 6. 9. 2025). Starší PNG z 10. 8. a PDF z 24. 8. byly rozpracované
  verze.
- /RST YM3812 se po /IRES uvolní o 8 taktů 224 kHz později (asi 36 µs). Emulace to nezdržuje. Program, který by
  hned po resetu zapisoval do YM, by se na desce choval jinak (zatím žádný takový neznáme).
- Směr os myši (která strana je „+“) jsme neověřili na skutečné myši s programem. Zatím žádný program pro SAPI
  myš nepoužívá, MPH-test jen ukazuje bity.
