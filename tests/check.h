// The simplest checks for tests without third-party libraries.
//
// CHECK(condition) - if the condition is false, prints the file, line and condition text
// and remembers the failure; the test continues to show all failures at once.
// At the end of main the test returns checkResult(): 0 - all passed, 1 - there were failures.
#pragma once

#include <cstdio>

namespace check {

// How many checks failed. inline - one variable for the whole program.
inline int failures = 0;

inline void report(bool ok, const char* expression, const char* file, int line) {
    if (!ok) {
        std::printf("FAILED: %s (%s:%d)\n", expression, file, line);
        failures++;
    }
}

}  // namespace check

#define CHECK(condition) check::report((condition), #condition, __FILE__, __LINE__)

// The test result for return in main.
inline int checkResult() {
    if (check::failures == 0) {
        std::printf("All checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", check::failures);
    return 1;
}
