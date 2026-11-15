# Plaits synthesis in the desktop main build

The normal Linux X64 build includes native synthesis through LGPT's existing
instrument/mixer interface, together with the shared Clouds reverb. It is a first integration, not a finished portable
instrument feature. Existing sample/MIDI instrument IDs and project fields are
unchanged. No live device or listening test has been performed by the assistant.

## Build and try

Use the existing X64 dependencies in `projects/README.md`, then:

```sh
cd projects
make PLATFORM=X64 -j2
./lgpt.x64
```

Objects go to `projects/buildX64` and the executable is `projects/lgpt.x64`.
No `PLAITS=1` flag or separate executable is needed. The former experimental
objects are rebuilt with the main build's flags automatically. Other platform
builds include Clouds but still need Plaits toolchain integration and validation.

Open the instrument view for a sample slot (`00`–`7F`). The first field is
`engine: Sampler`. Use A + Right/Up to select `Plaits`, or A + Left/Down to
return to `Sampler`. The view immediately shows the selected engine's controls;
the next tracker note uses that engine. MIDI slots retain their MIDI controls.
The selector is available in the normal X64 build.

Changing engines replaces the selected instrument's parameters with defaults;
returning to Sampler starts with an empty sample assignment. Sample files are
retained. Notes and table automation using the old engine are stopped; other
channels continue. Cached instrument references follow the replacement, including
notes with no explicit instrument number. Switching is handled on the UI thread
under the existing event/mixer lock. Saving persists the selected engine and its
parameters through the existing instrument XML format.

Alternatively, for a manually edited project, this is a PLTS patch entry inside
`INSTRUMENTBANK` in `lgptsav.dat`:

```xml
<INSTRUMENT ID="00" TYPE="PLTS">
    <PARAM NAME="MODEL" VALUE="8" />
    <PARAM NAME="HARMONICS" VALUE="128" />
    <PARAM NAME="TIMBRE" VALUE="128" />
    <PARAM NAME="MORPH" VALUE="128" />
    <PARAM NAME="DECAY" VALUE="160" />
</INSTRUMENT>
```

Load it with `lgpt.x64`, enter notes with instrument `00` in a phrase,
and play. Normal tracker note stops/KILL, channel volume, filters, mute and mix
bus routing apply. `VOLM 00xx` changes the synth gain; the next note restores
the default gain. The instrument screen edits the five saved patch parameters.
XML numbers are decimal; the screen displays the four continuous controls as
hexadecimal. They range from 0 to 255, mapped to 0–1. Invalid DSP parameter
values are clamped.

MODEL follows the pinned upstream registry (decimal):

| Model | Synthesis |
| --- | --- |
| 0 | Virtual analog VCF |
| 1 | Phase distortion |
| 2–4 | Six-operator FM banks |
| 5 | Wave terrain |
| 6 | String machine |
| 7 | Chiptune |
| 8 | Virtual analog (default) |
| 9 | Waveshaping |
| 10 | FM |
| 11 | Grain |
| 12 | Additive |
| 13 | Wavetable |
| 14 | Chord |
| 15 | Speech |
| 16 | Swarm |
| 17 | Noise |
| 18 | Particle |
| 19 | String |
| 20 | Modal |
| 21 | Bass drum |
| 22 | Snare drum |
| 23 | Hi-hat |

PLTS projects require a Plaits-enabled build (now the normal X64 build).
Targets without Plaits log and skip unsupported types; saving there loses
those patches. Older LGPT builds do not understand PLTS. Existing sample/MIDI
projects need no conversion. Loading an unknown type now avoids the old loader's
delete-without-replacement path.

## Integration points

- `I_Instrument`: appends `IT_PLAITS` without renumbering existing types.
- `InstrumentBank`: constructs, saves, loads and clones PLTS definitions;
  excludes them from sample-only casts/free-slot discovery. `SetType` replaces
  a sample/PLTS patch and redirects playback references before deleting it.
- `InstrumentView`: owns the engine selector and rebuilds the appropriate
  controls after a change; detaches observers and fields before replacement.
- `PlayerMixer::ReplaceInstrument`: stops affected voices, updates last-instrument
  caches, and detaches old table automation without stopping unrelated channels.
- `PlayerMixer::Init`: initializes voices after the output driver negotiates its
  sample rate and before the audio thread starts.
- `PlaitsInstrument`: caches parameter pointers, owns small patch definitions,
  and assigns a shared pool of eight voices by tracker channel. Sharing a patch
  across channels does not share its oscillator/envelope state.
