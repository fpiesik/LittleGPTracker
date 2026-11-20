#include "View.h"
#include "Application/AppWindow.h"
#include "Application/Model/Config.h"
#include "Application/Player/Player.h"
#include "Application/Utils/char.h"
#include "ModalView.h"
#include "System/Console/Trace.h"

bool View::initPrivate_ = false;

int View::margin_ = 0;
bool View::miniLayout_ = false;
int View::altRowNumber_ = 4;

View::View(GUIWindow &w, ViewData *viewData)
    : w_(w), modalView_(0), modalViewCallback_(0), hasFocus_(false) {
    if (!initPrivate_) {
        GUIRect rect = w.GetRect();
        miniLayout_ = (rect.Width() < 320);
        View::margin_ = 0;

        const char *altRowStr = Config::GetInstance()->GetValue("ALTROWNUMBER");
        if (altRowStr) {
            altRowNumber_ = atoi(altRowStr);
        }

        initPrivate_ = true;
    }
    mask_ = 0;
    viewMode_ = VM_NORMAL;
    locked_ = false;
    viewData_ = viewData;
    NOTIFICATION_TIMEOUT = 1000;
    notificationTime_ = SDL_GetTicks() - NOTIFICATION_TIMEOUT - 1;
    displayNotification_ = "";
    // Initialize VU meter inertia tracking
    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        prevLeftVU_[i] = 0;
        prevRightVU_[i] = 0;
    }
};

GUIPoint View::GetAnchor() {
    const int width = 40;
    // Reclaim the former navigation column while retaining mixer label space.
    return GUIPoint(miniLayout_ ? (width - SONG_CHANNEL_COUNT * 3) / 2 + 2 : 5,
                    miniLayout_ ? 7 : 4);
}

bool View::UsesLargeText() const {
    return hasFocus_ &&
           (viewType_ == VT_SONG || viewType_ == VT_CHAIN ||
            viewType_ == VT_PHRASE || viewType_ == VT_TABLE ||
            viewType_ == VT_TABLE2 || viewType_ == VT_MIXER) &&
           w_.GetRect().Width() == 480;
}

GUIPoint View::GetTitlePosition() {
#ifndef PLATFORM_CAANOO
    return GUIPoint(0, 0);
#else
    return GUIPoint(0, 1);
#endif
};

bool View::Lock() {
    if (locked_)
        return false;
    locked_ = true;
    return true;
};

void View::WaitForObject() {
    while (locked_) {
    };
}

void View::Unlock() { locked_ = false; }

int View::GetTrackIndicatorRow() const {
    return UsesLargeText() ? largeTextRows_ - trackIndicatorRows_
                           : AppWindow::SCREEN_ROWS - trackIndicatorRows_;
}

void View::drawMap(bool withTrackIndicators) {
    if (!miniLayout_) {
        GUIPoint anchor = GetAnchor();
        const bool largeText = UsesLargeText();
        // Leave a blank row under the four-row map and room beside the Song
        // meter.
        const int mapTop =
            withTrackIndicators ? GetTrackIndicatorRow() - 5 : 26;
        GUIPoint pos(anchor._x + SONG_CHANNEL_COUNT * 3 + (largeText ? 3 : 1),
                     mapTop);
        GUITextProperties props;

        // draw entire map
        SetColor(CD_HILITE1);
        char buffer[5];
        props.invert_ = true;
        // row1
        sprintf(buffer, "P G ");
        DrawString(pos._x, pos._y, buffer, props);
        pos._y++;
        // row2
        sprintf(buffer, "SCPI");
        DrawString(pos._x, pos._y, buffer, props);
        pos._y++;
        // row3
        sprintf(buffer, " MTT");
        DrawString(pos._x, pos._y, buffer, props);
        pos._y++;
        DrawString(pos._x, pos._y, " E  ", props);

        // draw current screen on map
        SetColor(CD_HILITE2);
        pos._y = mapTop;
        switch (viewType_) {
        case VT_CHAIN:
            pos._x += 1;
            pos._y += 1;
            DrawString(pos._x, pos._y, "C", props);
            break;
        case VT_PHRASE:
            pos._x += 2;
            pos._y += 1;
            DrawString(pos._x, pos._y, "P", props);
            break;
        case VT_PROJECT:
#if defined(PLATFORM_X64)
        case VT_SETTINGS:
        case VT_RECORD_OPTIONS:
#endif
            DrawString(pos._x, pos._y, "P", props);
            break;
        case VT_INSTRUMENT:
        case VT_SAMPLE_EDITOR:
            pos._x += 3;
            pos._y += 1;
            DrawString(pos._x, pos._y, "I", props);
            break;
        case VT_TABLE: // under phrase
            pos._x += 2;
            pos._y += 2;
            DrawString(pos._x, pos._y, "T", props);
            break;
        case VT_TABLE2: // under instrument
            pos._x += 3;
            pos._y += 2;
            DrawString(pos._x, pos._y, "T", props);
            break;
        case VT_GROOVE:
            pos._x += 2;
            DrawString(pos._x, pos._y, "G", props);
            break;
        case VT_MIXER:
            pos._x += 1;
            pos._y += 2;
            DrawString(pos._x, pos._y, "M", props);
            break;
        case VT_FX:
            pos._x += 1;
            pos._y += 3;
            DrawString(pos._x, pos._y, "E", props);
            break;
        default: // VT_SONG
            pos._y += 1;
            DrawString(pos._x, pos._y, "S", props);
            int foo = 0;
        }

    } //! minilayout
}

