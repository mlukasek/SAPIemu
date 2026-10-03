# RMD-1 (RMD-1Z) – RAM disk

Stav: **podklady sebrané, emulace zatím není** (soupis 2026-10-04). RMD-1 a RMD-1Z se liší jen názvem.

## Podklady

- **Návod (OCR) s přílohou** (mapa portů, PROM v HEX): `BKP\rmd-1.php`.
- **Zpětně nakreslené schéma:** `BKP\pdf\rmd-1.pdf` (vektorové). Stránka upozorňuje, že rozpiska neodpovídá
  schématu.
- **Fotka:** `H:\SAPI-1\Foto\Aukro\RMD-1Z_Aukro.jpg`.

## Zapojení

- **Paměť:** 32× 4164 = **256 KB** ve 4 řadách, nebo 41256 = 1 MB. Vlastní čítač refreshe taktovaný z FI2.
- **Bez baterie:** po zapnutí se musí inicializovat a před vypnutím zkopírovat na disketu.
- **X1:** jen /IOR, /IOW, A0–A7, D0–D7, FI2 a napájení. Žádné RDY, INT, DMA, MAP, /AEN ani RESET, takže stav
  registrů po zapnutí není definovaný.
- **Dekodér:** 74S287 v objímce, vstupy A0–A7 = A0–A7 sběrnice, CE trvale aktivní.
  - Standardní obsah: 40 = FB, 41 = FE, 42 = FD, 43 = F7, 45 = FE, jinak FF.
  - O1 (bit 0) = nahrání adresy (porty 41/45), O2 (bit 1) = data (42). Výstupy jsou sečtené s /IOW, O2 i s /IOR.
  - Čtení 40/41/43/45 vrací plovoucí sběrnici.
  - Základ 40h jde přesunout výměnou PROM. Jeden systém měl druhou desku na 50h.

## Registry (základ 40h)

| Port | Směr | Funkce |
|---|---|---|
| 40 | W | D0–D3 → 74LS174. D0/D1 = A16/A17 → 74LS138 → /RAS0–3 (řada). D2/D3 = A18/A19 → DRAM A8, jen u 41256 (u 256 KB se ignorují). |
| 41 | W | data → A8–A15 (dva 74LS193), A0–A7 := 00h |
| 45 | W | jako 41, ale A0–A7 := 80h (A2 sběrnice jde na load bitu 7) |
| 43 | W | „příprava čtení“: spustí čtení DRAM do záchytného registru 74LS373 |
| 42 | R/W | čtení vrátí záchytný registr a spustí další čtení, zápis jde přes 74LS374 do DRAM. Každý přístup zvýší A0–A15 (přenos A7→A8 ano, do A16 ne). |

## Software

Tři nezávislé BIOSy (Peřina `BIOS.MAC`, ELIS, JB `BIRMD.MAC`) dělají totéž:
1. `OUT 40,sektor`.
2. `OUT 41/45,L(stopa)`: port 45, když je bit 8 stopy 1.
3. Pro čtení `OUT 43` a 128× `IN 42`, pro zápis 128× `OUT 42`.

- **Adresa:** sektor<<16 | (stopa & FFh)<<8 | (bit 8 stopy)<<7, záznam 128 B.
- **DPB 256 KB:** SPT 4, BSH 3, DSM 255, DRM 63, OFF 0. **1 MB (JB):** SPT 16, BSH 4, DSM 511, DRM 127. Test
  velikosti v JB spoléhá na to, že sektor 15 se u 256 KB desky zrcadlí na sektor 3.
- **Peřinův CP/M** má RAM disk jako E:. `RSYS62A.COM` ho dá na A:, `RTEST.COM` ho inicializuje.
- **ELIS** má `INIRMD5.COM` (inicializace 256K/1M s nastavitelným základem).

## Model pro emulaci

- Záchytný registr = mem[a], po `OUT 43` a po každém čtení 42 se adresa zvýší.
- Obsah se po vypnutí ztrácí. Volitelně se dá ukládat do souboru (pohodlí emulátoru, skutečná deska to
  neumí).

## Nejasnosti

- **PROM O3/O4:** HEX z návodu dává 40 → bit 2 (O3) a 43 → bit 3 (O4). Schéma ale pojmenovává O3 (pin 10)
  „43H“ a O4 „40H“. Software potřebuje 40 = řada a 43 = příprava čtení. Do ověření platí bit 2 → řada,
  bit 3 → příprava čtení.
- Časové limity: INIR při 2/4 MHz, je NOP po `OUT 43` nutný?
