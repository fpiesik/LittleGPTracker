#include "PhraseView.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Model/Scale.h"
#include "Application/Model/Table.h"
#include "Application/Utils/HelpLegend.h"
#include "Application/Utils/char.h"
#include "Application/Views/CommandSelectorCommon.h"
#include "Application/Views/ModalDialogs/CommandSelectorModal.h"
#include "System/Console/Trace.h"
#include "UIController.h"
#include <algorithm>
#include <stdlib.h>
#include <string.h>

short PhraseView::offsets_[3][4] = {-1, 1,   12, -12, -1, 1,
                                    16, -16, -1, 1,   16, -16};

static void CommandSelectorCallback(View &v, ModalView &d) {
    ((PhraseView &)v).onCommandSelectorResult(d);
}

static void CommandSelectorPreviewCallback(View &v, ModalView &d) {
    ((PhraseView &)v).onCommandSelectorPreview(d);
}

PhraseView::PhraseView(GUIWindow &w, ViewData *viewData)
    : View(w, viewData), cmdEdit_("edit", FCC_EDIT, 0) {
    phrase_ = viewData_->song_->phrase_;
    lastPlayingPos_ = 0;
    GUIPoint pos(0, 10);
    cmdEditField_ =
        new UIBigHexVarField(pos, cmdEdit_, 4, "%4.4X", 0, 0xFFFF, 16, true);
    row_ = 0;
    viewData->phraseCurPos_ = 0;
    col_ = 0;
    lastNote_ = 60;
    lastVelocity_ = 100;
    lastInstr_ = 0;
    lastCmd_ = I_CMD_NONE;
    lastParam_ = 0;
    commandSelectorModalActive_ = false;

    clipboard_.active_ = false;
    clipboard_.width_ = 0;
    clipboard_.height_ = 0;

    for (int i = 0; i < 16; i++) {
        clipboard_.note_[i] = 0xFF;
        clipboard_.instr_[i] = 0;
    };
    View::EnableNotification();
}

PhraseView::~PhraseView() { delete cmdEditField_; };

void PhraseView::updateCursor(int dx, int dy) {

    col_ += dx;
    row_ += dy;
    if (col_ > 6)
        col_ = 6;
    if (col_ < 0)
        col_ = 0;
    if (row_ > 15) {
        // Try to see if the current chain has a phrase after this one

        if ((viewMode_ != VM_SELECTION) && (viewData_->chainRow_ < 15)) {
            viewData_->chainRow_++;
            unsigned char *p = viewData_->GetCurrentChainPointer();
            if (*p != 0xFF) {
                viewData_->currentPhrase_ = *p;
                row_ = 0;
            } else { // rollback
                viewData_->chainRow_--;
                row_ = 15;
            }
        } else {
            row_ = 15;
        }
    }
    if (row_ < 0) {

        // Try to see if the current chain has a phrase before this one

        if ((viewMode_ != VM_SELECTION) && (viewData_->chainRow_ > 0)) {
            viewData_->chainRow_--;
            unsigned char *p = viewData_->GetCurrentChainPointer();
            if (*p != 0xFF) {
                viewData_->currentPhrase_ = *p;
                row_ = 15;
            } else { // rollback
                viewData_->chainRow_++;
                row_ = 0;
            }
        } else {
            row_ = 0;
        }
    }
    GUIPoint anchor = GetAnchor();
    GUIPoint p(anchor);
    switch (col_) {
    case 4:
        p._x += 16;
        p._y += row_;
        cmdEditField_->SetPosition(p);
        cmdEdit_.SetInt(
            *(phrase_->param1_ + (16 * viewData_->currentPhrase_ + row_)));
        break;
    case 6:
        p._x += 26;
        p._y += row_;
        cmdEditField_->SetPosition(p);
        cmdEdit_.SetInt(
            *(phrase_->param2_ + (16 * viewData_->currentPhrase_ + row_)));
        break;
    };
    viewData_->phraseCurPos_ = row_;
    isDirty_ = true;
}

void PhraseView::stopAudition() {
    Player *player = Player::GetInstance();
    if (viewData_->playMode_ == PM_AUDITION)
        player->Stop();
}

bool PhraseView::isCommandColumn() const { return col_ == 3 || col_ == 5; }

FourCC *PhraseView::getCurrentCommandPointer() {
    return CommandSelectorCommon::getCommandPointerByCol(
        col_, 3, phrase_->cmd1_ + (16 * viewData_->currentPhrase_ + row_), 5,
        phrase_->cmd2_ + (16 * viewData_->currentPhrase_ + row_));
}

void PhraseView::enterCommandSelector() {
    FourCC *cmdPtr = getCurrentCommandPointer();
    if (!cmdPtr) return;
    commandSelectorModalActive_ = true;
    DoModal(new CommandSelectorModal(*this, cmdPtr, CommandSelectorPreviewCallback),
            CommandSelectorCallback);
}

void PhraseView::onCommandSelectorResult(ModalView &d) {
    commandSelectorModalActive_ = false;
    CommandSelectorModal &modal = (CommandSelectorModal &)d;
    if (modal.GetReturnCode() == 1) {
        FourCC *cmd = getCurrentCommandPointer();
        if (cmd) {
            lastCmd_ = *cmd;
        }
    }
    isDirty_ = true;
}

void PhraseView::onCommandSelectorPreview(ModalView &) {
    isDirty_ = true;
    Player *player = Player::GetInstance();
    // Don't audition when in playback, allow when browsing around
    if (!player->IsRunning() || viewData_->playMode_ == PM_AUDITION) {
        player->OnStartButton(PM_AUDITION, viewData_->songX_, false,
                              viewData_->chainRow_);
    }
}

