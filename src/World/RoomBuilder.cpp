#include "World/RoomBuilder.hpp"

#include <algorithm>
#include <cmath>

#include "Rendering/SignFactory.hpp"

using namespace Microsoft::Xna::Framework;

void RoomBuilder::AddBox(const Vector3& minLocal, const Vector3& maxLocal, const std::string& texture, float texMeters,
                         const Vector3& tint, bool collide, BoxFaces faces)
{
    const Vector3 lo = room_.ToWorld(minLocal.X, minLocal.Y, minLocal.Z);
    const Vector3 hi = room_.ToWorld(maxLocal.X, maxLocal.Y, maxLocal.Z);
    meshes_.Opaque(texture).AddBox(lo, hi, texMeters, tint, faces);
    if (collide) collision_.AddBox(BoundingBox(lo, hi));
}

Vector3 RoomBuilder::AddPedestal(float x, float z, float size, float height, const std::string& texture, const Vector3& tint)
{
    const float h = size * 0.5f;
    // Stepped plinth: a slightly wider base and cap give the block some architectural character.
    AddBox(Vector3(x - h, 0.0f, z - h), Vector3(x + h, height, z + h), texture, 1.0f, tint, true);
    AddBox(Vector3(x - h - 0.05f, 0.0f, z - h - 0.05f), Vector3(x + h + 0.05f, 0.1f, z + h + 0.05f), "stone", 1.0f,
           Vector3(0.7f, 0.7f, 0.7f), false);
    AddBox(Vector3(x - h - 0.04f, height - 0.06f, z - h - 0.04f), Vector3(x + h + 0.04f, height, z + h + 0.04f), "stone",
           1.0f, Vector3(0.75f, 0.75f, 0.75f), false);
    return room_.ToWorld(x, height, z);
}

void RoomBuilder::AddColumn(float x, float z, float radius, const std::string& texture, const Vector3& tint)
{
    const Vector3 base = room_.ToWorld(x, 0.0f, z);
    meshes_.Opaque(texture).AddCylinder(base, radius, room_.height, 20, 2.0f, tint, false);
    meshes_.Opaque("stone").AddBox(base + Vector3(-radius * 1.3f, 0.0f, -radius * 1.3f),
                                   base + Vector3(radius * 1.3f, 0.25f, radius * 1.3f), 1.0f, Vector3(0.8f, 0.8f, 0.8f));
    collision_.AddBox(BoundingBox(base + Vector3(-radius, 0.0f, -radius), base + Vector3(radius, room_.height, radius)));
}

Vector3 RoomBuilder::WallPoint(Wall wall, float along, float height, Vector3& normalOut) const
{
    switch (wall) {
        case Wall::North: normalOut = Vector3(0, 0, 1); return room_.ToWorld(along, height, -room_.halfD);
        case Wall::South: normalOut = Vector3(0, 0, -1); return room_.ToWorld(along, height, room_.halfD);
        case Wall::West: normalOut = Vector3(1, 0, 0); return room_.ToWorld(-room_.halfW, height, along);
        case Wall::East: normalOut = Vector3(-1, 0, 0); return room_.ToWorld(room_.halfW, height, along);
    }
    normalOut = Vector3(0, 0, 1);
    return room_.Center();
}

void RoomBuilder::AddWallSign(Wall wall, float along, float centreHeight, float width, float height, SignSpec spec)
{
    constexpr float kPixelsPerMetre = 220.0f;
    spec.pixelWidth = std::max(64, static_cast<int>(width * kPixelsPerMetre));
    spec.pixelHeight = std::max(32, static_cast<int>(height * kPixelsPerMetre));
    const std::string texture = services_.signs->Create(spec);

    Vector3 n;
    // WallPoint fills `n`, so it must be a separate statement: operand evaluation order is unspecified.
    const Vector3 wallPoint = WallPoint(wall, along, centreHeight, n);
    const Vector3 centre = wallPoint + n * 0.04f;
    const Vector3 right = Vector3::Normalize(Vector3::Cross(-n, Vector3::Up));
    const Vector3 origin = centre - right * (width * 0.5f) - Vector3(0, height * 0.5f, 0);
    // Signs are lit by the nearest ceiling lights like everything else but never darker than 55 %.
    meshes_.Opaque(texture).AddPatch(origin, right * width, Vector3(0, height, 0), 0.0f, Vector3(1, 1, 1), 2.0f, 0.55f);
}

