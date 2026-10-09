# KZD-1 – kazetopáskový záznamník dat

Stav: **emulovaný** od 2026-10-09 (`src/chips/kzd1.*`, připojený k DPP-1 parametrem `kzd1`), sestava
`machines/zps/ZPS-2_MIKOS_K.sapi` s MIKOSem/K V5.0, test `test_kzd1`. **Návod k obsluze:** `docs/navod-kzd1.md`.

Výrobce ZPA Košíře (KZD 1 stolní se zdrojem, KZD 1P panelový bez zdroje), návod OTS-N86/970 z 11/1986, opravenka
12/1987. Datová kazeta ISO 3407 (Philips, pásek 3,81 mm, 86 m na stranu). K SAPI-1 ho připojuje TESLA ELTOS DIZ
přes DPP-1 s operačním systémem MIKOS/K V5.0 (1988), k PMD 85 přes 8255 na K4 a ROM rozšíření BASICu.

## Podklady

- Návod: `..\SAPI_DOKU\KZD-1\kzd1.pdf` (29 stran, sken; shodný s pmd85.borik.net `did=71`). Obsahuje rozhraní,
  rychlosti, časy rozběhu, schémata obou desek (záznam a čtení obr. 4, řízení motorů obr. 6), zapojení konektoru (obr. 9, 10).
