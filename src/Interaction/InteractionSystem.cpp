#include "Interaction/InteractionSystem.hpp"

#include <exception>

#include <limits>

using namespace Microsoft::Xna::Framework;

int InteractionSystem::Add(Interactable interactable)
{
    // Bounds are often built from two ToWorld corners, which swap when a room's frame is turned.
    const Vector3 lo = Vector3::Min(interactable.bounds.Min, interactable.bounds.Max);
    const Vector3 hi = Vector3::Max(interactable.bounds.Min, interactable.bounds.Max);
    interactable.bounds = BoundingBox(lo, hi);
    items_.push_back(std::move(interactable));
    return static_cast<int>(items_.size()) - 1;
}

int InteractionSystem::Focus(const Ray& viewRay, const CollisionWorld& collision, bool keepCurrent)
{
    if (keepCurrent && focused_ >= 0 && items_[static_cast<std::size_t>(focused_)].wantsHold) {
        // A slider being dragged keeps the focus; its hit point follows the ray along the panel.
        const Interactable& item = items_[static_cast<std::size_t>(focused_)];
        BoundingBox wide = item.bounds;
        wide.Min -= Vector3(0.6f, 0.6f, 0.6f);
        wide.Max += Vector3(0.6f, 0.6f, 0.6f);
        if (const std::optional<float> hit = wide.Intersects(viewRay)) {
            lastHit_ = viewRay.Position + viewRay.Direction * *hit;
            return focused_;
        }
    }
    focused_ = -1;
    float best = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Interactable& item = items_[i];
        const std::optional<float> hit = item.bounds.Intersects(viewRay);
        if (!hit || *hit > item.maxDistance || *hit >= best) continue;
        // A wall or pedestal that is closer than the target hides it.
        const std::optional<float> blocker = collision.Raycast(viewRay, *hit);
        if (blocker && *blocker < *hit - 0.02f) continue;
        best = *hit;
        focused_ = static_cast<int>(i);
        lastHit_ = viewRay.Position + viewRay.Direction * *hit;
    }
    return focused_;
}

std::string InteractionSystem::FocusedPrompt() const
{
    if (focused_ < 0) return {};
    const Interactable& item = items_[static_cast<std::size_t>(focused_)];
    return item.prompt ? item.prompt() : std::string{};
}

std::function<void(const std::string& what)> InteractionSystem::failureReporter;

bool InteractionSystem::Dispatch(const InteractEvent& event)
{
    if (focused_ < 0) return false;
    const Interactable& item = items_[static_cast<std::size_t>(focused_)];
    try {
        if (item.interact) {
            item.interact(event);
            return true;
        }
        if (event.pressed && item.activate) {
            item.activate();
            return true;
        }
    } catch (const std::exception& e) {
        if (!failureReporter) throw;
        failureReporter(e.what());
        return true;
    }
    return false;
}

bool InteractionSystem::Activate()
{
    InteractEvent e;
    e.hit = lastHit_;
    e.pressed = true;
    return Dispatch(e);
}
