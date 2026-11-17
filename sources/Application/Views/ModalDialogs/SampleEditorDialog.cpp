#include "SampleEditorDialog.h"
#include "Application/AppWindow.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Player/PlayerMixer.h"
#include "MessageBox.h"
#include "Services/Audio/Audio.h"
#include <cstring>
#include <memory>
#include <new>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef MessageBox
#endif

// TinyXML's legacy stdio aliases must not intercept native WAV file I/O.
#undef FILE
#undef fopen
#undef fclose

static const char *operations[] = {"Crop",    "Delete",  "Normalize",
                                   "Silence", "Fade In", "Fade Out"};

SampleEditorDialog::SampleEditorDialog(View &parent)
    : ModalView(parent),
      action_(Config::GetInstance()->GetValue("SHORT_PRESS_MS")),
      undoFirst_(0), undoLast_(0), undoAvailable_(false), undoDirty_(false),
      undoZoom_(false), first_(0), last_(0), row_(0), digit_(0), operation_(0),
      nameCursor_(0), sampleIndex_(-1), dirty_(false), playing_(false),
      zoom_(false) {
    SampleInstrument *instrument = static_cast<SampleInstrument *>(
        viewData_->project_->GetInstrumentBank()->GetInstrument(
            viewData_->currentInstrument_));
    sampleIndex_ = instrument->GetSampleIndex();
    SoundSource *source =
        sampleIndex_ < 0 ? 0
                         : SamplePool::GetInstance()->GetSource(sampleIndex_);
    original_ = instrument->GetFileName();
    name_ = original_.substr(0, original_.find_last_of('.')).substr(0, 11) +
            "_edit";
    name_.resize(16, ' ');
    // SoundFonts are deliberately excluded: overwrite must target a WAV file.
    if (!source || source->IsMulti() ||
        (original_.find(".wav") == std::string::npos &&
         original_.find(".WAV") == std::string::npos)) {
        status_ = "Select a WAV sample first";
        return;
    }
    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    int frames = source->GetSize(-1);
    if (frames <= 0 || rate <= 0 || (channels != 1 && channels != 2) ||
        !source->GetSampleBuffer(-1)) {
        status_ = "Unsupported or empty sample";
        return;
    }
    buffer_.channels = channels;
    buffer_.rate = rate;
    try {
        short *pcm = static_cast<short *>(source->GetSampleBuffer(-1));
        buffer_.pcm.assign(pcm, pcm + size_t(frames) * buffer_.channels);
        last_ = frames;
    } catch (const std::bad_alloc &) {
        status_ = "Not enough memory";
    }
}

SampleEditorDialog::~SampleEditorDialog() { stopPreview(); }

void SampleEditorDialog::text(int x, int y, const char *s, bool selected) {
    GUITextProperties p;
    p.invert_ = selected;
    SetColor(selected ? CD_HILITE2 : CD_NORMAL);
    DrawString(x, y, s, p);
}

