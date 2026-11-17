#include "ChainView.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Utils/char.h"
#include "System/Console/Trace.h"
#include "UIController.h"

ChainView::ChainView(GUIWindow &w, ViewData *viewData) : View(w, viewData) {
    updatingPhrase_ = false;
    lastPhrase_ = 0;
    lastPlayingPos_ = 0;
    lastQueuedPos_ = 0;

    clipboard_.active_ = false;
    clipboard_.width_ = 0;
    clipboard_.height_ = 0;

    for (int i = 0; i < 16; i++) {
        clipboard_.phrase_[i] = 0xFF;
        clipboard_.transpose_[i] = 0;
    };
}

void ChainView::setPhrase(unsigned char value) {
    viewData_->SetChainPhrase(value);
    lastPhrase_ = value;
    isDirty_ = true;
}

void ChainView::cutPosition() {
    clipboard_.active_ = true;
    clipboard_.row_ = viewData_->chainRow_;
    clipboard_.col_ = viewData_->chainCol_;
    saveRow_ = viewData_->chainRow_;
    saveCol_ = viewData_->chainCol_;
    cutSelection();
};

void ChainView::pasteLastPhrase() {

    // If we're on an empty spot, we past the last phrase
    // otherwise we take the current phrase as last

    unsigned char *c = viewData_->GetCurrentChainPointer();
    if ((*c == 0xFF)) {
        *c = lastPhrase_;
        isDirty_ = true;
    } else {
        lastPhrase_ = *c;
    }
};

void ChainView::updateCursor(int dx, int dy) {
    viewData_->UpdateChainCursor(dx, dy);
    isDirty_ = true;
}

void ChainView::updateCursorValue(int offset, int dx, int dy) {

    unsigned char v = viewData_->UpdateChainCursorValue(offset, dx, dy);
    if (viewData_->chainCol_ == 0) {
        lastPhrase_ = v;
        updatingPhrase_ = true;
        updateRow_ = viewData_->chainRow_;
    }
    isDirty_ = true;
}

void ChainView::updateSelectionValue(int offset) { // HERE

    int savecol = viewData_->chainCol_;
    int saverow = viewData_->chainRow_;
    GUIRect r = getSelectionRect();
    viewData_->chainCol_ = r.Left();
    viewData_->chainRow_ = r.Top();
    for (int i = 0; i <= r.Width(); i++) {
        for (int j = 0; j <= r.Height(); j++) {
            updateCursorValue(offset, i, j);
        }
    }
    viewData_->chainCol_ = savecol;
    viewData_->chainRow_ = saverow;
}

void ChainView::warpInColumn(int offset) {
    // save current data
    int saveY = viewData_->songY_;
    int saveOffset = viewData_->songOffset_;

    // Jumping more than the rows displayed on screen causes infinite loop
    while (viewData_->songY_ >= 0 && viewData_->songY_ < 21) {
        viewData_->UpdateSongCursor(0, offset);
        unsigned char *data = viewData_->GetCurrentSongPointer();
        if (*data != 0xFF) {
            viewData_->currentChain_ = *data;
            isDirty_ = true;
            return;
        }
    }
    // restore old position
    viewData_->songY_ = saveY;
    viewData_->songOffset_ = saveOffset;
};

void ChainView::warpToNeighbour(int offset) {
    // save current data
    int saveX = viewData_->songX_;
    int saveOffset = viewData_->songOffset_;
    int newPos = saveX + offset;

    while ((newPos > -1) && (newPos < SONG_CHANNEL_COUNT)) {
        viewData_->songX_ = newPos;
        unsigned char *c = viewData_->GetCurrentSongPointer();
        if (*c != 0xFF) {
            viewData_->currentChain_ = *c;
            isDirty_ = true;
            return;
        } else {
            newPos += offset;
        }
    }
    viewData_->songX_ = saveX;
    viewData_->songOffset_ = saveOffset;
};

