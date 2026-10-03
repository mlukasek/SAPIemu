# RAM-1Z – dynamická RAM 64 KB

Zdroje:
- `H:\SAPI-1\sapi.cz_BKP_2023-02-22\sapi\ram-1z.php`
- `H:\SAPI-1\RAM-1Z\DESCRIPTION`

## Parametry

- **Paměť:** 4116 DRAM, 4 řady po 16 KB. Osadit lze 16–64 KB.
- **Refresh:** vychází ze signálu Z80 /REF, takže deska funguje **jen s JPR-1Z/ZI**.
- **Dekodér PROM D3B (74S571):** dumpy shodné s HEX v návodu:
  - `H:\SAPI-1\Petr_repliky_k_vyrobe\DMR-1\RAM-1Z_PROM_B9.hex`, `RAM1Z_B9.hex`;
  - `...\AND-1Z-89\SD-PROM-PROG\RAM1Z_B9.571`.
  - Vstupy podle schématu: A0–A5 = A10, A11, AZ12–AZ15 (AZ = zachycené 7475 během MRQ), **A6 = S1/1,
    A7 = S1/2** (pull-up 4k7, sepnuto = 0), A8 = MAP1 (X1/21) přes S1/4, nebo zem přes S1/3.
  - Výstupy jsou aktivní v nule a vybírají řady: 7 → 0000–3FFF, B → 4000–7FFF, D → 8000–BFFF, E → C000–FFFF, F → nic.

## Konfigurace

| S1/S2 | RAM |
|---|---|
| 00 | 64 KB |
| 01 | bez F800–FFFF |
| 10 | bez F000–FFFF |
| 11 | žádná |

- MAP=1 navíc odebere 0000–3FFF.
- Z výroby je nastaveno 62 KB, MAP nepoužit.
- V OCR tabulce je na 0x117 „E“, hex obsahuje FB. Jde o chybu OCR.

- **Chyba návodu:** pro 60 KB píše „S1 a S2 rozpojené“, to ale vybere blok 0xC0 (žádná RAM). S touto PROM
  je 60 KB = S1/1 sepnutý a S1/2 rozpojený. ZPS-4 používá 62 KB bez MAP.

## Nejasnosti

- Potvrdit nastavení přepínačů pro 60 KB na skutečné desce.
