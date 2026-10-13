#include "Exhibits/Exhibit.hpp"

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
    for (auto& panel : panels_) panel->BuildStand(builder);
}
