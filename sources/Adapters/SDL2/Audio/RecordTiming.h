#ifndef RECORD_TIMING_H
#define RECORD_TIMING_H

#include <SDL2/SDL.h>
#include <math.h>

// Integer sample positions keep beats independent of UI/callback size.
struct RecordTiming {
    static Sint64 BeatFrame(int beat, int rate, int tempo) {
        return (Sint64(beat) * rate * 60 + tempo / 2) / tempo;
    }

    static short Click(Sint64 frame, int rate, int tempo, int beats = 4,
                       int volume = 100) {
        for (int beat = 0; beat < beats; ++beat) {
            Sint64 phase = frame - BeatFrame(beat, rate, tempo);
            int length = rate / 20;
            if (phase >= 0 && phase < length) {
                int value = ((phase * 2000 / rate) & 1) ? 3000 : -3000;
                return (value * (length - phase) / length) * volume / 100;
            }
        }
        return 0;
    }

    // A positive round-trip latency removes more input from the start.
    static Sint64 CaptureSkip(double seconds) {
        return seconds > 0 ? Sint64(ceil(seconds * 44100.0)) : 0;
    }
};
#endif
