#pragma once
// Shared state of the frontend windows.
#include <stdint.h>
#include "core/xmodem.h"
#include <mutex>
#include <string>
#include <vector>
#include "json.hpp"
#include "core/machine_index.h"
#include "core/machine_map.h"
#include "core/term.h"
#include "plant/aps5_demo.h"

class Card;
class DisplayDevice;
class EmuHost;
class McpServer;
struct ImFont;
struct SDL_Window;

// Where a native file dialog opens: a folder gets the separator at its end (without it Windows takes the
// last part for a file name and opens the folder above), the separators of the platform; a file (a name
// offered for saving) stays a file.
std::string dialog_location(const std::string& path);

// Cassette dialogs of the Media window (answer from another thread).
struct TapeDialog {
    std::mutex mtx;
    int kind = 0;            // 0 none, 1 insert, 2 new tape, 3 Intel HEX into memory,
                             // 4 insert floppy, 5 new floppy, 6 binary file into memory,
                             // 7 save state, 8 load state, 9 export tape as WAV,
                             // 10/11 KZD-1 cassette, 12 insert RKD-1 cartridge, 13 new cartridge,
                             // 14 insert RMP-1 tape, 15 new tape
    bool done = false;
    std::string result;      // chosen file, empty = cancelled
    std::string card;        // DSM-1 / RPD-1 card name
    int drive = 0;           // RPD-1 drive (0 = A)
    std::string last_dir;    // where the last tape came from
};

// Machine configurator (View -> Machine configurator, window id ###rack): edits a copy of a machine description.
struct RackEditor {
    nlohmann::json cfg;            // description being edited
    nlohmann::json saved;          // as last opened / saved (unsaved changes = cfg != saved)
    std::string path;              // file (data root relative when inside it), empty = not saved yet
    uint64_t machine_gen = ~0ull;  // generation of the running machine the editor follows
    // live parameters as the running machine had them last frame ("slot/path" -> value): a value
    // changed by the machine side (MCP) goes into the editor, a change in the editor to the machine
    nlohmann::json live_seen;
    uint64_t live_gen = ~0ull;
    int sel = 0;                   // selected slot, 0 = the bus
    int drag_from = 0;             // first slot of the card dragged with the mouse, 0 = none
    int drag_grab = 0;             // its board under the mouse (0 = first, 1 = second of RPD-1, FDC-1)
    float drag_dx = 0;             // mouse x from the left edge of its first board when picked up
    // where the slots were drawn last frame (screen): left edge of the last slot, slot pitch,
    // top and bottom, number of slots; the UI snapshot drags cards by them
    float geo_x0 = 0, geo_pitch = 0, geo_y0 = 0, geo_y1 = 0;
    int geo_n = 0;
    bool show_types = true;        // exact card type across the panels
    // port and memory map of `cfg`, rebuilt when it changes
    nlohmann::json map_cfg;
    MachineMap map;
    std::vector<CfgProblem> problems;   // layout problems and map conflicts
    int map_state = 0;                  // MAP1/MAP2 state shown in the map
    bool open_map_tab = false;          // select the map tab once (UI snapshot)
    bool scroll_props = false;          // a slot was clicked: its settings tab selected and scrolled into view
    // the tabs under the frame, always the same three: Machine and bus | the card of the selected slot | the map
    enum { TAB_BUS = 0, TAB_CARD, TAB_MAP };
    int tab = TAB_BUS;                  // the one shown last frame
    int tab_select = -1;                // select this one in the next frame (-1 = none)
    int tab_sel = 0;                    // the slot the tabs had last frame: another one chosen (right click, a card
                                        // dragged) brings its tab from the bus tab, not from the map
    bool new_menu = false;              // open the menu New (UI snapshot --ui-rack-new)
    bool save_now = false;              // Save as if pressed (UI snapshot --ui-save)
    std::string message;
    bool message_error = false;
    // Question "save the unsaved changes?" before the description is replaced: the
    // action waits for the answer (Save, Save as, Keep for the next start, Discard changes, Cancel).
    enum { PEND_NONE = 0, PEND_START, PEND_OPEN, PEND_TEMPLATE, PEND_EMPTY, PEND_FOLLOW, PEND_EXIT };
    int pending = PEND_NONE;
    std::string pending_path;      // machine file of PEND_START / PEND_OPEN / PEND_TEMPLATE
    bool ask_unsaved = false;      // open the question in the next frame
    bool save_then_continue = false;   // "Save as" dialog for the pending action
    // what changed against `saved` (machine_cfg_changes), for the question and the title; computed again when
    // cfg or saved change
    std::vector<std::string> changes;
    nlohmann::json changes_cfg, changes_saved;
    int changes_lang = -1;
    // "Keep for the next start": machine file (data relative) -> { "base": the file as it was, "cfg": the description
    // with the changes }. A start of the file from the GUI and the configurator opening it take the changes, while the
    // file is still the same; Save, Revert and Discard changes forget them. In the settings ("kept_machines").
    nlohmann::json kept = nlohmann::json::object();

