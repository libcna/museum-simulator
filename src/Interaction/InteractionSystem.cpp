#include "Interaction/InteractionSystem.hpp"

#include <limits>

using namespace Microsoft::Xna::Framework;

int InteractionSystem::Add(Interactable interactable)
{
    items_.push_back(std::move(interactable));
    return static_cast<int>(items_.size()) - 1;
}

int InteractionSystem::Focus(const Ray& viewRay, RoomId currentRoom, const CollisionWorld& collision)
{
    focused_ = -1;
    float best = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Interactable& item = items_[i];
        if (item.room != currentRoom) continue;
        const std::optional<float> hit = item.bounds.Intersects(viewRay);
        if (!hit || *hit > item.maxDistance || *hit >= best) continue;
        // A wall or pedestal that is closer than the target hides it.
        const std::optional<float> blocker = collision.Raycast(viewRay, *hit);
        if (blocker && *blocker < *hit - 0.02f) continue;
        best = *hit;
        focused_ = static_cast<int>(i);
    }
    return focused_;
}

std::string InteractionSystem::FocusedPrompt() const
{
    if (focused_ < 0) return {};
    const Interactable& item = items_[static_cast<std::size_t>(focused_)];
    return item.prompt ? item.prompt() : std::string{};
}

bool InteractionSystem::Activate()
{
    if (focused_ < 0) return false;
    const Interactable& item = items_[static_cast<std::size_t>(focused_)];
    if (!item.activate) return false;
    item.activate();
    return true;
}
