# Sériová linka (DMR-1) a připojení terminálu

Každý sériový kanál (zatím DMR-1, později DSM-1) lze na straně PC připojit několika způsoby. Všechny mohou
fungovat najednou:

1. **Okno Sériový terminál** v emulátoru (Zobrazení → Sériový terminál).
2. **TCP server.** TeraTerm, PuTTY nebo telnet se připojí přímo, bez virtuálního COM portu.
3. **Skutečný COM port PC** (USB převodník, nebo pár virtuálních portů com0com).
4. **MCP** (`serial_send`, `serial_receive`, `serial_set_connected`) pro Clauda.

## TeraTerm přes TCP (doporučeno)

1. V sestavě je TCP server zapnutý (`machines/cpm_ide.jsonc`, `cpm_zrmd.jsonc`). Jinak ho spusťte v okně
   Sériový terminál tlačítkem **Spustit TCP server**.
2. V TeraTermu zvolte Soubor → Nové připojení → **TCP/IP**, Host `127.0.0.1`, TCP port `2323`,
   Service **Telnet** nebo **Other** (raw). Obojí funguje, emulátor telnet rozpozná sám.
3. **DCD = klient připojen** (`dcd_follows`): dokud je TeraTerm připojený, je na DMR-1 aktivní DCD
   (připojený terminál). ECMON i MikroMon si konzoli volí při startu. Postup je tedy: připojit TeraTerm,
   dát v emulátoru Reset (F9) a ECMON pak běží na TeraTermu.
4. **Nastavení terminálu v TeraTermu:**
   - Nový řádek Příjem: **CR**. ECMON posílá `CR CR CR LF`.
   - Vysílání: **CR**.
   - Řídicí kódy ECMON (1Dh domů, 1Fh smazání…) TeraTerm nezná a ignoruje je. Plnohodnotné zobrazení
     umí jen okno terminálu v emulátoru v režimu „SAPI (ECMON/AND-1Z)“.
   - Čeština: SAPI používá KOI-8čs, kterou TeraTerm neumí. Znaky C0–FFh se v něm zobrazí špatně.

Jiný port nebo síťový přístup se nastaví v sestavě:

```jsonc
"serial": {
    "tcp": { "port": 2323, "bind": "127.0.0.1", "dcd_follows": true },
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
| Terminál připojen (DCD) | okno terminálu, `connected` v DMR-1, MCP `serial_set_connected` | Aktivní DCD/CTS/DSR. Software podle toho volí konzoli. |
| DCD = klient připojen | okno terminálu, `serial.tcp.dcd_follows` | DCD sleduje připojení TCP klienta. |
| Bez přetečení | okno terminálu, `serial.pacing` (výchozí zapnuto) | PC pošle další znak až po přečtení předchozího. Bez toho se při vložení delšího textu znaky ztrácí (overrun), přesně jako na skutečné lince bez řízení toku. |
| Řízení toku RTS/CTS | `flow_control: "rts_cts"` v DMR-1 | PC posílá, jen když SAPI drží RTS (bit 0 registru modemu) aktivní. |

## Poznámky k softwaru

- **MikroMon bez `USYNC`** posílá znaky na UART bez čekání na volný vysílač, takže výstup je děravý.
  Po příkazu `USYNC` čeká na TBRE i CTS a výstup je úplný. Ověřeno testem `test_serial_tcp`.
- **ECMON** používá stejné řídicí kódy na obrazovce i na lince, viz `docs/software/ECMON.md`.
