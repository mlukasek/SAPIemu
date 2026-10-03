# ZRMD-1 / ZRD-1V – zálohovaný RAM disk

Zdroje:
- `..\ZRMD-1\GAL\deport60.eqn`, `mem.eqn`, `porty.eqn`
- `..\ZRMD-1\ZRMD-1.pdf`
- `H:\SAPI-1\sapi.cz_BKP_2023-02-22\sapi\zrmd-1.php`
- `..\ZRD-1V\DESCRIPTION`

## ZRMD-1

- **Paměť:** až 8× SRAM 512 KB, celkem 4 MB. Emulátor počítá s plným osazením, kapacita je konfigurovatelná.
- **Přístup jen přes I/O porty, okno v paměti není.**
- **Dekódování:** A7=0 a A5·A6=1, tedy rozsah 60–7Fh. Adresy pevně určuje GAL.

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
- **Ochrana zápisu:** DIP přepínače S1–S4. Zapnutý přepínač (na GND) povoluje zápis do své dvojice čipů (1 MB). Zápis do chráněného čipu se tiše neprovede.
- **Zálohování:** MAX691 s článkem NiMH/NiCd nebo napájením ze sběrnice Uz.
- **Adresní vodiče SRAM** jsou na DPS fyzicky přeházené. CPU to nevidí, emulátor používá lineární pole 4 MB.

## ZRD-1V

- **Programový model** je stejný jako u ZRMD-1.
- **Chybí** RTC a ochrana zápisu po čipech.
- **Bázová adresa** se nastavuje jumpery na 74LS688, výchozí je 60h.
- **Zamykání:** MikroMon při startu zamyká ZRD-1V proti zápisu sekvencí `OUT 61h,FF; OUT 62h,FF; OUT 63h,3F`, prodleva, `OUT 60h`. Mechanismus nutno ještě prostudovat.

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

## Nejasnosti

- Které adresní rozsahy chrání přepínače S1–S4: přiřazení čipů z výběru přes A19–A21 je třeba ověřit.
- Zamykání ZRD-1V z MikroMonu (viz výše).
