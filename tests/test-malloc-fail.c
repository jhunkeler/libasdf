/**
 * OOM regression tests
 *
 * This test module should be compiled with
 * ``-Wl,--wrap=malloc,--wrap=calloc,--wrap=strdup,--wrap=strndup``
 * so that all allocation calls within the link unit can be intercepted.
 * The wrappers let us trigger a NULL return after a controlled number of
 * successful allocations, then verify that the library handles the OOM
 * gracefully instead of crashing.
 *
 * Note that these tests can tend to be very fragile, as they depend on
 * counting the number of allocations performed by the library down specific
 * code paths.  If, after refactoring any of the affected code, it is not
 * sensible or feasible to update any of these tests don't be afraid to delete
 * them.  But it's useful for writing regression tests for when such bugs are
 * noticed.
 */
#include <stdbool.h>
#include <stdlib.h>

#include "munit.h"
#include "util.h"

#include "event.h"
#include "parser.h"


/** malloc injection */

static int fail_after = -1;
static int call_count = 0;


static void malloc_fail_after(int n) {
    call_count = 0;
    fail_after = n;
}


static void malloc_fail_reset(void) {
    fail_after = -1;
}


static bool should_fail(void) {
    return fail_after >= 0 && call_count++ >= fail_after;
}


extern void *__real_malloc(size_t n);
extern void *__real_calloc(size_t n, size_t s);
extern char *__real_strdup(const char *s);
extern char *__real_strndup(const char *s, size_t n);


void *__wrap_malloc(size_t n) {
    if (should_fail())
        return NULL;
    return __real_malloc(n);
}


void *__wrap_calloc(size_t n, size_t s) {
    if (should_fail())
        return NULL;
    return __real_calloc(n, s);
}


char *__wrap_strdup(const char *s) {
    if (should_fail())
        return NULL;
    return __real_strdup(s);
}


char *__wrap_strndup(const char *s, size_t n) {
    if (should_fail())
        return NULL;
    return __real_strndup(s, n);
}


/** Misc test helpers */

static asdf_parser_t *make_parser(const char *fixture) {
    const char *filename = get_fixture_file_path(fixture);
    asdf_parser_t *parser = asdf_parser_create(NULL);

    if (!parser)
        munit_error("failed to create parser");

    if (asdf_parser_set_input_file(parser, filename) != 0)
        munit_errorf("failed to set parser input file '%s'", filename);

    return parser;
}


/**
 * Consume exactly ``count`` events during test setup (no injection
 * active).
 */
static void consume_events(asdf_parser_t *parser, int count) {
    for (int idx = 0; idx < count; idx++) {
        asdf_event_t *event = asdf_event_iterate(parser);
        if (!event)
            munit_error("unexpected end of events during setup");
    }
}


/** Regression tests */

/**
 * ``parse_asdf_version`` -- ``malloc(sizeof(asdf_version_t))`` without NULL
 * check.
 *
 * First `asdf_event_iterate`: freelist is empty so `asdf_parse_event_alloc`
 * calls ``malloc`` (call 0).  fail_after=1 lets that succeed then fails
 * (call 1), which is the version-struct malloc inside ``parse_asdf_version``.
 */
MU_TEST(oom_parse_asdf_version) {
    asdf_parser_t *parser = make_parser("parse-minimal.asdf");

    malloc_fail_after(1);
    asdf_event_t *event = asdf_event_iterate(parser);
    malloc_fail_reset();

    assert_null(event);
    assert_true(asdf_parser_has_error(parser));

    asdf_parser_destroy(parser);
    return MUNIT_OK;
}


/**
 * ``parse_standard_version``--``malloc(sizeof(asdf_version_t))`` without NULL
 * check.
 *
 * Consume 1 event first (``ASDF_ASDF_VERSION_EVENT``).  On the second
 * iteration the previous event is recycled back to the freelist, so
 * ``asdf_parse_event_alloc`` takes it without calling malloc.
 * fail_after=0 therefore fails the first malloc call, which is the
 * version-struct allocation in ``parse_standard_version``.
 */
MU_TEST(oom_parse_standard_version) {
    asdf_parser_t *parser = make_parser("parse-minimal.asdf");

    consume_events(parser, 1);

    malloc_fail_after(0);
    asdf_event_t *event = asdf_event_iterate(parser);
    malloc_fail_reset();

    assert_null(event);
    assert_true(asdf_parser_has_error(parser));

    asdf_parser_destroy(parser);
    return MUNIT_OK;
}


/**
 * ``parse_comment`` -- ``strndup`` without NULL check.
 *
 * Consume 2 events first (``ASDF_ASDF_VERSION_EVENT``,
 * ``ASDF_STANDARD_VERSION_EVENT``).  The fixture has an extra comment
 * line, so the third iterate hits ``parse_comment``.  fail_after=0
 * fails call 0 = the strndup inside ``parse_comment``.
 */
MU_TEST(oom_parse_comment) {
    asdf_parser_t *parser = make_parser("parse-minimal-extra-comment.asdf");

    consume_events(parser, 2);

    malloc_fail_after(0);
    asdf_event_t *event = asdf_event_iterate(parser);
    malloc_fail_reset();

    assert_null(event);
    assert_true(asdf_parser_has_error(parser));

    asdf_parser_destroy(parser);
    return MUNIT_OK;
}


/**
 * ``emit_tree_start_event`` -- ``calloc(1, sizeof(asdf_tree_info_t))``
 * without NULL check.
 *
 * Consume 3 events first (``ASDF_ASDF_VERSION_EVENT``,
 * ``ASDF_STANDARD_VERSION_EVENT``, ``ASDF_BLOCK_INDEX_EVENT``) from a
 * fixture that has a full tree.  The fourth iterate enters
 * ``emit_tree_start_event``.  fail_after=0 fails call 0 = the calloc.
 */
MU_TEST(oom_emit_tree_start) {
    asdf_parser_t *parser = make_parser("255-padding-after-header.asdf");

    consume_events(parser, 3);

    malloc_fail_after(0);
    asdf_event_t *event = asdf_event_iterate(parser);
    malloc_fail_reset();

    assert_null(event);
    assert_true(asdf_parser_has_error(parser));

    asdf_parser_destroy(parser);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    malloc_fail,
    MU_RUN_TEST(oom_parse_asdf_version),
    MU_RUN_TEST(oom_parse_standard_version),
    MU_RUN_TEST(oom_parse_comment),
    MU_RUN_TEST(oom_emit_tree_start)
);


MU_RUN_SUITE(malloc_fail);
