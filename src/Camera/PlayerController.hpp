#pragma once

#include <functional>

#include "Camera/FirstPersonCamera.hpp"
#include "Camera/InputState.hpp"
#include "World/CollisionWorld.hpp"

/**
 * Turns input into camera motion: WASD / left stick to walk, mouse / right stick to
 * look, Shift to hurry. The visitor is a cylinder that slides along colliders; there is
 * one floor level, so no gravity or jumping is simulated.
 */
class PlayerController {
public:
    struct Settings {
        float walkSpeed = 3.2f;           // metres per second
        float sprintMultiplier = 1.8f;
        float eyeHeight = 1.65f;
        float radius = 0.32f;
        float mouseSensitivity = 0.0025f; // radians per pixel
        float stickLookSpeed = 2.4f;      // radians per second at full deflection
        bool invertY = false;
    };

    explicit PlayerController(const Settings& settings) : settings_(settings) {}

    /** Height of the floor the visitor stands on; the eye is `eyeHeight` above it. */
    void SetBaseY(float baseY) { baseY_ = baseY; }

    Settings& GetSettings() { return settings_; }
    [[nodiscard]] const Settings& GetSettings() const { return settings_; }

    /** Walk and look for one frame. `mouseLook` is false while the pointer is released (paused). */
    void Update(float dt, FirstPersonCamera& camera, const CollisionWorld& collision, const InputState& input,
                bool mouseLook);

    /** Captures or releases the pointer. Falls back to re-centring the cursor when relative mode is unavailable. */
    void SetMouseCaptured(bool captured, int windowWidth, int windowHeight);
    [[nodiscard]] bool MouseCaptured() const { return captured_; }

    /** Test hook: walk with the given forward/strafe input (each -1..1) for the next `frames` updates. */
    /** While true the walking keys are ignored (text-entry exhibits). */
    void SetKeyboardBlocked(bool blocked) { keyboardBlocked_ = blocked; }
    void ScriptWalk(float forward, float strafe, int frames, float yawTurnPerFrame = 0.0f)
    {
        scriptForward_ = forward;
        scriptStrafe_ = strafe;
        scriptFrames_ = frames;
        scriptTurn_ = yawTurnPerFrame;
    }

    /** Called with the stride distance each time a footstep lands. */
    std::function<void()> onFootstep;
    [[nodiscard]] float DistanceWalked() const { return walked_; }
    [[nodiscard]] bool IsMoving() const { return moving_; }

private:
    Settings settings_;
    float baseY_ = 0.0f;
    bool captured_ = false;
    bool relativeMode_ = false;
    int centreX_ = 0;
    int centreY_ = 0;
    float stride_ = 0.0f;
    float walked_ = 0.0f;
    float bobPhase_ = 0.0f;
    bool moving_ = false;
    float scriptForward_ = 0.0f;
    float scriptStrafe_ = 0.0f;
    float scriptTurn_ = 0.0f;
    int scriptFrames_ = 0;
    bool keyboardBlocked_ = false;
};
