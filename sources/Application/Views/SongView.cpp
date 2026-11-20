#include "SongView.h"
#if defined(PLATFORM_X64)
#include "Adapters/SDL2/Audio/SDLAudioDriver.h"
#endif
#include "Application/Commands/ApplicationCommandDispatcher.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/ProjectDatas.h"
#include "Application/Player/Player.h"
#include "Application/Utils/char.h"
#include "ModalDialogs/RecordDialog.h"
#include "System/Console/Trace.h"
#include "System/System/System.h"
#include "UIController.h"
#include "VuMeterUtil.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>

/****************
 Constructor
 ****************/

SongView::SongView(GUIWindow &w, ViewData *viewData, const char *song)
    : View(w, viewData) {

#if defined(PLATFORM_X64)
    recordDialog_ = 0;
#endif
    pendingChain_ = NO_MORE_CHAIN;
    lastChain_ = 0;
    songname_ = song;

    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        this->lastPlayedPosition_[i] = 0;
        this->lastQueuedPosition_[i] = 0;
    }
    vuBarHeightsL_[0] = 0;
    vuBarHeightsR_[0] = 0;
    clipboard_.active_ = false;
    clipboard_.data_ = 0;
    invertBatt_ = false;
    deepCloneChain_ = NO_MORE_CHAIN;
    jumpLength_ = 0x10; // B-jump 16 rows like LSDJ
}

/****************
 Destructor
 ****************/

SongView::~SongView() {
#if defined(PLATFORM_X64)
    delete recordDialog_;
    recordDialog_ = 0;
#endif
    if (clipboard_.data_ != 0)
        SYS_FREE((void *)clipboard_.data_);
};

/******************************************************
 updateChain:
        update current chain value by adding offset
        parameter
 ******************************************************/

void SongView::updateChain(int offset) {

    unsigned int chain = viewData_->UpdateSongChain(offset);
    pendingChain_ = chain;
    lastChain_ = chain;
    isDirty_ = true;
    deepCloneChain_ = NO_MORE_CHAIN;
}

/******************************************************
 updateChain:
        set current chain value to value parameter
 ******************************************************/

void SongView::setChain(unsigned char value) {
    viewData_->SetSongChain(value);
    lastChain_ = value;
    isDirty_ = true;
}

/******************************************************
 updateSongOffset:
        Jump from the current position up or down
    by [offset] rows
 ******************************************************/

void SongView::updateSongOffset(int offset) {
    viewData_->UpdateSongOffset(offset);
    isDirty_ = true;
    deepCloneChain_ = NO_MORE_CHAIN;
}

/******************************************************
 updateCursor:
        modify location of cursor in view by
        adding dx & dy parameters
 ******************************************************/

void SongView::updateCursor(int dx, int dy) {
    viewData_->UpdateSongCursor(dx, dy);
    isDirty_ = true;
    deepCloneChain_ = NO_MORE_CHAIN;
}

/******************************************************
 cutPosition:
        copy current position content to clipboard &
        erase current position value
 ******************************************************/

void SongView::cutPosition() {

    // prepare selection data
    clipboard_.x_ = viewData_->songX_;
    clipboard_.y_ = viewData_->songY_;
    clipboard_.offset_ = viewData_->songOffset_;

    saveX_ = viewData_->songX_;
    saveY_ = viewData_->songY_;
    saveOffset_ = viewData_->songOffset_;

    // cut selection
    cutSelection();
};

/******************************************************
 pastePosition:
        set current position to last chain value if
        current step is empty
 ******************************************************/

void SongView::pasteLast() {

    // If we're on an empty spot, we past the last chain
    // otherwise we take the current chain as last

    unsigned char *c = viewData_->GetCurrentSongPointer();
    if (*c == 0xFF) {
        *c = lastChain_;
        viewData_->song_->chain_->SetUsed(*c);
        isDirty_ = true;
    } else {
        lastChain_ = *c;
    }
};

/******************************************************
 clonePosition:
        slim clone current position
 ******************************************************/

bool SongView::clonePosition() {

    unsigned char *pos = viewData_->GetCurrentSongPointer();
    unsigned char current = *pos;
    if (current == 255)
        return false;

    unsigned short next = viewData_->song_->chain_->GetNext();
    if (next == NO_MORE_CHAIN)
        return false;

    unsigned char *src = viewData_->song_->chain_->data_ + 16 * current;
    unsigned char *dst = viewData_->song_->chain_->data_ + 16 * next;

    for (int i = 0; i < 16; i++) {
        *dst++ = *src++;
    };

    src = viewData_->song_->chain_->transpose_ + 16 * current;
    dst = viewData_->song_->chain_->transpose_ + 16 * next;

    for (int i = 0; i < 16; i++) {
        *dst++ = *src++;
    };
    setChain((unsigned char)next);
    return true;
};

/******************************************************
 deepClonePosition:
        deep clone chain and all phrases within
        made by koisignal (https://github.com/koi-ikeno)
 ******************************************************/

