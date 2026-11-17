#ifndef SAMPLE_EDITOR_ACTION_H
#define SAMPLE_EDITOR_ACTION_H

#include <cerrno>
#include <cstdlib>
#include <stdint.h>

// Consume the opening A press. A chord or repeat must never arm another click.
class SampleEditorAction {
  public:
    explicit SampleEditorAction(const char *tapMs = 0)
        : held_(true), armed_(false), started_(0), limit_(250) {
        if (tapMs) {
            char *end = 0;
            errno = 0;
            long value = std::strtol(tapMs, &end, 10);
            if (!errno && end != tapMs && *end == 0 && value >= 1 &&
                value <= 2000)
                limit_ = static_cast<uint32_t>(value);
        }
    }
    // A multi-button tap stays armed through partial releases until all keys
    // are up.
    bool UpdateChord(bool complete, bool onlyTapKeys, unsigned short held,
                     bool pressed, bool editing, uint32_t now) {
        const bool releasing = held_ && held && onlyTapKeys && !pressed;
        return Update(complete || releasing, onlyTapKeys && !editing, pressed,
                      now);
    }
    bool Update(bool aDown, bool aOnly, bool pressed, uint32_t now) {
        if (aDown) {
            if (!held_) {
                armed_ = pressed && aOnly;
                started_ = now;
            }
            if (!aOnly)
                armed_ = false;
            held_ = true;
            return false;
        }
        // Unsigned subtraction also handles SDL's tick counter wrapping.
        bool click = held_ && armed_ && !pressed && aOnly &&
                     static_cast<uint32_t>(now - started_) <= limit_;
        held_ = armed_ = false;
        return click;
    }

  private:
    bool held_, armed_;
    uint32_t started_, limit_;
};
#endif
