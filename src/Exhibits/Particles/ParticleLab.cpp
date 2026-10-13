// Particle laboratory: eleven classic effects from one emitter model with live controls. The simulation is the
// museum's own small ParticleSystem (Euler motion on the CPU); drawing batches all quads into one
// DynamicVertexBuffer rewritten with SetDataOptions.Discard each frame.
#include <algorithm>
#include <cmath>
#include <random>
#include <string>

#include "Audio/AudioSystem.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/Particles.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Pattern { Point, Ring, Box, Cone, Sphere };

struct Preset {
    const char* key;
    const char* title;
    const char* texture;
    bool additive;
    Pattern pattern;
    float rate, life, speed, spread, gravity, drag, size0, size1, spawnRadius, height;
    Color c0, c1;
    bool floorKills, bounce, swirl, sway, fireworks;
    int capacity;
};

const Preset kPresets[] = {
    {"fire", "Fire", "particle", true, Pattern::Cone, 160, 1.4f, 1.6f, 0.25f, 2.2f, 0.4f, 0.55f, 0.08f, 0.35f, 0.1f, Color(255, 190, 80, 255), Color(220, 40, 10, 0), false, false, false, true, false, 2500},
    {"smoke", "Smoke", "smoke", false, Pattern::Cone, 40, 4.5f, 0.7f, 0.3f, 0.6f, 0.2f, 0.4f, 1.6f, 0.3f, 0.1f, Color(120, 120, 125, 180), Color(60, 60, 65, 0), false, false, false, true, false, 1500},
    {"sparks", "Sparks", "particle", true, Pattern::Cone, 90, 1.1f, 4.5f, 0.55f, -9.0f, 0.1f, 0.07f, 0.02f, 0.05f, 0.1f, Color(255, 240, 150, 255), Color(255, 90, 20, 0), true, true, false, false, false, 3000},
    {"snow", "Snow", "particle", false, Pattern::Box, 120, 9.0f, 0.2f, 0.3f, -0.45f, 0.5f, 0.07f, 0.07f, 3.5f, 4.6f, Color(245, 248, 255, 230), Color(245, 248, 255, 200), true, false, false, true, false, 5000},
    {"rain", "Rain", "particle", false, Pattern::Box, 380, 1.4f, 0.5f, 0.02f, -14.0f, 0.0f, 0.045f, 0.045f, 3.5f, 4.8f, Color(170, 200, 255, 200), Color(170, 200, 255, 160), true, false, false, false, false, 6000},
    {"fountain", "Fountain", "particle", false, Pattern::Cone, 220, 2.2f, 5.4f, 0.12f, -9.8f, 0.0f, 0.09f, 0.06f, 0.05f, 0.1f, Color(150, 200, 255, 230), Color(200, 230, 255, 120), true, true, false, false, false, 4000},
    {"dust", "Dust motes", "particle", false, Pattern::Box, 18, 9.0f, 0.15f, 0.8f, 0.0f, 0.3f, 0.05f, 0.05f, 3.2f, 1.8f, Color(255, 245, 210, 110), Color(255, 245, 210, 0), false, false, false, true, false, 600},
    {"fireworks", "Fireworks", "particle", true, Pattern::Point, 0.5f, 1.8f, 11.0f, 1.0f, -4.0f, 0.4f, 0.18f, 0.05f, 0.05f, 0.4f, Color(255, 230, 150, 255), Color(255, 60, 40, 0), false, false, false, false, true, 8000},
    {"bubbles", "Bubbles", "particle", false, Pattern::Box, 22, 6.0f, 0.2f, 0.3f, 0.45f, 0.5f, 0.08f, 0.17f, 1.6f, 0.2f, Color(190, 230, 255, 140), Color(190, 230, 255, 30), false, false, false, true, false, 800},
    {"leaves", "Falling leaves", "particle", false, Pattern::Box, 25, 8.0f, 0.2f, 0.5f, -0.9f, 0.7f, 0.11f, 0.11f, 3.0f, 4.7f, Color(215, 140, 50, 255), Color(150, 90, 30, 200), true, false, false, true, false, 800},
    {"magic", "Magic spiral", "particle", true, Pattern::Ring, 140, 2.6f, 0.3f, 0.2f, 0.7f, 0.3f, 0.14f, 0.02f, 0.5f, 0.2f, Color(170, 120, 255, 255), Color(60, 220, 255, 0), false, false, true, false, false, 3000},
};

class ParticleLab final : public Exhibit {
public:
    ParticleLab(const RoomDef& room, Services& services) : Exhibit(room, services), rng_(1234)
    {
        const std::string key = room.params.Get("preset");
        for (const Preset& p : kPresets)
            if (key == p.key) preset_ = &p;
        if (preset_ == nullptr) preset_ = &kPresets[0];
        rate_ = preset_->rate;
        life_ = preset_->life;
        speed_ = preset_->speed;
        spread_ = preset_->spread;
        gravity_ = preset_->gravity;
        size_ = 1.0f;
        additive_ = preset_->additive;
        pattern_ = static_cast<int>(preset_->pattern);
        system_ = std::make_unique<ParticleSystem>(static_cast<std::size_t>(preset_->capacity));
    }

