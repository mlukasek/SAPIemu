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
| `tbre.asm` + `tbre_run.py` | TBRE po zápisech, porty +3 a +0, hodnoty kanálem PicoROM |
| `ststb.asm` + `ststb_run.py` | STSTB JPR-1V u paměti, I/O a INTA, čekání AND-1V při I/O F8h |
| `and1v.asm`, `cga1v.asm`, `cga1v_long.asm` + `and1v_run.py` | čekací takty AND-1V a CGA-1V, deska proti emulátoru |
| `rdy_analog.py` | RDY analogově (tvar hrany) při čtení z CGA-1V |
| `cga_window.py` | okno CPU karty CGA-1V (sondy QA a /CPU na IC7): řetěz /MR či /MW → QA → /CPU → RDY → CLK |
| `zrdlike.asm` | smyčky CGA-1V ze ZRDBENCH přesně, měřené analyzátorem |
| `zrdtimer.asm` + `zrdtimer_run.py` | smyčky ZRDBENCH v ROM, časovač 82C54 a analyzátor současně (`--noanalyzer` jen časovač) |
| `zrdtimer_manual.py` | totéž bez karty analyzátoru: RESET zmáčkne autor ručně (čtení CGA-1V při 4 MHz s kartou a bez ní) |
| `zrmd.asm` + `zrmd_run.py` | co ZRMD-1 drží přes /RES (adresa A0–A21) |
| `zrdrd.asm` + `zrdrd_run.py` | ZRD-1V rev 1.1: co vrací čtení 61h–63h a kam se pak posune adresa |
| `openbus.asm` + `openbus_run.py` | čtení, na které nikdo neodpoví (poslední bajt datové sběrnice) |
| `ymrst.asm` + `ymrst_run.py`, `ymstat.asm` | YM3812 na MPH-1V: kdy po /RES bere zápisy, stav před a po časovači |
| `ymstat2.asm` + `ymstat2_run.py` | histogramy stavu YM3812 (jednotlivá čtení a bloky INI, před a po časovači) |
| `iorom.asm` + `iorom_run.py` | odpovídá boot ROM JPR-1V na I/O čtení s horním bajtem adresy 00h–03h? (ne) |
| `databus.asm` + `databus_check.py` | kontrola sond datové sběrnice (od 2026-10-13: D3, D4, D5, D10, D11, D13, D14, D15 = D0–D7) |
| `intabus.py` | datová sběrnice kolem INTA při testu `intavec_gap` (co CPU přečte jako vektor) |
| `intblock.asm`, `intblock2.asm` + `intblock_run.py` | po kterých instrukcích Z80 nepřijme přerušení (doba /INT0 → INTA) |
| `and1b.asm` + `and1b_int.py`, `and1b_emu.sapi` | AND-1B místo AND-1V: obraz (skládání sloupců, atributy) pro fotku a emulátor, INT1 na D9 |
| `busclk.py` | takt sběrnice Φ2 (D13 přes DKS-1) proti CLK CPU, frekvence, střída, zpoždění hran |
| `intavec*.asm` | bajt čtený v INTA bez karty s vektorem (IM 2; přerušení z DSM-1V, z 82C54, mezera před INTA) |
| `vbi.asm` + `vbi_run.py` | VBI karty CGA-1V proti VSYNC (D14 na VSYNC) |
| `freeze.asm`, `freeze_and.asm` + `freeze_run.py`, `emu_freeze.py` | zamrzání FXS4: přerušení s PC v okně CGA-1V / AND-1V, na desce a v emulátoru |
| `bench.py` | společné: záznam všech 16 kanálů spuštěný /RES, načtení CSV, analog |
| `hw_run.py ROM OUT [s]` | záznam, nahrání ROM do PicoROM, reset, příjem z COM13, souhrn /IOR a TxD |
| `wire_check.py [ROM] [OUT]` | záznam všech 16 kanálů spuštěný /RES, vyhodnocení po kanálech |
| `decode_ior.py CSV` | dekóduje hodnoty z `diag.asm` |
| `emu_run.py ROM [s] [kanál] [--hex]` | stejná ROM v SAPIemu (`hwbench_v.sapi`), výpis sériového kanálu |
| `pico_run.py`, `pico_out.py` | zkouška kanálu PicoROM oběma směry |
| `logic_stop.py` | zastaví záznamy, které v Logic 2 zůstaly viset |

Pravidla:

- Reset přes PicoROM: `low`, 0,3 s, `z`. Nikdy nenechat `low`.
- PicoROM sleduje jen A0–A10: I/O s A10 = 1 (bit 2 horního bajtu: A u `IN A,(n)`/`OUT (n),A`, B u `IN r,(C)`,
  `OUT (C),r`, `INI`, `INIR`) bere jako přístup do komunikačního okna a při rychlém sledu se zasekne. U I/O držet
  A10 = 0 (např. `LD BC,0856h` + `OUT (C),A`, `INI` s B = F3h–F0h).
- Kanál PicoROM: SAPI smí posílat jen omezené množství nebo na žádost a PC musí vše dočíst před `end_comms`,
  jinak se firmware PicoROM zasekne a pomůže jen odpojit USB.
- DSM-1V: po nastavení UARTu jednou přečíst RAM, jinak zůstane v resetu.
- Analyzátor 125 MS/s, při pochybnostech hrany analogově.
