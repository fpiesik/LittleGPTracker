#include "Adapters/LINUX/System/LINUXSystem.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Controllers/Shortcuts.h"
#include "Externals/TinyXML/tinyxml.h"
#include <cassert>
#include <fstream>
#include <sstream>

// Independent transcription of the old GrooveView branches. Compare every
// possible physical pad state, including overlapping/opposing directions.
static unsigned Legacy(unsigned mask, bool pressed) {
    unsigned actions = 0;
    if (!pressed)
        return actions;
    if (mask & 32) {
        if (mask & 1)
            actions |= 1 << 0;
        if (mask & 4)
            actions |= 1 << 1;
        if (mask & 2)
            actions |= 1 << 2;
        if (mask & 8)
            actions |= 1 << 3;
        if (mask & 64)
            actions |= 1 << 4;
    } else if (mask & 64) {
        if (mask & 1)
            actions |= 1 << 5;
        if (mask & 4)
            actions |= 1 << 6;
        if (mask & 2)
            actions |= 1 << 7;
        if (mask & 8)
            actions |= 1 << 8;
        if (mask == 64)
            actions |= 1 << 9;
    } else if (mask & 128) {
        if (mask & 2)
            actions |= 1 << 10;
        if (mask & 256)
            actions |= 1 << 11;
    } else {
        if (mask & 2)
            actions |= 1 << 12;
        if (mask & 8)
            actions |= 1 << 13;
        if (mask & 256)
            actions |= 1 << 14;
    }
    return actions;
}
static unsigned Resolve(const Shortcuts &s, unsigned mask, bool pressed) {
    unsigned actions = 0;
    for (int i = 0; i <= Shortcuts::GroovePlay; ++i)
        if (s.Matches(static_cast<Shortcuts::Action>(i), mask, pressed))
            actions |= 1 << i;
    return actions;
}
static bool Load(Shortcuts &s, const char *xml) {
    TiXmlDocument document;
    document.Parse(xml);
    assert(!document.Error());
    std::string error;
    return s.Load(document.RootElement(), error);
}
int main() {
    // Linked application globals free their paths through the System factory.
    System::Install(new LINUXSystem);
    FileSystem::Install(new UnixFileSystem);
    Path::SetAlias("bin", "projects");
    const Shortcuts &startup = Shortcuts::Get();
    Shortcuts defaults, configured;
    std::ifstream file("projects/shortcuts.xml");
    assert(file.good());
    std::ostringstream xml;
    xml << file.rdbuf();
    assert(Load(configured, xml.str().c_str()));
    for (unsigned mask = 0; mask < 1024; ++mask)
        for (int pressed = 0; pressed < 2; ++pressed) {
            assert(Resolve(defaults, mask, pressed) == Legacy(mask, pressed));
            assert(Resolve(startup, mask, pressed) == Legacy(mask, pressed));
            assert(Resolve(configured, mask, pressed) == Legacy(mask, pressed));
        }
    assert(Load(configured,
                "<SHORTCUTS version='1'><BIND action='groove.clear' "
                "keys='L+A' exclude='B' event='press'/></SHORTCUTS>"));
    assert(configured.Matches(Shortcuts::GrooveClear, 16 | 64, true));
    assert(!configured.Matches(Shortcuts::GrooveClear, 32 | 64, true));
    assert(!configured.Matches(Shortcuts::GrooveClear, 16 | 64, false));
    // Invalid edits do not replace any part of the active configuration.
    const char *bad[] = {"<SHORTCUTS/>",
                         "<SHORTCUTS version='2'/>",
                         "<SHORTCUTS version='1'><BIND action='unknown' "
                         "keys='A' event='press'/></SHORTCUTS>",
                         "<SHORTCUTS version='1'><BIND action='groove.clear' "
                         "keys='A+' event='press'/></SHORTCUTS>",
                         "<SHORTCUTS version='1'><BIND action='groove.clear' "
                         "keys='A+A' event='press'/></SHORTCUTS>",
                         "<SHORTCUTS version='1'><BIND action='groove.clear' "
                         "keys='A' exclude='A' event='press'/></SHORTCUTS>",
                         "<SHORTCUTS version='1'><BIND action='groove.clear' "
                         "keys='A' event='pres'/></SHORTCUTS>",
                         "<SHORTCUTS version='1'><BIND action='groove.clear' "
                         "keys='A' event='press' typo='B'/></SHORTCUTS>",
                         "<SHORTCUTS version='1'><BIND action='groove.clear' "
                         "keys='A' event='press'/><BIND action='groove.clear' "
                         "keys='B' event='press'/></SHORTCUTS>"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        assert(!Load(configured, bad[i]));
        assert(configured.Matches(Shortcuts::GrooveClear, 16 | 64, true));
        assert(!configured.Matches(Shortcuts::GrooveClear, 32 | 64, true));
    }
    // Release matches the buttons STILL held, as supplied by AppWindow.
    assert(Load(configured,
                "<SHORTCUTS version='1'><BIND action='groove.clear' "
                "keys='L' exclude='A' event='release'/></SHORTCUTS>"));
    assert(!configured.Matches(Shortcuts::GrooveClear, 16 | 64, true));
    assert(configured.Matches(Shortcuts::GrooveClear, 16, false));
    assert(!configured.Matches(Shortcuts::GrooveClear, 0, false));
    // Loading a new document resets omitted entries to their defaults.
    assert(Load(configured, "<SHORTCUTS version='1'/>"));
    assert(configured.Matches(Shortcuts::GrooveClear, 32 | 64, true));
    return 0;
}
