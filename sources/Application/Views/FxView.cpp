#include "FxView.h"
#include "Application/Model/Mixer.h"
#include "BaseClasses/UIIntVarField.h"

FxView::FxView(GUIWindow &window, ViewData *data) : FieldView(window, data) {
    const char *labels[] = {"DECAY: %2.2X", "DAMP:  %2.2X", "DIFF:  %2.2X",
                            "MOD:   %2.2X", "LEVEL: %2.2X"};
    GUIPoint position = GetAnchor();
    for (int i = 0; i < Mixer::FX_COUNT; ++i) {
        T_SimpleList<UIField>::Insert(new UIIntVarField(
            position, Mixer::GetInstance()->GetReverbParameter(i), labels[i], 0,
            255, 1, 16));
        position._y += 2;
    }
    SetFocus(T_SimpleList<UIField>::GetFirst());
}

void FxView::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed)
        return;
    if (mask & EPBM_R) {
        if (mask & EPBM_UP) {
            ViewType type = VT_MIXER;
            ViewEvent event(VET_SWITCH_VIEW, &type);
            SetChanged();
            NotifyObservers(&event);
        } else if (mask & EPBM_START) {
            Player::GetInstance()->OnStartButton(PM_SONG, viewData_->songX_,
                                                 true, viewData_->songX_);
        }
        return;
    }
    if (mask & EPBM_B) {
        if ((mask & EPBM_A) && GetFocus()) {
            static_cast<UIIntVarField *>(GetFocus())->GetVariable().Reset();
            isDirty_ = true;
        }
        return;
    }
    if (mask & EPBM_START) {
        Player::GetInstance()->OnStartButton(PM_SONG, viewData_->songX_, false,
                                             viewData_->songX_);
        return;
    }
    FieldView::ProcessButtonMask(mask);
}

void FxView::DrawView() {
    Clear();
    GUITextProperties props;
    GUIPoint title = GetTitlePosition();
    SetColor(CD_NORMAL);
    DrawString(title._x, title._y, "E - Clouds Reverb", props);
    FieldView::Redraw();
    drawMap();
    EnableNotification();
}
