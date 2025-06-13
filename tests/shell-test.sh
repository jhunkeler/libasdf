#!/bin/sh

# Simple test driver for tests of the `asdf` shell command
#
# This is meant to be wrapped by test scripts for individual `asdf` sub-commands; for example:
#
# ./shell-test.sh info <list-of-input-reference-files>
#
# Running `./shell-test.sh info --upgrade <list-of-reference-files>` updates the reference files.
set -e

UPDATE=0
SUBCOMMAND=""
EXTRA_ARGS=""

# Parse arguments: allow --update before or after the subcommand
for arg in "$@"; do
  case "$arg" in
    --update)
      UPDATE=1
      shift
      ;;
    -*)
      EXTRA_ARGS="$EXTRA_ARGS $arg"
      shift
      ;;
    *)
      if [ -z "$SUBCOMMAND" ]; then
        SUBCOMMAND="$arg"
        shift
      elif [ ! -f ${arg} ]; then
        echo >&2 "error: unexpected argument: $arg"
        echo >&2 "Usage: $0 [--update] <subcommand>"
        exit 1
      fi
      ;;
  esac
done


if [ -z "$SRCDIR" ]; then
  SRCDIR=..
fi

fail=0

fixtures_dir="fixtures/${SUBCOMMAND}"
mkdir -p "${fixtures_dir}"

for input in $@; do
  base=$(basename "$input" .asdf)
  expected="${fixtures_dir}/${base}.${SUBCOMMAND}.txt"
  actual="tmp/${base}.${SUBCOMMAND}.out.txt"

  mkdir -p tmp

  # Generate actual output
  ${SRCDIR}/asdf ${SUBCOMMAND} ${EXTRA_ARGS} "$input" 2>&1 > "$actual" || true

  if [ "$UPDATE" -eq 1 ]; then
    cp "$actual" "$expected"
    echo "🔄 Updated: $expected"
  else
    if ! diff -u "$expected" "$actual"; then
      echo "❌ Test failed: $base"
      fail=1
    else
      echo "✅ Test passed: $base"
    fi
  fi
done

exit $fail
