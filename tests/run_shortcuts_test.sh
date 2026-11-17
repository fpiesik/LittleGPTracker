#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo/projects"
make PLATFORM=X64 -j2
cd "$repo"
test_dir=$(mktemp -d /tmp/lgpt-shortcuts-test.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
objects=()
for object in projects/buildX64/*.o; do
    case "$object" in
        */LINUXMain.o) ;;
        *) objects+=("$object") ;;
    esac
done
for test_name in shortcuts mixer_shortcuts fx_shortcuts table_shortcuts chain_shortcuts; do
    link_flags=()
    if [[ $test_name == fx_shortcuts || $test_name == table_shortcuts || $test_name == chain_shortcuts ]]; then
        link_flags+=(-Wl,--wrap=_ZN6Player13OnStartButtonE8PlayModejbh)
    fi
    if [[ $test_name == table_shortcuts ]]; then
        link_flags+=(-Wl,--wrap=_ZN4View7DoModalEP9ModalViewPFvRS_RS0_E
                    -Wl,--wrap=_ZN6Player14StartStreamingERK4Path,--wrap=_ZN6Player13StopStreamingEv
                    -Wl,--wrap=_ZN10SamplePool12ImportSampleER4Path)
    fi
    g++ -std=gnu++03 -g -Wall -DPLATFORM_X64 -D_64BIT -DSDL2 -DLGPT_PLAITS \
        -D_FEAT_MIDI_LOCK -Isources "tests/${test_name}_test.cpp" "${objects[@]}" \
        -lSDL2 -lasound -ljack "${link_flags[@]}" -o "$test_dir/${test_name}_test"
    if [[ $test_name == table_shortcuts ]]; then
        mkdir -p "$test_dir/picker/lgpt_one" "$test_dir/picker/lgpt_two"
        mkdir -p "$test_dir/samples/sub"
        touch "$test_dir/samples/sub/a.wav" "$test_dir/samples/sub/b.wav" "$test_dir/samples/root.wav"
        "$test_dir/${test_name}_test" "$test_dir/picker" "$test_dir/samples"
    else
        "$test_dir/${test_name}_test"
    fi
done
