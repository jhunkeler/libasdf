AM_CFLAGS = # CFLAGS applicable to all executables (products)
AM_CPPFLAGS = -I$(top_srcdir)/src # so that tests also find header files
ACLOCAL_AMFLAGS = -Im4

src_files = src/block.c src/event.c src/info.c src/parse.c src/yaml.c
src_headers = src/block.h src/event.h src/info.h src/parse.h src/yaml.h

noinst_HEADERS = $(src_headers)

# libasdf library
#include .c and .h in SOURCES so that both appear in dist
lib_LTLIBRARIES = libasdf.la
libasdf_la_SOURCES = $(src_files)
libasdf_la_CFLAGS = $(AM_CFLAGS) $(FYAML_CFLAGS) $(CODE_COVERAGE_CFLAGS)
libasdf_la_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)
libasdf_la_LIBADD = $(FYAML_LIBS)

# The asdf command line tool
if ASDF_BUILD_TOOL
bin_PROGRAMS = asdf
asdf_SOURCES = src/main.c
asdf_CFLAGS = $(AM_CFLAGS) $(FYAML_CFLAGS)
asdf_LDADD = libasdf.la $(ARGP_LIB)
endif # ASDF_BUILD_TOOL


# Misc make targets
format:
	clang-format -i $(src_files) $(src_headers)


tidy:
	clang-tidy $(src_files) -- \
	    $(DEFS) $(DEFAULT_INCLUDES) $(AM_CPPLFAGS) $(CPPFLAGS) $(asdf_CFLAGS)


SUBDIRS = tests
