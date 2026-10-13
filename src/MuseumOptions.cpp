#include "MuseumOptions.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

bool Match(const char* arg, const char* name, const char*& value, int& i, int argc, char** argv)
{
    const std::size_t n = std::strlen(name);
    if (std::strncmp(arg, name, n) != 0) return false;
    if (arg[n] == '=') { value = arg + n + 1; return true; }
    if (arg[n] == '\0' && i + 1 < argc) { value = argv[++i]; return true; }
    return false;
}

}  // namespace

const char* MuseumOptions::Usage()
{
    return
        "cna-3d-museum [options]\n"
        "  --width N --height N      window size (default 1280x720)\n"
        "  --fullscreen              start fullscreen\n"
        "  --no-vsync                disable vertical sync\n"
        "  --sensitivity F           mouse sensitivity multiplier (default 1.0)\n"
        "  --volume F                master volume 0..1\n"
        "  --no-audio                do not load or play any sound\n"
        "  --room KEY                start in a room by its key (see docs/MUSEUM_MAP.md), e.g. lighting\n"
        "  --pos X,Y,Z --yaw DEG --pitch DEG   start pose (metres, degrees)\n"
        "  --frames N                exit after N frames\n"
        "  --screenshot FILE.png [--screenshot-frame N]   save the back buffer and exit\n"
        "  --view NAME               start at a reference view (see --list-views); --no-hud hides the overlay\n"
        "  --list-views              print the reference views: name, room, description (no window)\n"
        "  --time HOURS [--time-run] set the museum clock (0..24, default 11); frozen unless --time-run\n"
        "  --fixed-step SECONDS      advance the simulation by a fixed step per frame\n"
        "  --script \"a;b;c\"          scripted test actions (see src/MuseumGame.cpp, RunScript)\n"
        "  --validate                check the museum's structure (no window); exit status 1 if invalid\n"
        "  --list-rooms              print the key of every room (no window)\n"
        "  --inventory               print the museum's counts as JSON (no window)\n"
        "  --map-markdown            print the museum map as Markdown (no window)\n"
        "  --coverage-markdown       print the CNA API coverage table as Markdown (no window)\n";
}

MuseumOptions MuseumOptions::Parse(int argc, char** argv)
{
    MuseumOptions o;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        const char* v = nullptr;
        if (Match(a, "--width", v, i, argc, argv)) o.width = std::atoi(v);
        else if (Match(a, "--height", v, i, argc, argv)) o.height = std::atoi(v);
        else if (!std::strcmp(a, "--fullscreen")) o.fullscreen = true;
        else if (!std::strcmp(a, "--no-vsync")) o.vsync = false;
        else if (Match(a, "--sensitivity", v, i, argc, argv)) o.mouseSensitivity = 0.0025f * static_cast<float>(std::atof(v));
        else if (Match(a, "--volume", v, i, argc, argv)) o.volume = static_cast<float>(std::atof(v));
        else if (!std::strcmp(a, "--no-audio")) o.audio = false;
        else if (Match(a, "--room", v, i, argc, argv)) o.startRoom = v;
        else if (Match(a, "--yaw", v, i, argc, argv)) o.startYawDegrees = static_cast<float>(std::atof(v));
        else if (Match(a, "--pitch", v, i, argc, argv)) o.startPitchDegrees = static_cast<float>(std::atof(v));
        else if (Match(a, "--pos", v, i, argc, argv)) {
            float x = 0, y = 0, z = 0;
            if (std::sscanf(v, "%f,%f,%f", &x, &y, &z) == 3)
                o.startPosition = Microsoft::Xna::Framework::Vector3(x, y, z);
        }
        else if (Match(a, "--frames", v, i, argc, argv)) o.maxFrames = std::atoi(v);
        else if (Match(a, "--screenshot-frame", v, i, argc, argv)) o.screenshotFrame = std::atoi(v);
        else if (Match(a, "--screenshot", v, i, argc, argv)) o.screenshotPath = v;
        else if (Match(a, "--time", v, i, argc, argv)) o.startHours = static_cast<float>(std::atof(v));
        else if (!std::strcmp(a, "--time-run")) o.timeRuns = true;
        else if (Match(a, "--fixed-step", v, i, argc, argv)) o.fixedTimeStep = static_cast<float>(std::atof(v));
        else if (Match(a, "--script", v, i, argc, argv)) o.script = v;
        else if (!std::strcmp(a, "--validate")) o.validate = true;
        else if (!std::strcmp(a, "--inventory")) o.inventory = true;
        else if (Match(a, "--view", v, i, argc, argv)) o.view = v;
        else if (!std::strcmp(a, "--no-hud")) o.hud = false;
        else if (!std::strcmp(a, "--list-views")) o.listViews = true;
        else if (!std::strcmp(a, "--map-markdown")) o.mapMarkdown = true;
        else if (!std::strcmp(a, "--coverage-markdown")) o.coverageMarkdown = true;
        else if (!std::strcmp(a, "--list-rooms")) o.listRooms = true;
        else if (!std::strcmp(a, "--help") || !std::strcmp(a, "-h")) o.showHelp = true;
        else std::fprintf(stderr, "cna-3d-museum: ignoring unknown option '%s'\n", a);
    }
    if (o.width < 320) o.width = 320;
    if (o.height < 200) o.height = 200;
    return o;
}
