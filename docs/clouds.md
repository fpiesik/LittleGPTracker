# Shared Clouds reverb

One global stereo Clouds reverb serves all eight tracker channels, including
samples and Plaits. It uses the existing audio backend and mixer graph. The
original MIT DSP headers are unchanged; source revision, license and checksums
are recorded in `sources/Externals/Mutable/README.md`.

## Controls

On the Mixer HPF and LPF rows, B + A disables the selected channel's filter
(shown as --), leaving its volume unchanged.

On the Mixer page, move down from LPF to **SEND**. Left/right selects a channel;
A + Left/Right changes its hex value by 01 and A + Up/Down by 10. B + A resets
SEND to 00. All channels start at 00. FF sends full channel amplitude to the
effect without attenuating the dry signal. Channel volume and mute affect the
send; changing SEND to zero ramps the input down and leaves the existing tail.

X + A (the R + A controller combination) follows the mute gesture: release A
while X remains held to undo the temporary solo; release X first to latch it.
Repeat the latched gesture on the same channel to restore previous mutes.
Mixer X + B uses the same temporary/latching behavior for mute.
Selecting another solo channel retains that original mute state, including
when switching between Song, Chain, Phrase and Mixer views. A manual mute edit
ends solo tracking and keeps the current mutes; Unmute All clears solo tracking.

R + Down from Mixer opens **E**, shown directly below M in the page map.
R + Up returns to Mixer, retaining the selected channel. On E, Up/Down selects
a parameter, A + arrows edits it using the same hex increments, and B + A
restores its default. Start and R + Start retain song playback controls.

| Control | Default | Mapping for normalized value u = hex / 255 |
| --- | --- | --- |
| DECAY | A0 | Feedback = 0.20 + 0.72u; maximum 0.92, no freeze |
| DAMP | 80 | Tank low-pass coefficient = 0.90 - 0.85u; higher is darker |
| DIFF | E0 | All-pass diffusion coefficient = 0.70u |
| MOD | 00 | Additional wet stereo delay modulation, up to ±0.75 ms |
| LEVEL | 60 | Wet return gain = u; dry gain is unchanged |

Clouds has fixed modulation inside its tank and exposes no modulation-depth
setter. MOD therefore controls an adapter delay on the stereo wet output:
1 ms base delay, opposing triangular modulation at 0.23 Hz, depth 0–0.75 ms.
MOD 00 leaves the original internal modulation present. LEVEL 00 silences the
return while preserving the tank state. It does not clear the tail.

## Shared audio pipeline

```text
Sampler or Plaits -> PlayerChannel HPF/LPF -> channel volume -> existing MixBus
                                                   |                 |
                                                   SEND              v
                                                   |            dry master
                                                   v                 |
                                         one ReverbBus -> LEVEL ----+
                                                                     |
                                                        existing master output
```

`PlayerChannel::Render` sends post-filter, post-volume, unmuted audio to
`ReverbBus`. This is before MixBus pregain. The browser preview has no send.
`MixerService` owns the single reverb and attaches it to its master
`AudioMixer` through the small `AudioSendEffect` interface. `BeginRender`
starts accumulation before child buses render; `MixReturn` runs afterwards,
including when every channel is silent. Instrument rendering, bus mixing and
the global effect remain separate. The normal master processing follows.

`PlayerMixer::Update` copies SEND and FX settings with the existing mixer
settings, under the existing synchronization. The adapter introduces no new
locks, blocking operations, allocation, logging or file access while rendering.
It allocates its state before playback. SEND gains slew over 5 ms; the five
global parameters use approximately 20 ms smoothing while the tank is active.

Clouds' delay lengths and LFOs are designed for **32 kHz**, independently of
Plaits' 48 kHz engine. The adapter runs Clouds at 32 kHz and converts the wet
path to/from the negotiated output rate, preserving fractional phase across
arbitrary render slices. Two inexpensive low-pass stages precede input linear
conversion; output conversion is linear. This is a CPU/quality compromise,
not a high-quality bandlimited resampler. No conversion is added to dry audio.
As in upstream, the tank excitation sums left/right input and generates a
stereo return; antiphase input can cancel in that excitation.
The upstream reverb accepts arbitrary frame counts, including one frame.

Feedback/diffusion are bounded below unity. Summed sends and wet output are
bounded, and the dry/wet addition saturates rather than overflowing fixed-point
PCM. The shared mixer also saturates its integer accumulator before its usual
clipping stage. This prevents wraparound; it is not a transparent limiter.
Eight loud channels and high LEVEL can still audibly clip. Adjust gain/send
levels to retain headroom.

With zero sends and no tail the DSP is skipped and the dry buffer is unchanged.
After two seconds of output below -96 dB with no input, processing sleeps.
The tank is not cleared by note-off, mute, song stop or LEVEL changes; project
initialization resets it. The fixed memory cost is approximately 112 KiB:
78.1 KiB send accumulation, 32 KiB upstream delay memory, plus adapter state.
There is one copy, not eight. Render sizes up to AudioOut's current 10,000
stereo-frame maximum are supported; oversized blocks safely bypass the effect.

