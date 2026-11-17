#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Model/Mixer.h"
#include "Application/Views/MixerView.h"
#include "Application/Views/UIController.h"
#include "Services/Audio/Audio.h"
#include "Services/Midi/MidiService.h"
#include "UIFramework/Interfaces/I_GUIWindowImp.h"
#include <cassert>
#include <fstream>
#include <sstream>

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

class TestMidi : public MidiService {
    void buildDriverList() {}
};

class TestAudio : public Audio {
  public:
    TestAudio(AudioSettings &settings) : Audio(settings) {}
    void Init() {}
    void Close() {}
};

static bool Load(Shortcuts &shortcuts, const char *xml) {
    TiXmlDocument document;
    document.Parse(xml);
    assert(!document.Error());
    std::string error;
    return shortcuts.Load(document.RootElement(), error);
}

// Reference the original nested branches, independently of the XML table.
static unsigned Legacy(unsigned m, bool pressed) {
    if (!pressed)
        return (m & EPBM_R) ? (1u << 20) | (1u << 21) : 0;
    unsigned result = 0;
    if (m & EPBM_R) {
        if (m & EPBM_B)
            result |= 1u << 0;
        else if (m & EPBM_A)
            result |= 1u << 1;
        else if (m & EPBM_L)
            result |= 1u << 2;
        else if (m & EPBM_UP)
            result |= 1u << 3;
        else if (m & EPBM_DOWN)
            result |= 1u << 4;
        else if (m & EPBM_RIGHT)
            result |= 1u << 5;
        else if (m & EPBM_START)
            result |= 1u << 6;
    } else if (m & EPBM_B) {
        if (m & EPBM_A)
            result |= 1u << 7;
    } else if (m & EPBM_A) {
        result |= 1u << 8;
        if (m & EPBM_UP)
            result |= 1u << 9;
        if (m & EPBM_DOWN)
            result |= 1u << 10;
        if (m & EPBM_RIGHT)
            result |= 1u << 11;
        if (m & EPBM_LEFT)
            result |= 1u << 12;
    } else {
        if (m & EPBM_UP)
            result |= 1u << 13;
        if (m & EPBM_DOWN)
            result |= 1u << 14;
        if (m & EPBM_LEFT)
            result |= 1u << 15;
        if (m & EPBM_RIGHT)
            result |= 1u << 16;
        if (m & EPBM_START)
            result |= 1u << 17;
    }
    // Selection mode has its own playback branches.
    if (!(m & (EPBM_A | EPBM_B)) && (m & EPBM_START))
        result |= (m & EPBM_R) ? 1u << 18 : 1u << 19;
    return result;
}
static unsigned Resolve(const Shortcuts &s, unsigned m, bool pressed) {
    unsigned result = 0;
    for (int i = Shortcuts::MixerMute; i <= Shortcuts::MixerSoloRelease; ++i)
        if (s.Matches(static_cast<Shortcuts::Action>(i), m, pressed))
            result |= 1u << (i - Shortcuts::MixerMute);
    return result;
}

