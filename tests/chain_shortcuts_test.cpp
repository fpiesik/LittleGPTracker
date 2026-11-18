#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Commands/ApplicationCommandDispatcher.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Views/ChainView.h"
#include "Application/Views/PhraseView.h"
#include "Application/Views/SongView.h"
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
    if (mode == PM_PHRASE)
        assert(from == 2 && to == 3);
    else
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
class NavigationPhraseView : public PhraseView {
  public:
    NavigationPhraseView(GUIWindow &window, ViewData *data)
        : PhraseView(window, data) {}
    GUIRect Selection() { return getSelectionRect(); }
};

static void CheckPhrasePlayback(TestWindow &window, ViewData &data,
                                Shortcuts &keys, bool remapped,
                                bool selection) {
    assert(Load(
        keys,
        remapped
            ? "<SHORTCUTS version='1'>"
              "<BIND action='phrase.mute' keys='L+DOWN' event='press'/>"
              "<BIND action='phrase.solo' keys='L+LEFT' event='press'/>"
              "<BIND action='phrase.selection_solo' keys='L+RIGHT' "
              "event='press'/>"
              "<BIND action='phrase.play' keys='SELECT' exclude='L' "
              "event='press'/>"
              "<BIND action='phrase.play_song' keys='L+SELECT' event='press'/>"
              "<BIND action='phrase.selection_play' keys='L+SELECT' "
              "event='press'/>"
              "<BIND action='phrase.selection_play_song' keys='SELECT' "
              "exclude='L' event='press'/>"
              "<BIND action='phrase.mute_release' keys='L' event='release'/>"
              "<BIND action='phrase.solo_release' keys='L' event='release'/>"
              "</SHORTCUTS>"
            : "<SHORTCUTS version='1'/>"));
    data.playMode_ = PM_PHRASE;
    data.songX_ = 2;
    data.chainRow_ = 3;
    data.currentPhrase_ = 0;
    PhraseView view(window, &data);
    view.ProcessButtonMask(EPBM_RIGHT,
                           true); // Velocity column, without note audition.
    if (selection) {
        view.ProcessButtonMask(EPBM_B | EPBM_L, true);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_DOWN, true);
    }

    Player &player = *Player::GetInstance();
    UIController::GetInstance()->UnMuteAll();
    unsigned short modifier = remapped ? EPBM_L : EPBM_R;
    unsigned short button = remapped ? EPBM_DOWN : EPBM_B;
    if (!selection) {
        view.ProcessButtonMask(modifier | button, true);
        assert(player.IsChannelMuted(2));
        view.ProcessButtonMask(modifier, false);
        assert(!player.IsChannelMuted(2));
        view.ProcessButtonMask(modifier | button, true);
        view.ProcessButtonMask(button, false);
        view.ProcessButtonMask(0, false);
        assert(player.IsChannelMuted(2));
        view.ProcessButtonMask(EPBM_R | EPBM_L, true);
        assert(!player.IsChannelMuted(2));
    } else {
        uchar *velocity = data.song_->phrase_->velocity_;
        velocity[0] = 50;
        velocity[2] = 60;
        view.ProcessButtonMask(EPBM_R | EPBM_B, true);
        assert(velocity[1] == 55 && !player.IsChannelMuted(2));
    }
    button = remapped ? (selection ? EPBM_RIGHT : EPBM_LEFT) : EPBM_A;
    player.SetChannelMute(4, true);
    view.ProcessButtonMask(modifier | button, true);
    assert(player.IsChannelMuted(1) && !player.IsChannelMuted(2));
    view.ProcessButtonMask(modifier, false);
    assert(!player.IsChannelMuted(1) && player.IsChannelMuted(4));
    view.ProcessButtonMask(modifier | button, true);
    view.ProcessButtonMask(button, false);
    view.ProcessButtonMask(0, false);
    assert(player.IsChannelMuted(1));
    view.ProcessButtonMask(EPBM_R | EPBM_L, true);
    assert(!player.IsChannelMuted(1) && !player.IsChannelMuted(4));

    playCalls = 0;
    if (remapped)
        view.ProcessButtonMask(EPBM_START, true);
    unsigned short play = remapped
                              ? (selection ? EPBM_L | EPBM_SELECT : EPBM_SELECT)
                              : EPBM_START;
    unsigned short playSong =
        remapped ? (selection ? EPBM_SELECT : EPBM_L | EPBM_SELECT)
                 : EPBM_R | EPBM_START;
    view.ProcessButtonMask(play, false);
    assert(playCalls == 0);
    view.ProcessButtonMask(play, true);
    assert(playCalls == 1 && !playFromStart);
    view.ProcessButtonMask(playSong, true);
    assert(playCalls == 2 && playFromStart);
}