void ChainView::clonePosition() {

    unsigned char *pos = viewData_->GetCurrentChainPointer();
    unsigned char current = *pos;
    if (current == 255)
        return;

    unsigned short next = viewData_->song_->phrase_->GetNext();
    if (next == NO_MORE_PHRASE)
        return;

    unsigned char *src = viewData_->song_->phrase_->note_ + 16 * current;
    unsigned char *dst = viewData_->song_->phrase_->note_ + 16 * next;
    for (int i = 0; i < 16; i++) {
        *dst++ = *src++;
    };

    src = viewData_->song_->phrase_->instr_ + 16 * current;
    dst = viewData_->song_->phrase_->instr_ + 16 * next;
    for (int i = 0; i < 16; i++) {
        *dst++ = *src++;
    };

    memcpy(viewData_->song_->phrase_->velocity_ + 16 * next,
           viewData_->song_->phrase_->velocity_ + 16 * current, 16);

    uint *isrc = viewData_->song_->phrase_->cmd1_ + 16 * current;
    uint *idst = viewData_->song_->phrase_->cmd1_ + 16 * next;
    for (int i = 0; i < 16; i++) {
        *idst++ = *isrc++;
    };

    ushort *ssrc = viewData_->song_->phrase_->param1_ + 16 * current;
    ushort *sdst = viewData_->song_->phrase_->param1_ + 16 * next;
    for (int i = 0; i < 16; i++) {
        *sdst++ = *ssrc++;
    };

    isrc = viewData_->song_->phrase_->cmd2_ + 16 * current;
    idst = viewData_->song_->phrase_->cmd2_ + 16 * next;
    for (int i = 0; i < 16; i++) {
        *idst++ = *isrc++;
    };

    ssrc = viewData_->song_->phrase_->param2_ + 16 * current;
    sdst = viewData_->song_->phrase_->param2_ + 16 * next;
    for (int i = 0; i < 16; i++) {
        *sdst++ = *ssrc++;
    };

    setPhrase((unsigned char)next);
    isDirty_ = true;
};

/******************************************************
 getSelectionRect:
        gets the normalized rectangle of the current
        selection. Valid only while selection is drawn
 ******************************************************/

