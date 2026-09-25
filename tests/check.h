#ifndef __TESTS_CHECK_H
#define __TESTS_CHECK_H

// Minimal test harness: TEST(name) { CHECK(...); } registers a test, and
// runTests() runs them all, returning a process exit status.

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace check {

struct Test {
    const char *name;
    std::function<void()> fn;
};

inline std::vector<Test> &registry () {
    static std::vector<Test> tests;
    return tests;
}

inline int &failures () {
    static int n = 0;
    return n;
}

struct Register {
    Register (const char *name, std::function<void()> fn) {
        registry().push_back({name, fn});
    }
};

inline int runTests () {
    int failedTests = 0;
    for (const Test &t : registry()) {
        int before = failures();
        t.fn();
        bool ok = failures() == before;
        if (!ok) ++failedTests;
        printf("%-50s %s\n", t.name, ok ? "ok" : "FAILED");
    }
    printf("%zu tests, %d failed\n", registry().size(), failedTests);
    return failedTests ? 1 : 0;
}

}

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2(a, b)

#define TEST(name) \
    static void name (); \
    static check::Register CHECK_CAT(reg_, name)(#name, name); \
    static void name ()

#define CHECK(cond) \
    do { if (!(cond)) { \
        fprintf(stderr, "%s:%d: CHECK(%s) failed\n", \
                __FILE__, __LINE__, #cond); \
        ++check::failures(); } } while (0)

#define CHECK_NEAR(a, b, tol) \
    do { double va_ = (a), vb_ = (b); \
        if (!(std::fabs(va_ - vb_) <= (tol))) { \
        fprintf(stderr, "%s:%d: CHECK_NEAR(%s, %s) failed: %.17g vs %.17g\n", \
                __FILE__, __LINE__, #a, #b, va_, vb_); \
        ++check::failures(); } } while (0)

#endif