static void CheckPhraseViewSwitches(TestWindow &window, ViewData &data,
                                    Shortcuts &keys, bool remapped,
                                    bool selection) {
    assert(Load(
        keys,
        remapped
            ? "<SHORTCUTS version='1'>"
              "<BIND action='phrase.chain' keys='L+LEFT' event='press'/>"
              "<BIND action='phrase.instrument' keys='L+RIGHT' event='press'/>"
              "<BIND action='phrase.table' keys='L+DOWN' event='press'/>"
              "<BIND action='phrase.groove' keys='L+UP' event='press'/>"
              "<BIND action='phrase.selection_chain' keys='L+RIGHT' "
              "event='press'/>"
              "<BIND action='phrase.selection_instrument' keys='L+LEFT' "
              "event='press'/>"
              "</SHORTCUTS>"
            : "<SHORTCUTS version='1'/>"));
    data.currentPhrase_ = 5;
    data.playMode_ = PM_PHRASE;
    PhraseView view(window, &data);
    SwitchObserver observer;
    view.AddObserver(observer);
    view.ProcessButtonMask(EPBM_DOWN, true);
    if (selection) {
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        view.ProcessButtonMask(0, true);
    }

    unsigned short mod = remapped ? EPBM_L : EPBM_R;
    unsigned short chain =
        mod | (selection && remapped ? EPBM_RIGHT : EPBM_LEFT);
    unsigned short instrument =
        mod | (selection && remapped ? EPBM_LEFT : EPBM_RIGHT);
    if (remapped)
        view.ProcessButtonMask(
            EPBM_R | EPBM_LEFT | EPBM_RIGHT | EPBM_UP | EPBM_DOWN, true);
    view.ProcessButtonMask(chain, false);
    assert(observer.views.empty());
    view.ProcessButtonMask(chain, true);
    assert(observer.views.size() == 1 && observer.views.back() == VT_CHAIN);

    Phrase &phrase = *data.song_->phrase_;
    const int base = 16 * data.currentPhrase_;
    std::memset(phrase.instr_ + base, 0xff, 16);
    phrase.instr_[base] = 2;
    phrase.instr_[base + 1] = 3;
    phrase.instr_[base + 2] = 4;
    view.ProcessButtonMask(instrument, true);
    assert(observer.views.back() == VT_INSTRUMENT &&
           data.currentInstrument_ == 3);
    phrase.instr_[base + 1] = 0xff;
    view.ProcessButtonMask(instrument, true);
    assert(data.currentInstrument_ == 2);
    std::memset(phrase.instr_ + base, 0xff, 16);
    view.ProcessButtonMask(instrument, true);
    assert(data.currentInstrument_ == 0);

    const int step = base + 1;
    data.currentTable_ = 7;
    phrase.cmd1_[step] = phrase.cmd2_[step] = I_CMD_NONE;
    view.ProcessButtonMask(mod | EPBM_DOWN, true);
    assert(data.currentTable_ == 7);
    phrase.cmd2_[step] = I_CMD_TABL;
    phrase.param2_[step] = 0x83;
    view.ProcessButtonMask(mod | EPBM_DOWN, true);
    assert(data.currentTable_ == (selection ? 7 : 3));
    phrase.cmd1_[step] = I_CMD_TABL;
    phrase.param1_[step] = 0x85;
    view.ProcessButtonMask(mod | EPBM_DOWN, true);
    assert(data.currentTable_ == (selection ? 7 : 5));
    view.ProcessButtonMask(mod | EPBM_UP, true);
    assert(observer.views.size() == (selection ? 4u : 8u));
    if (!selection) {
        assert(observer.views[4] == VT_TABLE && observer.views[5] == VT_TABLE &&
               observer.views[6] == VT_TABLE && observer.views[7] == VT_GROOVE);
    }
    view.RemoveObserver(observer);
}

