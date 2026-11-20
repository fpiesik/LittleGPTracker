#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Views/BaseClasses/ModalView.h"
#include "Application/Views/ModalDialogs/CommandSelectorModal.h"
#include "Application/Views/ModalDialogs/ImportSampleDialog.h"
#include "Application/Views/ModalDialogs/MessageBox.h"
#include "Application/Views/ModalDialogs/NewProjectDialog.h"
#include "Application/Views/ModalDialogs/RecordDialog.h"
#include "Application/Views/ModalDialogs/SelectProjectDialog.h"
#include "Application/Views/PhraseView.h"
#include "Application/Views/TableView.h"
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

static bool Load(Shortcuts &shortcuts, const char *xml) {
    TiXmlDocument document;
    document.Parse(xml);
    assert(!document.Error());
    std::string error;
    return shortcuts.Load(document.RootElement(), error);
}

// Capture playback and modal-opening requests without audio or a real GUI.
static int playCalls, modalCalls, auditionCalls, stopCalls;
extern "C" void __wrap__ZN6Player4StopEv(Player *) { ++stopCalls; }
static bool playFromStart;
extern "C" void
__wrap__ZN6Player13OnStartButtonE8PlayModejbh(Player *, PlayMode mode,
                                              unsigned int from, bool fromStart,
                                              unsigned char to) {
    if (mode == PM_AUDITION) {
        assert(!fromStart && from == 0 && to == 0);
        ++auditionCalls;
        return;
    }
    assert(mode == PM_PHRASE && from == 0 && to == 0);
    ++playCalls;
    playFromStart = fromStart;
}
extern "C" void
__wrap__ZN4View7DoModalEP9ModalViewPFvRS_RS0_E(View *, ModalView *modal,
                                               ModalViewCallback) {
    ++modalCalls;
    delete modal;
}

