#pragma once

#include <optional>
#include <string>

#include "Microsoft/Xna/Framework/Vector3.hpp"

// Command-line switches. They exist so the museum can be driven and verified
// without a human (screenshots, fixed frame counts, teleporting to a room).
struct MuseumOptions {
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool vsync = true;
    float mouseSensitivity = 0.0025f;   // radians per pixel
    float volume = 1.0f;
    bool audio = true;
    int maxFrames = 0;                  // 0 = run until the user quits
    std::string screenshotPath;         // written after `screenshotFrame`, then exits
    int screenshotFrame = 8;
    std::optional<std::string> startRoom;  // room id, e.g. "lighting"
    std::optional<float> startYawDegrees;
    std::optional<float> startPitchDegrees;
    std::optional<Microsoft::Xna::Framework::Vector3> startPosition;
    float fixedTimeStep = 0.0f;         // >0: advance the simulation by this many seconds per frame
    std::string script;                 // semicolon-separated test actions, see MuseumGame::RunScript
    bool showHelp = false;
    bool validate = false;              // check the catalogue (no window) and exit with the result
    bool listRooms = false;             // print every room key, one per line (no window) and exit
    bool inventory = false;             // print the museum's counts as JSON (no window) and exit

    static MuseumOptions Parse(int argc, char** argv);
    static const char* Usage();
};