void SongView::deepClonePosition() {
    Phrase *ph = viewData_->song_->phrase_;
    Chain *ch = viewData_->song_->chain_;
    unsigned char *pos = viewData_->GetCurrentSongPointer();
    unsigned char curChainNum = *pos;

    if (curChainNum == CHAIN_COUNT) {
        View::SetNotification("no more chains!");
        return;
    }

    unsigned char *steps = ch->data_ + 16 * curChainNum;
    unsigned char copies[PHRASE_COUNT];
    memset(copies, 0xff, sizeof(copies));
    int needed = 0;
    for (int i = 0; i < 16; ++i) {
        unsigned char phrase = steps[i];
        if (phrase != 0xff && copies[phrase] == 0xff) {
            copies[phrase] = 0;
            ++needed;
        }
    }
    int available = 0;
    for (int i = 0; i < PHRASE_COUNT && available < needed; ++i) {
        if (!ph->IsUsed(i))
            ++available;
    }
    if (available < needed) {
        View::SetNotification("no more phrases!");
        return;
    }

    memset(copies, 0xff, sizeof(copies));
    for (int i = 0; i < 16; ++i) {
        unsigned char phrase = steps[i];
        if (phrase == 0xff)
            continue;
        if (copies[phrase] == 0xff) {
            unsigned char next = ph->GetNext();
            copies[phrase] = next;
            for (int k = 0; k < 16; ++k) {
                int src = 16 * phrase + k;
                int dst = 16 * next + k;
                ph->note_[dst] = ph->note_[src];
                ph->velocity_[dst] = ph->velocity_[src];
                ph->instr_[dst] = ph->instr_[src];
                ph->cmd1_[dst] = ph->cmd1_[src];
                ph->param1_[dst] = ph->param1_[src];
                ph->cmd2_[dst] = ph->cmd2_[src];
                ph->param2_[dst] = ph->param2_[src];
            }
        }
        steps[i] = copies[phrase];
    }
    View::SetNotification("deep clone");

    setChain((unsigned char)curChainNum);
}

void SongView::extendSelection() {
    GUIRect rect = getSelectionRect();
    if (rect.Left() > 0 || rect.Right() < SONG_CHANNEL_COUNT - 1) {
        if (viewData_->songX_ < clipboard_.x_) {
            viewData_->songX_ = 0;
            clipboard_.x_ = SONG_CHANNEL_COUNT - 1;
        } else {
            viewData_->songX_ = SONG_CHANNEL_COUNT - 1;
            clipboard_.x_ = 0;
        }
        isDirty_ = true;
    } else {
        if (viewData_->songY_ < clipboard_.y_) {
            viewData_->songY_ = 0;
            clipboard_.y_ = std::min(songRowCount_ - 1,
                                     SONG_ROW_COUNT - 1 - clipboard_.offset_);
        } else {
            clipboard_.y_ = 0;
            viewData_->songY_ = std::min(
                songRowCount_ - 1, SONG_ROW_COUNT - 1 - viewData_->songOffset_);
        }
        isDirty_ = true;
    }
}

/******************************************************
 OnFocus:
        called when current view is becoming active
 ******************************************************/

void SongView::OnFocus() { clipboard_.active_ = false; };

GUIRect SongView::getSelectionRect() {

    GUIRect selRect(clipboard_.x_, clipboard_.y_ + clipboard_.offset_,
                    viewData_->songX_,
                    viewData_->songY_ + viewData_->songOffset_);

    selRect.Normalize();
    return selRect;
}

/******************************************************
 fillClipboard:
        fill clipboard with current selection value
 ******************************************************/

bool SongView::fillClipboardData() {
    GUIRect selRect = getSelectionRect();
    int width = selRect.Width() + 1;
    int height = selRect.Height() + 1;
    unsigned char *copy =
        static_cast<unsigned char *>(SYS_MALLOC(width * height));
    if (!copy) {
        SetNotification("Clipboard allocation failed");
        return false;
    }
    if (clipboard_.data_)
        SYS_FREE(clipboard_.data_);
    clipboard_.data_ = copy;
    clipboard_.width_ = width;
    clipboard_.height_ = height;
    for (int row = 0; row < clipboard_.height_; ++row) {
        const unsigned char *src = viewData_->song_->data_ +
                                   (selRect.Top() + row) * SONG_CHANNEL_COUNT +
                                   selRect.Left();
        std::memcpy(clipboard_.data_ + row * clipboard_.width_, src,
                    clipboard_.width_);
    }
    return true;
};

/******************************************************
 copySelection:
        copy current selection to clipboard
 ******************************************************/

void SongView::copySelection() {

    if (!fillClipboardData())
        return;
    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    viewData_->songX_ = saveX_;
    viewData_->songY_ = saveY_;
    viewData_->songOffset_ = saveOffset_;
    View::SetNotification("copied selection");
}

/******************************************************
 cutSelection:
        cut current selection to clipboard
 ******************************************************/

