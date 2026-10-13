#include "Camera/PlayerController.hpp"

#include <algorithm>
#include <cmath>
#include <exception>

#include "Diagnostics/Log.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;

namespace {
constexpr float kStrideLength = 1.55f;
constexpr float kMaxStepSeconds = 0.05f;   // a long frame is walked in several small steps
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
        const MouseState& before = input.PreviousMouse();
        float dx;
        float dy;
        if (relativeMode_) {
            dx = static_cast<float>(now.getXProperty() - before.getXProperty());
            dy = static_cast<float>(now.getYProperty() - before.getYProperty());
        } else {
            dx = static_cast<float>(now.getXProperty() - centreX_);
            dy = static_cast<float>(now.getYProperty() - centreY_);
            Mouse::SetPosition(centreX_, centreY_);
        }
        dyaw -= dx * settings_.mouseSensitivity;
        dpitch -= dy * settings_.mouseSensitivity * (settings_.invertY ? -1.0f : 1.0f);
    }
    const GamePadState& pad = input.Pad();
    Vector2 stickMove(0, 0);
    if (pad.getIsConnectedProperty()) {
        const Vector2 look = pad.getThumbSticksProperty().getRightProperty();
        dyaw -= look.X * settings_.stickLookSpeed * dt;
        dpitch += look.Y * settings_.stickLookSpeed * dt * (settings_.invertY ? -1.0f : 1.0f);
        stickMove = pad.getThumbSticksProperty().getLeftProperty();
    }
    camera.Rotate(dyaw, dpitch);

    // Walk ---------------------------------------------------------------
    float forward = 0.0f;
    float strafe = 0.0f;
    if (input.Down(Keys::W) || input.Down(Keys::Up)) forward += 1.0f;
    if (input.Down(Keys::S) || input.Down(Keys::Down)) forward -= 1.0f;
    if (input.Down(Keys::D)) strafe += 1.0f;
    if (input.Down(Keys::A)) strafe -= 1.0f;
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
    const bool sprint = input.Down(Keys::LeftShift) || input.Down(Keys::RightShift);
    const float speed = settings_.walkSpeed * (sprint ? settings_.sprintMultiplier : 1.0f);

    Vector3 position = camera.Position();
    position.Y = 0.0f;   // collision works on the floor plane
    const Vector3 wish = camera.GroundForward() * forward + camera.Right() * strafe;
    const Vector3 velocity = wish * speed;
    moving_ = wish.LengthSquared() > 1e-4f;

    float remaining = std::min(dt, 0.25f);
    float travelled = 0.0f;
    while (remaining > 1e-5f) {
        const float step = std::min(remaining, kMaxStepSeconds);
        const Vector3 next = collision.Move(position, velocity * step, settings_.radius);
        travelled += Vector3::Distance(position, next);
        position = next;
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
    camera.SetPosition(Vector3(position.X, settings_.eyeHeight + bob, position.Z));
}
