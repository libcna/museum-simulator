#include "World/RoomDef.hpp"

#include <cstring>

using Microsoft::Xna::Framework::BoundingBox;
using Microsoft::Xna::Framework::Vector3;

Vector3 RoomDef::Center() const
{
    return Vector3((static_cast<float>(col) - 1.0f) * kGridPitch, 0.0f, (static_cast<float>(row) - 3.0f) * kGridPitch);
}

BoundingBox RoomDef::Interior() const
{
    const Vector3 c = Center();
    return BoundingBox(Vector3(c.X - halfW, 0.0f, c.Z - halfD), Vector3(c.X + halfW, height, c.Z + halfD));
}

Vector3 RoomDef::ToWorld(float x, float y, float z) const
{
    const Vector3 c = Center();
    return Vector3(c.X + x, y, c.Z + z);
}

const std::array<RoomDef, kRoomCount>& AllRooms()
{
    // Order matches RoomId. Tints are linear multipliers on the texture colours.
    static const std::array<RoomDef, kRoomCount> rooms = {{
        {RoomId::Lobby, "lobby", "Entrance Lobby", "Welcome to the CNA 3D Museum",
         1, 3, 8.0f, 6.0f, 5.0f, "marble", "plaster", "plaster",
         Vector3(0.95f, 0.92f, 0.86f), Vector3(1.0f, 1.0f, 1.0f), Vector3(0.30f, 0.29f, 0.28f), 1.1f, false},
        {RoomId::Info, "info", "CNA Information Hall", "What CNA is, what you are running on, and how to get around",
         1, 1, 9.0f, 9.0f, 7.0f, "marble", "plaster", "plaster",
         Vector3(0.82f, 0.88f, 1.0f), Vector3(0.9f, 0.95f, 1.0f), Vector3(0.28f, 0.30f, 0.34f), 1.2f, false},
        {RoomId::Models, "models", "Model Gallery", "ContentManager.Load<Model>, world/view/projection and model transforms",
         0, 2, 7.0f, 7.0f, 5.0f, "wood", "plaster", "plaster",
         Vector3(0.96f, 0.88f, 0.76f), Vector3(1.0f, 1.0f, 1.0f), Vector3(0.28f, 0.26f, 0.24f), 1.1f, false},
        {RoomId::Materials, "materials", "Materials & Textures", "Texture2D, sampler filtering, addressing, mipmaps and UV mapping",
         1, 2, 7.0f, 7.0f, 5.0f, "tiles", "plaster", "plaster",
         Vector3(0.90f, 0.95f, 0.90f), Vector3(1.0f, 1.0f, 1.0f), Vector3(0.30f, 0.30f, 0.30f), 1.15f, false},
        {RoomId::Lighting, "lighting", "Lighting Room", "BasicEffect directional lights, per-pixel lighting, specular and emissive",
         0, 1, 7.0f, 7.0f, 5.0f, "carpet", "plaster", "plaster",
         Vector3(0.40f, 0.42f, 0.55f), Vector3(0.55f, 0.55f, 0.65f), Vector3(0.07f, 0.07f, 0.09f), 0.35f, false},
        {RoomId::RenderStates, "renderstates", "Render State Room", "DepthStencilState, RasterizerState and BlendState side by side",
         2, 2, 7.0f, 7.0f, 5.0f, "stone", "plaster", "plaster",
         Vector3(0.95f, 0.90f, 0.82f), Vector3(1.0f, 1.0f, 1.0f), Vector3(0.30f, 0.29f, 0.28f), 1.1f, false},
        {RoomId::RenderTarget, "rendertarget", "Render Target Room", "RenderTarget2D: the museum filmed by a security camera",
         0, 3, 7.0f, 6.0f, 4.6f, "tiles", "metal", "plaster",
         Vector3(0.60f, 0.72f, 0.95f), Vector3(0.75f, 0.85f, 1.0f), Vector3(0.20f, 0.23f, 0.30f), 1.0f, false},
        {RoomId::Particles, "particles", "Particle Room", "Dynamic geometry: fire, smoke, sparks and a fountain",
         2, 1, 7.0f, 7.0f, 5.5f, "cobble", "stone", "stone",
         Vector3(0.62f, 0.58f, 0.58f), Vector3(0.85f, 0.85f, 0.85f), Vector3(0.14f, 0.12f, 0.12f), 0.55f, false},
        {RoomId::Animation, "animation", "Animation Room", "Matrix hierarchies: a robot arm and an orrery",
         2, 0, 7.0f, 7.0f, 5.5f, "metal", "plaster", "plaster",
         Vector3(0.88f, 0.92f, 0.98f), Vector3(0.9f, 0.95f, 1.0f), Vector3(0.30f, 0.31f, 0.34f), 1.1f, false},
        {RoomId::Sky, "sky", "Sky Courtyard", "A sky dome, day-night cycle and directional sunlight",
         0, 0, 8.0f, 8.0f, 4.6f, "grass", "brick", "plaster",
         Vector3(1.0f, 0.95f, 0.9f), Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f), 0.0f, true},
        {RoomId::Audio, "audio", "Audio Room", "SoundEffect, 3D positional audio and interactive playback",
         1, 0, 7.0f, 7.0f, 5.0f, "carpet", "wood", "plaster",
         Vector3(0.80f, 0.72f, 0.65f), Vector3(1.0f, 0.9f, 0.9f), Vector3(0.22f, 0.20f, 0.20f), 0.95f, false},
    }};
    return rooms;
}

const RoomDef& GetRoom(RoomId id)
{
    return AllRooms()[static_cast<std::size_t>(id)];
}

const RoomDef* FindRoomByKey(const std::string& key)
{
    for (const RoomDef& r : AllRooms())
        if (key == r.key) return &r;
    return nullptr;
}
