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
// Copy the last `max` messages (oldest first).
void log_recent(std::vector<std::string>& out, int max);

#define LOGD(...) log_msg(LOG_DEBUG, __VA_ARGS__)
#define LOGI(...) log_msg(LOG_INFO, __VA_ARGS__)
#define LOGW(...) log_msg(LOG_WARN, __VA_ARGS__)
#define LOGE(...) log_msg(LOG_ERROR, __VA_ARGS__)
