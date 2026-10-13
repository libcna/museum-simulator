#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "World/CollisionWorld.hpp"

/** What the visitor is doing to an interactable this frame. */
struct InteractEvent {
    Microsoft::Xna::Framework::Vector3 hit;   // world-space point under the crosshair
    bool pressed = false;                     // the use button went down this frame
    bool held = false;                        // the use button is still down (drag)
    float wheel = 0.0f;                       // mouse-wheel notches this frame (+ = away)
    bool alternate = false;                   // the alternate button (right click / Q)
};

/** Something the visitor can look at and press E on. */
struct Interactable {
    Microsoft::Xna::Framework::BoundingBox bounds;           // world space; keep it a little larger than the object
    std::function<std::string()> prompt;                     // shown on the HUD, e.g. "Stop rotation"
    std::function<void()> activate;                          // simple press
    std::function<void(const InteractEvent&)> interact;      // press / drag / wheel with the hit point; optional
    std::function<bool(const std::string&, const float*)> command;   // test hook: "press:<label>[=value]" from the script runner
    float maxDistance = 3.2f;
    bool wantsHold = false;                                  // keeps focus while the button is held (sliders)
};

/**
 * Picks the interactable under the crosshair with CNA's Ray/BoundingBox intersection and
 * refuses targets hidden behind walls or furniture. One instance per built cell.
 */
class InteractionSystem {
public:
    int Add(Interactable interactable);

    /** Re-targets from the camera's view ray. Returns the index of the focused item or -1. */
    int Focus(const Microsoft::Xna::Framework::Ray& viewRay, const CollisionWorld& collision, bool keepCurrent = false);
    [[nodiscard]] int Focused() const { return focused_; }
    [[nodiscard]] std::string FocusedPrompt() const;
    /** Delivers an event to the focused item (activate and/or interact). Returns true when something handled it. */
    bool Dispatch(const InteractEvent& event);
    /** Convenience: a plain press. */
    bool Activate();
    void Clear() { items_.clear(); focused_ = -1; controls_ = 0; }
    /** Interactive controls inside an interactable (the rows of a control panel): counted for the museum inventory. */
    void AddControls(int n) { controls_ += n; }
    [[nodiscard]] int Controls() const { return controls_; }

    [[nodiscard]] std::size_t Count() const { return items_.size(); }
    [[nodiscard]] const std::vector<Interactable>& Items() const { return items_; }
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 LastHit() const { return lastHit_; }

private:
    int controls_ = 0;
    std::vector<Interactable> items_;
    int focused_ = -1;
    Microsoft::Xna::Framework::Vector3 lastHit_;
};