int main() {
    System::Install(new LINUXSystem);
    FileSystem::Install(new UnixFileSystem);
    Path::SetAlias("bin", "projects");
    TestMidi midi;
    MidiService::Install(&midi);
    AudioSettings settings;
    settings.bufferSize_ = 256;
    settings.preBufferCount_ = 2;
    settings.sampleRate_ = 44100;
    TestAudio audio(settings);
    Audio::Install(&audio);
    Shortcuts defaults;
    Shortcuts &configured = Shortcuts::Get();
    for (unsigned m = 0; m < 1024; ++m)
        for (int pressed = 0; pressed < 2; ++pressed) {
            assert(Resolve(defaults, m, pressed) == Legacy(m, pressed));
            assert(Resolve(configured, m, pressed) == Legacy(m, pressed));
        }

    ViewData data(new Project);
    TestWindow window;
    Mixer *mix = Mixer::GetInstance();
    Player *player = Player::GetInstance();
    UIController::GetInstance()->Init(data.project_, &data);
    // Exercise the real handler: volume, HPF, LPF, send, and clearing each.
    {
        MixerView view(window, &data);
        mix->SetChannelVolume(0, 100);
        view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(mix->GetChannelVolume(0) == 101);
        view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true); // autorepeat
        assert(mix->GetChannelVolume(0) == 102);
        view.ProcessButtonMask(EPBM_A, false);
        assert(mix->GetChannelVolume(0) == 102);
        view.ProcessButtonMask(EPBM_A | EPBM_B, true);
        assert(mix->GetChannelVolume(0) == 255);
        view.ProcessButtonMask(EPBM_DOWN, true);
        mix->SetChannelHPF(0, 0);
        view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(mix->GetChannelHPF(0) == 1);
        view.ProcessButtonMask(EPBM_A | EPBM_B, true);
        assert(mix->GetChannelHPF(0) == 0);
        view.ProcessButtonMask(EPBM_DOWN, true);
        mix->SetChannelLPF(0, 1000);
        view.ProcessButtonMask(EPBM_A | EPBM_UP, true);
        assert(mix->GetChannelLPF(0) == 1100);
        view.ProcessButtonMask(EPBM_A | EPBM_B, true);
        assert(mix->GetChannelLPF(0) == 0);
        view.ProcessButtonMask(EPBM_DOWN, true);
        mix->SetChannelSend(0, 100);
        view.ProcessButtonMask(EPBM_A | EPBM_UP, true);
        assert(mix->GetChannelSend(0) == 116);
        view.ProcessButtonMask(EPBM_A | EPBM_B, true);
        assert(mix->GetChannelSend(0) == 0);
        view.ProcessButtonMask(EPBM_RIGHT, true);
        assert(data.mixerCol_ == 1);
        view.ProcessButtonMask(EPBM_LEFT, true);
        assert(data.mixerCol_ == 0);
    }
    // Preserve row-specific handling of simultaneous value directions.
    for (int row = 1; row <= 4; ++row) {
        MixerView view(window, &data);
        for (int i = 1; i < row; ++i)
            view.ProcessButtonMask(EPBM_DOWN, true);
        for (unsigned directions = 0; directions < 16; ++directions) {
            mix->SetChannelVolume(0, 100);
            mix->SetChannelHPF(0, 1);
            mix->SetChannelLPF(0, 1000);
            mix->SetChannelSend(0, 100);
            int volume = 100, send = 100, lpf = 1000, hpf = 1;
            if (directions & EPBM_UP) {
                volume = 116;
                send += 16;
            }
            if (directions & EPBM_DOWN) {
                volume = 84;
                send -= 16;
            }
            if (directions & EPBM_RIGHT) {
                volume = 101;
                send += 1;
            }
            if (directions & EPBM_LEFT) {
                volume = 99;
                send -= 1;
            }
            if (directions & EPBM_UP)
                lpf = 1100;
            else if (directions & EPBM_DOWN)
                lpf = 900;
            else if (directions & EPBM_RIGHT)
                lpf = 1010;
            else if (directions & EPBM_LEFT)
                lpf = 990;
            if (directions & EPBM_RIGHT)
                hpf = 2;
            else if (directions & EPBM_LEFT)
                hpf = 0;
            view.ProcessButtonMask(EPBM_A | directions, true);
            assert(mix->GetChannelVolume(0) == (row == 1 ? volume : 100));
            assert(mix->GetChannelHPF(0) == (row == 2 ? hpf : 1));
            assert(mix->GetChannelLPF(0) == (row == 3 ? lpf : 1000));
            assert(mix->GetChannelSend(0) == (row == 4 ? send : 100));
        }
    }
    // Both release orders: action button first is temporary; R first latches.
    for (int solo = 0; solo < 2; ++solo) {
        for (int latch = 0; latch < 2; ++latch) {
            UIController::GetInstance()->Reset();
            UIController::GetInstance()->UnMuteAll();
            if (solo)
                player->SetChannelMute(2, true);
            MixerView view(window, &data);
            unsigned button = solo ? EPBM_A : EPBM_B;
            int observed = solo ? 1 : 0;
            view.ProcessButtonMask(EPBM_R, true);
            view.ProcessButtonMask(EPBM_R | button, true);
            assert(player->IsChannelMuted(observed));
            view.ProcessButtonMask(latch ? button : EPBM_R, false);
            assert(player->IsChannelMuted(observed) == bool(latch));
            view.ProcessButtonMask(0, false);
            assert(player->IsChannelMuted(observed) == bool(latch));
            if (solo)
                assert(player->IsChannelMuted(2)); // restore pre-solo mask
        }
    }
    // Remap Mute and its release modifier together, preserving the gesture.
    assert(Load(
        configured,
        "<SHORTCUTS version='1'>"
        "<BIND action='mixer.mute' keys='L+B' event='press'/>"
        "<BIND action='mixer.mute_release' keys='L' event='release'/>"
        "<BIND action='mixer.increase_fine' keys='SELECT+RIGHT' event='press'/>"
        "</SHORTCUTS>"));
    {
        UIController::GetInstance()->UnMuteAll();
        MixerView view(window, &data);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        assert(player->IsChannelMuted(0));
        view.ProcessButtonMask(EPBM_L, false);
        assert(!player->IsChannelMuted(0));
        mix->SetChannelVolume(0, 100);
        view.ProcessButtonMask(EPBM_SELECT | EPBM_RIGHT, true);
        assert(mix->GetChannelVolume(0) == 101);
        assert(data.mixerCol_ ==
               0); // edit consumes the event before navigation
        view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(mix->GetChannelVolume(0) ==
               101); // old chord no longer increments
    }
    assert(!Load(configured,
                 "<SHORTCUTS version='1'>"
                 "<BIND action='mixer.mute_release' keys='L' event='press'/>"
                 "</SHORTCUTS>"));
    assert(configured.Matches(Shortcuts::MixerMuteRelease, EPBM_L, false));
    assert(!Load(configured,
                 "<SHORTCUTS version='1'>"
                 "<BIND action='mixer.clear' keys='A+B' event='release'/>"
                 "</SHORTCUTS>"));
    UIController::GetInstance()->Reset();
    return 0;
}
