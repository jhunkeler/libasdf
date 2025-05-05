# Tests
# 'check' comes from 'make check'
AM_CPPFLAGS = -I$(top_srcdir)/src

@VALGRIND_CHECK_RULES@
@CODE_COVERAGE_RULES@

# See all code_coverage_* options in the Makefile or m4/ax_code_coverage.m4
CODE_COVERAGE_IGNORE_PATTERN = tests/*

check_PROGRAMS = test_info.unit
TESTS = $(check_PROGRAMS)

test_info_unit_SOURCES = info.c ../src/info.c ../src/info.h
test_info_unit_CPPFLAGS = $(AM_CPPFLAGS)
test_info_unit_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)
test_info_unit_CFLAGS = $(CODE_COVERAGE_CFLAGS)


AUTOMAKE_OPTIONS = parallel-tests
EXTRA_DIST = minunit.h
