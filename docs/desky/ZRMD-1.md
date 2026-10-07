# ZRMD-1 / ZRD-1V – zálohovaný RAM disk

Zdroje:
- `..\SAPI-karty\ZRMD-1\GAL\deport60.eqn`, `mem.eqn`, `porty.eqn`
- `..\SAPI-karty\ZRMD-1\ZRMD-1.pdf`
- `H:\SAPI-1\sapi.cz_BKP_2023-02-22\sapi\zrmd-1.php`
- `..\SAPI-karty\ZRD-1V\DESCRIPTION`

## ZRMD-1

- **Paměť:** až 8× SRAM 512 KB, celkem 4 MB. Emulátor počítá s plným osazením, kapacita je konfigurovatelná.
- **Přístup jen přes I/O porty, okno v paměti není.**
- **Dekódování** (GAL `deport60.eqn` / `porty.eqn`): A7=0 a A5·A6=1. Port 60h je dekódovaný úplně
  (A0–A4 = 0), 61–63h jen pro zápis, 70–7Fh RTC (A4=1, registr A0–A3). **64–6Fh nejsou dekódované.**

| Port | Funkce |
|---|---|
| 60h | Data (čtení i zápis). Po každém přístupu se zvýší spodní bajt adresy. |
| 61h | Jen zápis: A0–A7 (načte čítače). |
| 62h | Jen zápis: A8–A15. |
| 63h | Jen zápis: b0–5 = A16–A21. b6–7 volitelně budí MAP1/MAP2 na sběrnici (přes JP2). |
| 70–7Fh | RTC 72412 (typ RTC-72421), 4bitové registry 0–F. |

- **Autoinkrement:** port 61h plní dva 74LS193. Přenos z vyššího čítače není zapojen, takže **adresa se zalamuje v rámci 256 B** a nepřetéká do portu 62h.
- **Porty 61–63h nejdou číst.**
- **Přerušení RTC:** jumper JP3 volí INT0 nebo INT1/NMI.
- **Výběr čipu:** `mem.eqn` vybírá jednu z 8 SRAM podle A19–A21.
- **Ochrana zápisu** (GAL `mem.eqn`, ověřeno): DIP přepínače S1–S4 (v emulátoru i za chodu). Zapnutý přepínač (na GND) povoluje zápis.
  Při čtení jsou čipy vybírány vždy, při zápisu jen s povoleným přepínačem.

  | Přepínač | Čipy (A21 A20) | Rozsah |
  |---|---|---|
  | S1 | 0, 1 (00) | 000000–0FFFFF |
  | S2 | 2, 3 (01) | 100000–1FFFFF |
  | S3 | 4, 5 (10) | 200000–2FFFFF |
  | S4 | 6, 7 (11) | 300000–3FFFFF |
- **Zálohování:** MAX691 s článkem NiMH/NiCd nebo napájením ze sběrnice Uz.
- **Adresní vodiče SRAM** jsou na DPS fyzicky přeházené. CPU to nevidí, emulátor používá lineární pole 4 MB.

## ZRD-1V

- **Programový model** je stejný jako u ZRMD-1.
- **Chybí** RTC a ochrana zápisu po čipech.
- **Bázová adresa** se nastavuje jumpery na 74LS688, výchozí je 60h.
- **Zamykání z MikroMonu (vysvětleno schématem rev 2.1):** MikroMon 1.3 při startu (0053h) provede
  `OUT 61h,FF; OUT 62h,FF; OUT 63h,3F`, smyčku `BC = 3C19h` (400 010 taktů = 100 ms při 4 MHz) a `OUT 60h` (A = 0).
  Rev 2.1 povolí zápis na 60h jen do 20 ms po přístupu na 62h. Zápis po 100 ms kartu **úmyslně zamkne** až do
  resetu. Rev 1.x zámek nemá a zapíše jen 00h na poslední bajt 3FFFFFh, proto MikroMon adresu nastaví tam.

## Obsah RAM disku (`media/zrmd-1/SAPI_CPM_Martin_2019-10-07.bin`)

- **Velikost:** 4 194 304 B, lineární. Offset = `(port63 & 3F) << 16 | port62 << 8 | port61`.
- **Boot kód na offsetu 0:**
  - Zapíše `port63=0, port62=n>>1, port61=(n&1)<<7`.
  - Instrukcí `INIR` z portu 60 čte 128B záznamy.
  - Načte 63 záznamů na CE00h a skočí na E400h.
