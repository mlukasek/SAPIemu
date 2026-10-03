# MikroMon 1.3

- **Autor:** Libor Lasota. Popis na https://www.pmd85.cz/?page_id=2286.
- **Soubory v repu:** `roms/mikromon/MikroMon1_3_f000.txt` (Intel HEX, 2 KB) a návod `navod-k-MikroMon.pdf`.
- **Zdrojové kódy** starší verze 1.2 jsou jen pro studium v `..\MikroMon\starsi_zdrojove_kody\`. Do repa se nedávají.

## Zavedení a start

HEX má adresu **0000h** a patří do EPROM JPR-1ZI. Kód je přeložený pro běh od **F000h** a končí asi na F7A6h.

1. `DI`, pak `LDIR` zkopíruje 0000–07FF na F000 a následuje `JP F00F`.
2. `OUT (00h)` vypne BOOT, `LD SP,EF00h`.
3. Na kanál 1Ch (porty 1D, 1E, 1Fh, čeká se na bit 6 = TX prázdný) pošle `"sw i01"`+CR. Manuál uvádí „sw i02“, binárka i01.
4. Hledá UART: `LD BC,0310h`, tedy **jen 10h, 14h, 18h** (manuál uvádí i 1Ch). Kanál je aktivní, když `IN (base)` bit 5 = 0. Výsledek uloží do FD03h (FFh = žádný).
5. Inicializuje DSM-1/DMR-1: `OUT base,03`, `OUT base+1,0Dh` (8N1), `OUT base+3`.
6. Vypíše „ZPS-4 / MikroMon 2kB V1.3“.
7. Zamkne ZRD-1V: `OUT 61h,FF; OUT 62h,FF; OUT 63h,3F`, prodleva, `OUT 60h`.

## Ozvěna na UART (zjištěno při emulaci)

Bez příkazu `USYNC` posílá MikroMon každý znak na UART **bez čekání na TBRE**, viz rutina `Echo` v `dsm1.asm`.
Při 9600 Bd se tak znaky ve vysílacím registru MHB1012 přepisují a terminál dostane jen zhruba každý pátý znak.
Emulátor to věrně reprodukuje, po startu přijde na linku „Z4kn 3“. Na reálném HW ověřit, zda se to chová stejně.

## Potřebná paměť

- RAM EE00–F7FF (zásobník EF00h, kód F000–F7FF).
- FD00–FFFF pro proměnné. Může to být neviditelná část VRAM AND-1Z (řádky 20–23).

## Displej

- **Výstup:** AND-1Z/1V, VRAM na **F800h**, 64×20, zápis přímo do VRAM.
- **Scroll:** instrukcí `LDIR`.
- **Kurzor:** znak FFh.
- **Ozvěna:** výstup se zároveň posílá na UART. Ve verzi 1.3 nejsou rutiny ověřené, rozložení se převzalo ze zdrojů 1.2.

## Vstup

Klávesnice EKL-1 nebo Consul na portech 01h/02h (viz [klavesnice.md](../desky/klavesnice.md)), případně znaky z UARTu.

## Příkazy (podle návodu)

| Příkaz | Funkce |
|---|---|
| `?` | Seznam příkazů |
| `> adr bajty/"text"` | Zápis do paměti |
| `FILL a1 a2 seq` | Vyplnění oblasti |
| `IN port` / `OUT port seq` | Čtení a zápis portu |
| `JUMP adr` | Skok. `RET` se vrátí do MikroMonu a vypíše registry |
| `LOAD` | Intel HEX přes UART (záznamy 00/01, až 115,2 kBd při 2 MHz) |
| `M adr` | Výpis paměti: SPACE = řádek, CR = obrazovka, ESC = konec |
| `SCAN adr/port` | Opakovaný výpis do ESC |
| `USYNC` | Zapne handshake TX |
| `UART [port]` | Zobrazí nebo nastaví pracovní kanál |

## Sestava v emulátoru

`machines/mikromon.jsonc`:
- JPR-1ZI 2 MHz, dekodér D3C_2k, EPROM MikroMon 1.3.
- DMR-1 s RAM v konfiguraci 1 (0000–F7FF) a UARTem na 10h.
- AND-1Z-89 na F800.
- Klávesnice EKL-1.
