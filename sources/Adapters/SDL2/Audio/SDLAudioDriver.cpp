#include "SDLAudioDriver.h"
#include "Services/Midi/MidiService.h"
#include "Services/Time/TimeService.h"
#include "System/Console/Trace.h"
#include "System/System/System.h"

SDLAudioDriver *SDLAudioDriver::instance_ = 0;
static SDL_atomic_t recordingBeat;
int SDLAudioDriver::RecordChannels = 1;
int SDLAudioDriver::RecordMetronome = SDLAudioDriver::METRONOME_RECORD;
int SDLAudioDriver::RecordCountIn = 4;
int SDLAudioDriver::RecordMetronomeVolume = 100;

namespace {
SDL_AudioDeviceID captureDevice = 0;
// Four seconds of stereo PCM. Idle input is discarded, never queued
// indefinitely.
const unsigned captureCapacity = 44100 * 4 * 4;
Uint8 captureBuffer[captureCapacity];
unsigned captureRead = 0, captureSize = 0;
bool capturing = false, captureOverflow = false;
bool captureWaiting = false;
double captureTarget = 0;
Sint64 captureSkip = 0;
int capturePeriod = 1024;
int captureChannels = 1;
unsigned inputPeak = 0;

void captureCallback(void *, Uint8 *stream, int length) {
    if (!capturing || length <= 0)
        return;
    unsigned peaks[2] = {0, 0};
    for (int i = 0; i < length / 2; ++i) {
        int value = Sint16(SDL_SwapLE16(reinterpret_cast<Uint16 *>(stream)[i]));
        unsigned magnitude = value < 0 ? -value : value;
        int channel = i % captureChannels;
        peaks[channel] = SDL_max(peaks[channel], magnitude);
    }
    if (captureChannels == 1)
        peaks[1] = peaks[0];
    inputPeak = (SDL_max(inputPeak >> 16, peaks[0]) << 16) |
                SDL_max(inputPeak & 65535, peaks[1]);
    if (captureWaiting) {
        if (!captureTarget)
            return;
        double now =
            double(SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency();
        if (now > captureTarget) {
            captureOverflow =
                true; // Input stalled past the recording boundary.
            capturing = false;
            return;
        }
        captureSkip = RecordTiming::CaptureSkip(captureTarget - now);
        captureWaiting = false;
    }
    int skip =
        int(SDL_min(captureSkip, Sint64(length / (2 * captureChannels))));
    captureSkip -= skip;
    stream += skip * 2 * captureChannels;
    length -= skip * 2 * captureChannels;
    unsigned size = static_cast<unsigned>(length);
    if (size > captureCapacity - captureSize) {
        captureOverflow = true;
        capturing = false;
        return;
    }
    unsigned write = (captureRead + captureSize) % captureCapacity;
    unsigned first = SDL_min(size, captureCapacity - write);
    memcpy(captureBuffer + write, stream, first);
    memcpy(captureBuffer, stream + first, size - first);
    captureSize += size;
}
} // namespace

bool SDLAudioDriver::CapturePeak(unsigned int &peak) {
    if (!captureDevice)
        return false;
    SDL_LockAudioDevice(captureDevice);
    bool active = capturing;
    peak = inputPeak;
    inputPeak = 0;
    SDL_UnlockAudioDevice(captureDevice);
    return active;
}

SDL_AudioDeviceID SDLAudioDriver::OpenCapture() {
    if (captureDevice && captureChannels == RecordChannels &&
        SDL_GetAudioDeviceStatus(captureDevice) != SDL_AUDIO_STOPPED)
        return captureDevice;
    CloseCapture();
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) &&
        SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return 0;
    SDL_AudioSpec wanted = {}, obtained = {};
    wanted.freq = 44100;
    wanted.format = AUDIO_S16LSB;
    captureChannels = RecordChannels == 2 ? 2 : 1;
    wanted.channels = captureChannels;
    wanted.samples = 1024;
    wanted.callback = captureCallback;
    SDL_SetHint("SDL_AUDIO_DEVICE_APP_NAME", "LittleGPTracker");
    captureDevice = SDL_OpenAudioDevice(NULL, 1, &wanted, &obtained, 0);
    if (captureDevice) {
        capturePeriod = obtained.samples;
        // Keep the stream running so patchbay links survive between takes.
        SDL_PauseAudioDevice(captureDevice, 0);
    } else {
        Trace::Log("AUDIO", "Cannot open persistent input: %s", SDL_GetError());
    }
    return captureDevice;
}

