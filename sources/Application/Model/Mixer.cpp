
#include "Mixer.h"
#include "Application/Utils/HexBuffers.h"
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {
// The new send buffer has a fixed size; never trust lengths in project XML.
void RestoreSend(TiXmlElement *element, unsigned char *send) {
    unsigned char restored[SONG_CHANNEL_COUNT] = {0};
    int offset = 0;
    for (TiXmlElement *data = element->FirstChildElement(); data;
         data = data->NextSiblingElement()) {
        int value = 0, length = 0;
        if (data->QueryIntAttribute("VALUE", &value) == TIXML_SUCCESS) {
            if (data->QueryIntAttribute("LENGTH", &length) != TIXML_SUCCESS ||
                length < 0 || length > SONG_CHANNEL_COUNT - offset ||
                value < 0 || value > 255)
                return;
            for (int i = 0; i < length; ++i)
                restored[offset++] = value;
        } else {
            TiXmlNode *node = data->FirstChild();
            const char *text =
                node && node->Type() == TiXmlNode::TEXT ? node->Value() : 0;
            if (!text || std::strlen(text) % 2 ||
                std::strlen(text) >
                    static_cast<unsigned>(2 * (SONG_CHANNEL_COUNT - offset)))
                return;
            while (*text) {
                if (!std::isxdigit(static_cast<unsigned char>(text[0])) ||
                    !std::isxdigit(static_cast<unsigned char>(text[1])))
                    return;
                char hex[3] = {text[0], text[1], 0};
                restored[offset++] =
                    static_cast<unsigned char>(std::strtol(hex, 0, 16));
                text += 2;
            }
        }
    }
    std::memcpy(send, restored, sizeof(restored));
}
} // namespace

Mixer::Mixer() : Persistent("MIXER") {
    const char *names[] = {"DECAY", "DAMP", "DIFF", "MOD", "LEVEL"};
    const int defaults[] = {160, 128, 224, 0, 96};
    for (int i = 0; i < FX_COUNT; ++i)
        reverb_[i] = new Variable(names[i], MAKE_FOURCC('R', 'V', 'B', '0') + i,
                                  defaults[i]);
    Clear();
};

Mixer::~Mixer() {
    for (int i = 0; i < FX_COUNT; ++i)
        delete reverb_[i];
};

void Mixer::Clear() {
    for (int i = 0; i < FX_COUNT; ++i)
        reverb_[i]->Reset();

    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        channelBus_[i] = i;
        channelVolume_[i] = 0xFF;
        channelHPF_[i] = 0; // 0=OFF, 1=20Hz, 2=90Hz
        channelLPF_[i] = 0; // 0=OFF, else frequency in Hz (20-20000)
        channelSend_[i] = 0;
    }
};

void Mixer::SaveContent(TiXmlNode *node) {
    saveHexBuffer(node, "BUS", (unsigned char *)channelBus_,
                  SONG_CHANNEL_COUNT);
    saveHexBuffer(node, "VOL", channelVolume_, SONG_CHANNEL_COUNT);
    saveHexBuffer(node, "HPF", channelHPF_, SONG_CHANNEL_COUNT);
    saveHexBuffer(node, "LPF", channelLPF_, SONG_CHANNEL_COUNT);
    saveHexBuffer(node, "SEND", channelSend_, SONG_CHANNEL_COUNT);
    TiXmlElement reverb("REVERB");
    for (int i = 0; i < FX_COUNT; ++i)
        reverb.SetAttribute(reverb_[i]->GetName(), reverb_[i]->GetInt());
    node->InsertEndChild(reverb);
};

void Mixer::RestoreContent(TiXmlElement *element) {
    Clear(); // Missing fields in older projects must not inherit the last song.
    TiXmlElement *current = element->FirstChildElement();
    while (current) {
        const char *value = current->Value();
        if (!strcmp("BUS", value)) {
            restoreHexBuffer(current, (unsigned char *)channelBus_);
        } else if (!strcmp("VOL", value)) {
            restoreHexBuffer(current, channelVolume_);
        } else if (!strcmp("HPF", value)) {
            restoreHexBuffer(current, channelHPF_);
        } else if (!strcmp("LPF", value)) {
            restoreHexBuffer(current, (unsigned char *)channelLPF_);
        } else if (!strcmp("SEND", value)) {
            RestoreSend(current, channelSend_);
        } else if (!strcmp("REVERB", value)) {
            for (int i = 0; i < FX_COUNT; ++i) {
                int v;
                if (current->QueryIntAttribute(reverb_[i]->GetName(), &v) ==
                    TIXML_SUCCESS)
                    reverb_[i]->SetInt(v < 0 ? 0 : (v > 255 ? 255 : v));
            }
        }
        current = current->NextSiblingElement();
    }
}
