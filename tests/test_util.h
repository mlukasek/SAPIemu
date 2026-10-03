#pragma once
// Minimal test helpers: CHECK prints the failing expression and counts failures.
#include <stdio.h>
#include "core/util.h"

// Data root of the repository; battery backed RAM is not saved (user's work/ files stay untouched).
static inline void test_init(void)
{
    data_root_init();
    battery_files_enable(false);
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
