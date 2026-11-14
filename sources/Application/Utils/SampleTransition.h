#ifndef _SAMPLE_TRANSITION_H_
#define _SAMPLE_TRANSITION_H_

#include "Application/Utils/fixed.h"

// Blend from the last output frame over 2 ms without moving the playhead.
// Owned by the playback channel so slice/instrument changes retain continuity.
class SampleTransition {
  public:
    SampleTransition() { Reset(); }

    void Reset() {
        last_[0] = last_[1] = from_[0] = from_[1] = 0;
        remaining_ = length_ = 0;
    }

    void Process(fixed *buffer, int frames, int sampleRate, bool jumped) {
        if (frames <= 0)
            return;
        if (jumped) {
            length_ = sampleRate / 500;
            if (length_ < 1)
                length_ = 1;
            remaining_ = length_;
            from_[0] = last_[0];
            from_[1] = last_[1];
        }
        for (int i = 0; i < frames && remaining_; ++i) {
            fixed oldWeight = (remaining_ * FP_ONE) / length_;
            for (int ch = 0; ch < 2; ++ch) {
                // Widen before multiplying: opposite full-scale values
                // must not overflow during the blend.
                buffer[2 * i + ch] = static_cast<fixed>(
                    (static_cast<long long>(from_[ch]) * oldWeight +
                     static_cast<long long>(buffer[2 * i + ch]) *
                         (FP_ONE - oldWeight)) /
                    FP_ONE);
            }
            --remaining_;
        }
        last_[0] = buffer[2 * frames - 2];
        last_[1] = buffer[2 * frames - 1];
    }

  private:
    fixed last_[2], from_[2];
    int remaining_, length_;
};

#endif
