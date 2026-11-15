#ifndef _FX_VIEW_H_
#define _FX_VIEW_H_

#include "BaseClasses/FieldView.h"

class FxView : public FieldView {
  public:
    FxView(GUIWindow &window, ViewData *data);
    void ProcessButtonMask(unsigned short mask, bool pressed);
    void DrawView();
    void OnPlayerUpdate(PlayerEventType, unsigned int = 0) {}
    void OnFocus() { isDirty_ = true; }
};
#endif
