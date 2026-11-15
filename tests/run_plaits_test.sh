#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo/projects"
make PLATFORM=X64 -j2
cd "$repo"
task_dir=$(mktemp -d /tmp/lgpt-plaits-test.XXXXXX)
trap 'rm -rf "$task_dir"' EXIT
objects=()
for object in projects/buildX64/*.o; do
    case "$object" in */LINUXMain.o) ;; *) objects+=("$object");; esac
done
g++ -std=gnu++03 -g -Wall -DPLATFORM_X64 -D_64BIT -DSDL2 -DTEST \
    -DLGPT_PLAITS -D_FEAT_MIDI_LOCK -Isources -isystem sources/Externals/Mutable \
    tests/plaits_test.cpp "${objects[@]}" -lSDL2 -lasound -ljack \
    -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc -o "$task_dir/test"
"$task_dir/test"
g++ -std=gnu++03 -g -Wall -DPLATFORM_X64 -D_64BIT -DSDL2 \
    -DLGPT_PLAITS -D_FEAT_MIDI_LOCK -Isources \
    tests/engine_selection_test.cpp "${objects[@]}" -lSDL2 -lasound -ljack \
    -o "$task_dir/engine_selection"
"$task_dir/engine_selection"
