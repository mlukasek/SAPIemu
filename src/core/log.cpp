#include "core/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <mutex>

static const int LOG_RING_SIZE = 1000;

static std::mutex g_log_mutex;
static std::vector<std::string> g_ring;
static int g_ring_next = 0;
static LogLevel g_min_level = LOG_INFO;
static bool g_stderr = true;

static const char* level_name(LogLevel l)
{
    switch (l) {
    case LOG_DEBUG: return "debug";
    case LOG_INFO:  return "info";
    case LOG_WARN:  return "warn";
    default:        return "error";
    }
}

void log_msg(LogLevel level, const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_stderr && level >= g_min_level)
        fprintf(stderr, "[%s] %s\n", level_name(level), buf);
    if (level == LOG_DEBUG)
        return;
    std::string line = std::string(level_name(level)) + ": " + buf;
    if ((int)g_ring.size() < LOG_RING_SIZE) {
        g_ring.push_back(line);
    } else {
        g_ring[g_ring_next] = line;
        g_ring_next = (g_ring_next + 1) % LOG_RING_SIZE;
    }
}

void log_set_level(LogLevel level) { g_min_level = level; }
void log_set_stderr(bool enabled) { g_stderr = enabled; }

void log_recent(std::vector<std::string>& out, int max)
{
    std::lock_guard<std::mutex> lock(g_log_mutex);
    out.clear();
    int n = (int)g_ring.size();
    int start = (n < LOG_RING_SIZE) ? 0 : g_ring_next;
    int skip = n > max ? n - max : 0;
    for (int i = skip; i < n; i++)
        out.push_back(g_ring[(start + i) % n]);
}
