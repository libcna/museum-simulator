// Construction half of MuseumWorld: turns the RoomDef table into geometry.
#include <algorithm>
#include <cmath>
#include <optional>

#include "Diagnostics/Log.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/MuseumWorld.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;

namespace {

constexpr float kT = kWallThickness;
const Vector3 kWhite(1.0f, 1.0f, 1.0f);
const Vector3 kTrim(0.42f, 0.30f, 0.20f);

enum Side { N = 0, S = 1, E = 2, W = 3 };

const RoomDef* NeighbourAt(const RoomDef& r, int dc, int dr)
{
    for (const RoomDef& o : AllRooms())
        if (o.col == r.col + dc && o.row == r.row + dr) return &o;
    return nullptr;
}

// Adds one straight run of wall (inner face toward the room) covering [a0, a1] along the wall,
// between heights y0 and y1, plus skirting and cornice when it reaches the floor/ceiling.
void AddWallRun(const RoomDef& r, MeshSet& meshes, CollisionWorld& collision, Side side, float a0, float a1, float y0,
                float y1, bool collide, bool trim)
{
    const Vector3 c = r.Center();
    const float hw = r.halfW;
    const float hd = r.halfD;
    Vector3 lo, hi;
    BoxFaces faces;
    switch (side) {
        case N: lo = Vector3(c.X + a0, y0, c.Z - hd - kT); hi = Vector3(c.X + a1, y1, c.Z - hd); faces = BoxFaces::PosZ | BoxFaces::PosX | BoxFaces::NegX | BoxFaces::PosY; break;
        case S: lo = Vector3(c.X + a0, y0, c.Z + hd); hi = Vector3(c.X + a1, y1, c.Z + hd + kT); faces = BoxFaces::NegZ | BoxFaces::PosX | BoxFaces::NegX | BoxFaces::PosY; break;
        case W: lo = Vector3(c.X - hw - kT, y0, c.Z + a0); hi = Vector3(c.X - hw, y1, c.Z + a1); faces = BoxFaces::PosX | BoxFaces::PosZ | BoxFaces::NegZ | BoxFaces::PosY; break;
        default: lo = Vector3(c.X + hw, y0, c.Z + a0); hi = Vector3(c.X + hw + kT, y1, c.Z + a1); faces = BoxFaces::NegX | BoxFaces::PosZ | BoxFaces::NegZ | BoxFaces::PosY; break;
    }
    meshes.Opaque(r.wallTex).AddBox(lo, hi, 2.5f, r.wallTint, faces, 1.0f);
    if (collide) collision.AddBox(BoundingBox(lo, hi));
    if (!trim) return;

    // Skirting board along the visible part of the run, a cornice below the ceiling.
    const float clipLo = (side == N || side == S) ? std::max(a0, -hw) : std::max(a0, -hd);
    const float clipHi = (side == N || side == S) ? std::min(a1, hw) : std::min(a1, hd);
    if (clipHi - clipLo < 0.05f) return;
    const auto strip = [&](float ya, float yb, float depth, const Vector3& tint) {
        Vector3 slo, shi;
        switch (side) {
            case N: slo = Vector3(c.X + clipLo, ya, c.Z - hd); shi = Vector3(c.X + clipHi, yb, c.Z - hd + depth); break;
            case S: slo = Vector3(c.X + clipLo, ya, c.Z + hd - depth); shi = Vector3(c.X + clipHi, yb, c.Z + hd); break;
            case W: slo = Vector3(c.X - hw, ya, c.Z + clipLo); shi = Vector3(c.X - hw + depth, yb, c.Z + clipHi); break;
            default: slo = Vector3(c.X + hw - depth, ya, c.Z + clipLo); shi = Vector3(c.X + hw, yb, c.Z + clipHi); break;
        }
        meshes.Opaque("wood").AddBox(slo, shi, 1.0f, tint, BoxFaces::All & ~BoxFaces::NegY & ~BoxFaces::PosY, 2.0f);
    };
    if (y0 <= 0.001f) strip(0.0f, 0.16f, 0.04f, kTrim);
    if (!r.openRoof && y1 >= r.height - 0.001f) strip(r.height - 0.2f, r.height, 0.06f, Vector3(0.75f, 0.7f, 0.65f));
}

}  // namespace

void MuseumWorld::Finalize(Zone& zone, const MeshSet& meshes, const std::vector<BakedLight>& lights, const Vector3& ambient)
{
    const auto upload = [&](const std::map<std::string, MeshBuilder>& source, std::vector<DrawBatch>& out) {
        for (const auto& [textureName, builder] : source) {
            if (builder.Empty()) continue;
            DrawBatch batch;
            batch.texture = services_.textures->Get(textureName);
            batch.mesh = std::make_unique<StaticMesh>(*services_.device, builder.Bake(lights, ambient), builder.Indices(),
                                                       builder.Bounds());
            triangles_ += batch.mesh->TriangleCount();
            out.push_back(std::move(batch));
        }
    };
    upload(meshes.OpaqueMeshes(), zone.opaque);
    upload(meshes.TransparentMeshes(), zone.transparent);
}