void SongView::cutSelection() {

    // first copy the data to clipboard

    if (!fillClipboardData())
        return;
    GUIRect selRect = getSelectionRect();

    // now move all rows up for cut

    for (int row = selRect.Top(); row < SONG_ROW_COUNT; ++row) {
        unsigned char *dst =
            viewData_->song_->data_ + row * SONG_CHANNEL_COUNT + selRect.Left();
        if (row + clipboard_.height_ < SONG_ROW_COUNT)
            std::memcpy(dst, dst + clipboard_.height_ * SONG_CHANNEL_COUNT,
                        clipboard_.width_);
        else
            std::memset(dst, 0xff, clipboard_.width_);
    }

    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    viewData_->songX_ = saveX_;
    viewData_->songY_ = saveY_;
    viewData_->songOffset_ = saveOffset_;

    isDirty_ = true;
}

/******************************************************
 pasteSelection:
        paste clipboard content to song
 ******************************************************/

void SongView::pasteClipboard() {

    if (!clipboard_.data_)
        return;

    int top = viewData_->songY_ + viewData_->songOffset_;
    int width =
        std::min(clipboard_.width_, SONG_CHANNEL_COUNT - viewData_->songX_);
    int height = std::min(clipboard_.height_, SONG_ROW_COUNT - top);
    for (int row = SONG_ROW_COUNT - 1; row >= top + height; --row) {
        unsigned char *dst = viewData_->song_->data_ +
                             row * SONG_CHANNEL_COUNT + viewData_->songX_;
        std::memcpy(dst, dst - height * SONG_CHANNEL_COUNT, width);
    }
    for (int row = 0; row < height; ++row) {
        unsigned char *dst =
            viewData_->GetCurrentSongPointer() + row * SONG_CHANNEL_COUNT;
        std::memcpy(dst, clipboard_.data_ + row * clipboard_.width_, width);
    }
    updateCursor(0, height);
}

void SongView::unMuteAll() {

    UIController *controller = UIController::GetInstance();
    controller->UnMuteAll();
};

void SongView::toggleMute() {

    UIController *controller = UIController::GetInstance();

    int from = viewData_->songX_;
    int to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    };
    controller->ToggleMute(from, to);
    viewMode_ = (viewMode_ != VM_MUTEON) ? VM_MUTEON : VM_NORMAL;
};

void SongView::switchSoloMode() {

    UIController *controller = UIController::GetInstance();
    int from = viewData_->songX_;
    int to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    };
    controller->SwitchSoloMode(from, to);
    viewMode_ = (viewMode_ == VM_SOLOON) ? VM_NORMAL : VM_SOLOON;
    isDirty_ = true;
};

void SongView::onStart() {
    // Always play with zero offset in chains when in SongView
    viewData_->chainRow_ = 0;
    Player *player = Player::GetInstance();
    unsigned char from = viewData_->songX_;
    unsigned char to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    }
    int renderMode = viewData_->renderMode_;
    if (renderMode > 0 && !player->IsRunning()) {
        viewData_->isRendering_ = true;
        View::SetNotification("Rendering started!");
    } else if (viewData_->isRendering_ && player->IsRunning()) {
        viewData_->isRendering_ = false;
        View::SetNotification("Rendering done!");
    }
    player->OnSongStartButton(from, to, false, false);
};

void SongView::startCurrentRow() {
    Player *player = Player::GetInstance();
    player->SetSequencerMode(SM_LIVE);
    player->OnSongStartButton(0, SONG_CHANNEL_COUNT - 1, false, false);
}

void SongView::startImmediate() {
    Player *player = Player::GetInstance();

    unsigned char from = viewData_->songX_;
    unsigned char to = from;
    player->OnSongStartButton(from, to, false, true);
}

void SongView::onStop() {
    // Always play with zero offset in chains when in SongView
    viewData_->chainRow_ = 0;
    Player *player = Player::GetInstance();
    unsigned char from = viewData_->songX_;
    unsigned char to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    }

    player->OnSongStartButton(from, to, true, false);
};

void SongView::jumpToNextSection(int direction) {

    int current = viewData_->songY_ + viewData_->songOffset_;
    bool foundGap = false;
    for (int i = 0; i < SONG_ROW_COUNT; i++) {
        unsigned char *start = viewData_->song_->data_ + viewData_->songX_ +
                               SONG_CHANNEL_COUNT * current;
        if (foundGap && (*start != 0xFF)) {
            break;
        } else {
            if (*start == 0xFF) {
                foundGap = true;
            }
        }
        current += direction;
        if (current < 0) {
            current += SONG_ROW_COUNT;
        }
        if (current >= SONG_ROW_COUNT) {
            current -= SONG_ROW_COUNT;
        }
    }
    // If we go backwards, we stil have to go to the beginning of the block

    if (direction < 0) {
        while (current > 0) {
            unsigned char *start = viewData_->song_->data_ + viewData_->songX_ +
                                   SONG_CHANNEL_COUNT * current;
            if (*start == 0xFF) {
                current++;
                break;
            };
            current--;
        };
    }

    // Update viewdata position from current

    if ((current - viewData_->songOffset_ >= songRowCount_) ||
        (current - viewData_->songOffset_ < 0)) {
        viewData_->songOffset_ =
            std::min(std::max(0, current - 4), SONG_ROW_COUNT - songRowCount_);
    }
    viewData_->songY_ = current - viewData_->songOffset_;
    isDirty_ = true;
}

