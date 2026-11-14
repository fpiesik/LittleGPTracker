# WAV compatibility

Sample import supports uncompressed PCM WAV, 8 or 16 bits, mono or stereo.
The original sample rate is retained. Browser preview converts to the current
output rate using linear interpolation, including across audio buffer boundaries.
This does not provide band-limited downsampling; it matches the existing linear
interpolation approach used for instruments.

The reader skips unknown RIFF chunks, including their odd-length padding, and
accepts metadata before the format chunk. It validates chunk boundaries, PCM
block alignment and byte rate, channel count, sample rate and allocation sizes.
Empty, truncated, duplicate format/data chunks and unsupported encodings are
rejected. Only files fitting the existing signed-int buffer/offset API are loaded.
24-bit PCM, floating-point WAV and WAVE_FORMAT_EXTENSIBLE remain unsupported.

Run `bash tests/run_wav_test.sh` on the Linux X64 build environment for synthetic
loader and preview regression tests. Real-device listening tests are still needed,
especially for 8-bit stereo, mixed sample rates and previewing while a song plays.