    // native file dialogs answer from another thread
    enum { DLG_NONE = 0, DLG_OPEN, DLG_SAVE, DLG_PARAM, DLG_RUN, DLG_PARAM_NEW };
    std::mutex dlg_mtx;
    int dlg_kind = DLG_NONE;       // pending request
    bool dlg_done = false;
    std::string dlg_result;        // chosen path, empty = cancelled
    int dlg_card = -1;             // DLG_PARAM: card index and parameter
    std::string dlg_param;
};

struct UiState {
    EmuHost* host = nullptr;
    McpServer* mcp = nullptr;
    ImFont* mono = nullptr;

    bool show_debugger = false;
    bool show_memory = false;
    bool show_terminal = false;
    bool show_iolog = false;
    bool show_log = false;
    bool show_media = false;
    bool show_printer = false;
    bool show_plotter = false;     // the plotter Minigraf (DPP-1)
    uint64_t plotter_gen = 0;      // machine generation the plotter window opened itself for
    bool show_diag = false;        // the diagnostic board on K3 (JPR-1S, JPR-1X)
    bool show_aps5 = false;        // APS-5: the front panel and the furnaces
    bool show_ims2 = false;        // the measuring bench on the IMS-2 bus of DPS-1
    uint64_t ims2_gen = 0;         // machine generation the bench window opened itself for
    uint64_t aps5_gen = 0;         // machine generation the APS-5 window opened itself for
    int aps5_span = 1;             // plot: 0 = 10 min, 1 = 1 h, 2 = 4 h
    float aps5_outage_s = 60.0f;   // length of an outage from the window
    Aps5Demo aps5_demo;            // DEMO button: an operator typing the example
    uint64_t aps5_demo_gen = 0;    // the machine it types on
    bool aps5_demo_fast = false;   // it switched the emulation to full speed
    bool aps5_demo_was_max = false;   // the speed before
    bool aps5_demo_autostart = false; // --ui-aps5-demo: start it without the dialog
    bool ask_reset = false;        // request_reset: open the question "Reset the APS-5 unit?" in the next frame
    uint64_t ask_reset_gen = 0;    // the machine it asks about (another one closes the question)
    bool show_rack = false;
    // The configurator in the default layout (author 2026-10-29): View -> Default window layout and the first start
    // (no saved window places, its open state never saved) open it only where it fits clear of the other windows
    // (layout_window_if_room, draw_rack decides after the other windows took their places). rack_by_layout: open
    // because the layout opened it, not used yet (no click or key in it, not asked for from a menu; settings
    // "configuration_auto"): the lowest priority (layout_rack_low), behind the other windows when it appears, and the
    // next Default window layout decides again.
    bool rack_auto = false;
    bool rack_by_layout = false;
    // the window "Choose a machine" and the guide of a machine (frontend/browser.cpp)
    bool show_browser = false;
    bool browser_loaded = false;   // the index is read when the window opens
    bool browser_first = false;    // opened by the first start: closed without starting a machine, it lets the
                                   // guide of the running one (ZPS-1) open itself once
    std::string browser_sel;       // the machine selected in it
    std::string browser_sel_cat;   // the category of the row selected ("recent": Recent machines): one row lit
    std::string browser_sel_title; // the title of that row when the category has its own (Manic Miner), else empty
    // its start waits for the question about unsaved changes of the configurator: the window closes when the start is
    // done (rack.cpp start_machine), it stays open on Cancel
    bool browser_start = false;
    MachineIndex machine_index;
    bool machine_index_tried = false;  // machine_title read it once (an empty index is not read every frame)
    // File -> Recent machines (and the same list at the top of the window): the machine files started from the GUI and
    // saved by Save as... (UX review BEH-19), newest first (settings; recent_add)
    std::vector<std::string> recent;
    // the devices of the machine as it ran last (machine_devices_seen: a bit for each key of machine_has_device), for
    // the View items while it is switched off; devices_known: a machine ran in this run
    unsigned devices_last = 0;
    bool devices_known = false;
    bool show_settings = false;    // View -> Settings
    bool show_about = false;       // Help -> About SAPIemu
    bool show_shortcuts = false;   // Help -> Keyboard shortcuts
    bool show_report = false;      // Help -> Report a bug
    std::string report_status;     // its last copy ("Copied..."), shown under the buttons
    // Help -> First steps, Glossary: one window with a tab for each (help.cpp, HELP_*)
    bool show_help = false;
    int help_page = 0;             // the tab shown
    bool help_select = false;      // help_page chosen from the menu or a link: its tab selected in the next frame
    bool help_focus = false;       // the window to the front (it may be open behind others)
    int help_set_open = 0;         // the sections: 1 all open, -1 all closed (--ui-help-open), done when drawn
    float help_scroll = -1;        // --ui-help-scroll: the text scrolled to this part of it (0..1), -1 = as it is
    std::string settings_file;     // the settings of this run and its log (app_run), for Report a bug; empty = none
    std::string log_file;
    std::string log_prev_file;     // the log of the run before (after a crash), empty = none
    bool show_guide = false;
    bool guide_focus = false;      // bring the guide window to the front (Help -> Machine guide, the button Guide)
    std::string guide_machine;     // the machine whose guide is shown
    uint64_t guide_gen = ~0ull;    // the machine generation the guide was considered for
    // the machine generation the user started (Start, Restart, File -> Recent machines, Start a machine from a
    // file): only its guide opens itself, not after a power cycle, a restart from the configurator, a state or MCP
    // (2026-10-17)
    uint64_t guide_start_gen = ~0ull;
    bool guides_auto = true;       // guides open themselves for the basic, Libor's and the games machines (settings)
    std::vector<std::string> guides_hidden;   // machines whose guide does not open itself (settings)
    bool layout_reset = false;     // View -> Default window layout: done before the windows of the next frame (app.cpp)
    bool layout_arrange = false;   // View -> Arrange open windows: the same for the open windows only
    bool layout_fresh = false;     // no imgui.ini at the start: the first machine started from the window Choose a
                                   // machine of the first start lays the windows out for itself (browser.cpp)
    bool quit = false;             // exit confirmed (no unsaved machine description)
    std::string error;             // error for the message box of the main window
    SDL_Window* window = nullptr;
    RackEditor rack;
    TapeDialog tape_dlg;
    // File -> Load binary file: the chosen file waits for its address
    std::string bin_path;          // chosen file, empty = no question open
    bool bin_ask = false;          // open the question in the next frame
    char bin_addr[8] = "0100";     // load address (hex), remembered in the settings
    // save state: the result of the last save / load (the status bar shows it for a while,
    // an error opens the message box)
    std::string notice;
    bool notice_error = false;
    uint64_t notice_ns = 0;
    // screenshots (frontend/shot.cpp): enlarged 3x with the TV aspect instead of the exact dots
    // (remembered in the settings); "Save screenshot as" keeps the picture until the file is chosen
    bool shot_enlarged = false;
    std::string ffmpeg_path;       // video: ffmpeg from the settings ("video": {"ffmpeg"}), empty = from PATH
    struct ShotDialog {
        std::mutex mtx;
        bool active = false, done = false;
        std::string result;
        std::vector<uint32_t> px;
        int w = 0, h = 0, sx = 1, sy = 1;
    } shot_dlg;
    // File -> Load program (Intel HEX): the chosen file waits for a confirmation, with the
    // addresses of its records (read without loading)
    std::string hex_path;          // chosen file, empty = no question open
    bool hex_ask = false;          // open the question in the next frame
    std::string hex_error;         // the file cannot be read or is not valid Intel HEX
    int hex_bytes = 0;
    uint32_t hex_lo = 0, hex_hi = 0;
    // Media -> Insert a program (HEX, BIN) into a recorder: the format of the tape first
    std::string tprog_path;        // chosen file, empty = no question open
    std::string tprog_card;        // DSM-1, or DPP-1 with the KZD-1
    bool tprog_kzd = false;        // for the KZD-1: a new cassette as MIKOS/K writes it
    bool tprog_ask = false;        // open the question in the next frame
    int tprog_format = 0;          // TAPE_MIKOS (remembered while running)
    char tprog_name[12] = "";
    char tprog_addr[8] = "4400";   // load address of a binary file (hex)
    char tprog_entry[8] = "";      // start address (hex), empty = the lowest address
    std::string tprog_error;

