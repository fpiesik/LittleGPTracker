#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo/projects"
make PLATFORM=X64 -j2
cd "$repo"
test_dir=$(mktemp -d /tmp/lgpt-sample-jump.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
g++ -std=gnu++03 -Wall -Wextra -Isources tests/sample_transition_test.cpp -o "$test_dir/transition"
"$test_dir/transition"
objects=()
for object in projects/buildX64/*.o; do
    case "$object" in */LINUXMain.o) ;; *) objects+=("$object");; esac
done
g++ -std=gnu++03 -g -Wall -DPLATFORM_X64 -D_64BIT -DSDL2 -DLGPT_PLAITS -D_FEAT_MIDI_LOCK -Isources \
    tests/sample_jump_test.cpp "${objects[@]}" -lSDL2 -lasound -ljack -o "$test_dir/jump"
"$test_dir/jump"
