#include "SettingsView.h"
#include "Application/AppWindow.h"
#include "Application/Controllers/Shortcuts.h"
#include "UIFramework/BasicDatas/FontConfig.h"

void SettingsView::DrawView() {
    Clear();
    SetColor(CD_NORMAL);
    GUITextProperties props;
    GUIPoint title = GetTitlePosition();
    DrawString(title._x, title._y, "Settings", props);
    GUIPoint anchor = GetAnchor();
    props.invert_ = true;
    DrawString(anchor._x, anchor._y, CurrentFontLabel(), props);
    drawMap();
    EnableNotification();
}

void SettingsView::ProcessButtonMask(unsigned short mask, bool pressed) {
    const Shortcuts &keys = Shortcuts::Get();
    if (keys.Matches(Shortcuts::RecordOptionsBack, mask, pressed)) {
        ViewType type = VT_PROJECT;
        ViewEvent event(VET_SWITCH_VIEW, &type);
        SetChanged();
        NotifyObservers(&event);
    } else if (keys.Matches(Shortcuts::RecordOptionsLeft, mask, pressed)) {
        static_cast<AppWindow &>(w_).ChangeFont(-1);
        isDirty_ = true;
    } else if (keys.Matches(Shortcuts::RecordOptionsRight, mask, pressed)) {
        static_cast<AppWindow &>(w_).ChangeFont(1);
        isDirty_ = true;
    }
}