void PhraseView::updateCursorValue(ViewUpdateDirection direction, int xOffset,
                                   int yOffset) {

    unsigned char *c = 0;
    unsigned char limit = 0;
    bool wrap = false;
    FourCC *cc;

    switch (col_ + xOffset) {
    case 1:
        if (phrase_->note_[16 * viewData_->currentPhrase_ + row_ + yOffset] ==
            0xFF)
            return;
        c = phrase_->velocity_ + 16 * viewData_->currentPhrase_ + row_ +
            yOffset;
        limit = 127;
        break;
    case 0:
        c = phrase_->note_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        break;
    case 2:
        c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        limit = MAX_INSTRUMENT_COUNT - 1;
        wrap = true;
        break;
    case 3:
        cc = phrase_->cmd1_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        switch (direction) {
        case VUD_RIGHT:
            *cc = CommandList::GetNext(*cc);
            break;
        case VUD_UP:
            *cc = CommandList::GetNextAlpha(*cc);
            break;
        case VUD_LEFT:
            *cc = CommandList::GetPrev(*cc);
            break;
        case VUD_DOWN:
            *cc = CommandList::GetPrevAlpha(*cc);
            break;
        }
        lastCmd_ = *cc;
        break;
    case 4:
        switch (direction) {
        case VUD_RIGHT:
            cmdEditField_->ProcessArrow(EPBM_RIGHT);
            break;
        case VUD_UP:
            cmdEditField_->ProcessArrow(EPBM_UP);
            break;
        case VUD_LEFT:
            cmdEditField_->ProcessArrow(EPBM_LEFT);
            break;
        case VUD_DOWN:
            cmdEditField_->ProcessArrow(EPBM_DOWN);
            break;
        }
        *(phrase_->param1_ + (16 * viewData_->currentPhrase_ + row_ +
                              yOffset)) = cmdEdit_.GetInt();
        lastParam_ = cmdEdit_.GetInt();
        break;
    case 5:
        cc = phrase_->cmd2_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        switch (direction) {
        case VUD_RIGHT:
            *cc = CommandList::GetNext(*cc);
            break;
        case VUD_UP:
            *cc = CommandList::GetNextAlpha(*cc);
            break;
        case VUD_LEFT:
            *cc = CommandList::GetPrev(*cc);
            break;
        case VUD_DOWN:
            *cc = CommandList::GetPrevAlpha(*cc);
            break;
        }
        lastCmd_ = *cc;
        break;
    case 6:
        switch (direction) {
        case VUD_RIGHT:
            cmdEditField_->ProcessArrow(EPBM_RIGHT);
            break;
        case VUD_UP:
            cmdEditField_->ProcessArrow(EPBM_UP);
            break;
        case VUD_LEFT:
            cmdEditField_->ProcessArrow(EPBM_LEFT);
            break;
        case VUD_DOWN:
            cmdEditField_->ProcessArrow(EPBM_DOWN);
            break;
        }
        *(phrase_->param2_ + (16 * viewData_->currentPhrase_ + row_ +
                              yOffset)) = cmdEdit_.GetInt();
        lastParam_ = cmdEdit_.GetInt();
        break;
    }
    if ((c) && (*c != 0xFF)) {
        int offset = offsets_[col_ + xOffset][direction];
        // if note column apply the set scale
        if (col_ + xOffset == 0) {
            const int scale = viewData_->project_->GetScale();
            const int start =
                std::max(int(NOTE_C0), std::min(int(NOTE_C9), int(*c)));
            int note = start + offset;
            if (note > NOTE_C9)
                note = NOTE_C0 + (offset == 12 ? start % 12 : 0);
            if (note < NOTE_C0)
                note = offset == -12 ? start + (NOTE_C9 - start) / 12 * 12
                                     : NOTE_C9;
            while (!scaleSteps[scale][note % 12]) {
                note += offset > 0 ? 1 : -1;
                if (note > NOTE_C9)
                    note = NOTE_C0;
                if (note < NOTE_C0)
                    note = NOTE_C9;
            }
            *c = note;
        } else {
            updateData(c, offset, limit, wrap);
        }
        int index = 16 * viewData_->currentPhrase_ + row_ + yOffset;
        if (col_ + xOffset <= 2 && phrase_->note_[index] != 0xFF)
            lastVelocity_ = phrase_->velocity_[index];
        switch (col_ + xOffset) {
        case 0:
            lastNote_ = *c;
            break;
        case 2:
            lastInstr_ = *c;
            break;
        }
    }
    Player *player = Player::GetInstance();
    // Phrase FX params are currently not applied to preview
    if (col_ == 0 || col_ == 1 || col_ == 2 || col_ == 3 || col_ == 4 ||
        col_ == 5 || col_ == 6) {
        if (player->IsRunning()) {
            if ((viewData_->playMode_ == PM_AUDITION)) {
                player->Stop();
                player->OnStartButton(PM_AUDITION, viewData_->songX_, false,
                                      viewData_->chainRow_);
            }
        } else {
            player->OnStartButton(PM_AUDITION, viewData_->songX_, false,
                                  viewData_->chainRow_);
        }
    }
    isDirty_ = true;
}

// If we're on an empty spot, we past the last element
// otherwise we take the current phrase as last

void PhraseView::pasteLast() {

    uchar *c = 0;
    uint *i = 0;

    switch (col_) {
    case 0:
        c = phrase_->note_ + (16 * viewData_->currentPhrase_ + row_);
        if ((*c == 0xFF)) {
            *c = std::max(int(NOTE_C0), std::min(int(NOTE_C9), lastNote_));
            phrase_->velocity_[16 * viewData_->currentPhrase_ + row_] =
                lastVelocity_;
            c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
            *c = lastInstr_;
            isDirty_ = true;
        } else {
            lastNote_ = *c;
            lastVelocity_ =
                phrase_->velocity_[16 * viewData_->currentPhrase_ + row_];
            c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
            lastInstr_ = *c;
        }
        break;
    case 2:
        c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
        if ((*c == 0xFF)) {
            *c = lastInstr_;
            isDirty_ = true;
        } else {
            lastInstr_ = *c;
        }
        break;
    case 3:
        i = phrase_->cmd1_ + (16 * viewData_->currentPhrase_ + row_);
        if (*i == I_CMD_NONE) {
            *i = lastCmd_;
            isDirty_ = true;
        } else {
            lastCmd_ = *i;
        }
        break;

    case 4:
        /*			s=phrase_->param1_+(16*viewData_->currentPhrase_+row_) ;
                    if (*s==0) {
                        *s=lastParam_ ;
                        cmdEdit_.SetInt(lastParam_) ;
                        isDirty_=true ;
                    }
        �*/
        break;

    case 5:
        i = phrase_->cmd2_ + (16 * viewData_->currentPhrase_ + row_);
        if (*i == I_CMD_NONE) {
            *i = lastCmd_;
            isDirty_ = true;
        } else {
            lastCmd_ = *i;
        }
        break;

    case 6:
        /*			s=phrase_->param2_+(16*viewData_->currentPhrase_+row_) ;
                    if (*s==0) {
                        *s=lastParam_ ;
                        isDirty_=true ;
                        cmdEdit_.SetInt(lastParam_) ;
                    }
        */
        break;
    }
}

