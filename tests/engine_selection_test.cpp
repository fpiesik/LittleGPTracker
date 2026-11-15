#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Instruments/PlaitsInstrument.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Persistency/PersistencyService.h"
#include "Application/Player/PlayerMixer.h"
#include "Application/Views/BaseClasses/UIIntVarField.h"
#include "Application/Views/InstrumentView.h"
#include "Services/Audio/Audio.h"
#include "Services/Midi/MidiService.h"
#include "UIFramework/Interfaces/I_GUIWindowImp.h"
#include <cassert>
#include <cstdio>
#include <cstring>

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

static Variable &Focused(InstrumentView &view) {
    return static_cast<UIIntVarField *>(view.GetFocus())->GetVariable();
}

int main() {
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
    PlaitsInstrument::InitVoices(44100);
    TablePlayback::Reset();
    // ViewData owns and deletes its project.
    ViewData data(new Project);
    data.currentInstrument_ = 0;
    InstrumentBank *bank = data.project_->GetInstrumentBank();
    TestWindow window;
    {
        InstrumentView view(window, &data);
        assert(!std::strcmp(Focused(view).GetString(), "Sampler"));
        int selectorY = view.GetFocus()->GetPosition()._y;
        IteratorPtr<UIField> fields(view.GetIterator());
        int highlighted = 0;
        for (fields->Begin(); !fields->IsDone(); fields->Next()) {
            highlighted += fields->CurrentItem().HasFocus();
            assert(fields->CurrentItem().GetPosition()._y < 30);
        }
        assert(highlighted == 1);
        // Releasing buttons, or pressing B+A, must not change the engine.
        view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, false);
        view.ProcessButtonMask(EPBM_B | EPBM_A | EPBM_RIGHT, true);
        assert(bank->GetInstrument(0)->GetType() == IT_SAMPLE);
        view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(bank->GetInstrument(0)->GetType() == IT_PLAITS);
        assert(!std::strcmp(Focused(view).GetString(), "Plaits"));
        view.ProcessButtonMask(EPBM_DOWN, true);
        assert(!std::strcmp(Focused(view).GetName(), "MODEL"));
        assert(view.GetFocus()->GetPosition()._y > selectorY);
        view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
        assert(bank->GetInstrument(0)->FindVariable("MODEL")->GetInt() == 9);
        TiXmlElement saved("INSTRUMENTBANK");
        bank->SaveContent(&saved);
        assert(
            !std::strcmp(saved.FirstChildElement()->Attribute("TYPE"), "PLTS"));

        // Repeated switches rebuild fields without retaining a deleted
        // variable.
        view.ProcessButtonMask(EPBM_UP, true);
        for (int i = 0; i < 8; ++i) {
            view.ProcessButtonMask(EPBM_A | EPBM_LEFT, true);
            assert(bank->GetInstrument(0)->GetType() == IT_SAMPLE);
            view.ProcessButtonMask(EPBM_DOWN, true);
            assert(Focused(view).GetID() == SIP_SAMPLE);
            view.ProcessButtonMask(EPBM_UP, true);
            view.ProcessButtonMask(EPBM_A | EPBM_RIGHT, true);
            assert(bank->GetInstrument(0)->GetType() == IT_PLAITS);
        }
        // Simulate another view replacing the selected slot while hidden.
        assert(bank->SetType(0, IT_SAMPLE));
        view.OnFocus();
        assert(!std::strcmp(Focused(view).GetString(), "Sampler"));
        data.currentInstrument_ = MAX_SAMPLEINSTRUMENT_COUNT;
        view.OnFocus();
        assert(!std::strcmp(Focused(view).GetName(), "channel"));
        assert(!bank->SetType(MAX_SAMPLEINSTRUMENT_COUNT, IT_PLAITS));
        assert(!bank->SetType(-1, IT_PLAITS));
        assert(!bank->SetType(0, IT_MIDI));
    }

    // Verify that active voices, cached instruments and old table automation
    // cannot retain the replaced engine; unrelated channels continue playing.
    PlayerMixer mixer;
    PlaitsInstrument old, replacement, unrelated;
    mixer.StartInstrument(0, &old, 60, true);
    mixer.StartInstrument(1, &unrelated, 72, true);
    mixer.StartInstrument(2, &old, 64, true);
    mixer.StopInstrument(2); // Last-instrument cache survives note-off.
    Table &table = TableHolder::GetInstance()->GetTable(0);
    TablePlayback::GetTablePlayback(0).Start(&old, table, true);
    TablePlayback::GetTablePlayback(1).Start(&unrelated, table, true);
    mixer.ReplaceInstrument(&old, &replacement);
    assert(!mixer.GetInstrument(0));
    assert(mixer.GetLastInstrument(0) == &replacement);
    assert(mixer.GetLastInstrument(2) == &replacement);
    assert(mixer.GetInstrument(1) == &unrelated);
    assert(!TablePlayback::GetTablePlayback(0).GetTable());
    assert(TablePlayback::GetTablePlayback(1).GetTable());
    mixer.StartInstrument(0, mixer.GetLastInstrument(0), 67, false);
    assert(mixer.GetInstrument(0) == &replacement);
    mixer.StopInstrument(0);
    mixer.StopInstrument(1);
    std::puts("Engine selector, GUI rebuild, persistence and playback "
              "replacement passed (headless)");
}