    // keyboard routing (from the previous frame)
    bool screen_focused = false;
    // frames in which the display used last takes the keyboard back (app.cpp): after Type into SAPI, Paste text, a
    // program loaded, a medium put in, the guide or the window Choose a machine closed; the windows keep their order
    int focus_display = 0;
    bool terminal_focused = false;
    int terminal_channel = 0;
    // terminal options for new terminals (remembered in the settings)
    int term_emulation = 0;            // TERM_SAPI
    bool term_cr_newline = false;
    bool term_local_echo = false;
    int tcp_port = 2323;
    bool tcp_dcd_follows = true;
    int tcp_protocol = 0;   // TCP_TELNET
    char com_port[32] = "COM1";
    std::string bridge_error;

    // debugger
    int disasm_hover = -1;         // the address of the disassembly row under the mouse last frame (F9), -1 none
    bool disasm_follow = true;
    uint16_t disasm_addr = 0;
    char disasm_goto[8] = "";
    char bp_input[8] = "";
    int bp_kind = 0;
    uint16_t mem_addr = 0;
    char mem_goto[8] = "";
    bool mem_jump = false;         // scroll the memory view to mem_goto in the next frame (UI snapshot --ui-memory)
    char mem_edit_addr[8] = "";
    char mem_edit_data[64] = "";