void PhraseView::cutPosition() {

    clipboard_.active_ = true;
    clipboard_.row_ = row_;
    clipboard_.col_ = col_;
    saveRow_ = row_;
    saveCol_ = col_;

    if (col_ == 0)
        col_ = 2;
    else if (col_ == 3 || col_ == 5)
        col_ += 1; // This way, A+B on note cuts
                   // the instruments too and parameters get cut with commands
    cutSelection();
};

void PhraseView::warpInChain(int offset) {

    int currentRow = viewData_->chainRow_;
    viewData_->chainRow_ += offset;
    if ((viewData_->chainRow_ < 16) && (viewData_->chainRow_ >= 0)) {
        unsigned char *p = viewData_->GetCurrentChainPointer();
        if (*p != 0xFF) {
            viewData_->currentPhrase_ = *p;
            switch (col_) {
            case 4:
                cmdEdit_.SetInt(*(phrase_->param1_ +
                                  (16 * viewData_->currentPhrase_ + row_)));
                break;
            case 6:
                cmdEdit_.SetInt(*(phrase_->param2_ +
                                  (16 * viewData_->currentPhrase_ + row_)));
                break;
            };
        } else { // rollback
            viewData_->chainRow_ = currentRow;
        }
    } else { // rollback
        viewData_->chainRow_ = currentRow;
    }
    isDirty_ = true;
}

void PhraseView::warpToNeighbour(int offset) {
    // save current data
    int saveX = viewData_->songX_;
    int saveOffset = viewData_->songOffset_;
    int newPos = saveX + offset;

    while ((newPos > -1) && (newPos < SONG_CHANNEL_COUNT)) {
        // Go to neighbout song channel
        viewData_->songX_ = newPos;
        unsigned char *c = viewData_->GetCurrentSongPointer();
        // is there a chain ?
        unsigned char oldChain = viewData_->currentChain_;
        if (*c != 0xFF) {
            // go to chain
            viewData_->currentChain_ = *c;
            // get phrase at location
            unsigned char *p = viewData_->GetCurrentChainPointer();
            // is there a phrase ?
            if (*p != 0xFF) {
                viewData_->currentPhrase_ = *p;
                updateCursor(0, 0);
                isDirty_ = true;
                return;
            } else {
                viewData_->currentPhrase_ = 0xFE;
                viewData_->currentChain_ = *c;
                updateCursor(0, 0);
                isDirty_ = true;
                return;
            }
        } else {
            // no chain, to neighbour song channel
            newPos += offset;
        }
    }
    // restore song
    viewData_->songX_ = saveX;
    viewData_->songOffset_ = saveOffset;
}

/******************************************************
 getSelectionRect:
        gets the normalized rectangle of the current
        selection. Valid only while selection is drawn
 ******************************************************/

GUIRect PhraseView::getSelectionRect() {
    GUIRect r(clipboard_.col_, clipboard_.row_, col_, row_);
    r.Normalize();
    return r;
};

/******************************************************
 fillClipboardData:

        copies the necessary information from the
        current selection to the clipboard for future
        paste. We're copying data all across the row
        because we"re too lazy to try to figure a better
        procedure
 ******************************************************/

void PhraseView::fillClipboardData() {

    // Get Current normalized selection rect

    GUIRect selRect = getSelectionRect();

    // Get size & store in clipboard

    clipboard_.width_ = selRect.Width() + 1;
    clipboard_.height_ = selRect.Height() + 1;
    clipboard_.row_ = selRect.Top();
    clipboard_.col_ = selRect.Left();

    // Copy the data

    uchar *src1 =
        viewData_->song_->phrase_->note_ + 16 * viewData_->currentPhrase_;
    uchar *dst1 = clipboard_.note_;
    uchar *src2 =
        viewData_->song_->phrase_->instr_ + 16 * viewData_->currentPhrase_;
    uchar *dst2 = clipboard_.instr_;
    uint *src3 =
        viewData_->song_->phrase_->cmd1_ + 16 * viewData_->currentPhrase_;
    uint *dst3 = clipboard_.cmd1_;
    ushort *src4 =
        viewData_->song_->phrase_->param1_ + 16 * viewData_->currentPhrase_;
    ushort *dst4 = clipboard_.param1_;
    uint *src5 =
        viewData_->song_->phrase_->cmd2_ + 16 * viewData_->currentPhrase_;
    uint *dst5 = clipboard_.cmd2_;
    ushort *src6 =
        viewData_->song_->phrase_->param2_ + 16 * viewData_->currentPhrase_;
    ushort *dst6 = clipboard_.param2_;

    for (int i = 0; i < clipboard_.height_; i++) {
        dst1[i] = src1[clipboard_.row_ + i];
        clipboard_.velocity_[i] =
            phrase_->velocity_[16 * viewData_->currentPhrase_ +
                               clipboard_.row_ + i];
        dst2[i] = src2[clipboard_.row_ + i];
        dst3[i] = src3[clipboard_.row_ + i];
        dst4[i] = src4[clipboard_.row_ + i];
        dst5[i] = src5[clipboard_.row_ + i];
        dst6[i] = src6[clipboard_.row_ + i];
    };
    updateCursor(0, 0);
};

