#ifndef RECORD_DIALOG_H
#define RECORD_DIALOG_H

#if defined(PLATFORM_X64)
#include "Application/Views/BaseClasses/ModalView.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string>

// Audio capture is currently provided by the native Linux SDL2 build.
class RecordDialog : public ModalView {
  public:
    explicit RecordDialog(View &parent);
    virtual ~RecordDialog();
    void CancelRecording() {
        discardTake();
        state_ = ERROR;
    }
    virtual void DrawView();
    virtual void AnimationUpdate();
    virtual void OnFocus() {}
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int) {}
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);

  private:
    enum State { COUNT_IN, RECORDING, DECISION, ERROR };
    void beginTake();
    void stopTake();
    void discardTake();
    void fail(const char *message);
    bool drain();
    bool writeHeader();
    bool saveTake();
    bool findResources(int &chain, int &phrase, int &instrument);

    State state_;
    SDL_AudioDeviceID device_;
    int file_;
    std::string path_;
    std::string error_;
    Uint32 bytes_;
    int channels_;
    int row_, column_, selected_;
    SequencerMode previousMode_;
};
#endif
#endif
