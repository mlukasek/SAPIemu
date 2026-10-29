# SAPIemu – getting started

SAPIemu is an emulator of the Czechoslovak computer **Tesla SAPI-1** for Windows 10 and 11 (64 bit). This is the
short English guide; the full Czech one is `PRVNI-KROKY.md` (`docs/prvni-kroky.md`). It is a beta version: most
things work, but expect bugs (see "Reporting a bug" below).

## What SAPI-1 was

SAPI-1 (Tesla, 1983–1990) was a rack-mounted **industrial control computer**, not a home computer. It was designed
by Ing. Eduard Smutný and made by Tesla Liberec. Modular cards with an 8080 or a Z80 CPU sat in a rack on the ARB-1
bus; such a computer controlled furnaces, for example, or measured and recorded data. The basic computer sets ZPS-1
to ZPS-4 and SAPI-1.5 (ZPS-6) ran MIKROBASIC, the MIKOS monitor or CP/M. In the emulator you build a machine from cards and
set their jumpers like on the real boards. Start with ZPS-1, then try the instruments. The games and demos are a
modern extra on new boards: SAPI-1 was never a games machine.

## Start

1. Unpack the zip into a **new** folder (for example `C:\SAPIemu`), not over an older version.
2. Run **`sapiemu.exe`**. Windows warns about an unsigned program (SmartScreen): "More info" → "Run anyway".
3. Nothing is installed. The settings, the window places and the logs of the last two runs (`sapiemu.log`,
   `sapiemu.prev.log`) are kept next to the exe, the working copies of disks and tapes in the folder `work\`. If the
   folder cannot be written to (for example under `Program Files`), they go to `%APPDATA%\SAPIemu\SAPIemu` and
   `%LOCALAPPDATA%\SAPIemu\work`.
4. The user interface is in Czech and English: **View → Jazyk / Language**. The first start follows the language of
   Windows.
5. Run one window of SAPIemu. A second window over the same settings or the same `work\` folder warns "Another
   SAPIemu is running": the two would overwrite each other's settings, RAM disks and floppies. Quit closes it without
   touching any file, Continue keeps it.

This guide is also in the program: **Help → First steps**. **Help → Glossary** explains the words of SAPI and of
SAPIemu (machine, configurator, card, slot, jumper...).

## Choose a machine

**File → Choose a machine...** opens by itself at the first start; ZPS-1 already runs behind it. The machines are in
groups: **Basics: historic Tesla sets**, **Instruments and equipment**, **Boards by Libor Lasota (modern)**, **Games
and demos (modern)** and more groups with variants at the end. Each machine has a preview, a short text, its cards
by slot and a Start button (a double click starts it too). **Recent machines** are at the top (as in File → Recent
machines), and **My machines** too once you have saved a copy of a machine (see Saving). A machine that cannot start
(a missing ROM, a damaged file) does not stop the running one: the Error window says why and that the machine runs on
as it was.

After a start from the first four groups the **Machine guide** opens: what to try, in English when the user
interface is English. The button **Type into SAPI** types a block of code for you. **Help → Machine guide** brings
it back at any time.

## The basic machines

| Machine | What to try |
|---|---|
| ZPS-1 with MIKROBASIC 2.4 | type `10 FOR I=1 TO 5`, `20 PRINT I,I*I`, `30 NEXT I`, `RUN`; `LIST` shows the program, `NEW` clears it |
| ZPS-2 with MIKOS V4 | the monitor MIKOS with a tape recorder: `Q` lists the devices, `D0 1F` dumps memory |
| ZPS-2 with MIKOS V5 and graphics DGD-1 | File → Load program into memory (Intel HEX)... → `media\mikos\BASIC_EXP_V5.0G.hex`, `B`, then the line from the guide draws a fan of lines |
| ZPS-3 with CP/M 64K of Tesla IMA | the original CP/M from an 8" floppy: `DIR B:`, then `B:` and `MBASIC` (`PRINT 6*7`, `SYSTEM` goes back) |
| ZPS-4 with CP/M 62K | `DIR`, `TYPE CPM.DOC` |
| ZPS-6 (SAPI-1.5) | CP/M 2.2 from a 5.25" floppy with the colour semigraphics DBS-1A: `DIR`, `SETDBS1` |

## The instruments

Each instrument is drawn in its own window next to the screen.

- **APS-5 measuring unit (furnaces):** the original Tesla firmware controls eight furnaces. The **DEMO** button in
  the APS-5 window types the whole setup by itself; in about a minute the furnaces glow. Do not use Reset here: the
  real unit has none and its setup would be lost (the emulator asks first). Switch it off and on instead.
- **Measuring bench on the IMS-2 bus:** BASIC with `IMS#` commands reads a Metra voltmeter and sets a Tesla power
  supply; the knobs of the supply turn with the mouse.
