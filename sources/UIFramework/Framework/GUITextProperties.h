
#ifndef _GUI_TEXT_PROPERTIES_H_
#define _GUI_TEXT_PROPERTIES_H_

struct GUITextProperties {
    enum IndicatorEdge {
        INDICATOR_EDGE_NONE,
        INDICATOR_EDGE_LEFT,
        INDICATOR_EDGE_RIGHT,
        INDICATOR_EDGE_RIGHT_SHIFT_LEFT
    };

    GUITextProperties()
        : invert_(false), indicatorEdge_(INDICATOR_EDGE_NONE),
          extendBackgroundTop1_(false) {}
    bool invert_;
    IndicatorEdge indicatorEdge_;
    bool extendBackgroundTop1_;
};

#endif
