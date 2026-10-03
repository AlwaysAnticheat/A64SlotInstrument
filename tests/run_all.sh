#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
./tests/test_host.sh
./tests/test_aarch64_entry.sh
./tests/test_pic_relocation.sh
python3 ./tests/test_stub_words.py
python3 ./tests/test_register_coverage.py
