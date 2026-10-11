# Měřicí pracoviště: testovací ROM a skripty

Programy pro ověřování emulátoru na skutečné sestavě V (popis pracoviště a výsledky v `docs/zmeny-podle-hw.md`).
Výstupy (přeložené ROM, záznamy analyzátoru, přijatá data) jdou do `work/hwbench/`.

Potřeba: pasmo (`..\Tools\pasmo-0.5.3`), Python s balíčky `pyserial`, `logic2-automation` a `pypicorom`
(s opravou `picolink`, wheel u autora v `E:\SAPI_GIT\PicoROM\`), `picorom.exe`, Logic 2 se zapnutým Automation
(port 10430).

| Soubor | Co dělá |
|---|---|
| `build.py` | přeloží všechny `*.asm` pasmem do `work/hwbench/*.bin` |
| `hello.asm` | hlavička a řádky s počítadlem na DSM-1V (`RAMREAD` = 0 ukáže, že bez čtení RAM UART mlčí) |
| `diag.asm` | hodnoty registrů UARTu „vyťukané“ na /IOR (bez sériové linky), čte `decode_ior.py` |
| `diag2.asm` | jak UART bere zápisy: hned po resetu, dávka, dva za sebou, s TBRE |
| `mr.asm` | reset UARTu (MR) do prvního /MR: linka má přenést jen „CD“ |
| `wire.asm` | kontrola zapojení analyzátoru: známý vzor na každém kanálu |
| `pico.asm`, `pico_out.asm` | komunikační kanál PicoROM (ozvěna, tok SAPI → PC) |
| `hw_run.py ROM OUT [s]` | záznam, nahrání ROM do PicoROM, reset, příjem z COM13, souhrn /IOR a TxD |
| `wire_check.py [ROM] [OUT]` | záznam všech 16 kanálů spuštěný /RES, vyhodnocení po kanálech |
| `decode_ior.py CSV` | dekóduje hodnoty z `diag.asm` |
| `emu_run.py ROM [s] [kanál] [--hex]` | stejná ROM v SAPIemu (`hwbench_v.sapi`), výpis sériového kanálu |
| `pico_run.py`, `pico_out.py` | zkouška kanálu PicoROM oběma směry |
| `logic_stop.py` | zastaví záznamy, které v Logic 2 zůstaly viset |

Pravidla:

- Reset přes PicoROM: `low`, 0,3 s, `z`. Nikdy nenechat `low`.
- Kanál PicoROM: SAPI smí posílat jen omezené množství nebo na žádost a PC musí vše dočíst před `end_comms`,
  jinak se firmware PicoROM zasekne a pomůže jen odpojit USB.
- DSM-1V: po nastavení UARTu jednou přečíst RAM, jinak zůstane v resetu.
- Analyzátor 125 MS/s, při pochybnostech hrany analogově.