void MuseumWorld::BuildRoomShell(const RoomDef& r, const bool doors[4], MeshSet& meshes, std::vector<BakedLight>& lights)
{
    const Vector3 c = r.Center();
    const float hw = r.halfW;
    const float hd = r.halfD;
    const float H = r.height;
    const bool lobbyGlass = (r.id == RoomId::Lobby);   // the street-side wall is a glass facade

    // Floor and ceiling.
    meshes.Opaque(r.floorTex).AddPatch(Vector3(c.X - hw, 0, c.Z + hd), Vector3(2 * hw, 0, 0), Vector3(0, 0, -2 * hd), 2.5f,
                                       r.floorTint, 1.0f);
    if (!r.openRoof)
        meshes.Opaque(r.ceilTex).AddPatch(Vector3(c.X - hw, H, c.Z - hd), Vector3(2 * hw, 0, 0), Vector3(0, 0, 2 * hd),
                                          3.0f, Vector3(0.95f, 0.95f, 0.95f), 1.0f);

    const float halfDoor = kDoorWidth * 0.5f;
    for (int s = 0; s < 4; ++s) {
        const Side side = static_cast<Side>(s);
        const float L = (side == N || side == S) ? hw + kT : hd;
        if (lobbyGlass && side == S) {
            // Low sill wall, glass above it, thin mullions, and one solid collider for the whole facade.
            AddWallRun(r, meshes, collision_, S, -L, L, 0.0f, 1.0f, true, true);
            AddWallRun(r, meshes, collision_, S, -L, L, H - 0.6f, H, false, false);
            // Glass faces into the room (-Z); drawn from both sides, so it needs no back face.
            const float yb = 1.0f;
            const float yt = H - 0.6f;
            const Vector3 origin(c.X + hw, yb, c.Z + hd - 0.02f);
            meshes.Transparent("glass").AddPatch(origin, Vector3(-2 * hw, 0, 0), Vector3(0, yt - yb, 0), 2.0f,
                                                 Vector3(0.9f, 1.0f, 1.0f), 2.0f);
            for (int i = -2; i <= 2; ++i)
                meshes.Opaque("metal").AddBox(Vector3(c.X + static_cast<float>(i) * hw * 0.5f - 0.06f, yb, c.Z + hd - 0.12f),
                                              Vector3(c.X + static_cast<float>(i) * hw * 0.5f + 0.06f, yt, c.Z + hd), 1.0f,
                                              Vector3(0.5f, 0.5f, 0.55f), BoxFaces::All, 1.0f);
            continue;
        }
        if (doors[s]) {
            // Floor across the thickness of the wall, so the doorway has a threshold instead of a hole.
            const float dd = halfDoor;
            Vector3 o, U, V;
            switch (side) {
                case N: o = Vector3(c.X - dd, 0, c.Z - hd); U = Vector3(2 * dd, 0, 0); V = Vector3(0, 0, -kT); break;
                case S: o = Vector3(c.X - dd, 0, c.Z + hd + kT); U = Vector3(2 * dd, 0, 0); V = Vector3(0, 0, -kT); break;
                case W: o = Vector3(c.X - hw - kT, 0, c.Z - dd); U = Vector3(0, 0, 2 * dd); V = Vector3(kT, 0, 0); break;
                default: o = Vector3(c.X + hw, 0, c.Z - dd); U = Vector3(0, 0, 2 * dd); V = Vector3(kT, 0, 0); break;
            }
            meshes.Opaque("carpet").AddPatch(o, U, V, 2.0f, Vector3(0.9f, 0.85f, 0.85f), 1.0f);
            AddWallRun(r, meshes, collision_, side, -L, -halfDoor, 0.0f, H, true, true);
            AddWallRun(r, meshes, collision_, side, halfDoor, L, 0.0f, H, true, true);
            // Lintel above the opening: no collision, it is above head height.
            AddWallRun(r, meshes, collision_, side, -halfDoor, halfDoor, kDoorHeight, H, false, false);
        } else {
            AddWallRun(r, meshes, collision_, side, -L, L, 0.0f, H, true, true);
        }
    }

    // Ceiling fixtures: an emissive panel and a baked point light under each.
    if (!r.openRoof) {
        std::vector<std::pair<float, float>> spots = {{-hw * 0.5f, -hd * 0.5f}, {hw * 0.5f, -hd * 0.5f},
                                                      {-hw * 0.5f, hd * 0.5f}, {hw * 0.5f, hd * 0.5f}};
        if (hw >= 8.5f) spots.emplace_back(0.0f, 0.0f);
        const float radius = std::max(hw, hd) * 1.25f + 3.0f;
        for (const auto& [x, z] : spots) {
            meshes.Opaque("white").AddBox(Vector3(c.X + x - 0.7f, H - 0.07f, c.Z + z - 0.7f),
                                          Vector3(c.X + x + 0.7f, H - 0.005f, c.Z + z + 0.7f), 1.0f,
                                          Vector3(1.0f, 0.96f, 0.86f), BoxFaces::NoTop, 2.0f, 1.0f);
            BakedLight light;
            light.position = Vector3(c.X + x, H - 0.8f, c.Z + z);
            light.color = Vector3(1.0f, 0.93f, 0.80f);
            light.intensity = r.lightIntensity;
            light.radius = radius;
            lights.push_back(light);
        }
    }
}