void SDLAudioDriver::CloseCapture() {
    if (captureDevice)
        SDL_CloseAudioDevice(captureDevice);
    captureDevice = 0;
    capturing = false;
    captureOverflow = false;
    inputPeak = 0;
    captureRead = captureSize = 0;
}

void SDLAudioDriver::BeginRecording(int tempo, double latencyMs,
                                    void (*start)()) {
    SDLAudioDriver *driver = instance_;
    if (!driver || !captureDevice)
        return;
    SDL_LockMutex(driver->recordMutex_);
    SDL_LockAudio();
    SDL_LockAudioDevice(captureDevice);
    inputPeak = 0;
    captureRead = captureSize = 0;
    captureOverflow = false;
    captureWaiting = capturing = true;
    captureTarget = 0;
    driver->countTempo_ = tempo > 0 ? tempo : 120;
    driver->countFrame_ = 0;
    driver->countBeats_ = RecordCountIn;
    driver->countLength_ = RecordTiming::BeatFrame(
        driver->countBeats_, driver->sampleRate_, driver->countTempo_);
    // SDL2 has no portable hardware latency query. Estimate one period each
    // way, or use the user's measured callback-to-input round-trip latency.
    driver->recordLatency_ =
        latencyMs >= 0 ? latencyMs / 1000.0
                       : double(driver->fragSize_ / 4) / driver->sampleRate_ +
                             double(capturePeriod) / 44100;
    driver->recordStart_ = start;
    driver->recordOutputStarted_ = false;
    SDL_AtomicSet(&recordingBeat, 0);
    SDL_UnlockAudioDevice(captureDevice);
    SDL_UnlockAudio();
    SDL_UnlockMutex(driver->recordMutex_);
}

int SDLAudioDriver::RecordingBeat() { return SDL_AtomicGet(&recordingBeat); }

void SDLAudioDriver::StopCapture() {
    if (instance_) {
        SDL_LockMutex(instance_->recordMutex_);
        instance_->recordStart_ = 0;
        SDL_LockAudio();
        for (int i = 0; i < SOUND_BUFFER_COUNT; ++i) {
            if (instance_->countTags_[i] >= 0 && instance_->pool_[i].buffer_)
                memset(instance_->pool_[i].buffer_, 0,
                       instance_->pool_[i].size_);
            instance_->countTags_[i] = -1;
        }
        if (captureDevice) {
            SDL_LockAudioDevice(captureDevice);
            captureOverflow =
                captureOverflow || SDL_AtomicGet(&recordingBeat) == -2;
            capturing = false;
            SDL_UnlockAudioDevice(captureDevice);
        }
        SDL_AtomicSet(&recordingBeat, -1);
        SDL_UnlockAudio();
        SDL_UnlockMutex(instance_->recordMutex_);
    }
}

