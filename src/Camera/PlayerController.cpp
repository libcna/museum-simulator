#include "Camera/PlayerController.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>

#include "Diagnostics/Log.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;

namespace {
constexpr float kStrideLength = 1.55f;
constexpr float kMaxStepSeconds = 0.05f;
constexpr int kVerifyFrames = 6;   // frames spent checking that re-centring the pointer works   // a long frame is walked in several small steps
}  // namespace

void PlayerController::SetMouseCaptured(bool captured, int windowWidth, int windowHeight)
{
    if (captured == captured_) return;
    captured_ = captured;
    centreX_ = windowWidth / 2;
    centreY_ = windowHeight / 2;
    if (captured) {
        try {
            Mouse::setIsRelativeMouseModeEXTProperty(true);
            relativeMode_ = Mouse::getIsRelativeMouseModeEXTProperty();
        } catch (const std::exception& e) {
            relativeMode_ = false;
            Museum::Log("input", "relative mouse mode unavailable (%s); re-centring the cursor instead", e.what());
        }
        verifyFrames_ = 0;
        verifiedFrames_ = 0;
        pointerUnavailable_ = false;
        if (!relativeMode_) Mouse::SetPosition(centreX_, centreY_);
    } else {
        try {
            Mouse::setIsRelativeMouseModeEXTProperty(false);
        } catch (const std::exception&) {
        }
        relativeMode_ = false;
    }
}

void PlayerController::Update(float dt, FirstPersonCamera& camera, const CollisionWorld& collision,
                              const InputState& input, bool mouseLook)
{
    // Look ---------------------------------------------------------------
    float dyaw = 0.0f;
    float dpitch = 0.0f;
    if (mouseLook && captured_) {
        const MouseState& now = input.Mouse();
        float dx;
        float dy;
        if (relativeMode_) {
            // In relative mode CNA reports the motion since the last read in X/Y (consume-on-read), not a position.
            dx = static_cast<float>(now.getXProperty());
            dy = static_cast<float>(now.getYProperty());
        } else {
            // The pointer is re-centred every frame, so it must read back at the centre. A platform with no
            // window to warp the pointer in (CNA's SOFTWARE renderer presents a CPU buffer) keeps reporting one
            // fixed position instead: that is "no pointer", not a huge mouse movement. The first frames after
            // capturing only verify that, without turning the camera.
            const int x = now.getXProperty(), y = now.getYProperty();
            if (verifyFrames_ < kVerifyFrames) {
                ++verifyFrames_;
                const bool centred = std::abs(x - centreX_) <= 2 && std::abs(y - centreY_) <= 2;
                verifiedFrames_ += centred ? 1 : 0;
                Mouse::SetPosition(centreX_, centreY_);
                if (verifyFrames_ == kVerifyFrames && verifiedFrames_ == 0) {
                    pointerUnavailable_ = true;
                    Museum::Log("input", "the pointer cannot be re-centred on this platform: mouse look is off, use the arrow keys and Page Up/Down");
                }
                dx = dy = 0.0f;
            } else if (pointerUnavailable_) {
                dx = dy = 0.0f;
            } else {
                dx = static_cast<float>(x - centreX_);
                dy = static_cast<float>(y - centreY_);
                Mouse::SetPosition(centreX_, centreY_);
            }
        }
        dyaw -= dx * settings_.mouseSensitivity;
        dpitch -= dy * settings_.mouseSensitivity * (settings_.invertY ? -1.0f : 1.0f);
    }
    // Keyboard look: always available, and the only look control where there is no usable pointer.
    if (!keyboardBlocked_) {
        constexpr float kTurnRate = 1.9f, kPitchRate = 1.2f;
        if (input.Down(Keys::Left)) dyaw += kTurnRate * dt;
        if (input.Down(Keys::Right)) dyaw -= kTurnRate * dt;
        if (input.Down(Keys::PageUp)) dpitch += kPitchRate * dt;
        if (input.Down(Keys::PageDown)) dpitch -= kPitchRate * dt;
    }
    const GamePadState& pad = input.Pad();
    Vector2 stickMove(0, 0);
    // With CNA's keyboard-emulated gamepad on, the keys already walk the visitor: do not count them twice.
    if (pad.getIsConnectedProperty() && !GamePad::getKeyboardEmulationEnabledEXT()) {
        const Vector2 look = pad.getThumbSticksProperty().getRightProperty();
        dyaw -= look.X * settings_.stickLookSpeed * dt;
        dpitch += look.Y * settings_.stickLookSpeed * dt * (settings_.invertY ? -1.0f : 1.0f);
        stickMove = pad.getThumbSticksProperty().getLeftProperty();
    }
    camera.Rotate(dyaw, dpitch);

    // Walk ---------------------------------------------------------------
    float forward = 0.0f;
    float strafe = 0.0f;
    if (!keyboardBlocked_) {
        if (input.Down(Keys::W) || input.Down(Keys::Up)) forward += 1.0f;
        if (input.Down(Keys::S) || input.Down(Keys::Down)) forward -= 1.0f;
        if (input.Down(Keys::D)) strafe += 1.0f;
        if (input.Down(Keys::A)) strafe -= 1.0f;
    }
    forward += stickMove.Y;
    strafe += stickMove.X;
    if (scriptFrames_ > 0) {
        --scriptFrames_;
        forward += scriptForward_;
        strafe += scriptStrafe_;
        camera.Rotate(scriptTurn_, 0.0f);
    }
    const float len = std::sqrt(forward * forward + strafe * strafe);
    if (len > 1.0f) {
        forward /= len;
        strafe /= len;
    }
    const bool sprint = !keyboardBlocked_ && (input.Down(Keys::LeftShift) || input.Down(Keys::RightShift));
    const float speed = settings_.walkSpeed * (sprint ? settings_.sprintMultiplier : 1.0f);

    Vector3 position = camera.Position();
    position.Y = baseY_;   // collision uses the visitor's actual vertical span
    const Vector3 wish = camera.GroundForward() * forward + camera.Right() * strafe;
    const Vector3 velocity = wish * speed;
    moving_ = wish.LengthSquared() > 1e-4f;

    float remaining = std::min(dt, 0.25f);
    float travelled = 0.0f;
    while (remaining > 1e-5f) {
        const float step = std::min(remaining, kMaxStepSeconds);
        const Vector3 next = collision.Move(position, velocity * step, settings_.radius);
        Vector3 grounded = next;
        if (groundResolver_) grounded.Y = groundResolver_(grounded);
        travelled += std::sqrt((grounded.X - position.X) * (grounded.X - position.X) +
                               (grounded.Z - position.Z) * (grounded.Z - position.Z));
        position = grounded;
        remaining -= step;
    }

    walked_ += travelled;
    stride_ += travelled;
    if (stride_ >= kStrideLength) {
        stride_ -= kStrideLength;
        if (onFootstep) onFootstep();
    }
    if (moving_) bobPhase_ += travelled * 4.2f;
    const float bob = moving_ ? std::sin(bobPhase_) * 0.022f * (sprint ? 1.3f : 1.0f) : 0.0f;
    baseY_ = position.Y;
    camera.SetPosition(Vector3(position.X, baseY_ + settings_.eyeHeight + bob, position.Z));
}
