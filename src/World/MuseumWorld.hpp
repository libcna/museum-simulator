#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Exhibits/Exhibit.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Rendering/MeshBuilder.hpp"
#include "Rendering/StaticMesh.hpp"
#include "World/CollisionWorld.hpp"
#include "World/RoomDef.hpp"

/** One texture's worth of baked geometry. */
struct DrawBatch {
    Microsoft::Xna::Framework::Graphics::Texture2D* texture = nullptr;
    std::unique_ptr<StaticMesh> mesh;
};

/** A room or corridor: a unit of frustum culling. */
struct Zone {
    std::string name;
    std::optional<RoomId> room;   // empty for corridors
    Microsoft::Xna::Framework::BoundingBox bounds;
    std::vector<DrawBatch> opaque;
    std::vector<DrawBatch> transparent;
    float brightness = 1.0f;      // runtime multiplier on the baked vertex lighting
};

/**
 * The museum building and its exhibits. Build() generates every room shell,
 * corridor, collider and light from the RoomDef table, asks each exhibit to add its
 * furniture, and uploads the result to GPU buffers once. After that the world only
 * culls zones, draws them, and forwards updates to the exhibits.
 */
class MuseumWorld {
public:
    explicit MuseumWorld(Services& services);
    ~MuseumWorld();
    MuseumWorld(const MuseumWorld&) = delete;
    MuseumWorld& operator=(const MuseumWorld&) = delete;

    void Build();

    [[nodiscard]] const CollisionWorld& Collision() const { return collision_; }
    [[nodiscard]] InteractionSystem& Interaction() { return interaction_; }
    [[nodiscard]] std::optional<RoomId> RoomAt(const Microsoft::Xna::Framework::Vector3& position) const;
    [[nodiscard]] Exhibit* FindExhibit(RoomId id) const;
    [[nodiscard]] const std::vector<Zone>& Zones() const { return zones_; }
    [[nodiscard]] int ZonesDrawn() const { return zonesDrawn_; }
    [[nodiscard]] int ExhibitCount() const { return static_cast<int>(exhibits_.size()); }
    [[nodiscard]] int TriangleCount() const { return triangles_; }

    void SetRoomBrightness(RoomId room, float brightness);

    void Update(float dt, double time, const Microsoft::Xna::Framework::Vector3& playerPosition,
                const Microsoft::Xna::Framework::Vector3& playerForward);
    void RenderOffscreen(const DrawContext& ctx);
    void Draw(const DrawContext& ctx);
    void DrawOverlay();

private:
    void BuildRoomShell(const RoomDef& room, const bool doors[4], MeshSet& meshes, std::vector<BakedLight>& lights);
    void BuildCorridor(const RoomDef& a, const RoomDef& b);
    void BuildExterior();
    void Finalize(Zone& zone, const MeshSet& meshes, const std::vector<BakedLight>& lights,
                  const Microsoft::Xna::Framework::Vector3& ambient);

    Services& services_;
    CollisionWorld collision_;
    InteractionSystem interaction_;
    std::vector<Zone> zones_;
    std::vector<std::unique_ptr<Exhibit>> exhibits_;
    std::optional<RoomId> currentRoom_;
    int zonesDrawn_ = 0;
    int triangles_ = 0;
};

/** Creates the exhibit for a room; returns null for rooms that have none. Defined in Exhibits/ExhibitFactory.cpp. */
std::unique_ptr<Exhibit> CreateExhibit(const RoomDef& room, Services& services);
