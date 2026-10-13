#pragma once

#include <optional>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

// Static axis-aligned colliders for walls, pedestals and columns. The player is
// a vertical cylinder, so movement collides as a circle against each box's XZ
// footprint; the museum has one floor level and nothing to step over.
class CollisionWorld {
public:
    void AddBox(const Microsoft::Xna::Framework::BoundingBox& box);
    [[nodiscard]] std::size_t Count() const { return boxes_.size(); }
    [[nodiscard]] const std::vector<Microsoft::Xna::Framework::BoundingBox>& Boxes() const { return boxes_; }

    /** Moves a circle by `delta` on the XZ plane, sliding along whatever it hits. Y is kept. */
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 Move(const Microsoft::Xna::Framework::Vector3& from,
                                                         const Microsoft::Xna::Framework::Vector3& delta,
                                                         float radius) const;

    [[nodiscard]] bool OverlapsCircle(float x, float z, float radius) const;

    /** Distance along `ray` to the nearest collider, if it is hit within `maxDistance`. */
    [[nodiscard]] std::optional<float> Raycast(const Microsoft::Xna::Framework::Ray& ray, float maxDistance) const;

private:
    std::vector<Microsoft::Xna::Framework::BoundingBox> boxes_;
};
