#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/PlaitsEngine.h"
#include "Application/Instruments/PlaitsInstrument.h"
#include "Application/Instruments/SampleEditBuffer.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Persistency/PersistencyService.h"
#include "Application/Player/PlayerChannel.h"
#include "Application/Player/SyncMaster.h"
#include "Services/Audio/Audio.h"
#include "Services/Audio/AudioMixer.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

static bool forbidAllocation = false;
extern "C" void *__real_malloc(size_t);
extern "C" void *__real_calloc(size_t, size_t);
extern "C" void *__real_realloc(void *, size_t);
extern "C" void *__wrap_malloc(size_t size) {
    assert(!forbidAllocation);
    return __real_malloc(size);
}
extern "C" void *__wrap_calloc(size_t count, size_t size) {
    assert(!forbidAllocation);
    return __real_calloc(count, size);
}
extern "C" void *__wrap_realloc(void *ptr, size_t size) {
    assert(!forbidAllocation);
    return __real_realloc(ptr, size);
}
void *operator new(size_t size) throw(std::bad_alloc) {
    assert(!forbidAllocation);
    void *p = std::malloc(size);
    if (!p)
        throw std::bad_alloc();
    return p;
}
void *operator new[](size_t size) throw(std::bad_alloc) {
    return ::operator new(size);
}
void operator delete(void *p) throw() { std::free(p); }
void operator delete[](void *p) throw() { std::free(p); }

static long long Energy(const fixed *buffer, int frames) {
    long long sum = 0;
    for (int i = 0; i < frames * 2; ++i)
        sum += std::abs(buffer[i]);
    return sum;
}

static double MeasurePitch(PlaitsEngine &engine, int rate) {
    fixed buffer[512];
    // Skip oscillator/LPG attack and parameter smoothing.
    for (int i = 0; i < 16; ++i)
        assert(engine.Render(buffer, 256));
    double first = 0.0, last = 0.0;
    int crossings = 0;
    fixed previous = 0;
    for (int offset = 0; offset < rate / 2; offset += 256) {
        assert(engine.Render(buffer, 256));
        for (int i = 0; i < 256; ++i) {
            fixed value = buffer[i * 2];
            if (previous < 0 && value >= 0) {
                last = offset + i - double(value) / (value - previous);
                if (!crossings)
                    first = last;
                ++crossings;
            }
            previous = value;
        }
    }
    assert(crossings > 10);
    return (crossings - 1) * rate / (last - first);
}

static void TestEngine() {
    const int count = 4096;
    fixed whole[count * 2], sliced[count * 2];
    const int rates[] = {32000, 44100, 48000, 96000};
    for (unsigned r = 0; r < sizeof(rates) / sizeof(rates[0]); ++r) {
        PlaitsEngine a, b;
        a.Init(rates[r]);
        b.Init(rates[r]);
        forbidAllocation = true;
        a.SetParameters(8, 128, 128, 128, 255);
        b.SetParameters(8, 128, 128, 128, 255);
        a.NoteOn(69);
        b.NoteOn(69);
        assert(a.Render(whole, count));
        int offset = 0;
        while (offset < count) {
            int n = (offset % 79) + 1;
            if (n > count - offset)
                n = count - offset;
            assert(b.Render(sliced + offset * 2, n));
            offset += n;
        }
        assert(!std::memcmp(whole, sliced, sizeof(whole)));
        assert(Energy(whole, count) > 0);
        for (int i = 0; i < count; ++i) {
            assert(whole[i * 2] == whole[i * 2 + 1]);
            assert(std::abs(whole[i * 2]) <= 16384 * FIXED_SCALE);
        }
        a.NoteOff();
        assert(!a.Render(whole, count));
        assert(!a.Render(whole, 0));
        // Exercise every engine and repeated engine switches in one arena.
        for (int model = 0; model < 24; ++model) {
            a.SetParameters(model, 128, 128, 128, 128);
            a.NoteOn(60);
            assert(a.Render(whole, count));
            assert(Energy(whole, count) > 0);
            a.SetPitch(72);
            assert(a.Render(whole, count));
        }
        a.SetParameters(999, -1, 999, -1, 999);
        assert(a.Render(whole, count));
        a.SetParameters(9, 128, 0, 0, 255);
        a.NoteOn(69);
        double pitch = MeasurePitch(a, rates[r]);
        assert(pitch > 439.0 && pitch < 441.0);
        a.SetPitch(81);
        pitch = MeasurePitch(a, rates[r]);
        assert(pitch > 878.0 && pitch < 882.0);
        forbidAllocation = false;
    }
    std::printf("Adapter: 24 models, 4 rates, slicing, stop, bounds, no "
                "allocations; %lu bytes/voice\n",
                static_cast<unsigned long>(sizeof(PlaitsEngine)));
}

