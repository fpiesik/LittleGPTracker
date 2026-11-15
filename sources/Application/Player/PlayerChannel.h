
#ifndef _PLAYER_CHANNEL_H_
#define _PLAYER_CHANNEL_H_

#include "Application/Instruments/I_Instrument.h"
#include "Application/Mixer/MixBus.h"
#include "Application/Utils/SampleTransition.h"
#include "Services/Audio/AudioModule.h"
class ReverbBus;

class PlayerChannel: public AudioModule {
public:
	PlayerChannel(int index) ;
	virtual ~PlayerChannel() ;
	virtual bool Render(fixed *buffer,int samplecount) ;
	void StartInstrument(I_Instrument *instr,unsigned char note,bool cleanStart) ;
	void StopInstrument() ;
	I_Instrument *GetInstrument() ;
	void SetMute(bool muted) ;
	bool IsMuted() ;
	void SetMixBus(int i) ;
    void SetVolume(fixed volume);
    void SetReverbSend(ReverbBus *bus, int send) {
        reverb_ = bus;
        send_ = send;
    }
    void SetHPFMode(unsigned char mode);
    void ApplyHPF(fixed *buffer, int samplecount);
    void SetLPFFreq(unsigned short freq);
    void Reset();

  private:
    ReverbBus *reverb_;
    int send_;
    int index_ ;
	I_Instrument *instr_ ;
	bool muted_ ;
    fixed volume_;
    SampleTransition sampleTransition_;
    int busIndex_ ;
	MixBus *mixBus_ ;
    fixed hpfPrevInput_[2];
    fixed hpfPrevOutput_[2];
	fixed hpfAlpha_;
	unsigned char hpfMode_;
    fixed lpfPrevOutput_[2];
    fixed lpfAlpha_;
    unsigned short lpfFreq_;
};

#endif
