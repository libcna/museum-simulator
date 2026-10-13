#pragma once

#include "Exhibits/Services.hpp"
#include "World/RoomDef.hpp"

class RoomBuilder;
class InteractionSystem;
class MuseumWorld;

struct UpdateContext {
    float dt;                 // seconds, already clamped
    double time;              // museum clock in seconds
    Microsoft::Xna::Framework::Vector3 playerPosition;
    Microsoft::Xna::Framework::Vector3 playerForward;
    bool playerInRoom;        // the visitor is standing in this exhibit's room
    bool nearby;              // this room or a neighbour: worth simulating
};

/**
 * One room's exhibit. The base class carries no behaviour: each room overrides the
 * few hooks it needs. Exhibits own their dynamic objects (models, particles, render
 * targets); the static architecture they add goes through RoomBuilder.
 */
class Exhibit {
public:
    Exhibit(const RoomDef& room, Services& services) : room_(room), services_(services) {}
    virtual ~Exhibit() = default;
    Exhibit(const Exhibit&) = delete;
    Exhibit& operator=(const Exhibit&) = delete;

    [[nodiscard]] const RoomDef& Room() const { return room_; }

    /** Adds pedestals, signs, colliders and lights. Called once while the world is built. */
    virtual void BuildStatic(RoomBuilder&) {}
    /** Loads models, creates render targets and registers interactables. Called once after BuildStatic. */
    virtual void Load(InteractionSystem&, MuseumWorld&) {}
    virtual void Update(const UpdateContext&) {}
    /** Renders into render targets. Runs before the main scene, only when `nearby`. */
    virtual void RenderOffscreen(const DrawContext&) {}
    virtual void DrawOpaque(const DrawContext&) {}
    virtual void DrawTransparent(const DrawContext&) {}
    /** Called once per frame after the 3D scene, for overlays that need SpriteBatch. */
    virtual void DrawOverlay() {}
    virtual void OnEnter() {}
    virtual void OnLeave() {}

protected:
    [[nodiscard]] Services& S() { return services_; }
    [[nodiscard]] const RoomDef& R() const { return room_; }

private:
    const RoomDef& room_;
    Services& services_;
};
