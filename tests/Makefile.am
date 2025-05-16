# Tests
# 'check' comes from 'make check'
AM_CPPFLAGS = -I$(top_srcdir)/src

asdf_standard_submodule = $(top_srcdir)/asdf-standard/.git

$(asdf_standard_submodule):
	echo "Initializing asdf-standard submodule with reference files..."
	git submodule update --init --recursive


@VALGRIND_CHECK_RULES@
@CODE_COVERAGE_RULES@

# See all code_coverage_* options in the Makefile or m4/ax_code_coverage.m4
CODE_COVERAGE_IGNORE_PATTERN = tests/*

TESTS_ENVIRONMENT = SRCDIR=$(top_srcdir)
TESTS =

TOOL_TESTS = test-info.sh

if ASDF_BUILD_TOOL
TESTS += $(TOOL_TESTS)
else
$(warning Skipping running asdf command-line tests; make sure to run ./configure --enable-tool; make to build the tool)
endif


AUTOMAKE_OPTIONS = parallel-tests
EXTRA_DIST = minunit.h

# Bit of a hack to make syncing the submodule a prerequisite to make check
check-am: $(asdf_standard_submodule) $(top_srcdir)/asdf
