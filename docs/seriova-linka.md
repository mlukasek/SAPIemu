# Sériová linka (DMR-1) a připojení terminálu

Každý sériový kanál (DMR-1, DSM-1, DSM-1V, RSW-1, oba kanály DSP-1) lze na straně PC připojit několika způsoby. Všechny mohou
fungovat najednou:

1. **Okno Sériový terminál** v emulátoru (Zobrazení → Sériový terminál).
2. **TCP server.** TeraTerm, PuTTY nebo telnet se připojí přímo, bez virtuálního COM portu.
3. **Skutečný COM port PC** (USB převodník, nebo pár virtuálních portů com0com).
4. **MCP** (`serial_send`, `serial_receive`, `serial_set_connected`) pro Clauda.

## TeraTerm přes TCP (doporučeno)

1. V sestavě je TCP server zapnutý (`machines/cpm_ide.sapi`, `cpm_zrmd.sapi`). Jinak ho spusťte v okně
   Sériový terminál tlačítkem **Spustit TCP server**.
2. V TeraTermu zvolte Soubor → Nové připojení → **TCP/IP**, Host `127.0.0.1` (nebo `localhost`), TCP port
   `2323`, Service **Telnet**. Emulátor hned po připojení nabídne telnetový znakový režim (WILL ECHO,
   WILL SGA), takže TeraTerm posílá každou klávesu hned, ne až po Enteru.
   - Z příkazové řádky: `telnet localhost 2323`. Server poslouchá na IPv4 i IPv6 loopbacku.
   - Pro raw spojení (TeraTerm „Other“, PuTTY Raw, netcat) nastavte protokol **raw** (v okně terminálu
     nebo `"protocol": "raw"`). V režimu telnet by raw klient na začátku zobrazil pár znaků vyjednávání.
   - `"protocol": "auto"` zůstane v raw režimu, dokud klient sám nezačne telnetem.
3. **DCD = klient připojen** (`dcd_follows`): dokud je TeraTerm připojený, je na DMR-1 aktivní DCD
   (připojený terminál). ECMON i MikroMon si konzoli volí při startu. Postup je tedy: připojit TeraTerm,
   dát v emulátoru Reset (F12) a ECMON pak běží na TeraTermu.
4. **Nastavení terminálu v TeraTermu:**
   - Nový řádek Příjem: **CR**. ECMON posílá `CR CR CR LF`.
   - Vysílání: **CR**.
   - Řídicí kódy ECMON (1Dh domů, 1Fh smazání…) TeraTerm nezná a ignoruje je. Plnohodnotné zobrazení
     umí jen okno terminálu v emulátoru v režimu „SAPI (ECMON/AND-1Z)“.
   - Čeština: SAPI používá KOI-8čs, kterou TeraTerm neumí. Znaky C0–FFh se v něm zobrazí špatně.

Jiný port nebo síťový přístup se nastaví v sestavě:

```jsonc
"serial": {
    "tcp": { "port": 2323, "bind": "127.0.0.1", "dcd_follows": true, "protocol": "telnet" },
    "pacing": true
}
```

`"bind": "0.0.0.0"` zpřístupní port i z jiných počítačů v síti. Není tam žádné heslo, používat jen
v domácí síti.

## Skutečný nebo virtuální COM port

```jsonc
"serial": { "com": { "port": "COM5", "baud": 9600 } }
```

Totéž jde v okně terminálu tlačítkem **Otevřít COM port**.

- **Rychlost na PC:** ve výchozím stavu je stejná jako nastavení JP8 na DMR-1. Formát je 8N1.
- **Virtuální COM pro programy, které umí jen COM:** nainstalovat **com0com** (pár portů, např. COM20 ↔ COM21).
  Emulátor otevře COM20, terminálový program COM21. Ovladač je třetí strany a emulátor ho neinstaluje.
- **Linux:** jako port zadejte `/dev/ttyUSB0`, `/dev/ttyS0`, případně pseudoterminál z `socat`.

## Volby kanálu

| Volba | Kde | Význam |
|---|---|---|
| Terminál připojen (DCD) | okno terminálu, `connected` v DMR-1, MCP `serial_set_connected` | Aktivní DCD. ECMON a MikroMon podle něj při startu volí konzoli (sériová linka, nebo obrazovka). |
| Připraven (CTS) | okno terminálu, `cts` v DMR-1 (výchozí zapnuto), MCP `serial_set_connected` `cts` | Aktivní CTS a DSR: terminál je připraven přijímat. Programy vysílající s řízením RTS/CTS (DOPC.COM, MikroMon `USYNC`) na něj čekají. Nezávisí na DCD, takže přenos funguje i s konzolí na obrazovce. |
| DCD = klient připojen | okno terminálu, `serial.tcp.dcd_follows` | DCD sleduje připojení TCP klienta. |
| Bez přetečení | okno terminálu, `serial.pacing` (výchozí zapnuto) | PC pošle další znak až po přečtení předchozího. Bez toho se při vložení delšího textu znaky ztrácí (overrun), přesně jako na skutečné lince bez řízení toku. |
| Řízení toku RTS/CTS | `flow_control: "rts_cts"` v DMR-1 | PC posílá, jen když SAPI drží RTS (bit 0 registru modemu) aktivní. |

