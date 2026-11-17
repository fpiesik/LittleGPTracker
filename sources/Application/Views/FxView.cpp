#include "FxView.h"
#include "Application/Controllers/Shortcuts.h"
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
    const Shortcuts &shortcuts = Shortcuts::Get();
    if (shortcuts.Matches(Shortcuts::FxMixer, mask, pressed)) {
        ViewType type = VT_MIXER;
        ViewEvent event(VET_SWITCH_VIEW, &type);
        SetChanged();
        NotifyObservers(&event);
        return;
    }
    if (shortcuts.Matches(Shortcuts::FxPlayFromStart, mask, pressed)) {
        Player::GetInstance()->OnStartButton(PM_SONG, viewData_->songX_, true,
                                             viewData_->songX_);
        return;
    }
    if (shortcuts.Matches(Shortcuts::FxReset, mask, pressed)) {
        if (GetFocus()) {
            static_cast<UIIntVarField *>(GetFocus())->GetVariable().Reset();
            isDirty_ = true;
        }
        return;
    }
    if (shortcuts.Matches(Shortcuts::FxPlay, mask, pressed)) {
        Player::GetInstance()->OnStartButton(PM_SONG, viewData_->songX_, false,
                                             viewData_->songX_);
        return;
    }

    // Reuse field operations and their order/clamping; only resolve the
    // configurable actions here, without changing other FieldView clients.
    unsigned short adjustment = 0;
    if (shortcuts.Matches(Shortcuts::FxDecreaseCoarse, mask, pressed))
        adjustment |= EPBM_DOWN;
    if (shortcuts.Matches(Shortcuts::FxIncreaseCoarse, mask, pressed))
        adjustment |= EPBM_UP;
    if (shortcuts.Matches(Shortcuts::FxDecreaseFine, mask, pressed))
        adjustment |= EPBM_LEFT;
    if (shortcuts.Matches(Shortcuts::FxIncreaseFine, mask, pressed))
        adjustment |= EPBM_RIGHT;
    if (adjustment) {
        FieldView::ProcessButtonMask(EPBM_A | adjustment);
        return;
    }
    unsigned short navigation = 0;
    if (shortcuts.Matches(Shortcuts::FxDown, mask, pressed))
        navigation |= EPBM_DOWN;
    if (shortcuts.Matches(Shortcuts::FxUp, mask, pressed))
        navigation |= EPBM_UP;
    if (shortcuts.Matches(Shortcuts::FxRight, mask, pressed))
        navigation |= EPBM_RIGHT;
    if (shortcuts.Matches(Shortcuts::FxLeft, mask, pressed))
        navigation |= EPBM_LEFT;
    if (navigation)
        FieldView::ProcessButtonMask(navigation);
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
