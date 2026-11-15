#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Instruments/PlaitsInstrument.h"
#include "Application/Instruments/SampleEditBuffer.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Mixer/ReverbBus.h"
#include "Application/Model/Mixer.h"
#include "Application/Player/PlayerChannel.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Views/ChainView.h"
#include "Application/Views/FxView.h"
#include "Application/Views/MixerView.h"
#include "Application/Views/SongView.h"
#include "Application/Views/UIController.h"
#include "Services/Audio/Audio.h"
#include "Services/Audio/AudioMixer.h"
#include "Services/Midi/MidiService.h"
#include "UIFramework/Interfaces/I_GUIWindowImp.h"
#include <cassert>
#include <cmath>
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

static long long Energy(const fixed *buffer, int count) {
    long long e = 0;
    for (int i = 0; i < count * 2; ++i) {
        assert(buffer[i] <= 32767 * FIXED_SCALE);
        assert(buffer[i] >= -32767 * FIXED_SCALE);
        e += std::abs(buffer[i]);
    }
    return e;
}

static void TestDSP() {
    const int count = 4096;
    fixed input[count * 2], whole[count * 2], sliced[count * 2];
    for (int i = 0; i < count; ++i) {
        input[2 * i] = (i % 83 < 41 ? 12000 : -12000) * FIXED_SCALE;
        input[2 * i + 1] = (i % 61 < 30 ? 8000 : -8000) * FIXED_SCALE;
    }
    const int rates[] = {44100, 48000};
    for (int r = 0; r < 2; ++r) {
        ReverbBus a, b;
        a.Init(rates[r]);
        b.Init(rates[r]);
        forbidAllocation = true;
        std::memcpy(whole, input, sizeof(input));
        a.BeginRender(count);
        a.Add(0, input, count, 0);
        assert(a.MixReturn(whole, count, true));
        assert(!a.IsActive());
        assert(!std::memcmp(whole, input, sizeof(input)));
        a.BeginRender(count);
        assert(!a.MixReturn(whole, count, false));

        a.BeginRender(count);
        a.Add(0, input, count, 255);
        assert(a.MixReturn(whole, count, false));
        for (int offset = 0; offset < count;) {
            int n = offset % 97 + 1;
            if (n > count - offset)
                n = count - offset;
            b.BeginRender(n);
            b.Add(0, input + offset * 2, n, 255);
            assert(b.MixReturn(sliced + offset * 2, n, false));
            offset += n;
        }
        for (int i = 0; i < count * 2; ++i) {
            if (whole[i] != sliced[i]) {
                std::fprintf(stderr, "rate %d diff %d: %d vs %d\n", rates[r], i,
                             whole[i], sliced[i]);
                break;
            }
        }
        assert(!std::memcmp(whole, sliced, sizeof(whole)));
        assert(Energy(whole, count) > 0);
        bool stereo = false;
        for (int i = 0; i < count; ++i)
            stereo |= whole[2 * i] != whole[2 * i + 1];
        assert(stereo);

        long long earlyTail = 0, lateTail = 0;
        for (int offset = 0; offset < rates[r] * 15; offset += 256) {
            a.BeginRender(256);
            if (a.MixReturn(whole, 256, false)) {
                long long e = Energy(whole, 256);
                if (offset < rates[r])
                    earlyTail += e;
                if (offset > rates[r] * 12)
                    lateTail += e;
            }
        }
        assert(earlyTail > 0 && lateTail < earlyTail);
        assert(!a.IsActive());
        // Exercise all parameter limits, in-flight edits, and eight full sends.
        a.SetParameters(999, -1, 999, 999, 255);
        for (int block = 0; block < 128; ++block) {
            a.BeginRender(256);
            for (int channel = 0; channel < SONG_CHANNEL_COUNT; ++channel)
                a.Add(channel, input, 256, 255);
            assert(a.MixReturn(whole, 256, false));
            Energy(whole, 256);
            if (block == 64)
                a.SetParameters(0, 255, 0, 0, 0);
        }
        assert(a.IsActive()); // LEVEL=00 must not reset the reverb tank.
        a.BeginRender(ReverbBus::MAX_FRAMES + 1);
        assert(a.MixReturn(whole, ReverbBus::MAX_FRAMES + 1, true));
        forbidAllocation = false;
    }
    std::puts("Clouds DSP: 44.1/48 kHz, bypass, stereo, tails, slicing, "
              "bounds, no allocations passed");
}