- MIKOS/K: `..\SAPI_DOKU\MIKOS5_K\` obsahuje:
  - `MIKOS5K.DOC`: návod k MIKOSu/K a kopírovacímu programu CKM, včetně kabelu k DPP-1;
  - `KZD.MAC` (6. 4. 88): zdroják verze v EPROM;
  - `Překlad\` (PRN, HEX);
  - `Binárky\MIKOS5K.BIN`: tři EPROM 2716, Monitor V5.0E a MIKOS/K;
  - `CKM.MAC`;
  - `MIKOS-K\`: sken výpisu verze 18. 5. 88.
  
  Zdroják verze 18. 5. 88 je v `..\SAPI_DOKU\SAPI_EPROM\ZPS-2\V5\KZD2.MAC` (varianta pro IMS-2 / DPS-1).
- PMD 85: `..\SAPI_DOKU\KZD-1\` (`KZD.ASM`, `kzd.gif` s návodem k BASICu, `KZD_SAPI.txt` = ovladač upravený
  pro MIKROBASIC na SAPI), wiki https://pmd85.borik.net/wiki/KZD-1 a `did=62` (ROM ovladače, schéma kabelu,
  komentovaný disasembl R. Bórika).

## Zapojení (návod KZD-1)

- Vstupy (TTL, * = aktivní L): DATA (záznam), D.P.* (data platná, sestupná hrana zapíše úroveň DATA), ZÁZNAM*
  (L = záznam a mazání), CITLIVOST (L = snížená, pro kontrolní čtení), VPŘED*, RYCHLE VPŘED*, VZAD*,
  RYCHLE VZAD*. VPŘED* a RYCHLE VPŘED* současně = převíjení vpřed, VZAD* a RYCHLE VZAD* současně = převíjení vzad.
- Výstupy: DATA 1 a DATA 0 (impuls H 40 µs za každý přečtený bit na dané stopě, monostabil 74121 39k × 1,5n),
  MOTOR 1 a MOTOR 2 přetížen (H; jediný údaj o konci pásky), ZÁZN. U.V.* (záznam uvolněn, L; platí jen při
  ZÁZNAM* = L).
- **Záznam „CNRZ 1“ ve dvou stopách:** každý bit je jedna změna magnetizace, nula na stopě 0, jednička na
  stopě 1 (dva klopné obvody J-K, obr. 1). Čtení dává impulsy obou stop. Spojit je v jeden tok bitů musí
  počítač, hodiny jsou v impulsech samotných.
- Rychlosti: čtení a záznam 48 cm/s, vyhledávání 96 cm/s, převíjení 1,44 m/s. Rozběh a zastavení nejvýš
  50 ms (vyhledávací rychlost 100 ms). Hustota max. 21 bit/mm, asi 10 kbit/s.
- Dva motory GMM 26 (každý táhne jednu cívku, M2 vpřed, M1 vzad), součet jejich otáček se drží konstantní.
  Přetížení hlídá komparátor proudu s velkou integrační konstantou: ohlásí se až po „několika setinách
  sekundy“, rozběhové proudy ne.
- Ochrana proti zápisu: kazeta má na každé straně vylamovací zoubek. Mikrospínač při zachovaném zoubku propojí
  ZÁZNAM* zpět na výstup ZÁZN. U.V.*. Záznamník sám zápis nezablokuje, jen to ohlásí, kontrolu dělá program.

### Kabel KZD-1 → DPP-1 X2 (návod MIKOS/K)

DPP-1 na 30h–37h (1S1 = 1, 2S1 = 0, 3S1 = 1), 1PA výstup (1S2 = 1, 2S2 = 0), 1PB vstup (1S3 = 0), řídicí slovo
8Bh na 33h.

| KZD-1 | DPP-1 X2 | 8255 | Signál |
|---|---|---|---|
| 20 | 2 | PA0 | VPŘED* |
| 22 | 8 | PA1 | RYCHLE VPŘED* |
| 24 | 3 | PA2 | VZAD* |
| 30 | 7 | PA3 | RYCHLE VZAD* |
| 27 | 5 | PA4 | ZÁZNAM* (v návodu MIKOSu „záznam povolen“) |
| 23 | 1 | PA5 | D.P.* (data platná pro zápis) |
| 19 | 6 | PA6 | CITLIVOST |
| 25 | 4 | PA7 | DATA pro záznam |
| 17 | 30 | PB7 | DATA 1 |
| 21 | 24 | PB6 | DATA 0 |
| 28 | 25 | PB5 | MOTOR 1 přetížen |
| 26 | 29 | PB4 | MOTOR 2 přetížen |
| 29 | 27 | PB3 | ZÁZN. U.V.* (záznam uvolněn) |

Země: KZD 3, 4, 5, 6 → X2 17, 19. Piny X2 → 8255 jsou podle netlistu repliky DPP-1.

## MIKOS/K V5.0

- EPROM na REM-1: Monitor V5.0E 0000–0BFF, MIKOS/K 0C00–17FF (ovladač terminálu SM 7202 od 1400h), od 1800h
  uživatelská EPROM. V repu `roms/mikos/MONITOR_V5.0E_MIKOS_K_V5.0.bin` = `MIKOS5K.BIN` (verze 6. 4. 88,
  shodná s překladem `KZD.MAC`).
- Ovladač: čtení `IN 31h`, `ANI F0h`, hodnota bitu = PB7. Zápis bitu (PISBYT) = `A = bit << 7 | 2Eh` na 30h, pak
  `XRI 20h` (D.P.* dolů) a návrat. Asi 120 µs na bit, asi 1 ms na bajt.
- Formát: bloky se synchronizací FBh, 4bajtové označení (číslo bloku, délka, číslo souboru, kontrolní součet),
  data s typem bloku (`:` název pásky, SOH hlavička souboru, `A` ASCII, `B` binární, EOT poslední),
  CRC-16 (polynom 0007h), jméno 11 znaků, 55h 55h. Mezery: krátká mezibloková 110 ms, dlouhá mezisouborová 819 ms.
  Za posledním blokem souboru je FM, za posledním souborem EM.
- **Kazetu je nutné nejdřív připravit `.K_I`** (název pásky). K_I zapíše za zavaděč úsek 55h (12 000 bajtů,
  z něj zůstane asi 1100) a za ním EM.
- Příkazy: K_I, K_S (uložit), K_L (načíst), K_D (adresář), K_R (převinout na začátek), K_Z (převinout
  a nastavit před název pásky), K_E (na konec záznamu), K_N (soubor podle čísla), K_V (verifikace),
  K_P (kontrola souboru), K_> a K_< (o soubor vpřed, vzad), K_B (o blok), K_K (zapsat EM), K_X (délka bloku).
- **Past K_R:** „K_L JMÉNO“ hledá soubor od místa, kde páska stojí. Po K_R stojí na úplném začátku, hledání
  přečte úsek 55h jako vadný blok a MIKOS/K tuto chybu nerozliší: porovná zadané jméno se starým obsahem bufferu.
  Po K_D v něm zůstala hlavička posledního souboru, takže se „najde“ na špatném místě a čtení skončí
  `CHYBA - NENI KONEC SOUBORU`. Bez předchozího K_D se jméno neshoduje a hledání pokračuje správně. Stejně se chová
  verze 6. 4. 88 i 18. 5. 88 (přeložená M80 pro DPP-1 a vyzkoušená 2026-10-09). Podle návodu je správně
  **K_Z**, nebo jméno s tečkou (`.JMÉNO` = převinout a hledat). Emulace to nechává věrně.

## Kopírovací program CKM

- `media/mikos/CKM.hex` (od 4400h, sapi.cz `ckm_hex`, shodný s překladem `..\SAPI_DOKU\MIKOS5_K\CKM.MAC`). Obsahuje
  ovladač magnetofonu z MIKOSu V5 (DSM-1 na 10h, dálkové ovládání SP-210 přes 8255 na 34h–37h, tj. kanál 2 DPP-1)
  a záznamník volá přes služby MIKOSu/K (157h). Soubor načte do paměti za sebe (asi 44 KB) a zapíše ho na druhé zařízení.
- Postup v sestavě `zps/ZPS-2_MIKOS_K.sapi`:
  1. kazetu KZD-1 připravit `.K_I`;
  2. do magnetofonu (okno Média, DSM-1) vložit nahrávku, nebo program `.hex`/`.bin` převedený na kazetu MIKOS,
     a zmáčknout PŘEHRÁT;
  3. Soubor → Nahrát program → `media\mikos\CKM.hex`, `.G=4400` (menu);
  4. `X` přepne na magnetofon K-10 (výchozí je SP-210 s dálkovým ovládáním, v emulátoru ho DSM-1 nemá);
  5. `L`, jméno (`*` = první soubor), `CTENI HOTOVO ?` → `A`, `JMÉNO --(A/N)?` → `A`;
  6. `B` (binární soubor) nebo `A` (znakový), jméno na KZD-1;
  7. `Q` zpět do monitoru, `.K_D` ukáže soubor, `.K_Z` a `.K_L` ho načte.
  Opačně (KZD-1 → magnetofon) přepne `K`. Na novou (prázdnou) kazetu magnetofonu se píše jméno se zavináčem
  (`@JMÉNO`): CKM pak zapisuje hned bez dotazu, NAHRÁT musí být zmáčknuté předem. Bez zavináče CKM nejdřív
  přehrává a hledá konec záznamu (`CTENI HOTOVO ?`, ve zdrojáku pak `ZAPIS! HOTOVO ?`), na prázdné kazetě donekonečna.
- **Ověřeno:**
  - BASIC EXP V5.0 z kazety MIKOS zkopírovaný na KZD-1, načtený `.K_L` a spuštěný `.J` (`PRINT 6*7` → 42; přes MCP
    2026-10-09). Test `test_kzd1` dělá totéž s malým programem.
  - Cesta zpět (2026-10-09): BASIC z KZD-1 (`K`, `L .BASIC`) na magnetofon (`B @BASIC`, nová WAV, 64 s), přečtený
    zpět z magnetofonu (`M`, `L *`), zapsaný na KZD-1 jako BASIC2, `.K_L .BASIC2`, `J`, `PRINT 6*7` → 42.
- **Chyba v CKM (originál, ne emulace):** po čtení ze záznamníku CKM uzavře soubor kódem `XRA A`, `STA DEBL`
  (komentář „BLOK 255 B“). Do délky bloku MIKOSu/K (427Fh) tak zapíše 0. MIKOS/K ve stejném místě (`DIRSTP`) dělá
  `XRA A`, `CMA`, `STA DEBL`, tedy FFh. Při délce 0 končí blok po každém bajtu (`BUFEND`, `WOB`). Každý další zápis
  na KZD-1, z CKM i `.K_S`, pak dělá jednobajtové bloky s mezerou: asi 70 ms a 67 mm pásky na bajt. Soubor 10 KB
  skončí `CHYBA - KONEC PASKY`. `.K_I` délku nespraví, `.K_X FF` a reset ano (MIKOS/K ji při startu nastaví na FFh).
  Stejné je to s MIKOSem/K 18. 5. 88 (`KZD2.MAC` má stejný `DIRSTP`, `WOB`, `BUFEND`). Vyzkoušeno 2026-10-09 přes MCP:
  CKM `K`, `L`, `M`, `B` zapisoval přes 80 m. `.K_S` 1 KB s `DEBL` = 0 nahrával po 20 s ještě na 12 m, s FFh skončí
  za 2 s na 3,9 m. Postup s obejitím chyby je v `docs/navod-kzd1.md`.
- **Opravená kopie `media/mikos/CKM_opraveny.hex`** (2026-10-09, na přání autora; originál `CKM.hex` zůstává
  beze změny). Vyrábí ji z originálu `python tools/ckm_oprava.py`, který nejdřív zkontroluje původní bajty. Oprava
  neposouvá žádnou adresu:
  - 47D4h: `XRA A` (AFh) → `SBB A` (9Fh), za ní původní `STA DEBL`. Ve větvi KZD je po `ANI 0CH`, `CPI 0CH`
    a `JZ CLOA3` vždy A < 0Ch, příznak CY je tedy nastavený a `SBB A` dá FFh, stejně jako `XRA A`, `CMA` v MIKOSu/K.
    Vložit chybějící `CMA` by posunulo kód a všechny skoky za ním.
  - 4467h: úvodní nápis `KOPIROVACI PROGRAM  MGF - KZD-1` (před ním tři odřádkování) → `OPRAVENY KOPIR. PROGRAM
    MGF-KZD-1` (dvě odřádkování), stejná délka, aby kopie byla poznat na obrazovce.
  - Test `test_kzd1` pouští CKM s originálem i opravenou kopií: po čtení z KZD-1 (`K`, `L .VZOR`) je `DEBL` 0,
    resp. FFh.

## Emulace

- **Mechanika** (`Kzd1`): poloha pásky v mikrometrech, rozběh a doběh 9,6 m/s² (48 cm/s za 50 ms), rychlosti
  0,48 / 0,96 / 1,44 m/s. Na konci pásky se tažný motor zastaví a po 30 ms hlásí přetížení (vpřed M2, vzad M1).
  Zavaděč 0,4 m na obou koncích je bez magnetické vrstvy (záznam tam nic nezapíše).
- **Páska** je seřazený seznam změn magnetizace (poloha, stopa) pro každou stranu. Čtení při libovolné rychlosti
  a směru dává impulsy 40 µs (monostabil není znovu spustitelný). Záznam maže, co hlava přejede, a přidá změnu
  při každé sestupné hraně D.P.*. Během záznamu čtecí výstupy nic nedávají (předpoklad).
- **Soubor kazety `.kzd`:** `KZD1TAPE`, verze, délka, zoubky A a B, počty a seznamy změn obou stran (u32 poloha
  << 1 | stopa). Nová prázdná kazeta se vytvoří, když soubor chybí. Do souboru se zapisuje při vysunutí, ukončení a
  uložení stavu. Ve stavu (`.sapistate`) je jen mechanika.
- **Parametry DPP-1:** `kzd1_channel` (1 = X2 jako MIKOS/K, 2 = X3) a sekce `kzd1` s `cassette`. Kanál nesmí
  sdílet s PMD32-SD ani tiskárnou. Při portu A jako vstup (po resetu 8255) jsou řídicí vstupy v H = vše stojí.
- **Okno Média:** kreslená kazeta s cívkami (navinutí podle polohy, otáčející se náboje, hlava při záznamu
  červená), strana a poloha v metrech, pohyb, přetížení, zoubky A a B, Vložit, Nová kazeta, Vysunout, Otočit
  (druhá strana, poloha se zrcadlí).
- **Program rovnou na kazetu:** okno Média → Vložit… → soubor `.hex` nebo `.bin` (jméno, u `.bin` adresa, start).
  Emulátor vyrobí novou kazetu `work/kzd-1/JMÉNO.kzd` přesně tak, jak by ji zapsal MIKOS/K: úsek 55h z K_I, blok
  s názvem pásky, FM, hlavička, bloky `B` (poslední EOT), FM a EM, se stejnými rozestupy bitů a mezerami. Pak stačí
  `.K_Z`, `.K_L JMÉNO` a `.J`. Přes MCP `kzd1` s `program` (a `name`, `volume`, `address`, `entry`).
  - Zjištěno při tom: MIKOS/K při zápisu dělá za FBh a za čtyřbajtovým označením bloku delší pauzu (asi 200 µm,
    400 µs). Čtení na ni spoléhá. Bez ní se čtení posune o bit (jména a data vyjdou posunutá).
- **MCP:** nástroj `kzd1` (stav, `insert`, `new`, `program`, `eject`, `flip`, `tab_a`, `tab_b`, `flush`), `describe`
  u DPP-1.
- **Ověřeno** (`test_kzd1`): záznam bajtů přes 8255 a jejich přečtení zpět, převinutí do přetížení motoru,
  ZÁZN. U.V.* se zoubkem a bez něj, soubor kazety. MIKOS/K: K_I, K_S 1 KB, K_D (`00 PASKA1`, `01 VZOREK`), K_Z,
  K_L do vymazané paměti (data souhlasí), K_S bez zoubku → `ZAZNAM NENI POVOLEN`, kopie přes CKM, kazeta vyrobená
  z BASICu EXP (`.K_D`, `.K_Z`, `.K_L BASIC`, `.J`, `PRINT 6*7`).

## Nejasnosti k ověření na HW

- Co dávají čtecí výstupy DATA 0 / DATA 1 během záznamu (emulace: nic)? V obr. 4 je hradlo MH8438 na výstupu
  spojené s linkou ZÁZNAM, není čitelné.
- Za jak dlouho po zastavení pásky na konci se ohlásí přetížení (emulace 30 ms) a jak rychle zmizí po rozjezdu
  opačným směrem?
- Délka průhledného zavaděče skutečných datových kazet (emulace 0,4 m) a kde po K_R přesně stojí hlava.
- Skutečná rychlost převíjení s plnou a prázdnou cívkou (emulace stálá 1,44 m/s).
- Chová se K_L po K_D a K_R na skutečném KZD-1 stejně (chyba `NENI KONEC SOUBORU`)?
