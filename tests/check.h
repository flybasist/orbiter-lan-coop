// Простейшие проверки для тестов без сторонних библиотек.
//
// CHECK(условие) - если условие ложно, печатает файл, строку и текст условия
// и запоминает ошибку; тест продолжается, чтобы показать все ошибки сразу.
// В конце main тест возвращает checkResult(): 0 - всё прошло, 1 - были ошибки.
#pragma once

#include <cstdio>

namespace check {

// Сколько проверок не прошло. inline - одна переменная на всю программу.
inline int failures = 0;

inline void report(bool ok, const char* expression, const char* file, int line) {
    if (!ok) {
        std::printf("FAILED: %s (%s:%d)\n", expression, file, line);
        failures++;
    }
}

}  // namespace check

#define CHECK(condition) check::report((condition), #condition, __FILE__, __LINE__)

// Итог теста для return в main.
inline int checkResult() {
    if (check::failures == 0) {
        std::printf("All checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", check::failures);
    return 1;
}