namespace {
enum SongMovement {
    MoveDown = 1,
    MoveUp = 2,
    MoveLeft = 4,
    MoveRight = 8,
    PageDown = 16,
    PageUp = 32,
    NextSection = 64,
    PreviousSection = 128
};

bool songMatches(Shortcuts::Action action, unsigned short mask,
                 bool pressed = true) {
    const Shortcuts &keys = Shortcuts::Get();
#if defined(PLATFORM_X64)
    if ((mask & EPBM_SELECT) &&
        (keys.RemainingButtons(action, mask) & EPBM_SELECT))
        return false;
#endif
    return keys.Matches(action, mask, pressed);
}

enum SongEdit {
    CutCell = 1 << 0,
    PasteCells = 1 << 1,
    DecreaseCoarse = 1 << 2,
    IncreaseCoarse = 1 << 3,
    DecreaseFine = 1 << 4,
    IncreaseFine = 1 << 5,
    HoldEdit = 1 << 6,
    ExtendSelection = 1 << 7,
    CopySelection = 1 << 8,
    CutSelection = 1 << 9,
};

unsigned int songEdits(unsigned short mask, bool selection) {
    static const struct {
        Shortcuts::Action binding;
        bool selection;
        SongEdit edit;
    } bindings[] = {
        {Shortcuts::SongCut, false, CutCell},
        {Shortcuts::SongPaste, false, PasteCells},
        {Shortcuts::SongDecreaseCoarse, false, DecreaseCoarse},
        {Shortcuts::SongIncreaseCoarse, false, IncreaseCoarse},
        {Shortcuts::SongDecreaseFine, false, DecreaseFine},
        {Shortcuts::SongIncreaseFine, false, IncreaseFine},
        {Shortcuts::SongEditHold, false, HoldEdit},
        {Shortcuts::SongSelectionExtend, true, ExtendSelection},
        {Shortcuts::SongSelectionCopy, true, CopySelection},
        {Shortcuts::SongSelectionCut, true, CutSelection},
    };
    unsigned int edits = 0;
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i)
        if (bindings[i].selection == selection &&
            songMatches(bindings[i].binding, mask))
            edits |= bindings[i].edit;
    return edits;
}

enum SongChannelAction {
    NoChannelAction,
    MuteChannels,
    SoloChannels,
    UnmuteAllChannels
};

SongChannelAction songChannelAction(unsigned short mask, bool selection) {
    if (songMatches(selection ? Shortcuts::SongSelectionMute
                              : Shortcuts::SongMute,
                    mask))
        return MuteChannels;
    if (songMatches(selection ? Shortcuts::SongSelectionSolo
                              : Shortcuts::SongSolo,
                    mask))
        return SoloChannels;
    if (songMatches(selection ? Shortcuts::SongSelectionUnmuteAll
                              : Shortcuts::SongUnmuteAll,
                    mask))
        return UnmuteAllChannels;
    return NoChannelAction;
}

enum SongViewSwitch { OpenChain = 1, OpenProject = 2, OpenMixer = 4 };

unsigned int songViewSwitches(unsigned short mask, bool selection) {
    unsigned int switches = 0;
    if (songMatches(selection ? Shortcuts::SongSelectionChain
                              : Shortcuts::SongChain,
                    mask))
        switches |= OpenChain;
    if (songMatches(selection ? Shortcuts::SongSelectionProject
                              : Shortcuts::SongProject,
                    mask))
        switches |= OpenProject;
    if (songMatches(selection ? Shortcuts::SongSelectionMixer
                              : Shortcuts::SongMixer,
                    mask))
        switches |= OpenMixer;
    return switches;
}

enum SongTransport {
    ToggleMode = 1,
    Play = 2,
    Stop = 4,
    StartRow = 8,
    Immediate = 16,
    NudgeDown = 32,
    NudgeUp = 64
};

unsigned int songTransport(unsigned short mask, bool selection) {
    static const struct {
        Shortcuts::Action normal, selected;
        SongTransport action;
    } bindings[] = {
        {Shortcuts::SongToggleModeLeft, Shortcuts::Count, ToggleMode},
        {Shortcuts::SongToggleModeRight, Shortcuts::Count, ToggleMode},
        {Shortcuts::SongPlay, Shortcuts::SongSelectionPlay, Play},
        {Shortcuts::SongStop, Shortcuts::SongSelectionStop, Stop},
        {Shortcuts::SongStartRow, Shortcuts::Count, StartRow},
        {Shortcuts::SongImmediate, Shortcuts::Count, Immediate},
        {Shortcuts::SongNudgeDown, Shortcuts::Count, NudgeDown},
        {Shortcuts::SongNudgeUp, Shortcuts::Count, NudgeUp},
    };
    unsigned int result = 0;
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i)
        if (songMatches(selection ? bindings[i].selected : bindings[i].normal,
                        mask))
            result |= bindings[i].action;
    return result;
}