static void TestPersistence() {
    Mixer mixer;
    for (int i = 0; i < SONG_CHANNEL_COUNT; ++i)
        mixer.SetChannelSend(i, i * 31);
    for (int i = 0; i < Mixer::FX_COUNT; ++i)
        mixer.GetReverbParameter(i).SetInt(i * 41);
    TiXmlElement saved("MIXER");
    mixer.SaveContent(&saved);
    Mixer restored;
    restored.RestoreContent(&saved);
    for (int i = 0; i < SONG_CHANNEL_COUNT; ++i)
        assert(restored.GetChannelSend(i) == i * 31);
    for (int i = 0; i < Mixer::FX_COUNT; ++i)
        assert(restored.GetReverbParameter(i).GetInt() == i * 41);
    TiXmlDocument legacy;
    legacy.Parse(
        "<MIXER><VOL><DATA VALUE=\"128\" LENGTH=\"8\"/></VOL></MIXER>");
    restored.RestoreContent(legacy.RootElement());
    for (int i = 0; i < SONG_CHANNEL_COUNT; ++i) {
        assert(restored.GetChannelSend(i) == 0);
        assert(restored.GetChannelVolume(i) == 128);
    }
    assert(restored.GetReverbParameter(Mixer::FX_LEVEL).GetInt() == 96);
    TiXmlDocument invalid;
    invalid.Parse("<MIXER><SEND><DATA VALUE=\"255\" LENGTH=\"999\"/></SEND>"
                  "<REVERB DECAY=\"999\" DAMP=\"-1\"/></MIXER>");
    restored.RestoreContent(invalid.RootElement());
    assert(restored.GetChannelSend(0) == 0);
    assert(restored.GetReverbParameter(Mixer::FX_DECAY).GetInt() == 255);
    assert(restored.GetReverbParameter(Mixer::FX_DAMP).GetInt() == 0);
    std::puts("Clouds persistence: all sends/FX, old-project defaults and "
              "invalid values passed");
}

static void TestSharedMixer() {
    SampleEditBuffer *source = new SampleEditBuffer;
    source->channels = 2;
    source->rate = 44100;
    source->pcm.resize(8192);
    for (unsigned i = 0; i < source->pcm.size(); ++i)
        source->pcm[i] = i % 60 < 30 ? 9000 : -9000;
    SamplePool::GetInstance()->InstallEditedSample(0, strdup("clouds-test"),
                                                   source);
    SampleInstrument sampler;
    sampler.AssignSample(0);
    sampler.FindVariable(SIP_LOOPMODE)->SetInt(SILM_LOOP);
    PlaitsInstrument synth;
    PlaitsInstrument::InitVoices(44100);
    ReverbBus reverb;
    AudioMixer master("clouds-test");
    PlayerChannel sampleChannel(0), synthChannel(1);
    sampleChannel.SetReverbSend(&reverb, 0);
    synthChannel.SetReverbSend(&reverb, 255);
    master.SetSendEffect(&reverb);
    master.Insert(sampleChannel);
    master.Insert(synthChannel);
    fixed out[1024];
    sampleChannel.StartInstrument(&sampler, 60, true);
    assert(master.Render(out, 512));
    assert(Energy(out, 512) > 0 && !reverb.IsActive());
    sampleChannel.SetReverbSend(&reverb, 255);
    sampleChannel.SetVolume(0);
    assert(master.Render(out, 512));
    assert(!reverb.IsActive()); // Send is post-volume.
    sampleChannel.SetVolume(FP_ONE);
    sampleChannel.SetHPFMode(1);
    sampleChannel.SetLPFFreq(8000);
    assert(master.Render(out, 512));
    assert(reverb.IsActive());
    sampleChannel.StopInstrument();
    reverb.Init(44100);
    synthChannel.StartInstrument(&synth, 64, true);
    synthChannel.SetMute(true);
    assert(!master.Render(out, 512));
    assert(!reverb.IsActive());
    synthChannel.SetMute(false);
    for (int i = 0; i < 16; ++i)
        assert(master.Render(out, 512));
    assert(reverb.IsActive()); // Plaits uses the same return.
    sampleChannel.StartInstrument(&sampler, 60, true);
    assert(master.Render(out, 512));
    assert(Energy(out, 512) > 0);
    sampleChannel.StopInstrument();
    synthChannel.StopInstrument();
    long long tail = 0;
    for (int i = 0; i < 64; ++i) {
        assert(master.Render(out, 512));
        tail += Energy(out, 512);
    }
    assert(tail > 0);
    std::puts(
        "Shared sample/Plaits mixer: independent sends, volume/filter/mute, "
        "simultaneous playback and note-off tails passed");
}

class LoudChannel : public AudioModule {
  public:
    fixed value;
    bool Render(fixed *buffer, int frames) {
        for (int i = 0; i < frames * 2; ++i)
            buffer[i] = value;
        return true;
    }
};

