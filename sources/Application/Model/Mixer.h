
#ifndef _MIXER_H_
#define _MIXER_H_

#include "Application/Persistency/Persistent.h"
#include "Foundation/T_Singleton.h"

#include "Application/Utils/fixed.h"
#include "Foundation/Variables/Variable.h"
#include "Song.h"

class Mixer : public T_Singleton<Mixer>, Persistent {
  public:
    enum { FX_DECAY, FX_DAMP, FX_DIFF, FX_MOD, FX_LEVEL, FX_COUNT };
    Mixer();
    ~Mixer();
    void Clear();

    inline int GetBus(int i) { return channelBus_[i]; };
    inline void SetBus(int i, int value) { channelBus_[i] = value; };

    inline int GetChannelVolume(int i) { return channelVolume_[i]; };
    inline void SetChannelVolume(int i, int value) {
        channelVolume_[i] = (unsigned char)value;
    };
    inline int GetChannelHPF(int i) { return channelHPF_[i]; };
    inline void SetChannelHPF(int i, int value) {
        channelHPF_[i] = (unsigned char)value;
    };
    inline unsigned short GetChannelLPF(int i) { return channelLPF_[i]; };
    inline void SetChannelLPF(int i, unsigned short value) {
        channelLPF_[i] = value;
    };
    int GetChannelSend(int i) const { return channelSend_[i]; }
    void SetChannelSend(int i, int value) {
        channelSend_[i] = value < 0 ? 0 : (value > 255 ? 255 : value);
    }
    Variable &GetReverbParameter(int i) { return *reverb_[i]; }

    virtual void SaveContent(TiXmlNode *node);
    virtual void RestoreContent(TiXmlElement *element);

  private:
    unsigned char channelSend_[SONG_CHANNEL_COUNT];
    Variable *reverb_[FX_COUNT];
    char channelBus_[SONG_CHANNEL_COUNT];
    unsigned char channelVolume_[SONG_CHANNEL_COUNT];
    unsigned char channelHPF_[SONG_CHANNEL_COUNT];
    unsigned short channelLPF_[SONG_CHANNEL_COUNT];
};

#endif