- **CP/M 2.2, DPB** (na 0x1742, 0x3742, 0x5742, 0x7742):

| Pole | Hodnota | Pole | Hodnota |
|---|---|---|---|
| SPT | 2 | DSM | 1015 |
| BSH/BLM | 5/31 (bloky 4 KB) | DRM | 1023 |
| EXM | 1 | AL | FF00h |
| CKS | 0 | OFF | 128 |

  - Jedna „stopa“ = 256 B: port 62/63 je stopa, port 61 bit 7 vybírá záznam.
- **Adresář** začíná na 0x8000, blok n leží na `0x8000 + n*4096`.
- **Systémová oblast** obsahuje zřejmě 4 kopie 8KB systému.
- **Referenční obrazovka:** fotka `..\SAPI-karty\AND-1Z-89\Foto\*cpm-scr.jpg` ukazuje start „JPR-1Z 62K CP/M V2.2Z, A: ZRMD1/4MB BOOT SS256“.

## Emulace

- **Datový soubor:** volitelný, výchozí je kopie z `media/` ve `work/`.
- **Režimy:**
  - `rw`: zápis se ukládá do souboru.
  - `nosave`: RAM se mění, soubor zůstává nedotčený.
  - `readonly`: zápisy se zahazují, jako by byly zapnuté všechny přepínače WP.
- **Přepínače S1–S4** jsou emulované zvlášť.
- **RTC** bere čas z hostitele.
- **Port** je konfigurovatelný (výchozí 60h).

## Implementace v emulátoru

- **Karty:** `ZRMD-1` a `ZRD-1V` (`src/cards/zrmd1.*`), RTC `src/chips/rtc72421.*`.
- **Konfigurace:** `image`, `working_copy` (výchozí `true` = kopie ve `work/`), `mode`, `chips` (1–8),
  `base`, `write_enable` [S1..S4], `rtc`, `rtc_int` (JP3: `none`/`int0`/`int1`), `map_jp2`.
- **Obrazy Intel HEX** se načtou, ale změny se neukládají (režim `nosave`).
- **Ukládání:** soubor se uloží při ukončení, při načtení jiné sestavy a automaticky 5 s emulovaného času
  po první změně.
- **RTC:** hodiny běží podle místního času PC s posunem, který se mění, když program nastaví čas.
  HOLD, STOP, RESET, 30s ADJ, 12/24 h a periodické přerušení (1/64 s, 1 s, 1 min, 1 h) jsou emulované.
  Rozdíl mezi pulzním a úrovňovým režimem se zatím neemuluje, výstup je vždy úroveň do smazání příznaku.
  Nastavení HOLD zachytí čas toho okamžiku (do 2026-10-07 registry v HOLD neukazovaly aktuální čas a čtení
  postupem z datasheetu, HOLD → BUSY → čtení → HOLD = 0, vracelo starou hodnotu).
- **Den v týdnu** (registr W) má 0 = neděle. `RTC421.COM` J. Bíby (disketa `media/pmd32sd/b_rtc_zdp_zrmd.8sd`,
  zobrazení a nastavení času na ZRMD-1) vypisuje W + 1, středa je u něj tedy 4. Program je v emulátoru ověřený
  (2026-10-07, CP/M ze ZRMD-1, ECMON volba 1).
- **Nečtené bity:** horní 4 bity dat RTC nikdo nebudí, čtou se jako 1.

## Nejasnosti

- Nulují /RES adresní čítače a latch portu 63h? Emulátor předpokládá, že ano.
- Které bity portu 63h vedou přes JP2 na MAP1 a MAP2: emulátor předpokládá b6 → MAP1, b7 → MAP2.
- Zamykání ZRD-1V z MikroMonu (viz výše).

## ZRD-1V – doplnění z rozboru 2026-10-04

