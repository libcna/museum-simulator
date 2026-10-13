#include "World/MuseumWorld.hpp"

#include <algorithm>

#include "Rendering/EffectBank.hpp"
#include "Rendering/RenderStates.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {
// Exhibits in rooms farther than this from the visitor are not simulated.
constexpr float kSimulationDistance = 34.0f;
}

MuseumWorld::MuseumWorld(Services& services) : services_(services) {}
MuseumWorld::~MuseumWorld() = default;

std::optional<RoomId> MuseumWorld::RoomAt(const Vector3& p) const
{
    for (const RoomDef& room : AllRooms()) {
        const Vector3 c = room.Center();
        if (std::abs(p.X - c.X) <= room.halfW && std::abs(p.Z - c.Z) <= room.halfD) return room.id;
    }
    return std::nullopt;
}

Exhibit* MuseumWorld::FindExhibit(RoomId id) const
{
    for (const auto& e : exhibits_)
        if (e->Room().id == id) return e.get();
    return nullptr;
}

void MuseumWorld::SetRoomBrightness(RoomId room, float brightness)
{
    for (Zone& z : zones_)
        if (z.room && *z.room == room) z.brightness = brightness;
}

void MuseumWorld::Update(float dt, double time, const Vector3& playerPosition, const Vector3& playerForward)
{
    const std::optional<RoomId> room = RoomAt(playerPosition);
    if (room != currentRoom_) {
        if (currentRoom_) if (Exhibit* e = FindExhibit(*currentRoom_)) e->OnLeave();
        currentRoom_ = room;
        if (currentRoom_) if (Exhibit* e = FindExhibit(*currentRoom_)) e->OnEnter();
    }
    for (auto& exhibit : exhibits_) {
        const Vector3 c = exhibit->Room().Center();
        const float dist = Vector3::Distance(Vector3(c.X, 0, c.Z), Vector3(playerPosition.X, 0, playerPosition.Z));
        UpdateContext ctx{dt, time, playerPosition, playerForward,
                          currentRoom_ && *currentRoom_ == exhibit->Room().id, dist < kSimulationDistance};
        exhibit->Update(ctx);
    }
}

void MuseumWorld::RenderOffscreen(const DrawContext& ctx)
{
    const Vector3 eye = ctx.view.eye;
    for (auto& exhibit : exhibits_) {
        const Vector3 c = exhibit->Room().Center();
        if (Vector3::Distance(Vector3(c.X, 0, c.Z), Vector3(eye.X, 0, eye.Z)) < kSimulationDistance)
            exhibit->RenderOffscreen(ctx);
    }
}

void MuseumWorld::Draw(const DrawContext& ctx)
{
    GraphicsDevice& device = ctx.device;
    BasicEffect& fx = ctx.fx.World();
    int zonesDrawn = 0;

    const auto drawBatches = [&](const Zone& zone, const std::vector<DrawBatch>& batches) {
        const float b = zone.brightness;
        for (const DrawBatch& batch : batches) {
            fx.setTextureProperty(batch.texture);
            fx.setDiffuseColorProperty(Vector3(b, b, b));
            EffectBank::Apply(fx);
            batch.mesh->Draw(device);
        }
    };

    RenderStates::SetOpaque3D(device);
    ctx.fx.SetCamera(ctx.view.view, ctx.view.projection);
    fx.setWorldProperty(Matrix::getIdentityProperty());
    for (const Zone& zone : zones_) {
        if (!ctx.view.frustum.Intersects(zone.bounds)) continue;
        ++zonesDrawn;
        drawBatches(zone, zone.opaque);
    }

    if (!ctx.offscreen) zonesDrawn_ = zonesDrawn;   // statistics describe the visitor's view only

    for (auto& exhibit : exhibits_) exhibit->DrawOpaque(ctx);

    // Transparent surfaces last, no depth writes, so glass and particles blend over everything solid.
    RenderStates::SetTransparent3D(device, BlendState::NonPremultiplied);
    fx.setWorldProperty(Matrix::getIdentityProperty());
    for (const Zone& zone : zones_) {
        if (zone.transparent.empty() || !ctx.view.frustum.Intersects(zone.bounds)) continue;
        drawBatches(zone, zone.transparent);
    }
    for (auto& exhibit : exhibits_) exhibit->DrawTransparent(ctx);
}

void MuseumWorld::DrawOverlay()
{
    for (auto& exhibit : exhibits_) exhibit->DrawOverlay();
}