void SampleEditorDialog::DrawView() {
    SetWindow(36, 23);
    text(0, 0, dirty_ ? "SAMPLE EDIT *" : "SAMPLE EDIT");
    text(0, 1, original_.substr(0, 35).c_str());
    int size = buffer_.GetSize(-1);
    int from = zoom_ ? first_ : 0, to = zoom_ ? last_ : size;
    AppWindow &window = static_cast<AppWindow &>(w_);
    const bool pixels = window.SupportsPixelRects();
    const int width = pixels ? 36 * AppWindow::GetCharWidth() : 36;
    const int height = pixels ? 7 * AppWindow::GetCharHeight() : 7;
    // Partition the displayed frame interval into native pixel columns. Scan
    // every frame in each bin, so narrow transients are never subsampled away.
    for (int x = 0; size && x < width; ++x) {
        int a = from + static_cast<long long>(to - from) * x / width;
        int b = from + static_cast<long long>(to - from) * (x + 1) / width;
        b = std::min(to, std::max(a + 1, b));
        int low = 32767, high = -32768;
        for (int f = a; f < b; ++f)
            for (int c = 0; c < buffer_.channels; ++c) {
                int v = buffer_.pcm[size_t(f) * buffer_.channels + c];
                low = std::min(low, v);
                high = std::max(high, v);
            }
        bool selected = a < last_ && b > first_;
        int top = static_cast<long long>(32767 - high) * (height - 1) / 65535;
        int bottom = static_cast<long long>(32767 - low) * (height - 1) / 65535;
        if (pixels) {
            // SetWindow(36, 23) places this modal at character (2, 2).
            int px = 2 * AppWindow::GetCharWidth() + x;
            int py = 5 * AppWindow::GetCharHeight();
            window.AddPixelRect(GUIRect(px, py + top, px + 1, py + bottom + 1),
                                selected);
        } else {
            SetColor(selected ? CD_HILITE2 : CD_HILITE1);
            GUITextProperties p;
            p.invert_ = true;
            for (int y = top; y <= bottom; ++y)
                DrawString(x, 3 + y, " ", p);
        }
    }
    char line[64];
    sprintf(line, "Start %08X", first_);
    text(0, 11, line, row_ == 0);
    sprintf(line, "End   %08X", last_);
    text(19, 11, line, row_ == 1);
    if (row_ < 2)
        text((row_ ? 19 : 0) + 13 - digit_, 12, "^");
    text(0, 14, "Process");
    text(10, 14, operations[operation_], row_ == 2);
    text(26, 14, undoAvailable_ ? "Undo" : "----", row_ == 3);
    text(0, 16, "Name");
    text(10, 16, name_.c_str(), row_ == 4);
    if (row_ == 4)
        text(10 + nameCursor_, 17, "^");
    text(0, 18, "Save As", row_ == 5);
    text(12, 18, "Overwrite", row_ == 6);
    text(27, 18, "Exit", row_ == 7);
    text(0, 20, status_.substr(0, 36).c_str());
    const Shortcuts &keys = Shortcuts::Get();
    text(0, 21,
         ("Tap " + keys.KeyNames(Shortcuts::EditorTap) + ": execute")
             .substr(0, 36)
             .c_str());
    text(0, 22,
         (keys.KeyNames(Shortcuts::EditorPreview) + ": listen   " +
          keys.KeyNames(Shortcuts::EditorZoom) + ": zoom selection")
             .substr(0, 36)
             .c_str());
}

void SampleEditorDialog::stopPreview() {
    PlayerMixer *mixer = PlayerMixer::GetInstance();
    mixer->Lock();
    Player::GetInstance()->StopStreaming();
    mixer->Unlock();
    playing_ = false;
    if (!previewPath_.empty()) {
        std::remove(previewPath_.c_str());
        previewPath_.clear();
    }
}

void SampleEditorDialog::preview() {
    if (playing_) {
        stopPreview();
        return;
    }
    // Use a new path each time: the streaming reader may still hold the old
    // file.
    static unsigned int serial = 0;
    do {
        char leaf[64];
        sprintf(leaf, "samples:.editor-preview-%u.tmp", ++serial);
        previewPath_ = Path(leaf).GetPath();
    } while (Path(previewPath_).Exists());
    FILE *file = fopen(previewPath_.c_str(), "wb");
    bool ok = buffer_.Write(file, first_, last_,
                            Audio::GetInstance()->GetSampleRate());
    if (file && fclose(file))
        ok = false;
    if (!ok) {
        stopPreview();
        status_ = "Could not create preview";
        return;
    }
    PlayerMixer *mixer = PlayerMixer::GetInstance();
    mixer->Lock();
    Player::GetInstance()->StartStreaming(Path(previewPath_));
    mixer->Unlock();
    playing_ = true;
    status_ = "Playing selection";
}

void SampleEditorDialog::ConfirmOverwrite(View &v, ModalView &d) {
    SampleEditorDialog &editor = static_cast<SampleEditorDialog &>(v);
    if (d.GetReturnCode() == MBL_YES) {
        try {
            editor.save(true);
        } catch (const std::bad_alloc &) {
            editor.status_ = "Not enough memory";
        }
    }
}
void SampleEditorDialog::ConfirmExit(View &v, ModalView &d) {
    if (d.GetReturnCode() == MBL_YES)
        static_cast<SampleEditorDialog &>(v).EndModal(0);
}

