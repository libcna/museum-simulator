// Construction half of MuseumWorld: turns one RoomDef into baked, textured geometry plus colliders.
#include <algorithm>
#include <cmath>

#include "Diagnostics/Log.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/MuseumWorld.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;

namespace {

constexpr float kT = kWallThickness;
const Vector3 kTrim(0.42f, 0.30f, 0.20f);

// Adds one straight run of wall (inner face toward the room) covering [a0, a1] along the wall,
// between heights y0 and y1 above the floor, plus skirting and cornice when it reaches the floor/ceiling.
void AddWallRun(const RoomDef& r, MeshSet& meshes, CollisionWorld& collision, Wall side, float a0, float a1, float y0,
                float y1, bool collide, bool trim)
{
    const Vector3 c = r.centre;
    const float hw = r.halfW;
    const float hd = r.halfD;
    const bool alongX = (side == Wall::North || side == Wall::South);
    y0 += c.Y;
    y1 += c.Y;
    Vector3 lo, hi;
    BoxFaces faces;
    switch (side) {
        case Wall::North: lo = Vector3(c.X + a0, y0, c.Z - hd - kT); hi = Vector3(c.X + a1, y1, c.Z - hd); faces = BoxFaces::PosZ | BoxFaces::PosX | BoxFaces::NegX | BoxFaces::PosY; break;
        case Wall::South: lo = Vector3(c.X + a0, y0, c.Z + hd); hi = Vector3(c.X + a1, y1, c.Z + hd + kT); faces = BoxFaces::NegZ | BoxFaces::PosX | BoxFaces::NegX | BoxFaces::PosY; break;
        case Wall::West: lo = Vector3(c.X - hw - kT, y0, c.Z + a0); hi = Vector3(c.X - hw, y1, c.Z + a1); faces = BoxFaces::PosX | BoxFaces::PosZ | BoxFaces::NegZ | BoxFaces::PosY; break;
        default: lo = Vector3(c.X + hw, y0, c.Z + a0); hi = Vector3(c.X + hw + kT, y1, c.Z + a1); faces = BoxFaces::NegX | BoxFaces::PosZ | BoxFaces::NegZ | BoxFaces::PosY; break;
    }
    meshes.Opaque(r.theme.wallTex).AddBox(lo, hi, 2.5f, r.theme.wallTint, faces, 1.0f);
    if (collide) collision.AddBox(BoundingBox(lo, hi));
    if (!trim) return;

    // Skirting board along the visible part of the run, a cornice below the ceiling.
    const float clipLo = alongX ? std::max(a0, -hw) : std::max(a0, -hd);
    const float clipHi = alongX ? std::min(a1, hw) : std::min(a1, hd);
    if (clipHi - clipLo < 0.05f) return;
    const auto strip = [&](float ya, float yb, float depth, const Vector3& tint, const char* texture = "wood") {
        Vector3 slo, shi;
        switch (side) {
            case Wall::North: slo = Vector3(c.X + clipLo, c.Y + ya, c.Z - hd); shi = Vector3(c.X + clipHi, c.Y + yb, c.Z - hd + depth); break;
            case Wall::South: slo = Vector3(c.X + clipLo, c.Y + ya, c.Z + hd - depth); shi = Vector3(c.X + clipHi, c.Y + yb, c.Z + hd); break;
            case Wall::West: slo = Vector3(c.X - hw, c.Y + ya, c.Z + clipLo); shi = Vector3(c.X - hw + depth, c.Y + yb, c.Z + clipHi); break;
            default: slo = Vector3(c.X + hw - depth, c.Y + ya, c.Z + clipLo); shi = Vector3(c.X + hw, c.Y + yb, c.Z + clipHi); break;
        }
        meshes.Opaque(texture).AddBox(slo, shi, 1.0f, tint, BoxFaces::All & ~BoxFaces::NegY & ~BoxFaces::PosY, 2.0f);
    };
    if (y0 - c.Y <= 0.001f) strip(0.0f, 0.16f, 0.04f, kTrim);
    // Halls, concourses and galleries carry a band in the floor's accent colour, so the floor is recognisable at a glance.
    if (r.kind != CellKind::Room && r.kind != CellKind::Elevator && y0 - c.Y <= 0.001f) strip(1.05f, 1.3f, 0.05f, r.accent * 0.85f, "white");
    if (!r.openRoof && y1 - c.Y >= r.height - 0.001f) strip(r.height - 0.2f, r.height, 0.06f, Vector3(0.75f, 0.7f, 0.65f));
}

Wall WallFromName(const std::string& name, bool& ok)
{
    ok = true;
    if (name == "north") return Wall::North;
    if (name == "south") return Wall::South;
    if (name == "east") return Wall::East;
    if (name == "west") return Wall::West;
    ok = false;
    return Wall::North;
}

}  // namespace

