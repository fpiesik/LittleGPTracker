#include "PlaitsEngine.h"
#include <cstring>
#include <new>

namespace {
int Clamp(int value, int maximum) {
    return value < 0 ? 0 : (value > maximum ? maximum : value);
}
float Unit(int value) { return Clamp(value, 255) / 255.0f; }
} // namespace

PlaitsEngine::PlaitsEngine() : voice_(0), playing_(false) {}

PlaitsEngine::~PlaitsEngine() {
    if (voice_)
        voice_->~Voice();
}

void PlaitsEngine::Init(int sampleRate) {
    if (voice_)
        voice_->~Voice();
    std::memset(&storage_, 0, sizeof(storage_));
    std::memset(arena_, 0, sizeof(arena_));
    voice_ = new (storage_.bytes) plaits::Voice;
    stmlib::BufferAllocator allocator(arena_, sizeof(arena_));
    voice_->Init(&allocator);
    std::memset(&patch_, 0, sizeof(patch_));
    std::memset(&modulations_, 0, sizeof(modulations_));
    modulations_.trigger_patched = true;
    patch_.lpg_colour = 0.5f;
    step_ = 48000.0 / (sampleRate > 0 ? sampleRate : 44100);
    volume_ = 0.5f;
    NoteOff();
}

void PlaitsEngine::SetPitch(float note) {
    // Upstream compensates for the hardware's 47872.34 Hz clock. Undo that
    // compensation when rendering at exactly 48000 Hz (about -4.61 cents).
    patch_.note = note - 0.046105f;
}

void PlaitsEngine::SetParameters(int model, int harmonics, int timbre,
                                 int morph, int decay) {
    patch_.engine = Clamp(model, plaits::kMaxEngines - 1);
    patch_.harmonics = Unit(harmonics);
    patch_.timbre = Unit(timbre);
    patch_.morph = Unit(morph);
    patch_.decay = Unit(decay);
}

void PlaitsEngine::SetVolume(int volume) { volume_ = Unit(volume) * 0.5f; }

void PlaitsEngine::NoteOn(unsigned char note) {
    if (!voice_)
        return;
    SetPitch(note);
    playing_ = true;
    primed_ = false;
    phase_ = 0.0;
    position_ = plaits::kBlockSize;
    // A low block followed by a high block guarantees a fresh trigger even
    // when tracker notes arrive before the previous envelope has decayed.
    triggerPhase_ = 2;
}

void PlaitsEngine::NoteOff() { playing_ = false; }

float PlaitsEngine::NextSample() {
    if (position_ == plaits::kBlockSize) {
        modulations_.trigger = triggerPhase_ == 1 ? 1.0f : 0.0f;
        if (triggerPhase_)
            --triggerPhase_;
        voice_->Render(patch_, modulations_, block_, plaits::kBlockSize);
        position_ = 0;
    }
    return block_[position_++].out;
}

bool PlaitsEngine::Render(fixed *buffer, int frames) {
    if (!voice_ || !playing_ || frames <= 0)
        return false;
    if (!primed_) {
        left_ = NextSample();
        right_ = NextSample();
        primed_ = true;
    }
    for (int i = 0; i < frames; ++i) {
        // Streaming linear conversion preserves phase across arbitrary tracker
        // render slices. OUT is duplicated to stereo; AUX is not a right
        // channel.
        float sample = left_ + (right_ - left_) * static_cast<float>(phase_);
        fixed pcm = fl2fp(sample * volume_);
        buffer[2 * i] = buffer[2 * i + 1] = pcm;
        phase_ += step_;
        while (phase_ >= 1.0) {
            phase_ -= 1.0;
            left_ = right_;
            right_ = NextSample();
        }
    }
    return true;
}