GUIRect ChainView::getSelectionRect() {
    GUIRect r(clipboard_.col_, clipboard_.row_, viewData_->chainCol_,
              viewData_->chainRow_);
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

void ChainView::fillClipboardData() {

    // Get Current normalized selection rect

    GUIRect selRect = getSelectionRect();

    // Get size & store in clipboard

    clipboard_.width_ = selRect.Width() + 1;
    clipboard_.height_ = selRect.Height() + 1;
    clipboard_.row_ = selRect.Top();
    ;
    clipboard_.col_ = selRect.Left();

    // Copy the data

    unsigned char *src1 =
        viewData_->song_->chain_->data_ + 16 * viewData_->currentChain_;
    unsigned char *dst1 = clipboard_.phrase_;
    unsigned char *src2 =
        viewData_->song_->chain_->transpose_ + 16 * viewData_->currentChain_;
    unsigned char *dst2 = clipboard_.transpose_;

    for (int i = 0; i < clipboard_.height_; i++) {
        dst1[i] = src1[clipboard_.row_ + i];
        dst2[i] = src2[clipboard_.row_ + i];
    };
};

void ChainView::extendSelection() {
    GUIRect rect = getSelectionRect();
    if (rect.Left() > 0 || rect.Right() < 1) {
        if (viewData_->chainCol_ < clipboard_.col_) {
            viewData_->chainCol_ = 0;
            clipboard_.col_ = 1;
        } else {
            viewData_->chainCol_ = 1;
            clipboard_.col_ = 0;
        }
        isDirty_ = true;
    } else {
        if (viewData_->chainRow_ < clipboard_.row_) {
            viewData_->chainRow_ = 0;
            clipboard_.row_ = 15;
        } else {
            clipboard_.row_ = 0;
            viewData_->chainRow_ = 15;
        }
        isDirty_ = true;
    }
}

/******************************************************
 copySelection:
        copies data in the current selection to the
        clipboard & end selection process
 ******************************************************/

void ChainView::copySelection() {

    // Keep up with row,col of selection coz
    // fillClipboardData will trash it

    //	saveClipboardPosition() ;

    fillClipboardData();

    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;

    viewData_->chainRow_ = saveRow_;
    viewData_->chainCol_ = saveCol_;

    View::SetNotification("copied selection");
};

/******************************************************
 cut:  copies data in the current selection to the
       clipboard, clear selection content & end selection
       process
 ******************************************************/

void ChainView::cutSelection() {

    // Keep up with row,col of selection coz
    // fillClipboardData will trash it

    //	saveClipboardPosition() ;

    fillClipboardData();

    // Loop over selection col, row & clear data inside it

    unsigned char *dst1 =
        viewData_->song_->chain_->data_ + 16 * viewData_->currentChain_;
    unsigned char *dst2 =
        viewData_->song_->chain_->transpose_ + 16 * viewData_->currentChain_;

    for (int i = 0; i < clipboard_.width_; i++) {
        for (int j = 0; j < clipboard_.height_; j++) {
            switch (i + clipboard_.col_) {
            case 0:
                dst1[j + clipboard_.row_] = 0xFF;
                break;
            case 1:
                dst2[j + clipboard_.row_] = 00;
                break;
            }
        }
    }

    // Clear selection, end selection process & reposition cursor

    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    viewData_->chainRow_ = saveRow_;
    viewData_->chainCol_ = saveCol_;
    isDirty_ = true;
};

/******************************************************
 pasteClipboard:
        copies data in the clipboard to the current step
 ******************************************************/

void ChainView::pasteClipboard() {

    // Get number of row to paste

    int height = clipboard_.height_;
    if (viewData_->chainRow_ + height > 16) {
        height = 16 - viewData_->chainRow_;
    }

    unsigned char *dst1 =
        viewData_->song_->chain_->data_ + 16 * viewData_->currentChain_;
    unsigned char *src1 = clipboard_.phrase_;
    unsigned char *dst2 =
        viewData_->song_->chain_->transpose_ + 16 * viewData_->currentChain_;
    unsigned char *src2 = clipboard_.transpose_;

    for (int i = 0; i < clipboard_.width_; i++) {
        for (int j = 0; j < height; j++) {
            switch (i + clipboard_.col_) {
            case 0:
                dst1[j + viewData_->chainRow_] = src1[j];
                break;
            case 1:
                dst2[j + viewData_->chainRow_] = src2[j];
                break;
            }
        }
    }
    updateCursor(0x00, height);
    isDirty_ = true;
};

void ChainView::unMuteAll() {

    UIController *controller = UIController::GetInstance();
    controller->UnMuteAll();
};

void ChainView::toggleMute() {

    UIController *controller = UIController::GetInstance();
    controller->ToggleMute(viewData_->songX_, viewData_->songX_);
    viewMode_ = (viewMode_ != VM_MUTEON) ? VM_MUTEON : VM_NORMAL;
};

void ChainView::switchSoloMode() {

    UIController *controller = UIController::GetInstance();
    controller->SwitchSoloMode(viewData_->songX_, viewData_->songX_);
    viewMode_ = (viewMode_ == VM_SOLOON) ? VM_NORMAL : VM_SOLOON;
    isDirty_ = true;
};

void ChainView::ProcessButtonMask(unsigned short mask, bool pressed) {

    const Shortcuts &shortcuts = Shortcuts::Get();
    if (!pressed) {
        if (viewMode_ == VM_MUTEON) {
            if (shortcuts.Matches(Shortcuts::ChainMuteRelease, mask, pressed)) {
                toggleMute();
            }
        };
        if (viewMode_ == VM_SOLOON &&
            shortcuts.Matches(Shortcuts::ChainSoloRelease, mask, pressed)) {
            switchSoloMode();
        }
        return;
    };

    if (viewMode_ == VM_NEW) {
        if (shortcuts.Matches(Shortcuts::ChainNextUnused, mask, pressed)) {
            unsigned short next = viewData_->song_->phrase_->GetNext();
            if (next != NO_MORE_PHRASE) {
                setPhrase((unsigned char)next);
                isDirty_ = true;
            }
            mask = shortcuts.RemainingButtons(Shortcuts::ChainNextUnused, mask);
        }
    }

    if (viewMode_ == VM_CLONE) {
        if (shortcuts.Matches(Shortcuts::ChainClone, mask, pressed)) {
            clonePosition();
            mask = shortcuts.RemainingButtons(Shortcuts::ChainClone, mask);
        } else {
            viewMode_ = VM_SELECTION;
        }
    };

    // Process selection related keys

    if (viewMode_ == VM_SELECTION) {

        if (clipboard_.active_ == false) {
            clipboard_.active_ = true;
            clipboard_.col_ = viewData_->chainCol_;
            clipboard_.row_ = viewData_->chainRow_;
            saveCol_ = viewData_->chainCol_;
            saveRow_ = viewData_->chainRow_;
        }
        processSelectionButtonMask(mask);
    } else {

        // Switch back to normal mode

        viewMode_ = VM_NORMAL;
        processNormalButtonMask(mask);
    }
}

void ChainView::processNormalButtonMask(unsigned short mask) {
    const Shortcuts &shortcuts = Shortcuts::Get();
    Player *player = Player::GetInstance();
    if (shortcuts.Matches(Shortcuts::ChainPreviousChannel, mask, true)) {
        warpToNeighbour(-1);
    }
    if (shortcuts.Matches(Shortcuts::ChainNextChannel, mask, true)) {
        warpToNeighbour(1);
    }
    if (shortcuts.Matches(Shortcuts::ChainPreviousChain, mask, true)) {
        warpInColumn(-1);
    }
    if (shortcuts.Matches(Shortcuts::ChainNextChain, mask, true)) {
        warpInColumn(1);
    }
    if (shortcuts.Matches(Shortcuts::ChainCut, mask, true)) {
        cutPosition();
    }
    if (shortcuts.Matches(Shortcuts::ChainBeginCloneSelection, mask, true)) {
        viewMode_ = VM_CLONE;
    }
    if (shortcuts.Matches(Shortcuts::ChainMute, mask, true)) {
        toggleMute();
    }
    if (shortcuts.Matches(Shortcuts::ChainDecreaseCoarse, mask, true)) {
        updateCursorValue(viewData_->chainCol_ == 0 ? -0x10 : -0x0C);
    }
    if (shortcuts.Matches(Shortcuts::ChainIncreaseCoarse, mask, true)) {
        updateCursorValue(viewData_->chainCol_ == 0 ? 0x10 : 0x0C);
    }
    if (shortcuts.Matches(Shortcuts::ChainDecreaseFine, mask, true)) {
        updateCursorValue(-1);
    }
    if (shortcuts.Matches(Shortcuts::ChainIncreaseFine, mask, true)) {
        updateCursorValue(1);
    }
    if (shortcuts.Matches(Shortcuts::ChainPaste, mask, true)) {
        pasteClipboard();
    }
    if (shortcuts.Matches(Shortcuts::ChainInsert, mask, true)) {
        pasteLastPhrase();
        if (viewData_->chainCol_ == 0)
            viewMode_ = VM_NEW;
    }
    if (shortcuts.Matches(Shortcuts::ChainSolo, mask, true)) {
        switchSoloMode();
    }
    if (shortcuts.Matches(Shortcuts::ChainSong, mask, true)) {
        ViewType vt = VT_SONG;
        ViewEvent ve(VET_SWITCH_VIEW, &vt);
        SetChanged();
        NotifyObservers(&ve);
    }
    if (shortcuts.Matches(Shortcuts::ChainPhrase, mask, true)) {
        unsigned char *data = viewData_->GetCurrentChainPointer();
        if (*data != 0xFF) {
            ViewType vt = VT_PHRASE;
            ViewEvent ve(VET_SWITCH_VIEW, &vt);
            viewData_->currentPhrase_ = *data;
            SetChanged();
            NotifyObservers(&ve);
        }
    }
    if (shortcuts.Matches(Shortcuts::ChainMixer, mask, true)) {
        ViewType vt = VT_MIXER;
        ViewEvent ve(VET_SWITCH_VIEW, &vt);
        SetChanged();
        NotifyObservers(&ve);
    }
    if (shortcuts.Matches(Shortcuts::ChainPlayFromStart, mask, true)) {
        player->OnStartButton(PM_CHAIN, viewData_->songX_, true,
                              viewData_->chainRow_);
    }
    if (shortcuts.Matches(Shortcuts::ChainUnmuteAll, mask, true)) {
        unMuteAll();
    }
    if (shortcuts.Matches(Shortcuts::ChainDown, mask, true)) {
        updateCursor(0, 1);
    }
    if (shortcuts.Matches(Shortcuts::ChainUp, mask, true)) {
        updateCursor(0, -1);
    }
    if (shortcuts.Matches(Shortcuts::ChainLeft, mask, true)) {
        updateCursor(-1, 0);
    }
    if (shortcuts.Matches(Shortcuts::ChainRight, mask, true)) {
        updateCursor(1, 0);
    }
    if (shortcuts.Matches(Shortcuts::ChainPlay, mask, true)) {
        player->OnStartButton(PM_CHAIN, viewData_->songX_, false,
                              viewData_->chainRow_);
    }
    if (!shortcuts.Matches(Shortcuts::ChainEditHold, mask, true) &&
        updatingPhrase_) {
        unsigned char *c = viewData_->song_->chain_->data_ +
                           (16 * viewData_->currentChain_ + updateRow_);
        viewData_->song_->phrase_->SetUsed(*c);
        updatingPhrase_ = false;
    }
}

void ChainView::processSelectionButtonMask(unsigned short mask) {
    const Shortcuts &shortcuts = Shortcuts::Get();
    Player *player = Player::GetInstance();
    if (shortcuts.Matches(Shortcuts::ChainSelectionCopy, mask, true)) {
        copySelection();
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionMute, mask, true)) {
        toggleMute();
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionExtend, mask, true)) {
        extendSelection();
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionDecreaseCoarse, mask,
                          true)) {
        updateSelectionValue(viewData_->chainCol_ == 0 ? -0x10 : -0x0C);
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionIncreaseCoarse, mask,
                          true)) {
        updateSelectionValue(viewData_->chainCol_ == 0 ? 0x10 : 0x0C);
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionDecreaseFine, mask, true)) {
        updateSelectionValue(-1);
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionIncreaseFine, mask, true)) {
        updateSelectionValue(1);
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionCut, mask, true)) {
        cutSelection();
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionSolo, mask, true)) {
        switchSoloMode();
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionSong, mask, true)) {
        ViewType vt = VT_SONG;
        ViewEvent ve(VET_SWITCH_VIEW, &vt);
        SetChanged();
        NotifyObservers(&ve);
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionPhrase, mask, true)) {
        unsigned char *data = viewData_->GetCurrentChainPointer();
        if (*data != 0xFF) {
            ViewType vt = VT_PHRASE;
            ViewEvent ve(VET_SWITCH_VIEW, &vt);
            viewData_->currentPhrase_ = *data;
            SetChanged();
            NotifyObservers(&ve);
        }
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionMixer, mask, true)) {
        ViewType vt = VT_MIXER;
        ViewEvent ve(VET_SWITCH_VIEW, &vt);
        SetChanged();
        NotifyObservers(&ve);
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionPlayFromStart, mask, true)) {
        player->OnStartButton(PM_CHAIN, viewData_->songX_, true,
                              viewData_->chainRow_);
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionUnmuteAll, mask, true)) {
        unMuteAll();
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionDown, mask, true)) {
        updateCursor(0, 1);
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionUp, mask, true)) {
        updateCursor(0, -1);
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionLeft, mask, true)) {
        updateCursor(-1, 0);
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionRight, mask, true)) {
        updateCursor(1, 0);
    }
    if (shortcuts.Matches(Shortcuts::ChainSelectionPlay, mask, true)) {
        player->OnStartButton(PM_CHAIN, viewData_->songX_, false,
                              viewData_->chainRow_);
    }
}

