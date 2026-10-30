# JPR-1 – původní procesorová deska (8080)

Zdroje (`BKP` = web `https://sapi.cz/sapi/…` ([přehled desek](https://sapi.cz/sapi/desky.php)), `D11` = archiv dokumentace SAPI_DOKU, stav 2011-09-10):
- `BKP\jpr-1.php` (OCR manuálu), `BKP\pdf\jpr-1.pdf` (vektorové překreslení „EC1045.01“, 2011),
  `BKP\obrazky\jprpri2.png` (propojky)
- původní skeny od J. Slanského (archiv autora): dvě strany schématu, EPROM, propojky, popis, X2 a X3
- Eagle replika `D11\sapi_eagle\REPLIKY\JPR-1\`
- manuál ZPS-1 `BKP\pdf\zps1.pdf` (str. 16 mapa paměti, 17 mapa I/O, 18 sloty)

## Zapojení

- **CPU:** MHB 8080A, 8224 (krystal 18 MHz / 9 = 2 MHz), 8228. Po resetu začíná na 0000.
- **EPROM na desce** (patice D4E–D7E, dekodér D10, jen při MR a A14 = A15 = 0):
  - 2708 v 1 KB stránkách od 0000 (propojky 7-8, 4-5);
  - 2716 ve 2 KB stránkách 0000/0800/1000/1800 (propojky 9-7, 4-6).
  - V ZPS-1 jsou patice prázdné, MIKROBASIC je na REM-1. Verze z AR/B 1983 má EPROM na JPR-1.
- **D9** (A13 = 1, A14 = A15 = 0, bez hradlování MR/MW) dělí 2000–3FFF po 1 KB:

| Adresa | Funkce |
|---|---|
| 2000–23FF | RAM 1 KB (2× 2114), v ZPS-1 neosazená |
| 2400–27FF | P0: IN čte sloupce ANK-1, OUT řádky D4–D0, D5 LED SHIFT, D6 bzučák, D7 LED ERR (příloha VII manuálu ANK-1) |
| 2800–2BFF | P1: IN D7 klávesa INT, D6 klávesa T; OUT D7 LED na desce |
| 2C00–2FFF | P2: IN požadavky přerušení (R0 = /INT0, R1 = /INT1, R7 = klávesa INT), OUT tiskárna |
| 3000–33FF | zápis do 3214 (/ECS) |
| 3400–3FFF | nevyužito (AND-1 na 3800 je na sběrnici) |

  Porty jsou zrcadlené přes celý 1 KB blok. Výstupní registry nejdou číst, MIKROBASIC si P0 pamatuje v RAM
  (`PORT24` na 4000h).
- **Přerušení:** MH3214 má požadavky na vstupech P2-IN (aktivní v 0; R0 = /INT0, R1 = /INT1 sběrnice, tytéž vodiče).
  Při INTA dá 3212 na sběrnici `RST (7 − úroveň)`. Zápis na 3000h nastaví aktuální stav a povolí další
  přerušení, po přijetí přerušení čeká 3214 na další zápis. Propojka 10-11 připojuje klávesu INT na R7.
  - **Stav:** vstupy /B0–/B2 jsou aktivní v nule, zápis D2–D0 tedy dá stav `~D & 7`. Přerušení projde úroveň
    vyšší než stav. D3 = 1 (SGS) pustí každou úroveň.
  - Ověřeno firmwarem APS-5 (2026-10-08): zápis 04h musí pustit jen R4–R7 (na R0–R3 je kód klávesy), 02h jen
    R6–R7. Do té doby emulace brala D2–D0 přímo a D3 = 0 jako „vše“.
  - MIKOS zapisuje 01h nebo 02h: projde R7 (klávesa INT), případně i R6.
  - Schéma JPR-1X (Libor Lasota) vede u 3214 /B0–/B2 a /SGS, tedy také aktivní v nule (`JPR-1X.md`).
- **Konektory X2, X3** (tabulky návodu): X2 = P0-IN, P0-OUT, P1-IN (a STB/INT P0, P1); X3 = P1-OUT, P2-IN, P2-OUT
  (a STB/INT). Všechny vstupy mají pull-up 4k7. V APS-5 na nich visí panel HRK-1 a svorkovnice
  (`APS-5.md`); emulace je zapojuje jako „klávesnici“ `HRK-1`, která dostává i P2-IN, P1-OUT, P2-OUT a /RTL.
- **Propojky:** 12-13 READY z 8224 (výchozí), 12-14 jeden čekací stav; 1-2 INTA na sběrnici.

## Emulace (`src/cards/jpr1.*`)

- Jádro 8080 `third_party/i8080` (superzazu/8080, MIT) krokuje po instrukcích se správným počtem taktů.
  Prošlo testy TST8080, 8080PRE, CPUTEST a 8080EXM (`tests/test_i8080.cpp`).
- Paměť a porty na desce se obsluhují na kartě, ostatní jde na sběrnici (REM-1, AND-1). Instrukce IN/OUT jdou
  na sběrnici (DSM-1).
- Parametry: `rom` (EPROM na desce), `eprom_type`, `ram`, `int_key` (10-11), `wait_state` (12-14),
  `crystal_hz`, `keyboard` (ANK-1).
- Disassembler v debuggeru zatím ukazuje kód 8080 v mnemonice Z80 (instrukční sada 8080 je podmnožinou Z80).

## Nejasnosti

- Které RST dá která úroveň 3214 (odvozeno `RST 7 − úroveň`; APS-5 to nerozliší, všechny vektory jdou na
  jednu obsluhu). Význam D3 = 1 (emulace: všechny úrovně) firmware APS-5 nepoužívá.
- Manuál uvádí 2. pozici 2708 na 0400, dekodér pro 2716 dává 0800 (emulace: 2708 po 1 KB, 2716 po 2 KB).
- Co CPU čte z prázdných patic a z 3000h (emulace: prázdné patice = sběrnice, 3000h = FFh).
- Polarita LED na P1-OUT D7 (emulace: 1 = svítí, LED na panelu v konfigurátoru).
