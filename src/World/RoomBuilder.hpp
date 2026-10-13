#pragma once

#include <string>
#include <vector>

#include "Exhibits/Services.hpp"
#include "Rendering/MeshBuilder.hpp"
#include "Rendering/SignFactory.hpp"
#include "World/CollisionWorld.hpp"
#include "World/RoomDef.hpp"

enum class Wall { North, South, East, West };

/**
 * What an exhibit sees while the museum is being built. Coordinates are room-local:
 * x grows east, z grows south, y is height above the floor, the origin is the middle
 * of the room's floor. Furniture added here is lit by the room's baked lights.
 */
class RoomBuilder {
public:
    RoomBuilder(const RoomDef& room, MeshSet& meshes, CollisionWorld& collision, std::vector<BakedLight>& lights,
                Services& services)
        : room_(room), meshes_(meshes), collision_(collision), lights_(lights), services_(services)
    {
    }

    [[nodiscard]] const RoomDef& Room() const { return room_; }
    [[nodiscard]] Services& GetServices() { return services_; }
    [[nodiscard]] MeshSet& Meshes() { return meshes_; }
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 World(float x, float y, float z) const { return room_.ToWorld(x, y, z); }

    /** Solid furniture. Collides unless `collide` is false. */
    void AddBox(const Microsoft::Xna::Framework::Vector3& minLocal, const Microsoft::Xna::Framework::Vector3& maxLocal,
                const std::string& texture, float texMeters, const Microsoft::Xna::Framework::Vector3& tint,
                bool collide = true, BoxFaces faces = BoxFaces::All);

    /** Square pedestal centred at (x, z). Returns the world-space centre of its top face. */
    Microsoft::Xna::Framework::Vector3 AddPedestal(float x, float z, float size, float height,
                                                    const std::string& texture = "marble",
                                                    const Microsoft::Xna::Framework::Vector3& tint = {1.0f, 1.0f, 1.0f});

    void AddColumn(float x, float z, float radius, const std::string& texture = "stone",
                   const Microsoft::Xna::Framework::Vector3& tint = {1.0f, 1.0f, 1.0f});

    /** A textured plate on a wall's inner face. `along` is the local x (north/south walls) or z (east/west). */
    void AddWallSign(Wall wall, float along, float centreHeight, float width, float height, SignSpec spec);

    /** A free-standing plate facing +Z rotated by `yaw` (radians, around Y) at a local position. */
    void AddStandSign(float x, float z, float yaw, float bottomHeight, float width, float height, SignSpec spec);

    /** A flat quad with an arbitrary texture; front faces `normalDir` (local direction, horizontal). */
    void AddQuadFacing(const Microsoft::Xna::Framework::Vector3& centreLocal, float yaw, float width, float height,
                       const std::string& texture, const Microsoft::Xna::Framework::Vector3& tint,
                       float emissive = 0.0f, bool transparent = false);

    /** A plaque on the face of the pedestal at (x, z) that points toward the room centre (or along `faceYaw`). */
    void AddPedestalPlaque(float x, float z, float pedestalSize, float centreHeight, SignSpec spec, float width = 0.82f,
                           float height = 0.3f, float faceYaw = 1000.0f);

    void AddLight(const Microsoft::Xna::Framework::Vector3& localPosition, const Microsoft::Xna::Framework::Vector3& color,
                  float intensity, float radius);
    void AddCollider(const Microsoft::Xna::Framework::BoundingBox& worldBox) { collision_.AddBox(worldBox); }

    /** World-space centre of a wall's inner face at `along`, `height` -- plus the outward-from-wall normal. */
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 WallPoint(Wall wall, float along, float height,
                                                               Microsoft::Xna::Framework::Vector3& normalOut) const;

private:
    const RoomDef& room_;
    MeshSet& meshes_;
    CollisionWorld& collision_;
    std::vector<BakedLight>& lights_;
    Services& services_;
};
