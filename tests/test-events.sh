#!/bin/sh

"${srcdir}"/shell-test.sh events --verbose $@ \
  "${top_srcdir}/asdf-standard/reference_files/1.6.0/basic.asdf" \
  "${top_srcdir}/asdf-standard/reference_files/1.6.0/complex.asdf" \
  "${top_srcdir}/asdf-standard/reference_files/1.6.0/compressed.asdf" \
  "${top_srcdir}/asdf-standard/reference_files/1.6.0/int.asdf"