void PhraseView::updateSelectionValue(ViewUpdateDirection direction) {

    const int endpointRow = row_;
    const int endpointCol = col_;

    GUIRect r = getSelectionRect();
    col_ = r.Left();
    row_ = r.Top();

    for (int i = 0; i <= r.Width(); i++) {
        for (int j = 0; j <= r.Height(); j++) {
            if (col_ + i < 3) {
                updateCursorValue(direction, i, j);
            }
        }
    }
    row_ = endpointRow;
    col_ = endpointCol;
}

void PhraseView::extendSelection() {
    GUIRect rect = getSelectionRect();
    if (rect.Left() > 0 || rect.Right() < 6) {
        if (col_ < clipboard_.col_) {
            col_ = 0;
            clipboard_.col_ = 6;
        } else {
            col_ = 6;
            clipboard_.col_ = 0;
        }
        isDirty_ = true;
    } else {
        if (row_ < clipboard_.row_) {
            row_ = 0;
            clipboard_.row_ = 15;
        } else {
            clipboard_.row_ = 0;
            row_ = 15;
        }
        isDirty_ = true;
    }
}

/******************************************************
 interpolateSelection:
        expands the lowest value of selection to the highest
 ******************************************************/
void PhraseView::interpolateSelection() {
    if (!clipboard_.active_) {
        return;
    }

    GUIRect rect = getSelectionRect();
    // Only interpolate if we're in note (0) or param (3, 5) columns
    int col = rect.Left();
    if (col != rect.Right() || (col != 0 && col != 1 && col != 4 && col != 6)) {
        return;
    }

    int startRow = rect.Top();
    int endRow = rect.Bottom();
    // Need at least 2 rows to interpolate
    if (endRow - startRow < 1) {
        return;
    }

    // Select the appropriate data array based on column
    if (col == 0 || col == 1) {
        uchar *noteData = (col == 0 ? phrase_->note_ : phrase_->velocity_) +
                          (16 * viewData_->currentPhrase_);

        uchar startNote = noteData[startRow];
        uchar endNote = noteData[endRow];

        if (startNote == 0xFF || endNote == 0xFF) {
            View::SetNotification("No note info");
            return;
        }

        int numSteps = endRow - startRow;
        int noteDiff = (int)endNote - (int)startNote;

        for (int step = 0; step <= numSteps; step++) {
            int row = startRow + step;
            int value = startNote + (noteDiff * step) / (numSteps);
            noteData[row] = (uchar)value;
        }
    } else {
        // Parameter columns (3 or 5)
        ushort *paramData =
            (col == 4) ? phrase_->param1_ + (16 * viewData_->currentPhrase_)
                       : phrase_->param2_ + (16 * viewData_->currentPhrase_);

        ushort startParam = paramData[startRow];
        ushort endParam = paramData[endRow];

        int numSteps = endRow - startRow;
        int paramDiff = (int)endParam - (int)startParam;

        for (int step = 0; step <= numSteps; step++) {
            int row = startRow + step;
            int value = startParam + (paramDiff * step) / (numSteps);
            paramData[row] = (ushort)value;
        }
    }
    isDirty_ = true;
}

/******************************************************
 copySelection:
        copies data in the current selection to the
        clipboard & end selection process
 ******************************************************/

void PhraseView::copySelection() {

    // Keep up with row,col of selection coz
    // fillClipboardData will trash it

    fillClipboardData();

    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    row_ = saveRow_;
    col_ = saveCol_;

    updateCursor(0, 0);
    View::SetNotification("Copied selection");
};

/******************************************************
 cut:  copies data in the current selection to the
       clipboard, clear selection content & end selection
       process
 ******************************************************/

void PhraseView::cutSelection() {

    // Keep up with row,col of selection coz
    // fillClipboardData will trash it

    fillClipboardData();

    // Loop over selection col, row & clear data inside it

    uchar *dst1 =
        viewData_->song_->phrase_->note_ + 16 * viewData_->currentPhrase_;
    uchar *dst2 =
        viewData_->song_->phrase_->instr_ + 16 * viewData_->currentPhrase_;
    uint *dst3 =
        viewData_->song_->phrase_->cmd1_ + 16 * viewData_->currentPhrase_;
    ushort *dst4 =
        viewData_->song_->phrase_->param1_ + 16 * viewData_->currentPhrase_;
    uint *dst5 =
        viewData_->song_->phrase_->cmd2_ + 16 * viewData_->currentPhrase_;
    ushort *dst6 =
        viewData_->song_->phrase_->param2_ + 16 * viewData_->currentPhrase_;

    for (int i = 0; i < clipboard_.width_; i++) {
        for (int j = 0; j < clipboard_.height_; j++) {
            switch (i + clipboard_.col_) {
            case 1:
                phrase_->velocity_[16 * viewData_->currentPhrase_ +
                                   clipboard_.row_ + j] = 127;
                break;
            case 0:
                dst1[j + clipboard_.row_] = 0xFF;
                break;
            case 2:
                dst2[j + clipboard_.row_] = 0xFF;
                break;
            case 3:
                dst3[j + clipboard_.row_] = I_CMD_NONE;
                break;
            case 4:
                dst4[j + clipboard_.row_] = 0x0000;
                break;
            case 5:
                dst5[j + clipboard_.row_] = I_CMD_NONE;
                break;
            case 6:
                dst6[j + clipboard_.row_] = 0x0000;
                break;
            }
        }
    }

    // Clear selection, end selection process & reposition cursor

    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    row_ = saveRow_;
    col_ = saveCol_;
    updateCursor(0, 0);
    isDirty_ = true;
};

/******************************************************
 pasteClipboard:
        copies data in the clipboard to the current step
 ******************************************************/

