#include "World/MuseumWorld.hpp"

#include <algorithm>
#include <chrono>
#include <queue>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitRegistry.hpp"
#include "Exhibits/RoomCard.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SignFactory.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {
// Distances are measured on the floor plane, from the visitor to the edge of a cell.
constexpr float kBuildDistance = 75.0f;     // cells this close (and within two doors) have their architecture built
constexpr float kLoadDistance = 45.0f;      // cells this close (and within one door) load their exhibit
constexpr float kKeepDistance = 130.0f;     // built cells beyond this, or beyond kKeepDepth doors, are destroyed
constexpr int kBuildDepth = 2;
constexpr int kLoadDepth = 1;
constexpr int kKeepDepth = 4;
constexpr int kMaxVisibleDepth = 3;
// Exhibits farther from the visitor than this are not simulated.
constexpr float kSimulationDistance = 34.0f;

using Clock = std::chrono::steady_clock;
double MillisecondsSince(Clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}
}  // namespace

MuseumWorld::MuseumWorld(Services& services, const MuseumPlan& museum) : services_(services), museum_(museum)
{
    instances_.resize(museum_.rooms.size());
    BuildExterior();
}

MuseumWorld::~MuseumWorld()
{
    for (std::size_t i = 0; i < instances_.size(); ++i)
        if (instances_[i]) DestroyCell(static_cast<int>(i));
}

void MuseumWorld::BuildExterior()
{
    // A wide lawn under the whole building so windows and courtyards see ground, not void,
    // and a paved forecourt in front of the entrance.
    MeshSet meshes;
    meshes.Opaque("grass").AddPatch(Vector3(-400, -0.05f, 300), Vector3(800, 0, 0), Vector3(0, 0, -900), 3.0f,
                                    Vector3(0.9f, 0.95f, 0.9f), 50.0f);
    if (!museum_.floors.empty() && museum_.floors.front().hub >= 0) {
        const RoomDef& hub = museum_.Room(museum_.floors.front().hub);
        const float z0 = hub.centre.Z + hub.halfD + kWallThickness;
        meshes.Opaque("cobble").AddPatch(Vector3(-16, -0.02f, z0 + 30), Vector3(32, 0, 0), Vector3(0, 0, -30), 2.5f,
                                         Vector3(0.9f, 0.9f, 0.9f), 2.0f);
    }
    exterior_.bounds = BoundingBox(Vector3(-400, -1, -600), Vector3(400, 0.1f, 300));
    int triangles = 0;
    Finalize(exterior_, meshes, {}, Vector3(0.95f, 0.95f, 0.95f), triangles);
}

InteractionSystem* MuseumWorld::CurrentInteraction()
{
    if (current_ < 0 || !instances_[static_cast<std::size_t>(current_)]) return nullptr;
    return &instances_[static_cast<std::size_t>(current_)]->interaction;
}

Exhibit* MuseumWorld::FindExhibit(int cell) const
{
    if (cell < 0 || cell >= static_cast<int>(instances_.size()) || !instances_[static_cast<std::size_t>(cell)]) return nullptr;
    return instances_[static_cast<std::size_t>(cell)]->exhibit.get();
}

void MuseumWorld::SetCellBrightness(int cell, float brightness)
{
    if (cell >= 0 && instances_[static_cast<std::size_t>(cell)]) instances_[static_cast<std::size_t>(cell)]->zone.brightness = brightness;
}

float MuseumWorld::DistanceTo(int cell, const Vector3& p) const
{
    const RoomDef& r = museum_.Room(cell);
    const float dx = std::max(0.0f, std::abs(p.X - r.centre.X) - r.halfW);
    const float dz = std::max(0.0f, std::abs(p.Z - r.centre.Z) - r.halfD);
    return std::sqrt(dx * dx + dz * dz);
}

void MuseumWorld::BuildCell(int cell)
{
    const auto t0 = Clock::now();
    const RoomDef& room = museum_.Room(cell);
    auto inst = std::make_unique<RoomInstance>();
    inst->index = cell;
    services_.signs->SetScope(&inst->textures);

    MeshSet meshes;
    std::vector<BakedLight> lights;
    BuildCellShell(room, *inst, meshes, lights);
    inst->exhibit = CreateExhibit(room, services_);
    if (inst->exhibit) {
        RoomBuilder builder(room, meshes, inst->colliders, lights, services_);
        inst->exhibit->DoBuildStatic(builder);
    }
    if (room.kind == CellKind::Room) {
        RoomBuilder builder(room, meshes, inst->colliders, lights, services_);
        AddRoomCard(builder, services_, room);
    }
    BoundingBox b = room.Interior();
    b.Min -= Vector3(kWallThickness, 0.1f, kWallThickness);
    b.Max += Vector3(kWallThickness, 0.1f, kWallThickness);
    inst->zone.bounds = b;
    Finalize(inst->zone, meshes, lights, room.theme.ambient, inst->triangles);
    services_.signs->SetScope(nullptr);

    stats_.triangles += inst->triangles;
    ++stats_.cellsBuilt;
    ++stats_.cellsBuiltTotal;
    instances_[static_cast<std::size_t>(cell)] = std::move(inst);
    collisionDirty_ = true;
    stats_.lastBuildMilliseconds = MillisecondsSince(t0);
}