static void CheckPhraseNavigation(TestWindow &window, ViewData &data,
                                  Shortcuts &keys, bool remapped) {
    assert(Load(
        keys,
        remapped
            ? "<SHORTCUTS version='1'>"
              "<BIND action='phrase.down' keys='L+LEFT' event='press'/>"
              "<BIND action='phrase.next_channel' keys='L+UP' event='press'/>"
              "<BIND action='phrase.selection_right' keys='L+RIGHT' "
              "event='press'/>"
              "</SHORTCUTS>"
            : "<SHORTCUTS version='1'/>"));
    Reset(data);
    data.songY_ = data.songOffset_ = 0;
    data.playMode_ = PM_PHRASE;
    data.currentPhrase_ = 5;
    std::memset(data.song_->data_, 0xff, SONG_CHANNEL_COUNT);
    data.song_->data_[0] = 0;
    data.song_->data_[2] = 1;
    unsigned char *chain = data.song_->chain_->data_;
    chain[0] = 5;
    chain[1] = 6;
    chain[16] = 8;
    NavigationPhraseView phrase(window, &data);
    unsigned short down = remapped ? EPBM_L | EPBM_LEFT : EPBM_DOWN;
    unsigned short nextChannel =
        remapped ? EPBM_L | EPBM_UP : EPBM_B | EPBM_RIGHT;
    if (remapped) {
        phrase.ProcessButtonMask(EPBM_DOWN, true);
        phrase.ProcessButtonMask(EPBM_B | EPBM_RIGHT, true);
    }
    phrase.ProcessButtonMask(nextChannel, false);
    assert(data.phraseCurPos_ == 0 && data.songX_ == 0);
    phrase.ProcessButtonMask(nextChannel, true);
    assert(data.songX_ == 2 && data.currentChain_ == 1 &&
           data.currentPhrase_ == 8);
    phrase.ProcessButtonMask(EPBM_B | EPBM_LEFT, true);
    phrase.ProcessButtonMask(down, true);
    phrase.ProcessButtonMask(EPBM_B | EPBM_DOWN, true);
    assert(data.songX_ == 0 && data.currentPhrase_ == 6 &&
           data.phraseCurPos_ == 1);
    phrase.ProcessButtonMask(EPBM_B | EPBM_DOWN, true);
    assert(data.chainRow_ == 1 && data.currentPhrase_ == 6);
    phrase.ProcessButtonMask(EPBM_B | EPBM_UP, true);
    for (int i = 0; i < 15; ++i)
        phrase.ProcessButtonMask(down, true);
    assert(data.phraseCurPos_ == 0 && data.currentPhrase_ == 6);
    phrase.ProcessButtonMask(EPBM_UP, true);
    assert(data.phraseCurPos_ == 15 && data.currentPhrase_ == 5);

    phrase.ProcessButtonMask(EPBM_B | EPBM_L, true);
    phrase.ProcessButtonMask(EPBM_DOWN, true);
    assert(data.phraseCurPos_ == 15 && data.currentPhrase_ == 5);
    if (remapped) {
        phrase.ProcessButtonMask(EPBM_RIGHT, true);
        assert(phrase.Selection().Right() == 0);
    }
    phrase.ProcessButtonMask(remapped ? EPBM_L | EPBM_RIGHT : EPBM_RIGHT, true);
    assert(phrase.Selection().Left() == 0 && phrase.Selection().Right() == 1);
}

