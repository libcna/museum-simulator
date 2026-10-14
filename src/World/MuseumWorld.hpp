#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string>
#include <utility>
#include <vector>

#include "Exhibits/Exhibit.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Rendering/MeshBuilder.hpp"
#include "Rendering/StaticMesh.hpp"
#include "World/Catalogue.hpp"
#include "World/CollisionWorld.hpp"

/** One texture's worth of baked geometry. */
struct DrawBatch {
    Microsoft::Xna::Framework::Graphics::Texture2D* texture = nullptr;
    std::unique_ptr<StaticMesh> mesh;
};

/** A cell's baked architecture: the unit of frustum culling. */
struct Zone {
    Microsoft::Xna::Framework::BoundingBox bounds;
    std::vector<DrawBatch> opaque;
    std::vector<DrawBatch> transparent;
    float brightness = 1.0f;      // runtime multiplier on the baked vertex lighting
};

/**
 * Everything that exists while a cell is built: its architecture, colliders, interactables,
 * the exhibit and the textures that belong to them. Destroying an instance releases all of it.
 */
struct RoomInstance {
    int index = -1;
    Zone zone;
    CollisionWorld colliders;
    InteractionSystem interaction;
    std::unique_ptr<Exhibit> exhibit;
    std::vector<std::string> textures;   // run-time textures registered while building this cell
    bool loaded = false;                 // Exhibit::Load has run
    int triangles = 0;
    int failures = 0;                    // exceptions the exhibit has thrown from a hook
    bool disabled = false;               // too many: its hooks are skipped until the cell is rebuilt
};

/** Counters for the statistics overlay and the stress rooms. */
struct WorldStats {
    int cellsBuilt = 0;
    int cellsLoaded = 0;
    int cellsDrawn = 0;
    int cellsBuiltTotal = 0;       // lifetime counts, for leak checks
    int cellsDestroyedTotal = 0;
    int triangles = 0;
    double lastBuildMilliseconds = 0.0;
    int interactablesSeen = 0;     // distinct interactables of every room loaded so far (a panel counts as one)
    int controlsSeen = 0;          // sliders, toggles, choices and buttons on control panels, plus plain interactables
    int roomsLoadedEver = 0;       // distinct cells that have been loaded at least once
    int exhibitErrors = 0;         // exceptions caught from exhibit hooks (a room that throws does not take the museum down)
};

/**
 * The museum building. Cells (rooms, galleries, hubs) are built lazily as the visitor
 * approaches and destroyed when they are far away; what is built, loaded, updated and drawn
 * is decided from the door graph and distance, never from the total room count.
 *
 *   built   - geometry, colliders and static furniture exist (within ~two doors and 75 m)
 *   loaded  - the exhibit's own resources exist and it is simulated (one door, ~45 m)
 *   visible - reachable through doors that intersect the camera frustum (drawn)
 */
class MuseumWorld {
public:
    MuseumWorld(Services& services, const MuseumPlan& museum);
    ~MuseumWorld();
    MuseumWorld(const MuseumWorld&) = delete;
    MuseumWorld& operator=(const MuseumWorld&) = delete;

    [[nodiscard]] const MuseumPlan& GetMuseum() const { return museum_; }
    [[nodiscard]] const CollisionWorld& Collision() const { return collision_; }

    /** The cell containing the player (-1 between cells). */
    [[nodiscard]] int CurrentCell() const { return current_; }
    [[nodiscard]] InteractionSystem* CurrentInteraction();
    [[nodiscard]] Exhibit* FindExhibit(int cell) const;
    [[nodiscard]] bool IsBuilt(int cell) const { return instances_[static_cast<std::size_t>(cell)] != nullptr; }
    [[nodiscard]] const WorldStats& Stats() const { return stats_; }
    /** The newest uncollected message about an exhibit that threw (empty when none), for the HUD. */
    /** Counts an exception that an exhibit's control callback threw (the interaction system reports those, not the hooks `Guard` wraps). */
    void NoteExhibitError() { ++stats_.exhibitErrors; }
    [[nodiscard]] std::string TakeExhibitError() { return std::exchange(exhibitError_, std::string()); }

    /** Builds and loads the cell and its neighbours synchronously, e.g. before a teleport lands there. */
    void Arrive(int cell);

    void SetCellBrightness(int cell, float brightness);

    void Update(float dt, double time, const Microsoft::Xna::Framework::Vector3& playerPosition,
                const Microsoft::Xna::Framework::Vector3& playerForward);
    void RenderOffscreen(const DrawContext& ctx);
    void Draw(const DrawContext& ctx);
    void DrawOverlay();

private:
    struct Wanted {
        int cell;
        int depth;
    };

    void Activate(int cell, const Microsoft::Xna::Framework::Vector3& position, bool synchronous);
    void BuildCell(int cell);
    void LoadCell(int cell);
    void DestroyCell(int cell);
    /** Runs one exhibit hook; an exception is logged and counted, and the third one from a room switches that room's hooks off. */
    template <typename Fn>
    void Guard(RoomInstance& inst, const char* hook, Fn&& fn);
    void RebuildCollision();
    void BuildExterior();
    void CollectVisible(const DrawContext& ctx, std::vector<int>& out) const;
    void BuildCellShell(const RoomDef& room, RoomInstance& inst, MeshSet& meshes, std::vector<BakedLight>& lights);
    void Finalize(Zone& zone, const MeshSet& meshes, const std::vector<BakedLight>& lights,
                  const Microsoft::Xna::Framework::Vector3& ambient, int& triangles);
    [[nodiscard]] float DistanceTo(int cell, const Microsoft::Xna::Framework::Vector3& p) const;

    Services& services_;
    const MuseumPlan& museum_;
    Zone exterior_;
    std::vector<std::unique_ptr<RoomInstance>> instances_;   // one slot per cell; null = not built
    CollisionWorld collision_;                               // union of the built cells' colliders
    std::vector<int> visible_;                               // cells drawn last frame (for offscreen passes)
    int current_ = -1;
    bool collisionDirty_ = false;
    WorldStats stats_;
    std::string exhibitError_;
    std::vector<int> interactableCounts_, controlCounts_;  // per cell, -1 until the cell has been loaded once
};
