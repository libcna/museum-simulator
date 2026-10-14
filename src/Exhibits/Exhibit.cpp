#include "Exhibits/Exhibit.hpp"

#include <cmath>

#include "Audio/AudioSystem.hpp"
#include "Interaction/ControlPanel.hpp"

Exhibit::~Exhibit()
{
    if (services_.audio != nullptr)
        for (Voice3D* v : voices_) services_.audio->ReleaseLoop(v);
}

Voice3D* Exhibit::CreateLoop(const std::string& sound, const Microsoft::Xna::Framework::Vector3& position, float volume, float radius)
{
    if (services_.audio == nullptr) return nullptr;
    Voice3D* voice = services_.audio->CreateLoop(sound, position, volume, radius);
    if (voice != nullptr) voices_.push_back(voice);
    return voice;
}

ControlPanel& Exhibit::AddPanel(const std::string& title, float x, float y, float z, float yaw, float width)
{
    ControlPanel::Pose pose;
    pose.x = x;
    pose.y = y;
    pose.z = z;
    pose.yaw = yaw;
    pose.width = width;
    panels_.push_back(std::make_unique<ControlPanel>(services_, room_, title, pose));
    return *panels_.back();
}

void Exhibit::DoLoad(InteractionSystem& interaction, MuseumWorld& world)
{
    for (auto& panel : panels_) panel->Load(interaction);
    Load(interaction, world);
}

void Exhibit::DoRenderOffscreen(const DrawContext& ctx)
{
    for (auto& panel : panels_) panel->RenderOffscreen(ctx.time);
    RenderOffscreen(ctx);
}

void Exhibit::DoDrawOpaque(const DrawContext& ctx)
{
    for (auto& panel : panels_) panel->Draw(ctx);
    DrawOpaque(ctx);
}

void Exhibit::DoBuildStatic(RoomBuilder& builder)
{
    BuildStatic(builder);
    Dress(builder);
    for (auto& panel : panels_) panel->BuildStand(builder);
}

void Exhibit::FarScreenPose(float height, Microsoft::Xna::Framework::Vector3& centre, float& yaw) const
{
    switch (FarWall()) {
        case Wall::North: centre = room_.ToWorld(0.0f, height, -room_.halfD + 0.06f); yaw = 0.0f; break;
        case Wall::South: centre = room_.ToWorld(0.0f, height, room_.halfD - 0.06f); yaw = 3.14159265f; break;
        case Wall::East: centre = room_.ToWorld(room_.halfW - 0.06f, height, 0.0f); yaw = -1.57079633f; break;
        default: centre = room_.ToWorld(-room_.halfW + 0.06f, height, 0.0f); yaw = 1.57079633f; break;
    }
    yaw += room_.frameYaw;   // local yaw -> world yaw
}

Microsoft::Xna::Framework::Vector3 Exhibit::FarPoint(float along, float height, float& yaw) const
{
    using Microsoft::Xna::Framework::Vector3;
    Vector3 centre;
    FarScreenPose(height, centre, yaw);
    const Vector3 n(std::sin(yaw), 0, std::cos(yaw));
    const Vector3 right = Vector3::Normalize(Vector3::Cross(-n, Vector3::Up));
    return centre + right * along;
}

void Exhibit::FarQuad(float along, float bottom, float w, float h, Microsoft::Xna::Framework::Vector3& origin, Microsoft::Xna::Framework::Vector3& u, Microsoft::Xna::Framework::Vector3& v) const
{
    using Microsoft::Xna::Framework::Vector3;
    float yaw;
    const Vector3 c = FarPoint(along, bottom, yaw);
    const Vector3 n(std::sin(yaw), 0, std::cos(yaw));
    const Vector3 right = Vector3::Normalize(Vector3::Cross(-n, Vector3::Up));
    origin = c - right * (w * 0.5f);
    u = right * w;
    v = Vector3(0, h, 0);
}
