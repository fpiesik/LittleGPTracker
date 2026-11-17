#include "Shortcuts.h"
#include "Externals/TinyXML/tinyxml.h"
#include "System/Console/Trace.h"

namespace {
struct DefaultBinding {
    const char *name;
    const char *required, *forbidden;
    bool pressed;
};
const DefaultBinding defaults[] = {
    {"groove.previous", "LEFT+B", "", true},
    {"groove.next", "RIGHT+B", "", true},
    {"groove.previous_16", "DOWN+B", "", true},
    {"groove.next_16", "UP+B", "", true},
    {"groove.clear", "B+A", "", true},
    {"groove.decrease", "LEFT+A", "B", true},
    {"groove.increase", "RIGHT+A", "B", true},
    {"groove.decrease_sync", "DOWN+A", "B", true},
    {"groove.increase_sync", "UP+A", "B", true},
    {"groove.insert", "A", "LEFT+DOWN+RIGHT+UP+L+B+R+START+SELECT", true},
    {"groove.phrase", "DOWN+R", "B+A", true},
    {"groove.play_from_start", "R+START", "B+A", true},
    {"groove.down", "DOWN", "B+A+R", true},
    {"groove.up", "UP", "B+A+R", true},
    {"groove.play", "START", "B+A+R", true},
    {"mixer.mute", "R+B", "", true},
    {"mixer.solo", "R+A", "B", true},
    {"mixer.unmute_all", "R+L", "B+A", true},
    {"mixer.previous_view", "R+UP", "B+A+L", true},
    {"mixer.fx", "R+DOWN", "B+A+L+UP", true},
    {"mixer.table", "R+RIGHT", "B+A+L+UP+DOWN", true},
    {"mixer.play_from_start", "R+START", "B+A+L+UP+DOWN+RIGHT", true},
    {"mixer.clear", "B+A", "R", true},
    {"mixer.edit", "A", "B+R", true},
    {"mixer.increase_coarse", "A+UP", "B+R", true},
    {"mixer.decrease_coarse", "A+DOWN", "B+R", true},
    {"mixer.increase_fine", "A+RIGHT", "B+R", true},
    {"mixer.decrease_fine", "A+LEFT", "B+R", true},
    {"mixer.up", "UP", "A+B+R", true},
    {"mixer.down", "DOWN", "A+B+R", true},
    {"mixer.left", "LEFT", "A+B+R", true},
    {"mixer.right", "RIGHT", "A+B+R", true},
    {"mixer.play", "START", "A+B+R", true},
    {"mixer.selection_play_from_start", "R+START", "A+B", true},
    {"mixer.selection_play", "START", "A+B+R", true},
    {"mixer.mute_release", "R", "", false},
    {"mixer.solo_release", "R", "", false},
    {"fx.mixer", "R+UP", "", true},
    {"fx.play_from_start", "R+START", "UP", true},
    {"fx.reset", "B+A", "R", true},
    {"fx.play", "START", "R+B", true},
    {"fx.decrease_coarse", "A+DOWN", "R+B+START", true},
    {"fx.increase_coarse", "A+UP", "R+B+START", true},
    {"fx.decrease_fine", "A+LEFT", "R+B+START", true},
    {"fx.increase_fine", "A+RIGHT", "R+B+START", true},
    {"fx.down", "DOWN", "A+B+L+R+SELECT+START", true},
    {"fx.up", "UP", "A+B+L+R+SELECT+START", true},
    {"fx.right", "RIGHT", "A+B+L+R+SELECT+START", true},
    {"fx.left", "LEFT", "A+B+L+R+SELECT+START", true},
    {"table.previous", "B+LEFT", "", true},
    {"table.next", "B+RIGHT", "", true},
    {"table.previous16", "B+DOWN", "", true},
    {"table.next16", "B+UP", "", true},
    {"table.cut", "B+A", "", true},
    {"table.select", "B+L", "", true},
    {"table.decrease_coarse", "A+DOWN", "B", true},
    {"table.increase_coarse", "A+UP", "B", true},
    {"table.decrease_fine", "A+LEFT", "B", true},
    {"table.increase_fine", "A+RIGHT", "B", true},
    {"table.insert", "A", "LEFT+DOWN+RIGHT+UP+L+B+R+START+SELECT", true},
    {"table.paste", "A+L", "B", true},
    {"table.parent", "R+UP", "A+B", true},
    {"table.view_left", "R+LEFT", "A+B", true},
    {"table.view_right", "R+RIGHT", "A+B", true},
    {"table.play_from_start", "R+START", "A+B", true},
    {"table.down", "DOWN", "A+B+R", true},
    {"table.up", "UP", "A+B+R", true},
    {"table.left", "LEFT", "A+B+R", true},
    {"table.right", "RIGHT", "A+B+R", true},
    {"table.play", "START", "A+B+R", true},
    {"table.selection_extend", "B+L", "", true},
    {"table.selection_interpolate", "B+R", "L", true},
    {"table.selection_copy", "B", "L+R", true},
    {"table.selection_cut", "A+L", "B", true},
    {"table.selection_parent", "R+UP", "A+B", true},
    {"table.selection_play_from_start", "R+START", "A+B", true},
    {"table.selection_down", "DOWN", "A+B+R+L", true},
    {"table.selection_up", "UP", "A+B+R+L", true},
    {"table.selection_left", "LEFT", "A+B+R+L", true},
    {"table.selection_right", "RIGHT", "A+B+R+L", true},
    {"table.selection_play", "START", "A+B+R+L", true},
    {"chain.previous_channel", "B+LEFT", "", true},
    {"chain.next_channel", "B+RIGHT", "", true},
    {"chain.previous_chain", "B+UP", "", true},
    {"chain.next_chain", "B+DOWN", "", true},
    {"chain.cut", "B+A", "", true},
    {"chain.begin_clone_selection", "B+L", "", true},
    {"chain.mute", "B+R", "", true},
    {"chain.decrease_coarse", "A+DOWN", "B", true},
    {"chain.increase_coarse", "A+UP", "B", true},
    {"chain.decrease_fine", "A+LEFT", "B", true},
    {"chain.increase_fine", "A+RIGHT", "B", true},
    {"chain.paste", "A+L", "B", true},
    {"chain.insert", "A", "LEFT+DOWN+RIGHT+UP+L+B+R+START+SELECT", true},
    {"chain.solo", "A+R", "B", true},
    {"chain.song", "R+LEFT", "A+B", true},
    {"chain.phrase", "R+RIGHT", "A+B", true},
    {"chain.mixer", "R+DOWN", "A+B", true},
    {"chain.play_from_start", "R+START", "A+B", true},
    {"chain.unmute_all", "R+L", "A+B", true},
    {"chain.down", "DOWN", "A+B+R+L", true},
    {"chain.up", "UP", "A+B+R+L", true},
    {"chain.left", "LEFT", "A+B+R+L", true},
    {"chain.right", "RIGHT", "A+B+R+L", true},
    {"chain.play", "START", "A+B+R+L", true},
    {"chain.selection_copy", "B", "LEFT+DOWN+RIGHT+UP+L+A+R+START+SELECT",
     true},
    {"chain.selection_mute", "B+R", "", true},
    {"chain.selection_extend", "B+L", "", true},
    {"chain.selection_decrease_coarse", "A+DOWN", "B", true},
    {"chain.selection_increase_coarse", "A+UP", "B", true},
    {"chain.selection_decrease_fine", "A+LEFT", "B", true},
    {"chain.selection_increase_fine", "A+RIGHT", "B", true},
    {"chain.selection_cut", "A+L", "B", true},
    {"chain.selection_solo", "A+R", "B", true},
    {"chain.selection_song", "R+LEFT", "A+B", true},
    {"chain.selection_phrase", "R+RIGHT", "A+B", true},
    {"chain.selection_mixer", "R+DOWN", "A+B", true},
    {"chain.selection_play_from_start", "R+START", "A+B", true},
    {"chain.selection_unmute_all", "R+L", "A+B", true},
    {"chain.selection_down", "DOWN", "A+B+R", true},
    {"chain.selection_up", "UP", "A+B+R", true},
    {"chain.selection_left", "LEFT", "A+B+R", true},
    {"chain.selection_right", "RIGHT", "A+B+R", true},
    {"chain.selection_play", "START", "A+B+R", true},
    {"chain.next_unused", "A", "LEFT+DOWN+RIGHT+UP+L+B+R+START+SELECT", true},
    {"chain.clone", "A+L", "", true},
    {"chain.edit_hold", "A", "", true},
    {"chain.mute_release", "R", "", false},
    {"chain.solo_release", "R", "", false},
    {"command_selector.up", "UP", "", true},
    {"command_selector.down", "DOWN", "", true},
    {"command_selector.left", "LEFT", "", true},
    {"command_selector.right", "RIGHT", "", true},
    {"command_selector.confirm", "A", "", true},
    {"command_selector.cancel", "B", "", true},
    {"project_picker.delete_prompt", "B+A", "", true},
    {"project_picker.page_up", "B+UP", "A", true},
    {"project_picker.page_down", "B+DOWN", "A", true},
    {"project_picker.activate", "A", "B", true},
    {"project_picker.previous", "UP", "LEFT+DOWN+RIGHT+L+B+A+R+START+SELECT",
     true},
    {"project_picker.next", "DOWN", "LEFT+RIGHT+UP+L+B+A+R+START+SELECT", true},
    {"project_picker.left", "LEFT", "DOWN+RIGHT+UP+L+B+A+R+START+SELECT", true},
    {"project_picker.right", "RIGHT", "LEFT+DOWN+UP+L+B+A+R+START+SELECT",
     true},
    {"new_project.keyboard_input", "A", "LEFT+DOWN+RIGHT+UP+L+B+R+START+SELECT",
     true},
    {"new_project.keyboard_erase", "B", "LEFT+DOWN+RIGHT+UP+L+A+R+START+SELECT",
     true},
    {"new_project.keyboard_cursor_left", "L",
     "LEFT+DOWN+RIGHT+UP+B+A+R+START+SELECT", true},
    {"new_project.keyboard_cursor_right", "R",
     "LEFT+DOWN+RIGHT+UP+L+B+A+START+SELECT", true},
    {"new_project.keyboard_up", "UP", "LEFT+DOWN+RIGHT+L+B+A+R+START+SELECT",
     true},
    {"new_project.keyboard_down", "DOWN", "LEFT+RIGHT+UP+L+B+A+R+START+SELECT",
     true},
    {"new_project.keyboard_left", "LEFT", "DOWN+RIGHT+UP+L+B+A+R+START+SELECT",
     true},
    {"new_project.keyboard_right", "RIGHT", "LEFT+DOWN+UP+L+B+A+R+START+SELECT",
     true},
    {"new_project.keyboard_done", "START", "LEFT+DOWN+RIGHT+UP+L+B+A+R+SELECT",
     true},
    {"new_project.activate", "A", "LEFT+DOWN+RIGHT+UP+L+B+R+START+SELECT",
     true},
    {"new_project.increment", "A+UP", "", true},
    {"new_project.decrement", "A+DOWN", "", true},
    {"new_project.up", "UP", "LEFT+DOWN+RIGHT+L+B+A+R+START+SELECT", true},
    {"new_project.down", "DOWN", "LEFT+RIGHT+UP+L+B+A+R+START+SELECT", true},
    {"new_project.left", "LEFT", "DOWN+RIGHT+UP+L+B+A+R+START+SELECT", true},
    {"new_project.right", "RIGHT", "LEFT+DOWN+UP+L+B+A+R+START+SELECT", true},
    {"message_box.confirm", "A", "", true},
    {"message_box.left", "LEFT", "", true},
    {"message_box.right", "RIGHT", "", true},
    {"message_box.confirm_release", "A", "", false},
    {"message_box.left_release", "LEFT", "", false},
    {"message_box.right_release", "RIGHT", "", false},
    {"sample_browser.page_up", "B+UP", "", true},
    {"sample_browser.page_down", "B+DOWN", "", true},
    {"sample_browser.activate", "A", "B", true},
    {"sample_browser.activate_previous", "A+UP", "B", true},
    {"sample_browser.activate_next", "A+DOWN", "B", true},
    {"sample_browser.preview", "START", "A+B", true},
    {"sample_browser.preview_previous", "START+UP", "A+B", true},
    {"sample_browser.preview_next", "START+DOWN", "A+B", true},
    {"sample_browser.load", "START+RIGHT", "A+B", true},
    {"sample_browser.parent", "START+LEFT", "A+B", true},
    {"sample_browser.previous", "UP", "LEFT+DOWN+RIGHT+L+B+A+R+START+SELECT", true},
    {"sample_browser.next", "DOWN", "LEFT+RIGHT+UP+L+B+A+R+START+SELECT", true},
    {"sample_browser.left", "LEFT", "DOWN+RIGHT+UP+L+B+A+R+START+SELECT", true},
    {"sample_browser.right", "RIGHT", "LEFT+DOWN+UP+L+B+A+R+START+SELECT", true},
    {"song.record", "SELECT+A", "LEFT+DOWN+RIGHT+UP+L+B+R+START", true},
    {"record_options.left", "LEFT", "DOWN+RIGHT+UP+L+B+A+R+START+SELECT", true},
    {"record_options.right", "RIGHT", "LEFT+DOWN+UP+L+B+A+R+START+SELECT", true},
    {"record_options.toggle", "A", "LEFT+DOWN+RIGHT+UP+L+B+R+START+SELECT", true},
    {"record_options.back", "B", "LEFT+DOWN+RIGHT+UP+L+A+R+START+SELECT", true},
    {"record.cancel_count_in", "START", "LEFT+DOWN+RIGHT+UP+L+B+A+R+SELECT", true},
    {"record.stop", "START", "LEFT+DOWN+RIGHT+UP+L+B+A+R+SELECT", true},
    {"record.close", "A", "LEFT+DOWN+RIGHT+UP+L+B+R+START+SELECT", true},
    {"record.back", "B", "LEFT+DOWN+RIGHT+UP+L+A+R+START+SELECT", true},
    {"record.up", "UP", "LEFT+DOWN+RIGHT+L+B+A+R+START+SELECT", true},
    {"record.down", "DOWN", "LEFT+RIGHT+UP+L+B+A+R+START+SELECT", true},
    {"record.accept", "A", "LEFT+DOWN+RIGHT+UP+L+B+R+START+SELECT", true},
    {"sample_editor.tap", "A", "", true},
    {"sample_editor.exit", "B", "LEFT+DOWN+RIGHT+UP+L+A+R+START+SELECT", true},
    {"sample_editor.preview", "START", "LEFT+DOWN+RIGHT+UP+L+B+A+R+SELECT", true},
    {"sample_editor.zoom", "L", "LEFT+DOWN+RIGHT+UP+B+A+R+START+SELECT", true},
    {"sample_editor.edit_up", "A+UP", "", true},
    {"sample_editor.edit_down", "A+DOWN", "", true},
    {"sample_editor.edit_left", "A+LEFT", "", true},
    {"sample_editor.edit_right", "A+RIGHT", "", true},
    {"sample_editor.down", "DOWN", "LEFT+RIGHT+UP+L+B+A+R+START+SELECT", true},
    {"sample_editor.right", "RIGHT", "LEFT+DOWN+UP+L+B+A+R+START+SELECT", true},
    {"sample_editor.up", "UP", "LEFT+DOWN+RIGHT+L+B+A+R+START+SELECT", true},
    {"sample_editor.left", "LEFT", "DOWN+RIGHT+UP+L+B+A+R+START+SELECT", true},
    {"project.activate", "A", "LEFT+DOWN+RIGHT+UP+L+B+R+START+SELECT", true},
    {"project.tap_tempo", "B", "LEFT+DOWN+RIGHT+UP+L+A+R+START+SELECT", true},
    {"project.down", "DOWN", "A+B+L+R+SELECT+START", true},
    {"project.up", "UP", "A+B+L+R+SELECT+START", true},
    {"project.left", "LEFT", "A+B+L+R+SELECT+START", true},
    {"project.right", "RIGHT", "A+B+L+R+SELECT+START", true},
    {"project.edit_down", "A+DOWN", "", true},
    {"project.edit_up", "A+UP", "", true},
    {"project.edit_left", "A+LEFT", "", true},
    {"project.edit_right", "A+RIGHT", "", true},
    {"project.nudge_down", "B+LEFT", "A", true},
    {"project.nudge_up", "B+RIGHT", "A", true},
    {"project.song", "R+DOWN", "", true},
    {"project.play", "START", "R", true},
    {"instrument.previous", "B+LEFT", "", true},
    {"instrument.next", "B+RIGHT", "", true},
    {"instrument.previous_16", "B+DOWN", "", true},
    {"instrument.next_16", "B+UP", "", true},
    {"instrument.phrase", "R+LEFT", "B", true},
    {"instrument.table", "R+DOWN", "B", true},
    {"instrument.play_from_start", "R+START", "B", true},
    {"instrument.play", "START", "B+R", true},
};
const char *buttons[] = {"LEFT", "DOWN", "RIGHT", "UP",    "L",
                         "B",    "A",    "R",     "START", "SELECT"};
bool ParseMask(const char *text, unsigned short &mask) {
    mask = 0;
    if (!text)
        return false;
    std::string value(text);
    if (value.empty())
        return true;
    size_t start = 0;
    while (start < value.size()) {
        size_t end = value.find('+', start);
        std::string token =
            value.substr(start, end == std::string::npos ? end : end - start);
        unsigned short bit = 0;
        for (int i = 0; i < 10; ++i)
            if (token == buttons[i])
                bit = 1 << i;
        if (!bit || (mask & bit))
            return false;
        mask |= bit;
        if (end == std::string::npos)
            return true;
        start = end + 1;
    }
    return false; // Trailing '+'.
}
} // namespace

