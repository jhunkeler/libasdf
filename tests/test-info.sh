#!/bin/sh
set -e

UPDATE=0
if [ "$1" = "--update" ]; then
  UPDATE=1
  echo "📝 Updating fixture outputs..."
fi

if [ -z "$SRCDIR" ]; then
  SRCDIR=..
fi

fail=0

for input in ${SRCDIR}/asdf-standard/reference_files/1.6.0/*.asdf; do
  base=$(basename "$input" .asdf)
  expected="fixtures/info/${base}.info.txt"
  actual="tmp/${base}.info.out.txt"

  mkdir -p tmp

  # Generate actual output
  ${SRCDIR}/asdf info --blocks "$input" > "$actual"

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