void MuseumWorld::BuildCorridor(const RoomDef& a, const RoomDef& b)
{
    const float dw = kDoorWidth;
    const float Hc = kCorridorHeight;
    MeshSet meshes;
    std::vector<BakedLight> lights;
    const bool alongX = (a.row == b.row);
    Zone zone;
    zone.name = std::string("corridor ") + a.key + "-" + b.key;

    const Vector3 trim = Vector3(0.9f, 0.9f, 0.88f);
    if (alongX) {
        const float x0 = a.Center().X + a.halfW + kT;
        const float x1 = b.Center().X - b.halfW - kT;
        const float zc = a.Center().Z;
        const float L = x1 - x0;
        meshes.Opaque("carpet").AddPatch(Vector3(x0, 0, zc + dw / 2), Vector3(L, 0, 0), Vector3(0, 0, -dw), 2.0f, Vector3(0.9f, 0.85f, 0.85f), 1.0f);
        meshes.Opaque("plaster").AddPatch(Vector3(x0, Hc, zc - dw / 2), Vector3(L, 0, 0), Vector3(0, 0, dw), 3.0f, Vector3(0.9f, 0.9f, 0.9f), 1.0f);
        meshes.Opaque("plaster").AddPatch(Vector3(x0, 0, zc - dw / 2), Vector3(L, 0, 0), Vector3(0, Hc, 0), 2.5f, trim, 1.0f);
        meshes.Opaque("plaster").AddPatch(Vector3(x1, 0, zc + dw / 2), Vector3(-L, 0, 0), Vector3(0, Hc, 0), 2.5f, trim, 1.0f);
        collision_.AddBox(BoundingBox(Vector3(x0, 0, zc - dw / 2 - 0.3f), Vector3(x1, Hc, zc - dw / 2)));
        collision_.AddBox(BoundingBox(Vector3(x0, 0, zc + dw / 2), Vector3(x1, Hc, zc + dw / 2 + 0.3f)));
        const int fixtures = std::max(1, static_cast<int>(L / 5.0f));
        for (int i = 0; i < fixtures; ++i) {
            const float x = x0 + L * (static_cast<float>(i) + 0.5f) / static_cast<float>(fixtures);
            meshes.Opaque("white").AddBox(Vector3(x - 0.4f, Hc - 0.06f, zc - 0.3f), Vector3(x + 0.4f, Hc - 0.005f, zc + 0.3f),
                                          1.0f, Vector3(1.0f, 0.96f, 0.86f), BoxFaces::NoTop, 2.0f, 1.0f);
            lights.push_back({Vector3(x, Hc - 0.7f, zc), Vector3(1.0f, 0.93f, 0.80f), 0.9f, 6.0f});
        }
        zone.bounds = BoundingBox(Vector3(x0, 0, zc - dw / 2 - 0.3f), Vector3(x1, Hc, zc + dw / 2 + 0.3f));
    } else {
        const float xc = a.Center().X;
        const float z0 = a.Center().Z + a.halfD + kT;
        const float z1 = b.Center().Z - b.halfD - kT;
        const float L = z1 - z0;
        meshes.Opaque("carpet").AddPatch(Vector3(xc - dw / 2, 0, z1), Vector3(dw, 0, 0), Vector3(0, 0, -L), 2.0f, Vector3(0.9f, 0.85f, 0.85f), 1.0f);
        meshes.Opaque("plaster").AddPatch(Vector3(xc - dw / 2, Hc, z0), Vector3(dw, 0, 0), Vector3(0, 0, L), 3.0f, Vector3(0.9f, 0.9f, 0.9f), 1.0f);
        meshes.Opaque("plaster").AddPatch(Vector3(xc - dw / 2, 0, z1), Vector3(0, 0, -L), Vector3(0, Hc, 0), 2.5f, trim, 1.0f);
        meshes.Opaque("plaster").AddPatch(Vector3(xc + dw / 2, 0, z0), Vector3(0, 0, L), Vector3(0, Hc, 0), 2.5f, trim, 1.0f);
        collision_.AddBox(BoundingBox(Vector3(xc - dw / 2 - 0.3f, 0, z0), Vector3(xc - dw / 2, Hc, z1)));
        collision_.AddBox(BoundingBox(Vector3(xc + dw / 2, 0, z0), Vector3(xc + dw / 2 + 0.3f, Hc, z1)));
        const int fixtures = std::max(1, static_cast<int>(L / 5.0f));
        for (int i = 0; i < fixtures; ++i) {
            const float z = z0 + L * (static_cast<float>(i) + 0.5f) / static_cast<float>(fixtures);
            meshes.Opaque("white").AddBox(Vector3(xc - 0.3f, Hc - 0.06f, z - 0.4f), Vector3(xc + 0.3f, Hc - 0.005f, z + 0.4f),
                                          1.0f, Vector3(1.0f, 0.96f, 0.86f), BoxFaces::NoTop, 2.0f, 1.0f);
            lights.push_back({Vector3(xc, Hc - 0.7f, z), Vector3(1.0f, 0.93f, 0.80f), 0.9f, 6.0f});
        }
        zone.bounds = BoundingBox(Vector3(xc - dw / 2 - 0.3f, 0, z0), Vector3(xc + dw / 2 + 0.3f, Hc, z1));
    }
    Finalize(zone, meshes, lights, Vector3(0.28f, 0.27f, 0.26f));
    zones_.push_back(std::move(zone));
}

