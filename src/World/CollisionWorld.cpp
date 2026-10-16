#include "World/CollisionWorld.hpp"

#include <algorithm>
#include <cmath>

using namespace Microsoft::Xna::Framework;

namespace {

// Player cylinders span roughly floor..head. Boxes entirely above or below that
// band (ceiling beams, floor trim) are not obstacles.
constexpr float kBodyBottom = 0.1f;
constexpr float kBodyTop = 1.8f;

bool Blocks(const BoundingBox& b, float feetY)
{
    return b.Max.Y > feetY + kBodyBottom && b.Min.Y < feetY + kBodyTop;
}

bool CircleOverlapsBox(const BoundingBox& b, float x, float z, float r)
{
    const float cx = std::clamp(x, b.Min.X, b.Max.X);
    const float cz = std::clamp(z, b.Min.Z, b.Max.Z);
    const float dx = x - cx;
    const float dz = z - cz;
    return dx * dx + dz * dz < r * r;
}

}  // namespace

void CollisionWorld::AddBox(const BoundingBox& box)
{
    boxes_.push_back(box);
}

bool CollisionWorld::OverlapsCircle(float x, float z, float radius, float feetY) const
{
    for (const BoundingBox& b : boxes_)
        if (Blocks(b, feetY) && CircleOverlapsBox(b, x, z, radius)) return true;
    return false;
}

Vector3 CollisionWorld::Move(const Vector3& from, const Vector3& delta, float radius) const
{
    // A frame can move at most a few centimetres, far less than any wall's
    // thickness, so resolving each axis on its own is enough to slide along
    // walls without tunnelling.
    Vector3 p = from;
    if (!OverlapsCircle(p.X + delta.X, p.Z, radius, from.Y)) p.X += delta.X;
    if (!OverlapsCircle(p.X, p.Z + delta.Z, radius, from.Y)) p.Z += delta.Z;
    p.Y = from.Y;
    return p;
}

std::optional<float> CollisionWorld::Raycast(const Ray& ray, float maxDistance) const
{
    std::optional<float> best;
    for (const BoundingBox& b : boxes_) {
        const std::optional<float> d = b.Intersects(ray);
        if (d && *d >= 0.0f && *d <= maxDistance && (!best || *d < *best)) best = d;
    }
    return best;
}
