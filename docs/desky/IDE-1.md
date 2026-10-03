# IDE-1 – IDE disk + připojení PMD32-SD

Zdroje:
- `..\IDE-1\IDE-1_M_u_SCH.pdf` (rev 2.1, 13.10.2019), `IDE-1_M_u_BOM.xlsx`, `IDE-1_jumpers.xlsx`, `SAPI_IDE_pokusy.txt`
- `H:\SAPI-1\IDE-1\IDE_POPIS.TXT`

Nejlepší zdroje jsou ale zdrojové kódy uvnitř obrazů disků v SD dumpu `H:\SAPI-1\Petr_repliky_k_vyrobe\PMD32-SD\SD_EC1045_2019-01-31\`:
- `IMAGE_M\HDD_TOOL_2M.2MB`: IDE.MAC, INIHDBC.MAC, IHDFULL.MAC, COPY8HD.MAC, 8HDOU*.MAC.
- `SAPI1_MAP\4B_DBSC_311212\311212C.2MB`: BIOS B3112HD.MAC a B3112HDO.MAC, monitor P3112C.MAC a Z3112C.MAC (ovladač PMD32 od řádku 799).

## Obvody

| Obvod | Funkce |
|---|---|
| IC11 (8255) | Výběr CS0, připojení PMD32-SD |
| IC12 (8255) | Výběr CS1, IDE disk |
| IC1 (74LS245) | Buffer sběrnice, aktivní při výběru kterékoli 8255; směr podle /RD |
| IC3, IC4 (74LS245) | Data IDE, spodní a horní bajt |
| IC2 (74LS245) | Data PMD32-SD |
| IC10 (74LS138) | Dekodér adres |
| IC9 (74LS86) | XOR pro volbu polarity adres s JP5 |
| IC5, IC13 (74LS125), IC6 (7406), IC7 (7407), IC8 (74LS08) | Pomocná logika a budiče |

- **Sběrnice X1:** D0–D7, A0–A7, /IOR, /IOW, /RES, +5 V, +12 V.
- **Žádné přerušení:** IDE INTRQ (pin 31) není zapojený, INTRA (PC3) z 8255 pro PMD vede jen na lištu SV2. Všechny ovladače používají polling.

## Adresy

- **Dekódování:** A2–A4 vedou do 74LS138, každá 8255 má tedy slot 4 portů.
- **Okno:** A5–A7 přes XOR s jumpery JP5 vybírají 32bajtové okno.
- **Přiřazení:** jumpery JP1–JP3 přiřazují výstupy Y0–Y7 signálům CS0 a CS1.

| Nastavení | IDE 8255 | PMD 8255 | Zdroj |
|---|---|---|---|
| **Používá software (Martin)** | **58–5Bh** | **5C–5Fh** | `BASE_55 EQU 58H` v IDE.MAC a B3112HD*.MAC; `PIO_A EQU 5CH` v P3112C.MAC; ROM `JPR-1ZI_ECMON_2k_IDE-1_Martin.bin` |
| Příklad v `IDE-1_jumpers.xlsx` | 28–2Bh | 2C–2Fh | |
| Další varianty | 3C–3Fh | | |

- Starší ROM `JPR-1ZI_ECMON_2k_Martin.bin` volá PMD32-SD na 2C–2Fh. Šlo o dřívější připojení přes DPP-1.
- Přístupy na 28h–2Bh v ROM patří kartě DPP-1, ne IDE.
- Emulátor: obě báze konfigurovatelné, výchozí 58h a 5Ch.

## IDE strana (IC12)

PA = data D0–7, PB = data D8–15, PC = řízení, CW = control word. Disk se používá **jen v 16bitovém režimu**.

| PC bit | Maska | Signál IDE | Budič |
|---|---|---|---|
| PC0 | 01h | DA0 | 7407 |
| PC1 | 02h | DA1 | 7407 |
| PC2 | 04h | DA2 | 7407 |
| PC3 | 08h | /CS1FX (registry příkazů) | 7406, 1 = aktivní |
| PC4 | 10h | /CS3FX (registry řízení) | 7406, 1 = aktivní |
| PC5 | 20h | /DIOW | 7406, 1 = aktivní |
| PC6 | 40h | /DIOR | 7406, 1 = aktivní |
| PC7 | 80h | /RESET | 7406, 1 = aktivní |

Buffery dat jsou povolené jen při /DIOR nebo /DIOW a směr určuje /DIOR. JP4 přivádí +5 V na pin 20 pro DOM nebo CF adaptér.

**Čtení** (`I_RD_16`):
1. CW zůstává 92h (PA a PB vstupy, PC výstup, mód 0).
2. `PC := reg`.
3. `PC := reg | 40h` (aktivuje /DIOR).
4. Čte se PB (horní bajt) a pak PA (spodní bajt).
5. `PC := 0`.

**Zápis** (`I_WR_16`):
1. `CW := 80h` (vše výstupy).
2. Zápis PA a PB.
3. `PC := reg`.
4. `PC := reg | 20h`.
5. `PC := reg` (IDE zapisuje na této hraně).
6. `CW := 92h`.

**Emulace:**
- Registr IDE se čte při náběžné hraně PC6 (0→1). Ukazatel datového registru se posune jednou za strobe, ne za každé čtení portu.
- Zápis se provede při sestupné hraně PC5 (1→0) s hodnotami z výstupních latchů PA a PB.
- Zápis CW (nastavení módu) vynuluje výstupy, stejně jako u skutečné 8255.
- Když je PC nastavený jako vstup, signály se považují za neaktivní. Jde o předpoklad, viz Nejasnosti.

**Výběr registru (hodnota PC):**

| PC | Registr | PC | Registr |
|---|---|---|---|
| 08h | data | 0Ch | LBA 8–15 |
| 09h | error | 0Dh | LBA 16–23 |
| 0Ah | sector count | 0Eh | head (vždy E0h = LBA, master) |
| 0Bh | LBA 0–7 | 0Fh | status / command |
| 16h | device control | 17h | alt status |

**Příkazy:**
- Používané: 20h (čtení), 30h (zápis), ECh (identify).
- Jen v testovacím nástroji: E0h a E1h (spin down/up), 10h, 91h.

**Postup:**
- **Rozsah LBA:** jen 24 bitů, sector count vždy 1.
- **Inicializace:**
  1. Reset přes PC7 se smyčkou zpoždění.
  2. `head := A0h`.
  3. Čeká se na DRDY=1 a BSY=0 (až FFFFh pokusů, jinak „Nenalezen“).
  4. Identify (ECh). Model se vypisuje z bajtů 54–93 s prohozenými bajty.
- **Bez timeoutů:** `WAIT_DRQ` a další smyčky nemají timeout, emulovaný disk proto musí BSY vždy uvolnit.

## Rozdělení disku (BIOS B3112HD/HDO)

- **Boot:** CP/M startuje z **A: = ZRMD-1**.
- **Jednotky:** B: a C: jsou IDE po 8 MB, D: a E: jsou PMD32-SD.
- **DPB pro B: a C:** SPT 128, BSH 6 (bloky 8 KB), BLM 63, EXM 3, DSM 1023, DRM 767, AL0 E0h, CKS 0, OFF 0 (C: ve verzi s pevnými offsety má OFF 512).
- **Sektory:** 512 B, rozdělené na 4 záznamy CP/M. Výpočet: `LBA = (stopa + offset) * 32 + záznam/4`.
- **Bez MBR:** disk se používá od LBA 0 jako 16 slotů po 8 MB (4000h LBA), tedy 128 MB. IDE.MAC umí zálohovat do LBA 40000h–7FFFFh.
- **Offsety (HDO):** offsety pro B: a C: jsou na MONITB−2 a MONITB−4 (násobky 200h stop). Kontrolní slovo 55AAh je na MONITB−6 (F7FAh nebo EFFAh). Mění je `OFFSHDD.COM` a `INIHDBC.COM`.
- **Formát obrazu v emulátoru:** surový `.img`.

## PMD32-SD strana (IC11)

- **Data:** PA0–7 přes IC2 na data PMD32-SD.
- **Handshake:** PC4 = /STB, PC5 = IBF, PC6 = /ACK, PC7 = /OBF, přes 74LS125 na konektor X2 (signály /PSTB, PIBF, /PACK, /POBF).
- **Směr IC2:** řídí ho /ACK přes 7406. Signál DIR z PMD32-SD se nepoužívá.
- **Varianty:** IDE-1_M má samčí a IDE-1_F samičí konektor. Přechod mezi nimi řeší překřížený kabel.
- **Protokol:** popsán v [PMD32-SD.md](PMD32-SD.md).

## Nejasnosti

- Po resetu je PC 8255 vstup a vstupy 7406 mohou „plavat“ do H, takže by IDE signály byly aktivní. Není jisté, zda R8 a R9 jsou pull-upy. Emulátor zatím bere signály jako neaktivní.
- Zapojení lišty SV2 je jen odečtené z PDF.
- Zdroje OFFSHDD.COM a BIOSu 2015 („BI-3K V050815JB“) nejsou k dispozici.

## Implementace v emulátoru

- **Kód:** karta `IDE-1` (`src/cards/ide1.*`), obvody `src/chips/i8255.*`, `ide_disk.*`, `pmd32sd.*`.
- **Konfigurace:**
  - `ide_base` (výchozí 58h), `pmd_base` (výchozí 5Ch);
  - `ide`: `image`, `create_mb` (vytvoří obraz vyplněný E5h, když chybí), `readonly`, `working_copy`;
  - `pmd32sd`: `sd_card` (adresář jako SD karta, výchozí `media/pmd32sd`, kopie ve `work/pmd32sd`),
    `drives` [A..D] (jméno obrazu nebo `{image, wp}`), `card_locked`, `startup_ms`.
- **8255 IDE:** signály IDE jsou aktivní jen při CW 80h/92h, kdy je port C výstup.
  - Čtení registru se provede na náběžné hraně PC6 (/DIOR). Data jsou pak na vstupech PA a PB.
    U 8bitových registrů je D8–D15 FFh.
  - Zápis se provede na sestupné hraně PC5 (/DIOW) z výstupních latchů PA a PB.
  - PC7 (náběžná hrana) resetuje disk.
- **IDE disk:**
  - IDENTIFY (model „SAPIemu IDE disk“), READ/WRITE SECTORS (LBA28 i CHS, 16 hlav × 63 sektorů),
    SRST v registru device control.
  - Příkazy 10h, 91h, E0–E5h, E7h, EFh, C6h, 40h, 70h a 90h se jen přijmou.
  - Bez obrazu čtou registry FFh (plovoucí sběrnice) a BIOS ohlásí „nenalezen“.
- **Ověřeno** se sestavou `machines/cpm_ide.sapi`: ECMON IDE, volba 3 → „PMD32-SD OK“,
  „INIT HDD SAPIemu IDE disk“, „OFFSET B: 0000H, C: 0200H“. Fungují `DIR D:` a `DIR E:`,
  `PIP B:=D:STAT.COM` i `B:STAT B:` (8160K volných).
