#!/usr/bin/env python3
"""
Extract all C code blocks from ReST files and write them to test files.

Code blocks to test are always introduced by the ``.. code:: c`` block
directive.  Blocks to be extracted must include the ``:name:`` option with
a name beginning with ``test-``.
"""

from pathlib import Path
import re
import argparse

parser = argparse.ArgumentParser(description='Extract C code blocks from ReST')
parser.add_argument(
    'rst_files', nargs='+', type=Path,
    help='ReST files from which to extract code examples'
)
parser.add_argument(
    '--out-dir', type=Path, default=Path('doc_examples'),
    help='Directory to write extracted .c files'
)
args = parser.parse_args()

args.out_dir.mkdir(parents=True, exist_ok=True)

# Match a .. code:: c block, optionally with :name:
code_block_re = re.compile(
    r'^\s*\.\. code:: c\s*\n'
    r'((?:\s*:\w+:.*\n)*)'  # optional directives like :name:
    r'((?:\s{3,}.*\n)+)',   # indented code block
    re.MULTILINE
)

for rst_file in args.rst_files:
    if not rst_file.is_file():
        continue

    content = rst_file.read_text()

    for match in code_block_re.finditer(content):
        directives, code_lines = match.groups()

        # Look for :name: directive starting with test_
        name_match = re.search(r':name:\s*(test-\S+)', directives)
        if not name_match:
            continue

        filename = f'{name_match.group(1)}.c'

        # Remove leading indentation (3 or more spaces)
        code = '\n'.join(
            line[3:] if line.startswith('   ') else line
            for line in code_lines.splitlines()
        )

        out_file = args.out_dir / filename
        out_file.write_text(code)
        print(f'Wrote {out_file}')