void ChainView::OnFocus() { clipboard_.active_ = false; };

void ChainView::setTextProps(GUITextProperties &props, int row, int col,
                             bool restore) {

    bool invert = false;

    if (clipboard_.active_) {
        GUIRect selRect = getSelectionRect();
        if ((row >= selRect.Left()) && (row <= selRect.Right()) &&
            (col >= selRect.Top()) && (col <= selRect.Bottom())) {
            invert = true;
        }
    } else {
        if ((viewData_->chainCol_ == row) && (viewData_->chainRow_ == col)) {
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

void ChainView::DrawView() {

    Clear();
    View::EnableNotification();

    GUITextProperties props;
    GUIPoint pos = GetTitlePosition();

    // Draw title

    char title[20];
    SetColor(CD_NORMAL);
    sprintf(title, "Chain %2.2x", viewData_->currentChain_);
    DrawString(pos._x, pos._y, title, props);

    // Compute song grid location

    GUIPoint anchor = GetAnchor();

    // Display row numbers

    char row[3];
    pos = anchor;
    pos._x -= 3;
    for (int j = 0; j < 16; j++) {
        ((j / altRowNumber_) % 2) ? SetColor(CD_ROW) : SetColor(CD_ROW2);
        hex2char(j, row);
        DrawString(pos._x, pos._y, row, props);
        pos._y += 1;
    }

    // Draw column titles
    pos = anchor;
    pos._y -= 1;
    SetColor(CD_COL_TITLE);
    DrawString(pos._x, pos._y, "Ph", props);
    DrawString(pos._x + 3, pos._y, "Ts", props);

    SetColor(CD_NORMAL);

    pos = anchor;

    // Display phrases

    pos = anchor;

    unsigned char *data =
        viewData_->song_->chain_->data_ + (16 * viewData_->currentChain_);

    for (int j = 0; j < 16; j++) {
        unsigned char d = *data++;
        setTextProps(props, 0, j, false);
        if (d == 0xFF) {
            DrawString(pos._x, pos._y, "--", props);
        } else {
            hex2char(d, row);
            DrawString(pos._x, pos._y, row, props);
        }
        setTextProps(props, 0, j, true);
        pos._y++;
    }

    // Draw Transpose

    pos = anchor;
    pos._x += 3;

    data =
        viewData_->song_->chain_->transpose_ + (16 * viewData_->currentChain_);

    for (int j = 0; j < 16; j++) {
        unsigned char d = *data++;
        hex2char(d, row);
        setTextProps(props, 1, j, false);
        DrawString(pos._x, pos._y, row, props);
        setTextProps(props, 1, j, true);
        pos._y++;
    }
    Player *player = Player::GetInstance();

    drawMap();
    drawNotes();

    if (player->IsRunning()) {
        OnPlayerUpdate(PET_UPDATE);
    };
};

void ChainView::OnPlayerUpdate(PlayerEventType eventType, unsigned int tick) {

    Player *player = Player::GetInstance();

    drawNotes();

    GUIPoint anchor = GetAnchor();
    GUIPoint pos = anchor;
    pos._x -= 1;

    GUITextProperties props;
    SetColor(CD_NORMAL);

    // Clear last played & queued

    pos._y = anchor._y + lastPlayingPos_;
    DrawString(pos._x, pos._y, " ", props);

    pos._y = anchor._y + lastQueuedPos_;
    DrawString(pos._x, pos._y, " ", props);

    if (eventType != PET_STOP) {

        // Loop on all channels to see if one of them is playing current chain

        for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
            if (player->IsChannelPlaying(i)) {
                if (viewData_->currentPlayChain_[i] ==
                        viewData_->currentChain_ &&
                    viewData_->playMode_ != PM_AUDITION) {
                    pos._y = anchor._y + viewData_->chainPlayPos_[i];
                    if (!player->IsChannelMuted(i)) {
                        SetColor(CD_PLAY);
                        DrawString(pos._x, pos._y, ">", props);
                        SetColor(CD_NORMAL);
                    } else {
                        SetColor(CD_MUTE);
                        DrawString(pos._x, pos._y, "-", props);
                        SetColor(CD_NORMAL);
                    }
                    lastPlayingPos_ = viewData_->chainPlayPos_[i];
                    break;
                }
            }
        }

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
                        unsigned char chainPos =
                            player->GetQueueChainPosition(i);
                        pos._y = anchor._y + chainPos;
                        char *indicator = player->GetLiveIndicator(i);
                        DrawString(pos._x, pos._y, indicator, props);
                        lastQueuedPos_ = chainPos;
                        break;
                    }
                }
            }
        }
    }

    pos = anchor;
    pos._x += 200;
/*
	if (player->Clipped()) {
           w_.DrawString("clip",pos,props);
    } else {
           w_.DrawString("----",pos,props);
    }
*/} ;