void PhraseView::pasteClipboard() {

    // Get number of row to paste

    int height = clipboard_.height_;
    /*    if (row_+height>16) {
            height=16-row_ ;
        }
      */
    uchar *dst1 =
        viewData_->song_->phrase_->note_ + 16 * viewData_->currentPhrase_;
    uchar *src1 = clipboard_.note_;
    uchar *dst2 =
        viewData_->song_->phrase_->instr_ + 16 * viewData_->currentPhrase_;
    uchar *src2 = clipboard_.instr_;
    uint *dst3 =
        viewData_->song_->phrase_->cmd1_ + 16 * viewData_->currentPhrase_;
    uint *src3 = clipboard_.cmd1_;
    ushort *dst4 =
        viewData_->song_->phrase_->param1_ + 16 * viewData_->currentPhrase_;
    ushort *src4 = clipboard_.param1_;
    uint *dst5 =
        viewData_->song_->phrase_->cmd2_ + 16 * viewData_->currentPhrase_;
    uint *src5 = clipboard_.cmd2_;
    ushort *dst6 =
        viewData_->song_->phrase_->param2_ + 16 * viewData_->currentPhrase_;
    ushort *src6 = clipboard_.param2_;

    uint *noCmd = (uint *)-1;
    ushort *noPrm = (ushort *)-1;
    uint *srcCmd[6] = {noCmd, noCmd, noCmd, src3, noCmd, src5};
    ushort *srcPrm[7] = {noPrm, noPrm, noPrm, noPrm, src4, noPrm, src6};
    uint *dstCmd[6] = {noCmd, noCmd, noCmd, dst3, noCmd, dst5};
    ushort *dstPrm[7] = {noPrm, noPrm, noPrm, noPrm, dst4, noPrm, dst6};

    bool wasUpdated = false;

    for (int i = 0; i < clipboard_.width_; i++) {
        for (int j = 0; j < height; j++) {
            switch (i + clipboard_.col_) {
            case 1:
                phrase_->velocity_[16 * viewData_->currentPhrase_ +
                                   (row_ + j) % 16] = clipboard_.velocity_[j];
                wasUpdated = true;
                break;
            case 0:
                dst1[(j + row_) % 16] = src1[j];
                wasUpdated = true;
                break;
            case 2:
                dst2[(j + row_) % 16] = src2[j];
                wasUpdated = true;
                break;
            case 3:
            case 5:
                if ((col_ + i) == 3 ||
                    (col_ + i) == 5) { // Don't allow commands in notes, etc
                    dstCmd[col_ + i][(row_ + j) % 16] =
                        srcCmd[clipboard_.col_ + i][j];
                    wasUpdated = true;
                }
                break;
            case 4:
            case 6:
                if ((col_ + i) == 4 || (col_ + i) == 6) {
                    dstPrm[col_ + i][(row_ + j) % 16] =
                        srcPrm[clipboard_.col_ + i][j];
                    wasUpdated = true;
                }
                break;
            }
        }
    }
    if (wasUpdated) {
        updateCursor(0x00, ((row_ + height) % 16 - row_));
        isDirty_ = true;
    }
};

void PhraseView::unMuteAll() {

    UIController *controller = UIController::GetInstance();
    controller->UnMuteAll();
};

void PhraseView::toggleMute() {

    UIController *controller = UIController::GetInstance();
    controller->ToggleMute(viewData_->songX_, viewData_->songX_);
    viewMode_ = (viewMode_ != VM_MUTEON) ? VM_MUTEON : VM_NORMAL;
};

void PhraseView::switchSoloMode() {

    UIController *controller = UIController::GetInstance();
    controller->SwitchSoloMode(viewData_->songX_, viewData_->songX_);
    viewMode_ = (viewMode_ == VM_SOLOON) ? VM_NORMAL : VM_SOLOON;
    isDirty_ = true;
};

void PhraseView::OnFocus() {
    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    updateCursor(0, 0);
};

void PhraseView::ProcessButtonMask(unsigned short mask, bool pressed) {

    if (!pressed) {
        const Shortcuts &keys = Shortcuts::Get();
        if (viewMode_ == VM_MUTEON &&
            keys.Matches(Shortcuts::PhraseMuteRelease, mask, false))
            toggleMute();
        else if (viewMode_ == VM_SOLOON &&
                 keys.Matches(Shortcuts::PhraseSoloRelease, mask, false))
            switchSoloMode();
        return;
    };

    if (viewMode_ == VM_NEW || viewMode_ == VM_CLONE) {
        const bool clone = viewMode_ == VM_CLONE;
        const bool instrument = col_ < 3;
        int step = 16 * viewData_->currentPhrase_ + row_;
        ushort *tableParam = 0;
        if (col_ == 4 && phrase_->cmd1_[step] == I_CMD_TABL)
            tableParam = &phrase_->param1_[step];
        else if (col_ == 6 && phrase_->cmd2_[step] == I_CMD_TABL)
            tableParam = &phrase_->param2_[step];

        Shortcuts::Action action = Shortcuts::Count;
        if (instrument)
            action = clone ? Shortcuts::PhraseCloneInstrument
                           : Shortcuts::PhraseNextInstrument;
        else if (clone || tableParam)
            action = clone ? Shortcuts::PhraseCloneTable
                           : Shortcuts::PhraseNextTable;
        const Shortcuts &keys = Shortcuts::Get();
        if (keys.Matches(action, mask, pressed)) {
            if (instrument) {
                InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
                unsigned char &slot = phrase_->instr_[step];
                if (!clone || slot < MAX_INSTRUMENT_COUNT) {
                    unsigned short next =
                        clone ? bank->Clone(slot) : bank->GetNext();
                    if (next != NO_MORE_INSTRUMENT) {
                        slot = next;
                        lastInstr_ = next;
                        isDirty_ = true;
                    } else {
                        View::SetNotification("No more instruments");
                    }
                }
            } else if (tableParam) {
                TableHolder *tables = TableHolder::GetInstance();
                int next;
                if (clone) {
                    int source = *tableParam & 0x7f;
                    tables->SetUsed(source);
                    next = tables->Clone(source);
                } else {
                    next = tables->GetNext();
                }
                if (next != NO_MORE_TABLE) {
                    *tableParam = next;
                    cmdEdit_.SetInt(next);
                    isDirty_ = true;
                } else {
                    View::SetNotification("No more tables");
                }
            }
            mask = keys.RemainingButtons(action, mask);
        } else if (clone) {
            viewMode_ = VM_SELECTION;
        }
    }

    if (clipboard_.active_)
        viewMode_ = VM_SELECTION;
    if (viewMode_ == VM_SELECTION) {
        if (!clipboard_.active_) {
            clipboard_.active_ = true;
            clipboard_.col_ = col_;
            clipboard_.row_ = row_;
            saveCol_ = col_;
            saveRow_ = row_;
        }
        processSelectionButtonMask(mask);
    } else {
        viewMode_ = VM_NORMAL;
        processNormalButtonMask(mask);
    };
}

