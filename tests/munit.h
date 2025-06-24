/**
 * Thin wrapper around munit to make some common tasks quicker
 */
#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit/munit.h"


#if defined(__GNUC__) || defined(__clang__)
#define UNUSED(x) x __attribute__((unused))
#else
#define UNUSED(x) (void)(x)
#endif


#define MU_TEST(name) MunitResult name(UNUSED(const MunitParameter params[]), UNUSED(void *fixture))


#define __MU_RUN_TEST_DISPATCH(_1, _2, NAME, ...) NAME
#define __MU_RUN_TEST_2(name, params) \
    { "/" #name, name, NULL, NULL, MUNIT_TEST_OPTION_NONE, (params) }
#define __MU_RUN_TEST_1(name) __MU_RUN_TEST_2(name, NULL)

// Macro to declare tests to run within a test suite
#define MU_RUN_TEST(...) \
    __MU_RUN_TEST_DISPATCH( \
        __VA_ARGS__, \
        __MU_RUN_TEST_2, \
        __MU_RUN_TEST_1, \
    )(__VA_ARGS__)


// Helper to create a null-terminated array of tests from varargs
#define MU_TESTS(...) \
    static MunitTest __tests[] = { __VA_ARGS__, { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }}

// Macro to declare a test suite with a given name and tests inside
#define MU_TEST_SUITE(suite, ...) \
    MU_TESTS(__VA_ARGS__); \
    static const MunitSuite suite = { \
        "/" #suite, \
        __tests, \
        NULL, \
        1, \
        MUNIT_SUITE_OPTION_NONE \
    };


#define MU_RUN_SUITE(suite) \
    int main(int argc, char *argv[]) { \
        return munit_suite_main(&suite, NULL, argc, argv); \
    }
