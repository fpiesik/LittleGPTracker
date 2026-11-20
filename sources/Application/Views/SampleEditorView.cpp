#include "SampleEditorView.h"
#include "Application/AppWindow.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Player/SyncMaster.h"
#include "ModalDialogs/MessageBox.h"
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

// Beat positions are displayed and edited in ten-thousandths of a beat.
static long long beatUnits(int frame, int rate, int bpm) {
    return static_cast<long long>(
        frame * (double(bpm) * 10000 / (60.0 * rate)) + 0.5);
}

static int wholeBeatDigits(long long units) {
    int digits = 4;
    for (long long whole = units / 10000; whole >= 10000; whole /= 10)
        ++digits;
    return digits;
}

SampleEditorView::SampleEditorView(GUIWindow &window, ViewData *data)
    : View(window, data), previewProgress_(-1) {}

void SampleEditorView::OnFocus() {
    waveform_.clear();
    beatDigit_ = 4; // Start on whole beats.
    displayedBpm_ = 0;
    action_ =
        SampleEditorAction(Config::GetInstance()->GetValue("SHORT_PRESS_MS"));
    first_ = last_ = row_ = digit_ = operation_ = nameCursor_ = 0;
    undoAvailable_ = dirty_ = zoom_ = false;
    status_.clear();

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

SampleEditorView::~SampleEditorView() {
    if (!previewPath_.empty())
        stopPreview();
}

void SampleEditorView::text(int x, int y, const char *s, bool selected) {
    GUITextProperties p;
    p.invert_ = selected;
    SetColor(selected ? CD_HILITE2 : CD_NORMAL);
    DrawString(x, y, s, p);
}

void SampleEditorView::DrawView() {
    Clear();
    AppWindow &window = static_cast<AppWindow &>(w_);
    const GUIRect screen = window.GetRect();
    const int charWidth = AppWindow::GetCharWidth();
    const int charHeight = AppWindow::GetCharHeight();
    const int columns =
        std::min(screen.Width() / charWidth, int(AppWindow::SCREEN_COLUMNS));
    const int rows =
        std::min(screen.Height() / charHeight, int(AppWindow::SCREEN_ROWS));
    const GUIPoint title = GetTitlePosition();
    const int waveformY = title._y + 3;
    const int controlsY = rows - 12;
    const bool wide = columns >= 52;
    const int samplesY = controlsY - (wide ? 2 : 4);
    const int waveformRows = samplesY - waveformY - 1;
    text(title._x, title._y, dirty_ ? "SAMPLE EDIT *" : "SAMPLE EDIT");
    text(0, title._y + 1, original_.substr(0, columns).c_str());

    int size = buffer_.GetSize(-1);
    int from = zoom_ ? first_ : 0, to = zoom_ ? last_ : size;
    const bool pixels = window.SupportsPixelRects();
    const int width = pixels ? columns * charWidth : columns;
    const int height = pixels ? waveformRows * charHeight : waveformRows;
    const bool updateWaveform = static_cast<int>(waveform_.size()) != width;
    if (updateWaveform && size && height > 0)
        waveform_.resize(width);
    // Partition the displayed frame interval into native pixel columns. Scan
    // each bin once per waveform change to preserve narrow transients.
    for (int x = 0; size && height > 0 && x < width; ++x) {
        int a = from + static_cast<long long>(to - from) * x / width;
        int b = from + static_cast<long long>(to - from) * (x + 1) / width;
        b = std::min(to, std::max(a + 1, b));
        WaveformColumn &column = waveform_[x];
        if (updateWaveform) {
            column.low = 32767;
            column.high = -32768;
            for (int f = a; f < b; ++f)
                for (int c = 0; c < buffer_.channels; ++c) {
                    int v = buffer_.pcm[size_t(f) * buffer_.channels + c];
                    column.low = std::min(column.low, v);
                    column.high = std::max(column.high, v);
                }
        }
        bool selected = a < last_ && b > first_;
        ColorDefinition color = selected ? CD_HILITE2 : CD_HILITE1;
        int top =
            static_cast<long long>(32767 - column.high) * (height - 1) / 65535;
        int bottom =
            static_cast<long long>(32767 - column.low) * (height - 1) / 65535;
        if (pixels) {
            int py = waveformY * charHeight;
            window.AddPixelRect(GUIRect(x, py + top, x + 1, py + bottom + 1),
                                color);
        } else {
            SetColor(color);
            GUITextProperties p;
            p.invert_ = true;
            for (int y = top; y <= bottom; ++y)
                DrawString(x, waveformY + y, " ", p);
        }
    }
    if (previewProgress_ >= 0 && to > from && height > 0 && width > 0) {
        const double frame = first_ + (last_ - first_) * previewProgress_;
        const int x =
            std::min(width - 1, int((frame - from) * width / (to - from)));
        if (pixels) {
            const int y = waveformY * charHeight;
            window.AddPixelRect(GUIRect(x, y, x + 1, y + height), CD_PLAY);
        } else {
            SetColor(CD_PLAY);
            GUITextProperties props;
            for (int y = 0; y < height; ++y)
                DrawString(x, waveformY + y, "|", props);
        }
    }
    char line[64];
    displayedBpm_ = std::max(1, SyncMaster::GetInstance()->GetTempo());
    const int wholeDigits =
        wholeBeatDigits(beatUnits(size, buffer_.rate, displayedBpm_));
    beatDigit_ = std::min(beatDigit_, wholeDigits + 3);
    const int startX = wide ? 18 : 0;
    const int endX = wide ? 36 : columns / 2;
    for (int mode = 0; mode < 2; ++mode) {
        const int labelY = samplesY + mode * (wide ? 2 : 3);
        const int y = labelY + (wide ? 0 : 1);
        text(0, labelY, mode ? "Select by Beats" : "Select by Samples");
        for (int boundary = 0; boundary < 2; ++boundary) {
            const int frame = boundary ? last_ : first_;
            const int x = boundary ? endX : startX;
            const char *label = boundary ? "End  " : "Start";
            const bool selected = row_ == mode * 2 + boundary;
            if (mode) {
                const long long beat =
                    beatUnits(frame, buffer_.rate, displayedBpm_);
                sprintf(line, "%s %0*lld.%04lld", label, wholeDigits,
                        beat / 10000, beat % 10000);
            } else {
                sprintf(line, "%s %08X", label, frame);
            }
            text(x, y, line, selected);
            if (selected) {
                int offset = 13 - digit_;
                if (mode)
                    offset = 6 + wholeDigits + 4 - beatDigit_ -
                             (beatDigit_ >= 4 ? 1 : 0);
                text(x + offset, y + 1, "^");
            }
        }
    }
    text(0, controlsY + 3, "Process");
    text(10, controlsY + 3, operations[operation_], row_ == 4);
    text(std::min(26, columns - 4), controlsY + 3,
         undoAvailable_ ? "Undo" : "----", row_ == 5);
    text(0, controlsY + 5, "Name");
    text(10, controlsY + 5, name_.c_str(), row_ == 6);
    if (row_ == 6)
        text(10 + nameCursor_, controlsY + 6, "^");
    text(0, controlsY + 7, "Save As", row_ == 7);
    text(12, controlsY + 7, "Overwrite", row_ == 8);
    text(std::min(27, columns - 4), controlsY + 7, "Exit", row_ == 9);
    const int footerColumns = std::min(columns, 40);
    text(0, rows - 3, status_.substr(0, footerColumns).c_str());
    const Shortcuts &keys = Shortcuts::Get();
    text(0, rows - 2,
         ("Tap " + keys.KeyNames(Shortcuts::EditorTap) + ": execute")
             .substr(0, footerColumns)
             .c_str());
    text(0, rows - 1,
         (keys.KeyNames(Shortcuts::EditorPreview) + ": listen   " +
          keys.KeyNames(Shortcuts::EditorZoom) + ": zoom")
             .substr(0, footerColumns)
             .c_str());
    drawMap();
}

void SampleEditorView::AnimationUpdate() {
    if (displayedBpm_ != std::max(1, SyncMaster::GetInstance()->GetTempo())) {
        isDirty_ = true;
        Redraw();
    }
    if (previewProgress_ < 0)
        return;
    const double progress = Player::GetInstance()->GetStreamingProgress();
    if (progress == previewProgress_)
        return;
    if (progress < 0)
        stopPreview();
    else
        previewProgress_ = progress;
    isDirty_ = true;
    Redraw();
}

void SampleEditorView::stopPreview() {
    MixerService *mixer = MixerService::GetInstance();
    mixer->Lock();
    Player::GetInstance()->StopStreaming();
    mixer->Unlock();
    if (previewProgress_ >= 0)
        status_.clear();
    previewProgress_ = -1;
    if (!previewPath_.empty()) {
        std::remove(previewPath_.c_str());
        previewPath_.clear();
    }
}

void SampleEditorView::preview() {
    Player *player = Player::GetInstance();
    if (previewProgress_ >= 0) {
        const bool playing = player->GetStreamingProgress() >= 0;
        stopPreview();
        if (playing)
            return;
    }
    // Keep each preview in its own temporary file.
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
    MixerService *mixer = MixerService::GetInstance();
    const Path path(previewPath_);
    mixer->Lock();
    player->StartStreaming(path);
    mixer->Unlock();
    previewProgress_ = 0;
    status_ = "Playing selection";
}

void SampleEditorView::ConfirmOverwrite(View &v, ModalView &d) {
    SampleEditorView &editor = static_cast<SampleEditorView &>(v);
    if (d.GetReturnCode() == MBL_YES) {
        try {
            editor.save(true);
        } catch (const std::bad_alloc &) {
            editor.status_ = "Not enough memory";
        }
    }
}
void SampleEditorView::ConfirmExit(View &v, ModalView &d) {
    if (d.GetReturnCode() == MBL_YES)
        static_cast<SampleEditorView &>(v).closeEditor();
}

void SampleEditorView::closeEditor() {
    std::vector<short>().swap(buffer_.pcm);
    std::vector<short>().swap(undoBuffer_.pcm);
    ViewType type = VT_INSTRUMENT;
    ViewEvent event(VET_SWITCH_VIEW, &type);
    SetChanged();
    NotifyObservers(&event);
}

void SampleEditorView::save(bool overwrite) {
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
    MixerService *mixer = MixerService::GetInstance();
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

void SampleEditorView::ProcessButtonMask(unsigned short mask, bool pressed) {
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
        if (exitEditor || (click && row_ == 9)) {
            stopPreview();
            if (dirty_)
                DoModal(new MessageBox(*this, "Discard unsaved edits?",
                                       MBBF_YES | MBBF_NO),
                        ConfirmExit);
            else
                closeEditor();
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
            waveform_.clear();
            return;
        }
        if (editing || tapHeld || click) {
            int direction = up ? 1 : down ? -1 : 0;
            if (row_ < 4) {
                const bool musical = row_ >= 2;
                const bool end = row_ % 2;
                int &digit = musical ? beatDigit_ : digit_;
                if (left) {
                    const int limit =
                        musical ? wholeBeatDigits(beatUnits(buffer_.GetSize(-1),
                                                            buffer_.rate,
                                                            displayedBpm_)) +
                                      3
                                : 7;
                    digit = std::min(limit, digit + 1);
                }
                if (right)
                    digit = std::max(0, digit - 1);
                if (direction) {
                    stopPreview();
                    const int frame = end ? last_ : first_;
                    long long value = frame + direction * (1LL << (4 * digit_));
                    if (musical) {
                        const int bpm =
                            std::max(1, SyncMaster::GetInstance()->GetTempo());
                        long long step = 1;
                        for (int i = 0; i < beatDigit_; ++i)
                            step *= 10;
                        const long long target =
                            std::max(0LL, beatUnits(frame, buffer_.rate, bpm) +
                                              direction * step);
                        value = static_cast<long long>(
                            target * (60.0 * buffer_.rate / (10000.0 * bpm)) +
                            0.5);
                        // A beat fraction can be shorter than one source frame.
                        if (value == frame)
                            value += direction;
                    }
                    int minimum = end ? first_ + 1 : 0;
                    int maximum = end ? buffer_.GetSize(-1) : last_ - 1;
                    (end ? last_ : first_) = std::max(
                        static_cast<long long>(minimum),
                        std::min(static_cast<long long>(maximum), value));
                    if (zoom_)
                        waveform_.clear();
                }
            } else if (row_ == 4) {
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
                        waveform_.clear();
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
            } else if (row_ == 5 && click) {
                if (undoAvailable_) {
                    stopPreview();
                    buffer_.pcm.swap(undoBuffer_.pcm);
                    waveform_.clear();
                    std::vector<short>().swap(undoBuffer_.pcm);
                    first_ = undoFirst_;
                    last_ = undoLast_;
                    dirty_ = undoDirty_;
                    zoom_ = undoZoom_;
                    undoAvailable_ = false;
                    status_ = "Last process undone";
                }
            } else if (row_ == 6) {
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
            } else if (click && row_ == 7)
                save(false);
            else if (click && row_ == 8)
                DoModal(new MessageBox(*this, "Overwrite sample file?",
                                       MBBF_YES | MBBF_NO),
                        ConfirmOverwrite);
        } else {
            if (keys.Matches(Shortcuts::EditorDown, mask, pressed) ||
                keys.Matches(Shortcuts::EditorRight, mask, pressed))
                row_ = (row_ + 1) % 10;
            if (keys.Matches(Shortcuts::EditorUp, mask, pressed) ||
                keys.Matches(Shortcuts::EditorLeft, mask, pressed))
                row_ = (row_ + 9) % 10;
        }
    } catch (const std::bad_alloc &) {
        status_ = "Not enough memory";
    }
}
