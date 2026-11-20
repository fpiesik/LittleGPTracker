#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Instruments/SampleEditBuffer.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Player/PlayerChannel.h"
#include "Application/Player/SyncMaster.h"
#include "Services/Audio/Audio.h"
#include <cassert>
#include <cstdio>
#include <cstring>

class TestAudio : public Audio {
  public:
    TestAudio(AudioSettings &s) : Audio(s) {}
    void Init() {}
    void Close() {}
    int GetSampleRate() { return 48000; }
};

int main() {
    System::Install(new LINUXSystem());
    UnixFileSystem fs;
    FileSystem::Install(&fs);
    AudioSettings settings;
    settings.bufferSize_ = 256;
    settings.preBufferCount_ = 2;
    settings.sampleRate_ = 48000;
    TestAudio audio(settings);
    Audio::Install(&audio);
    SyncMaster::GetInstance()->SetTempo(120);
    SyncMaster::GetInstance()->Start();
    for (int channels = 1; channels <= 2; ++channels) {
        SampleEditBuffer *source = new SampleEditBuffer;
        source->channels = channels;
        source->rate = 48000;
        for (int frame = 0; frame < 4096; ++frame)
            for (int ch = 0; ch < channels; ++ch)
                source->pcm.push_back((frame < 2048 ? 10000 : -10000) *
                                      (ch ? -1 : 1));
        SamplePool::GetInstance()->InstallEditedSample(0, strdup("step"),
                                                       source);
        SampleInstrument instrument;
        instrument.AssignSample(0);
        instrument.FindVariable(SIP_LOOPMODE)->SetInt(SILM_LOOP);
        PlayerChannel channel(0);
        fixed out[256];
        channel.StartInstrument(&instrument, 60, true);
        assert(channel.Render(out, 128));
        fixed previous[2] = {out[254], out[255]};
        instrument.ProcessCommand(0, I_CMD_PLOF, 0x8000);
        assert(channel.Render(out, 128));
        assert(out[0] == previous[0] && out[1] == previous[1]);
        assert(out[254] * static_cast<long long>(previous[0]) < 0);
        instrument.ProcessCommand(0, I_CMD_PLOF, 0);
        assert(!instrument.ConsumePositionJump(0));
        instrument.ProcessCommand(0, I_CMD_RTRG, 1);
        assert(!instrument.ConsumePositionJump(0)); // Setup is not a jump.
        assert(channel.Render(out, 1));
        previous[0] = out[0];
        previous[1] = out[1];
        assert(channel.Render(out, 128)); // First actual repeat.
        assert(out[0] == previous[0] && out[1] == previous[1]);
        assert(out[254] * static_cast<long long>(previous[0]) < 0);
        instrument.ProcessCommand(0, I_CMD_RTRG, 0);
        instrument.FindVariable(SIP_SLICES)->SetInt(2);
        previous[0] = out[254];
        previous[1] = out[255];
        channel.StartInstrument(&instrument, 13, false);
        assert(channel.Render(out, 128));
        assert(out[0] == previous[0] && out[1] == previous[1]);
        assert(out[254] * static_cast<long long>(previous[0]) < 0);
        channel.StopInstrument();
        assert(!channel.Render(out, 128));
        channel.StartInstrument(&instrument, 12, true);
        assert(channel.Render(out, 128));
        assert(out[0] == 0 && out[1] == 0); // No stale audio after silence.
    }
    puts("PLOF, RTRG and slice tests passed (synthetic audio, no hardware)");
}