static void CheckSongEdits(TestWindow &window, ViewData &data, Shortcuts &keys,
                           bool remapped) {
    assert(Load(
        keys,
        remapped ? "<SHORTCUTS version='1'>"
                   "<BIND action='song.increase_fine' keys='SELECT+RIGHT' "
                   "event='press'/>"
                   "<BIND action='song.edit_hold' keys='SELECT' event='press'/>"
                   "<BIND action='song.cut' keys='SELECT+B' event='press'/>"
                   "<BIND action='song.paste' keys='SELECT+L' event='press'/>"
                   "<BIND action='song.selection_copy' keys='SELECT+DOWN' "
                   "event='press'/>"
                   "<BIND action='song.selection_cut' keys='SELECT+UP' "
                   "event='press'/>"
                   "<BIND action='song.selection_extend' keys='SELECT+RIGHT' "
                   "event='press'/>"
                   "</SHORTCUTS>"
                 : "<SHORTCUTS version='1'/>"));
    SongView song(window, &data, "test");
    data.songX_ = data.songY_ = data.songOffset_ = 0;
    unsigned char *cells = data.song_->data_;
    const int stride = SONG_CHANNEL_COUNT;
    const int last = (SONG_ROW_COUNT - 1) * stride;
    std::memset(cells, 0xff, stride * SONG_ROW_COUNT);
    Chain &chains = *data.song_->chain_;
    chains.ClearAllocation();
    cells[0] = 4;
    unsigned short edit = remapped ? EPBM_SELECT : EPBM_A;
    if (remapped)
        song.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
    song.ProcessButtonMask(edit | EPBM_RIGHT, true);
    song.ProcessButtonMask(edit | EPBM_RIGHT, true);
    song.ProcessButtonMask(0, false);
    assert(cells[0] == 6 && !chains.IsUsed(5) && !chains.IsUsed(6));
    song.ProcessButtonMask(EPBM_B | EPBM_DOWN, true);
    assert(data.songOffset_ == 16 && chains.IsUsed(6) && !chains.IsUsed(5));
    data.songOffset_ = 0;
    cells[stride] = 7;
    cells[1] = 8;
    cells[stride + 1] = 9;
    cells[last] = 10;
    unsigned short cut = remapped ? EPBM_SELECT | EPBM_B : EPBM_B | EPBM_A;
    unsigned short paste = remapped ? EPBM_SELECT | EPBM_L : EPBM_L | EPBM_A;
    song.ProcessButtonMask(cut, true);
    assert(cells[0] == 7 && cells[1] == 8 && cells[last] == 0xff);
    song.ProcessButtonMask(paste, true);
    assert(cells[0] == 6 && cells[stride] == 7 && cells[last] == 10);

    data.songY_ = 0;
    song.ProcessButtonMask(EPBM_L | EPBM_B, true);
    song.ProcessButtonMask(EPBM_RIGHT | EPBM_DOWN, true);
    song.ProcessButtonMask(remapped ? EPBM_SELECT | EPBM_DOWN : EPBM_B, true);
    assert(data.songX_ == 0 && data.songY_ == 0);
    data.songX_ = stride - 1;
    data.songOffset_ = SONG_ROW_COUNT - 1;
    song.ProcessButtonMask(paste, true);
    assert(cells[last + stride - 1] == 6 && cells[last] == 10);

    data.songX_ = data.songY_ = data.songOffset_ = 0;
    song.ProcessButtonMask(EPBM_L | EPBM_B, true);
    song.ProcessButtonMask(EPBM_DOWN, true);
    song.ProcessButtonMask(
        remapped ? EPBM_SELECT | EPBM_RIGHT : EPBM_B | EPBM_L, true);
    assert(data.songX_ == stride - 1);
    song.ProcessButtonMask(remapped ? EPBM_SELECT | EPBM_UP : EPBM_A | EPBM_L,
                           true);
    assert(cells[0] == 0xff && cells[1] == 0xff && cells[last] == 0xff);
    song.ProcessButtonMask(paste, true);
    assert(cells[0] == 6 && cells[1] == 8 && cells[stride] == 7 &&
           cells[stride + 1] == 9);
}