void RoomBuilder::AddStandSign(float x, float z, float yaw, float bottomHeight, float width, float height, SignSpec spec)
{
    constexpr float kPixelsPerMetre = 220.0f;
    spec.pixelWidth = std::max(64, static_cast<int>(width * kPixelsPerMetre));
    spec.pixelHeight = std::max(32, static_cast<int>(height * kPixelsPerMetre));
    const std::string texture = services_.signs->Create(spec);

    // yaw 0 faces +Z (towards a visitor standing south of it); positive yaw turns it towards +X.
    const Vector3 normal(std::sin(yaw), 0.0f, std::cos(yaw));
    const Vector3 right = Vector3::Normalize(Vector3::Cross(-normal, Vector3::Up));
    const Vector3 centre = room_.ToWorld(x, bottomHeight, z);
    meshes_.Opaque(texture).AddPatch(centre - right * (width * 0.5f), right * width, Vector3(0, height, 0), 0.0f,
                                     Vector3(1, 1, 1), 2.0f, 0.55f);
}

void RoomBuilder::AddQuadFacing(const Vector3& centreLocal, float yaw, float width, float height,
                                const std::string& texture, const Vector3& tint, float emissive, bool transparent)
{
    const Vector3 normal(std::sin(yaw), 0.0f, std::cos(yaw));
    const Vector3 right = Vector3::Normalize(Vector3::Cross(-normal, Vector3::Up));
    const Vector3 centre = room_.ToWorld(centreLocal.X, centreLocal.Y, centreLocal.Z);
    MeshBuilder& mesh = transparent ? meshes_.Transparent(texture) : meshes_.Opaque(texture);
    mesh.AddPatch(centre - right * (width * 0.5f) - Vector3(0, height * 0.5f, 0), right * width, Vector3(0, height, 0),
                  0.0f, tint, 2.0f, emissive);
}

void RoomBuilder::AddPedestalPlaque(float x, float z, float pedestalSize, float centreHeight, SignSpec spec,
                                    float width, float height, float faceYaw)
{
    const float yaw = faceYaw > 999.0f ? std::atan2(-x, -z) : faceYaw;
    const Vector3 normal(std::sin(yaw), 0.0f, std::cos(yaw));
    // Snap to the nearest pedestal face so the plaque lies flat against a side of the block.
    const bool alongX = std::abs(normal.X) > std::abs(normal.Z);
    const Vector3 faceNormal = alongX ? Vector3(normal.X > 0 ? 1.0f : -1.0f, 0, 0) : Vector3(0, 0, normal.Z > 0 ? 1.0f : -1.0f);
    const float snappedYaw = std::atan2(faceNormal.X, faceNormal.Z);
    const Vector3 centre(x + faceNormal.X * (pedestalSize * 0.5f + 0.012f), centreHeight,
                         z + faceNormal.Z * (pedestalSize * 0.5f + 0.012f));
    constexpr float kPixelsPerMetre = 220.0f;
    spec.pixelWidth = std::max(64, static_cast<int>(width * kPixelsPerMetre));
    spec.pixelHeight = std::max(32, static_cast<int>(height * kPixelsPerMetre));
    AddQuadFacing(centre, snappedYaw, width, height, services_.signs->Create(spec), Vector3(1, 1, 1), 0.55f);
}

void RoomBuilder::AddLight(const Vector3& localPosition, const Vector3& color, float intensity, float radius)
{
    BakedLight light;
    light.position = room_.ToWorld(localPosition.X, localPosition.Y, localPosition.Z);
    light.color = color;
    light.intensity = intensity;
    light.radius = radius;
    lights_.push_back(light);
}
