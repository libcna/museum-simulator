#pragma once

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

/**
 * The museum as data. Content files (the files in src/Catalogue) declare floors, wings and rooms;
 * Layout.cpp places them and generates the doors; everything else (building geometry,
 * visibility, the map, the directory, validation, inventory) reads this one immutable
 * description.
 */

enum class Wall { North, South, East, West };

constexpr Wall Opposite(Wall w)
{
    switch (w) {
        case Wall::North: return Wall::South;
        case Wall::South: return Wall::North;
        case Wall::East: return Wall::West;
        default: return Wall::East;
    }
}

/** What kind of space a cell is; exhibits for hubs, spines and galleries come from the catalogue itself. */
enum class CellKind { Room, Gallery, Spine, Hub, Elevator, Stairs };

/** Where an exhibit's API comes from; shown as a badge on every information plaque. */
enum class ApiOrigin { Xna, CnaExtension, Application };

constexpr float kFloorPitch = 9.0f;        // vertical distance between floors
constexpr float kWallThickness = 0.4f;
constexpr float kDoorWidth = 3.2f;
constexpr float kDoorHeight = 3.4f;
constexpr float kCorridorHeight = 3.4f;
constexpr float kCellGap = kWallThickness * 2.0f;   // distance between the interiors of two adjacent cells
constexpr float kStairHalfWidth = 6.0f;
constexpr float kStairHalfDepth = 4.0f;
constexpr float kStairLandingDepth = 0.8f;
constexpr float kStairLaneOffset = 1.6f;
constexpr float kStairLaneWidth = 2.4f;

/** Free-form key/value parameters that configure a data-driven exhibit. */
struct Params {
    std::vector<std::pair<std::string, std::string>> items;

    Params() = default;
    Params(std::initializer_list<std::pair<std::string, std::string>> init) : items(init) {}
    [[nodiscard]] std::string Get(const std::string& key, const std::string& fallback = {}) const;
    [[nodiscard]] float GetFloat(const std::string& key, float fallback) const;
    [[nodiscard]] int GetInt(const std::string& key, int fallback) const;
    [[nodiscard]] bool Has(const std::string& key) const;
};

/** Surface and lighting look of a cell. Named presets live in Catalogue/Themes.cpp. */
struct Theme {
    std::string floorTex = "marble";
    std::string wallTex = "plaster";
    std::string ceilTex = "plaster";
    Microsoft::Xna::Framework::Vector3 wallTint{1, 1, 1};
    Microsoft::Xna::Framework::Vector3 floorTint{1, 1, 1};
    Microsoft::Xna::Framework::Vector3 ambient{0.30f, 0.29f, 0.28f};   // baked ambient light level
    Microsoft::Xna::Framework::Vector3 lightColor{1.0f, 0.93f, 0.80f};
    float lightIntensity = 1.1f;                                        // ceiling fixture strength
};
const Theme& FindTheme(const std::string& name);   // unknown names fall back to "classic"

struct DoorDef {
    Wall wall = Wall::North;
    float along = 0.0f;       // offset from the wall's midpoint (x for north/south walls, z for east/west)
    float width = kDoorWidth;
    int to = -1;              // index of the cell on the other side
    float floorOffset = 0.0f; // height above this cell's base (upper stair landings use kFloorPitch)
};

struct FloorDef {
    int index = 0;
    std::string name;         // "Two-Dimensional Rendering"
    std::string tagline;
    std::string theme;        // default theme for the floor's hub and spine
    Microsoft::Xna::Framework::Vector3 accent{0.8f, 0.7f, 0.3f};   // colour on signs, maps and the HUD
    float baseY = 0.0f;
    std::vector<int> wings;   // indices into Museum::wings
    int hub = -1;             // cell indices
    int spine = -1;
    int elevator = -1;
    int stairsDown = -1;      // stair flight ending on this floor
    int stairsUp = -1;        // stair flight beginning on this floor
};

struct WingDef {
    int index = 0;
    int floor = 0;
    int number = 1;           // 1-based within the floor
    std::string name;
    std::string tagline;
    std::string theme;
    Microsoft::Xna::Framework::Vector3 accent{0.8f, 0.7f, 0.3f};
    int gallery = -1;         // cell index of the corridor
    std::vector<int> rooms;   // cell indices, in catalogue order
    Wall spineSide = Wall::East;   // which side of the spine the wing lies on
};

struct RoomDef {
    int index = -1;
    std::string key;          // unique, kebab-case; used on the command line and in scripts
    std::string number;       // "4.2.07"
    std::string title;
    std::string subtitle;
    CellKind kind = CellKind::Room;
    int floor = 0;
    int wing = -1;            // -1 for hubs, spines and elevators
    std::string exhibit;      // exhibit factory name (see Exhibits/ExhibitRegistry.hpp)
    Params params;
    std::vector<std::string> apis;   // CNA API names demonstrated, used for the coverage inventory
    ApiOrigin origin = ApiOrigin::Xna;

