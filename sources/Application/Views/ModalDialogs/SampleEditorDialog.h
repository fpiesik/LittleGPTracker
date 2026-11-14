#ifndef SAMPLE_EDITOR_DIALOG_H
#define SAMPLE_EDITOR_DIALOG_H
#include "Application/Instruments/SampleEditBuffer.h"
#include "Application/Views/BaseClasses/ModalView.h"
#include "SampleEditorAction.h"
#include <string>

class SampleEditorDialog : public ModalView {
  public:
    explicit SampleEditorDialog(View &parent);
    virtual ~SampleEditorDialog();
    virtual void DrawView();
    virtual void OnFocus() {}
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int) {}
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);

  private:
    static void ConfirmOverwrite(View &, ModalView &);
    static void ConfirmExit(View &, ModalView &);
    void save(bool overwrite);
    void stopPreview();
    void preview();
    void text(int x, int y, const char *s, bool selected = false);
    SampleEditorAction action_;
    SampleEditBuffer buffer_, undoBuffer_;
    int undoFirst_, undoLast_;
    bool undoAvailable_, undoDirty_, undoZoom_;
    int first_, last_, row_, digit_, operation_, nameCursor_, sampleIndex_;
    bool dirty_, playing_, zoom_;
    std::string original_, name_, status_, previewPath_;
};
#endif