void PhraseView::processNavigation(unsigned short mask, bool selection) {
    const Shortcuts &keys = Shortcuts::Get();
    if (!selection) {
        if (keys.Matches(Shortcuts::PhrasePreviousChannel, mask, true))
            warpToNeighbour(-1);
        if (keys.Matches(Shortcuts::PhraseNextChannel, mask, true))
            warpToNeighbour(1);
        if (keys.Matches(Shortcuts::PhrasePreviousInChain, mask, true))
            warpInChain(-1);
        if (keys.Matches(Shortcuts::PhraseNextInChain, mask, true))
            warpInChain(1);
    }
    if (keys.Matches(selection ? Shortcuts::PhraseSelectionDown
                               : Shortcuts::PhraseDown,
                     mask, true))
        updateCursor(0, 1);
    if (keys.Matches(selection ? Shortcuts::PhraseSelectionUp
                               : Shortcuts::PhraseUp,
                     mask, true))
        updateCursor(0, -1);
    if (keys.Matches(selection ? Shortcuts::PhraseSelectionLeft
                               : Shortcuts::PhraseLeft,
                     mask, true))
        updateCursor(-1, 0);
    if (keys.Matches(selection ? Shortcuts::PhraseSelectionRight
                               : Shortcuts::PhraseRight,
                     mask, true))
        updateCursor(1, 0);
}

void PhraseView::switchView(ViewType destination) {
    ViewEvent event(VET_SWITCH_VIEW, &destination);
    SetChanged();
    NotifyObservers(&event);
}

void PhraseView::processViewSwitches(unsigned short mask, bool selection) {
    const Shortcuts &keys = Shortcuts::Get();
    if (keys.Matches(selection ? Shortcuts::PhraseSelectionChain
                               : Shortcuts::PhraseChain,
                     mask, true))
        switchView(VT_CHAIN);

    if (keys.Matches(selection ? Shortcuts::PhraseSelectionInstrument
                               : Shortcuts::PhraseInstrument,
                     mask, true)) {
        int nearest = findClosestInstrumentFor(row_);
        viewData_->currentInstrument_ = nearest >= 0 ? nearest : lastInstr_;
        if (selection || viewData_->currentInstrument_ != 0xff)
            switchView(VT_INSTRUMENT);
    }
    if (selection)
        return;
    if (keys.Matches(Shortcuts::PhraseTable, mask, true)) {
        int step = 16 * viewData_->currentPhrase_ + row_;
        if (phrase_->cmd1_[step] == I_CMD_TABL)
            viewData_->currentTable_ = phrase_->param1_[step] & 0x7f;
        else if (phrase_->cmd2_[step] == I_CMD_TABL)
            viewData_->currentTable_ = phrase_->param2_[step] & 0x7f;
        switchView(VT_TABLE);
    }
    if (keys.Matches(Shortcuts::PhraseGroove, mask, true))
        switchView(VT_GROOVE);
}

void PhraseView::processPlayback(unsigned short mask, bool selection) {
    const Shortcuts &keys = Shortcuts::Get();
    if (!selection && keys.Matches(Shortcuts::PhraseMute, mask, true))
        toggleMute();
    if (keys.Matches(selection ? Shortcuts::PhraseSelectionSolo
                               : Shortcuts::PhraseSolo,
                     mask, true))
        switchSoloMode();

    bool playSong = keys.Matches(selection ? Shortcuts::PhraseSelectionPlaySong
                                           : Shortcuts::PhrasePlaySong,
                                 mask, true);
    if (playSong || keys.Matches(selection ? Shortcuts::PhraseSelectionPlay
                                           : Shortcuts::PhrasePlay,
                                 mask, true))
        Player::GetInstance()->OnStartButton(PM_PHRASE, viewData_->songX_,
                                             playSong, viewData_->chainRow_);

    if (keys.Matches(selection ? Shortcuts::PhraseSelectionUnmuteAll
                               : Shortcuts::PhraseUnmuteAll,
                     mask, true))
        unMuteAll();
}

void PhraseView::processValueEdits(unsigned short mask, bool selection) {
    const Shortcuts &keys = Shortcuts::Get();
    bool command = !selection && isCommandColumn();
    if (command &&
        (keys.Matches(Shortcuts::PhraseCommandPickerDown, mask, true) ||
         keys.Matches(Shortcuts::PhraseCommandPickerUp, mask, true)))
        enterCommandSelector();

    static const struct {
        Shortcuts::Action single, selected;
        ViewUpdateDirection direction;
    } edits[] = {
        {Shortcuts::PhraseEditDown, Shortcuts::PhraseSelectionEditDown,
         VUD_DOWN},
        {Shortcuts::PhraseEditUp, Shortcuts::PhraseSelectionEditUp, VUD_UP},
        {Shortcuts::PhraseEditLeft, Shortcuts::PhraseSelectionEditLeft,
         VUD_LEFT},
        {Shortcuts::PhraseEditRight, Shortcuts::PhraseSelectionEditRight,
         VUD_RIGHT},
    };
    for (unsigned i = 0; i < sizeof(edits) / sizeof(edits[0]); ++i) {
        ViewUpdateDirection direction = edits[i].direction;
        if (command && (direction == VUD_DOWN || direction == VUD_UP))
            continue;
        if (keys.Matches(selection ? edits[i].selected : edits[i].single, mask,
                         true)) {
            if (selection)
                updateSelectionValue(direction);
            else
                updateCursorValue(direction);
        }
    }
}