// The worker queues count-in and song audio consecutively. Starting the player
// here avoids both UI jitter and a second trip through the playback queue.
void SDLAudioDriver::RenderBuffer() {
    SDL_LockMutex(recordMutex_);
    if (SDL_AtomicGet(&recordingBeat) == -2)
        recordStart_ = 0;
    if (recordStart_ && countFrame_ < countLength_) {
        short buffer[SOUND_BUFFER_MAX / 2];
        int frames =
            SDL_min(SOUND_BUFFER_MAX / 4, sampleRate_ * 60 / countTempo_ / 24);
        frames = int(
            SDL_min(Sint64(SDL_max(frames, 1)), countLength_ - countFrame_));
        for (int i = 0; i < frames; ++i)
            buffer[i * 2] = buffer[i * 2 + 1] =
                RecordMetronome == METRONOME_OFF
                    ? 0
                    : RecordTiming::Click(countFrame_ + i, sampleRate_,
                                          countTempo_, countBeats_,
                                          RecordMetronomeVolume);
        SDL_LockAudio();
        countTags_[poolQueuePosition_] = countFrame_;
        AddBuffer(buffer, frames);
        SDL_UnlockAudio();
        countFrame_ += frames;
        MidiService::GetInstance()->AdvancePlayQueue();
    } else {
        if (recordStart_) {
            void (*start)() = recordStart_;
            recordStart_ = 0;
            // With no count-in, tag the first song buffer as the capture
            // origin.
            if (countLength_ == 0) {
                SDL_LockAudio();
                countTags_[poolQueuePosition_] = 0;
                SDL_UnlockAudio();
            }
            start();
        }
        OnNewBufferNeeded();
    }
    SDL_UnlockMutex(recordMutex_);
}

// Mix only into device playback, after the song mix/file rendering stage.
void SDLAudioDriver::MixMetronome(short *buffer, int frames, int tempo,
                                  bool beatStart, bool playing) {
    SDLAudioDriver *driver = instance_;
    if (!driver || !playing)
        return;
    if (beatStart)
        driver->metronomeFrame_ = 0;
    bool enabled =
        RecordMetronome == METRONOME_PLAY_AND_REC ||
        (RecordMetronome == METRONOME_RECORD && RecordingBeat() >= 0);
    for (int i = 0; i < frames; ++i) {
        int click = enabled ? RecordTiming::Click(driver->metronomeFrame_,
                                                  driver->sampleRate_, tempo, 1,
                                                  RecordMetronomeVolume)
                            : 0;
        for (int channel = 0; channel < 2; ++channel) {
            int value = buffer[i * 2 + channel] + click;
            buffer[i * 2 + channel] = SDL_max(-32768, SDL_min(32767, value));
        }
        ++driver->metronomeFrame_;
    }
}

int SDLAudioDriver::ReadCapture(Uint8 *buffer, int size) {
    if (!captureDevice ||
        SDL_GetAudioDeviceStatus(captureDevice) == SDL_AUDIO_STOPPED)
        return -1;
    if (size <= 0)
        return 0;
    SDL_LockAudioDevice(captureDevice);
    int result = -1;
    if (!captureOverflow) {
        unsigned count = SDL_min(static_cast<unsigned>(size), captureSize);
        unsigned first = SDL_min(count, captureCapacity - captureRead);
        memcpy(buffer, captureBuffer + captureRead, first);
        memcpy(buffer + first, captureBuffer, count - first);
        captureRead = (captureRead + count) % captureCapacity;
        captureSize -= count;
        result = count;
    }
    SDL_UnlockAudioDevice(captureDevice);
    return result;
}

void sdl_callback(void *userdata, Uint8 *stream, int len) {
    SDLAudioDriver *sound = (SDLAudioDriver *)userdata;
    sound->OnChunkDone(stream, len);
};

SDLAudioDriverThread::SDLAudioDriverThread(SDLAudioDriver *driver) {
    // Uncap the semaphore to prevent dropped requests during long OS stalls.
    semaphore_ = SysSemaphore::Create(0, 1024);
    driver_ = driver;
};

bool SDLAudioDriverThread::Execute() {
    while (!shouldTerminate()) {
        semaphore_->Wait();
        driver_->RenderBuffer();
    };
    SysSemaphore *semaphore = semaphore_;
    semaphore_ = 0;
    delete semaphore;
    return true;
};

void SDLAudioDriverThread::Notify() {
    if (semaphore_) {
        semaphore_->Post();
    }
};