- **Plotters:** Minigraf 0507 (`.G=1809` draws the character set), XY 4130 (press PEN in its window, then
  `.G=181E`) and the plotter from the magazine Amatérské radio 1990 (READY, then Demo or TEST).
- **Data storage:** the cassette data recorder KZD-1, the cartridge disk RKD-1 and the magnetic tape RMP-1, with
  their mechanics drawn in the Media window.

The guide of each machine has the steps. The longer user guides (Help → User guides) are in Czech.

## The games and demos

- **SAPI-1 V: games and demos:** type `3` (CP/M from the RAM disk), then a program name: `BADAPPLE`, `FXS4`
  (music, keys A–Z), `FLAPPY`, `BOMBER`, `FLAPDGD`, `BKP`. `DIR` lists everything. ESC usually returns to CP/M; a
  program that does not end by itself is stopped by F12. Warning: Bad Apple!! flashes fast in places; skip the demos
  if flashing light bothers you.
- **SAPI-1 V: Prince of Persia:** `3`, then `POP`. An unofficial, non-commercial port shown as a demonstration of
  historic technology.
- **SAPI-1 V: BomberNet:** `3`, then `BOMBNET`; it can play over the internet only after you choose HOST or JOIN.
- **Manic Miner** (JPR-1S and graphics BGP-1S): File → Load program into memory (Intel HEX)... →
  `media\jpr-1s\manic_miner.hex`, then `JUMP 4400`; Ctrl+F7 is the game mode of the keyboard.
- **Ctrl+F8** turns on the joystick from the PC keyboard (arrows, space = fire). The sound plays at 100 % speed only.

## Menus and settings

- **View → Settings...** in sections: **General** (Jazyk / Language, text size, the guides), **Appearance** (mode:
  as the system, light or dark; windows: floating or docked), **Picture** (integer scaling, TV aspect ratio,
  smoothing), **Sound** (on, volume), **Controls** (mouse sensitivity, the PC mouse as the SAPI mouse until exit),
  **Recording** (enlarged screenshots, the path to ffmpeg with a line saying whether it was found, the folders of
  screenshots and videos) and, collapsed, **Advanced (for developers)** with the MCP server.
- **View → Displays** lists the display windows of the machine by their cards (AND-1V, CGA-1V...). Untick one to hide
  it (the machine runs on), tick it to bring it back with the keyboard. Another machine shows all of them again.
- **View → Open the folder of screenshots and videos** opens `work\snimky` or `work\videa` in Explorer.
- **Machine → Media...** opens the Media window, as View → Media does. The media show their file names; the full
  path is in the tooltip.
- **Machine → Keyboard** swaps the keyboard while running. It is a change of the machine: the configurator shows it
  as unsaved, and switching off and on keeps it.
- The windows of the instruments (Serial terminal, Printer, Plotter, IMS-2, Diagnostic board, APS-5) are greyed out
  when the machine has no such device; the tooltip says which machine in Choose a machine has it.
- **Drag and drop:** a file dropped from Explorer on the SAPIemu window does what the menu does. A `.sapi` starts the
  machine, a `.sapistate` loads the state, `.hex` and `.bin` open the Load program windows, a floppy image (`.img`,
  `.imd`...), an RKD-1 cartridge (`.rkd`) or an RMP-1 tape (`.tap`) goes into the first empty drive, a cassette
  (`.wav`, `.mp3`, `.kzd`) into the recorder. The status bar says where it went.

## Keyboard and focus

- **Click the screen window** before typing: the keys then go to the SAPI keyboard. While the screen does not have
  the keyboard, it shows "Click here and type".
