#ifndef LGPT_SETTINGS_VIEW_H
#define LGPT_SETTINGS_VIEW_H

#include "BaseClasses/View.h"

class SettingsView : public View {
  public:
    SettingsView(GUIWindow &window, ViewData *data) : View(window, data) {}
    void DrawView();
    void ProcessButtonMask(unsigned short mask, bool pressed);
    void OnFocus() { isDirty_ = true; }
    void OnPlayerUpdate(PlayerEventType, unsigned int) {}
};

#endif