static void CheckSongViewSwitches(TestWindow &window, ViewData &data,
                                  Shortcuts &keys, bool remapped,
                                  bool selection) {
    assert(Load(
        keys,
        remapped
            ? "<SHORTCUTS version='1'>"
              "<BIND action='song.chain' keys='SELECT+LEFT' event='press'/>"
              "<BIND action='song.project' keys='SELECT+UP' event='press'/>"
              "<BIND action='song.mixer' keys='SELECT+DOWN' event='press'/>"
              "<BIND action='song.selection_chain' keys='SELECT+UP' "
              "event='press'/>"
              "<BIND action='song.selection_project' keys='SELECT+DOWN' "
              "event='press'/>"
              "<BIND action='song.selection_mixer' keys='SELECT+LEFT' "
              "event='press'/>"
              "</SHORTCUTS>"
            : "<SHORTCUTS version='1'/>"));
    data.songX_ = data.songY_ = data.songOffset_ = 0;
    SongView song(window, &data, "test");
    SwitchObserver observer;
    song.AddObserver(observer);
    if (selection) {
        song.ProcessButtonMask(EPBM_L | EPBM_B, true);
        song.ProcessButtonMask(EPBM_RIGHT, true);
        assert(data.songX_ == 1);
    }
    const unsigned short modifier = remapped ? EPBM_SELECT : EPBM_R;
    const unsigned short chain =
        remapped ? (selection ? EPBM_UP : EPBM_LEFT) : EPBM_RIGHT;
    const unsigned short project = remapped && selection ? EPBM_DOWN : EPBM_UP;
    const unsigned short mixer = remapped && selection ? EPBM_LEFT : EPBM_DOWN;
    unsigned char *cell = data.GetCurrentSongPointer();
    *cell = 0xff;
    data.currentChain_ = 3;
    song.ProcessButtonMask(modifier | chain, true);
    assert(observer.views.empty() && data.currentChain_ == 3);
    *cell = 7;
    song.ProcessButtonMask(modifier | chain, false);
    if (remapped)
        song.ProcessButtonMask(EPBM_R | EPBM_RIGHT | EPBM_UP | EPBM_DOWN, true);
    assert(observer.views.empty());
    song.ProcessButtonMask(modifier | chain, true);
    song.ProcessButtonMask(modifier | project, true);
    song.ProcessButtonMask(modifier | mixer, true);
    assert(data.currentChain_ == 7 && observer.views.size() == 3);
    assert(observer.views[0] == VT_CHAIN && observer.views[1] == VT_PROJECT &&
           observer.views[2] == VT_MIXER);
    song.RemoveObserver(observer);
}

static void CheckSongMovement(TestWindow &window, ViewData &data,
                              Shortcuts &keys) {
    SongView song(window, &data, "test");
    data.songX_ = data.songY_ = data.songOffset_ = 0;
    std::memset(data.song_->data_, 0xff, SONG_ROW_COUNT * SONG_CHANNEL_COUNT);
    data.song_->data_[0] = 1;
    data.song_->data_[6 * SONG_CHANNEL_COUNT] = 2;
    song.ProcessButtonMask(EPBM_L | EPBM_DOWN, true);
    assert(data.songY_ == 6);
    song.ProcessButtonMask(EPBM_L | EPBM_UP, true);
    assert(data.songY_ == 0);
    song.ProcessButtonMask(EPBM_B | EPBM_DOWN, true);
    assert(data.songOffset_ == 16);
    song.ProcessButtonMask(EPBM_B | EPBM_UP, true);
    assert(data.songOffset_ == 0);
    song.ProcessButtonMask(EPBM_DOWN | EPBM_RIGHT, true);
    assert(data.songX_ == 1 && data.songY_ == 1);
    assert(Load(
        keys,
        "<SHORTCUTS version='1'>"
        "<BIND action='song.down' keys='SELECT+RIGHT' event='press'/>"
        "<BIND action='song.selection_right' keys='SELECT+UP' event='press'/>"
        "</SHORTCUTS>"));
    song.ProcessButtonMask(EPBM_DOWN, true);
    song.ProcessButtonMask(EPBM_SELECT | EPBM_RIGHT, false);
    assert(data.songY_ == 1);
    song.ProcessButtonMask(EPBM_SELECT | EPBM_RIGHT, true);
    assert(data.songY_ == 2 && data.songX_ == 1);
    song.ProcessButtonMask(EPBM_L | EPBM_B, true);
    song.ProcessButtonMask(EPBM_SELECT | EPBM_UP, true);
    assert(data.songY_ == 2 && data.songX_ == 2);
    song.ProcessButtonMask(EPBM_L | EPBM_DOWN, true);
    assert(data.songY_ == 3 && data.songOffset_ == 0);
    song.ProcessButtonMask(EPBM_RIGHT, true);
    assert(data.songX_ == 2);
}

