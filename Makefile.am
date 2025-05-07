AM_CFLAGS = # CFLAGS applicable to all executables (products)
AM_CPPFLAGS = -I$(top_srcdir)/src # so that tests also find header files

SUBDIRS = tests

# The asdf command line tool
bin_PROGRAMS = asdf

#include .c and .h in SOURCES so that both appear in dist
asdf_SOURCES = src/main.c src/info.c src/info.h
asdf_LDADD = $(FYAML_LIBS)

asdf_CFLAGS = $(AM_CFLAGS) $(CODE_COVERAGE_CFLAGS)
asdf_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)
