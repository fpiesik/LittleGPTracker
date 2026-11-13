#include "SDLAudioDriver.h"
#include "Services/Midi/MidiService.h"
#include "Services/Time/TimeService.h"
#include "System/Console/Trace.h"
#include "System/System/System.h"

// The UI requests clicks; synthesis and phase state stay on the audio thread.
static SDL_atomic_t countInRequest;
void SDLAudioDriver::CountInClick(bool enabled) {
    SDL_AtomicSet(&countInRequest, enabled ? 1 : -1);
}

namespace {
SDL_AudioDeviceID captureDevice = 0;
// Four seconds of mono PCM. Idle input is discarded, never queued indefinitely.
const unsigned captureCapacity = 44100 * 2 * 4;
Uint8 captureBuffer[captureCapacity];
unsigned captureRead = 0, captureSize = 0;
bool capturing = false, captureOverflow = false;

void captureCallback(void *, Uint8 *stream, int length) {
    if (!capturing || length <= 0)
        return;
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

SDL_AudioDeviceID SDLAudioDriver::OpenCapture() {
    if (captureDevice &&
        SDL_GetAudioDeviceStatus(captureDevice) != SDL_AUDIO_STOPPED)
        return captureDevice;
    CloseCapture();
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) &&
        SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return 0;
    SDL_AudioSpec wanted = {}, obtained = {};
    wanted.freq = 44100;
    wanted.format = AUDIO_S16LSB;
    wanted.channels = 1;
    wanted.samples = 1024;
    wanted.callback = captureCallback;
    SDL_SetHint("SDL_AUDIO_DEVICE_APP_NAME", "LittleGPTracker");
    captureDevice = SDL_OpenAudioDevice(NULL, 1, &wanted, &obtained, 0);
    if (captureDevice) {
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
    captureRead = captureSize = 0;
}

void SDLAudioDriver::StartCapture() {
    if (!captureDevice)
        return;
    SDL_LockAudioDevice(captureDevice);
    captureRead = captureSize = 0;
    captureOverflow = false;
    capturing = true;
    SDL_UnlockAudioDevice(captureDevice);
}

void SDLAudioDriver::StopCapture() {
    if (!captureDevice)
        return;
    SDL_LockAudioDevice(captureDevice);
    capturing = false;
    SDL_UnlockAudioDevice(captureDevice);
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
        driver_->OnNewBufferNeeded();
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
}

SDLAudioDriver::~SDLAudioDriver() {}

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

    // Look if we have enough data in main buffer

    while (bufferSize_ - bufferPos_ < len) {

        // First move remaining bytes at the front
        memmove(mainBuffer_, mainBuffer_ + bufferPos_,
                bufferSize_ - bufferPos_);

        // then get next queued buffer and copy data from it

        if (pool_[poolPlayPosition_].buffer_ == 0) {
            // Use fragSize_ instead of len! miniBlank_ is only allocated to
            // fragSize_. Using len causes a buffer over-read if SDL requests a
            // larger chunk.
            SYS_MEMCPY(mainBuffer_ + bufferSize_ - bufferPos_, miniBlank_,
                       fragSize_);
            bufferSize_ = bufferSize_ - bufferPos_ + fragSize_;
            bufferPos_ = 0;
        } else {

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

    static int clickRemaining = 0;
    int request = SDL_AtomicSet(&countInRequest, 0);
    int clickLength = sampleRate_ / 20;
    if (request)
        clickRemaining = request > 0 ? clickLength : 0;
    short *output = reinterpret_cast<short *>(stream);
    for (int frame = 0; frame < len / 4 && clickRemaining > 0; ++frame) {
        int phase = clickLength - clickRemaining;
        int click = ((phase * 2000 / sampleRate_) & 1) ? 3000 : -3000;
        click = click * clickRemaining / clickLength;
        for (int channel = 0; channel < 2; ++channel) {
            int mixed = output[frame * 2 + channel] + click;
            output[frame * 2 + channel] =
                mixed > 32767 ? 32767 : (mixed < -32768 ? -32768 : mixed);
        }
        --clickRemaining;
    }
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
