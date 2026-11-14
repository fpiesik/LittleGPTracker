// Uses the real SDL driver with paused dummy devices and a manually stepped
// worker/clock. Including the implementation also lets us feed synthetic input.
#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/SDL2/Audio/SDLAudioDriver.cpp"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include <cassert>
#include <cstdio>
#include <vector>

static Uint64 testCounter;
extern "C" Uint64 __wrap_SDL_GetPerformanceCounter() { return testCounter; }
extern "C" Uint64 __wrap_SDL_GetPerformanceFrequency() { return 1000000000; }
extern "C" void __wrap_SDL_PauseAudio(int) {}
extern "C" void __wrap_SDL_PauseAudioDevice(SDL_AudioDeviceID, int) {}

class TestSemaphore : public SysSemaphore {
  public:
    int pending;
    TestSemaphore() : pending(0) {}
    virtual SysSemaphoreResult Post() {
        ++pending;
        return SSR_NO_ERROR;
    }
    virtual SysSemaphoreResult Wait() { return SSR_NO_ERROR; }
    virtual SysSemaphoreResult TryWait() { return SSR_NO_ERROR; }
    virtual SysSemaphoreResult WaitTimeout(unsigned long) {
        return SSR_NO_ERROR;
    }
};
class TestProcess : public SysProcessFactory {
  public:
    TestSemaphore semaphore;
    virtual bool BeginThread(SysThread &) { return true; }
    virtual SysSemaphore *CreateNewSemaphore(int, int) { return &semaphore; }
    void Pump(SDLAudioDriver &driver) {
        while (semaphore.pending) {
            --semaphore.pending;
            driver.RenderBuffer();
        }
    }
};
class TestMidi : public MidiService {
    virtual void buildDriverList() {}
};
static int starts;
static void startSong() { ++starts; }
class TestSong : public I_Observer {
  public:
    SDLAudioDriver &driver;
    int frames;
    TestSong(SDLAudioDriver &d, int n) : driver(d), frames(n) {}
    virtual void Update(Observable &, I_ObservableData *data) {
        if (static_cast<AudioDriver::Event *>(data)->type_ !=
            AudioDriver::Event::ADET_BUFFERNEEDED)
            return;
        std::vector<short> pcm(frames * 2, starts ? 12345 : 0);
        driver.AddBuffer(&pcm[0], frames);
    }
};