static void CheckSongClone(TestWindow &window, ViewData &data, Shortcuts &keys,
                           bool remapped) {
    assert(Load(
        keys,
        remapped
            ? "<SHORTCUTS version='1'>"
              "<BIND action='song.insert' keys='SELECT' exclude='B+L+R+UP' event='press'/>"
              "<BIND action='song.next_unused' keys='SELECT+UP' event='press'/>"
              "<BIND action='song.begin_clone_selection' keys='SELECT+B' "
              "event='press'/>"
              "<BIND action='song.clone' keys='SELECT+L' event='press'/>"
              "<BIND action='song.deep_clone' keys='SELECT+R' event='press'/>"
              "</SHORTCUTS>"
            : "<SHORTCUTS version='1'/>"));
    unsigned short insert = remapped ? EPBM_SELECT : EPBM_A;
    unsigned short next = remapped ? EPBM_SELECT | EPBM_UP : EPBM_A;
    unsigned short begin = remapped ? EPBM_SELECT | EPBM_B : EPBM_L | EPBM_B;
    unsigned short clone = remapped ? EPBM_SELECT | EPBM_L : EPBM_L | EPBM_A;
    unsigned short deep = remapped ? EPBM_SELECT | EPBM_R : EPBM_L | EPBM_A;

    data.songX_ = data.songY_ = data.songOffset_ = 0;
    unsigned char *cell = data.GetCurrentSongPointer();
    *cell = 0xff;
    Chain &chains = *data.song_->chain_;
    Phrase &phrases = *data.song_->phrase_;
    chains.ClearAllocation();
    phrases.ClearAllocation();
    SongView song(window, &data, "test");
    if (remapped) {
        song.ProcessButtonMask(EPBM_A, true);
        assert(*cell == 0xff);
    }
    song.ProcessButtonMask(insert, true);
    assert(*cell == 0);
    song.ProcessButtonMask(0, false);
    song.ProcessButtonMask(next, true);
    assert(*cell == 1 && data.songY_ == 0);
    std::memset(chains.data_ + 16, 0xff, 16);
    chains.data_[16] = chains.data_[17] = 5;
    chains.data_[18] = 8;
    chains.transpose_[16] = 3;
    phrases.SetUsed(5);
    phrases.SetUsed(8);
    phrases.velocity_[5 * 16 + 4] = 43;
    phrases.cmd2_[5 * 16 + 4] = 123;
    song.ProcessButtonMask(begin, true);
    song.ProcessButtonMask(remapped ? EPBM_SELECT : EPBM_L, false);
    song.ProcessButtonMask(clone, true);
    assert(*cell == 2 && chains.data_[32] == 5 && chains.transpose_[32] == 3);
    song.ProcessButtonMask(remapped ? EPBM_SELECT : EPBM_L, false);
    song.ProcessButtonMask(deep, true);
    int copy = chains.data_[32];
    assert(copy != 5 && copy == chains.data_[33] && chains.data_[34] != 8);
    assert(chains.data_[16] == 5 && chains.data_[18] == 8 &&
           chains.data_[35] == 0xff);
    assert(phrases.velocity_[copy * 16 + 4] == 43 &&
           phrases.cmd2_[copy * 16 + 4] == 123);

    // Two distinct phrases need two free slots. Keep the shallow clone intact.
    *cell = 1;
    for (int i = 0; i < PHRASE_COUNT - 1; ++i)
        phrases.SetUsed(i);
    song.ProcessButtonMask(begin, true);
    song.ProcessButtonMask(clone, true);
    song.ProcessButtonMask(deep, true);
    assert(*cell == 3 && chains.data_[48] == 5 && chains.data_[50] == 8);
    assert(!phrases.IsUsed(PHRASE_COUNT - 1));
}

