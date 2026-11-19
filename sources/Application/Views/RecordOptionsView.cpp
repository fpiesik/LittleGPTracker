#include "RecordOptionsView.h"
#include "Adapters/SDL2/Audio/SDLAudioDriver.h"
#include "Application/Controllers/Shortcuts.h"

RecordOptionsView::RecordOptionsView(GUIWindow &window, ViewData *data)
    : View(window, data), selected_(0) {}

void RecordOptionsView::OnFocus() {
    selected_ = 0;
    isDirty_ = true;
}

void RecordOptionsView::DrawView() {
    Clear();
    GUIPoint title = GetTitlePosition();
    GUIPoint anchor = GetAnchor();
    SetColor(CD_NORMAL);
    GUITextProperties props;
    DrawString(title._x, title._y, "Record Options", props);
    const char *metronome[] = {"off", "record", "play and rec"};
    char countIn[24];
    if (SDLAudioDriver::RecordCountIn == 0)
        snprintf(countIn, sizeof(countIn), "Count in: off");
    else
        snprintf(countIn, sizeof(countIn), "Count in: %d",
                 SDLAudioDriver::RecordCountIn);
    char volume[32];
    snprintf(volume, sizeof(volume), "Metronome volume: %d%%",
             SDLAudioDriver::RecordMetronomeVolume);
    std::string rows[] = {
        SDLAudioDriver::RecordChannels == 2 ? "Channels: Stereo"
                                            : "Channels: Mono",
        std::string("Metronome: ") + metronome[SDLAudioDriver::RecordMetronome],
        volume, countIn};
    for (int i = 0; i < 4; ++i) {
        props.invert_ = selected_ == i;
        DrawString(anchor._x, anchor._y + i, rows[i].c_str(), props);
    }
    drawMap();
}

void RecordOptionsView::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed)
        return;
    const Shortcuts &keys = Shortcuts::Get();
    if (keys.Matches(Shortcuts::RecordOptionsUp, mask, pressed)) {
        selected_ = (selected_ + 3) % 4;
        isDirty_ = true;
    } else if (keys.Matches(Shortcuts::RecordOptionsDown, mask, pressed)) {
        selected_ = (selected_ + 1) % 4;
        isDirty_ = true;
    } else if (keys.Matches(Shortcuts::RecordOptionsLeft, mask, pressed) ||
               keys.Matches(Shortcuts::RecordOptionsRight, mask, pressed)) {
        int direction =
            keys.Matches(Shortcuts::RecordOptionsLeft, mask, pressed) ? -1 : 1;
        if (selected_ == 0)
            SDLAudioDriver::RecordChannels = 3 - SDLAudioDriver::RecordChannels;
        else if (selected_ == 1)
            SDLAudioDriver::RecordMetronome =
                (SDLAudioDriver::RecordMetronome + direction + 3) % 3;
        else if (selected_ == 2)
            SDLAudioDriver::RecordMetronomeVolume =
                SDL_max(0, SDL_min(100, SDLAudioDriver::RecordMetronomeVolume +
                                            direction * 10));
        else
            SDLAudioDriver::RecordCountIn =
                (SDLAudioDriver::RecordCountIn + direction + 9) % 9;
        isDirty_ = true;
    }
    if (keys.Matches(Shortcuts::RecordOptionsBack, mask, pressed)) {
        ViewType type = VT_PROJECT;
        ViewEvent event(VET_SWITCH_VIEW, &type);
        SetChanged();
        NotifyObservers(&event);
    }
}
