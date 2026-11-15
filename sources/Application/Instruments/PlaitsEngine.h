#ifndef _PLAITS_ENGINE_H_
#define _PLAITS_ENGINE_H_

#include "Application/Utils/fixed.h"
#include "plaits/dsp/voice.h"

// One monophonic voice. Init runs before playback; all subsequent operations
// use fixed storage. Only the audio thread may operate on a running voice.
class PlaitsEngine {
  public:
    PlaitsEngine();
    ~PlaitsEngine();
    void Init(int sampleRate);
    void NoteOn(unsigned char note);
    void NoteOff();
    void SetPitch(float note);
    void SetParameters(int model, int harmonics, int timbre, int morph,
                       int decay);
    void SetVolume(int volume);
    bool Render(fixed *buffer, int frames);
    bool IsInitialized() const { return voice_ != 0; }

  private:
    float NextSample();
    // Upstream assumes zero-initialized voice storage, as in its firmware.
    union {
        long double alignment;
        unsigned char bytes[sizeof(plaits::Voice)];
    } storage_;
    plaits::Voice *voice_;
    uint32_t arena_[16384 / sizeof(uint32_t)];
    plaits::Patch patch_;
    plaits::Modulations modulations_;
    plaits::Voice::Frame block_[plaits::kBlockSize];
    size_t position_;
    int triggerPhase_;
    bool playing_;
    bool primed_;
    double phase_;
    double step_;
    float left_, right_, volume_;

    PlaitsEngine(const PlaitsEngine &);
    PlaitsEngine &operator=(const PlaitsEngine &);
};
#endif
