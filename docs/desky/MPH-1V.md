# MPH-1V – zvuková a joysticková karta (Libor Lasota)

Podklady (mimo repo): `..\MPH-1V\`:
- `schema_mph1v_rev10_1.png` a `_2.png` (rev 1.0, 10. 8. 2025, listy 1/3 a 2/3), `schema_mph1v_rev10_3.png`
  (zvukový generátor, 6. 9. 2025, list 3/4);
- `MPH-1V.pdf` vytištěný z `MPH-1V_2025-08-24\MPH-1V.sch` (24. 8. 2025, 4 listy, deska `.brd`);
- kusovník `kusovník MPH-1V rev. 1.0.xlsx`;
- programy: `MPH-test` (test karty 0.99), `SPLAY` (přehrávač VGM a 11 skladeb), `RATplayer` (RAT PLAYER 0.99.93
  a tři skladby RAD), `y1stdemo_2025-10-26`.

Emulace: `src/cards/mph1v.*`, obvody `src/chips/i8253.*` a `src/chips/ym3812.*` (jádro Nuked-OPL3), typ karty
`MPH-1V`, v sestavě `machines/sapi1v.sapi` na pozici X8 s bází 50h. Test `test_mph1v`. Programy jsou
v `media/mph-1v/`, hudba v repu není.

## Obvody

- **YM3812** (OPL2) s DAC YM3014B, takt 3,579545 MHz, jeden vzorek na 72 taktů = 49716 vzorků/s.
- **8253**: CLK0 = CLK2 = 55930,4 Hz (14,31818 MHz / 256), CLK1 = OUT0 (kaskáda).
- Oscilátor: list 3 z 6. 9. 2025 má 14,31818 MHz a děliče 74LS393 (/4 pro YM, /256 pro 8253), PDF z 24. 8. má
  3,579545 MHz a /64. Výsledné kmitočty jsou stejné.
- /RST YM3812 drží 74LS164 ještě 8 taktů po /IRES (asi 36 µs). Emulace resetuje YM přímo.
- Výstup: Sallen-Key dolní propust (R4 = R5 = 2k2, C9 = 6n8, C10 = 3n3: 15,3 kHz, Q 0,72), oddělení C8 10 µF
  s R3 47k (horní propust 0,34 Hz). Mono na oba kanály jacku K3. Emulace oba filtry počítá.
- Konektory na panelu (štítek): K4 OVL (joystick), K3 ZVUK (jack 3,5 mm), K2 MYŠ. Na listu 1 PNG je joystick
  označen K3, PDF má K4 jako štítek.

## Porty (báze S1, 74LS688 na A3–A7, výchozí 50h)

Bity registrů jsou podle **testovacího programu karty MPH-test 0.99** (říjen 2025). Stejně je používají SPLAY,
RAT PLAYER a y1stdemo. Obě dostupná schémata se v nich liší, viz níže.

| Port | Čtení | Zápis |
|---|---|---|
| +0 … +2 | 8253 čítač 0–2 | 8253 čítač 0–2 |
| +3 | STATUS | 8253 řídicí slovo |
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

- **T0–T2:** úrovně OUT0–OUT2 8253.
- **F2, F1:** klopné obvody 74LS74 nastavené náběžnou hranou OUT2 a OUT1. Nastavují se i bez povolení
  přerušení (y1stdemo čeká na F2 s IEN = 20h). IACK s D7 / D6 = 1 je nuluje. 74LS175 drží nulování do dalšího
  STSTB, takže jde o krátký impulz.
- **YM:** požadavek přerušení YM3812 (negovaný /IRQ, aktivní v 1). SPLAY na něj čeká.
- **IEN** (74LS174, /RES nuluje): G0–G2 jsou hradla čítačů. F2, F1 a YM povolují přerušení.
- **/INT0** = (F2 a IEN.D7) nebo (F1 a IEN.D6) nebo (YM a IEN.D0), 74LS38 s otevřeným kolektorem.
- **JOY/MOUSE:** aktivní v 0, pull-upy 4k7. MPH-test u portu JOY ukazuje i význam pro myš (XA, XB, YB, YA).
  Myš se zatím neemuluje, MOUSE čte stav druhého ovladače (dnes nic, tedy FFh).
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

- 8253 počítá takty z bázového času sběrnice (zbytky se přenášejí, žádný drift), YM3812 generuje vzorky
  podle emulovaného času. Zvuk jde do `Bus::audio` (mono, 49716 Hz).
- GUI hraje zvuk jen při rychlosti emulace 100 % a mimo pauzu (Stroj → Zvuk a joystick: zapnutí, hlasitost).
  Latence je asi 60 ms. Při zpoždění se nejstarší vzorky zahodí, po vyprázdnění se zvuk znovu nabuferuje.
- Joystick: první připojený gamepad (SDL3: křížový ovladač nebo levá páčka, A/X = FL, B/Y = FR). Volitelně
  šipky a mezerník (FL), které pak nejdou do klávesnice SAPI.
- `sapiemu-cli --audio-wav soubor.wav` nahraje zvuk od místa v akcích do konce. MCP `audio_record` (WAV, RMS,
  špička) a `joystick`.

## Nejasnosti

- **Rozložení bitů:** PNG list 2 (10. 8. 2025) má STATUS `– – YM T2 T1 T0 – –`, latche T0/T2 a hradla v D0/D1.
  PDF (24. 8. 2025) má STATUS `EX F2 T2 T1 T0 0 0 YM` a klopný obvod „EX“ s nezapojeným hodinovým vstupem. Žádné
  z nich neodpovídá programům. Emulace je podle MPH-test 0.99. Existuje schéma hotové desky?
- **F1:** nastavuje ho OUT1, nebo OUT0? Emulace: OUT1 (podle názvu). PDF má klopný obvod pro D6 hodinovaný z OUT2.
- **IACK D2, D1** (PDF: „EE“, „FE“, nastavení příznaků softwarem): jsou na hotové desce?
- **STATUS D2, D1** čtou 0 (PDF: vstupy 74LS244 na zem)?
- **JOY D7, D6** čtou 1?
- Oscilátor osazený na desce: 14,31818 MHz (list 3 PNG), nebo 3,579545 MHz (PDF)?
