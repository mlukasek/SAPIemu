# DSP-1 – deska dvou sériových kanálů

Stav: **emulovaná** (typ `DSP-1`, `src/cards/dsp1.*`, USART `src/chips/i8251.*`) od 2026-10-07, test `test_dsp1`.
Tesla Elstroj / Liberec / Eltos-DIZ, 3WN 120 05, deska 3WK 055 05. Ověřeno programy XMODEM J. Bíby
(`media/dsp-1`) pod CP/M oběma směry.

## Podklady

- **Návod Tesla:** OCR (doplněné a opravené) `..\SAPI.CZ\sapi\dsp-1.php`, sken `..\SAPI_DOKU\Manuály_desek\DSP-1\DSP-1.pdf`
  a `..\SAPI_DOKU\skeny_dokumentace_bez_retusi_Petr\DSP-1_s_retusi\TMP*.tif` (originální schéma Tesla mezi skeny
  chybí, TMP19–23).
- **Replika EC1045** (EAGLE, PDF): `..\SAPI_DOKU\sapi_eagle\REPLIKY\DSP-1\`. Fotky `TMP_026/027` ve složce skenů.
- **Programy J. Bíby** (2013, `..\SAPI_DOKU\SAPIMAN\CPM_PROGRAMY\XMODEM\DSP-1\`, kopie v `media/dsp-1/`): `DOPCP`
  (soubor do PC), `DOCPMP` (soubor z PC), varianty `…X3` pro konektor X3, zdrojáky `.MAC`, popis
  `IDENTIFIKACE.txt`. Porty 48h (X2), 4Ah (X3), 4Ch (maska přerušení), 9600 Bd, úplný kabel s řízením.

## Zapojení

- **Adresa:** horní číslice portů = SA2 (74LS85 D6A, **sepnuto = bit adresy 0**; pull-upy 4k7). Pořadí bitů na
  přepínači: návod 1..4 = A7, A5, A6, A4, replika shora A7, A6, A5, A4 (nejasnost; emulace volí číslici přímo).
  Spodní číslici dekóduje 74LS138 D5A z A3..A1 (**A0 = C/D obou 8251**). SA1 je šest samostatných spínačů,
  každá funkce si vybere spodní, nebo horní skupinu:

| SA1 | Výstup 138 | Porty | Funkce |
|---|---|---|---|
| 2 / 6 | Y0 / Y4 | X0, X1 / X8, X9 | kanál 2: 8251 D3B, **konektor X2**, rychlost SA3 |
| 3 / 7 | Y1 / Y5 | X2, X3 / XA, XB | kanál 1: 8251 D4B, **konektor X3**, rychlost SA4 |
| 4 / 8 | Y2 / Y6 | X4, X5 / XC, XD | registr povolení přerušení (jen zápis, čtení FFh) |

  X6/X7 a XE/XF nic nedekóduje. Sepnutí obou spínačů jedné funkce by zkratovalo dva výstupy 138 (neplatné).
  /AEN (X1/13) a /RES dekodér vypínají. J. Bíba: horní číslice 4, SA1 6, 7, 8 (48h, 4Ah, 4Ch).
- **8251:** CLK = Φ2 sběrnice, RESET = /RES, TxC = RxC z multiplexeru 74151 (D5B kanál 2, D6B kanál 1).
  RxD, /CTS, /DSR přes 75154 z konektoru (kolíky 3, 5, 6), TxD, /RTS, /DTR přes 75150 (2, 4, 20). TxE a SYNDET
  nezapojené. Bez kabelu je /CTS neaktivní a vysílač stojí (proto „úplný propojovací kabel“).
- **Rychlost** (SA3 kanál 2, SA4 kanál 1; váhy 4, 2, 1, **N = součet rozepnutých**; spínač 1 = vnější hodiny):
  krystal 12,288 MHz → 7490 (/10) → 7493: N = 0 153,6 kHz (**9600 Bd při ×16**, 2400 při ×64), 1 76,8 kHz, 2 38,4,
  3 19,2, 4 9,6, 5 4,8 kHz, 6 3,2 kHz (7493 jako /6; 200 Bd), 7 vnější hodiny z kolíku 15 (BD).
- **Přerušení:** maska 74LS175 D2B (D0–D3, zápis do X4/XC, /RES ji nuluje), hradla 7403 s otevřeným kolektorem na
  **/INT0** (X1/62, úroveň): D0 & RxRDY kanálu 2, D1 & TxRDY 2, D2 & TxRDY 1, D3 & RxRDY 1. Bere vývody 8251
  (TxRDY jen s TxEN a /CTS).
- **Proudová smyčka** (optrony) a RS-232 běží současně, přepínač není. Příjem ze smyčky jde na kolík 16 (RDD)
  a do 8251 se dostane propojkou 16–3 v kabelu.

## Emulace

- **Parametry:** `high` (SA2, výchozí 4), `ch2` (`X0`/`X8`/`none`, výchozí `X8`), `ch1` (`X2`/`XA`/`none`, `XA`),
  `int` (`X4`/`XC`/`none`, `XC`), pro každý konektor sekce `x2` a `x3`: `rate` (N, výchozí 0 = 9600 Bd),
  `ext_hz` (vnější hodiny), `connected` (DCD), `cts` (úplný kabel, terminál připraven) a `serial` (TCP, COM,
  pacing) jako u ostatních sériových karet. Výchozí nastavení odpovídá J. Bíbovi.
- Kanály jsou v okně Sériový terminál a v MCP jako „DSP-1 X2 #48“ a „DSP-1 X3 #4A“. Výchozí TCP porty (když se
  zapnou) 2324 a 2325.
- **RTS se k PC nepřenáší:** J. Bíba jím řídí příjem (RTS jen při čekání na znak). Emulace místo toho nepošle další
  znak, dokud SAPI předchozí nepřečte (`pacing`).
- **Test** `test_dsp1`: porty, 9600 i 4800 Bd, maska a INT0, /RES; CP/M ze ZRMD-1 s DSP-1, `DOPCP` pošle soubor
  na stranu PC (přijímač XMODEM emulátoru), `DOCPMP` přijme text a `DOPCP` ho vrátí beze změny.

## Nejasnosti k ověření na HW

1. Pořadí bitů A5/A6 na přepínači SA2 (návod a replika se liší).
2. Úroveň výstupu 75154 při nezapojeném vstupu (/CTS, /DSR, RxD bez kabelu): emulace bez kabelu `cts = false`.
3. Deska v. č. 44127 na fotce má SA1 2, 7, 8 a SA2 jinak než J. Bíba: s čím byla používaná?