unsigned int songMovements(unsigned short mask, bool selection) {
    static const struct {
        Shortcuts::Action normal, selected;
        SongMovement movement;
    } bindings[] = {
        {Shortcuts::SongDown, Shortcuts::SongSelectionDown, MoveDown},
        {Shortcuts::SongUp, Shortcuts::SongSelectionUp, MoveUp},
        {Shortcuts::SongLeft, Shortcuts::SongSelectionLeft, MoveLeft},
        {Shortcuts::SongRight, Shortcuts::SongSelectionRight, MoveRight},
        {Shortcuts::SongPageDown, Shortcuts::Count, PageDown},
        {Shortcuts::SongPageUp, Shortcuts::Count, PageUp},
        {Shortcuts::SongNextSection, Shortcuts::Count, NextSection},
        {Shortcuts::SongPreviousSection, Shortcuts::Count, PreviousSection},
    };
    unsigned int result = 0;
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i) {
        Shortcuts::Action action = selection ? bindings[i].selected : bindings[i].normal;
        if (!songMatches(action, mask))
            continue;
        result |= bindings[i].movement;
    }
    return result;
}
} // namespace

void SongView::processMovement(unsigned int movements) {
    if (movements & PageDown)
        updateSongOffset(jumpLength_);
    if (movements & PageUp)
        updateSongOffset(-jumpLength_);
    if (movements & NextSection)
        jumpToNextSection(1);
    if (movements & PreviousSection)
        jumpToNextSection(-1);
    if (movements & MoveDown)
        updateCursor(0, 1);
    if (movements & MoveUp)
        updateCursor(0, -1);
    if (movements & MoveLeft)
        updateCursor(-1, 0);
    if (movements & MoveRight)
        updateCursor(1, 0);
}

/******************************************************
 ProcessButtonMask:
        process button mask even coming from the main
        application window
 ******************************************************/

void SongView::ProcessButtonMask(unsigned short mask, bool pressed) {
    const Shortcuts &keys = Shortcuts::Get();
    if (!pressed) {
        if (viewMode_ == VM_MUTEON &&
            songMatches(Shortcuts::SongMuteRelease, mask, false))
            toggleMute();
        else if (viewMode_ == VM_SOLOON &&
                 songMatches(Shortcuts::SongSoloRelease, mask, false))
            switchSoloMode();
        return;
    }
    Shortcuts::Action sequence = Shortcuts::Count;
    if (viewMode_ == VM_NEW && songMatches(Shortcuts::SongNextUnused, mask))
        sequence = Shortcuts::SongNextUnused;
    else if (viewMode_ == VM_CLONE && songMatches(Shortcuts::SongClone, mask))
        sequence = Shortcuts::SongClone;
    else if (deepCloneChain_ == *viewData_->GetCurrentSongPointer() &&
             songMatches(Shortcuts::SongDeepClone, mask))
        sequence = Shortcuts::SongDeepClone;

#if defined(PLATFORM_X64)
    if (Shortcuts::Get().Matches(Shortcuts::SongRecord, mask, pressed)) {
        if (Player::GetInstance()->IsRunning() || viewData_->renderMode_ != 0) {
            SetNotification("Stop playback and disable render");
        } else {
            clipboard_.active_ = false;
            viewMode_ = VM_NORMAL;
            recordDialog_ = new RecordDialog(*this);
            DoModal(recordDialog_, RecordClosed);
        }
        return;
    }
    if (mask & EPBM_SELECT) {
        bool selection = clipboard_.active_ || viewMode_ == VM_SELECTION ||
                         viewMode_ == VM_CLONE;
        if (sequence == Shortcuts::Count && !songMovements(mask, selection) &&
            songChannelAction(mask, selection) == NoChannelAction &&
            !songTransport(mask, selection) &&
            !songViewSwitches(mask, selection) && !songEdits(mask, selection) &&
            (selection ||
             (!songMatches(Shortcuts::SongInsert, mask) &&
              !songMatches(Shortcuts::SongBeginCloneSelection, mask))))
            return;
    }
#endif
    if (sequence == Shortcuts::SongNextUnused) {
        unsigned short next = viewData_->song_->chain_->GetNext();
        if (next != NO_MORE_CHAIN)
            setChain((unsigned char)next);
    } else if (sequence == Shortcuts::SongClone) {
        deepCloneChain_ = clonePosition() ? *viewData_->GetCurrentSongPointer()
                                          : NO_MORE_CHAIN;
    } else if (sequence == Shortcuts::SongDeepClone) {
        deepClonePosition();
        deepCloneChain_ = NO_MORE_CHAIN;
    } else if (viewMode_ == VM_CLONE) {
        viewMode_ = VM_SELECTION;
    }
    if (sequence != Shortcuts::Count)
        mask = keys.RemainingButtons(sequence, mask);
    if (clipboard_.active_) {
        viewMode_ = VM_SELECTION;
    };
    // Process selection related keys

    if (viewMode_ == VM_SELECTION) {
        if (clipboard_.active_ == false) {
            clipboard_.active_ = true;
            clipboard_.x_ = viewData_->songX_;
            clipboard_.y_ = viewData_->songY_;
            clipboard_.offset_ = viewData_->songOffset_;
            saveX_ = clipboard_.x_;
            saveY_ = clipboard_.y_;
            saveOffset_ = clipboard_.offset_;
        }
        processMovement(songMovements(mask, true));
        processSelectionButtonMask(mask);
    } else {

        // Switch back to normal mode

        viewMode_ = VM_NORMAL;
        processMovement(songMovements(mask, false));
        processNormalButtonMask(mask);
    }
}