    // terminals, one per serial channel of the current machine
    uint64_t machine_gen = 0;
    std::vector<TerminalState> terms;
    // terminal: sending a file (1 as it is, 2 text, 3 XMODEM), receiving one (4 XMODEM), the clipboard
    TapeDialog term_dlg;
    XmodemSender xmodem;
    XmodemReceiver xmodem_rx;
    std::string xmodem_rx_path;        // where the received file goes (chosen before)
    bool xmodem_rx_saved = false;
    bool term_strip_sub = true;        // cut the 1Ah padding off the end of a received file
    int xmodem_channel = -1;
    uint64_t xmodem_pos = 0;           // read position in the stream of the channel
    double xmodem_last = 0;            // ImGui time of the last answer of the receiver
    std::string term_status;           // result of the last send / copy
    bool term_selecting = false;       // the mouse is selecting text in the terminal
    bool term_copy_on_select = true;   // a finished selection goes to the clipboard at once (PuTTY, TeraTerm)
};

// The result of a save, a load, a screenshot... (app.cpp): the status bar shows it for 7 s and it goes into the Log;
// an error opens the window Error.
void ui_notice(UiState& ui, bool ok, const std::string& text);
// The keys of a modal window, called between its BeginPopupModal and EndPopup (app.cpp): Enter for its default button
// (only where that is not destructive), Esc for Cancel / Close. Only the window in front answers, and not in the frame
// it opens (the key that opened it does not answer it).
bool popup_enter(void);
bool popup_escape(void);
// The keyboard navigation (Tab, arrows; io.ConfigFlags NavEnableKeyboard) shows its cursor on an item of the current
// window and no text field is being edited: Enter and Space belong to that item (ImGui presses it), not to a default.
bool nav_cursor_on_item(void);
// The previews of the machines (sapiemu --ui-snapshot --ui-crop KEY --ui-picture, tools/make_previews.py): a window
// of an instrument marks its drawn hardware (the panel, the mechanism, the paper) and leaves out the controls of the
// emulator around it, so that the picture does not depend on the language of the UI. picture_only(): this run makes
// such a preview; picture_mark(): the rectangle (screen points, with the margin it wants) to add to the picture of
// this frame.
bool picture_only(void);
void picture_mark(float x0, float y0, float x1, float y1);
// The text of the console display to the clipboard (View -> Copy screen text, Ctrl+Shift+C), told in the status bar.
void copy_screen_text(UiState& ui);
// Debug -> Breakpoint on / off (F9, as in Visual Studio): in the debugger at the disassembly row under the mouse, else
// at the PC; without the debugger open only a hint (F9 was the reset before).
void debug_toggle_breakpoint(UiState& ui);
// Serial terminal window: the clipboard (Alt+C / Alt+V as in TeraTerm, Ctrl+Insert / Shift+Insert)
// Screenshots of a display output (frontend/shot.cpp): PNG of the exact emulated dots (or enlarged 3x with
// px_aspect = the width of a dot on a TV), named by the card and the time in work/snimky, into a chosen
// file, or to the clipboard (PNG and BMP). The result shows in the status bar.
void shot_save(UiState& ui, DisplayDevice* d, const char* name, float px_aspect);
void shot_save_as(UiState& ui, DisplayDevice* d, const char* name, float px_aspect);
void shot_copy(UiState& ui, DisplayDevice* d, float px_aspect);
void shot_poll(UiState& ui);   // the file of "Save screenshot as", once chosen
std::string shot_dir(void);    // the folder of the screenshots (work/snimky in the work root)
std::string video_dir(void);   // the folder of the videos (work/videa)
// Video of a display output with the sound (frontend/video.cpp): MP4 through ffmpeg into work/videa,
// the frames at 50 per second of emulated time, enlarged like a screenshot "as shown".
std::string video_ffmpeg(UiState& ui);   // the ffmpeg to use, empty if none
bool video_recording(UiState& ui);
double video_seconds(UiState& ui);       // length recorded so far
void video_start(UiState& ui, DisplayDevice* d, const char* name, float px_aspect);
void video_stop(UiState& ui);
void video_poll(UiState& ui);            // every frame: a failure, a changed machine, the end of the mux
void video_shutdown(UiState& ui);        // at exit
bool video_busy(UiState& ui);            // recording, or the last video still being finished
void terminal_copy(UiState& ui);
void terminal_paste(UiState& ui);