static void run(int rate, int tempo, int callbackFrames, double latencyMs) {
    TestProcess process;
    SysProcessFactory::Install(&process);
    AudioSettings settings;
    settings.bufferSize_ = callbackFrames;
    settings.preBufferCount_ = 8;
    settings.sampleRate_ = rate;
    SDLAudioDriver driver(settings);
    assert(driver.Init());
    assert(driver.OpenCapture());
    assert(driver.Start());
    TestSong song(driver, rate * 60 / tempo / 24);
    driver.AddObserver(song);
    starts = 0;
    testCounter = 0;
    driver.BeginRecording(tempo, latencyMs, startSong);
    std::vector<short> output;
    std::vector<short> block(callbackFrames * 2);
    int firstClick = -1, firstSong = -1;
    for (int n = 0; firstSong < 0 && n < 3000; ++n) {
        testCounter = Uint64(double(n * callbackFrames) / rate * 1000000000);
        driver.OnChunkDone(reinterpret_cast<Uint8 *>(&block[0]),
                           callbackFrames * 4);
        for (int i = 0; i < callbackFrames; ++i) {
            assert(block[i * 2] == block[i * 2 + 1]);
            int frame = n * callbackFrames + i;
            if (firstClick < 0 && block[i * 2])
                firstClick = frame;
            if (firstSong < 0 && block[i * 2] == 12345)
                firstSong = frame;
            output.push_back(block[i * 2]);
        }
        assert(driver.RecordingBeat() != -2);
        process.Pump(driver);
    }
    assert(starts == 1 && firstClick >= 0 && firstSong > firstClick);
    // Independent rounded musical duration: the fifth beat is the first song
    // sample.
    int duration = int(240.0 * rate / tempo + 0.5);
    assert(firstSong - firstClick == duration);
    for (int beat = 0; beat < 4; ++beat) {
        int onset = firstClick + int(60.0 * rate * beat / tempo + 0.5);
        assert(output[onset] == -3000);
        if (beat)
            assert(output[onset - 1] == 0);
    }
    double latency = latencyMs >= 0
                         ? latencyMs / 1000.0
                         : double(callbackFrames) / rate + 1024.0 / 44100;
    double expectedTarget = double(firstSong) / rate + latency;
    assert(fabs(captureTarget - expectedTarget) < 0.000000002);

    // Input starts at a different callback phase. A numbered signal verifies
    // trimming inside a capture block, the correction sign, and PCM continuity.
    testCounter = 3000000; // 3 ms input/output callback phase difference.
    const int skipped = int(ceil((captureTarget - 0.003) * 44100));
    int inputFrame = 0;
    std::vector<short> inputBlock(1024);
    while (inputFrame < skipped + 3000) {
        for (int i = 0; i < 1024; ++i)
            inputBlock[i] = (inputFrame + i) % 30000;
        captureCallback(0, reinterpret_cast<Uint8 *>(&inputBlock[0]), 2048);
        inputFrame += 1024;
    }
    std::vector<short> recorded(inputFrame - skipped);
    assert(driver.ReadCapture(reinterpret_cast<Uint8 *>(&recorded[0]),
                              recorded.size() * 2) == int(recorded.size() * 2));
    for (unsigned i = 0; i < recorded.size(); ++i)
        assert(recorded[i] == (skipped + int(i)) % 30000);

    // Cancelling an armed start must prevent playback, including on a retry.
    std::vector<Uint8> oversized(captureCapacity + 2);
    captureCallback(0, &oversized[0], oversized.size());
    assert(driver.ReadCapture(&oversized[0], 2) == -1);
    driver.StopCapture();
    starts = 0;
    driver.BeginRecording(tempo, 0, startSong);
    driver.StopCapture();
    for (int i = 0; i < 4; ++i)
        driver.RenderBuffer();
    assert(starts == 0 && driver.RecordingBeat() == -1);
    driver.BeginRecording(tempo, -1, startSong);
    assert(captureSize == 0 && captureWaiting && captureTarget == 0);
    for (int i = 0; !captureTarget && i < 100; ++i) {
        driver.OnChunkDone(reinterpret_cast<Uint8 *>(&block[0]),
                           callbackFrames * 4);
        process.Pump(driver);
    }
    assert(captureTarget > 0);
    // Stall the producer after the first click: never return a valid take.
    for (int i = 0; i < 50 && driver.RecordingBeat() != -2; ++i)
        driver.OnChunkDone(reinterpret_cast<Uint8 *>(&block[0]),
                           callbackFrames * 4);
    assert(driver.RecordingBeat() == -2 && starts == 0);
    driver.StopCapture();
    assert(driver.ReadCapture(&oversized[0], 2) == -1);
    driver.RemoveObserver(song);
    driver.Stop();
    driver.CloseCapture();
    driver.Close();
}

int main() {
    captureChannels = 2;
    capturing = true;
    captureWaiting = false;
    captureSkip = 1;
    Uint16 stereo[] = {SDL_SwapLE16(100), SDL_SwapLE16(200),
                       SDL_SwapLE16(300), SDL_SwapLE16(400)};
    captureCallback(0, reinterpret_cast<Uint8 *>(stereo), sizeof(stereo));
    assert(captureSize == 4);
    assert(memcmp(captureBuffer, stereo + 2, 4) == 0);
    assert(inputPeak == ((300u << 16) | 400u));
    captureChannels = 1;
    capturing = false;
    captureRead = captureSize = inputPeak = 0;
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    assert(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0);
    System::Install(new LINUXSystem()); // Also used by global destructors.
    UnixFileSystem filesystem;
    FileSystem::Install(&filesystem);
    TestMidi midi;
    MidiService::Install(&midi);
    midi.Start();
    run(44100, 120, 1024, 75);
    run(44100, 137, 256, 0);
    run(48000, 300, 1024, -1);
    assert(RecordTiming::CaptureSkip(-0.01) == 0);
    assert(RecordTiming::CaptureSkip(0.1) == 4410);
    puts("record timing tests passed (synthetic audio, no hardware)");
    SDL_Quit();
}
