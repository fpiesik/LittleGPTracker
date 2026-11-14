#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Audio/AudioFileStreamer.h"
#include "Services/Audio/Audio.h"
#include <cassert>
#include <cmath>
#include <cstdio>

class TestAudio : public Audio {
  public:
    TestAudio(AudioSettings &s) : Audio(s) {}
    void Init() {}
    void Close() {}
    int GetSampleRate() { return 48000; }
};
int main(int argc, char **argv) {
    assert(argc == 2);
    System::Install(new LINUXSystem());
    UnixFileSystem fs;
    FileSystem::Install(&fs);
    AudioSettings settings;
    settings.bufferSize_ = 256;
    settings.preBufferCount_ = 2;
    settings.sampleRate_ = 48000;
    TestAudio audio(settings);
    Audio::Install(&audio);
    std::string base = std::string(argv[1]) + "/";
    WavFile *w = WavFile::Open((base + "stereo.wav").c_str());
    assert(w && w->GetSize(-1) == 3 && w->GetChannelCount(-1) == 2);
    assert(w->GetBuffer(0, 3));
    short expected[] = {-32768, 32512, 0, -16384, 16384, 256};
    short *pcm = static_cast<short *>(w->GetSampleBuffer(-1));
    for (int i = 0; i < 6; ++i)
        assert(pcm[i] == expected[i]);
    assert(!w->GetBuffer(-1, 1) && !w->GetBuffer(2, 2));
    delete w;
    const char *invalid[] = {"truncated", "channels", "rate",    "align",
                             "missing",   "empty",    "oversize"};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i)
        assert(!WavFile::Open((base + invalid[i] + ".wav").c_str()));
    // A 24 kHz ramp becomes six frames at 48 kHz, including the held last
    // frame.
    AudioFileStreamer stream;
    stream.Start(Path(base + "ramp.wav"));
    fixed output[8];
    assert(stream.Render(output, 2));
    assert(std::fabs(fp2fl(output[0])) < 0.01);
    assert(std::fabs(fp2fl(output[2]) - 125) < 0.01);
    assert(stream.Render(output, 4));
    float want[] = {250, 375, 500, 500};
    for (int i = 0; i < 4; ++i) {
        assert(std::fabs(fp2fl(output[2 * i]) - want[i]) < 0.01);
        assert(output[2 * i] == output[2 * i + 1]);
    }
    assert(!stream.Render(output, 1));
    int rates[] = {44100, 96000};
    for (int r = 0; r < 2; ++r) {
        char name[32];
        sprintf(name, "%d.wav", rates[r]);
        stream.Start(Path(base + name));
        int total = int(ceil(100.0 * 48000 / rates[r]));
        for (int offset = 0; offset < total; offset += 4) {
            assert(stream.Render(output, 4));
            for (int i = 0; i < 4; ++i) {
                double position = double(offset + i) * rates[r] / 48000;
                double expected = offset + i < total
                                      ? (position < 99 ? position : 99) * 25
                                      : 0;
                assert(std::fabs(fp2fl(output[2 * i]) - expected) < 0.01);
            }
        }
        assert(!stream.Render(output, 1));
    }
    puts("WAV tests passed (synthetic files/audio, no hardware)");
}