void View::drawNotes() {
    if (miniLayout_)
        return;

    const int row = GetTrackIndicatorRow();
    Player *player = Player::GetInstance();
    const bool playing =
        player->IsRunning() && viewData_->playMode_ != PM_AUDITION;
    GUITextProperties props;
    props.invert_ = true;
    const int firstTrack = GetAnchor()._x;
    for (int i = 0; i < SONG_CHANNEL_COUNT; ++i) {
        const int column = firstTrack + i * 3;
        SetColor(i == viewData_->songX_ ? CD_HILITE2 : CD_HILITE1);
        const char *note = playing ? player->GetPlayedNote(i) : "  ";
        char first[] = {note[0], 0};
        props.indicatorEdge_ = GUITextProperties::INDICATOR_EDGE_LEFT;
        props.extendBackgroundTop1_ = true;
        DrawString(column, row, first, props);
        props.indicatorEdge_ = GUITextProperties::INDICATOR_EDGE_NONE;
        DrawString(column + 1, row, note + 1, props);
        const char *octave = playing ? player->GetPlayedOctive(i) : "  ";
        // A minus sign identifies the only legacy octave below C0.
        char digit[] = {octave[0] == '-' ? '-' : octave[1], 0};
        props.indicatorEdge_ =
            GUITextProperties::INDICATOR_EDGE_RIGHT_SHIFT_LEFT;
        DrawString(column + 2, row, digit, props);
        props.extendBackgroundTop1_ = false;
        const char *instrument =
            playing ? player->GetPlayedInstrument(i) : "  ";
        first[0] = instrument[0];
        props.indicatorEdge_ = GUITextProperties::INDICATOR_EDGE_LEFT;
        DrawString(column, row + 1, first, props);
        props.indicatorEdge_ = GUITextProperties::INDICATOR_EDGE_NONE;
        DrawString(column + 1, row + 1, instrument + 1, props);
        props.indicatorEdge_ = GUITextProperties::INDICATOR_EDGE_RIGHT;
        DrawString(column + 2, row + 1, " ", props);
    }
}

void View::DoModal(ModalView *view, ModalViewCallback cb) {
    modalView_ = view;
    modalView_->OnFocus();
    modalViewCallback_ = cb;
    isDirty_ = true;
};

void View::Redraw() {
    if (modalView_) {
        if (isDirty_) {
            DrawView();
        }
        modalView_->Redraw();
    } else {
        DrawView();
    }
    isDirty_ = false;
};

void View::SetDirty(bool isDirty) { isDirty_ = true; };

void View::ProcessButton(unsigned short mask, bool pressed) {
    isDirty_ = false;
    if (modalView_) {
        modalView_->ProcessButton(mask, pressed);
        modalView_->isDirty_;
        if (modalView_->IsFinished()) {
            // process callback sending the modal dialog
            if (modalViewCallback_) {
                modalViewCallback_(*this, *modalView_);
            }
            SAFE_DELETE(modalView_);
            isDirty_ = true;
        }
    } else {
        ProcessButtonMask(mask, pressed);
    }
    if (isDirty_)
        ((AppWindow &)w_).SetDirty();
};

void View::Clear() { ((AppWindow &)w_).Clear(); }

