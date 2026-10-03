# MCP server

Emulátor obsahuje server **MCP** (Model Context Protocol). Přes něj může Claude ovládat běžící SAPI-1: psát
na klávesnici, číst obrazovku, nahrávat programy, krokovat a nastavovat breakpointy.

## Spuštění

- **GUI (`sapiemu.exe`):** server se spouští automaticky na `http://127.0.0.1:8580/mcp`. Adresa je uvedena
  v menu Nápověda. Ve stavovém řádku je pak „MCP“.
  - `--mcp-port N` nastaví jiný port, `--no-mcp` server vypne.
- **Bez okna:** `sapiemu-cli --mcp [--machine ...] [--mcp-port N]`. Emuluje v reálném čase a obsluhuje MCP,
  dokud se nestiskne Ctrl+C.
- **Bezpečnost:** server poslouchá jen na `127.0.0.1` a přijme jen požadavek, který splní tři podmínky:
  - hlavička `Host` je `127.0.0.1`, `localhost` nebo `[::1]` (ochrana proti DNS rebindingu);
  - `Origin`, pokud je uvedený, je přesně jeden z těchto počítačů na libovolném portu, takže cizí webové
    stránky nemohou poslat podvržený požadavek (CSRF);
  - `Content-Type` je `application/json`.

  Ostatní požadavky dostanou 403. Claude Code, curl i lokální nástroje na PC fungují beze změny. Nástroje
  s cestou k souboru (`load_hex`, `load_binary`) dál berou i absolutní cesty, protože je mohou volat jen
  programy na vlastním PC.

## Připojení Claude Code

V repozitáři je soubor `.mcp.json` (projektový rozsah), takže Claude Code při práci v repu server sám nabídne.
Při prvním použití je potřeba ho schválit. Ručně se dá přidat takto:

```
claude mcp add --transport http sapiemu http://127.0.0.1:8580/mcp
```

Emulátor musí běžet dřív, než se Claude Code spustí (nebo zadejte `/mcp` → reconnect).

## Protokol

- JSON-RPC 2.0 přes **Streamable HTTP**: `POST /mcp`. Odpovědi jsou jako prostý JSON, bez SSE streamu.
  `GET` vrací 405.
- **Podporované verze:** 2025-11-25, 2025-06-18, 2025-03-26, 2024-11-05.
- **Metody:** `initialize`, `ping`, `tools/list`, `tools/call`. Notifikace se přijmou s 202.
- **Vlákna:** MCP běží ve vlastním vlákně. Každé volání nástroje se předá emulačnímu vláknu (`EmuHost::call`)
  a provede se mezi dvěma snímky. Synchronní běhy (`run_for`, `run_until`) na tu dobu GUI pozastaví.

## Nástroje

Adresy a porty se zadávají jako **hex řetězce** (`"F000"`, `"0xF000"`, `"F000h"`). Čísla v JSON jsou desítková.

| Nástroj | Popis |
|---|---|
| `machine_info` | Přehled: sestava, stav, takt, karty ve slotech s jumpery, sériové kanály, registry |
| `list_machines`, `load_machine` | Seznam a načtení sestav z `machines/` |
| `reset`, `pause`, `resume` | Reset tlačítkem, pauza a běh v reálném čase (volitelně turbo) |
| `step` | Krok (`count`), `over=true` = krok přes CALL, RST, DJNZ a LDIR |
| `run_for` | Synchronní běh po zadaný emulovaný čas v ms |
| `run_until` | Běh do adresy, do textu na obrazovce nebo na sériové lince, nebo do timeoutu |
| `get_registers`, `set_registers` | Registry Z80 (nastavení PC restartuje fetch) |
| `read_memory`, `write_memory` | Paměť z pohledu CPU včetně boot ROM; zápis přes sběrnici |
| `io_read`, `io_write` | IN a OUT na port (s vedlejšími efekty jako u CPU) |
| `disassemble` | Disassembler Z80 |
| `set_breakpoint`, `clear_breakpoint`, `list_breakpoints` | Druhy `exec`, `mem_read`, `mem_write`, `mem_rw`, `io_in`, `io_out`, `io_rw` |
| `load_hex`, `load_binary` | Nahrání programu ze souboru nebo z inline Intel HEX |
| `type_text` | Psaní na klávesnici EKL-1 (`\r` = Enter, čeština → KOI-8čs), volitelně s během `run_ms` |
| `screen_text`, `screenshot` | Text obrazovky (UTF-8) a obrázek PNG |
| `serial_send`, `serial_receive` | Sériový kanál (DMR-1) |
| `pmd32_images`, `pmd32_mount` | Jednotky PMD32-SD (A–D; v CP/M A = D:, B = E:): obsah SD karty, vložení a vysunutí obrazu |
| `serial_set_connected` | Připojení a odpojení terminálu (DCD/CTS/DSR). ECMON a MikroMon podle DCD volí konzoli |
| `io_log`, `get_log` | Záznam I/O přístupů a log emulátoru |

## Poznámky k používání

- **Běh mezi voláními:** pokud stroj není pozastavený, běží mezi voláními nástrojů dál v reálném čase.
  Pro deterministické ladění nejdřív zavolat `pause`.
- **Breakpoint během běhu:** když se breakpoint trefí při běhu v reálném čase, stroj se pozastaví a důvod
  vrátí další volání jako `previous_stop`.
- **Příkaz `M` v MikroMonu** po stránce výpisu čeká na SPACE, CR nebo ESC.
