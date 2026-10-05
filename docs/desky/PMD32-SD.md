# PMD32-SD – emulátor disketových jednotek

V emulátoru se implementuje **protokol na úrovni 8255 desky IDE-1**, ne vnitřek s ATmega. Stejně se připojuje
i na kanál desky **DPP-1** (`docs/desky/DPP-1.md`; ECMON bez IDE a CP/M J. Bíby: 2Ch–2Fh).

Zdroje:
- firmware v3.2 (26. 2. 2012, Roman Bórik): `..\SAPI-karty\PMD32-SD\Firmware\pmd32sd-v32-src\pmd32sd-v32\src\pmd32.c`, `pmd32.h`, `config.c`, `timer.c`
- popis protokolu: `...\pmd32-protokol.txt`
- ovladač pro PMD-85: `H:\SAPI-1\Petr_repliky_k_vyrobe\PMD32-SD\Borik\pmd32sd-doc\pmd85-3_pmd32.asm`
- ovladač pro SAPI: P3112C.MAC od řádku 799 (je uvnitř obrazu disku, viz [IDE-1.md](IDE-1.md))
- web: https://pmd85.borik.net/wiki/Blog:Postavte_si_PMD_32-SD (popisuje starou verzi 1.3, platí zdroj v3.2)

## Hardware

- ATmega644 na 16 MHz, LCD 2×16, 4 tlačítka, SD karta přes SPI.
- Verze 3.2 podporuje SDHC, FAT16 i FAT32, dlouhá jména a cestu do 64 znaků.

## Handshake (8255 mód 2 na portu A)

- **Host → zařízení:**
  1. Host zapíše PA, /OBF jde do 0.
  2. Zařízení pulzem /ACK byte převezme.
  3. /OBF se vrátí do 1.
- **Zařízení → host:**
  1. Zařízení pulzem /STB zapíše byte a nastaví se IBF.
  2. Zařízení čeká, až host přečte PA a IBF klesne.
  3. Před strobe zařízení IBF nekontroluje, takže nepřečtený byte se přepíše.
- **Nastavení na straně hostitele (SAPI i PMD-85):**
  - CW = C4h (port A mód 2, port B mód 1 výstup).
  - Host čte INTRA (PC3) a přepíná povolení přerušení set/reset příkazy:
    - čekání na vstup: 09h (INTE2 = PC4 := 1), 0Ch (INTE1 := 0);
    - čekání na výstup: 08h (INTE2 := 0), 0Dh (INTE1 := 1).
  - **INTRA = (INTE1 · výstup prázdný) + (INTE2 · IBF)**, počítá se jako úroveň. `CheckDevice` spoléhá na to, že INTRA = 1 hned po povolení výstupu.

## Relace

1. **Klid:** zařízení posílá AAh a čeká až 15 ms na přečtení.
2. **Synchronizace:** host po přečtení AAh musí do 10 ms odpovědět AAh.
3. **Příkazový stav:** zařízení na každý příkaz čeká až 30 ms. Příkaz AAh ignoruje, po timeoutu se vrátí do klidu.

Host (`IsPresent`) před každým příkazem zkontroluje, zda čeká AAh. Pokud ano, odpoví AAh a pošle příkaz. Timeouty hostitele jsou asi 5 ms na bajt, 125–250 ms na detekci přítomnosti a asi 15 s na výsledný bajt. Pokus se opakuje čtyřikrát.

## Rámec

- **Požadavek:** příkaz, parametry a CRC. CRC je XOR všech bajtů včetně příkazu, takže XOR celého rámce je 0. **Každý parametr musí přijít do 5 ms**, jinak přijde NAK.
- **Odpověď:**
  - ACK (33h) nebo NAK (99h).
  - Po ACK volitelně ERR, pak data a CRC. CRC je XOR jen dat, bez ACK, NAK a ERR.
  - Pokud ERR ≠ 0, nenásledují data ani CRC.
- **Neznámý příkaz** nedostane žádný NAK (na rozdíl od dokumentace), zařízení se jen vrátí k AAh. Po NAK nebo chybě CRC také.
- **Časování firmwaru:** tik 1 ms. Timeouty: ACK 500 ms, data 30 ms.

**Parametr „disk+sektor“:**
- Bity 7–6 = jednotka: 00 = A, 10 = B, 01 = C, 11 = D (firmware kódy 01 a 10 prohazuje).
- Bity 5–0 = sektor 0–63. Stopa 0–255.
- Offset v obrazu = `(stopa * spt + sektor) * 128`.
- Kontrola: sektor < spt a stopa < počet stop (uložená hodnota 0 znamená 256).

## Příkazy

| Příkaz | Parametry | Odpověď |
|---|---|---|
| B | – | ACK, ERR, 128 B, CRC (A, stopa 0, sektor 0) |
| Q, R | d+s, stopa | ACK, ERR (03 = chyba čtení), 128 B, CRC |
| T, W | d+s, stopa, 128 B | ACK, ERR (01 = WP nebo zamčená karta, 04 = chyba zápisu) |
| S | d+s, stopa, 513 B | Zapíše 512 B na (sektor & 3Ch), 513. bajt se ignoruje |
| F | d+s, stopa | Vyplní stopu E5h, čeká 200 ms, vrátí ERR |
| I | jednotka | ERR (05 = špatná jednotka) |
| G | jednotka | ACK, ERR, WP, LEN, cesta (max 63 znaků), CRC (CRC včetně ERR) |
| H | jednotka, WP, LEN, jméno | ERR. LEN 0 = vysunout, LEN FFh = jen nastavit WP. Jméno je relativní k aktuálnímu adresáři |
| K | – | ACK, LEN, aktuální cesta, CRC (bez ERR) |
| L | 0 = první, jinak další | ACK, LEN, jméno, CRC. Adresáře jako `[jméno]`, LEN 0 = konec |
| M | LEN, adresář | ERR. Bez ohledu na velikost písmen, `..` = nahoru |
| N | LEN, jméno | ERR. Vytvoří obraz vyplněný E5h podle geometrie přípony |
| P | jednotka | ACK, ERR, stopy, sektory, kód fyzického sektoru, CRC (sektory 0 = bez disku) |
| U, C, J, *, @ | – | Přijme, ale nic nedělá (C vrací AAh) |

