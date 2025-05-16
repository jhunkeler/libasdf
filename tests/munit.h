/**
 * Thin wrapper around munit to make some common tasks quicker
 */
#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit/munit.h"


#define MU_TEST(name) MunitResult name(const MunitParameter params[], void *fixture)


// Macro to declare tests to run within a test suite
#define MU_RUN_TEST(name) \
    { "/" #name, name, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }


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