Shortcuts::Shortcuts() {
    for (int i = 0; i < Count; ++i) {
        ParseMask(defaults[i].required, bindings_[i].required);
        ParseMask(defaults[i].forbidden, bindings_[i].forbidden);
        bindings_[i].pressed = defaults[i].pressed;
    }
}

Shortcuts &Shortcuts::Get() {
    static Shortcuts shortcuts;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        Path path("bin:shortcuts.xml");
        if (path.Exists()) {
            TiXmlDocument document(path.GetPath());
            std::string error;
            if (!document.LoadFile())
                Trace::Log("SHORTCUTS",
                           "Cannot parse shortcuts.xml; using defaults");
            else if (!shortcuts.Load(document.RootElement(), error))
                Trace::Log("SHORTCUTS", "%s; using defaults", error.c_str());
        }
    }
    return shortcuts;
}

bool Shortcuts::Load(const TiXmlElement *root, std::string &error) {
    error.clear();
    if (!root || std::string(root->Value()) != "SHORTCUTS" ||
        !root->Attribute("version") ||
        std::string(root->Attribute("version")) != "1") {
        error = "Expected SHORTCUTS version=1";
        return false;
    }
    Shortcuts candidate;
    bool seen[Count] = {};
    for (const TiXmlElement *element = root->FirstChildElement(); element;
         element = element->NextSiblingElement()) {
        const char *action = element->Attribute("action");
        int index = -1;
        for (int i = 0; action && i < Count; ++i)
            if (std::string(action) == defaults[i].name)
                index = i;
        if (std::string(element->Value()) != "BIND" || index < 0) {
            error = "Unknown shortcut action or element";
            return false;
        }
        if (seen[index]) {
            error = std::string("Duplicate shortcut: ") + action;
            return false;
        }
        seen[index] = true;
        for (const TiXmlAttribute *attr = element->FirstAttribute(); attr;
             attr = attr->Next()) {
            std::string name(attr->Name());
            if (name != "action" && name != "keys" && name != "exclude" &&
                name != "event") {
                error = std::string("Unknown shortcut attribute: ") + name;
                return false;
            }
        }
        Binding &binding = candidate.bindings_[index];
        const char *exclude = element->Attribute("exclude");
        const char *event = element->Attribute("event");
        if (!ParseMask(element->Attribute("keys"), binding.required) ||
            !binding.required ||
            !ParseMask(exclude ? exclude : "", binding.forbidden) ||
            (binding.required & binding.forbidden) || !event ||
            (std::string(event) != "press" &&
             std::string(event) != "release") ||
            element->FirstChildElement()) {
            error = std::string("Invalid shortcut: ") + action;
            return false;
        }
        binding.pressed = std::string(event) == "press";
        // Mute/Solo release actions run only in their corresponding active
        // mode. Other migrated view actions are dispatched on press.
        if (index >= MixerMute && binding.pressed != defaults[index].pressed) {
            error = std::string("Unsupported event for shortcut: ") + action;
            return false;
        }
    }
    *this = candidate;
    return true;
}

bool Shortcuts::Matches(Action action, unsigned short held,
                        bool pressed) const {
    if (action < 0 || action >= Count)
        return false;
    const Binding &binding = bindings_[action];
    return pressed == binding.pressed &&
           (held & binding.required) == binding.required &&
           !(held & binding.forbidden);
}

unsigned short Shortcuts::RemainingButtons(Action action,
                                           unsigned short held) const {
    if (action < 0 || action >= Count)
        return held;
    return held & ~bindings_[action].required;
}

std::string Shortcuts::KeyNames(Action action) const {
    std::string result;
    for (int i = 0; i < 10; ++i) {
        if (bindings_[action].required & (1 << i)) {
            if (!result.empty()) result += "+";
            result += buttons[i];
        }
    }
    return result;
}
