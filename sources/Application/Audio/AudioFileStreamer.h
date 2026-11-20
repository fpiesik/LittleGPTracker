#ifndef _AUDIO_FILE_STREAMER_H_
#define _AUDIO_FILE_STREAMER_H_

#include "Application/Instruments/WavFile.h"
#include "Services/Audio/AudioModule.h"
#include "System/FileSystem/FileSystem.h"

enum AudioFileStreamerMode { AFSM_STOPPED, AFSM_PLAYING };

class AudioFileStreamer : public AudioModule {
  public:
    AudioFileStreamer();
    virtual ~AudioFileStreamer();
    virtual bool Render(fixed *buffer, int samplecount);
    bool Start(const Path &);
    void Stop();
    // Fraction of the stream rendered, or -1 when idle.
    double GetProgress() const;

  protected:
    AudioFileStreamerMode mode_;
    Path path_;
    bool newPath_;
    WavFile *wav_;
    double position_;
    int shift_;
};

#endif
