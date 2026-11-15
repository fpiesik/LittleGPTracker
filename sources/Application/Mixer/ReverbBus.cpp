#include "ReverbBus.h"
#include <cmath>
#include <cstring>
#include <new>

// Upstream's portable math implementation, scoped to this translation unit.
#ifndef TEST
#define TEST
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "clouds/dsp/frame.h"
#include "clouds/dsp/fx/reverb.h"
#undef TAIL

namespace {
float Unit(int value) {
    return (value < 0 ? 0 : (value > 255 ? 255 : value)) / 255.0f;
}
float Limit(float value, float bound) {
    return value < -bound ? -bound : (value > bound ? bound : value);
}
} // namespace

struct ReverbBus::State {
    union {
        long double alignment;
        unsigned char bytes[sizeof(clouds::Reverb)];
    } storage;
    clouds::Reverb *core;
    uint16_t delay[16384];
    float target[5], current[5];
    float inputLP[2][2], previousInput[2], wet[2], previousWet[2];
    float modDelay[2][128];
    unsigned modPosition;
    double modPhase, phase, step;
    float lowPass;
    unsigned quietFrames;
    bool active;

    State() : core(0) {}
    ~State() {
        if (core)
            core->~Reverb();
    }

    void Init(int rate) {
        if (core)
            core->~Reverb();
        core = new (storage.bytes) clouds::Reverb;
        // Upstream expects zero-initialized firmware storage, and Init leaves
        // the decay filters untouched. Clear after construction: an optimizer
        // can discard clearing raw storage before the object's lifetime starts.
        std::memset(static_cast<void *>(core), 0, sizeof(*core));
        core->Init(delay);
        core->set_amount(1.0f); // Return wet only; dry stays in LGPT's mixer.
        core->set_input_gain(0.2f);
        std::memset(inputLP, 0, sizeof(inputLP));
        std::memset(previousInput, 0, sizeof(previousInput));
        std::memset(wet, 0, sizeof(wet));
        std::memset(previousWet, 0, sizeof(previousWet));
        std::memset(modDelay, 0, sizeof(modDelay));
        modPosition = quietFrames = 0;
        modPhase = phase = 0.0;
        step = 32000.0 / rate;
        lowPass = 1.0f - std::exp(-6.283185307f * 10000.0f / rate);
        active = false;
    }

    void Process(float left, float right, float *output) {
        float input[2] = {Limit(left, 2.0f), Limit(right, 2.0f)};
        for (int ch = 0; ch < 2; ++ch) {
            // Inexpensive two-pole anti-alias filtering of the wet input only.
            inputLP[ch][0] += lowPass * (input[ch] - inputLP[ch][0]);
            inputLP[ch][1] += lowPass * (inputLP[ch][0] - inputLP[ch][1]);
            input[ch] = inputLP[ch][1];
        }
        double next = phase + step;
        while (next >= 1.0) {
            float fraction = static_cast<float>((1.0 - phase) / step);
            clouds::FloatFrame frame;
            frame.l =
                previousInput[0] + (input[0] - previousInput[0]) * fraction;
            frame.r =
                previousInput[1] + (input[1] - previousInput[1]) * fraction;
            for (int p = 0; p < 5; ++p)
                current[p] += (target[p] - current[p]) * (1.0f / 640.0f);
            core->set_time(0.2f +
                           current[0] * 0.72f); // Never freeze/unity feedback.
            core->set_lp(0.9f - current[1] * 0.85f);
            core->set_diffusion(current[2] * 0.7f);
            core->Process(&frame,
                          1); // Upstream supports arbitrary frame counts.

            // Clouds' internal modulation depth is fixed. MOD adds a stereo
            // variable delay to the wet output, leaving upstream untouched.
            modDelay[0][modPosition] = frame.l;
            modDelay[1][modPosition] = frame.r;
            modPhase += 0.23 / 32000.0;
            if (modPhase >= 1.0)
                modPhase -= 1.0;
            float triangle =
                static_cast<float>(4.0 * std::fabs(modPhase - 0.5) - 1.0);
            for (int ch = 0; ch < 2; ++ch) {
                float distance =
                    32.0f + triangle * current[3] * (ch ? -24.0f : 24.0f);
                int whole = static_cast<int>(distance);
                float f = distance - whole;
                unsigned a = (modPosition - whole) & 127;
                unsigned b = (a - 1) & 127;
                previousWet[ch] = wet[ch];
                wet[ch] =
                    modDelay[ch][a] + (modDelay[ch][b] - modDelay[ch][a]) * f;
            }
            modPosition = (modPosition + 1) & 127;
            phase -= 1.0;
            next -= 1.0;
        }
        phase = next;
        for (int ch = 0; ch < 2; ++ch) {
            previousInput[ch] = input[ch];
            output[ch] = Limit(previousWet[ch] + (wet[ch] - previousWet[ch]) *
                                                     static_cast<float>(phase),
                               1.0f) *
                         current[4];
        }
    }
};

