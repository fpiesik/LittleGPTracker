#ifndef _RECORD_OPTIONS_VIEW_H_
#define _RECORD_OPTIONS_VIEW_H_

#include "BaseClasses/View.h"

class RecordOptionsView : public View {
  public:
    RecordOptionsView(GUIWindow &window, ViewData *data);
    void OnFocus();
    void OnPlayerUpdate(PlayerEventType, unsigned int) {}
    void DrawView();
    void ProcessButtonMask(unsigned short mask, bool pressed);

  private:
    int selected_;
};

#endif