/******************************************************
 processNormalButtonMask:
        process button mask in the case there is no
        selection active
 ******************************************************/

void SongView::processNormalButtonMask(unsigned int mask) {

    unsigned int edits = songEdits(mask, false);
    unsigned int switches = songViewSwitches(mask, false);
    unsigned int transport = songTransport(mask, false);
    SongChannelAction channel = songChannelAction(mask, false);
    bool insert = songMatches(Shortcuts::SongInsert, mask);
    bool beginClone = songMatches(Shortcuts::SongBeginCloneSelection, mask);
    if (transport & ToggleMode) {
        Player *player = Player::GetInstance();
        player->SetSequencerMode(
            player->GetSequencerMode() == SM_SONG ? SM_LIVE : SM_SONG);
        isDirty_ = true;
    }
    if (edits & CutCell)
        cutPosition();
    if (beginClone)
        viewMode_ = VM_CLONE;
    if (insert) {
        pasteLast();
        viewMode_ = VM_NEW;
    }
    if (edits & DecreaseCoarse)
        updateChain(-0x10);
    if (edits & IncreaseCoarse)
        updateChain(0x10);
    if (edits & DecreaseFine)
        updateChain(-1);
    if (edits & IncreaseFine)
        updateChain(1);
    if ((edits & PasteCells) &&
        deepCloneChain_ != *viewData_->GetCurrentSongPointer())
        pasteClipboard();
    if (channel == MuteChannels)
        toggleMute();
    else if (channel == SoloChannels)
        switchSoloMode();
    else if (channel == UnmuteAllChannels)
        unMuteAll();

    processViewSwitch(switches);
    processPlayback(transport);

    if (!(edits & HoldEdit) && pendingChain_ < CHAIN_COUNT) {
        viewData_->song_->chain_->SetUsed(pendingChain_);
        pendingChain_ = NO_MORE_CHAIN;
    }
};

/******************************************************
 processSelectionButtonMask:
        process button mask in the case there is a
        selection active
 ******************************************************/

void SongView::processSelectionButtonMask(unsigned int mask) {

    unsigned int edits = songEdits(mask, true);
    unsigned int switches = songViewSwitches(mask, true);
    unsigned int transport = songTransport(mask, true);
    SongChannelAction channel = songChannelAction(mask, true);
    if (channel == MuteChannels)
        toggleMute();
    if (edits & ExtendSelection)
        extendSelection();
    if (edits & CopySelection)
        copySelection();
    if (edits & CutSelection)
        cutSelection();
    if (channel == SoloChannels)
        switchSoloMode();
    else if (channel == UnmuteAllChannels)
        unMuteAll();
    processViewSwitch(switches);
    processPlayback(transport);
}

void SongView::processViewSwitch(unsigned int switches) {
    if (switches & OpenChain) {
        unsigned char *data = viewData_->GetCurrentSongPointer();
        if (*data != 0xFF) {
            ViewType vt = VT_CHAIN;
            ViewEvent ve(VET_SWITCH_VIEW, &vt);
            viewData_->currentChain_ = *data;
            SetChanged();
            NotifyObservers(&ve);
        }
    }

    if (switches & OpenProject) {
        ViewType vt = VT_PROJECT;
        ViewEvent ve(VET_SWITCH_VIEW, &vt);
        SetChanged();
        NotifyObservers(&ve);
    }

    if (switches & OpenMixer) {
        ViewType vt = VT_MIXER;
        ViewEvent ve(VET_SWITCH_VIEW, &vt);
        SetChanged();
        NotifyObservers(&ve);
    }
}

void SongView::processPlayback(unsigned int transport) {
    if (transport & Immediate)
        startImmediate();
    else if (transport & Stop)
        onStop();
    else if (transport & StartRow)
        startCurrentRow();
    else if (transport & Play)
        onStart();

    if (transport & NudgeDown)
        nudgeTempo(-1);
    if (transport & NudgeUp)
        nudgeTempo(1);
}

/******************************************************
 DrawVuBars:
        Draw stereo L/R VU meters to the right of tracks
        Called from both AnimationUpdate and DrawView to ensure
        bars are always visible and never flicker
 ******************************************************/

