# ZRMD-1 / ZRD-1V – zálohovaný RAM disk

Zdroje:
- `..\ZRMD-1\GAL\deport60.eqn`, `mem.eqn`, `porty.eqn`
- `..\ZRMD-1\ZRMD-1.pdf`
- `H:\SAPI-1\sapi.cz_BKP_2023-02-22\sapi\zrmd-1.php`
- `..\ZRD-1V\DESCRIPTION`

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
- **Ochrana zápisu** (GAL `mem.eqn`, ověřeno): DIP přepínače S1–S4. Zapnutý přepínač (na GND) povoluje zápis.
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
- **„Zamykání“ z MikroMonu:** MikroMon při startu provede `OUT 61h,FF; OUT 62h,FF; OUT 63h,3F`, prodlevu
  a `OUT 60h`. Popis ZRD-1V žádný zamykací mechanismus nemá, sekvence tedy nejspíš jen zapíše bajt na poslední
  adresu 3FFFFFh a nechá tam adresu „zaparkovanou“. Emulátor to provede doslova. Ověřit u autora MikroMonu.

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
- **Referenční obrazovka:** fotka `..\AND-1Z-89\Foto\*cpm-scr.jpg` ukazuje start „JPR-1Z 62K CP/M V2.2Z, A: ZRMD1/4MB BOOT SS256“.

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
- **Nečtené bity:** horní 4 bity dat RTC nikdo nebudí, čtou se jako 1.

## Nejasnosti

- Nulují /RES adresní čítače a latch portu 63h? Emulátor předpokládá, že ano.
- Které bity portu 63h vedou přes JP2 na MAP1 a MAP2: emulátor předpokládá b6 → MAP1, b7 → MAP2.
- Zamykání ZRD-1V z MikroMonu (viz výše).

## ZRD-1V – doplnění z rozboru 2026-10-04

Zdroje:
- `..\ZRD-1V\DESCRIPTION`, rozpiska, řetězce v binárním `ZRD-1V.sch` (PDF schéma chybí);
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
- **Rev 2.0-alpha:** list „NAPAJENI, ZAMEK ZAPISU, INDIKACE“ přidává zámek zápisu a LED přístupu
  (monostabilní obvody 74LS123, 20 ms). To může vysvětlit „zamykací“ sekvenci MikroMonu.

