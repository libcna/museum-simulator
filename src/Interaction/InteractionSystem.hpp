#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"
#include "World/CollisionWorld.hpp"
#include "World/RoomDef.hpp"

/** Something the visitor can look at and press E on. */
struct Interactable {
    RoomId room;
    Microsoft::Xna::Framework::BoundingBox bounds;           // world space; keep it a little larger than the object
    std::function<std::string()> prompt;                     // shown on the HUD, e.g. "Stop rotation"
    std::function<void()> activate;
    float maxDistance = 3.2f;
};

/**
 * Picks the interactable under the crosshair with CNA's Ray/BoundingBox
 * intersection, and refuses targets hidden behind walls or furniture.
 */
class InteractionSystem {
public:
    int Add(Interactable interactable);

    /** Re-targets from the camera's view ray. Returns the index of the focused item or -1. */
    int Focus(const Microsoft::Xna::Framework::Ray& viewRay, RoomId currentRoom, const CollisionWorld& collision);
    [[nodiscard]] int Focused() const { return focused_; }
    [[nodiscard]] std::string FocusedPrompt() const;
    /** Activates the focused item. Returns true when something was activated. */
    bool Activate();

    [[nodiscard]] std::size_t Count() const { return items_.size(); }
    [[nodiscard]] const std::vector<Interactable>& Items() const { return items_; }

private:
    std::vector<Interactable> items_;
    int focused_ = -1;
};
