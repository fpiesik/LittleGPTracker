#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/SDL2/Audio/SDLAudioDriver.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Model/Mixer.h"
#include "Application/Views/BaseClasses/UIIntVarField.h"
#include "Application/Views/FxView.h"
#include "Application/Views/ProjectView.h"
#include "Application/Views/RecordOptionsView.h"
#include "Services/Audio/Audio.h"
#include "Services/Midi/MidiService.h"
#include "UIFramework/Interfaces/I_GUIWindowImp.h"
#include <cassert>
#include <vector>

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

// Capture the real handler's playback request without starting audio.
static int playCalls;
static bool playFromStart;
extern "C" void
__wrap__ZN6Player13OnStartButtonE8PlayModejbh(Player *, PlayMode mode,
                                              unsigned int from, bool fromStart,
                                              unsigned char to) {
    assert(mode == PM_SONG && from == 3 && to == 3);
    ++playCalls;
    playFromStart = fromStart;
}

// Frozen pre-migration handler for behavioral comparison, including its
// modifier precedence and delegation to the shared field operations.
class LegacyFxView : public FxView {
  public:
    LegacyFxView(GUIWindow &w, ViewData *d) : FxView(w, d) {}
    void ProcessButtonMask(unsigned short mask, bool pressed) {
        if (!pressed)
            return;
        if (mask & EPBM_R) {
            if (mask & EPBM_UP) {
                ViewType type = VT_MIXER;
                ViewEvent event(VET_SWITCH_VIEW, &type);
                SetChanged();
                NotifyObservers(&event);
            } else if (mask & EPBM_START) {
                Player::GetInstance()->OnStartButton(PM_SONG, viewData_->songX_,
                                                     true, viewData_->songX_);
            }
            return;
        }
        if (mask & EPBM_B) {
            if ((mask & EPBM_A) && GetFocus()) {
                static_cast<UIIntVarField *>(GetFocus())->GetVariable().Reset();
                isDirty_ = true;
            }
            return;
        }
        if (mask & EPBM_START) {
            Player::GetInstance()->OnStartButton(PM_SONG, viewData_->songX_,
                                                 false, viewData_->songX_);
            return;
        }
        FieldView::ProcessButtonMask(mask);
    }
};
class SwitchObserver : public I_Observer {
  public:
    int calls;
    ViewType destination;
    explicit SwitchObserver(ViewType type = VT_MIXER) : calls(0), destination(type) {}
    void Update(Observable &, I_ObservableData *data) {
        ViewEvent *event = static_cast<ViewEvent *>(data);
        assert(event->GetType() == VET_SWITCH_VIEW);
        assert(*static_cast<ViewType *>(event->GetData()) == destination);
        ++calls;
    }
};
class TestRecordOptionsView : public RecordOptionsView {
  public:
    bool projectHighlighted;
    ColorDefinition color;
    TestRecordOptionsView(GUIWindow &window, ViewData *data)
        : RecordOptionsView(window, data), projectHighlighted(false),
          color(CD_NORMAL) {}
    void SetColor(ColorDefinition value) { color = value; }
    void DrawString(int x, int y, const char *text, GUITextProperties &props) {
        if (color == CD_HILITE2) {
            GUIPoint anchor = GetAnchor();
            assert(x == anchor._x + SONG_CHANNEL_COUNT * 3 + 1 && y == 26);
            assert(std::string(text) == "P" && props.invert_);
            projectHighlighted = true;
        }
    }
    void DrawMap() { drawMap(); }
};
static std::vector<UIField *> Fields(FxView &view) {
    std::vector<UIField *> result;
    IteratorPtr<UIField> it(view.T_SimpleList<UIField>::GetIterator());
    for (it->Begin(); !it->IsDone(); it->Next())
        result.push_back(&it->CurrentItem());
    return result;
}
static bool Load(Shortcuts &shortcuts, const char *xml) {
    TiXmlDocument document;
    document.Parse(xml);
    assert(!document.Error());
    std::string error;
    return shortcuts.Load(document.RootElement(), error);
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
    Shortcuts &shortcuts = Shortcuts::Get();
    ViewData data(new Project);
    data.songX_ = 3;
    TestWindow window;
    FxView view(window, &data);
    LegacyFxView legacy(window, &data);
    std::vector<UIField *> fields = Fields(view), oldFields = Fields(legacy);
    SwitchObserver currentSwitch, oldSwitch;
    view.AddObserver(currentSwitch);
    legacy.AddObserver(oldSwitch);
    Mixer *mixer = Mixer::GetInstance();
    const int values[] = {0, 100, 255};
    // All states, releases, all five fields, and boundary values. Also repeat
    // with the compiled fallback defaults rather than just the shipped XML.
    for (int configuration = 0; configuration < 2; ++configuration) {
        if (configuration)
            assert(Load(shortcuts, "<SHORTCUTS version='1'/>"));
        for (int row = 0; row < Mixer::FX_COUNT; ++row)
            for (unsigned v = 0; v < sizeof(values) / sizeof(values[0]); ++v)
                for (unsigned mask = 0; mask < 1024; ++mask)
                    for (int pressed = 0; pressed < 2; ++pressed) {
                        int expected[Mixer::FX_COUNT];
                        for (int i = 0; i < Mixer::FX_COUNT; ++i)
                            mixer->GetReverbParameter(i).SetInt(values[v]);
                        legacy.SetFocus(oldFields[row]);
                        oldSwitch.calls = playCalls = 0;
                        playFromStart = false;
                        legacy.ProcessButtonMask(mask, pressed);
                        int expectedPlayCalls = playCalls;
                        bool expectedFromStart = playFromStart;
                        int expectedFocus = legacy.GetFocusIndex();
                        for (int i = 0; i < Mixer::FX_COUNT; ++i) {
                            expected[i] = mixer->GetReverbParameter(i).GetInt();
                            mixer->GetReverbParameter(i).SetInt(values[v]);
                        }
                        view.SetFocus(fields[row]);
                        currentSwitch.calls = playCalls = 0;
                        playFromStart = false;
                        view.ProcessButtonMask(mask, pressed);
                        assert(view.GetFocusIndex() == expectedFocus);
                        assert(currentSwitch.calls == oldSwitch.calls);
                        assert(playCalls == expectedPlayCalls);
                        assert(playFromStart == expectedFromStart);
                        for (int i = 0; i < Mixer::FX_COUNT; ++i)
                            assert(mixer->GetReverbParameter(i).GetInt() ==
                                   expected[i]);
                    }
    }
    view.SetFocus(fields[0]);
    mixer->GetReverbParameter(0).SetInt(100);
    view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
    view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true); // repeat
    view.ProcessButtonMask(EPBM_A, false);
    assert(mixer->GetReverbParameter(0).GetInt() == 102);
    assert(Load(
        shortcuts,
        "<SHORTCUTS version='1'>"
        "<BIND action='fx.increase_fine' keys='L+RIGHT' event='press'/>"
        "<BIND action='fx.reset' keys='SELECT+A' exclude='R' event='press'/>"
        "<BIND action='fx.down' keys='SELECT+DOWN' event='press'/>"
        "</SHORTCUTS>"));
    view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
    assert(mixer->GetReverbParameter(0).GetInt() == 102);
    view.ProcessButtonMask(EPBM_L | EPBM_RIGHT, true);
    assert(mixer->GetReverbParameter(0).GetInt() == 103);
    view.ProcessButtonMask(EPBM_B | EPBM_A, true);
    assert(mixer->GetReverbParameter(0).GetInt() == 103);
    view.ProcessButtonMask(EPBM_SELECT | EPBM_A, true);
    assert(mixer->GetReverbParameter(0).GetInt() == 160);
    view.ProcessButtonMask(EPBM_DOWN, true);
    assert(view.GetFocusIndex() == 0);
    view.ProcessButtonMask(EPBM_SELECT | EPBM_DOWN, true);
    assert(view.GetFocusIndex() == 1);
    assert(!Load(shortcuts,
                 "<SHORTCUTS version='1'>"
                 "<BIND action='fx.reset' keys='B+A' event='release'/>"
                 "</SHORTCUTS>"));
    assert(shortcuts.Matches(Shortcuts::FxReset, EPBM_SELECT | EPBM_A, true));
    view.RemoveObserver(currentSwitch);
    legacy.RemoveObserver(oldSwitch);
    ProjectView project(window, &data);
    project.ProcessButtonMask(EPBM_DOWN, true);
    assert(project.GetFocusIndex() == 1);
    Variable &master =
        static_cast<UIIntVarField *>(project.GetFocus())->GetVariable();
    master.SetInt(50);
    project.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
    assert(master.GetInt() == 51);
    assert(
        Load(shortcuts,
             "<SHORTCUTS version='1'>"
             "<BIND action='project.edit_right' keys='L+RIGHT' event='press'/>"
             "</SHORTCUTS>"));
    project.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
    project.ProcessButtonMask(EPBM_L | EPBM_RIGHT, false);
    assert(master.GetInt() == 51);
    project.ProcessButtonMask(EPBM_L | EPBM_RIGHT, true);
    assert(master.GetInt() == 52);
    SwitchObserver songSwitch(VT_SONG);
    project.AddObserver(songSwitch);
    playCalls = 0;
    project.ProcessButtonMask(EPBM_R | EPBM_DOWN | EPBM_START, true);
    assert(songSwitch.calls == 1 && playCalls == 0);
    project.ProcessButtonMask(EPBM_START, true);
    assert(playCalls == 1 && !playFromStart);
    project.RemoveObserver(songSwitch);

    // Record Options is a separate view: ordinary A/B and bare arrows no
    // longer edit or exit. Exercise shipped XML and compiled defaults.
    for (int configuration = 0; configuration < 2; ++configuration) {
        if (configuration == 0) {
            TiXmlDocument document("projects/shortcuts.xml");
            assert(document.LoadFile());
            std::string error;
            assert(shortcuts.Load(document.RootElement(), error));
        } else {
            assert(Load(shortcuts, "<SHORTCUTS version='1'/>"));
        }
        TestRecordOptionsView record(window, &data);
        record.SetFocus(VT_RECORD_OPTIONS);
        record.DrawMap();
        assert(record.projectHighlighted);
        SwitchObserver back(VT_PROJECT);
        record.AddObserver(back);
        SDLAudioDriver::RecordChannels = 1;
        const unsigned short inactive[] = {EPBM_A, EPBM_B, EPBM_LEFT,
                                           EPBM_RIGHT};
        for (unsigned i = 0; i < sizeof(inactive) / sizeof(inactive[0]); ++i)
            record.ProcessButtonMask(inactive[i], true);
        assert(SDLAudioDriver::RecordChannels == 1 && back.calls == 0);
        record.ProcessButtonMask(EPBM_A | EPBM_RIGHT, false);
        assert(SDLAudioDriver::RecordChannels == 1);
        record.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(SDLAudioDriver::RecordChannels == 2);
        record.ProcessButtonMask(EPBM_A | EPBM_LEFT, true);
        assert(SDLAudioDriver::RecordChannels == 1);
        record.ProcessButtonMask(EPBM_DOWN, true);
        SDLAudioDriver::RecordMetronome = 0;
        record.ProcessButtonMask(EPBM_A | EPBM_LEFT, true);
        assert(SDLAudioDriver::RecordMetronome == 2);
        record.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(SDLAudioDriver::RecordMetronome == 0);
        record.ProcessButtonMask(EPBM_DOWN, true);
        SDLAudioDriver::RecordMetronomeVolume = 100;
        record.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(SDLAudioDriver::RecordMetronomeVolume == 100);
        record.ProcessButtonMask(EPBM_A | EPBM_LEFT, true);
        assert(SDLAudioDriver::RecordMetronomeVolume == 90);
        SDLAudioDriver::RecordMetronomeVolume = 0;
        record.ProcessButtonMask(EPBM_A | EPBM_LEFT, true);
        assert(SDLAudioDriver::RecordMetronomeVolume == 0);
        record.ProcessButtonMask(EPBM_DOWN, true);
        SDLAudioDriver::RecordCountIn = 8;
        record.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(SDLAudioDriver::RecordCountIn == 0);
        record.ProcessButtonMask(EPBM_A | EPBM_LEFT, true);
        assert(SDLAudioDriver::RecordCountIn == 8);
        record.ProcessButtonMask(EPBM_R | EPBM_LEFT, false);
        assert(back.calls == 0);
        record.ProcessButtonMask(EPBM_R | EPBM_LEFT, true);
        assert(back.calls == 1 && SDLAudioDriver::RecordCountIn == 8);
        assert(Load(shortcuts,
                    "<SHORTCUTS version='1'>"
                    "<BIND action='record_options.back' keys='SELECT+LEFT' "
                    "event='press'/></SHORTCUTS>"));
        record.ProcessButtonMask(EPBM_R | EPBM_LEFT, true);
        assert(back.calls == 1);
        record.ProcessButtonMask(EPBM_SELECT | EPBM_LEFT, true);
        assert(back.calls == 2);
        record.RemoveObserver(back);
    }
    return 0;
}
