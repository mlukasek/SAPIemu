#pragma once
// Minimal test helpers: CHECK prints the failing expression and counts failures.
#include <stdio.h>
#include <filesystem>
#include "core/util.h"

// Data root of the repository; battery backed RAM is not saved (user's work/ files stay untouched).
// The folders for the tests' own files exist from the start: tests run in parallel (ctest -j) and
// must not rely on another test having created them.
static inline void test_init(void)
{
    data_root_init();
    battery_files_enable(false);
    std::error_code ec;
    for (const char* d : { "work/test", "work/tests", "work/test-state" }) {
        std::string p = data_path(d);   // UTF-8
        std::filesystem::create_directories(std::filesystem::path(std::u8string(p.begin(), p.end())), ec);
    }
}

// A real tape recording of the SAPI_DOKU archive: next to the repository (../SAPI_DOKU, the current copy),
// else the older archive on H: (author's PCs). Empty when neither has it (the test part is skipped).
static inline std::string real_tape_path(const char* file)
{
    std::string p = data_path(std::string("../SAPI_DOKU/Kazetopáskové vybavení AUDIO WAV/") + file);
    if (file_exists(p)) return p;
    p = std::string("H:/SAPI-1/SAPI_DOKU_2024-01-03/Kazetopáskové vybavení AUDIO WAV/") + file;
    return file_exists(p) ? p : std::string();
}

static int g_test_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); g_test_failures++; } \
} while (0)

#define CHECK_EQ(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a != _b) { fprintf(stderr, "%s:%d: CHECK_EQ failed: %s == %s (%lld != %lld)\n", \
                            __FILE__, __LINE__, #a, #b, _a, _b); g_test_failures++; } \
} while (0)

#define TEST_RESULT() (g_test_failures == 0 ? (printf("OK\n"), 0) : (printf("FAILED (%d)\n", g_test_failures), 1))