    void BuildStatic(RoomBuilder& b) override
    {
        b.AddPedestal(0.0f, 0.0f, 1.2f, 0.35f, "stone");
        ControlPanel& p = AddPanel(preset_->title, -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        const bool fw = preset_->fireworks;
        p.AddSlider(fw ? "Rockets per second" : "Emission rate (per s)", 0.0f, std::max(2.0f, preset_->rate * 3.0f), rate_, [this](float v) { rate_ = v; }, preset_->rate < 5.0f ? "%.1f" : "%.0f");
        p.AddSlider("Lifetime (s)", 0.2f, 10.0f, life_, [this](float v) { life_ = v; }, "%.1f");
        p.AddSlider("Initial speed", 0.0f, 12.0f, speed_, [this](float v) { speed_ = v; }, "%.1f");
        p.AddSlider("Spread", 0.0f, 1.0f, spread_, [this](float v) { spread_ = v; });
        p.AddSlider("Gravity (m/s2)", -15.0f, 5.0f, gravity_, [this](float v) { gravity_ = v; }, "%.1f");
        p.AddSlider("Size", 0.3f, 4.0f, size_, [this](float v) { size_ = v; });
        p.AddChoice("Blend", {"AlphaBlend", "Additive"}, additive_ ? 1 : 0, [this](int v) { additive_ = v == 1; });
        p.AddChoice("Spawn pattern", {"Point", "Ring", "Box", "Cone", "Sphere"}, pattern_, [this](int v) { pattern_ = v; });
        p.AddButton("Burst of 200", [this] { burst_ = 200; });
        p.AddReadout("Particles alive / capacity", [this] { return std::to_string(system_->Size()) + " / " + std::to_string(preset_->capacity); });
        p.AddReadout("Quads drawn", [this] { return std::to_string(quads_); });
        SignSpec what = Kit::Plate(std::string("Particles: ") + preset_->title, {"Each particle is a camera-facing quad with a position, velocity, age and a colour and size", "interpolated over its life. Drag the controls and the effect changes at once.", "All quads share one DynamicVertexBuffer rewritten each frame (SetDataOptions.Discard)."});
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        renderer_ = std::make_unique<ParticleRenderer>(*S().device, preset_->capacity);
        if (preset_->key == std::string("fire")) fireVoice_ = CreateLoop("fire", R().ToWorld(0, 0.6f, 0), 0.4f, 9.0f);
        if (!preset_->fireworks)
            for (int i = 0; i < static_cast<int>(std::min(life_, 6.0f) * 20.0f); ++i) Step(0.05f);   // warm up to a steady state
        if (fireVoice_ != nullptr && S().audio != nullptr) S().audio->SetPlaying(fireVoice_, true);
        if (preset_->key == std::string("fountain")) waterVoice_ = CreateLoop("water", R().ToWorld(0, 0.6f, 0), 0.4f, 9.0f);
        if (waterVoice_ != nullptr && S().audio != nullptr) S().audio->SetPlaying(waterVoice_, true);
    }

    void Update(const UpdateContext& ctx) override
    {
        if (ctx.nearby) Step(std::min(ctx.dt, 0.05f));
    }

private:
    // One simulation step. Load() runs a few seconds of these so an effect is already full when the visitor arrives.
    void Step(float dt)
    {
        time_ += dt;
        const Preset& P = *preset_;
        // Emission.
        emit_ += rate_ * dt;
        int count = static_cast<int>(emit_);
        emit_ -= static_cast<float>(count);
        if (burst_ > 0) {
            count += std::min(burst_, 100);
            burst_ -= std::min(burst_, 100);
        }
        for (int i = 0; i < count; ++i) Spawn(P);

        // Per-particle behaviours the base system does not have.
        auto& list = system_->MutableParticles();
        for (Particle& q : list) {
            if (P.swirl) {
                const float r = std::max(0.2f, std::sqrt(q.position.X * q.position.X + q.position.Z * q.position.Z));
                const Vector3 centre = R().ToWorld(0, 0, 0);
                const Vector3 rel = q.position - centre;
                q.velocity += Vector3(-rel.Z, 0.0f, rel.X) / r * (1.8f * dt) - Vector3(rel.X, 0, rel.Z) * (0.8f * dt);
            }
            if (P.sway) {
                q.velocity.X += std::sin(time_ * 1.3f + q.rotation * 5.0f) * 0.9f * dt;
                q.velocity.Z += std::cos(time_ * 1.1f + q.rotation * 3.0f) * 0.9f * dt;
            }
        }
        // Fireworks: particles that reach the end of their life explode into a burst of sparks.
        if (P.fireworks) {
            std::vector<Vector3> bursts;
            for (const Particle& q : list)
                if (q.size0 > 0.17f && q.age + dt >= q.life) bursts.push_back(q.position);
            for (const Vector3& at : bursts) Explode(at);
        }
        ParticleSystem::Physics physics;
        physics.gravity = Vector3(0, gravity_, 0);
        physics.drag = P.drag;
        physics.hasFloor = P.floorKills;
        physics.floorY = R().centre.Y + 0.02f;
        physics.bounce = P.bounce ? 0.35f : 0.0f;
        system_->Update(dt, physics);
    }

public:
    void DrawTransparent(const DrawContext& ctx) override
    {
        if (!renderer_) return;
        const BlendState& blend = additive_ ? BlendState::Additive : BlendState::NonPremultiplied;
        renderer_->Draw(ctx, S().textures->Get(preset_->texture), blend, !additive_, {system_.get()});
        quads_ = renderer_->LastQuadCount();
    }

    [[nodiscard]] float Rand(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng_); }

