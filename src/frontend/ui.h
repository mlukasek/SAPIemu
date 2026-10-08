#pragma once
// Shared state of the frontend windows.
#include <stdint.h>
#include "core/xmodem.h"
#include <mutex>
#include <string>
#include <vector>
#include "json.hpp"
#include "core/machine_map.h"
#include "core/term.h"
#include "plant/aps5_demo.h"

class Card;
class DisplayDevice;
class EmuHost;
class McpServer;
struct ImFont;
struct SDL_Window;

// Cassette dialogs of the Media window (answer from another thread).
struct TapeDialog {
    std::mutex mtx;
    int kind = 0;            // 0 none, 1 insert, 2 new tape, 3 Intel HEX into memory,
                             // 4 insert floppy, 5 new floppy, 6 binary file into memory,
                             // 7 save state, 8 load state, 9 export tape as WAV
    bool done = false;
    std::string result;      // chosen file, empty = cancelled
    std::string card;        // DSM-1 / RPD-1 card name
    int drive = 0;           // RPD-1 drive (0 = A)
    std::string last_dir;    // where the last tape came from
};

// Machine configurator (window "Sestava"): edits a copy of a machine description.
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
    std::string message;
    bool message_error = false;
    // Question "save the unsaved changes?" before the description is replaced: the
    // action waits for the answer (Save, Save as, Discard, Cancel).
    enum { PEND_NONE = 0, PEND_START, PEND_OPEN, PEND_TEMPLATE, PEND_EMPTY, PEND_FOLLOW, PEND_EXIT };
    int pending = PEND_NONE;
    std::string pending_path;      // machine file of PEND_START / PEND_OPEN / PEND_TEMPLATE
    bool ask_unsaved = false;      // open the question in the next frame
    bool save_then_continue = false;   // "Save as" dialog for the pending action

    // native file dialogs answer from another thread
    enum { DLG_NONE = 0, DLG_OPEN, DLG_SAVE, DLG_PARAM, DLG_RUN };
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
    bool show_diag = false;        // the diagnostic board on K3 (JPR-1S, JPR-1X)
    bool show_aps5 = false;        // APS-5: the front panel and the furnaces
    uint64_t aps5_gen = 0;         // machine generation the APS-5 window opened itself for
    int aps5_span = 1;             // plot: 0 = 10 min, 1 = 1 h, 2 = 4 h
    float aps5_outage_s = 60.0f;   // length of an outage from the window
    Aps5Demo aps5_demo;            // DEMO button: an operator typing the example
    uint64_t aps5_demo_gen = 0;    // the machine it types on
    bool aps5_demo_fast = false;   // it switched the emulation to full speed
    bool aps5_demo_was_max = false;   // the speed before
    bool aps5_demo_autostart = false; // --ui-aps5-demo: start it without the dialog
    bool show_rack = false;
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
    std::string tprog_card;        // DSM-1
    bool tprog_ask = false;        // open the question in the next frame
    int tprog_format = 0;          // TAPE_MIKOS (remembered while running)
    char tprog_name[12] = "";
    char tprog_addr[8] = "4400";   // load address of a binary file (hex)
    char tprog_entry[8] = "";      // start address (hex), empty = the lowest address
    std::string tprog_error;

    // keyboard routing (from the previous frame)
    bool screen_focused = false;
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
    bool disasm_follow = true;
    uint16_t disasm_addr = 0;
    char disasm_goto[8] = "";
    char bp_input[8] = "";
    int bp_kind = 0;
    uint16_t mem_addr = 0;
    char mem_goto[8] = "";
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

// Serial terminal window: the clipboard (Alt+C / Alt+V as in TeraTerm, Ctrl+Insert / Shift+Insert)
// Screenshots of a display output (frontend/shot.cpp): PNG of the exact emulated dots (or enlarged 3x with
// px_aspect = the width of a dot on a TV), named by the card and the time in work/snimky, into a chosen
// file, or to the clipboard (PNG and BMP). The result shows in the status bar.
void shot_save(UiState& ui, DisplayDevice* d, const char* name, float px_aspect);
void shot_save_as(UiState& ui, DisplayDevice* d, const char* name, float px_aspect);
void shot_copy(UiState& ui, DisplayDevice* d, float px_aspect);
void shot_poll(UiState& ui);   // the file of "Save screenshot as", once chosen
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

void draw_debugger(UiState& ui);
void draw_memory(UiState& ui);
void draw_iolog(UiState& ui);
void draw_terminal(UiState& ui);
void draw_media(UiState& ui);
void draw_printer(UiState& ui);
void draw_diag(UiState& ui);
void draw_aps5(UiState& ui);
// File -> Load program (Intel HEX): native dialog, then a confirmation with the addresses.
void hex_load_dialog(UiState& ui);
// File -> Load binary file: native dialog, then the address; the file goes into memory as is.
void bin_load_dialog(UiState& ui);
// File -> Save state / Load state (.sapistate, native dialog); quick slot of the machine
// (Ctrl+F2 / Ctrl+F3, work/state/).
void state_save_dialog(UiState& ui);
void state_load_dialog(UiState& ui);
void state_quick(UiState& ui, bool save);
void draw_rack(UiState& ui);
// File -> Open: native dialog, the chosen machine is started.
void rack_open_and_run(UiState& ui);
// File -> Open machine: start a machine file; asks first when the configurator holds
// unsaved changes.
void rack_start_machine(UiState& ui, const std::string& file);
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
// card and in the description of the configurator (TURBO and BIOS buttons, menu).
void rack_set_live_param(UiState& ui, Card* c, const char* path, const nlohmann::json& v);
// Open a machine description in the configurator without starting it.
void rack_open_in_editor(UiState& ui, const std::string& path);
// Keyboard input for the focused terminal; returns true if consumed.
bool terminal_key(UiState& ui, int sapi_code);
bool terminal_text(UiState& ui, const char* utf8);
