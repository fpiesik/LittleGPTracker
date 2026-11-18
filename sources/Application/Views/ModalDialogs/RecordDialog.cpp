#include "RecordDialog.h"
#if defined(PLATFORM_X64)
#include "Adapters/SDL2/Audio/SDLAudioDriver.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Model/Config.h"
#include "Application/Views/ViewData.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

namespace {
bool writeAll(int fd, const unsigned char *data, size_t size) {
    while (size) {
        ssize_t n = write(fd, data, size);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        data += n;
        size -= n;
    }
    return true;
}

void startRecordedSong() {
    Player::GetInstance()->OnSongStartButton(0, SONG_CHANNEL_COUNT - 1, false,
                                             false);
}

const Uint32 sampleRate = 44100;
const Uint32 maxBytes = sampleRate * 2 * 600; // Ten minutes per channel.
void put32(unsigned char *p, Uint32 value) {
    for (int i = 0; i < 4; ++i)
        p[i] = (value >> (8 * i)) & 255;
}
} // namespace

RecordDialog::RecordDialog(View &parent)
    : ModalView(parent), state_(ERROR), device_(0), file_(-1), bytes_(0),
      channels_(SDLAudioDriver::RecordChannels), selected_(1) {
    row_ = viewData_->songY_ + viewData_->songOffset_;
    column_ = viewData_->songX_;
    previousMode_ = Player::GetInstance()->GetSequencerMode();
    beginTake();
}

RecordDialog::~RecordDialog() {
    discardTake();
    Player::GetInstance()->SetSequencerMode(previousMode_);
}

void RecordDialog::discardTake() {
    if (device_) {
        SDLAudioDriver::StopCapture();
        device_ = 0;
    }
    if (state_ == COUNT_IN || state_ == RECORDING)
        Player::GetInstance()->Stop();
    if (file_ >= 0) {
        close(file_);
        file_ = -1;
    }
    if (!path_.empty()) {
        unlink(path_.c_str());
        path_.clear();
    }
    bytes_ = 0;
}

void RecordDialog::fail(const char *message) {
    if (device_) {
        SDLAudioDriver::StopCapture();
        device_ = 0;
    }
    if (state_ == COUNT_IN || state_ == RECORDING)
        Player::GetInstance()->Stop();
    error_ = message;
    state_ = ERROR;
    isDirty_ = true;
}

bool RecordDialog::findResources(int &chain, int &phrase, int &instrument) {
    Song *song = viewData_->song_;
    for (chain = 0; chain < CHAIN_COUNT && song->chain_->IsUsed(chain);
         ++chain) {
    }
    for (phrase = 0; phrase < PHRASE_COUNT && song->phrase_->IsUsed(phrase);
         ++phrase) {
    }
    instrument = viewData_->project_->GetInstrumentBank()->GetNext();
    return chain < CHAIN_COUNT && phrase < PHRASE_COUNT &&
           instrument != NO_MORE_INSTRUMENT &&
           SamplePool::GetInstance()->GetNameListSize() < MAX_PIG_SAMPLES;
}

void RecordDialog::beginTake() {
    discardTake();
    state_ = ERROR;
    int chain, phrase, instrument;
    if (viewData_->song_->data_[row_ * SONG_CHANNEL_COUNT + column_] != 0xff) {
        fail("Choose an empty song slot");
        return;
    }
    if (!findResources(chain, phrase, instrument)) {
        fail("No free chain/phrase/instrument");
        return;
    }
    // A named temporary file survives capture without holding the take in RAM.
    Path folder("samples:.record-XXXXXX.wav");
    std::string pattern = folder.GetPath();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back(0);
    int fd = mkstemps(&name[0], 4);
    if (fd < 0) {
        fail("Cannot create recording file");
        return;
    }
    path_ = &name[0];
    file_ = fd;
    if (!writeHeader()) {
        fail("Cannot write recording file");
        return;
    }
    device_ = SDLAudioDriver::OpenCapture();
    if (!device_) {
        fail("Cannot open audio input");
        return;
    }
    Player::GetInstance()->Stop();
    viewData_->chainRow_ = 0;
    Player::GetInstance()->SetSequencerMode(SM_SONG);
    double latencyMs = -1; // Automatic estimate unless explicitly calibrated.
    const char *value = Config::GetInstance()->GetValue("RECORD_LATENCY_MS");
    if (value) {
        char *end;
        double parsed = strtod(value, &end);
        if (end != value && !*end && parsed >= 0 && parsed <= 2000)
            latencyMs = parsed;
    }
    state_ = SDLAudioDriver::RecordCountIn ? COUNT_IN : RECORDING;
    SDLAudioDriver::BeginRecording(viewData_->project_->GetTempo(), latencyMs,
                                   startRecordedSong);
    selected_ = 1;
    isDirty_ = true;
}

