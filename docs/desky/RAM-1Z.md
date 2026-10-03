# RAM-1Z – dynamická RAM 64 KB

Zdroje:
- `H:\SAPI-1\sapi.cz_BKP_2023-02-22\sapi\ram-1z.php`
- `H:\SAPI-1\RAM-1Z\DESCRIPTION`

## Parametry

- **Paměť:** 4116 DRAM, 4 řady po 16 KB. Osadit lze 16–64 KB.
- **Refresh:** vychází ze signálu Z80 /REF, takže deska funguje **jen s JPR-1Z/ZI**.
- **Dekodér PROM:** 74S571. Hex je jen v HTML stránce, samostatný dump v repu není.
  - Vstupy: A0..A5 = A10..A15, A6/A7 = přepínače S1/S2, A8 = MAP.
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

## Nejasnosti

Který přepínač budí který vstup PROM. Tabulka 64/62/60 KB v manuálu přesně neodpovídá binárním konfiguracím PROM.
