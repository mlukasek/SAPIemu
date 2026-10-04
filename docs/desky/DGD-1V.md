# DGD-1V – grafický displej 320×240 s výstupem VGA (Libor Lasota)

Podklady (mimo repo, `..\DGD-1V\`): popis `Popis-DGD-1V-rev.-1.1.pdf` (specifikace, mapa videoram, propojky S1
a S2, schéma, rozpiska), schéma `DGD-1V\schema1–6.png` (DPLA0048DGD rev 1.0), fotky autorovy desky
`Foto_moje\DGD-1V_Martin_*.jpg`, testovací program `TEST.BAS` a jeho výstup `DGD-1V_Martin_SCR.jpg`.

Emulace: varianta třídy DGD-1 (`src/cards/dgd1.*`, typ `DGD-1V`). Sestava `machines/zps/ZPS-2_MIKOS_V5_DGD-1V.sapi`
(ZPS-2 s MIKOSem V5 a BASICem EXP V5.0/G). Test `test_mikos` (BASIC EXP V5.0/G a dekodér S2).

## Parametry

- Kompatibilní s DGD-1: 320×240 bodů, 1 bit na bod, 16 KB v jedné čtvrtině adresního prostoru.
- Řádek y začíná na základ + y · 40h a má 40 bajtů. Řádky 240–255 a sloupce 40–63 se nezobrazují. Bit 7 je
  bod vlevo. Adresa: A15–A14 báze, A13–A6 řádek, A5–A0 bajtový sloupec.
- SRAM 62256 (využito 16 KB), bez PROM.
- Výstup VGA 640×480/60 Hz (oscilátor 25,175 MHz), každý bod je 2×2 body VGA. Konektor D-sub HDF 15 (K2).
- Bez přerušení.

## Propojky

- **S1, barva** (/BL, /RD, /GH, /GL, osazená = složka vypnutá): 15 barev, vše rozpojené = bílá. Stejné jako
  u AND-1V. Autorova deska (fotka): /BL a /GL osazené = jantarová (potisk „AMB“).
- **S2, adresa a mapování** (74LS85 IC27 porovnává A15, A14, MAP1, MAP2, /RES výběr vypne), osazená = 0:
  - /A15 /A14: báze v negovaném tvaru (obě rozpojené = C000h, /A14 osazená = 8000h, /A15 = 4000h, obě = 0000h);
  - MAP1EN, MAP1LOW a MAP2EN, MAP2LOW jako u CGA-1V: obě rozpojené = MAP se ignoruje, EN = při MAP = H, EN + LOW
    = při MAP = L, jen LOW = karta vypnutá. Obě podmínky platí současně.
  - Popis uvádí dva příklady: C000h bez mapování (minimální systém, výchozí v emulátoru) a 0000h s MAP2EN
    (karta jen při MAP2 = H, odpovídá PROM původní DGD-1).

## Časování a přístup CPU

- Bodový takt 25,175 / 2 = 12,59 MHz, bajt = 8 bodů (636 ns). Čtyři body patří CPU (adresní multiplexer na
  CPU), čtyři videu (list 6 schématu).
- /STSTB se SEL nastaví RDY = 0, čekání skončí v okně CPU (IC4A/B, IC5A). I/O cykly a /RES čekání ruší, takže
  IN/OUT s A8–A15 v oblasti karty nečekají.
- **Emulace:** čeká se do konce aktuálního okna 4 bodů (318 ns), stejně přibližně jako u CGA-1V.

## Emulace – parametry

`base` (S2 /A15 /A14), `map1`, `map2` (`ignore`, `h`, `l`, `never`), `color` (S1: white, green, amber, red,
blue, cyan, yellow, magenta; mění se i za běhu přes Zobrazení → Barva), `wait_states`.

Okno GUI „DGD-1V“, snímek přes MCP `screenshot display=DGD-1V` a `sapiemu-cli --display DGD-1V --screenshot`.

## Ověřeno v emulátoru

- BASIC EXP V5.0/G pod MIKOSem V5 kreslí na správné bity (CLSG, PLOT), jako s DGD-1.
- Autorův `TEST.BAS` (rámeček, úhlopříčky, LABEL, sinusovka) vypadá jako fotka obrazovky skutečné desky.

## Nejasnosti

- Na fotce autorovy desky není jisté, které propojky S2 jsou osazené (modrá propojka u /A14, /A15?). Sestava
  používá C000h bez mapování, protože obrazovka je z MIKOSu V5 s RAM-1 na 4000–BFFF.
- Přesný počet čekacích taktů (synchronizace IC4A/B).