void PhraseView::processNormalButtonMask(unsigned short mask) {
    const Shortcuts &keys = Shortcuts::Get();
    if (!keys.Matches(Shortcuts::PhraseAuditionHold, mask, true)) {
        stopAudition();
    }
    processNavigation(mask, false);
    processViewSwitches(mask, false);

    if (keys.Matches(Shortcuts::PhraseCut, mask, true))
        cutPosition();
    if (keys.Matches(Shortcuts::PhraseBeginCloneSelection, mask, true))
        viewMode_ = VM_CLONE;
    if ((col_ == 0 || col_ == 2) &&
        keys.Matches(Shortcuts::PhraseAudition, mask, true)) {
        Player *player = Player::GetInstance();
        if (!player->IsRunning())
            player->OnStartButton(PM_AUDITION, viewData_->songX_, false,
                                  viewData_->chainRow_);
    }
    processValueEdits(mask, false);
    if (keys.Matches(Shortcuts::PhrasePaste, mask, true))
        pasteClipboard();
    if (keys.Matches(Shortcuts::PhraseInsert, mask, true)) {
        pasteLast();
        if ((col_ == 2) || (col_ == 4) || (col_ == 6))
            viewMode_ = VM_NEW;
    }
    processPlayback(mask, false);
};

/*
 * For currently selected row, find nearest instrument from the top
 */
int PhraseView::findClosestInstrumentFor(int row) {
    unsigned char *instr = phrase_->instr_ + (16 * viewData_->currentPhrase_);
    if (instr[row] != 0xFF) return instr[row];
    for (int d = 1; d < 16; ++d) {
        int up = row - d;
        int down = row + d;
        if (up >= 0 && instr[up] != 0xFF) return instr[up];
        if (down < 16 && instr[down] != 0xFF) return instr[down];
    }
    return -1; // none found in phrase
}

void PhraseView::processSelectionButtonMask(unsigned short mask) {
    processNavigation(mask, true);
    processViewSwitches(mask, true);

    const Shortcuts &keys = Shortcuts::Get();
    if (keys.Matches(Shortcuts::PhraseSelectionExtend, mask, true))
        extendSelection();
    else if (keys.Matches(Shortcuts::PhraseSelectionInterpolate, mask, true))
        interpolateSelection();
    else if (keys.Matches(Shortcuts::PhraseSelectionCopy, mask, true))
        copySelection();

    processValueEdits(mask, true);
    if (keys.Matches(Shortcuts::PhraseSelectionCut, mask, true))
        cutSelection();
    processPlayback(mask, true);
};

void PhraseView::setTextProps(GUITextProperties &props, int row, int col,
                              bool restore) {

    bool invert = false;

    if (clipboard_.active_) {
        GUIRect selRect = getSelectionRect();
        if ((row >= selRect.Left()) && (row <= selRect.Right()) &&
            (col >= selRect.Top()) && (col <= selRect.Bottom())) {
            invert = true;
        }
    } else {
        if ((col_ == row) && (row_ == col)) {
            invert = true;
        }
    }

    if (invert) {
        if (restore) {
            SetColor(CD_NORMAL);
            props.invert_ = false;
        } else {
            SetColor(CD_HILITE2);
            props.invert_ = true;
        }
    }
};

void PhraseView::DrawView() {

    Clear();
    View::EnableNotification();

    GUITextProperties props;
    GUIPoint pos = GetTitlePosition();

    // Draw title

    char title[20];

    SetColor(CD_NORMAL);
    sprintf(title, "Phrase %2.2x", viewData_->currentPhrase_);
    DrawString(pos._x, pos._y, title, props);

    // Compute song grid location

    GUIPoint anchor = GetAnchor();

    // Display row numbers

    char buffer[6];
    pos = anchor;
    pos._x -= 3;
    for (int j = 0; j < 16; j++) {
        ((j / altRowNumber_) % 2) ? SetColor(CD_ROW) : SetColor(CD_ROW2);
        hex2char(j, buffer);
        DrawString(pos._x, pos._y, buffer, props);
        pos._y++;
    }

    // Draw column titles
    pos = anchor;
    pos._y -= 1;
    SetColor(CD_COL_TITLE);
    DrawString(pos._x, pos._y, "Nt", props);
    DrawString(pos._x + 5, pos._y, "Vl", props);
    DrawString(pos._x + 8, pos._y, "I", props);
    DrawString(pos._x + 11, pos._y, "Cmd1", props);
    DrawString(pos._x + 16, pos._y, "", props);
    DrawString(pos._x + 21, pos._y, "Cmd2", props);
    DrawString(pos._x + 26, pos._y, "", props);

    SetColor(CD_NORMAL);

    pos = anchor;

    // Display notes

    unsigned char *data = phrase_->note_ + (16 * viewData_->currentPhrase_);

    buffer[4] = 0;
    for (int j = 0; j < 16; j++) {
        unsigned char d = *data++;
        setTextProps(props, 0, j, false);
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        if (d == 0xFF) {
            DrawString(pos._x, pos._y, "----", props);
        } else {
            note2char(d, buffer);
            DrawString(pos._x, pos._y, buffer, props);
        }
        setTextProps(props, 0, j, true);
        pos._y++;
    }

    // Per-note velocity, 00..7F.
    pos = anchor;
    pos._x += 5;
    for (int j = 0; j < 16; ++j) {
        setTextProps(props, 1, j, false);
        SetColor(CD_NORMAL);
        int index = 16 * viewData_->currentPhrase_ + j;
        if (phrase_->note_[index] == 0xFF) {
            DrawString(pos._x, pos._y++, "--", props);
        } else {
            hex2char(phrase_->velocity_[index], buffer);
            DrawString(pos._x, pos._y++, buffer, props);
        }
        setTextProps(props, 1, j, true);
    }

    // Draw instruments

    pos = anchor;
    pos._x += 8;

    data = phrase_->instr_ + (16 * viewData_->currentPhrase_);

    for (int j = 0; j < 16; j++) {
        unsigned char d = *data++;
        setTextProps(props, 2, j, false);
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        if (d == 0xFF) {
            SetColor(CD_NORMAL);
            DrawString(pos._x, pos._y, "--", props);
        } else {
            hex2char(d, buffer);
            DrawString(pos._x, pos._y, buffer, props);
            if (j == row_ && (col_ == 0 || col_ == 2)) {
                SetColor(CD_NORMAL);
                sprintf(buffer, "I%2.2x: ", d);
                std::string instrLine = buffer;
                setTextProps(props, 2, j, true);
                GUIPoint location = GetTitlePosition();
                location._x += 12;
                InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
                I_Instrument *instr = bank->GetInstrument(d);
                instrLine += instr->GetName();
                DrawString(location._x, location._y, instrLine.c_str(), props);
            }
        }
        setTextProps(props, 2, j, true);
        pos._y++;
    }

    // Draw command 1

    pos = anchor;
    pos._x += 11;

    FourCC *f = phrase_->cmd1_ + (16 * viewData_->currentPhrase_);

    buffer[4] = 0;

    for (int j = 0; j < 16; j++) {
        FourCC command = *f++;
        fourCC2char(command, buffer);
        setTextProps(props, 3, j, false);
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 3, j, true);
        pos._y++;
        if (j == row_ && col_ == 3) {
            printHelpLegend(command, props);
        }
    }

    // Draw commands params 1

    pos = anchor;
    pos._x += 16;

    ushort *param = phrase_->param1_ + (16 * viewData_->currentPhrase_);
    buffer[5] = 0;

    for (int j = 0; j < 16; j++) {
        ushort p = *param++;
        setTextProps(props, 4, j, false);
        /*		if (p==0xFFFF) {
                    DrawString(pos._x,pos._y,"----",props) ;
                } else {
        */ (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                   : SetColor(CD_NORMAL);
        hexshort2char(p, buffer);
        DrawString(pos._x, pos._y, buffer, props);
        /*		}
         */
        setTextProps(props, 4, j, true);
        pos._y++;
    }

    // Draw commands 2

    pos = anchor;
    pos._x += 21;

    f = phrase_->cmd2_ + (16 * viewData_->currentPhrase_);

    buffer[4] = 0;

    for (int j = 0; j < 16; j++) {
        FourCC command = *f++;
        fourCC2char(command, buffer);
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        setTextProps(props, 5, j, false);
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 5, j, true);
        pos._y++;
        if (j == row_ && col_ == 5) {
            printHelpLegend(command, props);
        }
    }

    // Draw commands params

    pos = anchor;
    pos._x += 26;

    param = phrase_->param2_ + (16 * viewData_->currentPhrase_);
    buffer[5] = 0;

    for (int j = 0; j < 16; j++) {
        ushort p = *param++;
        setTextProps(props, 6, j, false);
        /*		if (p==0xFFFF) {
                    DrawString(pos._x,pos._y,"----",props) ;
                } else {
        */ (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                   : SetColor(CD_NORMAL);
        hexshort2char(p, buffer);
        DrawString(pos._x, pos._y, buffer, props);
        /*		}
         */
        setTextProps(props, 6, j, true);
        pos._y++;
    }

    drawMap(true);
    drawNotes();

    Player *player = Player::GetInstance();
    if (player->IsRunning()) {
        OnPlayerUpdate(PET_UPDATE);
    };

    if ((viewMode_ != VM_SELECTION) && ((col_ == 4) || (col_ == 6))) {
        cmdEditField_->SetFocus();
        cmdEditField_->Draw(w_);
    };
};

