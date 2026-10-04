# DGD-1 – deska grafického displeje

Zdroje: `BKP\sapi\dgd-1.php` (OCR návodu, obsahy PROM D5D, D1A, D10C), obrázky `BKP\sapi\obrazky\dgd-1*.png`,
`ndgd-1*.png`, popis grafiky v BASIC EXP V5.0/G (`BKP\sapi\doku2\bexpg.php`).

## Zapojení

- **Rastr 320 × 240 bodů**, černobílý. Vlastní videovýstup (TV s videovstupem, AZJ-462) vedle AND-1. AND-1
  zůstává pro text, kabely k televizoru se případně přepojují.
- **Paměť:** 16 KB dynamické RAM (8× K565RU6 = 4116) jako obyčejná paměť v jedné čtvrtině adresního prostoru.
  Program do ní zapisuje i z ní čte.
- **Přepínač SP1** (1 = sepnuto):

| SP1-1 | SP1-2 | Začátek | Sestava |
|---|---|---|---|
| 0 | 0 | C000H bez mapování | ZPS-2, ZPS-3 |
| 0 | 1 | 0000H bez mapování | |
| 1 | 0 | C000H s mapováním | |
| 1 | 1 | 0000H s mapováním | SAPI-1Z (RAM-1Z maskuje 0000–3FFF) |

- V ZPS-2 je potřeba propojkami na RAM-1 vymaskovat C000–FFFF (S1 = 0, S2 = 1, 4000–BFFF). Mapování v ZPS-2
  nejde bez výměny PROM na RAM-1.
- **Uspořádání:** řádek y (0 = nahoře) začíná na základ + y · 40h a má 40 bajtů (320 bodů). Zbylých 24 bajtů řádku
  se nezobrazuje. Bit 7 je bod vlevo, 1 = bod svítí. 240 řádků = základ až základ + 3BFFH.
- **Časování:** krystal 8 MHz (bodový kmitočet), Johnsonův čítač dělí osmi na 1 MHz (bajt), řádek 64 µs, 240
  viditelných z 312 řádků, snímek 20,5 ms.
- **Přístup CPU:** SEL nastaví klopný obvod WAIT (RDY = 0), CPU dostane paměť v okně daném náběhem HSCK
  a pak RDY uvolní.
- **PROM:** D1A (74S287) = dekodér adres (vstupy MAP1, MAP2, SP1 a adresy), D5D (74S287) = synchronizace,
  D10C (74S571) = video. Obsahy jsou v návodu.

## Emulace (`src/cards/dgd1.*`)

- Parametry: `base` (`"C000"` / `"0000"`, SP1-2), `mapping` (SP1-1, výběr jen při aktivním MAP1),
  `wait_states` (výchozí zapnuto), `monitor` (barva luminoforu monitoru: white, green, amber).
- **Čekací stavy:** CPU smí do paměti v první polovině každé mikrosekundy (předpoklad), jinak čeká na další
  okno. Při 2 MHz je to 0–1 čekací takt.
- **Zobrazení:** vlastní okno „DGD-1“ v GUI. Snímek přes MCP `screenshot` s `display: "DGD-1"`, přes
  `sapiemu-cli --display DGD-1 --screenshot soubor.png`.
- **PROM D1A se nepoužívá.** Výběr se řídí tabulkou SP1 z návodu. Zapojení vstupů D1A neznáme, jedničky
  v dumpu jsou na adresách 1C, 5F, 98 a DB.

## Ověřeno v emulátoru

`tests/test_mikos.cpp`: BASIC EXP V5.0/G pod MIKOSem V5:
- CLSG vymaže 16 KB C000–FFFF;
- PLOT kreslí na očekávané bity (y = 0 na řádku 239 = FBC0H, bit 7 vlevo);
- BASIC běží dál i s plnou grafickou pamětí.

Ručně ověřeno: sinusovka se SCALE a rámeček přes `sapiemu-cli`.

## Nejasnosti

- Které vstupy má PROM D1A (A15, A14, MAP1, MAP2, SP1…)? Obsah dumpu nesedí na jednoduché přiřazení.
- Délka okna CPU v periodě 1 µs (počet čekacích stavů).
- Chování s mapováním (aktivní úrovně MAP1/MAP2 na sběrnici).
- Má autor DGD-1 nebo fotku obrazovky s grafikou pro porovnání (poměr stran, polarita)?