class TestAudio : public Audio {
  public:
    TestAudio(AudioSettings &settings) : Audio(settings) {}
    void Init() {}
    void Close() {}
    int GetSampleRate() { return 44100; }
};

static void TestInstrument() {
    PersistencyDocument document("");
    document.version_ = 999;
    document.Parse("<INSTRUMENTBANK><INSTRUMENT ID=\"00\" TYPE=\"PLTS\">"
                   "<PARAM NAME=\"MODEL\" VALUE=\"8\"/>"
                   "<PARAM NAME=\"DECAY\" VALUE=\"255\"/>"
                   "</INSTRUMENT></INSTRUMENTBANK>");
    assert(!document.Error());
    InstrumentBank bank;
    bank.RestoreContent(document.RootElement());
    bank.Init();
    PlaitsInstrument::InitVoices(Audio::GetInstance()->GetSampleRate());
    I_Instrument *instrument = bank.GetInstrument(0);
    assert(instrument->GetType() == IT_PLAITS);
    assert(instrument->IsInitialized());
    assert(bank.GetInstrument(1)->GetType() == IT_SAMPLE);
    assert(bank.GetInstrument(MAX_SAMPLEINSTRUMENT_COUNT)->GetType() ==
           IT_MIDI);
    assert(bank.GetNext() == 1);
    unsigned short clone = bank.Clone(0);
    assert(clone == 1 && bank.GetInstrument(clone)->GetType() == IT_PLAITS);
    TiXmlElement saved("INSTRUMENTBANK");
    bank.SaveContent(&saved);
    const TiXmlElement *first = saved.FirstChildElement();
    assert(!std::strcmp(first->Attribute("TYPE"), "PLTS"));
    PersistencyDocument restored("");
    restored.version_ = 999;
    restored.InsertEndChild(saved);
    instrument->FindVariable("DECAY")->SetInt(0);
    bank.RestoreContent(restored.RootElement());
    assert(instrument->FindVariable("DECAY")->GetInt() == 255);

    fixed buffer[4096 * 2];
    PlayerChannel channel(0), other(1);
    forbidAllocation = true;
    channel.StartInstrument(instrument, 60, true);
    other.StartInstrument(instrument, 72, true);
    assert(channel.Render(buffer, 4096));
    assert(Energy(buffer, 4096) > 0);
    channel.StopInstrument();
    assert(!channel.Render(buffer, 4096));
    assert(other.Render(buffer, 4096));
    assert(Energy(buffer, 4096) > 0);
    instrument->ProcessCommand(1, I_CMD_VOLM, 0);
    assert(other.Render(buffer, 4096));
    assert(Energy(buffer, 4096) == 0);
    other.SetMute(true);
    assert(!other.Render(buffer, 4096));
    other.SetMute(false);
    other.StopInstrument();
    forbidAllocation = false;

    // Real mixer path, with a sample and synth sounding together. The existing
    // mixer allocates scratch memory, so the allocation trap is off here.
    SampleEditBuffer *sample = new SampleEditBuffer;
    sample->channels = 1;
    sample->rate = 44100;
    sample->pcm.assign(8192, 1000);
    SamplePool::GetInstance()->InstallEditedSample(0, strdup("plaits-test"),
                                                   sample);
    SampleInstrument sampler;
    sampler.AssignSample(0);
    AudioMixer mixer("plaits-test");
    mixer.Insert(channel);
    mixer.Insert(other);
    channel.StartInstrument(instrument, 69, true);
    other.StartInstrument(&sampler, 60, true);
    assert(mixer.Render(buffer, 4096));
    assert(Energy(buffer, 4096) > 0);
    assert(!mixer.Clipped());
    channel.StopInstrument();
    other.StopInstrument();
    std::puts("Instrument: XML, clone, channel isolation, volume, mute, "
              "sample+synth mixer passed");
}

int main() {
    TestEngine();
    System::Install(new LINUXSystem());
    UnixFileSystem fs;
    FileSystem::Install(&fs);
    AudioSettings settings;
    settings.bufferSize_ = 256;
    settings.preBufferCount_ = 2;
    settings.sampleRate_ = 44100;
    TestAudio audio(settings);
    Audio::Install(&audio);
    SyncMaster::GetInstance()->SetTempo(120);
    SyncMaster::GetInstance()->Start();
    TestInstrument();
}
