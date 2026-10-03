#pragma once
// Shared state of the frontend windows.
#include <stdint.h>
#include <mutex>
#include <string>
#include <vector>
#include "json.hpp"
#include "core/term.h"

class EmuHost;
class McpServer;
struct ImFont;
struct SDL_Window;

// Machine configurator (window "Sestava"): edits a copy of a machine description.
struct RackEditor {
    nlohmann::json cfg;            // description being edited
    nlohmann::json saved;          // as last opened / saved (unsaved changes = cfg != saved)
    std::string path;              // file (data root relative when inside it), empty = not saved yet
    uint64_t machine_gen = ~0ull;  // generation of the running machine the editor follows
    int sel = 0;                   // selected slot, 0 = the bus
    bool show_types = true;        // exact card type across the panels
    std::string message;
    bool message_error = false;

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
    bool show_rack = false;
    SDL_Window* window = nullptr;
    RackEditor rack;

    // keyboard routing (from the previous frame)
    bool screen_focused = false;
    bool terminal_focused = false;
    int terminal_channel = 0;
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
};

void draw_debugger(UiState& ui);
void draw_memory(UiState& ui);
void draw_iolog(UiState& ui);
void draw_terminal(UiState& ui);
void draw_media(UiState& ui);
void draw_rack(UiState& ui);
// File -> Open: native dialog, the chosen machine is started.
void rack_open_and_run(UiState& ui);
// Open a machine description in the configurator without starting it.
void rack_open_in_editor(UiState& ui, const std::string& path);
// Keyboard input for the focused terminal; returns true if consumed.
bool terminal_key(UiState& ui, int sapi_code);
bool terminal_text(UiState& ui, const char* utf8);
