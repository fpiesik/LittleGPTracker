#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Views/ChainView.h"
#include "Application/Views/UIController.h"
#include "Services/Audio/Audio.h"
#include "Services/Midi/MidiService.h"
#include "UIFramework/Interfaces/I_GUIWindowImp.h"
#include <cassert>
#include <cstring>
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

static bool Load(Shortcuts &shortcuts, const char *xml) {
    TiXmlDocument document;
    document.Parse(xml);
    assert(!document.Error());
    std::string error;
    return shortcuts.Load(document.RootElement(), error);
}

typedef std::vector<Shortcuts::Action> Actions;
static Actions Legacy(unsigned m, bool pressed, bool selection) {
    Actions result;
    if (!pressed)
        return result;
    if (!selection) {
        if (m & EPBM_B) {
            if (m & EPBM_LEFT)
                result.push_back(Shortcuts::ChainPreviousChannel);
            if (m & EPBM_RIGHT)
                result.push_back(Shortcuts::ChainNextChannel);
            if (m & EPBM_UP)
                result.push_back(Shortcuts::ChainPreviousChain);
            if (m & EPBM_DOWN)
                result.push_back(Shortcuts::ChainNextChain);
            if (m & EPBM_A)
                result.push_back(Shortcuts::ChainCut);
            if (m & EPBM_L)
                result.push_back(Shortcuts::ChainBeginCloneSelection);
            if (m & EPBM_R)
                result.push_back(Shortcuts::ChainMute);
        } else if (m & EPBM_A) {
            if (m & EPBM_DOWN)
                result.push_back(Shortcuts::ChainDecreaseCoarse);
            if (m & EPBM_UP)
                result.push_back(Shortcuts::ChainIncreaseCoarse);
            if (m & EPBM_LEFT)
                result.push_back(Shortcuts::ChainDecreaseFine);
            if (m & EPBM_RIGHT)
                result.push_back(Shortcuts::ChainIncreaseFine);
            if (m & EPBM_L)
                result.push_back(Shortcuts::ChainPaste);
            if (m == EPBM_A)
                result.push_back(Shortcuts::ChainInsert);
            if (m & EPBM_R)
                result.push_back(Shortcuts::ChainSolo);
        } else if (m & EPBM_R) {
            if (m & EPBM_LEFT)
                result.push_back(Shortcuts::ChainSong);
            if (m & EPBM_RIGHT)
                result.push_back(Shortcuts::ChainPhrase);
            if (m & EPBM_DOWN)
                result.push_back(Shortcuts::ChainMixer);
            if (m & EPBM_START)
                result.push_back(Shortcuts::ChainPlayFromStart);
            if (m & EPBM_L)
                result.push_back(Shortcuts::ChainUnmuteAll);
        } else if (!(m & EPBM_L)) {
            if (m & EPBM_DOWN)
                result.push_back(Shortcuts::ChainDown);
            if (m & EPBM_UP)
                result.push_back(Shortcuts::ChainUp);
            if (m & EPBM_LEFT)
                result.push_back(Shortcuts::ChainLeft);
            if (m & EPBM_RIGHT)
                result.push_back(Shortcuts::ChainRight);
            if (m & EPBM_START)
                result.push_back(Shortcuts::ChainPlay);
        }
    } else {
        if (m & EPBM_B) {
            if (m == EPBM_B)
                result.push_back(Shortcuts::ChainSelectionCopy);
            if (m & EPBM_R)
                result.push_back(Shortcuts::ChainSelectionMute);
            if (m & EPBM_L)
                result.push_back(Shortcuts::ChainSelectionExtend);
        } else if (m & EPBM_A) {
            if (m & EPBM_DOWN)
                result.push_back(Shortcuts::ChainSelectionDecreaseCoarse);
            if (m & EPBM_UP)
                result.push_back(Shortcuts::ChainSelectionIncreaseCoarse);
            if (m & EPBM_LEFT)
                result.push_back(Shortcuts::ChainSelectionDecreaseFine);
            if (m & EPBM_RIGHT)
                result.push_back(Shortcuts::ChainSelectionIncreaseFine);
            if (m & EPBM_L)
                result.push_back(Shortcuts::ChainSelectionCut);
            if (m & EPBM_R)
                result.push_back(Shortcuts::ChainSelectionSolo);
        } else if (m & EPBM_R) {
            if (m & EPBM_LEFT)
                result.push_back(Shortcuts::ChainSelectionSong);
            if (m & EPBM_RIGHT)
                result.push_back(Shortcuts::ChainSelectionPhrase);
            if (m & EPBM_DOWN)
                result.push_back(Shortcuts::ChainSelectionMixer);
            if (m & EPBM_START)
                result.push_back(Shortcuts::ChainSelectionPlayFromStart);
            if (m & EPBM_L)
                result.push_back(Shortcuts::ChainSelectionUnmuteAll);
        } else {
            if (m & EPBM_DOWN)
                result.push_back(Shortcuts::ChainSelectionDown);
            if (m & EPBM_UP)
                result.push_back(Shortcuts::ChainSelectionUp);
            if (m & EPBM_LEFT)
                result.push_back(Shortcuts::ChainSelectionLeft);
            if (m & EPBM_RIGHT)
                result.push_back(Shortcuts::ChainSelectionRight);
            if (m & EPBM_START)
                result.push_back(Shortcuts::ChainSelectionPlay);
        }
    }
    return result;
}
static Actions Resolve(const Shortcuts &s, unsigned m, bool pressed,
                       bool selection) {
    Actions result;
    int first = selection ? Shortcuts::ChainSelectionCopy
                          : Shortcuts::ChainPreviousChannel;
    int last = selection ? Shortcuts::ChainSelectionPlay : Shortcuts::ChainPlay;
    for (int i = first; i <= last; ++i) {
        Shortcuts::Action action = static_cast<Shortcuts::Action>(i);
        if (s.Matches(action, m, pressed))
            result.push_back(action);
    }
    return result;
}
class TestChain : public ChainView {
  public:
    TestChain(GUIWindow &w, ViewData *d) : ChainView(w, d) {
        SetFocus(VT_CHAIN);
    }
    ViewMode Mode() { return viewMode_; }
    GUIRect Selection() { return getSelectionRect(); }
};
class SwitchObserver : public I_Observer {
  public:
    std::vector<ViewType> views;
    void Update(Observable &, I_ObservableData *d) {
        ViewEvent *event = static_cast<ViewEvent *>(d);
        assert(event->GetType() == VET_SWITCH_VIEW);
        views.push_back(*static_cast<ViewType *>(event->GetData()));
    }
};
static int playCalls;
static bool playFromStart;
extern "C" void
__wrap__ZN6Player13OnStartButtonE8PlayModejbh(Player *, PlayMode mode,
                                              unsigned int from, bool fromStart,
                                              unsigned char to) {
    assert(mode == PM_CHAIN && from == 0 && to == 0);
    ++playCalls;
    playFromStart = fromStart;
}
static void Reset(ViewData &data) {
    data.chainRow_ = data.chainCol_ = data.currentChain_ = data.songX_ = 0;
    std::memset(data.song_->chain_->data_, 0xff, CHAIN_COUNT * 16);
    std::memset(data.song_->chain_->transpose_, 0, CHAIN_COUNT * 16);
    data.song_->phrase_->ClearAllocation();
    data.song_->phrase_->SetUsed(0);
    UIController::GetInstance()->Reset();
    UIController::GetInstance()->UnMuteAll();
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
    for (unsigned mask = 0; mask < 1024; ++mask)
        for (int pressed = 0; pressed < 2; ++pressed) {
            for (int selection = 0; selection < 2; ++selection) {
                assert(Resolve(defaults, mask, pressed, selection) ==
                       Legacy(mask, pressed, selection));
                assert(Resolve(configured, mask, pressed, selection) ==
                       Legacy(mask, pressed, selection));
            }
            assert(configured.Matches(Shortcuts::ChainNextUnused, mask,
                                      pressed) == (pressed && mask == EPBM_A));
            assert(configured.Matches(Shortcuts::ChainClone, mask, pressed) ==
                   (pressed && (mask & EPBM_A) && (mask & EPBM_L)));
            assert(configured.Matches(Shortcuts::ChainMuteRelease, mask,
                                      pressed) ==
                   (!pressed && (mask & EPBM_R)));
            assert(configured.Matches(Shortcuts::ChainSoloRelease, mask,
                                      pressed) ==
                   (!pressed && (mask & EPBM_R)));
            assert(configured.Matches(Shortcuts::ChainEditHold, mask,
                                      pressed) == (pressed && (mask & EPBM_A)));
        }
    ViewData data(new Project);
    TestWindow window;
    unsigned char *chain = data.song_->chain_->data_;
    Phrase *phrase = data.song_->phrase_;
    Reset(data);
    {
        TestChain view(window, &data);
        view.ProcessButtonMask(EPBM_A, true);
        assert(chain[0] == 0 && view.Mode() == VM_NEW);
        view.ProcessButtonMask(0, false);
        assert(view.Mode() == VM_NEW);
        view.ProcessButtonMask(EPBM_A, true);
        assert(chain[0] == 1 && phrase->IsUsed(1) && view.Mode() == VM_NORMAL);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_A, true);
        assert(chain[1] == 1); // remembered phrase, not another allocation
    }
    Reset(data);
    chain[0] = 0;
    for (int i = 0; i < 16; ++i) {
        phrase->note_[i] = 30 + i;
        phrase->instr_[i] = i;
        phrase->velocity_[i] = 70 + i;
        phrase->cmd1_[i] = 1234 + i;
        phrase->param1_[i] = 2345 + i;
        phrase->cmd2_[i] = 3456 + i;
        phrase->param2_[i] = 4567 + i;
    }
    {
        TestChain view(window, &data);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        assert(view.Mode() == VM_CLONE);
        view.ProcessButtonMask(EPBM_L, false);
        assert(view.Mode() == VM_CLONE);
        view.ProcessButtonMask(EPBM_L | EPBM_A, true);
        assert(chain[0] == 1 && view.Mode() == VM_NORMAL);
        assert(!std::memcmp(phrase->note_, phrase->note_ + 16, 16));
        assert(!std::memcmp(phrase->instr_, phrase->instr_ + 16, 16));
        assert(!std::memcmp(phrase->velocity_, phrase->velocity_ + 16, 16));
        assert(!std::memcmp(phrase->cmd1_, phrase->cmd1_ + 16,
                            16 * sizeof(FourCC)));
        assert(!std::memcmp(phrase->cmd2_, phrase->cmd2_ + 16,
                            16 * sizeof(FourCC)));
        assert(!std::memcmp(phrase->param1_, phrase->param1_ + 16,
                            16 * sizeof(ushort)));
        assert(!std::memcmp(phrase->param2_, phrase->param2_ + 16,
                            16 * sizeof(ushort)));
    }
    Reset(data);
    chain[0] = 1;
    chain[1] = 2;
    {
        TestChain view(window, &data);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        view.ProcessButtonMask(EPBM_DOWN,
                               true); // non-clone event starts selection
        assert(view.Mode() == VM_SELECTION && view.Selection().Bottom() == 1);
        view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(chain[0] == 2 && chain[1] == 3);
        view.ProcessButtonMask(EPBM_B, true);
        assert(view.Mode() == VM_NORMAL && data.chainRow_ == 0);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_L | EPBM_A, true);
        assert(chain[2] == 2 && chain[3] == 3 && data.chainRow_ == 4);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        view.ProcessButtonMask(EPBM_DOWN, true);
        chain[4] = 6;
        chain[5] = 7;
        view.ProcessButtonMask(EPBM_L | EPBM_A, true);
        assert(chain[4] == 255 && chain[5] == 255 && data.chainRow_ == 4);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_L | EPBM_A, true);
        assert(chain[6] == 6 && chain[7] == 7);
    }
    Reset(data);
    {
        TestChain view(window, &data);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        assert(view.Mode() == VM_SELECTION && view.Selection().Right() == 1);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        assert(view.Selection().Top() == 0 && view.Selection().Bottom() == 15);
    }
    Reset(data);
    chain[0] = 0;
    {
        TestChain view(window, &data);
        view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(chain[0] == 1 && !phrase->IsUsed(1));
        view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(chain[0] == 2 && !phrase->IsUsed(2));
        view.ProcessButtonMask(
            0, false); // original behavior commits on next press
        assert(!phrase->IsUsed(2));
        view.ProcessButtonMask(EPBM_DOWN, true);
        assert(phrase->IsUsed(2) && !phrase->IsUsed(1));
    }
    for (int solo = 0; solo < 2; ++solo)
        for (int latch = 0; latch < 2; ++latch) {
            Reset(data);
            TestChain view(window, &data);
            Player *player = Player::GetInstance();
            if (solo)
                player->SetChannelMute(2, true);
            unsigned button = solo ? EPBM_A : EPBM_B;
            int observed = solo ? 1 : 0;
            view.ProcessButtonMask(EPBM_R, true);
            view.ProcessButtonMask(EPBM_R | button, true);
            assert(player->IsChannelMuted(observed));
            view.ProcessButtonMask(latch ? button : EPBM_R, false);
            view.ProcessButtonMask(0, false);
            assert(player->IsChannelMuted(observed) == bool(latch));
            if (solo)
                assert(player->IsChannelMuted(2));
        }
    for (int selection = 0; selection < 2; ++selection) {
        Reset(data);
        TestChain view(window, &data);
        if (selection) {
            view.ProcessButtonMask(EPBM_L | EPBM_B, true);
            view.ProcessButtonMask(0, true);
        }
        playCalls = 0;
        view.ProcessButtonMask(EPBM_START, true);
        assert(playCalls == 1 && !playFromStart);
        view.ProcessButtonMask(EPBM_R | EPBM_START, true);
        assert(playCalls == 2 && playFromStart);
    }
    Reset(data);
    {
        TestChain view(window, &data);
        data.songY_ = data.songOffset_ = 0;
        data.song_->data_[0] = 0;
        data.song_->data_[1] = 4;
        data.song_->data_[SONG_CHANNEL_COUNT] = 5;
        view.ProcessButtonMask(EPBM_B | EPBM_RIGHT, true);
        assert(data.songX_ == 1 && data.currentChain_ == 4);
        view.ProcessButtonMask(EPBM_B | EPBM_LEFT, true);
        assert(data.songX_ == 0 && data.currentChain_ == 0);
        view.ProcessButtonMask(EPBM_B | EPBM_DOWN, true);
        assert(data.songY_ == 1 && data.currentChain_ == 5);
        view.ProcessButtonMask(EPBM_B | EPBM_UP, true);
        assert(data.songY_ == 0 && data.currentChain_ == 0);
        view.ProcessButtonMask(EPBM_RIGHT, true);
        view.ProcessButtonMask(EPBM_A | EPBM_UP, true);
        assert(data.song_->chain_->transpose_[0] == 12);
        view.ProcessButtonMask(EPBM_A | EPBM_DOWN, true);
        assert(data.song_->chain_->transpose_[0] == 0);
    }
    for (int selection = 0; selection < 2; ++selection) {
        Reset(data);
        TestChain view(window, &data);
        SwitchObserver observer;
        view.AddObserver(observer);
        if (selection) {
            view.ProcessButtonMask(EPBM_L | EPBM_B, true);
            view.ProcessButtonMask(0, true);
        }
        view.ProcessButtonMask(EPBM_R | EPBM_RIGHT, true);
        assert(observer.views.empty()); // empty phrase does not open a view
        chain[0] = 7;
        view.ProcessButtonMask(EPBM_R | EPBM_RIGHT, true);
        assert(observer.views.back() == VT_PHRASE && data.currentPhrase_ == 7);
        view.ProcessButtonMask(EPBM_R | EPBM_LEFT, true);
        assert(observer.views.back() == VT_SONG);
        view.ProcessButtonMask(EPBM_R | EPBM_DOWN, true);
        assert(observer.views.back() == VT_MIXER);
        view.RemoveObserver(observer);
    }
    // Exhausted allocation must consume the gesture without changing the row.
    Reset(data);
    chain[0] = 0;
    for (int i = 0; i < PHRASE_COUNT; ++i)
        phrase->SetUsed(i);
    {
        TestChain view(window, &data);
        view.ProcessButtonMask(EPBM_A, true);
        view.ProcessButtonMask(EPBM_A, true);
        assert(chain[0] == 0 && view.Mode() == VM_NORMAL);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        view.ProcessButtonMask(EPBM_L | EPBM_A, true);
        assert(chain[0] == 0 && view.Mode() == VM_NORMAL);
    }
    assert(Load(configured,
                "<SHORTCUTS version='1'>"
                "<BIND action='chain.insert' keys='SELECT' event='press'/>"
                "<BIND action='chain.next_unused' keys='SELECT' event='press'/>"
                "<BIND action='chain.begin_clone_selection' keys='SELECT+B' "
                "event='press'/>"
                "<BIND action='chain.clone' keys='SELECT+A' event='press'/>"
                "</SHORTCUTS>"));
    Reset(data);
    {
        TestChain view(window, &data);
        view.ProcessButtonMask(EPBM_SELECT, true);
        assert(chain[0] == 0 && view.Mode() == VM_NEW);
        view.ProcessButtonMask(0, false);
        view.ProcessButtonMask(EPBM_SELECT, true);
        assert(chain[0] == 1 &&
               view.Mode() == VM_NORMAL); // consumed, no re-insert
    }
    // Avoid overlaps with the SELECT insert binding while testing clone.
    assert(Load(configured,
                "<SHORTCUTS version='1'>"
                "<BIND action='chain.begin_clone_selection' keys='SELECT+B' "
                "event='press'/>"
                "<BIND action='chain.clone' keys='SELECT+A' event='press'/>"
                "</SHORTCUTS>"));
    Reset(data);
    chain[0] = 0;
    {
        TestChain view(window, &data);
        view.ProcessButtonMask(EPBM_SELECT | EPBM_B, true);
        assert(view.Mode() == VM_CLONE);
        view.ProcessButtonMask(EPBM_SELECT, false);
        view.ProcessButtonMask(EPBM_SELECT | EPBM_A | EPBM_RIGHT, true);
        assert(chain[0] == 1 && view.Mode() == VM_NORMAL);
        assert(data.chainCol_ ==
               1); // residual RIGHT remains a navigation action
        assert(data.song_->chain_->transpose_[0] == 0);
    }
    assert(configured.RemainingButtons(Shortcuts::ChainClone,
                                       EPBM_SELECT | EPBM_A | EPBM_RIGHT) ==
           EPBM_RIGHT);
    assert(!Load(configured,
                 "<SHORTCUTS version='1'>"
                 "<BIND action='chain.clone' keys='A+L' event='release'/>"
                 "</SHORTCUTS>"));
    UIController::GetInstance()->Reset();
    return 0;
}
