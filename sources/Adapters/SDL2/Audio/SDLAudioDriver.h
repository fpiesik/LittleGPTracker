#ifndef _SDL_SOUND_H_
#define _SDL_SOUND_H_

#include "RecordTiming.h"
#include "Services/Audio/AudioDriver.h"
#include "System/Process/Process.h"
#include <SDL2/SDL.h>

class SDLAudioDriver;

class SDLAudioDriverThread : public SysThread {
  public:
    SDLAudioDriverThread(SDLAudioDriver *driver);
    virtual ~SDLAudioDriverThread(){};
    virtual bool Execute();
    virtual void RequestTermination();
    void Notify();

  private:
    SDLAudioDriver *driver_;
    SysSemaphore *semaphore_;
};

class SDLAudioDriver : public AudioDriver {
  public:
    SDLAudioDriver(AudioSettings &settings);
    virtual ~SDLAudioDriver();

    // Sound implementation
    virtual bool InitDriver();
    virtual void CloseDriver();
    virtual bool StartDriver();
    virtual void StopDriver();
    virtual int GetPlayedBufferPercentage();
    virtual int GetSampleRate();
    virtual bool Interlaced() { return true; };
    virtual double GetStreamTime();
    static void BeginRecording(int tempo, double latencyMs, void (*start)());
    static int RecordingBeat();
    static SDL_AudioDeviceID OpenCapture();
    static int RecordChannels;
    enum MetronomeMode {
        METRONOME_OFF,
        METRONOME_RECORD,
        METRONOME_PLAY_AND_REC
    };
    static int RecordMetronome;
    static int RecordMetronomeVolume;
    static int RecordCountIn;
    static void MixMetronome(short *buffer, int frames, int tempo,
                             bool beatStart, bool playing);
    static bool CapturePeak(unsigned int &peak);
    static void CloseCapture();
    static void StopCapture();
    static int ReadCapture(Uint8 *buffer, int size);
    // Additional
    void OnChunkDone(Uint8 *stream, int len);
    void RenderBuffer();

  private:
    static SDLAudioDriver *instance_;
    SDL_mutex *recordMutex_;
    void (*recordStart_)();
    Sint64 countFrame_, countLength_;
    int countTempo_, countBeats_;
    Sint64 metronomeFrame_;
    Sint64 countTags_[SOUND_BUFFER_COUNT];
    double recordLatency_;
    bool recordOutputStarted_;
    int fragSize_; // Actual fragsize used by the driver
    char *unalignedMain_;
    char *mainBuffer_;
    char *miniBlank_;
    int bufferPos_;
    int bufferSize_;
    int sampleRate_;
    SDLAudioDriverThread *thread_;
    Uint32 startTime_;
};

#endif