void MuseumWorld::BuildExterior()
{
    // A wide lawn under the whole building so the lobby windows and the courtyard see ground, not void.
    MeshSet meshes;
    meshes.Opaque("grass").AddPatch(Vector3(-160, -0.05f, 160), Vector3(320, 0, 0), Vector3(0, 0, -320), 3.0f,
                                    Vector3(0.9f, 0.95f, 0.9f), 40.0f);
    // A paved forecourt in front of the entrance.
    meshes.Opaque("cobble").AddPatch(Vector3(-14, -0.02f, 34), Vector3(28, 0, 0), Vector3(0, 0, -28), 2.5f,
                                     Vector3(0.9f, 0.9f, 0.9f), 2.0f);
    Zone zone;
    zone.name = "exterior";
    zone.bounds = BoundingBox(Vector3(-160, -1, -160), Vector3(160, 0.1f, 160));
    Finalize(zone, meshes, {}, Vector3(0.95f, 0.95f, 0.95f));
    zones_.push_back(std::move(zone));
}

void MuseumWorld::Build()
{
    BuildExterior();

    // Rooms first (each asks its exhibit for furniture), then the corridors that join them.
    for (const RoomDef& room : AllRooms()) {
        const bool doors[4] = {
            NeighbourAt(room, 0, -1) != nullptr,   // N
            NeighbourAt(room, 0, +1) != nullptr && room.id != RoomId::Lobby,  // S (lobby faces the street)
            NeighbourAt(room, +1, 0) != nullptr,   // E
            NeighbourAt(room, -1, 0) != nullptr,   // W
        };
        MeshSet meshes;
        std::vector<BakedLight> lights;
        BuildRoomShell(room, doors, meshes, lights);

        std::unique_ptr<Exhibit> exhibit = CreateExhibit(room, services_);
        if (exhibit) {
            RoomBuilder builder(room, meshes, collision_, lights, services_);
            exhibit->BuildStatic(builder);
        }

        Zone zone;
        zone.name = room.title;
        zone.room = room.id;
        BoundingBox b = room.Interior();
        b.Min -= Vector3(kT, 0.1f, kT);
        b.Max += Vector3(kT, 0.1f, kT);
        zone.bounds = b;
        Finalize(zone, meshes, lights, room.ambient);
        zones_.push_back(std::move(zone));
        if (exhibit) exhibits_.push_back(std::move(exhibit));
    }

    for (const RoomDef& a : AllRooms()) {
        if (const RoomDef* east = NeighbourAt(a, +1, 0)) BuildCorridor(a, *east);
        if (const RoomDef* south = NeighbourAt(a, 0, +1)) {
            if (a.id != RoomId::Lobby) BuildCorridor(a, *south);
        }
    }

    for (auto& exhibit : exhibits_) exhibit->Load(interaction_, *this);
    Museum::Log("world", "built %zu zones, %d triangles, %zu colliders, %zu interactables", zones_.size(), triangles_,
                collision_.Count(), interaction_.Count());
}
