#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)

#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  %-50s", #name); \
    name(); \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ_INT(a, b) do { \
    int _a = (a), _b = (b); \
    if (_a != _b) { \
        printf("FAIL\n    %s:%d: %s == %d, expected %d\n", __FILE__, __LINE__, #a, _a, _b); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, eps) do { \
    float _a = (a), _b = (b); \
    if (fabsf(_a - _b) > (eps)) { \
        printf("FAIL\n    %s:%d: %s == %f, expected %f (eps=%f)\n", __FILE__, __LINE__, #a, _a, _b, (float)(eps)); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_SUITE(name) printf("\n%s:\n", name)

#define TEST_REPORT() do { \
    printf("\n--- Results: %d/%d passed", tests_passed, tests_run); \
    if (tests_failed > 0) printf(", %d FAILED", tests_failed); \
    printf(" ---\n"); \
    return tests_failed > 0 ? 1 : 0; \
} while(0)

#endif
