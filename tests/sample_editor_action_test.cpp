#include "Application/Views/SampleEditorAction.h"
#include <cassert>
#include <cstdio>

int main() {
    SampleEditorAction action;
    uint32_t now = 0;
    // Opening the editor while A is held must not execute an action.
    assert(!action.Update(true, true, true, now++));
    assert(!action.Update(false, true, false, now++));
    // A press and autorepeat only arm; release executes exactly once.
    assert(!action.Update(true, true, true, now++));
    assert(!action.Update(true, true, true, now++));
    assert(action.Update(false, true, false, now++));
    assert(!action.Update(false, true, false, now++));
    // A+arrow, arrow released first, then A: no click, even after repeats.
    assert(!action.Update(true, true, true, now++));
    assert(!action.Update(true, false, true, now++));
    assert(!action.Update(true, true, false, now++));
    assert(!action.Update(true, true, true, now++));
    assert(!action.Update(false, true, false, now++));
    // A released first while the arrow is still down: also no click.
    assert(!action.Update(true, true, true, now++));
    assert(!action.Update(true, false, true, now++));
    assert(!action.Update(false, false, false, now++));
    assert(!action.Update(false, true, false, now++));
    // Pressing A while another button is held never arms a click.
    assert(!action.Update(true, false, true, now++));
    assert(!action.Update(true, true, false, now++));
    assert(!action.Update(false, true, false, now++));
    // A new, separate tap works again.
    assert(!action.Update(true, true, true, now++));
    assert(action.Update(false, true, false, now++));
    // Boundary: at the limit is a tap, one millisecond longer is not.
    assert(!action.Update(true, true, true, 1000));
    assert(action.Update(false, true, false, 1250));
    assert(!action.Update(true, true, true, 2000));
    assert(!action.Update(false, true, false, 2251));
    // Autorepeat does not restart the timer.
    assert(!action.Update(true, true, true, 3000));
    assert(!action.Update(true, true, true, 3400));
    assert(!action.Update(false, true, false, 3450));
    // Tick counter wrap is still a short press.
    assert(!action.Update(true, true, true, 0xfffffff0U));
    assert(action.Update(false, true, false, 0x20U));
    // A remapped two-button tap executes only on the final release.
    SampleEditorAction chord;
    assert(!chord.UpdateChord(false, true, 0, false, false, 0));
    assert(!chord.UpdateChord(false, true, 1, true, false, 10));
    assert(!chord.UpdateChord(true, true, 3, true, false, 20));
    assert(!chord.UpdateChord(false, true, 2, false, false, 30));
    assert(chord.UpdateChord(false, true, 0, false, false, 40));
    assert(!chord.UpdateChord(false, true, 0, false, false, 50));
    // Holding past the limit or using the chord to edit disarms execution.
    assert(!chord.UpdateChord(true, true, 3, true, false, 100));
    assert(!chord.UpdateChord(false, true, 1, false, false, 150));
    assert(!chord.UpdateChord(false, true, 0, false, false, 351));
    assert(!chord.UpdateChord(true, true, 3, true, true, 400));
    assert(!chord.UpdateChord(false, true, 0, false, false, 410));
    assert(!chord.UpdateChord(true, true, 3, true, false, 420));
    assert(!chord.UpdateChord(true, false, 7, true, true, 430));
    assert(!chord.UpdateChord(true, true, 3, false, false, 440));
    assert(!chord.UpdateChord(false, true, 0, false, false, 450));
    const char *settings[] = {"100",
                              "500",
                              "1",
                              "2000",
                              "0",
                              "-1",
                              "2001",
                              "abc",
                              "100ms",
                              "",
                              "999999999999999999999999"};
    const uint32_t limits[] = {100, 500, 1,   2000, 250, 250,
                               250, 250, 250, 250,  250};
    for (unsigned int i = 0; i < sizeof(limits) / sizeof(limits[0]); ++i) {
        SampleEditorAction configured(settings[i]);
        assert(!configured.Update(false, true, false, 0));
        assert(!configured.Update(true, true, true, 100));
        assert(configured.Update(false, true, false, 100 + limits[i]));
        assert(!configured.Update(true, true, true, 3000));
        assert(!configured.Update(false, true, false, 3001 + limits[i]));
    }
    puts("Sample editor A-release and timing tests passed");
}
