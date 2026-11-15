#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Application/Model/Mixer.h"
#include "Application/Model/Song.h"
#include "Application/Utils/HexBuffers.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
    System::Install(new LINUXSystem);
    assert(SONG_CHANNEL_COUNT == 12);
    Song song;
    unsigned char legacy[256 * 8];
    for (int row = 0; row < 256; ++row)
        for (int ch = 0; ch < 8; ++ch)
            legacy[row * 8 + ch] = (row + ch) % 128;
    TiXmlElement old("ROOT");
    saveHexBuffer(&old, "SONG", legacy, sizeof(legacy));
    song.RestoreContent(&old);
    for (int row = 0; row < 256; ++row)
        for (int ch = 0; ch < 12; ++ch)
            assert(song.data_[row * 12 + ch] ==
                   (ch < 8 ? legacy[row * 8 + ch] : 255));

    for (int row = 0; row < 256; ++row)
        for (int ch = 8; ch < 12; ++ch)
            song.data_[row * 12 + ch] = (row + ch) % 128;
    for (int i = 0; i < PHRASE_COUNT * 16; ++i) {
        assert(song.phrase_->velocity_[i] == 127);
        song.phrase_->velocity_[i] = i % 128;
    }
    TiXmlElement saved("ROOT");
    song.SaveContent(&saved);
    unsigned char projection[sizeof(legacy)];
    assert(restoreHexBuffer(saved.FirstChildElement("SONG"), projection,
                            sizeof(projection)) == sizeof(projection));
    assert(!memcmp(legacy, projection, sizeof(legacy)));
    Song restored;
    restored.RestoreContent(&saved);
    assert(!memcmp(song.data_, restored.data_, 256 * 12));

    assert(!memcmp(song.phrase_->velocity_, restored.phrase_->velocity_,
                   PHRASE_COUNT * 16));
    saved.RemoveChild(saved.FirstChildElement("VELOCITY"));
    restored.RestoreContent(&saved);
    for (int i = 0; i < PHRASE_COUNT * 16; ++i)
        assert(restored.phrase_->velocity_[i] == 127);

    Mixer mixer;
    for (int ch = 0; ch < 12; ++ch) {
        mixer.SetChannelVolume(ch, 100 + ch);
        mixer.SetChannelHPF(ch, ch % 3);
        mixer.SetChannelLPF(ch, 1000 + ch * 100);
        mixer.SetChannelSend(ch, ch * 10);
    }
    TiXmlElement mix("ROOT");
    mixer.SaveContent(&mix);
    mixer.Clear();
    mixer.RestoreContent(&mix);
    for (int ch = 0; ch < 12; ++ch) {
        assert(mixer.GetBus(ch) == ch);
        assert(mixer.GetChannelVolume(ch) == 100 + ch);
        assert(mixer.GetChannelHPF(ch) == ch % 3);
        assert(mixer.GetChannelLPF(ch) == 1000 + ch * 100);
        assert(mixer.GetChannelSend(ch) == ch * 10);
    }
    const char *extensions[] = {"BUS12", "VOL12", "HPF12", "LPF12", "SEND12"};
    for (int i = 0; i < 5; ++i)
        mix.RemoveChild(mix.FirstChildElement(extensions[i]));
    mixer.RestoreContent(&mix);
    for (int ch = 8; ch < 12; ++ch) {
        assert(mixer.GetBus(ch) == ch);
        assert(mixer.GetChannelVolume(ch) == 255);
        assert(mixer.GetChannelHPF(ch) == 0);
        assert(mixer.GetChannelLPF(ch) == 0);
        assert(mixer.GetChannelSend(ch) == 0);
    }
    TiXmlDocument bad;
    bad.Parse("<SONG><DATA VALUE='0' LENGTH='999999'/></SONG>");
    memset(projection, 42, sizeof(projection));
    assert(restoreHexBuffer(bad.RootElement(), projection,
                            sizeof(projection)) == -1);
    for (unsigned i = 0; i < sizeof(projection); ++i)
        assert(projection[i] == 42);
    puts("12-track persistence and legacy compatibility passed");
}