void SDLAudioDriverThread::RequestTermination() {
    SysThread::RequestTermination();
    // post to be sure we're not locked
    semaphore_->Post();
    // Wait for thread to finish
    SDL_Delay(10);
}

//-------------------------------------------------------------------------------------------------

SDLAudioDriver::SDLAudioDriver(AudioSettings &settings)
    : AudioDriver(settings), unalignedMain_(0), miniBlank_(0) {
    isPlaying_ = false;
    thread_ = 0;
    recordMutex_ = SDL_CreateMutex();
    recordStart_ = 0;
    recordOutputStarted_ = false;
    metronomeFrame_ = 0;
    for (int i = 0; i < SOUND_BUFFER_COUNT; ++i)
        countTags_[i] = -1;
    SDL_AtomicSet(&recordingBeat, -1);
    instance_ = this;
}

SDLAudioDriver::~SDLAudioDriver() {
    SDL_DestroyMutex(recordMutex_);
    instance_ = 0;
}

struct SDL_AudioSpec input;
struct SDL_AudioSpec returned;

bool SDLAudioDriver::InitDriver() {

    // set sound
    input.freq = settings_.sampleRate_;
    input.format = AUDIO_S16SYS;
    input.channels = 2;
    input.callback = sdl_callback;
    input.samples = settings_.bufferSize_;
    input.userdata = this;

    SDL_SetHint("SDL_APP_NAME", "LittleGPTracker");
    SDL_SetHint("SDL_AUDIO_DEVICE_APP_NAME", "LittleGPTracker");

    // On my machine this wasn't working.
    // SDL_AudioDeviceID deviceId =
    // SDL_OpenAudioDevice(NULL,0,&input,&returned,0); The above may return 0
    // meaning an error or success.
    int ret = SDL_OpenAudio(&input, &returned);
    if (ret != 0) {
        Trace::Error("AUDIO", "Couldn't open sdl audio: %s", SDL_GetError());
        return false;
    }
    const char *driverName = SDL_GetCurrentAudioDriver();

    fragSize_ = returned.size;
    sampleRate_ = returned.freq;
    // Allocates a rotating sound buffer
    unalignedMain_ = (char *)SYS_MALLOC(fragSize_ + SOUND_BUFFER_MAX);
    // Make sure the buffer is aligned
#ifdef _64BIT
    mainBuffer_ = (char *)unalignedMain_;
#else
    mainBuffer_ = (char *)((((int)unalignedMain_) + 1) & (0xFFFFFFFC));
#endif

    Trace::Log("AUDIO", "%s successfully opened with %d samples %d", driverName,
               fragSize_ / 4, returned.freq);

    // Create mini blank buffer in case of underruns

    miniBlank_ = (char *)malloc(fragSize_);
    SYS_MEMSET(miniBlank_, 0, fragSize_);

    return true;
};

void SDLAudioDriver::CloseDriver() {

    if (miniBlank_) {
        SYS_FREE(miniBlank_);
        miniBlank_ = 0;
    }

    if (unalignedMain_) {
        SYS_FREE(unalignedMain_);
        unalignedMain_ = 0;
    };
    SDL_CloseAudio();
};

bool SDLAudioDriver::StartDriver() {

    thread_ = new SDLAudioDriverThread(this);
    thread_->Start();

    short blank[4000];
    SYS_MEMSET(blank, 0, 4000);
    bufferPos_ = 0;
    bufferSize_ = 0;

    for (int i = 0; i < settings_.preBufferCount_; i++) {
        AddBuffer((short *)miniBlank_, fragSize_ / 4);
        MidiService::GetInstance()->AdvancePlayQueue();
    }
    if (settings_.preBufferCount_ == 0) {
        thread_->Notify();
    }

    SDL_PauseAudio(0);
    startTime_ = SDL_GetTicks();

    return 1;
};

void SDLAudioDriver::StopDriver() {
    if (thread_) {
        thread_->RequestTermination();
        SysThread *thread = thread_;
        thread_ = 0;
        SDL_PauseAudio(1);
        delete thread;
    };
};