static void CheckSongChannels(TestWindow &window, ViewData &data,
                              Shortcuts &keys, bool remapped, bool selection) {
    assert(Load(
        keys,
        remapped
            ? "<SHORTCUTS version='1'>"
              "<BIND action='song.mute' keys='SELECT+B' event='press'/>"
              "<BIND action='song.solo' keys='SELECT+RIGHT' event='press'/>"
              "<BIND action='song.unmute_all' keys='SELECT+L' event='press'/>"
              "<BIND action='song.selection_mute' keys='SELECT+UP' "
              "event='press'/>"
              "<BIND action='song.selection_solo' keys='SELECT+DOWN' "
              "event='press'/>"
              "<BIND action='song.selection_unmute_all' keys='SELECT+LEFT' "
              "event='press'/>"
              "<BIND action='song.mute_release' keys='SELECT' event='release'/>"
              "<BIND action='song.solo_release' keys='SELECT' event='release'/>"
              "</SHORTCUTS>"
            : "<SHORTCUTS version='1'/>"));
    Player &player = *Player::GetInstance();
    for (int solo = 0; solo < 2; ++solo) {
        for (int latch = 0; latch < 2; ++latch) {
            Reset(data);
            SongView song(window, &data, "test");
            data.songX_ = 1;
            if (selection) {
                song.ProcessButtonMask(EPBM_L | EPBM_B, true);
                song.ProcessButtonMask(EPBM_RIGHT, true);
                assert(data.songX_ == 2);
            }
            player.SetChannelMute(3, true);
            unsigned short modifier = remapped ? EPBM_SELECT : EPBM_R;
            unsigned short button = solo ? EPBM_A : EPBM_B;
            if (remapped && selection)
                button = solo ? EPBM_DOWN : EPBM_UP;
            else if (remapped && solo)
                button = EPBM_RIGHT;
            song.ProcessButtonMask(modifier, true);
            song.ProcessButtonMask(modifier | button, true);
            for (int pass = 0; pass < 2; ++pass) {
                for (int track = 0; track < SONG_CHANNEL_COUNT; ++track) {
                    bool inRange = track == 1 || (selection && track == 2);
                    bool expected = track == 3;
                    if (!pass || latch)
                        expected = solo ? !inRange : (inRange || track == 3);
                    assert(player.IsChannelMuted(track) == expected);
                }
                if (!pass) {
                    song.ProcessButtonMask(latch ? button : modifier, false);
                    song.ProcessButtonMask(0, false);
                }
            }
            unsigned short unmute = remapped && selection ? EPBM_LEFT : EPBM_L;
            song.ProcessButtonMask(modifier | unmute, true);
            for (int track = 0; track < SONG_CHANNEL_COUNT; ++track)
                assert(!player.IsChannelMuted(track));
        }
    }
}

struct SongRequest {
    unsigned int from, to;
    bool stop, immediate;
};
static std::vector<SongRequest> songRequests;
extern "C" void __wrap__ZN6Player17OnSongStartButtonEjjbb(
    Player *, unsigned int from, unsigned int to, bool stop, bool immediate) {
    SongRequest request = {from, to, stop, immediate};
    songRequests.push_back(request);
}

static void ExpectSongRequest(SongView &song, unsigned short buttons,
                              unsigned int from, unsigned int to, bool stop,
                              bool immediate) {
    songRequests.clear();
    song.ProcessButtonMask(buttons, false);
    assert(songRequests.empty());
    song.ProcessButtonMask(buttons, true);
    assert(songRequests.size() == 1);
    const SongRequest &request = songRequests.front();
    assert(request.from == from && request.to == to && request.stop == stop &&
           request.immediate == immediate);
}