## Přenos souborů XMODEM (DOPC / DOCPM)

Postup pro přenos ze SAPI do PC s DOPC10.COM a TeraTermem (nebo bez něj tlačítkem „Přijmout soubor XMODEM…“
v okně terminálu, kapitola níže):
1. Konzole může zůstat na obrazovce (DCD neaktivní). **CTS musí být aktivní** („Připraven (CTS)“, výchozí stav).
2. V CP/M spustit `DOPC10 SOUBOR.EXT`. Program vypíše „Soubor otevren“ a čeká na přijímač.
3. V TeraTermu File → Transfer → XMODEM → Receive, protokol Checksum.
4. Pro binární přenosy je bezpečnější TCP v režimu **raw** (TeraTerm „Other“). Telnet funguje také,
   emulátor vyjedná BINARY a znak FFh zdvojuje.

Ověřeno testem `test_xmodem`: DOPC10 přenese TD.MAC (40 paketů) bez chyb. První paket se na začátku může
jednou opakovat, to XMODEM řeší sám.

## Odesílání souborů a schránka v okně terminálu (2026-10-06)

Okno Zobrazení → Sériový terminál umí poslat soubor do SAPI bez TeraTermu (pro vybraný kanál):

| Tlačítko | Co pošle |
|---|---|
| **Poslat soubor…** | bajty souboru tak, jak jsou (např. Intel HEX pro `BOOT` MIKROBIOSu na JPR-1S nebo JPR-1X, `docs/desky/JPR-1S.md`) |
| **Poslat textový soubor…** | text UTF-8: česká písmena jako kódy SAPI (jako psaní z klávesnice), konce řádků CR LF / LF jako CR (Enter) |
| **Poslat soubor XMODEM…** | XMODEM pro přijímací program v SAPI (např. `DOCPM10 SOUBOR.EXT` v CP/M): pakety po 128 B, součet nebo CRC-16 podle toho, čím přijímač začne (NAK / „C“), 10 opakování, bez odpovědi 10 s se paket pošle znovu, Zrušit pošle CAN CAN |
| **Přijmout soubor XMODEM…** | soubor z vysílacího programu v SAPI (např. `DOPC10 SOUBOR.EXT`). XMODEM jméno nepřenáší, proto se nejdřív zvolí, kam soubor uložit (jako v TeraTermu). Přijímač pak posílá NAK (součet) každé 3 s, dokud vysílač nezačne; pakety po 128 B i 1 KB, opakovaný paket potvrdí a zahodí, špatný odmítne (NAK, 10×). Soubor se zapíše po EOT. Pakety se mezitím v terminálu nevypisují. |

- Pod tlačítky je průběh: kolik bajtů zbývá poslat (Zastavit zahodí zbytek), u XMODEM číslo paketu.
- Bajty jdou rychlostí linky a s volbou „Bez přetečení“ jako psaní z terminálu; fronta pojme 1 MB.
- **Odříznout 1Ah na konci** (výchozí zapnuto): XMODEM doplní poslední paket znaky 1Ah, textový soubor CP/M jimi
  končí. U binárních souborů, které 1Ah opravdu končí, vypnout (soubor pak má délku násobku 128 B jako v CP/M).
- **Výběr myší:** tažením se vybere text (i přes více řádků, u okraje okno odroluje), dvojklik vybere slovo,
  trojklik řádek, klik výběr zruší. S volbou **Výběr rovnou do schránky** (výchozí zapnuto, ukládá se do nastavení)
  jde výběr do schránky hned po puštění tlačítka myši, jako v PuTTY nebo TeraTermu.
  **Kopírovat** vloží do schránky výběr, a když žádný není, celý text terminálu.
  Mezery na koncích řádků se vynechají. **Vložit** pošle schránku jako psaný text (konce řádků jako CR). Zkratky jako v TeraTermu: **Alt+C** a **Alt+V** (také Ctrl+Insert a Shift+Insert); Ctrl+C a Ctrl+V jdou do
  SAPI jako řídicí kódy 03h a 16h, AltGr (Ctrl+Alt) píše znaky. Totéž je v menu pod pravým tlačítkem.
- Vysílač i přijímač XMODEM jsou v jádru (`src/core/xmodem.*`), test `test_xmodem`: DOPC10.COM pošle TD.MAC
  (40 paketů, 5025 B textu bez výplně), DOCPM10.COM přijme 14 řádků textu (7 paketů, bez opakování), `TYPE` je
  vypíše.

## Poznámky k softwaru

- **MikroMon bez `USYNC`** posílá znaky na UART bez čekání na volný vysílač, takže výstup je děravý.
  Po příkazu `USYNC` čeká na TBRE i CTS a výstup je úplný. Ověřeno testem `test_serial_tcp`.
- **ECMON** používá stejné řídicí kódy na obrazovce i na lince, viz `docs/software/ECMON.md`.
