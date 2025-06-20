#!/bin/sh

if [ -z "${srcdir}" ]; then
  srcdir="."
fi

if [ -z "${top_srcdir}" ]; then
  top_srcdir=".."
fi

"${srcdir}"/shell-test.sh events --verbose $@ \
  "${top_srcdir}/asdf-standard/reference_files/1.6.0/basic.asdf" \
  "${top_srcdir}/asdf-standard/reference_files/1.6.0/complex.asdf" \
  "${top_srcdir}/asdf-standard/reference_files/1.6.0/compressed.asdf" \
  "${top_srcdir}/asdf-standard/reference_files/1.6.0/int.asdf"

