#include "PlaitsInstrument.h"
#include "CommandList.h"
#include "PlaitsEngine.h"

PlaitsEngine PlaitsInstrument::voices_[SONG_CHANNEL_COUNT];
PlaitsInstrument *PlaitsInstrument::owners_[SONG_CHANNEL_COUNT] = {0};

PlaitsInstrument::PlaitsInstrument() {
    const char *names[] = {"MODEL", "HARMONICS", "TIMBRE", "MORPH", "DECAY"};
    const FourCC ids[] = {
        MAKE_FOURCC('P', 'M', 'O', 'D'), MAKE_FOURCC('P', 'H', 'A', 'R'),
        MAKE_FOURCC('P', 'T', 'I', 'M'), MAKE_FOURCC('P', 'M', 'O', 'R'),
        MAKE_FOURCC('P', 'D', 'E', 'C')};
    for (int i = 0; i < PARAMETER_COUNT; ++i) {
        parameters_[i] = new Variable(names[i], ids[i], i == MODEL ? 8 : 128);
        Insert(parameters_[i]);
    }
}

PlaitsInstrument::~PlaitsInstrument() {
    for (int i = 0; i < SONG_CHANNEL_COUNT; ++i)
        Stop(i);
}

bool PlaitsInstrument::IsInitialized() { return voices_[0].IsInitialized(); }

void PlaitsInstrument::InitVoices(int sampleRate) {
    for (int i = 0; i < SONG_CHANNEL_COUNT; ++i) {
        voices_[i].Init(sampleRate);
        owners_[i] = 0;
    }
}

bool PlaitsInstrument::Start(int channel, unsigned char note, int flags) {
    if (channel < 0 || channel >= SONG_CHANNEL_COUNT || note > 127 ||
        !voices_[channel].IsInitialized())
        return false;
    owners_[channel] = this;
    voices_[channel].SetVolume(255);
    voices_[channel].NoteOn(note);
    return true;
}

void PlaitsInstrument::Stop(int channel) {
    if (channel >= 0 && channel < SONG_CHANNEL_COUNT &&
        owners_[channel] == this) {
        voices_[channel].NoteOff();
        owners_[channel] = 0;
    }
}

bool PlaitsInstrument::Render(int channel, fixed *buffer, int size, int flags) {
    if (channel < 0 || channel >= SONG_CHANNEL_COUNT ||
        owners_[channel] != this)
        return false;
    voices_[channel].SetParameters(
        parameters_[MODEL]->GetInt(), parameters_[HARMONICS]->GetInt(),
        parameters_[TIMBRE]->GetInt(), parameters_[MORPH]->GetInt(),
        parameters_[DECAY]->GetInt());
    // Muting is applied by PlayerChannel, as with samples; keep time advancing.
    return voices_[channel].Render(buffer, size);
}

void PlaitsInstrument::ProcessCommand(int channel, FourCC cc, ushort value) {
    if (channel < 0 || channel >= SONG_CHANNEL_COUNT ||
        owners_[channel] != this)
        return;
    if (cc == I_CMD_VOLM)
        voices_[channel].SetVolume(value & 0xff);
}

void PlaitsInstrument::Purge() {
    for (int i = 0; i < SONG_CHANNEL_COUNT; ++i)
        Stop(i);
    for (int i = 0; i < PARAMETER_COUNT; ++i)
        parameters_[i]->Reset();
}