typedef std::vector<Shortcuts::Action> Actions;
static Actions Legacy(unsigned m, bool pressed, bool selection) {
    Actions result;
    if (!pressed)
        return result;
    if (selection) {
        if (m & EPBM_B) {
            if (m & EPBM_L)
                result.push_back(Shortcuts::TableSelectionExtend);
            else if (m & EPBM_R)
                result.push_back(Shortcuts::TableSelectionInterpolate);
            else
                result.push_back(Shortcuts::TableSelectionCopy);
        } else if (m & EPBM_A) {
            if (m & EPBM_L)
                result.push_back(Shortcuts::TableSelectionCut);
        } else if (m & EPBM_R) {
            if (m & EPBM_UP)
                result.push_back(Shortcuts::TableSelectionParent);
            if (m & EPBM_START)
                result.push_back(Shortcuts::TableSelectionPlayFromStart);
        } else if (!(m & EPBM_L)) {
            if (m & EPBM_DOWN)
                result.push_back(Shortcuts::TableSelectionDown);
            if (m & EPBM_UP)
                result.push_back(Shortcuts::TableSelectionUp);
            if (m & EPBM_LEFT)
                result.push_back(Shortcuts::TableSelectionLeft);
            if (m & EPBM_RIGHT)
                result.push_back(Shortcuts::TableSelectionRight);
            if (m & EPBM_START)
                result.push_back(Shortcuts::TableSelectionPlay);
        }
    } else if (m & EPBM_B) {
        if (m & EPBM_LEFT)
            result.push_back(Shortcuts::TablePrevious);
        if (m & EPBM_RIGHT)
            result.push_back(Shortcuts::TableNext);
        if (m & EPBM_DOWN)
            result.push_back(Shortcuts::TablePrevious16);
        if (m & EPBM_UP)
            result.push_back(Shortcuts::TableNext16);
        if (m & EPBM_A)
            result.push_back(Shortcuts::TableCut);
        if (m & EPBM_L)
            result.push_back(Shortcuts::TableSelect);
    } else if (m & EPBM_A) {
        if (m & EPBM_DOWN)
            result.push_back(Shortcuts::TableDecreaseCoarse);
        if (m & EPBM_UP)
            result.push_back(Shortcuts::TableIncreaseCoarse);
        if (m & EPBM_LEFT)
            result.push_back(Shortcuts::TableDecreaseFine);
        if (m & EPBM_RIGHT)
            result.push_back(Shortcuts::TableIncreaseFine);
        if (m == EPBM_A)
            result.push_back(Shortcuts::TableInsert);
        if (m & EPBM_L)
            result.push_back(Shortcuts::TablePaste);
    } else if (m & EPBM_R) {
        if (m & EPBM_UP)
            result.push_back(Shortcuts::TableParent);
        if (m & EPBM_LEFT)
            result.push_back(Shortcuts::TableViewLeft);
        if (m & EPBM_RIGHT)
            result.push_back(Shortcuts::TableViewRight);
        if (m & EPBM_START)
            result.push_back(Shortcuts::TablePlayFromStart);
    } else {
        if (m & EPBM_DOWN)
            result.push_back(Shortcuts::TableDown);
        if (m & EPBM_UP)
            result.push_back(Shortcuts::TableUp);
        if (m & EPBM_LEFT)
            result.push_back(Shortcuts::TableLeft);
        if (m & EPBM_RIGHT)
            result.push_back(Shortcuts::TableRight);
        if (m & EPBM_START)
            result.push_back(Shortcuts::TablePlay);
    }
    return result;
}
static Actions Resolve(const Shortcuts &s, unsigned m, bool pressed,
                       bool selection) {
    Actions result;
    int first =
        selection ? Shortcuts::TableSelectionExtend : Shortcuts::TablePrevious;
    int last = selection ? Shortcuts::TableSelectionPlay : Shortcuts::TablePlay;
    for (int i = first; i <= last; ++i) {
        Shortcuts::Action action = static_cast<Shortcuts::Action>(i);
        if (s.Matches(action, m, pressed))
            result.push_back(action);
    }
    return result;
}
class TestTable : public TableView {
  public:
    TestTable(GUIWindow &w, ViewData *data, ViewType type = VT_TABLE)
        : TableView(w, data) {
        SetFocus(type);
    }
    bool Selecting() { return viewMode_ == VM_SELECTION; }
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
class TestProjectPicker : public SelectProjectDialog {
  public:
    TestProjectPicker(View &parent, Path &folder)
        : SelectProjectDialog(parent) {
        setCurrentFolder(folder);
    }
    void DrawView() {}
};
// Drive the browser without playing audio or copying files into a project.
static std::string previewed, imported;
extern "C" void __wrap__ZN6Player14StartStreamingERK4Path(Player *,
                                                          const Path &p) {
    previewed = Path(p).GetName();
}
extern "C" void __wrap__ZN6Player13StopStreamingEv(Player *) {
    previewed.clear();
}
extern "C" int __wrap__ZN10SamplePool12ImportSampleER4Path(SamplePool *,
                                                           Path &p) {
    imported = p.GetName();
    return -1;
}
class TestSampleBrowser : public ImportSampleDialog {
  public:
    TestSampleBrowser(View &parent, Path &folder) : ImportSampleDialog(parent) {
        setCurrentFolder(&folder);
    }
    bool FolderChangeRequestsRedraw(unsigned short mask) {
        isDirty_ = false; // View::ProcessButton clears this before dispatch.
        ProcessButtonMask(mask, true);
        return isDirty_;
    }
};
static void CheckPhraseClone(TestWindow &window, ViewData &data,
                             Shortcuts &keys, bool remapped) {
    assert(Load(
        keys,
        remapped
            ? "<SHORTCUTS version='1'>"
              "<BIND action='phrase.insert' keys='L+SELECT' event='press'/>"
              "<BIND action='phrase.next_instrument' keys='SELECT' "
              "event='press'/>"
              "<BIND action='phrase.next_table' keys='SELECT' event='press'/>"
              "<BIND action='phrase.begin_clone_selection' keys='SELECT+B' "
              "event='press'/>"
              "<BIND action='phrase.clone_instrument' keys='L+UP' "
              "event='press'/>"
              "<BIND action='phrase.clone_table' keys='L+DOWN' event='press'/>"
              "</SHORTCUTS>"
            : "<SHORTCUTS version='1'/>"));
    data.currentPhrase_ = data.songX_ = data.chainRow_ = 0;
    data.playMode_ = PM_PHRASE;
    PhraseView view(window, &data);
    Phrase &phrase = *data.song_->phrase_;
    unsigned short insert = remapped ? EPBM_L | EPBM_SELECT : EPBM_A;
    unsigned short next = remapped ? EPBM_SELECT : EPBM_A;
    unsigned short arm = remapped ? EPBM_SELECT | EPBM_B : EPBM_L | EPBM_B;
    unsigned short cloneInstrument =
        remapped ? EPBM_L | EPBM_UP : EPBM_L | EPBM_A;
    unsigned short cloneTable = remapped ? EPBM_L | EPBM_DOWN : EPBM_L | EPBM_A;
    phrase.note_[0] = 0xff;
    if (remapped)
        view.ProcessButtonMask(EPBM_A, true);
    assert(phrase.note_[0] == 0xff);
    view.ProcessButtonMask(insert, true);
    assert(phrase.note_[0] == 60);
    view.ProcessButtonMask(EPBM_RIGHT, true);
    view.ProcessButtonMask(EPBM_RIGHT, true);
    InstrumentBank &bank = *data.project_->GetInstrumentBank();
    int freeSlot = bank.GetNext();
    assert(freeSlot < MAX_SAMPLEINSTRUMENT_COUNT);
    phrase.instr_[0] = MAX_SAMPLEINSTRUMENT_COUNT;
    view.ProcessButtonMask(insert, true);
    view.ProcessButtonMask(0, false);
    view.ProcessButtonMask(next, true);
    assert(phrase.instr_[0] == freeSlot);
    bank.GetInstrument(freeSlot)->FindVariable(SIP_VOLUME)->SetInt(73);
    view.ProcessButtonMask(arm, true);
    view.ProcessButtonMask(remapped ? EPBM_SELECT : EPBM_L, false);
    view.ProcessButtonMask(cloneInstrument, true);
    int copy = phrase.instr_[0];
    assert(copy != freeSlot && copy < MAX_SAMPLEINSTRUMENT_COUNT);
    Variable *volume = bank.GetInstrument(copy)->FindVariable(SIP_VOLUME);
    assert(volume->GetInt() == 73);
    volume->SetInt(42);
    assert(bank.GetInstrument(freeSlot)->FindVariable(SIP_VOLUME)->GetInt() ==
           73);

    TableHolder &tables = *TableHolder::GetInstance();
    tables.Reset();
    for (int lane = 0; lane < 2; ++lane) {
        view.ProcessButtonMask(EPBM_RIGHT, true);
        view.ProcessButtonMask(EPBM_RIGHT, true);
        (lane ? phrase.cmd2_ : phrase.cmd1_)[0] = I_CMD_TABL;
        ushort &param = (lane ? phrase.param2_ : phrase.param1_)[0];
        param = 77;
        view.ProcessButtonMask(insert, true);
        view.ProcessButtonMask(0, false);
        view.ProcessButtonMask(next, true);
        int source = param;
        assert(source < TABLE_COUNT && source != 77);
        tables.GetTable(source).param3_[15] = 0xbeef;
        param |= 0x8080; // Playback uses the low seven bits.
        view.ProcessButtonMask(arm, true);
        view.ProcessButtonMask(remapped ? EPBM_SELECT : EPBM_L, false);
        view.ProcessButtonMask(cloneTable, true);
        assert(param < TABLE_COUNT && param != source);
        assert(tables.GetTable(param).param3_[15] == 0xbeef);
    }
    for (int i = 0; i < TABLE_COUNT; ++i)
        tables.SetUsed(i);
    ushort original = phrase.param2_[0];
    view.ProcessButtonMask(arm, true);
    view.ProcessButtonMask(cloneTable, true);
    assert(phrase.param2_[0] == original);
}

static void CheckPhraseClipboard(TestWindow &window, ViewData &data,
                                 Shortcuts &keys, bool remapped) {
    assert(Load(
        keys, remapped
                  ? "<SHORTCUTS version='1'>"
                    "<BIND action='phrase.cut' keys='L+SELECT' event='press'/>"
                    "<BIND action='phrase.paste' keys='L+RIGHT' event='press'/>"
                    "<BIND action='phrase.begin_clone_selection' "
                    "keys='SELECT+B' event='press'/>"
                    "<BIND action='phrase.selection_extend' keys='L+SELECT' "
                    "event='press'/>"
                    "<BIND action='phrase.selection_copy' keys='SELECT' "
                    "exclude='L+B' event='press'/>"
                    "<BIND action='phrase.selection_interpolate' keys='L+UP' "
                    "event='press'/>"
                    "<BIND action='phrase.selection_cut' keys='L+DOWN' "
                    "event='press'/>"
                    "</SHORTCUTS>"
                  : "<SHORTCUTS version='1'/>"));
    data.currentPhrase_ = data.songX_ = data.chainRow_ = 0;
    data.playMode_ = PM_PHRASE;
    PhraseView view(window, &data);
    Phrase &phrase = *data.song_->phrase_;
    unsigned short cut = remapped ? EPBM_L | EPBM_SELECT : EPBM_A | EPBM_B;
    unsigned short paste = remapped ? EPBM_L | EPBM_RIGHT : EPBM_A | EPBM_L;
    unsigned short begin = remapped ? EPBM_SELECT | EPBM_B : EPBM_B | EPBM_L;
    phrase.note_[0] = 60;
    phrase.velocity_[0] = 40;
    phrase.instr_[0] = 7;
    phrase.param1_[0] = 1234;
    if (remapped)
        view.ProcessButtonMask(EPBM_A | EPBM_B, true);
    view.ProcessButtonMask(cut, false);
    assert(phrase.note_[0] == 60);
    view.ProcessButtonMask(cut, true);
    assert(phrase.note_[0] == 0xff && phrase.velocity_[0] == 127 &&
           phrase.instr_[0] == 0xff);
    assert(phrase.param1_[0] == 1234);
    view.ProcessButtonMask(paste, true);
    assert(phrase.note_[0] == 60 && phrase.velocity_[0] == 40 &&
           phrase.instr_[0] == 7);
    view.ProcessButtonMask(EPBM_UP, true);

    phrase.note_[2] = 64;
    view.ProcessButtonMask(begin, true);
    view.ProcessButtonMask(EPBM_DOWN, true);
    view.ProcessButtonMask(EPBM_DOWN, true);
    view.ProcessButtonMask(remapped ? EPBM_L | EPBM_UP : EPBM_B | EPBM_R, true);
    assert(phrase.note_[1] == 62);
    view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
    view.ProcessButtonMask(remapped ? EPBM_SELECT : EPBM_B, true);
    assert(data.phraseCurPos_ == 0);
    for (int row = 0; row < 15; ++row)
        view.ProcessButtonMask(EPBM_DOWN, true);
    view.ProcessButtonMask(paste, true);
    assert(data.phraseCurPos_ == 2 && data.currentPhrase_ == 0);
    assert(phrase.note_[15] == 61 && phrase.note_[0] == 63 &&
           phrase.note_[1] == 65);
    assert(phrase.velocity_[0] == 40 && phrase.instr_[0] == 7);

    view.ProcessButtonMask(begin, true);
    view.ProcessButtonMask(EPBM_DOWN, true);
    unsigned short extend = remapped ? EPBM_L | EPBM_SELECT : EPBM_B | EPBM_L;
    view.ProcessButtonMask(extend, true);
    view.ProcessButtonMask(extend, true);
    view.ProcessButtonMask(remapped ? EPBM_L | EPBM_DOWN : EPBM_A | EPBM_L,
                           true);
    assert(data.phraseCurPos_ == 2 && phrase.note_[15] == 0xff &&
           phrase.param1_[0] == 0);
    assert(phrase.instr_[0] == 0xff && phrase.velocity_[0] == 127);

    for (int col = 0; col < 3; ++col)
        view.ProcessButtonMask(EPBM_RIGHT, true);
    phrase.cmd1_[2] = I_CMD_TABL;
    phrase.param1_[2] = 0x37;
    view.ProcessButtonMask(cut, true);
    assert(phrase.cmd1_[2] == I_CMD_NONE && phrase.param1_[2] == 0);
    view.ProcessButtonMask(EPBM_RIGHT, true);
    view.ProcessButtonMask(EPBM_RIGHT, true);
    view.ProcessButtonMask(paste, true);
    assert(phrase.cmd2_[2] == I_CMD_TABL && phrase.param2_[2] == 0x37);
}

static void CheckPhraseEdits(TestWindow &window, ViewData &data,
                             Shortcuts &keys, bool remapped) {
    assert(Load(
        keys,
        remapped
            ? "<SHORTCUTS version='1'>"
              "<BIND action='phrase.edit_up' keys='L+UP' event='press'/>"
              "<BIND action='phrase.edit_left' keys='L+LEFT' event='press'/>"
              "<BIND action='phrase.command_picker_up' keys='SELECT' "
              "event='press'/>"
              "<BIND action='phrase.command_picker_down' keys='L+DOWN' "
              "event='press'/>"
              "<BIND action='phrase.selection_edit_right' keys='L+RIGHT' "
              "event='press'/>"
              "<BIND action='phrase.audition' keys='L+SELECT' event='press'/>"
              "<BIND action='phrase.audition_hold' keys='L' event='press'/>"
              "</SHORTCUTS>"
            : "<SHORTCUTS version='1'/>"));
    data.currentPhrase_ = data.songX_ = data.chainRow_ = 0;
    data.playMode_ = PM_PHRASE;
    PhraseView view(window, &data);
    Phrase &phrase = *data.song_->phrase_;
    unsigned short modifier = remapped ? EPBM_L : EPBM_A;
    unsigned short preview = remapped ? EPBM_L | EPBM_SELECT : EPBM_A;
    auditionCalls = stopCalls = 0;
    view.ProcessButtonMask(preview, false);
    assert(auditionCalls == 0);
    view.ProcessButtonMask(preview, true);
    assert(auditionCalls == 1);
    data.playMode_ = PM_AUDITION;
    view.ProcessButtonMask(modifier, true);
    assert(stopCalls == 0);
    view.ProcessButtonMask(EPBM_SELECT, true);
    assert(stopCalls == 1);
    data.playMode_ = PM_PHRASE;

    phrase.note_[0] = 12; // C0
    view.ProcessButtonMask(modifier | EPBM_LEFT, true);
    assert(phrase.note_[0] == 120); // C9
    view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
    assert(phrase.note_[0] == 12);
    view.ProcessButtonMask(EPBM_RIGHT, true);
    phrase.velocity_[0] = 100;
    if (remapped)
        view.ProcessButtonMask(EPBM_A | EPBM_UP, true);
    view.ProcessButtonMask(modifier | EPBM_UP, false);
    assert(phrase.velocity_[0] == 100);
    view.ProcessButtonMask(modifier | EPBM_UP, true);
    assert(phrase.velocity_[0] == 116);
    view.ProcessButtonMask(EPBM_RIGHT, true);
    view.ProcessButtonMask(EPBM_RIGHT, true);
    for (int lane = 0; lane < 2; ++lane) {
        modalCalls = 0;
        if (remapped)
            view.ProcessButtonMask(EPBM_A | EPBM_UP, true);
        assert(modalCalls == 0);
        view.ProcessButtonMask(remapped ? EPBM_SELECT : EPBM_A | EPBM_UP, true);
        view.ProcessButtonMask(modifier | EPBM_DOWN, true);
        assert(modalCalls == 2);
        ushort *parameter = lane ? phrase.param2_ : phrase.param1_;
        parameter[0] = 0x20;
        view.ProcessButtonMask(EPBM_RIGHT, true);
        view.ProcessButtonMask(modifier | EPBM_LEFT, true);
        view.ProcessButtonMask(modifier | EPBM_UP, true);
        assert(parameter[0] == 0x30);
        view.ProcessButtonMask(EPBM_A | EPBM_RIGHT,
                               true); // Return to low hex digit.
        view.ProcessButtonMask(EPBM_RIGHT, true);
    }

    PhraseView selection(window, &data);
    phrase.note_[0] = 60;
    phrase.note_[1] = 64;
    selection.ProcessButtonMask(EPBM_B | EPBM_L, true);
    selection.ProcessButtonMask(EPBM_DOWN, true);
    if (remapped)
        selection.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
    assert(phrase.note_[0] == 60 && phrase.note_[1] == 64);
    selection.ProcessButtonMask(modifier | EPBM_RIGHT, true);
    assert(phrase.note_[0] == 61 && phrase.note_[1] == 65);
}

int main(int argc, char **argv) {
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
        for (int pressed = 0; pressed < 2; ++pressed)
            for (int selection = 0; selection < 2; ++selection) {
                assert(Resolve(defaults, mask, pressed, selection) ==
                       Legacy(mask, pressed, selection));
                assert(Resolve(configured, mask, pressed, selection) ==
                       Legacy(mask, pressed, selection));
            }
    ViewData data(new Project);
    TestWindow window;
    Table &table = TableHolder::GetInstance()->GetTable(0);
    table.Reset();
    {
        TestTable view(window, &data);
        view.ProcessButtonMask(EPBM_RIGHT, true); // first parameter column
        table.param1_[0] = 0x1234;
        view.OnFocus(); // synchronize edit field with seeded value
        view.ProcessButtonMask(EPBM_A | EPBM_UP, true);
        assert(table.param1_[0] == 0x1235);
        view.ProcessButtonMask(EPBM_A | EPBM_LEFT, true); // higher hex digit
        view.ProcessButtonMask(EPBM_A | EPBM_UP, true);
        assert(table.param1_[0] == 0x1245);
        view.ProcessButtonMask(EPBM_A | EPBM_UP, true); // repeat
        assert(table.param1_[0] == 0x1255);
        view.ProcessButtonMask(EPBM_A, false);
        assert(table.param1_[0] == 0x1255);
    }
    table.Reset();
    {
        TestTable view(window, &data);
        view.ProcessButtonMask(EPBM_RIGHT, true);
        table.param1_[0] = 10;
        table.param1_[1] = 99;
        table.param1_[2] = 30;
        view.OnFocus();
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        assert(view.Selecting());
        view.ProcessButtonMask(EPBM_L, false);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_DOWN, true);
        assert(view.Selection().Top() == 0 && view.Selection().Bottom() == 2);
        view.ProcessButtonMask(EPBM_R | EPBM_B, true);
        assert(table.param1_[1] == 20);
        assert(view.Selecting());
        view.ProcessButtonMask(EPBM_B,
                               true); // copy and return to selection origin
        assert(!view.Selecting());
        for (int i = 0; i < 3; ++i)
            view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_L | EPBM_A, true);
        assert(table.param1_[3] == 10 && table.param1_[4] == 20 &&
               table.param1_[5] == 30);
        // Paste advances by clipboard height, so current row is now 6.
        table.param1_[6] = 44;
        view.ProcessButtonMask(EPBM_B | EPBM_A, true);
        assert(table.param1_[6] == 0);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_L | EPBM_A, true);
        assert(table.param1_[7] == 44);
    }
    table.Reset();
    {
        TestTable view(window, &data);
        view.ProcessButtonMask(EPBM_RIGHT, true);
        table.param1_[0] = 12;
        table.param1_[1] = 34;
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_L | EPBM_A, true); // cut selected rows
        assert(!view.Selecting());
        assert(table.param1_[0] == 0 && table.param1_[1] == 0);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_L | EPBM_A, true);
        assert(table.param1_[2] == 12 && table.param1_[3] == 34);
    }
    {
        TestTable view(window, &data);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true); // extend to full row
        assert(view.Selection().Left() == 0 && view.Selection().Right() == 6);
        assert(view.Selection().Top() == 0 && view.Selection().Bottom() == 0);
        view.ProcessButtonMask(EPBM_L, false); // releases must not extend
        assert(view.Selection().Bottom() == 0);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true); // entire table
        assert(view.Selection().Top() == 0 && view.Selection().Bottom() == 15);
    }
    for (int second = 0; second < 2; ++second) {
        TestTable view(window, &data, second ? VT_TABLE2 : VT_TABLE);
        SwitchObserver observer;
        view.AddObserver(observer);
        view.ProcessButtonMask(EPBM_R | EPBM_UP, true);
        assert(observer.views.back() == (second ? VT_INSTRUMENT : VT_PHRASE));
        view.ProcessButtonMask(EPBM_R | EPBM_LEFT, true);
        assert(observer.views.back() == (second ? VT_TABLE : VT_MIXER));
        unsigned count = observer.views.size();
        view.ProcessButtonMask(EPBM_R | EPBM_RIGHT, true);
        assert(observer.views.size() == count + (second ? 0 : 1));
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        view.ProcessButtonMask(EPBM_R | EPBM_UP, true);
        assert(observer.views.back() ==
               VT_PHRASE); // original selection behavior
        view.RemoveObserver(observer);
    }
    {
        table.Reset();
        TestTable view(window, &data);
        table.cmd1_[0] = I_CMD_ARPG;
        table.param1_[0] = 0x1234;
        view.ProcessButtonMask(EPBM_B | EPBM_A, true);
        assert(table.cmd1_[0] == I_CMD_NONE && table.param1_[0] == 0);
        view.ProcessButtonMask(EPBM_DOWN, true);
        view.ProcessButtonMask(EPBM_L | EPBM_A, true);
        assert(table.cmd1_[1] == I_CMD_ARPG && table.param1_[1] == 0x1234);
        modalCalls = 0;
        view.ProcessButtonMask(EPBM_A | EPBM_UP, true);
        assert(modalCalls == 1);
        view.ProcessButtonMask(EPBM_A | EPBM_DOWN, true);
        assert(modalCalls == 2);
        view.ProcessButtonMask(EPBM_A | EPBM_UP, false);
        assert(modalCalls == 2);
    }
    for (int selection = 0; selection < 2; ++selection) {
        TestTable view(window, &data);
        if (selection)
            view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        playCalls = 0;
        view.ProcessButtonMask(EPBM_START, true);
        assert(playCalls == 1 && !playFromStart);
        view.ProcessButtonMask(EPBM_R | EPBM_START, true);
        assert(playCalls == 2 && playFromStart);
        view.ProcessButtonMask(EPBM_R, false);
        assert(playCalls == 2);
    }
    assert(Load(
        configured,
        "<SHORTCUTS version='1'>"
        "<BIND action='table.select' keys='SELECT+B' event='press'/>"
        "<BIND action='table.selection_extend' keys='SELECT+B' event='press'/>"
        "<BIND action='table.cut' keys='SELECT+A' event='press'/>"
        "</SHORTCUTS>"));
    {
        TestTable view(window, &data);
        view.ProcessButtonMask(EPBM_RIGHT, true);
        table.param1_[0] = 55;
        view.ProcessButtonMask(EPBM_B | EPBM_A, true);
        assert(table.param1_[0] == 55);
        view.ProcessButtonMask(EPBM_SELECT | EPBM_A, true);
        assert(table.param1_[0] == 0);
        view.ProcessButtonMask(EPBM_L | EPBM_B, true);
        assert(!view.Selecting());
        view.ProcessButtonMask(EPBM_SELECT | EPBM_B, true);
        assert(view.Selecting());
        view.ProcessButtonMask(EPBM_SELECT | EPBM_B, true);
        assert(view.Selection().Right() == 6);
    }
    assert(!Load(configured,
                 "<SHORTCUTS version='1'>"
                 "<BIND action='table.cut' keys='A+B' event='release'/>"
                 "</SHORTCUTS>"));
    assert(configured.Matches(Shortcuts::TableCut, EPBM_SELECT | EPBM_A, true));
    // The editor opens this shared dialog with A+direction still held.
    TestTable parent(window, &data);
    FourCC command = I_CMD_ARPG;
    CommandSelectorModal picker(parent, &command);
    picker.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
    assert(!picker.IsFinished() && command != I_CMD_ARPG);
    picker.ProcessButtonMask(EPBM_A, false);
    assert(!picker.IsFinished());
    picker.ProcessButtonMask(EPBM_B, true);
    assert(picker.IsFinished() && picker.GetReturnCode() == 0);
    assert(command == I_CMD_ARPG);

    assert(Load(
        configured,
        "<SHORTCUTS version='1'>"
        "<BIND action='command_selector.confirm' keys='SELECT' event='press'/>"
        "</SHORTCUTS>"));
    CommandSelectorModal remapped(parent, &command);
    remapped.ProcessButtonMask(EPBM_RIGHT, true);
    FourCC preview = command;
    remapped.ProcessButtonMask(EPBM_A, true);
    assert(!remapped.IsFinished());
    remapped.ProcessButtonMask(EPBM_SELECT, true);
    assert(remapped.IsFinished() && remapped.GetReturnCode() == 1);
    assert(command == preview && command != I_CMD_ARPG);
    assert(argc == 3);
    Path folder(argv[1]);
    TestProjectPicker projects(parent, folder);
    const std::string first = projects.GetCurrentProjectPath().GetName();
    projects.ProcessButtonMask(EPBM_B | EPBM_DOWN, true);
    projects.ProcessButtonMask(EPBM_B | EPBM_UP, true);
    assert(projects.GetCurrentProjectPath().GetName() == first);
    for (int i = 0;
         i < 3 && projects.GetCurrentProjectPath().GetName() != "lgpt_one"; ++i)
        projects.ProcessButtonMask(EPBM_DOWN, true);
    assert(projects.GetCurrentProjectPath().GetName() == "lgpt_one");
    int prompts = modalCalls;
    projects.ProcessButtonMask(EPBM_B | EPBM_A | EPBM_DOWN, true);
    assert(modalCalls == prompts + 1 && !projects.IsFinished());
    assert(projects.GetCurrentProjectPath().GetName() == "lgpt_one");
    assert(Load(configured,
                "<SHORTCUTS version='1'><BIND action='project_picker.activate' "
                "keys='SELECT' event='press'/></SHORTCUTS>"));
    projects.ProcessButtonMask(EPBM_A, true);
    projects.ProcessButtonMask(EPBM_SELECT, false);
    assert(!projects.IsFinished());
    projects.ProcessButtonMask(EPBM_SELECT, true);
    assert(projects.IsFinished() && projects.GetReturnCode() == 1);
    assert(projects.GetSelection().GetName() == "lgpt_one");
    assert(
        Load(configured,
             "<SHORTCUTS version='1'><BIND action='new_project.keyboard_input' "
             "keys='SELECT' event='press'/></SHORTCUTS>"));
    NewProjectDialog nameDialog(parent, folder);
    nameDialog.OnFocus();
    nameDialog.ProcessButtonMask(EPBM_A, true); // enter keyboard
    nameDialog.ProcessButtonMask(EPBM_A, true); // old input key
    nameDialog.ProcessButtonMask(EPBM_SELECT, false);
    nameDialog.ProcessButtonMask(EPBM_SELECT, true); // A
    nameDialog.ProcessButtonMask(EPBM_RIGHT, true);
    nameDialog.ProcessButtonMask(EPBM_SELECT, true); // S
    nameDialog.ProcessButtonMask(EPBM_B, true);      // erase S
    nameDialog.ProcessButtonMask(EPBM_START, true);
    nameDialog.ProcessButtonMask(EPBM_DOWN, true);  // Random
    nameDialog.ProcessButtonMask(EPBM_RIGHT, true); // Ok
    nameDialog.ProcessButtonMask(EPBM_A, true);
    assert(nameDialog.IsFinished() && nameDialog.GetReturnCode() == 1);
    assert(nameDialog.GetName() == "lgpt_A");

    NewProjectDialog cancel(parent, folder);
    cancel.OnFocus();
    cancel.ProcessButtonMask(EPBM_A | EPBM_UP, true); // space to '!'
    cancel.ProcessButtonMask(EPBM_DOWN, true);
    cancel.ProcessButtonMask(EPBM_RIGHT, true);
    cancel.ProcessButtonMask(EPBM_RIGHT, true);
    cancel.ProcessButtonMask(EPBM_A, true);
    assert(cancel.IsFinished() && cancel.GetReturnCode() == 0);
    assert(cancel.GetName() == "lgpt_!");
    // Confirm takes the pre-movement answer, including in a deletion prompt.
    MessageBox prompt(parent, "Delete?", MBBF_YES | MBBF_NO);
    prompt.ProcessButtonMask(EPBM_A | EPBM_LEFT, true);
    assert(prompt.IsFinished() && prompt.GetReturnCode() == MBL_NO);

    MessageBox released(parent, "Delete?", MBBF_YES | MBBF_NO);
    released.ProcessButtonMask(EPBM_LEFT, false); // another button released
    released.ProcessButtonMask(0, false);
    assert(!released.IsFinished());
    released.ProcessButtonMask(EPBM_A, false);
    assert(released.IsFinished() && released.GetReturnCode() == MBL_YES);

    assert(
        Load(configured,
             "<SHORTCUTS version='1'>"
             "<BIND action='message_box.confirm' keys='SELECT' event='press'/>"
             "<BIND action='message_box.confirm_release' keys='SELECT' "
             "event='release'/>"
             "</SHORTCUTS>"));
    MessageBox changed(parent, "Delete?", MBBF_YES | MBBF_NO);
    changed.ProcessButtonMask(EPBM_A, true);
    changed.ProcessButtonMask(EPBM_A, false);
    assert(!changed.IsFinished());
    changed.ProcessButtonMask(EPBM_RIGHT, true);
    changed.ProcessButtonMask(EPBM_SELECT, true);
    assert(changed.IsFinished() && changed.GetReturnCode() == MBL_YES);
    Path library(argv[2]);
    TestSampleBrowser browser(parent, library);
    browser.ProcessButtonMask(EPBM_DOWN, true);         // sub
    assert(browser.FolderChangeRequestsRedraw(EPBM_A)); // enter folder
    browser.ProcessButtonMask(EPBM_START | EPBM_DOWN, true);
    assert(previewed == "a.wav");
    browser.ProcessButtonMask(EPBM_START | EPBM_DOWN, true);
    assert(previewed == "b.wav");
    browser.ProcessButtonMask(EPBM_B | EPBM_UP, true); // page to ..
    assert(previewed.empty());
    assert(
        browser.FolderChangeRequestsRedraw(EPBM_START | EPBM_LEFT)); // parent
    browser.ProcessButtonMask(EPBM_DOWN, true);
    assert(Load(configured,
                "<SHORTCUTS version='1'><BIND action='sample_browser.load' "
                "keys='SELECT' event='press'/></SHORTCUTS>"));
    browser.ProcessButtonMask(EPBM_START | EPBM_RIGHT, true);
    assert(previewed == "root.wav" && imported.empty());
    browser.ProcessButtonMask(EPBM_SELECT, false);
    assert(imported.empty());
    browser.ProcessButtonMask(EPBM_SELECT, true);
    assert(imported == "root.wav" && previewed.empty());
    // An occupied slot takes the real error path before any audio device opens.
    data.song_->data_[data.songX_ + (data.songY_ + data.songOffset_) *
                                        SONG_CHANNEL_COUNT] = 0;
    RecordDialog record(parent);
    assert(Load(configured,
                "<SHORTCUTS version='1'>"
                "<BIND action='record.close' keys='SELECT+R' event='press'/>"
                "</SHORTCUTS>"));
    assert(configured.KeyNames(Shortcuts::RecordClose) == "R+SELECT");
    record.ProcessButtonMask(EPBM_A, true);
    record.ProcessButtonMask(EPBM_SELECT | EPBM_R, false);
    assert(!record.IsFinished());
    record.ProcessButtonMask(EPBM_SELECT | EPBM_R, true);
    assert(record.IsFinished() && record.GetReturnCode() == 0);
    CheckPhraseClone(window, data, configured, false);
    CheckPhraseClone(window, data, configured, true);
    CheckPhraseClipboard(window, data, configured, false);
    CheckPhraseClipboard(window, data, configured, true);
    CheckPhraseEdits(window, data, configured, false);
    CheckPhraseEdits(window, data, configured, true);
    return 0;
}