- `PlaitsEngine`: initializes upstream `plaits::Voice` in zeroed, aligned storage
  with a fixed 16 KiB arena; handles notes, pitch, model/parameter changes,
  gain and streaming output.
- Existing `Player` → `PlayerMixer` → `PlayerChannel` → `MixBus`/`AudioMixer`
  routing needs no new audio backend. OUT is duplicated to stereo; AUX is not
  treated as the right channel. Output uses LGPT's fixed-point 16-bit PCM scale
  with 6 dB headroom per voice, before channel/mixer gain.
- The shared [Clouds reverb](clouds.md) taps both sample and synth channels
  after their HPF/LPF and volume controls. SEND and the E page use the same
  mixer and project; the reverb tail continues after a Plaits note is cut.
- `Makefile.plaits`: is included by the main X64 Makefile and compiles the unmodified
  upstream `.cc` files in portable `TEST` mode. Provenance and MIT notices are in
  `sources/Externals/Mutable`.

## Timing, memory and limits

Synthesis runs at 48 kHz in upstream's 12-frame blocks. A streaming linear
resampler keeps fractional phase between arbitrary tracker render slices.
Pitch compensates for upstream's 47872.34 Hz hardware-clock correction. Linear
conversion is a proof-of-concept quality choice, with no bandlimited anti-alias
filter when downsampling. Model/control updates take effect at the next internal
block. Triggering retains upstream's five-block delay, plus the adapter's low
block before a new trigger (about 1.5 ms total at 48 kHz).

Every tracker note sends a trigger pulse. DECAY controls upstream's LPG behavior;
percussion and other internally enveloped models retain their own envelope
behavior. Note-off/KILL cuts immediately, matching the channel's existing stop
contract; there is no release-tail routing or click suppression yet. Voices keep
their oscillator state between notes and stop consuming DSP when detached from a
channel. Muted active channels keep advancing, as samples do.

The X64 test reports 27,552 bytes per adapter (215.25 KiB for eight), including
the arena. The pool is fixed for the enabled build, independent of patch count.
Initialization, variable allocation and XML work occur before audio playback.
Start/stop/parameter updates and DSP rendering add no heap allocation, locks or
I/O. Cached variable pointers avoid LGPT's allocating `FindVariable` iterator.
The existing mixer and other instruments still allocate in their callbacks;
this patch does not make the whole LGPT callback allocation-free. Model switches
can perform bounded upstream reset/data-unpack work; worst-case callback timing
has not been measured. Upstream random state is global, so noise models are not
bitwise independent across channel scheduling, and rendering must remain serial.

Deferred: synth-specific tracker automation, tables,
pitch slides/legato/retrigger commands, sustained gates, AUX routing, high-quality
resampling, custom FM/wavetable data, CPU profiling, Windows/macOS project files
and handheld enablement. Only VOLM is implemented as an instrument command;
unsupported instrument commands are ignored. The adapter exposes pitch and
parameter setters for later automation work.

## Verification

From the repository root:

```sh
bash tests/run_plaits_test.sh
bash tests/run_clouds_test.sh
bash tests/run_sample_jump_test.sh
bash tests/run_record_timing_test.sh
bash tests/run_wav_test.sh
```

The Plaits test checks all 24 models at 32/44.1/48/96 kHz, identical output under
different buffer slicing, measured 440/880 Hz pitch, nonzero/bounded stereo
output, stop, parameter clamping,
and absence of allocations in the new path. It also exercises XML load/save,
cloning, channel isolation, mute, VOLM and simultaneous sample/synth mixer output.
Its headless engine-selector test sends button masks to the actual instrument
view, checks repeated Sampler/Plaits changes, field focus/layout, saved engine
type, unchanged MIDI slots, and replacement of active/cached playback references.
These are offline tests, not evidence of audible quality or real-time deadlines.

Historical checks before the main-build promotion covered enabled and disabled
X64 configurations. Both compiled without new warning diagnostics. The final
combined build and Clouds checks are recorded in [clouds.md](clouds.md).
Non-X64 Plaits builds, live UI/audio, listening quality and real-time CPU deadlines
remain unverified. These automated checks do not establish hardware readiness.

Implementation, tests and documentation were prepared with LLM assistance.
No human testing or review is recorded by this work. The existing pre-commit
hook already matches the repository's hook; LGPT additions were formatted with
clang-format and the vendored sources were kept unchanged.

Before any PR, the human author must read every line of the full diff, test the
change on a real device and record the hardware/OS, understand and explain the
approach, and be prepared to own/debug it. Build all supported platforms and
compare warnings against a build without the change. Use the PR template and
disclose LLM assistance; do not mark human testing complete based on these tests.