void View::SetColor(ColorDefinition cd) { ((AppWindow &)w_).SetColor(cd); };

void View::ClearRect(int x, int y, int w, int h) {
    GUIRect rect(x, y, (x + w), (y + h));
    w_.ClearRect(rect);
};

void View::DrawString(int x, int y, const char *txt, GUITextProperties &props) {
    GUIPoint pos(x, y);
    w_.DrawString(txt, pos, props);
};

/*
    Displays the saved notification for 1 second
*/
void View::EnableNotification() {
    if ((SDL_GetTicks() - notificationTime_) <= NOTIFICATION_TIMEOUT) {
        SetColor(CD_NORMAL);
        GUITextProperties props;
        int xOffset = 4;
        // support single-line or two-line notifications separated by '\n'
        size_t nl = displayNotification_.find('\n');
        if (nl == std::string::npos) {
            DrawString(xOffset, notiDistY_, displayNotification_.c_str(),
                       props);
        } else {
            std::string line1 = displayNotification_.substr(0, nl);
            std::string line2 = displayNotification_.substr(nl + 1);
            DrawString(xOffset, notiDistY_, line1.c_str(), props);
            DrawString(xOffset, notiDistY_ + 1, line2.c_str(), props);
        }
    } else {
        displayNotification_ = "";
    }
}

/*
    Set displayed notification
    Saves the current time
    Optionally set display y offset if not in a project (default == 2)
    Allows negative offsets, use with care!
*/
void View::SetNotification(const char *notification, int offset) {
    notificationTime_ = SDL_GetTicks();
    displayNotification_ = notification;
    notiDistY_ = offset;
    isDirty_ = true;
}

void View::drawVUMeter(uint8_t leftBars, uint8_t rightBars, GUIPoint pos,
                       GUITextProperties props, int vuIndex, bool forceRedraw) {
    // Clamp bars to VU meter height
    leftBars = (leftBars > VU_METER_HEIGHT) ? VU_METER_HEIGHT : leftBars;
    rightBars = (rightBars > VU_METER_HEIGHT) ? VU_METER_HEIGHT : rightBars;

    // Apply inertia effect: fast rise, slow fall
    const int maxStepUp = 2;   // fast rise (up to 2 bars per update)
    const int maxStepDown = 1; // slow fall (only 1 bar per update)

    // Left channel inertia
    if (leftBars > prevLeftVU_[vuIndex]) {
        // Rising: allow faster response (up to maxStepUp)
        int diff = leftBars - prevLeftVU_[vuIndex];
        if (diff > maxStepUp) {
            leftBars = prevLeftVU_[vuIndex] + maxStepUp;
        }
    } else if (leftBars < prevLeftVU_[vuIndex]) {
        // Falling: limit to maxStepDown for inertia
        int diff = prevLeftVU_[vuIndex] - leftBars;
        if (diff > maxStepDown) {
            leftBars = prevLeftVU_[vuIndex] - maxStepDown;
        }
    }

    // Right channel inertia
    if (rightBars > prevRightVU_[vuIndex]) {
        int diff = rightBars - prevRightVU_[vuIndex];
        if (diff > maxStepUp) {
            rightBars = prevRightVU_[vuIndex] + maxStepUp;
        }
    } else if (rightBars < prevRightVU_[vuIndex]) {
        int diff = prevRightVU_[vuIndex] - rightBars;
        if (diff > maxStepDown) {
            rightBars = prevRightVU_[vuIndex] - maxStepDown;
        }
    }

    // Skip redraw if values unchanged (unless forced)
    if (!forceRedraw && leftBars == prevLeftVU_[vuIndex] &&
        rightBars == prevRightVU_[vuIndex]) {
        return;
    }

    // Store current values for next update
    prevLeftVU_[vuIndex] = leftBars;
    prevRightVU_[vuIndex] = rightBars;

    // Draw the VU meter bars (simple bar representation)
    std::string bar = "";
    for (int i = 0; i < VU_METER_HEIGHT; i++) {
        if (i < leftBars) {
            bar += "|";
        } else {
            bar += " ";
        }
    }
    bar += " ";
    for (int i = 0; i < VU_METER_HEIGHT; i++) {
        if (i < rightBars) {
            bar += "|";
        } else {
            bar += " ";
        }
    }
    DrawString(pos._x, pos._y, bar.c_str(), props);
}