void MuseumWorld::LoadCell(int cell)
{
    RoomInstance& inst = *instances_[static_cast<std::size_t>(cell)];
    if (inst.loaded) return;
    inst.loaded = true;
    ++stats_.cellsLoaded;
    if (!inst.exhibit) return;
    services_.signs->SetScope(&inst.textures);
    inst.exhibit->DoLoad(inst.interaction, *this);
    services_.signs->SetScope(nullptr);
}

void MuseumWorld::DestroyCell(int cell)
{
    std::unique_ptr<RoomInstance> inst = std::move(instances_[static_cast<std::size_t>(cell)]);
    if (!inst) return;
    stats_.triangles -= inst->triangles;
    --stats_.cellsBuilt;
    if (inst->loaded) --stats_.cellsLoaded;
    ++stats_.cellsDestroyedTotal;
    inst->exhibit.reset();   // releases the exhibit's GPU resources and audio voices before its textures go
    for (const std::string& name : inst->textures) services_.textures->Unregister(name);
    collisionDirty_ = true;
}

void MuseumWorld::RebuildCollision()
{
    collision_ = CollisionWorld();
    for (const auto& inst : instances_)
        if (inst)
            for (const BoundingBox& box : inst->colliders.Boxes()) collision_.AddBox(box);
    collisionDirty_ = false;
}

void MuseumWorld::Activate(int cell, const Vector3& position, bool synchronous)
{
    if (cell < 0) return;
    // Breadth-first over the door graph.
    std::vector<int> depth(museum_.rooms.size(), -1);
    std::vector<int> order;
    std::queue<int> open;
    depth[static_cast<std::size_t>(cell)] = 0;
    open.push(cell);
    while (!open.empty()) {
        const int c = open.front();
        open.pop();
        order.push_back(c);
        const int d = depth[static_cast<std::size_t>(c)];
        if (d >= kKeepDepth) continue;
        for (const DoorDef& door : museum_.Room(c).doors) {
            if (door.to >= 0 && depth[static_cast<std::size_t>(door.to)] < 0) {
                depth[static_cast<std::size_t>(door.to)] = d + 1;
                open.push(door.to);
            }
        }
    }

    // Release what is no longer needed first, so peak memory stays flat while the visitor walks.
    for (std::size_t i = 0; i < instances_.size(); ++i) {
        if (!instances_[i]) continue;
        const int d = depth[i];
        if (d < 0 || d > kKeepDepth || DistanceTo(static_cast<int>(i), position) > kKeepDistance) DestroyCell(static_cast<int>(i));
    }

    int budget = synchronous ? 1 << 20 : 1;
    for (const int c : order) {
        const int d = depth[static_cast<std::size_t>(c)];
        const float dist = DistanceTo(c, position);
        const bool wantBuilt = d <= kBuildDepth && dist < kBuildDistance;
        const bool wantLoaded = d <= kLoadDepth && dist < kLoadDistance;
        const bool essential = d <= kLoadDepth;   // the cell and its direct neighbours must exist at once
        if (wantBuilt && !instances_[static_cast<std::size_t>(c)] && (essential || budget > 0)) {
            BuildCell(c);
            if (!essential) --budget;
        }
        if (wantLoaded && instances_[static_cast<std::size_t>(c)] && !instances_[static_cast<std::size_t>(c)]->loaded &&
            (c == cell || budget > 0)) {
            LoadCell(c);
            if (c != cell) --budget;
        }
    }
    if (collisionDirty_) RebuildCollision();
}

void MuseumWorld::Arrive(int cell)
{
    current_ = cell;
    const RoomDef& r = museum_.Room(cell);
    Activate(cell, r.centre, true);
    if (Exhibit* e = FindExhibit(cell)) e->OnEnter();
}

void MuseumWorld::Update(float dt, double time, const Vector3& playerPosition, const Vector3& playerForward)
{
    const int cell = museum_.CellAt(playerPosition, current_);
    if (cell != current_) {
        if (Exhibit* e = FindExhibit(current_)) e->OnLeave();
        current_ = cell;
        if (Exhibit* e = FindExhibit(current_)) e->OnEnter();
    }
    // Between cells (outside the building) keep activating around the last cell.
    if (current_ >= 0) Activate(current_, playerPosition, false);

    for (auto& inst : instances_) {
        if (!inst || !inst->loaded || !inst->exhibit) continue;
        const float dist = DistanceTo(inst->index, playerPosition);
        UpdateContext ctx{dt, time, playerPosition, playerForward, inst->index == current_, dist < kSimulationDistance};
        inst->exhibit->Update(ctx);
    }
}

