AM_CFLAGS = # CFLAGS applicable to all executables (products)
AM_CPPFLAGS = -I$(top_srcdir)/src # so that tests also find header files

SUBDIRS = tests

src_files = src/block.c src/event.c src/info.c src/parse.c src/yaml.c src/main.c
src_headers = src/block.h src/event.h src/info.h src/parse.h src/yaml.h

# The asdf command line tool
bin_PROGRAMS = asdf

#include .c and .h in SOURCES so that both appear in dist
asdf_SOURCES = $(src_files)
noinst_HEADERS = $(src_headers)
asdf_LDADD = $(FYAML_LIBS)

asdf_CFLAGS = $(AM_CFLAGS) $(FYAML_CFLAGS) $(CODE_COVERAGE_CFLAGS)
asdf_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)


format:
	clang-format -i $(src_files) $(src_headers)


tidy:
	clang-tidy $(src_files) -- \
	    $(DEFS) $(DEFAULT_INCLUDES) $(AM_CPPLFAGS) $(CPPFLAGS) $(asdf_CFLAGS)