bool RecordDialog::writeHeader() {
    unsigned char header[44] = {};
    memcpy(header, "RIFF", 4);
    put32(header + 4, bytes_ + 36);
    memcpy(header + 8, "WAVEfmt ", 8);
    put32(header + 16, 16);
    header[20] = 1; // PCM
    header[22] = channels_;
    put32(header + 24, sampleRate);
    put32(header + 28, sampleRate * 2 * channels_);
    header[32] = 2 * channels_;
    header[34] = 16;
    memcpy(header + 36, "data", 4);
    put32(header + 40, bytes_);
    return lseek(file_, 0, SEEK_SET) == 0 &&
           writeAll(file_, header, sizeof(header)) &&
           lseek(file_, 0, SEEK_END) >= 0;
}

bool RecordDialog::drain() {
    unsigned char buffer[8192];
    // Bound each UI update even while new input arrives.
    Uint32 remaining = sampleRate * 2 * channels_ * 4;
    while (remaining && bytes_ < maxBytes * channels_) {
        Uint32 size = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        if (size > maxBytes * channels_ - bytes_)
            size = maxBytes * channels_ - bytes_;
        int read = SDLAudioDriver::ReadCapture(buffer, size);
        if (read < 0) {
            fail("Input lost or capture overflow");
            return false;
        }
        size = read;
        if (!size)
            break;
        if (!writeAll(file_, buffer, size)) {
            fail("Recording write failed");
            return false;
        }
        bytes_ += size;
        remaining -= size;
    }
    return true;
}

void RecordDialog::stopTake() {
    if (SDLAudioDriver::RecordingBeat() == -2) {
        fail("Output underrun: retry take");
        return;
    }
    SDLAudioDriver::StopCapture();
    if (!drain())
        return;
    device_ = 0;
    Player::GetInstance()->Stop();
    if (!writeHeader() || fsync(file_) != 0) {
        fail("Recording write failed");
        return;
    }
    state_ = DECISION;
    selected_ = 1; // Default to abort, never accidentally save on release.
    isDirty_ = true;
}

void RecordDialog::AnimationUpdate() {
    if (state_ == COUNT_IN || state_ == RECORDING) {
        int beat = SDLAudioDriver::RecordingBeat();
        if (beat == -2)
            fail("Output underrun: retry take");
        else if (beat >= SDLAudioDriver::RecordCountIn)
            state_ = RECORDING;
    }
    // Also drain during count-in: a calibrated input boundary can precede the
    // next UI update. No disk operations run on either audio thread.
    if (state_ == COUNT_IN || state_ == RECORDING) {
        if (SDL_GetAudioDeviceStatus(device_) == SDL_AUDIO_STOPPED)
            fail("Audio input disconnected");
        else if (drain() && bytes_ >= maxBytes * channels_)
            stopTake();
    }
    DrawView();
}

