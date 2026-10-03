# AND-1Z / AND-1Z-89 – alfanumerický displej

Zdroje:
- `H:\SAPI-1\sapi.cz_BKP_2023-02-22\sapi\and-1z.php` (OCR manuálu Tesla)
- `..\AND-1Z-89\AND-1Z-89.pdf`, `X2_connector_pinout.txt`
- dumpy v `roms/and-1z-89/`

Rozdíl mezi AND-1Z a AND-1Z/89: místo 4× MHB2114 (NM6514) je osazena 1× 6116, desky jsou „plně záměnné“.

## Text a paměť

- **Rozlišení:** 64 sloupců × 20 viditelných řádků, znak 6×12 px, tedy 384×240 px.
- **VRAM:** 2 KB, paměťově mapovaná, bez I/O portů. Výchozí adresa F800–FFFF, `adresa = F800 + řádek*64 + sloupec`.
- **Řádky 20–23** (FD00–FFFF) jsou běžná RAM, kterou CPU čte i zapisuje, ale displej je nezobrazuje. MikroMon tam má proměnné.
- **Dekodér D1/DA1** (74S287, `PROM_D1.hex.bin`):
  - Vstupy: A0=A15, A1=A11, A2=/RES, A3=A14, A4=A13, A5=A12, A6 (neoznačený, předpoklad H), A7=A10.
  - Výstupy: O1 (bit 0) = /SEL, O4 (bit 3) = MA10 (A10 pro RAM).
  - Obsah: všude F1, jen 0x7F = F0 a 0xFF = F8. Vybrán je tedy pouze rozsah F800–FFFF.
- **MAP1:** vede přes přepínač S1 (pull-down 4k7) na CE PROM. MAP1=1 kartu odpojí. Nastavení S1: „MAP vyřazen = S1/1 on, S1/2 off; zařazen = S1/1 off, S1/2 on“.

Jiné adresy VRAM podle systému:

| Systém | Adresa VRAM |
|---|---|
| MIKOS | 3800h |
| CP/M ZPS-3 s AND-1A | E800h |
| CP/M ZPS-4 s AND-1Z | F800h |
| TRS kompatibilní (16 řádků) | 3C00h |

Pro 3800h a 3C00h je potřeba jiná PROM D1 a jiná EPROM. **Jejich dumpy nemáme.** ZZRMD ROM uvádí AND-1Z na C000h při MAP.

## Znakový generátor

- **Soubory:** `AND-1Z_CharGen.bin` (4096 B) a `AND-1Z_CharGen_upr10-1F.bin` (liší se jen znaky 10–1Fh).
- **Osazení:** 2732, nebo 2× 2716 podle JP1/JP2 (2732 → 1-2/1-2, 2716 → 2-3/2-3).
- **Adresa v ROM:** `kód*16 + L`, kde L = 0..11.
  - **L=11 je horní řádek** (čítač řádků čte pozpátku), ROM je tedy uložená „vzhůru nohama“.
  - L=12–15 obsahují 0.
- **Pixely:** D7..D2, D7 je vlevo. D1 a D0 se nepoužívají.
- **Sada znaků** (zobrazitelných je všech 256 kódů):

| Kódy | Obsah |
|---|---|
| 00–1F | rámečková grafika |
| 20–7F | ASCII |
| 80–BF | 64 semigrafických bloků 2×3 (buňky 3×4 px) |
| C0–FF | české znaky KOI-8čs2 |

- **Žádné atributy:** chybí inverze, podtržení, blikání, kurzor i dvojitá šířka. Kurzor kreslí software; MikroMon používá znak FFh.

## Časování

- **Bodový takt:** krystal 10 MHz, 1 px = 100 ns.
- **Znakový takt:** Johnsonův čítač ÷6, tedy 600 ns na znak. První polovina (300 ns, QA=H) patří CPU, druhá displeji.

Horizontální PROM DD4/D24 (`PROM_DD4-D24.hex`):
- Vstupy: A0=H4, A1=H2, A2=H32, A3=H16, A4=H8, A5=H1, A6=H64, A7≈/VS.
- Výstupy: O1=/HB, O2=RCH, O3=TV sync, O4=AZJ sync.
- Viditelné jsou znaky 2..65 (zpoždění pipeline 2 znaky). TV HS je na znacích 81–88.
- RCH je na 107, řádek tedy trvá 64,2 µs (107 × 0,6 µs). **Nejasnost:** může to být 108 znaků.

Vertikální PROM DE6/D35 (74S571, `PROM_DE6_D35.hex`):
- Vstupy: A0=L8, A1=V4, A2=V8, A3=V1, A4=V2, A5=V16, A6=L4, A7=L2, A8=L1.
- Výstupy: O1=/VS_AZJ, O2=RCV, O3=/VB, O4=/VS.
- Snímek má 26 řádků × 12 linek = 312 linek, bez prokládání, asi 20,0 ms (manuál uvádí 20,5 ms).
- Viditelné jsou řádky 0–19. TV VS je v řádku 22, linky L=9..8.

OCR tabulky na sapi.cz se od hex souborů místy liší. Platí hex.

## Přístup CPU

- **Wait stavy:** STSTB spolu s /SEL nastaví klopný obvod WAIT, který stáhne RDY. Čeká se do dalšího okna pro CPU.
- **Emulace:** při přístupu do VRAM vložit tolik wait stavů, aby přístup padl do CPU okna znakového taktu (600 ns cyklus). Prakticky jde o 0–1 wait stav při 2 MHz.
- **Vedlejší efekt u 8080:** IN/OUT na porty F8–FF by také spustily čekání, protože číslo portu je i na A8–A15. Při 8080 se tedy může objevit nanejvýš 1 wait stav.
- **Bez přerušení:** konektor X1 nemá pin INT. To je v rozporu s tvrzením manuálu.

## Výstupy

Kompozitní video na X2 pin 30. Pro terminál AZJ-462 navíc /HS, VS a /VID s otevřeným kolektorem.

## Nejasnosti (ověřit na HW)

- 20 nebo 24 řádků: PROM a manuál říkají 20, dokumentace CP/M 24 („poslední znak na FDFFh“).
- Délka řádku 107 nebo 108 znaků.
- Zapojení neoznačených vstupů DD4 A7 a DA1 A6.