void MuseumWorld::CollectVisible(const DrawContext& ctx, std::vector<int>& out) const
{
    out.clear();
    const int start = museum_.CellAt(ctx.view.eye, -1);
    if (start < 0) {
        // Outside any cell (the forecourt, a camera in the lawn): everything built that is in view.
        for (const auto& inst : instances_)
            if (inst && ctx.view.frustum.Intersects(inst->zone.bounds)) out.push_back(inst->index);
        return;
    }
    std::vector<int> depth(museum_.rooms.size(), -1);
    std::queue<int> open;
    depth[static_cast<std::size_t>(start)] = 0;
    open.push(start);
    while (!open.empty()) {
        const int c = open.front();
        open.pop();
        out.push_back(c);
        const int d = depth[static_cast<std::size_t>(c)];
        if (d >= kMaxVisibleDepth) continue;
        const RoomDef& room = museum_.Room(c);
        for (const DoorDef& door : room.doors) {
            if (door.to < 0 || depth[static_cast<std::size_t>(door.to)] >= 0) continue;
            if (!instances_[static_cast<std::size_t>(door.to)]) continue;
            // The doorway has to be in view for what lies behind it to be.
            BoundingBox portal = room.DoorBounds(door);
            portal.Min -= Vector3(0.1f, 0.1f, 0.1f);
            portal.Max += Vector3(0.1f, 0.1f, 0.1f);
            if (!ctx.view.frustum.Intersects(portal)) continue;
            depth[static_cast<std::size_t>(door.to)] = d + 1;
            open.push(door.to);
        }
    }
}

void MuseumWorld::RenderOffscreen(const DrawContext& ctx)
{
    const Vector3 eye = ctx.view.eye;
    for (auto& inst : instances_) {
        if (!inst || !inst->loaded || !inst->exhibit) continue;
        if (DistanceTo(inst->index, eye) < kSimulationDistance) inst->exhibit->DoRenderOffscreen(ctx);
    }
}

void MuseumWorld::Draw(const DrawContext& ctx)
{
    GraphicsDevice& device = ctx.device;
    BasicEffect& fx = ctx.fx.World();

    const auto drawBatches = [&](const Zone& zone, const std::vector<DrawBatch>& batches) {
        const float b = zone.brightness;
        for (const DrawBatch& batch : batches) {
            fx.setTextureProperty(batch.texture);
            fx.setDiffuseColorProperty(Vector3(b, b, b));
            EffectBank::Apply(fx);
            batch.mesh->Draw(device);
        }
    };

    std::vector<int> visible;
    CollectVisible(ctx, visible);
    if (!ctx.offscreen) {
        visible_ = visible;
        stats_.cellsDrawn = static_cast<int>(visible.size());
    }

    RenderStates::SetOpaque3D(device);
    ctx.fx.SetCamera(ctx.view.view, ctx.view.projection);
    fx.setWorldProperty(Matrix::getIdentityProperty());
    if (ctx.view.frustum.Intersects(exterior_.bounds)) drawBatches(exterior_, exterior_.opaque);
    for (const int c : visible) {
        const RoomInstance* inst = instances_[static_cast<std::size_t>(c)].get();
        if (inst) drawBatches(inst->zone, inst->zone.opaque);
    }
    for (const int c : visible) {
        const RoomInstance* inst = instances_[static_cast<std::size_t>(c)].get();
        if (inst && inst->loaded && inst->exhibit) inst->exhibit->DoDrawOpaque(ctx);
    }

    // Transparent surfaces last, no depth writes, so glass and particles blend over everything solid.
    RenderStates::SetTransparent3D(device, BlendState::NonPremultiplied);
    fx.setWorldProperty(Matrix::getIdentityProperty());
    for (const int c : visible) {
        const RoomInstance* inst = instances_[static_cast<std::size_t>(c)].get();
        if (inst && !inst->zone.transparent.empty()) drawBatches(inst->zone, inst->zone.transparent);
    }
    for (const int c : visible) {
        const RoomInstance* inst = instances_[static_cast<std::size_t>(c)].get();
        if (inst && inst->loaded && inst->exhibit) inst->exhibit->DrawTransparent(ctx);
    }
}

void MuseumWorld::DrawOverlay()
{
    // Overlays are screen-space (labels, markers) and know nothing about walls, so only the room the
    // visitor stands in draws them: otherwise a neighbour's labels would float over this room.
    if (current_ < 0) return;
    const RoomInstance* inst = instances_[static_cast<std::size_t>(current_)].get();
    if (inst && inst->loaded && inst->exhibit) inst->exhibit->DrawOverlay();
}
