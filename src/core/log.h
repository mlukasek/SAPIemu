#pragma once
// Simple thread-safe logger: prints to stderr and keeps a ring buffer of recent
// messages for the GUI log window and the MCP server.
#include <string>
#include <vector>

enum LogLevel { LOG_DEBUG = 0, LOG_INFO, LOG_WARN, LOG_ERROR };

void log_msg(LogLevel level, const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
void log_set_level(LogLevel level);        // minimum level printed to stderr
void log_set_stderr(bool enabled);
// Also write the messages (info and up) into a file, rewritten at each start; the messages logged
// before are written first. Empty path: no file.
bool log_set_file(const std::string& utf8_path);
// Copy the last `max` messages (oldest first). Each starts with its level: "error: ", "warn: ", "info: ".
void log_recent(std::vector<std::string>& out, int max);
// Errors logged since the start (the GUI status bar shows the ones not seen in the Log window yet).
int log_errors(void);

// A message held back by log_hold.
struct LogHeld {
    LogLevel level;
    std::string text;
};
// The messages of this thread go into `into` instead of the log until log_hold(nullptr) (EmuHost builds a new machine
// as a trial first: its messages are logged once, not twice); log_replay logs them then.
void log_hold(std::vector<LogHeld>* into);
void log_replay(const std::vector<LogHeld>& held);

#define LOGD(...) log_msg(LOG_DEBUG, __VA_ARGS__)
#define LOGI(...) log_msg(LOG_INFO, __VA_ARGS__)
#define LOGW(...) log_msg(LOG_WARN, __VA_ARGS__)
#define LOGE(...) log_msg(LOG_ERROR, __VA_ARGS__)