static void TestOverload() {
    AudioMixer mixer("overload-test");
    LoudChannel channels[8];
    for (int i = 0; i < 8; ++i)
        mixer.Insert(channels[i]);
    fixed out[2];
    for (int sign = -1; sign <= 1; sign += 2) {
        for (int i = 0; i < 8; ++i)
            channels[i].value = sign * 30000 * FIXED_SCALE;
        assert(mixer.Render(out, 1));
        assert(mixer.Clipped());
        assert(out[0] == (sign > 0 ? 32767 : -32768) * FIXED_SCALE);
    }
    std::puts("Eight-channel overload clips without integer wraparound");
}

class TestWindowImp : public I_GUIWindowImp {
  public:
    void Clear(GUIColor &, bool) {}
    void SetColor(GUIColor &) {}
    void ClearRect(GUIRect &) {}
    void DrawString(const char *, GUIPoint &, GUITextProperties &, bool) {}
    void DrawChar(char, GUIPoint &, GUITextProperties &) {}
    GUIRect GetRect() { return GUIRect(0, 0, 320, 240); }
    void Invalidate() {}
    void Lock() {}
    void Unlock() {}
    void Flush() {}
    void PushEvent(GUIEvent &) {}
};
class TestWindow : public GUIWindow {
  public:
    TestWindow() : GUIWindow(*new TestWindowImp) {}
    void onUpdate() {}
    bool onEvent(GUIEvent &) { return false; }
};
class Navigation : public I_Observer {
  public:
    ViewType destination;
    void Update(Observable &, I_ObservableData *data) {
        ViewEvent *event = static_cast<ViewEvent *>(data);
        assert(event->GetType() == VET_SWITCH_VIEW);
        destination = *static_cast<ViewType *>(event->GetData());
    }
};
class TestMidi : public MidiService {
    void buildDriverList() {}
};
class TestAudio : public Audio {
  public:
    TestAudio(AudioSettings &s) : Audio(s) {}
    void Init() {}
    void Close() {}
};

template <class T>
static void GestureButton(void *view, unsigned short mask, bool pressed) {
    static_cast<T *>(view)->ProcessButtonMask(mask, pressed);
}

