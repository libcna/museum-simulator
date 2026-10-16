#pragma once

#include <optional>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

// Static axis-aligned colliders for walls, pedestals and columns. The player is
// a vertical cylinder, so movement collides as a circle against the XZ footprint
// of boxes which overlap the player's current vertical span.
class CollisionWorld {
public:
    void AddBox(const Microsoft::Xna::Framework::BoundingBox& box);
    [[nodiscard]] std::size_t Count() const { return boxes_.size(); }
    [[nodiscard]] const std::vector<Microsoft::Xna::Framework::BoundingBox>& Boxes() const { return boxes_; }

    /** Moves a vertical cylinder, sliding on XZ; `from.Y` is the height of its feet and is kept. */
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 Move(const Microsoft::Xna::Framework::Vector3& from,
                                                         const Microsoft::Xna::Framework::Vector3& delta,
                                                         float radius) const;

    [[nodiscard]] bool OverlapsCircle(float x, float z, float radius, float feetY = 0.0f) const;

    /** Distance along `ray` to the nearest collider, if it is hit within `maxDistance`. */
    [[nodiscard]] std::optional<float> Raycast(const Microsoft::Xna::Framework::Ray& ray, float maxDistance) const;

private:
    std::vector<Microsoft::Xna::Framework::BoundingBox> boxes_;
};