// A file of a medium or an output as the windows show it (Media, Printer): its name, the whole path in a wrapped tooltip
// (a relative one from the data root). disabled: in the colour of the secondary text.
void file_name_text(const std::string& path, bool disabled = false);
void draw_debugger(UiState& ui);
void draw_memory(UiState& ui);
void draw_iolog(UiState& ui);
void draw_terminal(UiState& ui);
void draw_media(UiState& ui);
void draw_printer(UiState& ui);
void draw_plotter(UiState& ui);
// File -> Choose a machine: categories, previews, short texts (docs/vydani/sestavy.md); the guide of a machine
void browser_open(UiState& ui);
void draw_browser(UiState& ui);
// A start from the window is done (rack.cpp start_machine): the window closes; started (ok), the keyboard goes to the
// display and the first machine of the first start lays the windows out for itself.
void browser_started(UiState& ui, bool ok);
// A machine file at the top of Recent machines: started from the GUI (not a start that failed or was cancelled, not
// MCP or sapiemu-cli), saved by Save as... in the configurator.
void recent_add(UiState& ui, const std::string& file);
// The name of a machine file for the title of the OS window, File -> Recent machines and the configurator: the title
// of the index in the UI language (title_en in English) when the file is in the index and `name` (its name now, empty
// = any) is the name in the file (not renamed in the configurator); else `name`, or the file when it is empty.
std::string machine_title(UiState& ui, const std::string& file, const std::string& name);
// The machine index read again (machines saved since, the configurator's New).
void machine_index_refresh(UiState& ui);
// The machines of the index as menus by category, for a popup or a menu (the configurator's New): the categories of a
// group in a menu of their group, the titles as in Choose a machine, the file and the summary in the tooltip. Returns
// the file chosen in this frame, else empty.
std::string machine_index_menus(UiState& ui);
void draw_guide(UiState& ui);
// Help (frontend/help.cpp): About, Keyboard shortcuts, Report a bug, the user guides of docs/, the window Help with
// First steps and the Glossary (docs/prvni-kroky.md, docs/slovnicek.md; English: docs/getting-started.md,
// docs/glossary.md) drawn by mdview.cpp
enum HelpPage { HELP_FIRST_STEPS, HELP_GLOSSARY, HELP_PAGES };
void help_open(UiState& ui, int page);   // the window Help with that page in front
void draw_help(UiState& ui);
void draw_about(UiState& ui);
void draw_shortcuts(UiState& ui);
void draw_report(UiState& ui);
std::string report_diagnostics(UiState& ui, int log_lines);   // the text "Copy diagnostics" puts on the clipboard
void help_docs_menu(UiState& ui);
void open_doc(const char* rel);   // a file of docs/ in the program of the system
// A folder in the file manager of the system (Explorer), made first when it is not there yet (an empty folder shows
// where the files will go): the log folder, the folders of screenshots and videos.
void open_folder(const std::string& dir);
// Default places of the windows (frontend/layout.cpp), called before Begin: the place and the size at the first use
// of a window (ImGuiCond_FirstUseEver: a place saved in imgui.ini wins, View -> Default window layout takes them again).
// A dialog opening at a place partly outside the work area comes back inside it.
enum LayoutWin {
    LW_GUIDE, LW_MEDIA, LW_PLOTTER, LW_APS5, LW_IMS2, LW_DIAG,
    LW_DEBUGGER, LW_MEMORY, LW_IOLOG, LW_TERMINAL, LW_PRINTER, LW_LOG, LW_RACK,
    LW_BROWSER, LW_SETTINGS, LW_SHORTCUTS, LW_ABOUT, LW_REPORT, LW_HELP,
    LW_COUNT
};
void layout_window(int which);
// A window the layout opens itself (the configurator: View -> Default window layout, the first start): its first place
// only in the free room clear of every window, at a readable size (font heights). False: no
// such room (or it has a saved place, or no first places now), it stays closed. (The configurator: 44 x 44.)
bool layout_window_if_room(int which);
// After Begin of a window that just opened by itself: behind the other windows (the dialogs stay in front of it).
void layout_to_back(void);
// Every frame before the windows (app.cpp): the configurator is open because the layout opened it (ui.rack_by_layout),
// the lowest priority: the windows opening later take their places as if it was not there (they cover it).
void layout_rack_low(bool low);
float layout_width(int which);           // the width it asks for (pixels): the main output leaves room for it
// The work area of this frame (the main window below the menu bar, after draw_menu) less the gap kept at its edges and
// between the windows (pixels).
void layout_work_size(float* w, float* h, float* gap);
bool layout_first_use(const char* id);   // the window ("###screen") takes its first place in this frame
// The main output planned for this frame (app.cpp, before the windows): its window id, its first size (pixels) and the
// number of graphic display windows (###gfx0...) drawn with it.
void layout_main(const char* id, float w, float h, int gfx_count);
// A display window ("###gfx0"), its size at zoom k zero + k * step (pixels: the frame with the small border, and the
// picture at 1:1; step 0 = zero is its size). The main output where it was planned; another display (or the main output
// when another window is there) in the free room next to the main output at the largest zoom from k_max down to 1:1,
// then 3/4; else at 1:1 clear of the other displays, covering the least of the rest. exact: only at k_max (a zoom chosen
// from the menu, View -> Arrange open windows): in the free room, else clear of the other displays covering the least.
void layout_display(const char* id, float zero_w, float zero_h, float step_w, float step_h, float k_max,
                    bool exact = false);
