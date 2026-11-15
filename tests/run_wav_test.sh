#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo/projects"
make PLATFORM=X64 -j2
cd "$repo"
task_dir=$(mktemp -d /tmp/lgpt-wav-test.XXXXXX)
trap 'rm -rf "$task_dir"' EXIT
python3 - "$task_dir" <<'PY'
import struct, sys
from pathlib import Path
p=Path(sys.argv[1])
def chunk(tag,data): return tag+struct.pack('<I',len(data))+data+b'\0'*(len(data)%2)
def fmt(ch=1,rate=24000,align=2,bits=16): return chunk(b'fmt ',struct.pack('<HHIIHH',1,ch,rate,rate*align,align,bits))
def write(name,body): (p/(name+'.wav')).write_bytes(b'RIFF'+struct.pack('<I',len(body)+4)+b'WAVE'+body)
write('stereo',chunk(b'LIST',b'x')+fmt(2,24000,2,8)+chunk(b'data',bytes([0,255,128,64,192,129])))
write('ramp',fmt()+chunk(b'data',struct.pack('<hhh',0,1000,2000)))
for rate in (44100, 96000):
    write(str(rate),fmt(rate=rate)+chunk(b'data',struct.pack('<100h',*range(0,10000,100))))
for name,f in [('channels',fmt(3)),('rate',fmt(rate=0)),('align',fmt(align=1))]: write(name,f+chunk(b'data',b'\0\0'))
write('missing',fmt());write('empty',fmt()+chunk(b'data',b''))
write('oversize',fmt()+b'data'+struct.pack('<I',0xffffffff))
(p/'truncated.wav').write_bytes((p/'ramp.wav').read_bytes()[:-1])
PY
objects=()
for object in projects/buildX64/*.o; do
    case "$object" in */LINUXMain.o) ;; *) objects+=("$object");; esac
done
g++ -std=gnu++03 -g -Wall -DPLATFORM_X64 -D_64BIT -DSDL2 -DLGPT_PLAITS -Isources \
    tests/wav_test.cpp "${objects[@]}" -lSDL2 -lasound -ljack -o "$task_dir/test"
"$task_dir/test" "$task_dir"
