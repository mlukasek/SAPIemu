# Glossary

The words SAPIemu and its guides use. The column In Czech gives the word of the Czech user interface (View → Jazyk /
Language) and of the Czech guides. In the program the glossary is in Help → Glossary, in Czech in `docs/slovnicek.md`.

## The SAPI-1 computer

| Term | In Czech | Meaning |
|---|---|---|
| SAPI-1 | SAPI-1 | Tesla's industrial control microcomputer (1983–1990) with an 8080 or a Z80 CPU. Its cards sit in a rack on the ARB-1 bus. |
| ARB-1 bus | sběrnice ARB-1 | The common lines of the rack (address, data, control) whose connectors take the cards. The buses ARB-1C and Libor Lasota's ImBUS have other numbers of slots. |
| rack | rám | The cabinet with the bus and the slots. The configurator draws its front after Libor Lasota's modernised cabinet. |
| slot | slot | A place for a card in the rack, X1, X2... X1 holds the CPU card. |
| card, board | karta, deska | A printed circuit board in the rack: the CPU (JPR-1...), memory, a display, a floppy controller... The program says card, the guides often board ("the V boards"). |
| jumper | propojka | A wire link or a switch on a card that sets addresses, memory and options. The configurator sets it as on the real board. |
| PROM, EPROM | PROM, EPROM | Read-only memories. PROMs hold the address decoders of the boards (the emulator reads them from dumps of the real chips), EPROMs hold programs: a monitor, BASIC. |
| port | port | An input or output address (00h–FFh) through which a program talks to a card. The port and memory map is in the configurator. |
| ZPS set | sestava ZPS | A basic computer set as Tesla supplied it: ZPS-1 to ZPS-4 and ZPS-6 (SAPI-1.5). "Set" is used only in these historic names. |
| V boards | desky V | Libor Lasota's modern boards with V in their names (JPR-1V, CGA-1V, MPH-1V...). The machine SAPI-1 V with the games and demos runs on them. |
| TURBO | TURBO | The switch of a CPU card to the double clock (4 MHz). |

## Programs

| Term | In Czech | Meaning |
|---|---|---|
| monitor | monitor | A program in EPROM that runs the computer from the keyboard after power on: memory dumps and changes, starting a program, the tape. |
| MIKROBASIC | MIKROBASIC | The BASIC in the EPROM of ZPS-1. You type a program right after power on. |
| MIKOS | MIKOS | The monitor and cassette system of ZPS-2 (versions V4, V5, MIKOS/K for the data recorder KZD-1, MIKOS/D from a floppy). |
| ECMON | ECMON | The monitor of the machines with the RAM disk ZRMD-1 and of the V machines. After power on it offers the choices 0–3: CP/M from the RAM disk by the drives of the machine (the V machines: `3`). |
| MikroMon, MIKROBIOS | MikroMon, MIKROBIOS | Libor Lasota's monitors for his CPU cards. |
| CP/M | CP/M | A disk operating system: the commands `DIR`, `TYPE`, programs `.COM` on floppies or a RAM disk. |

## Media and data

| Term | In Czech | Meaning |
|---|---|---|
| medium | médium | What goes into a drive: a floppy, a cassette, a tape, a cartridge disk, a hard disk or an SD card. In the emulator it is a file, inserted in the Media window. |
| disk image | obraz disku, diskety | A file with the whole content of a floppy or a disk, sector by sector (`.img`, ImageDisk `.imd`). |
| RAM disk | RAM disk | A disk in the memory of a card (ZRMD-1, RMD-1, ZRD-1V). The V machines boot CP/M from it as the disk A:. |
| working copy | pracovní kopie | The emulator works on a copy of a medium of the folder `media\` in the folder `work\`; the original stays untouched. The button ... of the medium in the Media window → Restore from the original... brings it back. |
| state, saved state | stav, uložený stav | A `.sapistate` file with the whole computer: the memory, the cards, the media and the RAM disks. File → Save state... and Load state... |

## The SAPIemu program

| Term | In Czech | Meaning |
|---|---|---|
| machine | sestava | A computer made of cards and devices, described by a `.sapi` file in the folder `machines\`. |
| Choose a machine | Vybrat sestavu | The window with the machines in groups, with a preview, a description and the Start button (File → Choose a machine...). |
| configurator | konfigurátor sestavy | The window with the rack, the cards, the jumpers and the port map where a machine is changed (View → Machine configurator...). Changes apply after Restart with this configuration. |
| Keep for the next start | Ponechat pro příští spuštění | Changes from the configurator used at every start of the machine, although they are not saved in its file. |
| My machines | Moje sestavy | Your own machines in the folder `work\machines`, at the top of Choose a machine. Save in the configurator puts the changes of a supplied machine into a copy there; the supplied machine stays as it came. |
| Machine (menu) | Stroj (menu) | The running computer: switch on and off, reset (F12), pause, speed, keyboard, sound and media. |
| machine guide | průvodce sestavou | The window with what to try in the machine. The button Type into SAPI types a block of code on the SAPI keyboard (Help → Machine guide). |
| display, screen | displej, obrazovka | The window with the picture of a display card (AND-1, CGA-1V, BGP-1S...). A machine may have more of them; View → Displays hides or shows them. |
| console | konzole | The text display where the monitor, BASIC or CP/M runs. |
| main output | hlavní výstup | The display the machine mainly works on: the console, the graphics for games. The window layout puts it at the top left and it gets the keyboard when the machine starts. |
| instrument | přístroj | A device drawn in its own window: the APS-5 measuring unit, the plotters, the IMS-2 measuring bench. |
| window layout | rozložení oken | The places of the windows: floating, or docked to the edges (View → Settings → Appearance). View → Default window layout brings them back. |
| game mode | režim pro hry | The ANK-1 keys by their position on the PC keyboard (Ctrl+F7), for games such as Manic Miner. |
| joystick from the keyboard | joystick z klávesnice | The arrows and Space as a joystick (Ctrl+F8). |
| log | log | The record of what the emulator did, with the errors and warnings (Debug → Log, the file `sapiemu.log`). It helps with Help → Report a bug... |
| MCP | MCP | A server for developing SAPI programs with Claude. It is in Settings under Advanced (for developers), off in a new installation. |