- **F5** pause, **F8** speed (100 / 200 / 400 % / maximum), **F12** reset (the memory stays), **Ctrl+F12** switch
  off and on (the memory is lost, like the power switch).
- **Ctrl+Shift+V** types the clipboard on the SAPI keyboard (a BASIC program, for example), **Ctrl+Shift+C** copies
  the screen text.
- **Ctrl+F5** screenshot, **Alt+Enter** full screen, **Ctrl+F2 / Ctrl+F3** quick save and load of the state.
- All the keys are in **Help → Keyboard shortcuts**. In the debugger F9 (breakpoint), F10 and F11 work as in
  Visual Studio. F9 is no longer a reset, as it was in the alpha versions.
- **In the windows of the emulator** (dialogs, Settings, the configurator) Tab shows a frame on a control and moves
  it to the next one, then the arrows move it too; Space and Enter press the control in the frame, a mouse click
  hides the frame, Esc closes a dialog. Without the frame the arrows and Space press nothing. In the screen window and
  the serial terminal these keys belong to SAPI.
- **Screen readers do not see the user interface:** it is drawn by Dear ImGui, which gives the system no tree of
  controls. The text of the SAPI screen can be copied (Ctrl+Shift+C) or printed by `sapiemu-cli --print-screen`.

## Saving

- **Disks, tapes and RAM disks** are saved by themselves: a few seconds after the last write, at a pause and when
  you switch to another window. The originals in `media\` stay untouched; the emulator works on copies in `work\`.
  In the Media window the button "..." of a drive offers **Restore from the original...**.
- **File → Save state... / Load state...** stores the whole computer in a `.sapistate` file. A state that cannot be
  loaded leaves the running machine as it is. The quick save (Ctrl+F2) has one slot per machine; a copy in My
  machines has its own.
- **Machine configurator...** (View menu): the rack and three tabs below it, **Machine and bus**, the card of the
  selected slot (for example "X7 CGA-1V": jumpers, ROMs, images) and **Port and memory map**. **New** offers an empty
  machine or a copy of another, in the groups of Choose a machine. Changes need "Restart with this configuration".
  When you leave with unsaved changes, SAPIemu lists them and offers Save, Save as..., **Keep for the next start** or
  Discard changes. Keep for the next start leaves the file as it is and uses the changes at **every** start of that
  machine until you save them or press Revert in the configurator; each such start says so in the status bar, and
  Choose a machine marks the machine with `*`.
- **My machines:** Save never overwrites a machine supplied with SAPIemu. It writes **your copy** into
  `work\machines\` (the tooltip of Save says so), named as the original with "(mine)", the second copy "(mine 2)".
  The running machine, the window title and Recent machines then follow the copy, and the next Save writes into it.
  In Choose a machine the copies are at the top under My machines, with the preview and guide of the original;
  "Modified from" selects the original, which is unchanged, and **Delete...** removes the copy. Your own machine
  file (saved elsewhere, or under a new name) is saved as usual.

## Reporting a bug

**Help → Report a bug...** says what to describe and shows the version, the machine and the folders. **Copy
diagnostics** puts all of that and the end of the log into the clipboard; paste it into your report. A screenshot
(Ctrl+F5) and a state saved just before the bug help a lot. Until SAPIemu is public, send the report the way you
got the program; after that, use GitHub Issues.

**After a crash**, start SAPIemu again and send `sapiemu.prev.log` too (Report a bug → Open the log folder): it is
the log of the run that crashed. The start after that replaces it, so send it right away.

## Where the docs are

- In the program: Help → First steps (this guide) and Help → Glossary in the Help window (chapters fold, links open
  the web or the file), the Machine guides (English and Czech) and Help → Keyboard shortcuts.
- `docs/glossary.md`: the glossary as a file (in Czech `docs/slovnicek.md`).
- `README.md`: an English summary at the top, then the full Czech description.
- `PRVNI-KROKY.md`: the full first steps in Czech, with every machine and what to type.
- `docs\`: user guides of the instruments (`navod-*.md`), notes on each board (`docs\desky\`) and on the software
  (`docs\software\`), all in Czech.
