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

## RAM-1A (ZPS-3)

Podklady:
- návod `BKPam-1a.php` (tab. 1 a 2, X1, poznámka RAM-1A-5V s KR565RU6, PROM D1A v Intel HEX);
- strojopisná tabulka PROM `BKP\obrazkyam-1a_pri.png` (také `D11\CPM\JPR-1A\RAM-1A_obsah_PROM.png`);
- skeny `D11\sapi1zps3.zip`, složka `6xk199135`;
- Eagle `D11\sapi_eagle\REPLIKY\RAM-1A\`.

Hardware je stejný jako RAM-1, liší se jen obsah D1A:
- vstupy PROM A0–A5 = A10–A15, A6 = S1, A7 = S2 (rozpojená = 1);
- výstupy D0 SEL, D1 CA0, D2 CA2, D3 CA1;
- nejnižších 16K rozsahu dostane CA0.

| S1 | S2 | Adresy | Kapacita |
|---|---|---|---|
| 0 | 0 | C000–E7FF | 10K |
| 0 | 1 | 0000–BFFF | 48K (standard ZPS-3) |
| 1 | 1 | 4000–FFFF | 48K |
| 1 | 0 | C000–FFFF | 16K |

HEX se přesně shoduje se strojopisnou tabulkou. Nejspíš je z ní přepsaný, fyzický dump to není.

**Pozor:** `BKPam-1.php` obsahuje i dvě tabulky D1A pro RAM-1 (48K a 16K). Emulace RAM-1 je zatím
generuje z tab. 1 a s těmito tabulkami se musí porovnat.

## Nejasnosti

- Dump PROM D1A (RAM-1 i RAM-1A), hlavně jestli výrobní obsah opravdu odpovídá tabulce 1 i pro neúplně osazené
  desky.
- Co CPU čte z neosazené řady.
