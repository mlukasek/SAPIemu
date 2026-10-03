# RAM-1 – deska dynamické paměti RAM

Zdroje: `BKP\sapi\ram-1.php` (OCR dokumentace ze ZPS-2), obrázky `BKP\sapi\obrazky\ram-1.png`, `nram-1.png`.
RAM-1A se liší jen obsahem PROM D1A.

## Zapojení

- **Paměti:** až 3 řady po 8× MHB 4116 (16K × 1 bit). Vyráběly se varianty RAM-1/16K, /32K a /48K.
- **Dekodér D1A** (PROM MH 74S287 v objímce, adresy A10–A15 a propojky S1, S2):
  - Y1 = SEL (deska odpovídá), po 1 KB;
  - Y2/Y4/Y3 = CA0/CA1/CA2 vybírají řadu podle A15–A14: 4000–7FFF řada 1, 8000–BFFF řada 2, C000–FFFF řada 3;
  - 0000–3FFF se nikdy nepoužívá (blokováno v tabulce adres systému).
- **Tabulka 1 (výrobní PROM):**

| S1 | S2 | Adresy | Kapacita |
|---|---|---|---|
| 1 | 0 | 4000–FFFF | 48 K |
| 0 | 1 | 4000–BFFF | 32 K |
| 1 | 1 | 4000–7FFF | 16 K |
| 0 | 0 | 4000–DFFF | 40 K |

- Místo 74S287 jde osadit 74S571 a vybírat paměť signálem S1 ze špičky 21 konektoru X1 (přerušit spoj A–B).
- **Refresh:** skrytý, v periodě T4 cyklu M1. Není zajištěný při dlouhém HALT nebo WAIT. První instrukce po
  RESET nesmí být čtena z RAM-1. Bez čekacích stavů.

## Emulace (`src/cards/ram1.*`)

- Parametry: `fitted_kb` (16/32/48 = osazené řady), `s1`, `s2`.
- **PROM D1A není dumpnutá.** Obsah se generuje z tabulky 1.
- Řada bez obvodů čte FFh (předpoklad), deska ale v dekódovaném rozsahu odpovídá (SEL je jen z PROM).
- Refresh se neemuluje (paměť nezapomíná).

## Nejasnosti

- Dump PROM D1A (RAM-1 i RAM-1A), hlavně jestli výrobní obsah opravdu odpovídá tabulce 1 i pro neúplně osazené
  desky.
- Co CPU čte z neosazené řady.