static void TestViews() {
    Mixer::GetInstance()->SetChannelSend(0, 255);
    ViewData data(new Project);
    assert(Mixer::GetInstance()->GetChannelSend(0) == 0);
    data.songX_ = 0;
    TestWindow window;
    MixerView mixer(window, &data);
    mixer.OnFocus();
    for (int i = 0; i < 3; ++i)
        mixer.ProcessButtonMask(EPBM_DOWN, true);
    for (int channel = 0; channel < 8; ++channel) {
        mixer.ProcessButtonMask(EPBM_A | EPBM_UP, true);
        mixer.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        mixer.ProcessButtonMask(EPBM_A | EPBM_RIGHT, false);
        assert(Mixer::GetInstance()->GetChannelSend(channel) == 17);
        mixer.ProcessButtonMask(EPBM_RIGHT, true);
    }
    mixer.ProcessButtonMask(EPBM_B | EPBM_A, true);
    assert(Mixer::GetInstance()->GetChannelSend(7) == 0);
    // Cutting either filter must leave volume and the other filter untouched.
    Mixer *model = Mixer::GetInstance();
    model->SetChannelVolume(7, 73);
    model->SetChannelHPF(7, 2);
    model->SetChannelLPF(7, 1000);
    mixer.ProcessButtonMask(EPBM_UP, true); // LPF
    mixer.ProcessButtonMask(EPBM_B | EPBM_A, true);
    assert(model->GetChannelLPF(7) == 0);
    assert(model->GetChannelHPF(7) == 2);
    assert(model->GetChannelVolume(7) == 73);
    model->SetChannelLPF(7, 1000);
    mixer.ProcessButtonMask(EPBM_UP, true); // HPF
    mixer.ProcessButtonMask(EPBM_B | EPBM_A, true);
    assert(model->GetChannelHPF(7) == 0);
    assert(model->GetChannelLPF(7) == 1000);
    assert(model->GetChannelVolume(7) == 73);
    mixer.ProcessButtonMask(EPBM_UP, true); // Volume
    mixer.ProcessButtonMask(EPBM_B | EPBM_A, true);
    assert(model->GetChannelVolume(7) == 255);
    Navigation navigation;
    mixer.AddObserver(navigation);
    mixer.ProcessButtonMask(EPBM_R | EPBM_DOWN, true);
    assert(navigation.destination == VT_FX);
    assert(data.songX_ == 7);
    // Like mute, solo is temporary if A is released while R stays held.
    // Releasing R first latches the change.
    UIController *controller = UIController::GetInstance();
    Player *player = Player::GetInstance();
    controller->UnMuteAll();
    player->SetChannelMute(3, true);
    SongView song(window, &data, "solo-test");
    data.songX_ = 2;
    for (int attempt = 0; attempt < 2; ++attempt) {
        song.ProcessButtonMask(EPBM_R | EPBM_A, true);
        song.ProcessButtonMask(EPBM_A, false);
        song.ProcessButtonMask(0, false);
        for (int ch = 0; ch < 8; ++ch)
            assert(player->IsChannelMuted(ch) ==
                   (attempt == 0 ? ch != 2 : ch == 3));
    }
    // Temporary solo restores the pre-gesture state.
    song.ProcessButtonMask(EPBM_R | EPBM_A, true);
    song.ProcessButtonMask(EPBM_R, false);
    for (int ch = 0; ch < 8; ++ch)
        assert(player->IsChannelMuted(ch) == (ch == 3));
    song.ProcessButtonMask(0, false);
    song.ProcessButtonMask(EPBM_R | EPBM_A, true);
    song.ProcessButtonMask(EPBM_A, false);
    data.mixerCol_ = 4;
    mixer.ProcessButtonMask(EPBM_R | EPBM_A, true);
    for (int ch = 0; ch < 8; ++ch)
        assert(player->IsChannelMuted(ch) == (ch != 4));
    mixer.ProcessButtonMask(EPBM_A, false);
    mixer.ProcessButtonMask(EPBM_R | EPBM_A, true);
    for (int ch = 0; ch < 8; ++ch)
        assert(player->IsChannelMuted(ch) == (ch == 3));
    mixer.ProcessButtonMask(EPBM_A, false);
    mixer.ProcessButtonMask(EPBM_R | EPBM_A, true);
    mixer.ProcessButtonMask(EPBM_R, false);
    for (int ch = 0; ch < 8; ++ch)
        assert(player->IsChannelMuted(ch) == (ch == 3));
    mixer.ProcessButtonMask(0, false);
    controller->SwitchSoloMode(1, 2);
    controller->SwitchSoloMode(1, 2);
    assert(player->IsChannelMuted(3));
    controller->UnMuteAll();
    std::puts("Solo toggle: release, original mutes, channel/view switches and "
              "selection passed");
    ChainView chain(window, &data);
    void *gestureViews[] = {&song, &chain, &mixer};
    typedef void (*ButtonHandler)(void *, unsigned short, bool);
    ButtonHandler handlers[] = {GestureButton<SongView>,
                                GestureButton<ChainView>,
                                GestureButton<MixerView>};
    data.songX_ = data.mixerCol_ = 2;
    const unsigned short actions[] = {EPBM_B, EPBM_A};
    for (int v = 0; v < 3; ++v) {
        for (int action = 0; action < 2; ++action) {
            controller->UnMuteAll();

            unsigned short button = actions[action];
            // Releasing B/A first restores the original mute mask.
            handlers[v](gestureViews[v], EPBM_R | button, true);
            for (int ch = 0; ch < 8; ++ch)
                assert(player->IsChannelMuted(ch) ==
                       (action == 0 ? ch == 2 : ch != 2));
            handlers[v](gestureViews[v], EPBM_R, false);
            handlers[v](gestureViews[v], 0, false);
            for (int ch = 0; ch < 8; ++ch)
                assert(!player->IsChannelMuted(ch));
            // Releasing X first latches; repeating the gesture undoes it.
            for (int attempt = 0; attempt < 2; ++attempt) {
                handlers[v](gestureViews[v], EPBM_R | button, true);
                handlers[v](gestureViews[v], button, false);
                handlers[v](gestureViews[v], 0, false);
                for (int ch = 0; ch < 8; ++ch)
                    assert(player->IsChannelMuted(ch) ==
                           (attempt == 0 && (action == 0 ? ch == 2 : ch != 2)));
            }
        }
    }
    std::puts("Mute/Solo temporary and latched gestures passed in Song, Chain, "
              "and Mixer");
    FxView fx(window, &data);
    for (int i = 0; i < Mixer::FX_COUNT; ++i) {
        int before = Mixer::GetInstance()->GetReverbParameter(i).GetInt();
        fx.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(Mixer::GetInstance()->GetReverbParameter(i).GetInt() ==
               before + 1);
        fx.ProcessButtonMask(EPBM_DOWN, true);
    }
    fx.AddObserver(navigation);
    fx.ProcessButtonMask(EPBM_R | EPBM_UP, true);
    assert(navigation.destination == VT_MIXER);
    std::puts(
        "Mixer SEND editing and E page controls/navigation passed (headless)");
}

int main() {
    TestDSP();
    System::Install(new LINUXSystem);
    UnixFileSystem fs;
    FileSystem::Install(&fs);
    TestMidi midi;
    MidiService::Install(&midi);
    AudioSettings settings;
    settings.bufferSize_ = 256;
    settings.preBufferCount_ = 2;
    settings.sampleRate_ = 44100;
    TestAudio audio(settings);
    Audio::Install(&audio);
    SyncMaster::GetInstance()->SetTempo(120);
    SyncMaster::GetInstance()->Start();
    TestPersistence();
    TestSharedMixer();
    TestOverload();
    TestViews();
}