void SongView::DrawVuBars() {
    Player *player = Player::GetInstance();

    GUIPoint anchor = GetAnchor();
    GUIPoint vuPos = anchor;
    vuPos._x += SONG_CHANNEL_COUNT * 3 + 1;
    vuPos._y += View::songRowCount_ - 1;

    GUITextProperties vuProps;
    vuProps.invert_ = true;

    float peakLevelsL[1];
    float peakLevelsR[1];

    // Read master peak level (post-volume) to match MixerView's Master meter
#if defined(PLATFORM_X64)
    unsigned int inputLevel = 0;
    if (SDLAudioDriver::CapturePeak(inputLevel)) {
        peakLevelsL[0] = float(inputLevel >> 16) / 32767.0f;
        peakLevelsR[0] = float(inputLevel & 65535) / 32767.0f;
    } else
#endif
        if (!player->IsRunning()) {
        peakLevelsL[0] = peakLevelsR[0] = 0.0f;
    } else {
        MixerService *ms = MixerService::GetInstance();
        uint32_t masterLevel = ms->GetMasterPeakLevel();
        peakLevelsL[0] = (float)((masterLevel >> 16) & 0xFFFF) / 32767.0f;
        peakLevelsR[0] = (float)(masterLevel & 0xFFFF) / 32767.0f;
    }

    // Update bar height for master using utility
    int displayHeightsL[1];
    int displayHeightsR[1];
    UpdateVuBarHeights(vuBarHeightsL_, displayHeightsL, peakLevelsL, 1);
    UpdateVuBarHeights(vuBarHeightsR_, displayHeightsR, peakLevelsR, 1);

    // Draw vertical VU bars for L and R channels (two columns side by side)
    for (int row = 0; row < VU_METER_HEIGHT; row++) {
        SetColor(GetVuBarColor(row));

        // Draw left channel at x
        GUIPoint posL = vuPos;
        posL._y -= row;
        DrawVuBarRow(this, posL, row, displayHeightsL[0], vuProps,
                     GetVuBarColor(row));

        // Draw right channel at x+1 (one character to the right)
        GUIPoint posR = vuPos;
        posR._x += 1;
        posR._y -= row;
        DrawVuBarRow(this, posR, row, displayHeightsR[0], vuProps,
                     GetVuBarColor(row));
    }

    SetColor(CD_NORMAL);
}

/******************************************************
 AnimationUpdate:
        Update animation and draw VU meters
 ******************************************************/

void SongView::AnimationUpdate() {
#if defined(PLATFORM_X64)
    if (recordDialog_) {
        recordDialog_->AnimationUpdate();
        DrawVuBars();
        return;
    }
#endif
    DrawVuBars();
}

/******************************************************
 Redraw:
        redraw completely the song view
 ******************************************************/

void SongView::DrawView() {

    Clear();
    View::EnableNotification();

    GUITextProperties props;
    GUIPoint pos = GetTitlePosition();

    // Prepare selection related information

    GUIRect selRect;
    if (clipboard_.active_) {
        selRect = GUIRect(clipboard_.x_, clipboard_.y_ + clipboard_.offset_,
                          viewData_->songX_,
                          viewData_->songY_ + viewData_->songOffset_);

        selRect.Normalize();
    }

    // Draw title

    SetColor(CD_NORMAL);

    Player *player = Player::GetInstance();

    std::ostringstream os;

    os << ((player->GetSequencerMode() == SM_SONG) ? "Song" : "Live");
    os << ": ";

    if (songname_.substr(0, 5) == "lgpt_") {
        os << songname_.substr(5);
    }

    std::string buffer(os.str());

    DrawString(pos._x, pos._y, buffer.c_str(), props);

    // Compute song grid location

    GUIPoint anchor = GetAnchor();

    // Display row numbers

    char row[3];
    pos = anchor;
    pos._x -= 3;
    for (int j = 0; j < View::songRowCount_; j++) {
        char p = j + viewData_->songOffset_;
        ((p / altRowNumber_) % 2) ? SetColor(CD_ROW) : SetColor(CD_ROW2);
        hex2char(p, row);
        DrawString(pos._x, pos._y, row, props);
        pos._y += 1;
    }

    // Draw column titles
    pos = anchor;
    pos._y -= 1;
    SetColor(CD_COL_TITLE);
    char chbuf[4];
    short dx = 3;
    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        snprintf(chbuf, sizeof(chbuf), "%02d", i);
        DrawString(pos._x + i * dx, pos._y, chbuf, props);
    }

    SetColor(CD_NORMAL);

    pos = anchor;
    unsigned char *data =
        viewData_->song_->data_ + (SONG_CHANNEL_COUNT * viewData_->songOffset_);
    dx = 3;
    short dy = 1;
    for (int j = 0; j < View::songRowCount_; j++) {

        pos._x = anchor._x;

        for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {

            bool invert = false;

            // see if we need to invert current step
            // if there's a selection or we are at cursor position

            if (clipboard_.active_) {
                if ((i >= selRect.Left()) && (i <= selRect.Right()) &&
                    (j + viewData_->songOffset_ >= selRect.Top()) &&
                    (j + viewData_->songOffset_ <= selRect.Bottom())) {
                    invert = true;
                }
            } else {
                if (i == viewData_->songX_ && j == viewData_->songY_) {
                    invert = true;
                }
            }

            // draw current step

            unsigned char d = *data++;

            if (d == 0xFE) {
                SetColor(CD_SONGVIEWFE);
            } else if (d == 0x00) {
                SetColor(CD_SONGVIEW00);
            } else {
                SetColor(CD_NORMAL);
            }

            if (invert) {
                SetColor(CD_HILITE2);
                props.invert_ = true;
            }

            if (d == 0xFF) {
                DrawString(pos._x, pos._y, "--", props);
            } else {
                hex2char(d, row);
                DrawString(pos._x, pos._y, row, props);
            }

            // Put back drawing state

            if (invert) {
                SetColor(CD_NORMAL);
                props.invert_ = false;
            }

            // Next step

            pos._x += dx;
        }
        pos._y += dy;
    }
    SetColor(CD_NORMAL);

    drawMap(true);
    drawNotes();

    if (player->IsRunning()) {
        OnPlayerUpdate(PET_UPDATE);
    };

    // Draw VU bars at the end so they appear on top of everything
    // They will always show the empty dashes and smoothly decay when paused
    DrawVuBars();
};

