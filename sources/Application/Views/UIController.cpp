#include "UIController.h"

#include "Application/Player/Player.h"

UIController::UIController() : soloFrom_(-1), soloTo_(-1){};

UIController *UIController::GetInstance() {
    if (instance_ == 0) {
        instance_ = new UIController();
    }
    return instance_;
}

void UIController::Init(Project *project, ViewData *viewData) {
    viewData_ = viewData;
    project_ = project;
}

void UIController::Reset() {
    soloFrom_ = soloTo_ = -1;
    viewData_ = 0;
    project_ = 0;
};

void UIController::UnMuteAll() {
    soloFrom_ = soloTo_ = -1;

    Player *player = Player::GetInstance();
    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        player->SetChannelMute(i, false);
    };
};

void UIController::ToggleMute(int from, int to) {
    // A manual mute edit ends solo tracking and keeps the current mute mask.
    soloFrom_ = soloTo_ = -1;

    Player *player = Player::GetInstance();
    for (int i = from; i < to + 1; i++) {
        bool muted = player->IsChannelMuted(i);
        player->SetChannelMute(i, !muted);
    };
};

void UIController::SwitchSoloMode(int from, int to) {
    Player *player = Player::GetInstance();
    if (soloFrom_ == from && soloTo_ == to) {
        for (int i = 0; i < SONG_CHANNEL_COUNT; ++i)
            player->SetChannelMute(i, soloMask_[i]);
        soloFrom_ = soloTo_ = -1;
        return;
    }
    // Retain the original mask when moving solo to another channel/selection.
    if (soloFrom_ < 0) {
        for (int i = 0; i < SONG_CHANNEL_COUNT; ++i)
            soloMask_[i] = player->IsChannelMuted(i);
    }
    soloFrom_ = from;
    soloTo_ = to;
    for (int i = 0; i < SONG_CHANNEL_COUNT; ++i)
        player->SetChannelMute(i, i < from || i > to);
}
