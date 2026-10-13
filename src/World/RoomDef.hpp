#pragma once

#include <array>
#include <string>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

enum class RoomId {
    Lobby, Info, Models, Materials, Lighting, RenderStates, RenderTarget, Particles, Animation, Sky, Audio,
    Count
};
constexpr int kRoomCount = static_cast<int>(RoomId::Count);

struct RoomDef {
    RoomId id;
    const char* key;        // command-line / script name
    const char* title;      // shown on the HUD when entering
    const char* subtitle;   // one-line summary of what the room demonstrates
    int col, row;           // position in the 3x4 grid (row 3 is the entrance row)
    float halfW, halfD;     // interior half extents on X and Z
    float height;
    const char* floorTex;
    const char* wallTex;
    const char* ceilTex;
    Microsoft::Xna::Framework::Vector3 wallTint;
    Microsoft::Xna::Framework::Vector3 floorTint;
    Microsoft::Xna::Framework::Vector3 ambient;   // baked ambient light level
    float lightIntensity;                          // ceiling fixture strength
    bool openRoof;

    [[nodiscard]] Microsoft::Xna::Framework::Vector3 Center() const;
    [[nodiscard]] Microsoft::Xna::Framework::BoundingBox Interior() const;
    /** Position in room-local metres (x east, z south) -> world, on the floor. */
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 ToWorld(float x, float y, float z) const;
};

constexpr float kGridPitch = 20.0f;   // distance between room centres
constexpr float kWallThickness = 0.4f;
constexpr float kDoorWidth = 3.2f;
constexpr float kDoorHeight = 3.4f;
constexpr float kCorridorHeight = 3.4f;   // equal to the door height so lintels and corridor ceilings line up

const std::array<RoomDef, kRoomCount>& AllRooms();
const RoomDef& GetRoom(RoomId id);
const RoomDef* FindRoomByKey(const std::string& key);