## Persistence and files

The existing `MIXER` XML block gains a `SEND` hex buffer of eight bytes and
a `REVERB` element with decimal attributes DECAY, DAMP, DIFF, MOD and LEVEL.
`Mixer::RestoreContent` resets defaults before reading optional fields, validates
new send lengths and clamps FX values. `Project` registers/resets Mixer before
loading so a missing MIXER block also starts with zero sends and default FX.
Existing BUS, VOL, HPF and LPF fields retain their format.
Older LGPT versions ignore the added settings and lose them if they resave;
Plaits patches still require a Plaits-enabled build.

Principal changes:

- `Application/Mixer/ReverbBus.*`, `Services/Audio/AudioSendEffect.h` and
  `AudioMixer.*`: adapter, shared return hook and bounded summation.
- `MixerService.*`, `PlayerChannel.*`, `PlayerMixer.cpp`: ownership and routing.
- `Model/Mixer.*`, `Project.cpp`: settings and backward-compatible load defaults.
- `Views/MixerView.*`, `FxView.*`, `BaseClasses/View.*`, `AppWindow.*`: SEND,
  E controls, page navigation and view lifecycle.
- `projects/Makefile` and existing Visual Studio/Xcode project files: common
  adapter/view compilation and Mutable include path. Plaits is included in the normal X64 build.
- Three original Clouds headers, checksum manifest, documentation and
  `tests/clouds_test.cpp` / `run_clouds_test.sh`.

## Build and verification

For the combined desktop build:

```sh
cd projects
make PLATFORM=X64 -j2
./lgpt.x64
```

The main `lgpt.x64` contains both sampler and Plaits engines plus Clouds.
There is no separate Plaits executable or opt-in build flag. From the repo root:

```sh
bash tests/run_clouds_test.sh
bash tests/run_plaits_test.sh
bash tests/run_sample_jump_test.sh
bash tests/run_record_timing_test.sh
bash tests/run_wav_test.sh
```

The Clouds headless test covers 44.1/48 kHz, deterministic buffer slicing,
zero-send dry identity, stereo output, decaying tails and sleep, extreme
parameters, live parameter edits, eight full sends and allocation guards in
the new DSP path. Integration checks exercise sample/Plaits channels together,
volume, filters, mute and tails after both instruments stop. XML checks cover
all sends/FX, missing fields and invalid new values; button-mask checks exercise
all eight SEND controls and E navigation/editing. Overload checks prevent
signed integer wraparound in eight-channel mixing.

Validation on 2026-11-15, Debian 12 AArch64, GCC 12:

- The normal `make PLATFORM=X64` build compiled/linked `lgpt.x64` with
  Plaits, Clouds and E. X64 is the repository configuration name; this host
  produced a native AArch64 binary, not an x86-64 binary.
- Full-build comparison retained the same 197 warning lines/diagnostics as
  the pre-Clouds baseline; no new warnings. The earlier sample-only build
  also passed before Plaits was made part of the main build.
- All five scripts above passed against the main build, including the
  Plaits engine-selector test. Audio and GUI tests are synthetic/headless.
- An isolated 30-second Clouds signal/tail probe passed AddressSanitizer and
  UBSan; leak detection was disabled in this sandbox. A standalone AArch64
  adapter compile also passed.
- All 129 vendored source files matched the pinned checkout and checksum
  manifest. Visual Studio XML parsed and the new Xcode object references
  were checked; neither constitutes a Windows/macOS build.
- Windows/macOS and full handheld builds were not run: their required
  toolchains/SDKs are unavailable here. Plaits remains enabled only in the
  normal X64 configuration. No live device audio or listening test was run.

## Remaining limits

These automated checks do not verify audible quality, live UI rendering,
device audio or callback deadlines. Hardware listening and performance tests
are required, particularly on older handhelds without fast floating point.
The new DSP path is allocation-free during rendering, but the pre-existing
LGPT mixer, instruments and driver still allocate and use existing locks;
the complete callback is not allocation-free or lock-free.

Stereo master recording includes the wet return while recording is active.
Existing stem recording taps the individual dry buses and has no separate wet
stem. Existing stop/end recording behavior does not append an automatic tail
to an exported file. Pattern commands for SEND/FX automation are not added;
the page controls work during playback. Plaits' previously documented command
and platform limitations still apply.

Implementation and automated checks were prepared with LLM assistance. Before
any PR the human author must read every line of the full diff, test on a real
device and record hardware/OS, understand and explain the implementation, and
accept responsibility for investigating related bugs. Human review/testing
has not been recorded here. All supported target builds and warning comparison
are required by AGENTS.md before a PR; local checks are not a substitute.

Solo gesture follow-up: the desktop main build and headless tests passed for
both key-release orders, temporary/latching mute and solo in Song, Chain and
Mixer, repeated solo-off, and saved mute restoration. Phrase uses the same
release handling but its GUI constructor requires a real AppWindow, so it was
not exercised by this headless harness. No hardware test was performed.