void MuseumWorld::Finalize(Zone& zone, const MeshSet& meshes, const std::vector<BakedLight>& lights, const Vector3& ambient,
                           int& triangles)
{
    const auto upload = [&](const std::map<std::string, MeshBuilder>& source, std::vector<DrawBatch>& out) {
        for (const auto& [textureName, builder] : source) {
            if (builder.Empty()) continue;
            DrawBatch batch;
            batch.texture = services_.textures->Get(textureName);
            batch.mesh = std::make_unique<StaticMesh>(*services_.device, builder.Bake(lights, ambient), builder.Indices(),
                                                       builder.Bounds());
            triangles += batch.mesh->TriangleCount();
            out.push_back(std::move(batch));
        }
    };
    upload(meshes.OpaqueMeshes(), zone.opaque);
    upload(meshes.TransparentMeshes(), zone.transparent);
}

void MuseumWorld::BuildCellShell(const RoomDef& r, RoomInstance& inst, MeshSet& meshes, std::vector<BakedLight>& lights)
{
    CollisionWorld& collision = inst.colliders;
    const Vector3 c = r.centre;
    const float hw = r.halfW;
    const float hd = r.halfD;
    const float H = r.height;
    const Theme& th = r.theme;

    if (r.kind == CellKind::Stairs) {
        // A real two-flight switchback: visitors walk the lower ramp west, cross the
        // half landing, and return east on the upper ramp. The visible treads follow
        // the same analytic surfaces used by MuseumPlan::GroundHeightAt.
        AddWallRun(r, meshes, collision, Wall::North, -hw, hw, 0.0f, H, true, true);
        AddWallRun(r, meshes, collision, Wall::South, -hw, hw, 0.0f, H, true, true);
        AddWallRun(r, meshes, collision, Wall::West, -hd, hd, 0.0f, H, true, true);

        std::vector<const DoorDef*> doors;
        for (const DoorDef& d : r.doors)
            if (d.wall == Wall::East) doors.push_back(&d);
        std::sort(doors.begin(), doors.end(), [](const DoorDef* a, const DoorDef* b) { return a->along < b->along; });
        float cursor = -hd;
        for (const DoorDef* d : doors) {
            const float d0 = d->along - d->width * 0.5f;
            const float d1 = d->along + d->width * 0.5f;
            if (d0 > cursor) AddWallRun(r, meshes, collision, Wall::East, cursor, d0, 0.0f, H, true, true);
            if (d->floorOffset > 0.0f)
                AddWallRun(r, meshes, collision, Wall::East, d0, d1, 0.0f, d->floorOffset, true, false);
            if (d->floorOffset + kDoorHeight < H)
                AddWallRun(r, meshes, collision, Wall::East, d0, d1, d->floorOffset + kDoorHeight, H, true, false);
            meshes.Opaque("carpet").AddPatch(Vector3(c.X + hw, c.Y + d->floorOffset, c.Z + d0),
                                              Vector3(kT, 0, 0), Vector3(0, 0, d1 - d0), 2.0f,
                                              Vector3(0.9f, 0.85f, 0.85f), 1.0f);
            cursor = d1;
        }
        if (cursor < hd) AddWallRun(r, meshes, collision, Wall::East, cursor, hd, 0.0f, H, true, true);

        meshes.Opaque(th.ceilTex).AddPatch(Vector3(c.X - hw, c.Y + H, c.Z - hd), Vector3(2 * hw, 0, 0),
                                           Vector3(0, 0, 2 * hd), 3.0f, Vector3(0.95f, 0.95f, 0.95f), 1.0f);

        const float west = c.X - hw + kStairLandingDepth;
        const float east = c.X + hw - kStairLandingDepth;
        const float run = east - west;
        const float rise = kFloorPitch * 0.5f;
        const float laneHalf = kStairLaneWidth * 0.5f;
        const float lowerZ = c.Z + kStairLaneOffset;
        const float upperZ = c.Z - kStairLaneOffset;
        MeshBuilder& steps = meshes.Opaque(th.floorTex);

        // A base slab closes the shaft from the exterior lawn. The analytic walk surface is
        // smooth, while the visible boxes below make its two runs read as ordinary treads.
        steps.AddPatch(Vector3(c.X - hw, c.Y + 0.01f, c.Z + hd), Vector3(2 * hw, 0, 0),
                       Vector3(0, 0, -2 * hd), 1.2f, th.floorTint, 0.8f);
        steps.AddPatch(Vector3(c.X - hw, c.Y + rise, upperZ - laneHalf), Vector3(kStairLandingDepth, 0, 0),
                       Vector3(0, 0, (lowerZ + laneHalf) - (upperZ - laneHalf)), 1.2f, th.floorTint, 0.7f);
        steps.AddPatch(Vector3(east, c.Y, lowerZ - laneHalf), Vector3(kStairLandingDepth, 0, 0),
                       Vector3(0, 0, kStairLaneWidth), 1.2f, th.floorTint, 0.7f);
        steps.AddPatch(Vector3(east, c.Y + kFloorPitch, upperZ + laneHalf), Vector3(kStairLandingDepth, 0, 0),
                       Vector3(0, 0, -kStairLaneWidth), 1.2f, th.floorTint, 0.7f);

        // With the two-metre landings, 22 treads keep each rise near 20 cm and
        // leave enough flat space for a first-person visitor to make the turn.
        constexpr int treadCount = 22;
        const float tread = run / static_cast<float>(treadCount);
        const float treadRise = rise / static_cast<float>(treadCount);
        MeshBuilder& rails = meshes.Opaque("metal");
        for (int i = 0; i < treadCount; ++i) {
            const float lx1 = east - static_cast<float>(i) * tread;
            const float lx0 = lx1 - tread;
            const float ly = c.Y + static_cast<float>(i + 1) * treadRise;
            steps.AddBox(Vector3(lx0, c.Y, lowerZ - laneHalf), Vector3(lx1, ly, lowerZ + laneHalf), 1.2f,
                         th.floorTint, BoxFaces::PosY | BoxFaces::PosX, 0.7f);
            const float ux0 = west + static_cast<float>(i) * tread;
            const float ux1 = ux0 + tread;
            const float uy = c.Y + rise + static_cast<float>(i + 1) * treadRise;
            steps.AddBox(Vector3(ux0, c.Y + rise, upperZ - laneHalf), Vector3(ux1, uy, upperZ + laneHalf), 1.2f,
                         th.floorTint, BoxFaces::PosY | BoxFaces::NegX, 0.7f);
            if (i % 3 == 0) {
                for (const float z : {lowerZ - laneHalf, lowerZ + laneHalf})
                    rails.AddBox(Vector3(lx0, ly, z - 0.035f), Vector3(lx0 + 0.07f, ly + 0.92f, z + 0.035f),
                                 1.0f, Vector3(0.72f, 0.74f, 0.78f), BoxFaces::All, 1.0f);
                for (const float z : {upperZ - laneHalf, upperZ + laneHalf})
                    rails.AddBox(Vector3(ux1 - 0.07f, uy, z - 0.035f), Vector3(ux1, uy + 0.92f, z + 0.035f),
                                 1.0f, Vector3(0.72f, 0.74f, 0.78f), BoxFaces::All, 1.0f);
            }
            for (const float z : {lowerZ - laneHalf, lowerZ + laneHalf})
                rails.AddBox(Vector3(lx0, ly + 0.84f, z - 0.045f), Vector3(lx1, ly + 0.92f, z + 0.045f),
                             1.0f, Vector3(0.72f, 0.74f, 0.78f), BoxFaces::All, 1.0f);
            for (const float z : {upperZ - laneHalf, upperZ + laneHalf})
                rails.AddBox(Vector3(ux0, uy + 0.84f, z - 0.045f), Vector3(ux1, uy + 0.92f, z + 0.045f),
                             1.0f, Vector3(0.72f, 0.74f, 0.78f), BoxFaces::All, 1.0f);
        }

        // Invisible lane guards keep the cylinder on a flight while leaving the west landing open for the turn.
        const float guardStart = west + 1.0f;
        collision.AddBox(BoundingBox(Vector3(guardStart, c.Y, lowerZ - laneHalf - 0.08f),
                                     Vector3(east, c.Y + rise + 1.8f, lowerZ - laneHalf + 0.08f)));
        collision.AddBox(BoundingBox(Vector3(guardStart, c.Y, lowerZ + laneHalf - 0.08f),
                                     Vector3(east, c.Y + rise + 1.8f, lowerZ + laneHalf + 0.08f)));
        collision.AddBox(BoundingBox(Vector3(guardStart, c.Y + rise, upperZ - laneHalf - 0.08f),
                                     Vector3(east, c.Y + kFloorPitch + 1.8f, upperZ - laneHalf + 0.08f)));
        collision.AddBox(BoundingBox(Vector3(guardStart, c.Y + rise, upperZ + laneHalf - 0.08f),
                                     Vector3(east, c.Y + kFloorPitch + 1.8f, upperZ + laneHalf + 0.08f)));

        for (const float y : {2.8f, 7.3f, 11.4f}) {
            meshes.Opaque("white").AddBox(Vector3(c.X - 0.65f, c.Y + y, c.Z - 0.65f),
                                           Vector3(c.X + 0.65f, c.Y + y + 0.06f, c.Z + 0.65f), 1.0f,
                                           th.lightColor, BoxFaces::NoTop, 2.0f, 1.0f);
            lights.push_back({Vector3(c.X, c.Y + y - 0.6f, c.Z), th.lightColor, th.lightIntensity, 8.0f});
        }
        return;
    }

    // Floor and ceiling.
    meshes.Opaque(th.floorTex).AddPatch(Vector3(c.X - hw, c.Y, c.Z + hd), Vector3(2 * hw, 0, 0), Vector3(0, 0, -2 * hd), 2.5f,
                                        th.floorTint, 1.0f);
    if (!r.openRoof)
        meshes.Opaque(th.ceilTex).AddPatch(Vector3(c.X - hw, c.Y + H, c.Z - hd), Vector3(2 * hw, 0, 0), Vector3(0, 0, 2 * hd),
                                           3.0f, Vector3(0.95f, 0.95f, 0.95f), 1.0f);

    bool glassOk = false;
    const Wall glassWall = WallFromName(r.params.Get("glass-wall"), glassOk);

    for (const Wall side : {Wall::North, Wall::South, Wall::East, Wall::West}) {
        const bool alongX = (side == Wall::North || side == Wall::South);
        const float L = alongX ? hw + kT : hd;

        if (glassOk && side == glassWall) {
            // Low sill wall, glass above it, thin mullions, and one solid collider for the whole facade.
            AddWallRun(r, meshes, collision, side, -L, L, 0.0f, 1.0f, true, true);
            AddWallRun(r, meshes, collision, side, -L, L, H - 0.6f, H, false, false);
            const float yb = 1.0f + c.Y;
            const float yt = H - 0.6f + c.Y;
            Vector3 origin, U, mullionLo, mullionHi;
            const float span = alongX ? hw : hd;
            switch (side) {
                case Wall::South: origin = Vector3(c.X + hw, yb, c.Z + hd - 0.02f); U = Vector3(-2 * hw, 0, 0); break;
                case Wall::North: origin = Vector3(c.X - hw, yb, c.Z - hd + 0.02f); U = Vector3(2 * hw, 0, 0); break;
                case Wall::East: origin = Vector3(c.X + hw - 0.02f, yb, c.Z - hd); U = Vector3(0, 0, 2 * hd); break;
                default: origin = Vector3(c.X - hw + 0.02f, yb, c.Z + hd); U = Vector3(0, 0, -2 * hd); break;
            }
            meshes.Transparent("glass").AddPatch(origin, U, Vector3(0, yt - yb, 0), 2.0f, Vector3(0.9f, 1.0f, 1.0f), 2.0f);
            for (int i = -2; i <= 2; ++i) {
                const float o = static_cast<float>(i) * span * 0.5f;
                switch (side) {
                    case Wall::South: mullionLo = Vector3(c.X + o - 0.06f, yb, c.Z + hd - 0.12f); mullionHi = Vector3(c.X + o + 0.06f, yt, c.Z + hd); break;
                    case Wall::North: mullionLo = Vector3(c.X + o - 0.06f, yb, c.Z - hd); mullionHi = Vector3(c.X + o + 0.06f, yt, c.Z - hd + 0.12f); break;
                    case Wall::East: mullionLo = Vector3(c.X + hw - 0.12f, yb, c.Z + o - 0.06f); mullionHi = Vector3(c.X + hw, yt, c.Z + o + 0.06f); break;
                    default: mullionLo = Vector3(c.X - hw, yb, c.Z + o - 0.06f); mullionHi = Vector3(c.X - hw + 0.12f, yt, c.Z + o + 0.06f); break;
                }
                meshes.Opaque("metal").AddBox(mullionLo, mullionHi, 1.0f, Vector3(0.5f, 0.5f, 0.55f), BoxFaces::All, 1.0f);
            }
            continue;
        }

        // Doors on this wall, west to east / north to south.
        std::vector<const DoorDef*> doors;
        for (const DoorDef& d : r.doors)
            if (d.wall == side) doors.push_back(&d);
        std::sort(doors.begin(), doors.end(), [](const DoorDef* a, const DoorDef* b) { return a->along < b->along; });

        float cursor = -L;
        for (const DoorDef* d : doors) {
            const float d0 = d->along - d->width * 0.5f;
            const float d1 = d->along + d->width * 0.5f;
            if (d0 > cursor) AddWallRun(r, meshes, collision, side, cursor, d0, 0.0f, H, true, true);
            // Floor across the thickness of the wall, so the doorway has a threshold instead of a hole.
            Vector3 o, U, V;
            switch (side) {
                case Wall::North: o = Vector3(c.X + d0, c.Y, c.Z - hd); U = Vector3(d1 - d0, 0, 0); V = Vector3(0, 0, -kT); break;
                case Wall::South: o = Vector3(c.X + d0, c.Y, c.Z + hd + kT); U = Vector3(d1 - d0, 0, 0); V = Vector3(0, 0, -kT); break;
                case Wall::West: o = Vector3(c.X - hw - kT, c.Y, c.Z + d0); U = Vector3(0, 0, d1 - d0); V = Vector3(kT, 0, 0); break;
                default: o = Vector3(c.X + hw, c.Y, c.Z + d0); U = Vector3(0, 0, d1 - d0); V = Vector3(kT, 0, 0); break;
            }
            meshes.Opaque("carpet").AddPatch(o, U, V, 2.0f, Vector3(0.9f, 0.85f, 0.85f), 1.0f);
            // Lintel above the opening: no collision, it is above head height.
            AddWallRun(r, meshes, collision, side, d0, d1, kDoorHeight, H, false, false);
            cursor = d1;
        }
        if (cursor < L) AddWallRun(r, meshes, collision, side, cursor, L, 0.0f, H, true, true);
    }

    // Ceiling fixtures: an emissive panel and a baked point light under each, on a grid about 6.5 m apart.
    if (!r.openRoof && th.lightIntensity > 0.0f) {
        const int nx = std::max(1, static_cast<int>(std::lround(2.0f * hw / 6.5f)));
        const int nz = std::max(1, static_cast<int>(std::lround(2.0f * hd / 6.5f)));
        const float radius = std::min(std::max(hw, hd), 6.5f) * 1.25f + 3.0f;
        const float panelHalf = (r.kind == CellKind::Room || r.kind == CellKind::Hub) ? 0.7f : 0.4f;
        for (int ix = 0; ix < nx; ++ix) {
            for (int iz = 0; iz < nz; ++iz) {
                const float x = -hw + (static_cast<float>(ix) + 0.5f) * (2.0f * hw / static_cast<float>(nx));
                const float z = -hd + (static_cast<float>(iz) + 0.5f) * (2.0f * hd / static_cast<float>(nz));
                meshes.Opaque("white").AddBox(Vector3(c.X + x - panelHalf, c.Y + H - 0.07f, c.Z + z - panelHalf),
                                              Vector3(c.X + x + panelHalf, c.Y + H - 0.005f, c.Z + z + panelHalf), 1.0f,
                                              th.lightColor, BoxFaces::NoTop, 2.0f, 1.0f);
                BakedLight light;
                light.position = Vector3(c.X + x, c.Y + H - 0.8f, c.Z + z);
                light.color = th.lightColor;
                light.intensity = th.lightIntensity;
                light.radius = radius;
                lights.push_back(light);
            }
        }
    }
}
