# Mutable Instruments DSP snapshot

Unmodified source subset from:

- https://github.com/pichenettes/eurorack/tree/f4cfa7697095a28d292bff758beb51deb49b5a86/plaits
- https://github.com/pichenettes/eurorack/blob/f4cfa7697095a28d292bff758beb51deb49b5a86/clouds/dsp/fx/reverb.h
- Its pinned stmlib submodule: https://github.com/pichenettes/stmlib/tree/e3bd7c9cc00e4364166f9905c0509b6ffd0535ec

Includes Plaits DSP translation units, resources, `user_data.h`, stmlib
`dsp/units.cc` and `utils/random.cc`, and their recursively included headers.
Clouds adds only `dsp/frame.h`, `dsp/fx/fx_engine.h` and `dsp/fx/reverb.h`;
its stmlib dependencies are shared with Plaits. No Clouds firmware is included.
Firmware, hardware, UI, bootloaders and resource generators are excluded.
The STM32-only `stmlib/system/flash_programming.h` include is excluded because
the desktop build uses upstream's `TEST` mode. No upstream sources are patched.

Each source retains its MIT copyright and permission notice. `LICENSE`
reproduces the notice from `plaits/dsp/voice.h`; individual files retain their
respective copyright years. This component remains MIT licensed within LGPT's
GPLv3 distribution. Keep these notices when distributing sources/binaries.

`SHA256SUMS` records all 129 imported files. Verify from this directory with
`sha256sum -c SHA256SUMS`. On refresh, compare against both pinned checkouts;
do not run LGPT's formatter over this upstream snapshot.

The build defines `TEST` only for DSP translation units and the adapter. This
selects portable math and disables flash reads. `voice.cc` needs a forced
`cstdio` include because upstream's desktop `user_data.h` uses `printf` without
including its declaration. The application does not call the flash-save mocks.