Zdroje:
- `..\SAPI-karty\ZRD-1V\DESCRIPTION`, rozpiska, řetězce v binárním `ZRD-1V.sch` (PDF schéma chybí);
- https://www.pmd85.cz/?page_id=2245;
- rev 2.0-alpha v `H:\SAPI-1\LiborLasota\ZRD-1V\ZRD-1V_2-0-alpha_2025-10-16\`.

**Bez PROM a GAL.**

- **Proti ZRMD-1:** bez RTC, bez ochrany zápisu jednotlivých SRAM a **bez výstupů MAP1/MAP2** (web: „bez
  výstupních signálů MAP1/MAP2“, ve schématu nejsou signály MAP). Mapování dělá RAM-1V. **Parametr `map_jp2` je
  u ZRD-1V v emulátoru navíc a musí pryč**, u ZRMD-1 ho prověřit zvlášť. Přerušení nemá.
- **Adresa:** 74LS688 na A2–A7, **propojky S1 = /A7…/A2, osazená = 0**. Potisk „DEFAULT 60h“ = osazené /A7,
  /A4, /A3, /A2 (rev 1.0 měla chybně 70h). Základ je vždy násobek 4. 74LS138 dává /RAM (60h) a /AD1–/AD3
  (61h–63h).
- **Registry:**
  - 61h: dva 74LS193 pro MA0–7, zvyšují se a přetékají po 256 B;
  - 62h: 74LS374 pro MA8–15;
  - 63h: 74HCT374 pro MA16–21, bity 6–7 se neukládají.
  - 74AC138 dekóduje MA19–21 na /SEL0–7 přes cestu /CE obvodu MAX808L (ochrana zápisu při výpadku).
  - 8× AS6C4008 (512 KB).
  - 64h–6Fh a 70h–7Fh se nedekódují.
- **Zálohování:** 2× alkalický článek AAA v držáku a vnější svorky +BAT/− (K2/K3) přes R2 100R. D1/D2 BAT41,
  dohled MAX808L. Web odhaduje výdrž přes 2 roky. Propojka S2: STDBY = napájení z +5V/S4 sběrnice, nebo NO
  STDBY.
- **Schéma** `..\SAPI-karty\ZRD-1V\ZRD-1V.pdf` (rev 1.1, doplnil autor):
  - **/RES nic nenuluje.** Čítače 74LS193 mají CLR na zemi, registry 374 nulování nemají, /RES jen vypne
    dekodér. Emulátor proto u ZRD-1V adresu po resetu ponechá.
  - Číst jde jen 60h. Čtení 61h–63h je dekódované, ale vnitřní sběrnici nic nebudí: vrací nedefinovanou
    hodnotu a navíc ji zapíše do registru adresy (vedlejší účinek). Emulace to nedělá.
  - Čítač se zvyšuje po každém přístupu na 60h a přetéká FF→00 bez přenosu do 62h. MA22/MA23 nejsou (max.
    4 MB).
  - MAX808L blokuje přístup jen při podpětí, propojka S2 STDBY je jen napájení.
  - Autorova deska (fotka rev 1.0): S1 = 60h, S2 osazená.
- **Rev 2.0-alpha:** list „NAPAJENI, ZAMEK ZAPISU, INDIKACE“ přidává zámek zápisu a LED přístupu
  (monostabilní obvody 74LS123, 20 ms). To může vysvětlit „zamykací“ sekvenci MikroMonu.
  - Z binárního `.sch` jsou čitelné jen texty: 74LS123 A/B „(20msec)“, 74LS00, 74LS02, signály IRD, IWR,
    /IRES, /WE, ACCESS, PWRN-1, LED HLMP-3762 a HLMP-3962. Jak se zámek zapíná, z toho nejde poznat.
  - **Emulace:** LED DSK na panelu svítí 20 ms po čtení nebo zápisu dat (port 60h). LED ZAM ukazuje režim zápisu
    karty (autor): zelená `rw` (ukládá se), žlutá `nosave` (jen v paměti), červená `readonly` (zápis zablokovaný).
    Režim jde v konfigurátoru přepnout za chodu („(platí hned)“).

## ZRD-1V rev 2.1 – zámek zápisu a měření baterie (schéma 2026-09-05)

Zdroj: `..\SAPI-karty\ZRD-1V\ZRD-1V_rev20\` (`ZRD-1V.pdf` 3 listy, `schema1-3.png`, Eagle, `DESCRIPTION` rev 2.1). Proti 2.0
vypuštěno zálohování ze sběrnice signálem S4 a doplněny sériové odpory u 74AC138. Emulace: parametr
`revision` (`2.1` výchozí, `1.1`), `battery_mv` (výchozí 3000, platí hned). Test `test_sapi1v` (`zrd_lock`).

- **Dekodér** (IC19 74LS02, IC18 74LS138): /RAM (60h) a /AD1–/AD3 (61h–63h) jsou aktivní při čtení i zápisu,
  IWR jen při zápisu, /RES vypne dekodér. 74LS245 budí sběrnici při čtení kteréhokoli z portů 60h–63h.
- **Zámek** (list 3):
  - IC21B 74LS123 (R6 100k, C25 680n, ve schématu „20msec“) se spouští sestupnou hranou /AD2. **Každý
    přístup na 62h** (čtení i zápis) otevře okno 20 ms, opakovaný přístup ho prodlouží.
  - /WE SRAM = NAND(IWR, Q okna) OR zámek: **zápis na 60h se provede jen uvnitř okna**.
  - RS klopný obvod IC13C/IC13B (74LS00): zápis na 60h mimo okno (IC13A, IC20B) ho překlopí. **Zámek drží až do
    /IRES**, pak se zapisuje zase normálně. Zápis, který zámek spustil, se neprovede.
  - Běžný zápis sektoru (adresa 61h/62h/63h a hned `OTIR` na 60h) se do okna vejde. ECMON a CP/M zapisují
    normálně, `SAVE` v testech funguje.
- **LED:** D1 HLMP-3762 červená „WRITE PROTECT“ svítí, když je zamčeno. D2 HLMP-3962 zelená „ACCESS“ svítí 20 ms
  (IC21A 74LS123 z /OE) po **jakémkoli** přístupu na 60h–63h. Na panelu emulátoru je ZAM = D1, DSK = D2. Rev 1.1
  LED nemá, na panelu jsou zhasnuté.
- **MAX187** (IC22, 12 bitů, vnitřní reference 4,096 V, tedy 1 mV na krok) měří zálohovací baterii (AIN přes R7
  24M):
  - /CS = /Q okna: převod začne prvním přístupem na 62h, když okno nebylo otevřené, a trvá 8,5 µs.
  - SCLK = NOR(/IRD, /AD1): **každé čtení 61h** je jeden takt, DOUT jde přes 74LVC1G126 na **D5**.
  - Pořadí čtení: během převodu D5 = 0, pak 1 (konec převodu), pak D11…D0 a za LSB nuly. Po zavření okna DOUT
    plave (čte se 1).
- **Čtení 61h–63h** zapíše do registru adresy, co je na vnitřní sběrnici: 74LS193 se načítají úrovní /AD1 a
  74LS374 hranou /AD2 / /AD3. Kromě DOUT ji nic nebudí, emulace čte FFh (61h s DOUT na D5) a stejnou hodnotu
  zapíše do registru. Platí i pro rev 1.1 (bez DOUT).
- **Konfigurace vs. zámek (autor):** zámek je stav karty jako na desce, neukládá se do sestavy. Konfigurátor ho
  ukazuje pod parametry („zámek zápisu: zamčeno programem…“), MCP v `machine_info` (`locked`). Parametr „Režim“
  (rw/nosave/readonly) dál řídí jen obraz v emulátoru.

### ZRDTEST

`media/zrd-1v/ZRDTEST.ASM` a `zrdtest.hex` (CP/M, `SAVE 6 ZRDTEST.COM`, spuštění `ZRDTEST [MHz]`, výchozí 4):
1. změří baterii;
2. ukáže, co čte 61h a kam se tím posune adresa;
3. ověří zápis hned po 62h;
4. zkusí zápis za 10…40 ms po 62h a pak za 100 ms.

Na rev 2.1 zjistí délku okna a kartu na konci zamkne (odemkne RESET). Používá jen bajty 3FFFF0h a 3FFFFFh
a vrací je. Překlad `python tools/asm8080.py` (nebo M80). V emulátoru vyjde rev 2.1: 3000 mV, IN 61h = DFh
(čte se během převodu, DOUT = 0), okno 20…21 ms. Rev 1.1: převodník nenalezen, IN 61h = FFh, zámek není.

### Nejasnosti rev 2.1

- Autorova deska je **rev 1.1** (autor), sestavy V to mají. Rev 2.1 se zatím nedá ověřit na skutečné desce.
- Okno 74LS123: schéma píše 20 ms, vzorec 0,37–0,45 · R · C dává 25–31 ms. Emulace: 20 ms.
- AIN přes 24 MΩ: kolik ukazuje skutečná deska proti napětí baterie (vstupní proud MAX187)? Emulace: přesně
  `battery_mv`.
- Čte se volná vnitřní sběrnice (61h–63h) opravdu jako FFh?