// A machine change (app.cpp, before the windows): the display window `id` keeps its place and takes the size w x h
// (pixels), moved into the work area (smaller only when it does not fit). Not a window that takes its first place now
// (nor one neither created nor saved) or a docked one. Before the windows: those opening later see the new size.
void layout_refit(const char* id, float w, float h);
// Every frame after the menu bar, before the windows (app.cpp): when the work area has a new size (the start, the main
// window resized, moved to a smaller monitor) for a few frames, every floating window and every saved place of a window
// not created yet that is more than half outside it (a dialog: any part; a display: any part when it fits whole) comes
// back inside with the gap of the layout, the least move, its size kept unless it does not fit (then smaller; a display
// that fits only without the gap keeps its size). Not while the windows take no first places (full screen, minimized).
void layout_keep_inside(void);
// View -> Default window layout: the saved places of the windows not created in this run are forgotten too.
void layout_forget_closed(void);
// The window places of the other layout just loaded (View -> Settings -> Windows): the windows that have none in it take
// their default places.
void layout_take_default_unsaved(void);
// Finding a window (author 2026-10-29: "a window easily hides behind another"). A window is covered when it is open and
// collapsed, a tab of a dock that is not selected, or another window in front of it overlaps it (not a menu or popup;
// a window docked into the dock space over the main window only by the first two: it cannot come in front of a
// floating window). id: the part of the title from ### on ("###Media"). layout_to_front: an open window to the front with the keyboard,
// its tab selected in a dock, a floating one moved into the work area when it is partly outside.
bool layout_covered(const char* id);
void layout_to_front(const char* id);
// View -> Windows: the open windows (docked or floating, not the menus and popups), the displays first, then in the
// order of LayoutWin, the others last. title: as shown (a display without its zoom).
struct LayoutOpenWindow {
    std::string id, title;
    bool covered = false;
};
void layout_open_windows(std::vector<LayoutOpenWindow>& out);
// No first places while the display is in full screen (app.cpp, every frame): a window opening then takes its place in
// the main window at the first frame after it. layout_held: that, or the main window too small (minimized).
void layout_hold(bool hold);
bool layout_held(void);
// The docked layout (View -> Settings -> Windows: docked), built before the dock space of the frame: the main output in
// the middle; a column at the right edge with the other displays at its top, under them the windows of the machine
// (instruments, Media, terminal, printer) as tabs, at its bottom the guide and the debugging tools as tabs. A part with
// no window open takes no room. The dialogs and the configurator stay floating.
struct DockPlan {
    const char* displays[5];   // display window ids ("###screen", "###gfx0"...): the main output first, then the other
    int n_displays;            // displays of the machine, then those of other machines (they dock there when they come)
    int machine_displays;      // the displays of the machine (the first ones of `displays`)
    float side_w;              // the widest window the machine opens beside its output (pixels), 0 = none
    float display_w;           // the widest other display of the machine at 1:1 (pixels), 0 = none
    float main_w;              // the width of the main output in the floating layout (pixels): the middle keeps it
    const char* machine_tab;   // the window id whose tab is selected among the windows of the machine, or null
    bool guide;                // the guide is open: its tab selected among the tools
};
unsigned layout_dockspace_id(void);   // the dock space over the main window (DockSpaceOverViewport with id 0)
void layout_dock(unsigned dockspace, const DockPlan& p);
// After a machine change in the docked layout: its main output ("###gfx1") into the middle when it is elsewhere in the
// dock; the displays in the middle go where it was. The other windows and the sizes of the parts stay.
void layout_dock_main(unsigned dockspace, const char* main_id);
// After the windows of a frame: for a few frames after layout_dock its chosen tabs stay selected (a window that opens
// takes the keyboard, and with it its tab would come in front).
void layout_dock_frame(void);
void draw_ims2(UiState& ui);
void draw_diag(UiState& ui);
void draw_aps5(UiState& ui);
// Reset from the GUI (F12, Machine -> Reset, RESET on the ZDR panel, the debugger): at once, or on the APS-5 unit
// (no reset button; the firmware clears its settings after one) first the question of draw_reset_question (aps5.cpp).
void request_reset(UiState& ui);
void draw_reset_question(UiState& ui);
// File -> Load program (Intel HEX): native dialog, then a confirmation with the addresses.
void hex_load_dialog(UiState& ui);
// File -> Load binary file: native dialog, then the address; the file goes into memory as is.
void bin_load_dialog(UiState& ui);
// A file dropped on the main window (app.cpp: SDL_EVENT_DROP_FILE, the UI snapshot --ui-drop), by its suffix as the
// menus take it: a machine (.sapi) starts as File -> Start a machine from a file (the question about unsaved changes
// first), a state (.sapistate) loads, Intel HEX (.hex, .ihx, a .txt that is HEX) and a binary file (.bin) open the
// questions of File -> Load program and Load binary file, a disk image (.img, .imd, .8sd, .sp2, .sp1), an RKD-1
// cartridge (.rkd) and an RMP-1 tape (.tap) go into the first empty unit of the running machine (none: the Error window
// says so), a cassette (.wav, .mp3; .kzd for KZD-1) into the first recorder (an empty one first). Anything else, a
// switched off machine or a file dialog waiting: the status bar says so. The caller checks the modal windows.
void file_drop(UiState& ui, const std::string& path);
// File -> Save state / Load state (.sapistate, native dialog); quick slot of the machine
// (Ctrl+F2 / Ctrl+F3, work/state/).
void state_save_dialog(UiState& ui);
void state_load_dialog(UiState& ui);
void state_quick(UiState& ui, bool save);
// The text of the Error window when the state file `path` did not load (err = the reason of EmuHost): a state of a
// version of SAPIemu that this one does not read is named as such; back = StateBack of EmuHost::load_state (what
// became of the running machine).
std::string state_load_error(UiState& ui, const std::string& path, const std::string& err, int back);
void draw_rack(UiState& ui);
// Windows a machine description opens and puts on top at its start ("windows": ["minigraf"]): the key in the
// machine file, the window title (tr_id), the label in the configurator. Keyboard input stays with the screen.
struct MachineWindow {
    const char* key;
    const char* title;
    const char* en;
    const char* cs;
};
const MachineWindow* machine_windows(int* count);
// The running machine has the device of the window with the key `key` of machine_windows() (View enables its item
// only then); switched off: the machine as it ran last (Switch on builds the same description again), any device when
// none ran yet. device_missing: the sentence for a machine without it, in the UI language (the disabled item's tooltip,
// the window itself): what is missing, a machine with it in the window Choose a machine, where it goes in the
// configurator. machine_devices_seen: the devices of the running machine remembered (after each machine change).
bool machine_has_device(UiState& ui, const char* key);
std::string device_missing(UiState& ui, const char* key);
void machine_devices_seen(UiState& ui);
// Inside the window of a device: switched off (no machine), it says so; true = nothing more to draw.
bool device_window_off(UiState& ui);
bool aps5_in_machine(UiState& ui);   // the plant of APS-5 or its panel HRK-1 (aps5.cpp)
bool ims2_in_machine(UiState& ui);   // the measuring bench on the IMS-2 bus (ims2.cpp)
// File -> Open: native dialog, the chosen machine is started.
void rack_open_and_run(UiState& ui);
// File -> Open machine: start a machine file; asks first when the configurator holds
// unsaved changes.
void rack_start_machine(UiState& ui, const std::string& file);
// Build a machine file now (the machine of the last session at the program start, a start from the GUI): with the
// changes kept for it (RackEditor::kept) when its file is still the same, else as the file says.
bool rack_load_machine(UiState& ui, const std::string& file, std::string& err);
// The machine file has changes kept for the next start (the browser marks it).
bool rack_has_kept(UiState& ui, const std::string& file);
// My machines (core/machine_index.h): Save of a supplied machine writes a copy into work/machines in a release build
// (NDEBUG), into the file itself in a debug build; on = as a release build also in a debug one (UI snapshot
// --ui-save-copy).
void rack_save_copies_set(bool on);
// My machines -> Delete: the file of the copy removed, and what the settings keep for it (Recent machines, the changes
// kept for the next start); the configurator holding it keeps its changes as a machine not saved yet. False: the file
// was not removed (err says why).
bool rack_delete_copy(UiState& ui, const std::string& file, std::string& err);
// The running machine runs with the changes kept for its file (Report a bug says so).
bool rack_runs_kept(UiState& ui);
// Exit: asks first when the configurator holds unsaved changes, then sets ui.quit.
void rack_request_exit(UiState& ui);
// POWER switch: off when the machine runs, else on with the configurator's description.
void rack_power_switch(UiState& ui);
// The configurator holds changes of the running machine it does not have yet (restart applies them).
bool rack_pending(UiState& ui);
// Ink colour of a monochrome display (card parameter: jumpers S1 of AND-1V, the monitor
// phosphor of the others): shown at once on the running machine, the description in
// the configurator follows (it is saved with the machine).
void rack_set_display_ink(UiState& ui, DisplayDevice* d, const std::string& value);
// A parameter of a running card that applies at once (Machine::live_param): set it on the
// card and in the description of the configurator (TURBO and BIOS buttons, menu). False: the card refused it (logged,
// the reason into err_out when given).
bool rack_set_live_param(UiState& ui, Card* c, const char* path, const nlohmann::json& v,
                         std::string* err_out = nullptr);
// Machine -> Keyboard: another keyboard (KeyboardPreset::config) plugged into the running CPU card through its
// description, as View -> Colour: an unsaved change of the machine in the configurator (saved or kept for the next
// start), not lost at the next power cycle. rack_keyboard_cfg: the description of that keyboard for a card whose
// keyboard is `old` now (its own options stay).
bool rack_set_keyboard(UiState& ui, const char* preset, std::string& err);
nlohmann::json rack_keyboard_cfg(const nlohmann::json& old, const char* preset);
// Open a machine description in the configurator without starting it (asks first about unsaved changes there).
void rack_open_in_editor(UiState& ui, const std::string& path);
// Keyboard input for the focused terminal; returns true if consumed.
bool terminal_key(UiState& ui, int sapi_code);
bool terminal_text(UiState& ui, const char* utf8);
