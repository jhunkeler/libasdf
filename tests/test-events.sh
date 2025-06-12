#!/bin/sh

if [ -z "$SRCDIR" ]; then
  SRCDIR=..
fi

./shell-test.sh events --verbose $@ \
  ${SRCDIR}/asdf-standard/reference_files/1.6.0/basic.asdf \
  ${SRCDIR}/asdf-standard/reference_files/1.6.0/complex.asdf \
  ${SRCDIR}/asdf-standard/reference_files/1.6.0/compressed.asdf \
  ${SRCDIR}/asdf-standard/reference_files/1.6.0/int.asdf

