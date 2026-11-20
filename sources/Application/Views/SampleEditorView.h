#ifndef SAMPLE_EDITOR_VIEW_H
#define SAMPLE_EDITOR_VIEW_H
#include "Application/Instruments/SampleEditBuffer.h"
#include "BaseClasses/View.h"
#include "SampleEditorAction.h"
#include <string>

class SampleEditorView : public View {
  public:
    SampleEditorView(GUIWindow &window, ViewData *data);
    virtual ~SampleEditorView();
    virtual void DrawView();
    virtual void OnFocus();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int) {}
    virtual void AnimationUpdate();
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);

  private:
    static void ConfirmOverwrite(View &, ModalView &);
    static void ConfirmExit(View &, ModalView &);
    void closeEditor();
    void save(bool overwrite);
    void stopPreview();
    void preview();
    void text(int x, int y, const char *s, bool selected = false);

    struct WaveformColumn {
        int low, high;
    };
    std::vector<WaveformColumn> waveform_;
    SampleEditorAction action_;
    SampleEditBuffer buffer_, undoBuffer_;
    int undoFirst_, undoLast_;
    bool undoAvailable_, undoDirty_, undoZoom_;
    int first_, last_, row_, digit_, operation_, nameCursor_, sampleIndex_;
    bool dirty_, zoom_;
    int displayedBpm_, beatDigit_;
    double previewProgress_;
    std::string original_, name_, status_, previewPath_;
};
#endif
