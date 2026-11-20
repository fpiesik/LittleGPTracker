#include "AudioFileStreamer.h"
#include "Application/Model/Config.h"
#include "Application/Utils/fixed.h"
#include "Services/Audio/Audio.h"
#include "System/Console/Trace.h"
#include <algorithm>
#include <cmath>

AudioFileStreamer::AudioFileStreamer() {
    wav_ = 0;
    shift_ = 1;
    mode_ = AFSM_STOPPED;
    newPath_ = false;
};

AudioFileStreamer::~AudioFileStreamer() { SAFE_DELETE(wav_); };

bool AudioFileStreamer::Start(const Path &path) {
    Trace::Debug("Starting to stream %s", path.GetPath().c_str());
    path_ = path;
    const char *shift = Config::GetInstance()->GetValue("PRELISTENATTENUATION");
    shift_ = (shift) ? atoi(shift) : 1;
    Trace::Debug("Streaming shift is %d", shift_);
    newPath_ = true;
    mode_ = AFSM_PLAYING;
    return true;
};

void AudioFileStreamer::Stop() {
    mode_ = AFSM_STOPPED;
    SAFE_DELETE(wav_);
    Trace::Debug("Streaming stopped");
};

double AudioFileStreamer::GetProgress() const {
    if (mode_ == AFSM_STOPPED)
        return -1;
    if (newPath_ || !wav_)
        return 0;
    return position_ / wav_->GetSize(-1);
}

bool AudioFileStreamer::Render(fixed *buffer, int samplecount) {

    // See if we're playing

    if (samplecount <= 0)
        return false;
    if (mode_ == AFSM_STOPPED) {
        SAFE_DELETE(wav_);
        return false;
    }

    // Do we need to get a new file ?

    if (newPath_) {
        SAFE_DELETE(wav_);
        newPath_ = false;
    }

    // new look if we need to load the file

    if (!wav_) {
        wav_ = WavFile::Open(path_.GetPath().c_str());
        if (!wav_) {
            Trace::Error("Failed to open streaming of %s",
                         path_.GetPath().c_str());
            mode_ = AFSM_STOPPED;
            return false;
        }
        position_ = 0;
    }

    // we are playing a valid file

    int size = wav_->GetSize(-1);
    int outputRate = Audio::GetInstance()->GetSampleRate();
    memset(buffer, 0, 2 * samplecount * sizeof(fixed));
    if (outputRate <= 0 || position_ >= size) {
        mode_ = AFSM_STOPPED;
        return false;
    }
    double step = double(wav_->GetSampleRate(-1)) / outputRate;
    int count =
        int(std::min(double(samplecount), ceil((size - position_) / step)));
    int first = int(position_);
    int last = int(std::min(double(size - 1), position_ + (count - 1) * step));
    last = std::min(size - 1, last + 1);
    if (!wav_->GetBuffer(first, last - first + 1)) {
        mode_ = AFSM_STOPPED;
        return false;
    }
    const short *src = static_cast<short *>(wav_->GetSampleBuffer(-1));
    int channels = wav_->GetChannelCount(-1);
    int attenuation = std::max(0, std::min(14, shift_));
    for (int i = 0; i < count; ++i) {
        int frame = int(position_);
        double fraction = position_ - frame;
        int next = std::min(frame + 1, size - 1);
        for (int c = 0; c < 2; ++c) {
            int channel = channels == 2 ? c : 0;
            double value =
                src[(frame - first) * channels + channel] * (1 - fraction) +
                src[(next - first) * channels + channel] * fraction;
            buffer[i * 2 + c] = fl2fp(value / (1 << (1 + attenuation)));
        }
        position_ += step;
    }
    if (position_ >= size)
        mode_ = AFSM_STOPPED;
    return true;
}
