#ifndef SAMPLE_EDIT_BUFFER_H
#define SAMPLE_EDIT_BUFFER_H

#include "SoundSource.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// Owned, interleaved signed 16-bit PCM. All positions are frame boundaries.
class SampleEditBuffer : public SoundSource {
  public:
    enum Operation { CROP, DELETE, NORMALIZE, SILENCE, FADE_IN, FADE_OUT };
    std::vector<short> pcm;
    int channels, rate;
    SampleEditBuffer() : channels(1), rate(44100) {}
    int GetSize(int) { return pcm.size() / channels; }
    int GetSampleRate(int) { return rate; }
    int GetChannelCount(int) { return channels; }
    void *GetSampleBuffer(int) { return pcm.empty() ? 0 : &pcm[0]; }
    bool IsMulti() { return false; }
    int GetRootNote(int) { return 60; }

    bool Process(Operation op, int first, int last) {
        if (first < 0 || first >= last || last > GetSize(-1))
            return false;
        size_t a = size_t(first) * channels, b = size_t(last) * channels;
        if (op == CROP) {
            std::vector<short> selected(pcm.begin() + a, pcm.begin() + b);
            pcm.swap(selected);
        } else if (op == DELETE) {
            // An empty WAV cannot be played by the tracker.
            if (first == 0 && last == GetSize(-1))
                return false;
            pcm.erase(pcm.begin() + a, pcm.begin() + b);
        } else {
            int peak = 0;
            if (op == NORMALIZE)
                for (size_t i = a; i < b; ++i)
                    peak = std::max(peak, std::abs(int(pcm[i])));
            for (int frame = first; frame < last; ++frame) {
                double gain = 0;
                if (op == NORMALIZE)
                    gain = peak ? 32767.0 / peak : 1;
                if (op == FADE_IN || op == FADE_OUT) {
                    gain = last - first > 1
                               ? double(frame - first) / (last - first - 1)
                               : 0;
                    if (op == FADE_OUT && last - first > 1)
                        gain = 1 - gain;
                }
                for (int c = 0; c < channels; ++c) {
                    size_t i = size_t(frame) * channels + c;
                    pcm[i] = short(pcm[i] * gain);
                }
            }
        }
        return true;
    }

    // Explicit little endian output also works on big endian targets.
    bool Write(FILE *file, int first, int last, int outputRate = 0) {
        if (!file || first < 0 || first >= last || last > GetSize(-1))
            return false;
        if (!outputRate)
            outputRate = rate;
        unsigned long frames = (static_cast<unsigned long long>(last - first) *
                                outputRate + rate - 1) / rate;
        if (frames > (0xffffffffUL - 36) / (2 * channels))
            return false;
        unsigned long bytes = frames * channels * 2;
        bool ok = fwrite("RIFF", 1, 4, file) == 4;
        ok = Put(file, bytes + 36, 4) && ok;
        ok = (fwrite("WAVEfmt ", 1, 8, file) == 8) && ok;
        ok = Put(file, 16, 4) && Put(file, 1, 2) &&
             Put(file, channels, 2) && Put(file, outputRate, 4) &&
             Put(file, outputRate * channels * 2, 4) &&
             Put(file, channels * 2, 2) && Put(file, 16, 2) && ok;
        ok = (fwrite("data", 1, 4, file) == 4) && ok;
        ok = Put(file, bytes, 4) && ok;
        for (unsigned long i = 0; ok && i < frames; ++i) {
            double pos = double(i) * rate / outputRate;
            int f = std::min(first + int(pos), last - 1);
            int next = std::min(f + 1, last - 1);
            double fraction = pos - int(pos);
            for (int c = 0; c < channels; ++c) {
                short value = short(pcm[size_t(f) * channels + c] * (1 - fraction) +
                                    pcm[size_t(next) * channels + c] * fraction);
                ok = Put(file, static_cast<unsigned short>(value), 2) && ok;
            }
        }
        return ok && !ferror(file);
    }

  private:
    static bool Put(FILE *file, unsigned long value, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            if (fputc(value & 255, file) == EOF)
                return false;
            value >>= 8;
        }
        return true;
    }
};
#endif