Chybové kódy: 05 špatná jednotka, 06 nenalezeno, 07 dlouhé jméno, 08 chyba vytvoření, 09 chybí konfigurace, 0A chyba čtení konfigurace, 0B poškozená konfigurace, 0C neznámý typ obrazu, 99 jiná chyba.

## SD karta, obrazy, jednotky

- **Jednotky:** čtyři. Připojené obrazy a jejich WP si firmware ukládá do EEPROM podle sériového čísla karty. Emulátor místo toho použije konfiguraci.
- **`images.cfg`** v kořeni SD karty:
  - Řádky mají max. 32 znaků ve tvaru `PŘÍPONA STOPY SPT [PHYS]`. PHYS je kód 0–7 pro 128–16384 B a je jen informativní.
  - Řádek začínající `*` určuje výchozí obraz pro jednotku A.

Obsah `media/pmd32sd/images.cfg`:

```
*VZOR_2M.2MB
2MB 255 64 0
8SD 77 26 0
SAP 77 26
P32 80 36
DCZ 80 36
DC8 154 26
MAX 256 64
```

- **Formát 8SD:** sektory jsou uloženy ve fyzickém pořadí 1..26, prokládání řeší překladová tabulka BIOSu.

### Rozdíl PMD-85 a SAPI

| | PMD-85-3 | SAPI s IDE-1 |
|---|---|---|
| 8255 | GPIO/0 na 4C–4Fh | Vlastní 8255 na IDE-1 |
| Jednotky | A, B; sektory 0–35, stopy 0–79 | A = CP/M **D:** (2MB), B = CP/M **E:** (8SD) |
| Použité příkazy | B, Q, T, I, *, @ (ROM bootuje přes B) | Jen synchronizace AAh, **Q a T** |

Na SAPI `SendSectorTrack` mapuje D: na 00h a E: na 80h se sektorem −1.

## DPB

- **2MB:** SPT 64, BSH 4, BLM 15, EXM 0, DSM 1019, DRM 255, AL0 F0h, CKS 64, OFF 0.
- **8SD:** standardní 8" disketa (26 sektorů na stopu, bloky 1 KB, DSM 242, DRM 63, OFF 2, skew 6).

## Emulace

- **SD karta:** složka na hostiteli, výchozí je `work/pmd32sd`, kopie z `media/pmd32sd`.
- **Protokol:** stavový automat s časováním v emulovaném čase (tik 1 ms), aby odpovídaly timeouty.

## Implementace v emulátoru

- **Firmware jako stavový automat:** hlavní smyčka firmwaru v3.2 (`main()` v `pmd32.c`) je přepsaná
  1:1 jako stavový automat (`src/chips/pmd32sd.cpp`). Běží v emulovaném čase, takže timeouty (15/10/30/5/500 ms)
  odpovídají originálu.
- **Komunikace s 8255:** zařízení přímo sleduje /OBF a IBF a odpovídá impulzy /ACK a /STB.
- **SD karta** je adresář na hostiteli (`images.cfg` v kořeni).
  - Rozpoznání typu obrazu, výchozí obraz `*` pro jednotku A a podadresáře (příkazy K, L, M) se chovají
    jako ve firmwaru.
  - Příkaz N (vytvoření obrazu vyplněného E5h) také.
  - Konfiguraci jednotek, kterou firmware drží v EEPROM, nahrazuje konfigurace sestavy, okno **Média**
    a MCP `pmd32_mount`.
- **Úvodní hláška:** firmware po zapnutí 3 s ukazuje na LCD hlášku a nekomunikuje. Emulátor má výchozí
  `startup_ms: 0`.
- **Zjištění (přímo ze zdrojového kódu firmwaru, testem to nejde ověřit):** po chybném CRC firmware sice
  pošle NAK (99h, bez čekání), ale hned se vrátí do smyčky prezentace a pošle AAh. Tím NAK ve vstupním
  latchi 8255 přepíše, takže hostitel místo NAK uvidí AAh. Dokumentace protokolu uvádí NAK, ve skutečnosti
  rozhoduje timeout a opakování v ovladači SAPI.
- **Jména obrazů jsou omezená na SD kartu.** Příkazy H (vložení) a N (vytvoření) i vkládání z GUI a MCP
  přijmou jen cestu uvnitř SD karty. Podadresáře `HRY/HRA.2MB` jsou povolené, `..`, `\`, `:`, absolutní
  a síťové cesty ne. Odmítnuté jméno dostane „nenalezeno“ (06), u N „chyba vytvoření“ (08), stejně jako
  na skutečném zařízení, kde takový soubor na kartě FAT neexistuje. Bez toho by software v emulátoru mohl
  číst a zapisovat soubory na PC mimo SD kartu.
