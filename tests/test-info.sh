#!/bin/sh

if [ -z "$SRCDIR" ]; then
  SRCDIR=..
fi

./shell-test.sh info --blocks $@ ${SRCDIR}/asdf-standard/reference_files/1.6.0/*.asdf