double SDLAudioDriver::GetStreamTime() {
    return (SDL_GetTicks() - startTime_) / 1000.0;
}

void SDLAudioDriver::OnChunkDone(Uint8 *stream, int len) {
    const double callbackTime =
        double(SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency();

    // Look if we have enough data in main buffer

    while (bufferSize_ - bufferPos_ < len) {

        // First move remaining bytes at the front
        memmove(mainBuffer_, mainBuffer_ + bufferPos_,
                bufferSize_ - bufferPos_);

        // then get next queued buffer and copy data from it

        if (pool_[poolPlayPosition_].buffer_ == 0) {
            if (recordOutputStarted_ && SDL_AtomicGet(&recordingBeat) >= 0)
                SDL_AtomicSet(&recordingBeat,
                              -2); // Never save an unaligned take.
            // Use fragSize_ instead of len! miniBlank_ is only allocated to
            // fragSize_. Using len causes a buffer over-read if SDL requests a
            // larger chunk.
            SYS_MEMCPY(mainBuffer_ + bufferSize_ - bufferPos_, miniBlank_,
                       fragSize_);
            bufferSize_ = bufferSize_ - bufferPos_ + fragSize_;
            bufferPos_ = 0;
        } else {
            Sint64 tag = countTags_[poolPlayPosition_];
            if (SDL_AtomicGet(&recordingBeat) >= 0) {
                if (tag == 0 && captureDevice) {
                    recordOutputStarted_ = true;
                    SDL_LockAudioDevice(captureDevice);
                    captureTarget =
                        callbackTime +
                        double((bufferSize_ - bufferPos_) / 4 + countLength_) /
                            sampleRate_ +
                        recordLatency_;
                    SDL_UnlockAudioDevice(captureDevice);
                }
                int beat = countBeats_;
                if (tag >= 0) {
                    beat = 0;
                    while (beat < countBeats_ &&
                           tag >= RecordTiming::BeatFrame(beat + 1, sampleRate_,
                                                          countTempo_))
                        ++beat;
                }
                // Untagged buffers ahead of the first click are still idle.
                if (recordOutputStarted_)
                    SDL_AtomicSet(&recordingBeat, beat);
            }
            countTags_[poolPlayPosition_] = -1;
            memcpy(mainBuffer_ + bufferSize_ - bufferPos_,
                   pool_[poolPlayPosition_].buffer_,
                   pool_[poolPlayPosition_].size_);

            // Adapt buffer variables

            bufferSize_ =
                bufferSize_ - bufferPos_ + pool_[poolPlayPosition_].size_;
            bufferPos_ = 0;

            SYS_FREE(pool_[poolPlayPosition_].buffer_);

            pool_[poolPlayPosition_].buffer_ = 0;
        }
        // Tick the engine and flush MIDI for every logical audio block
        // processed. This ensures consistent timing for application logic and
        // MIDI output, even during underruns where silence is played.
        onAudioBufferTick();
        MidiService::GetInstance()->Flush();

        // Advance poolPlayPosition_ and notify the thread after consuming
        // a logical chunk, regardless of whether it was real data or silence.
        poolPlayPosition_ = (poolPlayPosition_ + 1) % SOUND_BUFFER_COUNT;
        if (thread_) {
            thread_->Notify();
        }
    }
    // Now dump audio to the device

    SYS_MEMCPY(stream, (short *)(mainBuffer_ + bufferPos_), len);
    bufferPos_ += len;
}

int SDLAudioDriver::GetPlayedBufferPercentage() {
    //	return
    // 100-(bufferSize_-bufferPos_-fragSize_)*100/(bufferSize_-fragSize_) ;
    return 0;
};

int SDLAudioDriver::GetSampleRate() {
    if (sampleRate_ == 0) {
        Trace::Error(
            "AUDIO",
            "Sample rate requested before audio driver is initialised!");
    }
    return sampleRate_;
};
