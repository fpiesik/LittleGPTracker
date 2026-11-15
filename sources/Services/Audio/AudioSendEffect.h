#ifndef _AUDIO_SEND_EFFECT_H_
#define _AUDIO_SEND_EFFECT_H_

#include "Application/Utils/fixed.h"

// Optional send/return attached to an existing mixer. BeginRender precedes
// channel rendering; MixReturn runs afterwards, before master gain/clipping.
class AudioSendEffect {
  public:
    virtual ~AudioSendEffect() {}
    virtual void BeginRender(int frames) = 0;
    virtual bool MixReturn(fixed *buffer, int frames, bool hasDry) = 0;
};
#endif
