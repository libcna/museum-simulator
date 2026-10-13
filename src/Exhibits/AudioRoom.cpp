#include <cmath>
#include <string>

#include "Audio/AudioSystem.hpp"
#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SignFactory.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const Color kGold(255, 226, 150, 255);
const Color kText(222, 228, 240, 255);

struct Speaker {
    const char* sound;
    const char* title;
    float x, z;
    float volume;
    Vector3 colour;
};

constexpr float kSpeakerSize = 0.8f;
constexpr float kSpeakerHeight = 0.9f;
constexpr float kBellX = 0.0f, kBellZ = -2.0f;
constexpr float kBellHeight = 1.0f;

// Doors are in the middle of the west, east and south walls; the speakers sit in the corners.
const Speaker kSpeakers[4] = {
    {"ambience", "Ambience", -4.4f, -4.4f, 0.7f, Vector3(0.3f, 0.8f, 0.5f)},
    {"drums", "Drums", 4.4f, -4.4f, 0.8f, Vector3(0.9f, 0.4f, 0.3f)},
    {"arpeggio", "Arpeggio", -4.4f, 4.4f, 0.7f, Vector3(0.4f, 0.5f, 1.0f)},
    {"tone", "Tone", 4.4f, 4.4f, 0.5f, Vector3(0.9f, 0.8f, 0.3f)},
};

class AudioRoom final : public Exhibit {
public:
    AudioRoom(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        for (const Speaker& s : kSpeakers) {
            b.AddPedestal(s.x, s.z, kSpeakerSize, kSpeakerHeight, "metal", Vector3(0.45f, 0.45f, 0.5f));
            SignSpec plaque;
            plaque.lines = {{s.title, true, kGold}, {std::string("SoundEffectInstance.Apply3D, looped"), false, kText}};
            plaque.background = Color(26, 24, 30, 255);
            b.AddPedestalPlaque(s.x, s.z, kSpeakerSize, 0.5f, plaque, 0.74f, 0.28f);
        }
        b.AddPedestal(kBellX, kBellZ, 0.8f, kBellHeight, "marble", Vector3(0.9f, 0.9f, 0.95f));

        SignSpec title;
        title.lines = {{"3D audio", true, kGold},
                       {"Each speaker is an AudioEmitter. Walk around: pan and distance", false, kText},
                       {"attenuation come from Apply3D against the listener (the camera).", false, kText}};
        b.AddWallSign(Wall::North, 0.0f, 3.3f, 5.2f, 1.3f, title);
        SignSpec bell;
        bell.lines = {{"One-shot", true, kGold}, {"SoundEffect.Play at a position; use it at any range", false, kText}};
        b.AddPedestalPlaque(kBellX, kBellZ, 0.8f, 0.5f, bell, 0.74f, 0.28f);
        b.AddLight(Vector3(0, 4.0f, 0), Vector3(1.0f, 0.85f, 0.75f), 0.5f, 9.0f);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        for (int i = 0; i < 4; ++i) {
            const Speaker& s = kSpeakers[i];
            if (S().audio != nullptr) voices_[i] = CreateLoop(s.sound, R().ToWorld(s.x, kSpeakerHeight + 0.5f, s.z), s.volume, 10.0f);
            Interactable ia;
            ia.bounds = BoundingBox(R().ToWorld(s.x - 0.55f, 0.0f, s.z - 0.55f), R().ToWorld(s.x + 0.55f, kSpeakerHeight + 1.0f, s.z + 0.55f));
            ia.prompt = [this, i] { return std::string(on_[i] ? "Stop " : "Start ") + kSpeakers[i].title; };
            ia.activate = [this, i] {
                on_[i] = !on_[i];
                if (S().audio != nullptr) S().audio->SetPlaying(voices_[i], on_[i]);
            };
            interaction.Add(std::move(ia));
        }
        Interactable bell;
        bell.bounds = BoundingBox(R().ToWorld(kBellX - 0.5f, 0.0f, kBellZ - 0.5f), R().ToWorld(kBellX + 0.5f, kBellHeight + 0.8f, kBellZ + 0.5f));
        bell.prompt = [] { return std::string("Ring the chirp"); };
        bell.activate = [this] {
            ringTime_ = time_;
            if (S().audio != nullptr) S().audio->PlayAt("chirp", R().ToWorld(kBellX, kBellHeight + 0.5f, kBellZ), 1.0f, 14.0f);
        };
        interaction.Add(std::move(bell));
    }

    void OnEnter() override
    {
        // Ambience welcomes the visitor.
        if (!started_ && S().audio != nullptr) {
            started_ = true;
            on_[0] = true;
            S().audio->SetPlaying(voices_[0], true);
        }
    }

    void Update(const UpdateContext& ctx) override { time_ = static_cast<float>(ctx.time); }

    void DrawOpaque(const DrawContext& ctx) override
    {
        GraphicsDevice& device = ctx.device;
        RenderStates::SetOpaque3D(device);
        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setSpecularColorProperty(Vector3(0.4f, 0.4f, 0.4f));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        for (int i = 0; i < 4; ++i) {
            const Speaker& s = kSpeakers[i];
            const float pulse = on_[i] ? 1.0f + 0.25f * std::sin(time_ * (6.0f + static_cast<float>(i) * 2.0f)) : 0.8f;
            fx.setDiffuseColorProperty(s.colour * (on_[i] ? 1.0f : 0.35f));
            fx.setEmissiveColorProperty(on_[i] ? s.colour * 0.5f : Vector3(0, 0, 0));
            fx.setWorldProperty(Matrix::CreateScale(0.5f * pulse) * Matrix::CreateTranslation(R().ToWorld(s.x, kSpeakerHeight + 0.5f, s.z)));
            EffectBank::Apply(fx);
            S().shapes->Sphere().Draw(device);
        }
        const float since = time_ - ringTime_;
        const float swing = since < 2.5f ? std::sin(since * 18.0f) * 0.35f * (1.0f - since / 2.5f) : 0.0f;
        fx.setDiffuseColorProperty(Vector3(0.9f, 0.75f, 0.3f));
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setWorldProperty(Matrix::CreateScale(0.45f, 0.5f, 0.45f) * Matrix::CreateRotationZ(swing) * Matrix::CreateTranslation(R().ToWorld(kBellX, kBellHeight + 0.1f, kBellZ)));
        EffectBank::Apply(fx);
        S().shapes->Cone().Draw(device);
    }

private:
    Voice3D* voices_[4] = {};
    bool on_[4] = {};
    bool started_ = false;
    float time_ = 0.0f;
    float ringTime_ = -10.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeAudioRoom(const RoomDef& room, Services& services)
{
    return std::make_unique<AudioRoom>(room, services)
        ;
}