bool RecordDialog::saveTake() {
    int chain, phrase, instrument;
    if (!bytes_) {
        error_ = "Recording is empty";
        return false;
    }
    if (!findResources(chain, phrase, instrument)) {
        error_ = "No free chain/phrase/instrument";
        return false;
    }
    // Publish a complete WAV with a unique name; never overwrite a sample.
    std::string target = path_;
    size_t slash = target.find_last_of('/');
    target.erase(slash == std::string::npos ? 0 : slash + 1, 1);
    if (link(path_.c_str(), target.c_str()) != 0) {
        error_ = "Cannot save recording";
        return false;
    }
    Path saved(target.c_str());
    int sample = SamplePool::GetInstance()->RegisterRecordedSample(saved);
    if (sample < 0) {
        unlink(target.c_str());
        error_ = "Cannot load recorded sample";
        return false;
    }
    Song *song = viewData_->song_;
    SampleInstrument *inst = static_cast<SampleInstrument *>(
        viewData_->project_->GetInstrumentBank()->GetInstrument(instrument));
    inst->Purge();
    inst->AssignSample(sample);
    for (int i = 0; i < 16; ++i) {
        int c = chain * 16 + i, p = phrase * 16 + i;
        song->chain_->data_[c] = 0xff;
        song->chain_->transpose_[c] = 0;
        song->phrase_->note_[p] = song->phrase_->instr_[p] = 0xff;
        song->phrase_->cmd1_[p] = song->phrase_->cmd2_[p] =
            MAKE_FOURCC('-', '-', '-', '-');
        song->phrase_->param1_[p] = song->phrase_->param2_[p] = 0;
    }
    song->phrase_->note_[phrase * 16] = 60;
    song->phrase_->instr_[phrase * 16] = instrument;
    song->chain_->data_[chain * 16] = phrase;
    song->chain_->SetUsed(chain);
    song->phrase_->SetUsed(phrase);
    song->data_[row_ * SONG_CHANNEL_COUNT + column_] = chain;
    discardTake();
    return true;
}

void RecordDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed)
        return;
    const Shortcuts &keys = Shortcuts::Get();
    if (state_ == COUNT_IN) {
        if (keys.Matches(Shortcuts::RecordCancel, mask, pressed))
            EndModal(0);
    } else if (state_ == RECORDING) {
        if (keys.Matches(Shortcuts::RecordStop, mask, pressed))
            stopTake();
    } else if (state_ == ERROR) {
        if (keys.Matches(Shortcuts::RecordClose, mask, pressed) ||
            keys.Matches(Shortcuts::RecordBack, mask, pressed))
            EndModal(0);
    } else {
        if (keys.Matches(Shortcuts::RecordUp, mask, pressed))
            selected_ = (selected_ + 2) % 3;
        if (keys.Matches(Shortcuts::RecordDown, mask, pressed))
            selected_ = (selected_ + 1) % 3;
        if (keys.Matches(Shortcuts::RecordAccept, mask, pressed)) {
            if (selected_ == 0) {
                if (saveTake())
                    EndModal(1);
            } else if (selected_ == 1) {
                EndModal(0);
            } else {
                error_.clear();
                beginTake();
            }
        }
    }
    isDirty_ = true;
}

void RecordDialog::DrawView() {
    const Shortcuts &keys = Shortcuts::Get();
    SetWindow(32, 7);
    SetColor(CD_NORMAL);
    GUITextProperties props;
    if (state_ == COUNT_IN) {
        char message[32];
        int beat = SDLAudioDriver::RecordingBeat();
        if (beat < 0)
            beat = 0;
        snprintf(message, sizeof(message), "Count in: %d",
                 beat < SDLAudioDriver::RecordCountIn
                     ? SDLAudioDriver::RecordCountIn - beat
                     : 1);
        DrawString(0, 0, message, props);
        DrawString(
            0, 2, (keys.KeyNames(Shortcuts::RecordCancel) + ": cancel").c_str(),
            props);
    } else if (state_ == RECORDING) {
        char message[32];
        snprintf(message, sizeof(message), "Recording: %us",
                 bytes_ / (sampleRate * 2 * channels_));
        DrawString(0, 0, message, props);
        DrawString(
            0, 2,
            (keys.KeyNames(Shortcuts::RecordStop) + ": stop recording").c_str(),
            props);
    } else if (state_ == ERROR) {
        DrawString(0, 0, error_.c_str(), props);
        DrawString(0, 2,
                   (keys.KeyNames(Shortcuts::RecordClose) + "/" +
                    keys.KeyNames(Shortcuts::RecordBack) + ": close")
                       .c_str(),
                   props);
    } else {
        DrawString(0, 0, "Save Record?", props);
        const char *choices[] = {"Yes", "No, abort", "No, rec again"};
        for (int i = 0; i < 3; ++i) {
            props.invert_ = selected_ == i;
            DrawString(0, i + 2, choices[i], props);
        }
        props.invert_ = false;
        DrawString(0, 6, error_.c_str(), props);
    }
}
#endif