/******************************************************
 OnPlayerUpdate:
        Called when positions in player change. Should
        provide visual feedback of currently played
        position
 ******************************************************/

void SongView::OnPlayerUpdate(PlayerEventType eventType, unsigned int tick) {
#if defined(PLATFORM_X64)
    if (recordDialog_)
        return;
#endif

    Player *player = Player::GetInstance();

    GUIPoint anchor = GetAnchor();
    GUIPoint pos = anchor;
    pos._x -= 1;

    GUITextProperties props;
    SetColor(CD_CURSOR);

    // Loop on all channels

    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {

        // Clear all current positions

        int y = lastPlayedPosition_[i] - viewData_->songOffset_;
        if (y >= 0 && y < View::songRowCount_ &&
            viewData_->playMode_ != PM_AUDITION) {
            pos._y = anchor._y + y;
            DrawString(pos._x, pos._y, " ", props);
        }

        // Clear all last queued positions

        y = lastQueuedPosition_[i] - viewData_->songOffset_;
        if (y >= 0 && y < View::songRowCount_) {
            pos._y = anchor._y + y;
            DrawString(pos._x, pos._y, " ", props);
        }

        // For each playing position, draw current location

        if (player->IsChannelPlaying(i)) {
            if (eventType != PET_STOP) {
                if (viewData_->currentPlayChain_[i] != 0xFF) {
                    int y = viewData_->songPlayPos_[i] - viewData_->songOffset_;
                    if (y >= 0 && y < View::songRowCount_) {
                        pos._y = anchor._y + y;
                        if (!player->IsChannelMuted(i)) {
                            SetColor(CD_PLAY);
                            DrawString(pos._x, pos._y, ">", props);
                        } else {
                            SetColor(CD_MUTE);
                            DrawString(pos._x, pos._y, "-", props);
                        }
                        SetColor(CD_CURSOR);
                        lastPlayedPosition_[i] = viewData_->songPlayPos_[i];
                    }
                }
            }
        }

        // If in live mode, update queued position

        if (player->GetSequencerMode() == SM_LIVE) {
            if (player->GetQueueingMode(i) != QM_NONE) {

                if (eventType != PET_STOP) {
                    int y =
                        player->GetQueuePosition(i) - viewData_->songOffset_;
                    if (y >= 0 && y < View::songRowCount_) {
                        pos._y = anchor._y + y;
                        char *indicator = player->GetLiveIndicator(i);
                        DrawString(pos._x, pos._y, indicator, props);
                        lastQueuedPosition_[i] = player->GetQueuePosition(i);
                    }
                }
            };
        }
        pos._x += 3;
    }

    SetColor(CD_NORMAL);

    // Draw clipping indicator & CPU usage

    if (View::miniLayout_) {
        pos._y = 0;
        pos._x = 25;
    } else {
        pos = anchor;
        pos._x += SONG_CHANNEL_COUNT * 3 + 1;
    }

    if (player->Clipped()) {
        DrawString(pos._x, pos._y, "clip", props);
    } else {
        DrawString(pos._x, pos._y, "----", props);
    }

    char strbuffer[10];
    pos._y += 1;
    sprintf(strbuffer, "%3.3d%%", player->GetPlayedBufferPercentage());
    DrawString(pos._x, pos._y, strbuffer, props);

    System *sys = System::GetInstance();
    int batt = sys->GetBatteryLevel();
    if (batt >= 0) {
        if (batt < 90) {
            SetColor(CD_HILITE2);
            invertBatt_ = !invertBatt_;
        } else {
            invertBatt_ = false;
        };
        props.invert_ = invertBatt_;

        pos._y += 1;
        sprintf(strbuffer, "%3.3d", batt);
        DrawString(pos._x, pos._y, strbuffer, props);
    }

    if (eventType != PET_STOP) {
        SetColor(CD_NORMAL);
        props.invert_ = false;
        int time = int(player->GetPlayTime());
        int mi = time / 60;
        int se = time - mi * 60;
        sprintf(strbuffer, "%2.2d:%2.2d", mi, se);
        pos._y += 1;
        DrawString(pos._x, pos._y, strbuffer, props);
    }
    drawNotes();
};

void SongView::nudgeTempo(int direction) {
    ApplicationCommandDispatcher *dispatcher =
        ApplicationCommandDispatcher::GetInstance();
    switch (direction) {
    case -1:
        dispatcher->OnNudgeDown();
        break;
    case 1:
        dispatcher->OnNudgeUp();
        break;
    }
}

#if defined(PLATFORM_X64)
void SongView::RecordClosed(View &view, ModalView &dialog) {
    SongView &song = static_cast<SongView &>(view);
    song.recordDialog_ = 0;
    song.isDirty_ = true;
}
#endif

void SongView::CancelRecording() {
#if defined(PLATFORM_X64)
    if (recordDialog_)
        recordDialog_->CancelRecording();
#endif
}
