#ifndef _REVERB_BUS_H_
#define _REVERB_BUS_H_

#include "Application/Model/Song.h"
#include "Services/Audio/AudioSendEffect.h"

// One instance owned by MixerService. All storage is allocated before playback.
class ReverbBus : public AudioSendEffect {
  public:
    enum {
        MAX_FRAMES = 10000
    }; // AudioOut's MIX_BUFFER_SIZE / 4 stereo frames.
    ReverbBus();
    ~ReverbBus();
    void Init(int sampleRate);
    void SetParameters(int decay, int damp, int diffusion, int modulation,
                       int level);
    void BeginRender(int frames);
    void Add(int channel, const fixed *buffer, int frames, int send);
    bool MixReturn(fixed *buffer, int frames, bool hasDry);
    bool IsActive() const;

  private:
    struct State;
    State *state_;
    float send_[MAX_FRAMES * 2];
    float sendGain_[SONG_CHANNEL_COUNT];
    float sendSlew_;
    int frames_;
    bool hasInput_;
    ReverbBus(const ReverbBus &);
    ReverbBus &operator=(const ReverbBus &);
};
#endif