void PhraseView::OnPlayerUpdate(PlayerEventType eventType, unsigned int tick) {

    GUITextProperties props;
    drawNotes();

    GUIPoint anchor = GetAnchor();
    GUIPoint pos = anchor;
    pos._x -= 1;

    SetColor(CD_NORMAL);

    pos._y = anchor._y + lastPlayingPos_;
    if (!commandSelectorModalActive_ ||
        !CommandSelectorCommon::popupContainsPoint(anchor, pos._x, pos._y)) {
        DrawString(pos._x, pos._y, " ", props);
    }

    Player *player = Player::GetInstance();

    if (eventType != PET_STOP) {

        // Clear current position if needed

        for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
            if (player->IsChannelPlaying(i)) {
                if (viewData_->currentPlayPhrase_[i] ==
                        viewData_->currentPhrase_ &&
                    viewData_->playMode_ != PM_AUDITION) {
                    pos._y = anchor._y + viewData_->phrasePlayPos_[i];
                    if (!commandSelectorModalActive_ ||
                        !CommandSelectorCommon::popupContainsPoint(
                            anchor, pos._x, pos._y)) {
                        if (!player->IsChannelMuted(i)) {
                            SetColor(CD_PLAY);
                            DrawString(pos._x, pos._y, ">", props);
                        } else {
                            SetColor(CD_MUTE);
                            DrawString(pos._x, pos._y, "-", props);
                        }
                    }
                    SetColor(CD_NORMAL);
                    lastPlayingPos_ = viewData_->phrasePlayPos_[i];
                    break;
                }
            }
        }

        // clear any live indicator
        pos._y = anchor._y;

        // Loop on all channels to see if one has queued current chain
        if (player->GetSequencerMode() == SM_LIVE) {

            for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
                // is anything queued ?
                if (player->GetQueueingMode(i) != QM_NONE) {
                    // find the chain queued in channel
                    unsigned char songPos = player->GetQueuePosition(i);
                    unsigned char *chain = viewData_->song_->data_ + i +
                                           SONG_CHANNEL_COUNT * songPos;
                    if (*chain == viewData_->currentChain_) {
                        char *indicator = player->GetLiveIndicator(i);
                        if (!commandSelectorModalActive_ ||
                            !CommandSelectorCommon::popupContainsPoint(
                                anchor, pos._x, pos._y)) {
                            DrawString(pos._x, pos._y, indicator, props);
                        }
                        break;
                    }
                }
            }
        }
    }

    pos = anchor;
    pos._x += 200;

    /*	if (player->Clipped()) {
               w_.DrawString("clip",pos,props);
        } else {
               w_.DrawString("----",pos,props);
        }
    */
};

void PhraseView::printHelpLegend(FourCC command, GUITextProperties props) {
    SetColor(CD_NORMAL);
    std::string *cmdStr = getHelpLegend(command);
    DrawString(GetAnchor()._x, 0, cmdStr[0].c_str(), props);
    DrawString(GetAnchor()._x, 1, cmdStr[1].c_str(), props);
    DrawString(GetAnchor()._x, 2, cmdStr[2].c_str(), props);
}