static void CheckSongTransport(TestWindow &window, ViewData &data,
                               Shortcuts &keys) {
    assert(Load(keys, "<SHORTCUTS version='1'/>"));
    SongView song(window, &data, "test");
    data.songX_ = 1;
    Player &player = *Player::GetInstance();
    player.SetSequencerMode(SM_SONG);
    song.ProcessButtonMask(EPBM_B | EPBM_LEFT | EPBM_RIGHT, true);
    assert(player.GetSequencerMode() == SM_LIVE);
    song.ProcessButtonMask(EPBM_B | EPBM_RIGHT, true);
    assert(player.GetSequencerMode() == SM_SONG);
    ExpectSongRequest(song, EPBM_START, 1, 1, false, false);
    ExpectSongRequest(song, EPBM_B | EPBM_START, 1, 1, false, true);
    ExpectSongRequest(song, EPBM_R | EPBM_START, 1, 1, true, false);
    ExpectSongRequest(song, EPBM_L | EPBM_START, 0, SONG_CHANNEL_COUNT - 1,
                      false, false);
    assert(player.GetSequencerMode() == SM_LIVE);

    ApplicationCommandDispatcher::GetInstance()->Init(data.project_);
    int tempo = data.project_->GetTempo();
    song.ProcessButtonMask(EPBM_L | EPBM_LEFT, true);
    assert(data.project_->GetTempo() < tempo);
    song.ProcessButtonMask(EPBM_L | EPBM_RIGHT, true);
    assert(data.project_->GetTempo() == tempo);
    ApplicationCommandDispatcher::GetInstance()->Close();

    assert(Load(
        keys,
        "<SHORTCUTS version='1'>"
        "<BIND action='song.play' keys='SELECT+UP' event='press'/>"
        "<BIND action='song.selection_stop' keys='SELECT+DOWN' event='press'/>"
        "</SHORTCUTS>"));
    songRequests.clear();
    song.ProcessButtonMask(EPBM_START, true);
    assert(songRequests.empty());
    ExpectSongRequest(song, EPBM_SELECT | EPBM_UP, 1, 1, false, false);
    song.ProcessButtonMask(EPBM_L | EPBM_B, true);
    song.ProcessButtonMask(EPBM_RIGHT, true);
    ExpectSongRequest(song, EPBM_L | EPBM_START, 1, 2, false, false);
    ExpectSongRequest(song, EPBM_SELECT | EPBM_DOWN, 1, 2, true, false);
    songRequests.clear();
    song.ProcessButtonMask(EPBM_R | EPBM_START, true);
    assert(songRequests.empty());
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
    for (int remapped = 0; remapped < 2; ++remapped)
        for (int selection = 0; selection < 2; ++selection)
            CheckSongViewSwitches(window, data, configured, remapped, selection);
    CheckSongEdits(window, data, configured, false);
    CheckSongEdits(window, data, configured, true);
    for (int remapped = 0; remapped < 2; ++remapped)
        for (int selection = 0; selection < 2; ++selection)
            CheckPhraseViewSwitches(window, data, configured, remapped, selection);
    for (int remapped = 0; remapped < 2; ++remapped)
        for (int selection = 0; selection < 2; ++selection)
            CheckPhrasePlayback(window, data, configured, remapped, selection);
    CheckPhraseNavigation(window, data, configured, false);
    CheckPhraseNavigation(window, data, configured, true);
    CheckSongTransport(window, data, configured);
    CheckSongMovement(window, data, configured);
    CheckSongClone(window, data, configured, false);
    CheckSongClone(window, data, configured, true);
    for (int remapped = 0; remapped < 2; ++remapped)
        for (int selection = 0; selection < 2; ++selection)
            CheckSongChannels(window, data, configured, remapped, selection);
    UIController::GetInstance()->Reset();
    return 0;
}