    Vector3 SpawnPoint()
    {
        const Preset& P = *preset_;
        const Vector3 base = R().ToWorld(0, P.height, 0);
        switch (static_cast<Pattern>(pattern_)) {
            case Pattern::Ring: {
                const float a = Rand(0, MathHelper::TwoPi);
                return base + Vector3(std::cos(a), 0, std::sin(a)) * std::max(0.2f, P.spawnRadius);
            }
            case Pattern::Box: return base + Vector3(Rand(-3.5f, 3.5f), Rand(0.0f, 0.4f), Rand(-3.5f, 3.5f)) * (P.spawnRadius > 1.0f ? 1.0f : 0.5f);
            case Pattern::Sphere: {
                Vector3 d(Rand(-1, 1), Rand(-1, 1), Rand(-1, 1));
                if (d.LengthSquared() < 1e-4f) d = Vector3(0, 1, 0);
                return base + Vector3::Normalize(d) * std::max(0.2f, P.spawnRadius);
            }
            default: return base + Vector3(Rand(-1, 1), 0, Rand(-1, 1)) * (static_cast<Pattern>(pattern_) == Pattern::Cone ? P.spawnRadius * 0.3f : 0.0f);
        }
    }
    Vector3 Direction(const Vector3& from)
    {
        const Preset& P = *preset_;
        Vector3 up(0, 1, 0);
        if (P.gravity < 0 && (P.key == std::string("snow") || P.key == std::string("rain") || P.key == std::string("leaves"))) up = Vector3(0, -1, 0);
        if (P.swirl) return Vector3(0, 0.6f, 0);
        if (P.fireworks) return Vector3(Rand(-0.1f, 0.1f), 1.0f, Rand(-0.1f, 0.1f));
        Vector3 d = up + Vector3(Rand(-1, 1), Rand(-0.3f, 0.3f), Rand(-1, 1)) * spread_;
        if (static_cast<Pattern>(pattern_) == Pattern::Sphere) d = Vector3::Normalize(from - R().ToWorld(0, P.height, 0) + Vector3(0, 1e-3f, 0));
        return Vector3::Normalize(d);
    }

    void Spawn(const Preset& P)
    {
        Particle q;
        q.position = SpawnPoint();
        q.velocity = Direction(q.position) * speed_ * Rand(0.8f, 1.2f);
        q.life = life_ * Rand(0.8f, 1.2f);
        q.size0 = P.size0 * size_ * (P.fireworks ? 2.2f : 1.0f);
        q.size1 = P.size1 * size_ * (P.fireworks ? 2.2f : 1.0f);
        q.rotation = Rand(0, MathHelper::TwoPi);
        q.spin = Rand(-1.5f, 1.5f);
        q.colour0 = P.c0;
        q.colour1 = P.c1;
        system_->Emit(q);
    }

    void Explode(const Vector3& at)
    {
        const Color tints[4] = {Color(255, 120, 90, 255), Color(120, 255, 150, 255), Color(120, 170, 255, 255), Color(255, 230, 120, 255)};
        const Color tint = tints[burstIndex_++ % 4];
        for (int i = 0; i < 90; ++i) {
            Vector3 d(Rand(-1, 1), Rand(-1, 1), Rand(-1, 1));
            if (d.LengthSquared() < 1e-3f) d = Vector3(0, 1, 0);
            Particle q;
            q.position = at;
            q.velocity = Vector3::Normalize(d) * Rand(2.5f, 4.2f) * (speed_ / 11.0f + 0.5f);
            q.life = Rand(1.0f, 1.8f);
            q.size0 = 0.09f * size_;
            q.size1 = 0.02f * size_;
            q.rotation = Rand(0, 6.28f);
            q.colour0 = tint;
            q.colour1 = Kit::Rgba(tint.getRProperty(), tint.getGProperty() / 2, tint.getBProperty() / 2, 0);
            system_->Emit(q);
        }
    }

    const Preset* preset_ = nullptr;
    std::mt19937 rng_;
    std::unique_ptr<ParticleSystem> system_;
    std::unique_ptr<ParticleRenderer> renderer_;
    Voice3D* fireVoice_ = nullptr;
    Voice3D* waterVoice_ = nullptr;
    float time_ = 0.0f, emit_ = 0.0f, rate_ = 100, life_ = 2, speed_ = 1, spread_ = 0.3f, gravity_ = 0, size_ = 1;
    bool additive_ = true;
    int pattern_ = 0, burst_ = 0, quads_ = 0, burstIndex_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeParticleLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ParticleLab>(room, services);
}