void SampleEditorDialog::save(bool overwrite) {
    stopPreview();
    std::string name = name_;
    name.erase(name.find_last_not_of(' ') + 1);
    if (!overwrite &&
        (name.empty() ||
         name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrst"
                                "uvwxyz0123456789_- ") != std::string::npos)) {
        status_ = "Use letters, digits, _ or -";
        return;
    }
    name = overwrite ? original_ : name + ".wav";
    Path target(std::string("samples:") + name);
    if (!overwrite && target.Exists()) {
        status_ = "Name exists; choose another name";
        return;
    }
    SamplePool *pool = SamplePool::GetInstance();
    if (!overwrite && pool->GetNameListSize() >= MAX_PIG_SAMPLES) {
        status_ = "Sample pool is full";
        return;
    }
    std::auto_ptr<SampleEditBuffer> saved(new SampleEditBuffer(buffer_));
    std::string temporary = target.GetPath() + ".edit-tmp";
    if (Path(temporary).Exists()) {
        status_ = "Temporary file already exists";
        return;
    }
    char *savedName = static_cast<char *>(SYS_MALLOC(name.size() + 1));
    if (!savedName) {
        status_ = "Not enough memory";
        return;
    }
    strcpy(savedName, name.c_str());
    FILE *file = fopen(temporary.c_str(), "wb");
    bool ok = buffer_.Write(file, 0, buffer_.GetSize(-1));
    if (file && fclose(file))
        ok = false;
    if (ok) {
#ifdef _WIN32
        ok = MoveFileExA(temporary.c_str(), target.GetPath().c_str(),
                         overwrite ? MOVEFILE_REPLACE_EXISTING : 0) != 0;
#else
        ok = std::rename(temporary.c_str(), target.GetPath().c_str()) == 0;
#endif
    }
    if (!ok) {
        std::remove(temporary.c_str());
        SAFE_FREE(savedName);
        status_ = "Save failed; original retained";
        return;
    }
    Player::GetInstance()->Stop();
    PlayerMixer *mixer = PlayerMixer::GetInstance();
    mixer->Lock();
    int index = overwrite ? sampleIndex_ : pool->GetNameListSize();
    pool->InstallEditedSample(index, savedName, saved.release());
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    for (int i = 0; i < MAX_INSTRUMENT_COUNT; ++i) {
        I_Instrument *base = bank->GetInstrument(i);
        if (base->GetType() != IT_SAMPLE)
            continue;
        SampleInstrument *instrument = static_cast<SampleInstrument *>(base);
        if (i == viewData_->currentInstrument_ ||
            (overwrite && instrument->GetSampleIndex() == index))
            instrument->AssignSample(index);
    }
    mixer->Unlock();
    sampleIndex_ = index;
    original_ = name;
    dirty_ = false;
    undoAvailable_ = false;
    std::vector<short>().swap(undoBuffer_.pcm);
    status_ = "Saved (16-bit PCM WAV)";
}

void SampleEditorDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    const Shortcuts &keys = Shortcuts::Get();
    const bool up = keys.Matches(Shortcuts::EditorEditUp, mask, pressed);
    const bool down = keys.Matches(Shortcuts::EditorEditDown, mask, pressed);
    const bool left = keys.Matches(Shortcuts::EditorEditLeft, mask, pressed);
    const bool right = keys.Matches(Shortcuts::EditorEditRight, mask, pressed);
    const bool editing = up || down || left || right;
    const bool exitEditor = keys.Matches(Shortcuts::EditorExit, mask, pressed);
    const bool listen = keys.Matches(Shortcuts::EditorPreview, mask, pressed);
    const bool zoom = keys.Matches(Shortcuts::EditorZoom, mask, pressed);
    // Test the held chord on both event edges. Only an isolated short tap
    // executes.
    const bool tapHeld = keys.Matches(Shortcuts::EditorTap, mask, true);
    const bool click = action_.UpdateChord(
        tapHeld, !keys.RemainingButtons(Shortcuts::EditorTap, mask), mask,
        pressed, editing || exitEditor || listen || zoom, SDL_GetTicks());
    if (!pressed && !click)
        return;
    isDirty_ = true;
    try {
        if (exitEditor || (click && row_ == 7)) {
            stopPreview();
            if (dirty_)
                DoModal(new MessageBox(*this, "Discard unsaved edits?",
                                       MBBF_YES | MBBF_NO),
                        ConfirmExit);
            else
                EndModal(0);
            return;
        }
        if (buffer_.pcm.empty())
            return;
        if (listen) {
            preview();
            return;
        }
        if (zoom) {
            zoom_ = !zoom_;
            return;
        }
        if (editing || tapHeld || click) {
            int direction = up ? 1 : down ? -1 : 0;
            if (row_ < 2) {
                if (left)
                    digit_ = std::min(7, digit_ + 1);
                if (right)
                    digit_ = std::max(0, digit_ - 1);
                if (direction) {
                    stopPreview();
                    long long value = (row_ ? last_ : first_) +
                                      direction * (1LL << (4 * digit_));
                    int minimum = row_ ? first_ + 1 : 0;
                    int maximum = row_ ? buffer_.GetSize(-1) : last_ - 1;
                    (row_ ? last_ : first_) = std::max(
                        static_cast<long long>(minimum),
                        std::min(static_cast<long long>(maximum), value));
                }
            } else if (row_ == 2) {
                if (left)
                    operation_ = (operation_ + 5) % 6;
                if (right)
                    operation_ = (operation_ + 1) % 6;
                if (click) {
                    stopPreview();
                    SampleEditBuffer edited(buffer_);
                    if (edited.Process(static_cast<SampleEditBuffer::Operation>(
                                           operation_),
                                       first_, last_)) {
                        undoBuffer_.channels = buffer_.channels;
                        undoBuffer_.rate = buffer_.rate;
                        undoBuffer_.pcm.swap(buffer_.pcm);
                        buffer_.pcm.swap(edited.pcm);
                        undoFirst_ = first_;
                        undoLast_ = last_;
                        undoDirty_ = dirty_;
                        undoZoom_ = zoom_;
                        undoAvailable_ = true;
                        dirty_ = true;
                        if (operation_ <= SampleEditBuffer::DELETE) {
                            first_ = 0;
                            last_ = buffer_.GetSize(-1);
                        }
                        status_ = "Selection processed";
                    } else
                        status_ = "Cannot delete the entire sample";
                }
            } else if (row_ == 3 && click) {
                if (undoAvailable_) {
                    stopPreview();
                    buffer_.pcm.swap(undoBuffer_.pcm);
                    std::vector<short>().swap(undoBuffer_.pcm);
                    first_ = undoFirst_;
                    last_ = undoLast_;
                    dirty_ = undoDirty_;
                    zoom_ = undoZoom_;
                    undoAvailable_ = false;
                    status_ = "Last process undone";
                }
            } else if (row_ == 4) {
                if (left)
                    nameCursor_ = std::max(0, nameCursor_ - 1);
                if (right)
                    nameCursor_ = std::min(15, nameCursor_ + 1);
                const char *alphabet = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijkl"
                                       "mnopqrstuvwxyz0123456789_-";
                const char *p = strchr(alphabet, name_[nameCursor_]);
                int n = p ? p - alphabet : 0;
                if (direction)
                    name_[nameCursor_] =
                        alphabet[(n + direction + strlen(alphabet)) %
                                 strlen(alphabet)];
            } else if (click && row_ == 5)
                save(false);
            else if (click && row_ == 6)
                DoModal(new MessageBox(*this, "Overwrite sample file?",
                                       MBBF_YES | MBBF_NO),
                        ConfirmOverwrite);
        } else {
            if (keys.Matches(Shortcuts::EditorDown, mask, pressed) ||
                keys.Matches(Shortcuts::EditorRight, mask, pressed))
                row_ = (row_ + 1) % 8;
            if (keys.Matches(Shortcuts::EditorUp, mask, pressed) ||
                keys.Matches(Shortcuts::EditorLeft, mask, pressed))
                row_ = (row_ + 7) % 8;
        }
    } catch (const std::bad_alloc &) {
        status_ = "Not enough memory";
    }
}
