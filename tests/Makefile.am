# Tests
# 'check' comes from 'make check'
AM_CPPFLAGS = -I$(top_srcdir)/src

asdf_standard_dir = $(top_srcdir)/asdf-standard
asdf_standard_submodule = $(asdf_standard_dir)/.git
munit_submodule = $(srcdir)/tests/munit.git

$(asdf_standard_submodule) $(munit_submodule):
	echo "Initializing submodules..."
	git submodule update --init --recursive
	git config submodule.tests/munit.ignore untracked


@VALGRIND_CHECK_RULES@

check_PROGRAMS = test-event.unit

TESTS_ENVIRONMENT = SRCDIR=$(top_srcdir)
TESTS = $(check_PROGRAMS)

# Unit tests
test_event_unit_SOURCES = test-event.c util.c munit/munit.c
test_event_unit_CPPFLAGS = \
    $(AM_CPPFLAGS) -DREFERENCE_FILES_DIR=\"$(asdf_standard_dir)/reference_files\"
test_event_unit_CFLAGS = $(FYAML_CFLAGS) $(CODE_COVERAGE_CFLAGS)
test_event_unit_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)
test_event_unit_LDADD = $(top_srcdir)/libasdf.la

# Shell tests
TOOL_TESTS = test-info.sh

if ASDF_BUILD_TOOL
TESTS += $(TOOL_TESTS)
else
$(warning Skipping running asdf command-line tests; make sure to run ./configure --enable-tool; make to build the tool)
endif


AUTOMAKE_OPTIONS = parallel-tests
EXTRA_DIST = minunit.h

# Bit of a hack to make syncing the submodule a prerequisite to make check
check-am: $(asdf_standard_submodule)