    // Geometry (world space). `centre.Y` is the floor's base height.
    Microsoft::Xna::Framework::Vector3 centre{0, 0, 0};
    float halfW = 7.0f, halfD = 7.0f, height = 5.0f;
    bool openRoof = false;

    Theme theme;
    Microsoft::Xna::Framework::Vector3 accent{0.8f, 0.7f, 0.3f};
    std::vector<DoorDef> doors;

    [[nodiscard]] Microsoft::Xna::Framework::Vector3 Center() const { return centre; }
    [[nodiscard]] Microsoft::Xna::Framework::BoundingBox Interior() const;
    /**
     * Turn of the room's local frame about its centre (0, or pi for a room entered through its north wall).
     * Local coordinates are what exhibits are written in: the door is always on the local south wall and
     * the far wall (where screens hang) is always the local north wall, whichever side of the gallery the
     * room is on. Everything that places furniture or content goes through ToWorld, Rotate and LocalWall.
     */
    float frameYaw = 0.0f;
    [[nodiscard]] bool Turned() const { return frameYaw != 0.0f; }
    /** Room-local metres (x east, z south, y above the floor) -> world. */
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 ToWorld(float x, float y, float z) const
    {
        return Turned() ? Microsoft::Xna::Framework::Vector3(centre.X - x, centre.Y + y, centre.Z - z)
                        : Microsoft::Xna::Framework::Vector3(centre.X + x, centre.Y + y, centre.Z + z);
    }
    /** Room centre plus a world-axis offset (ignores the local frame): for code that works in world walls. */
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 WorldAt(float x, float y, float z) const
    {
        return Microsoft::Xna::Framework::Vector3(centre.X + x, centre.Y + y, centre.Z + z);
    }
    /** A local direction or offset -> world direction. */
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 Rotate(const Microsoft::Xna::Framework::Vector3& v) const
    {
        return Turned() ? Microsoft::Xna::Framework::Vector3(-v.X, v.Y, -v.Z) : v;
    }
    /** The world wall a local wall lies on, and the local wall a world wall appears as. */
    [[nodiscard]] Wall WorldWall(Wall local) const { return Turned() ? Opposite(local) : local; }
    [[nodiscard]] Wall LocalWall(Wall world) const { return Turned() ? Opposite(world) : world; }
    /** An offset along a wall (x for north/south walls, z for east/west) in the other frame. */
    [[nodiscard]] float AlongSign() const { return Turned() ? -1.0f : 1.0f; }
    [[nodiscard]] bool Contains(const Microsoft::Xna::Framework::Vector3& p, float margin = 0.0f) const;
    /** The portal volume of a door, in world space. */
    [[nodiscard]] Microsoft::Xna::Framework::BoundingBox DoorBounds(const DoorDef& door) const;
};

struct Spawn {
    Microsoft::Xna::Framework::Vector3 eye;
    float yaw = 0.0f;   // radians; 0 looks down -Z, positive turns left
};

/** The finished, laid-out museum. Immutable after Build(). */
class MuseumPlan {
public:
    std::vector<FloorDef> floors;
    std::vector<WingDef> wings;
    std::vector<RoomDef> rooms;   // every cell: hubs, spines, galleries, elevators and rooms

    [[nodiscard]] const RoomDef& Room(int index) const { return rooms[static_cast<std::size_t>(index)]; }
    [[nodiscard]] const RoomDef* FindByKey(const std::string& key) const;
    /** The cell containing a world position (interior, or a doorway), or -1. */
    [[nodiscard]] int CellAt(const Microsoft::Xna::Framework::Vector3& p, int hint = -1) const;
    [[nodiscard]] int FloorAt(float worldY) const;
    /** Floor height under a visitor's feet, including the two ramps and landing of a stair flight. */
    [[nodiscard]] float GroundHeightAt(const Microsoft::Xna::Framework::Vector3& feet) const;
    [[nodiscard]] const std::vector<int>& CellsOnFloor(int floor) const { return byFloor_[static_cast<std::size_t>(floor)]; }
    /** Where a visitor arrives in a cell: just inside its first door, looking into the room. */
    [[nodiscard]] Spawn SpawnPoint(int cell) const;

    void IndexCells();   // builds byFloor_ and the key map; called by the layout code

private:
    std::vector<std::vector<int>> byFloor_;
    std::map<std::string, int> byKey_;
};

/** The one museum instance, built (and validated) on first use. */
const MuseumPlan& TheMuseum();

/** Structural problems found by Validate(): empty when the museum is sound. */
std::vector<std::string> ValidateMuseum(const MuseumPlan& museum);