ReverbBus::ReverbBus() : state_(new State) { Init(44100); }
ReverbBus::~ReverbBus() { delete state_; }

void ReverbBus::Init(int sampleRate) {
    int rate = sampleRate > 0 ? sampleRate : 44100;
    state_->Init(rate);
    sendSlew_ = 1.0f / (0.005f * rate);
    std::memset(sendGain_, 0, sizeof(sendGain_));
    frames_ = 0;
    hasInput_ = false;
    SetParameters(160, 128, 224, 0, 96);
    for (int i = 0; i < 5; ++i)
        state_->current[i] = state_->target[i];
}

void ReverbBus::SetParameters(int decay, int damp, int diffusion,
                              int modulation, int level) {
    const int values[] = {decay, damp, diffusion, modulation, level};
    for (int i = 0; i < 5; ++i)
        state_->target[i] = Unit(values[i]);
}

void ReverbBus::BeginRender(int frames) {
    frames_ = frames > 0 && frames <= MAX_FRAMES ? frames : 0;
    hasInput_ = false;
}

void ReverbBus::Add(int channel, const fixed *buffer, int frames, int send) {
    if (channel < 0 || channel >= SONG_CHANNEL_COUNT || frames != frames_ ||
        !frames_)
        return;
    float target = Unit(send);
    float &gain = sendGain_[channel];
    if (target == 0.0f && gain == 0.0f)
        return;
    if (!hasInput_)
        std::memset(send_, 0, frames * 2 * sizeof(float));
    hasInput_ = true;
    for (int i = 0; i < frames; ++i) {
        float difference = target - gain;
        gain += Limit(difference, sendSlew_);
        send_[2 * i] += fp2fl(buffer[2 * i]) * (gain / 32768.0f);
        send_[2 * i + 1] += fp2fl(buffer[2 * i + 1]) * (gain / 32768.0f);
    }
}

bool ReverbBus::MixReturn(fixed *buffer, int frames, bool hasDry) {
    if (!frames_ || frames != frames_)
        return hasDry;
    // Do not wake the DSP just because a silent instrument still renders.
    bool signal = false;
    if (hasInput_) {
        for (int i = 0; i < frames * 2; ++i)
            if (std::fabs(send_[i]) > 0.000001f) {
                signal = true;
                break;
            }
    }
    if (!state_->active && !signal)
        return hasDry;
    if (!state_->active) {
        for (int i = 0; i < 5; ++i)
            state_->current[i] = state_->target[i];
    }
    state_->active = true;
    bool quiet = !signal;
    for (int i = 0; i < frames; ++i) {
        float wet[2];
        state_->Process(hasInput_ ? send_[2 * i] : 0.0f,
                        hasInput_ ? send_[2 * i + 1] : 0.0f, wet);
        for (int ch = 0; ch < 2; ++ch) {
            // Inspect the internal output, so LEVEL=00 does not discard a tail.
            if (std::fabs(state_->wet[ch]) > (1.0f / 65536.0f))
                quiet = false;
            long long mixed = hasDry ? buffer[2 * i + ch] : 0;
            mixed += fl2fp(wet[ch] * 32767.0f);
            const fixed limit = 32767 * FIXED_SCALE;
            buffer[2 * i + ch] =
                mixed > limit
                    ? limit
                    : (mixed < -limit ? -limit : static_cast<fixed>(mixed));
        }
    }
    state_->quietFrames = quiet ? state_->quietFrames + frames : 0;
    // Two seconds below -96 dB, longer than every internal delay path.
    if (state_->quietFrames * state_->step > 64000.0) {
        state_->active = false;
        state_->quietFrames = 0;
    }
    return true;
}

bool ReverbBus::IsActive() const { return state_->active; }
