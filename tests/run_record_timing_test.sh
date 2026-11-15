#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo/projects"
make PLATFORM=X64 -j2
cd "$repo"
test_dir=$(mktemp -d /tmp/lgpt-record-test.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
objects=()
for object in projects/buildX64/*.o; do
    case "$object" in
        */LINUXMain.o|*/SDLAudioDriver.o) ;;
        *) objects+=("$object") ;;
    esac
done
flags=()
if [[ ${SANITIZE:-0} == 1 ]]; then
    flags+=(-fsanitize=address,undefined)
fi
g++ -std=gnu++03 -g -Wall -DPLATFORM_X64 -D_64BIT -DSDL2 -DLGPT_PLAITS \
    -D_FEAT_MIDI_LOCK -Isources "${flags[@]}" tests/record_timing_test.cpp \
    "${objects[@]}" -lSDL2 -lasound -ljack \
    -Wl,--wrap=SDL_GetPerformanceCounter,--wrap=SDL_GetPerformanceFrequency,--wrap=SDL_PauseAudio,--wrap=SDL_PauseAudioDevice \
    -o "$test_dir/record_timing_test"
# Existing application singletons live until process exit. This harness checks
# address/undefined behaviour in the driver, not application-wide leak cleanup.
ASAN_OPTIONS=detect_leaks=0 "$test_dir/record_timing_test"
