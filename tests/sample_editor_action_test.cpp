#include "Application/Views/ModalDialogs/SampleEditorAction.h"
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
