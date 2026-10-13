#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "Audio/AudioSystem.hpp"
#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/PunctualLightEXT.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/Particles.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SignFactory.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const Color kGold(255, 226, 150, 255);
const Color kText(222, 228, 240, 255);

constexpr float kFireX = -3.6f, kFireZ = -3.2f;
constexpr float kPoolX = 3.6f, kPoolZ = -3.2f;
constexpr float kSparkX = 3.6f, kSparkZ = 3.6f;
constexpr float kPoolHalf = 1.5f;
constexpr float kWaterY = 0.40f;
constexpr float kSpoutTop = 1.15f;
constexpr float kSparkTop = 1.0f;

const char* const kFireNames[3] = {"low", "normal", "roaring"};
const float kFireScale[3] = {0.55f, 1.0f, 1.9f};
const char* const kFountainNames[3] = {"off", "low jet", "high jet"};

class ParticleRoom final : public Exhibit {
public:
    ParticleRoom(const RoomDef& room, Services& services)
        : Exhibit(room, services), rng_(12345), flames_(520), smoke_(260), embers_(160), water_(1100), splash_(320), sparks_(520), dust_(120)
    {
    }

    void BuildStatic(RoomBuilder& b) override
    {
        // Fire pit: a ring of stones and a bed of ash. The flames and logs are dynamic.
        for (int i = 0; i < 10; ++i) {
            const float a = static_cast<float>(i) / 10.0f * MathHelper::TwoPi;
            b.Meshes().Opaque("stone").AddCylinder(b.World(kFireX + std::cos(a) * 0.78f, 0.0f, kFireZ + std::sin(a) * 0.78f), 0.17f, 0.24f, 10,
                                                    0.5f, Vector3(0.75f, 0.72f, 0.7f), true);
        }
        b.Meshes().Opaque("white").AddCylinder(b.World(kFireX, 0.0f, kFireZ), 0.62f, 0.04f, 20, 1.0f, Vector3(0.07f, 0.06f, 0.06f), true);
        b.AddCollider(BoundingBox(b.World(kFireX - 0.97f, 0.0f, kFireZ - 0.97f), b.World(kFireX + 0.97f, 0.5f, kFireZ + 0.97f)));

        // Fountain: a square basin with a spout in the middle.
        const float x0 = kPoolX - kPoolHalf, x1 = kPoolX + kPoolHalf, z0 = kPoolZ - kPoolHalf, z1 = kPoolZ + kPoolHalf;
        const float t = 0.25f;
        b.AddBox(Vector3(x0 - t, 0.0f, z0 - t), Vector3(x1 + t, 0.55f, z0), "stone", 1.0f, Vector3(0.8f, 0.8f, 0.85f));
        b.AddBox(Vector3(x0 - t, 0.0f, z1), Vector3(x1 + t, 0.55f, z1 + t), "stone", 1.0f, Vector3(0.8f, 0.8f, 0.85f));
        b.AddBox(Vector3(x0 - t, 0.0f, z0), Vector3(x0, 0.55f, z1), "stone", 1.0f, Vector3(0.8f, 0.8f, 0.85f));
        b.AddBox(Vector3(x1, 0.0f, z0), Vector3(x1 + t, 0.55f, z1), "stone", 1.0f, Vector3(0.8f, 0.8f, 0.85f));
        b.AddBox(Vector3(x0, 0.0f, z0), Vector3(x1, 0.12f, z1), "tiles", 1.0f, Vector3(0.4f, 0.5f, 0.65f), false);
        b.Meshes().Opaque("marble").AddCylinder(b.World(kPoolX, 0.0f, kPoolZ), 0.16f, kSpoutTop, 16, 1.0f, Vector3(0.9f, 0.9f, 1.0f), true);
        b.Meshes().Opaque("marble").AddCylinder(b.World(kPoolX, 0.0f, kPoolZ), 0.34f, 0.5f, 16, 1.0f, Vector3(0.9f, 0.9f, 1.0f), true);

        // Spark pedestal and a bench.
        b.AddPedestal(kSparkX, kSparkZ, 0.8f, kSparkTop - 0.1f, "metal", Vector3(0.6f, 0.6f, 0.65f));
        SignSpec ps;
        ps.lines = {{"Spark burst", true, kGold}, {"ParticleSystem with a floor bounce", false, kText}};
        ps.background = Color(26, 24, 30, 255);
        b.AddPedestalPlaque(kSparkX, kSparkZ, 0.8f, 0.5f, ps, 0.76f, 0.3f, 0.0f);
        b.AddBox(Vector3(-5.2f, 0.0f, 3.2f), Vector3(-2.4f, 0.45f, 3.9f), "stone", 1.0f, Vector3(0.7f, 0.68f, 0.66f));

        SignSpec title;
        title.lines = {{"Dynamic geometry", true, kGold},
                       {"Every flame, drop and spark is a quad rebuilt each frame in a DynamicVertexBuffer", false, kText},
                       {"and drawn with one DrawIndexedPrimitives call per texture and blend mode.", false, kText}};
        b.AddWallSign(Wall::West, 4.2f, 3.3f, 5.6f, 1.4f, title);
        SignSpec fireSign;
        fireSign.lines = {{"Campfire", true, kGold}, {"flames and embers: additive. smoke: alpha blended and sorted.", false, kText}};
        b.AddWallSign(Wall::North, -4.2f, 3.0f, 4.0f, 0.9f, fireSign);
        SignSpec fountainSign;
        fountainSign.lines = {{"Fountain", true, kGold}, {"drops fall under gravity; splashes are spawned where they land", false, kText}};
        b.AddWallSign(Wall::North, 4.2f, 3.0f, 4.0f, 0.9f, fountainSign);

        b.AddLight(Vector3(kFireX, 1.0f, kFireZ), Vector3(1.0f, 0.55f, 0.22f), 0.95f, 8.5f);
        b.AddLight(Vector3(kPoolX, 1.4f, kPoolZ), Vector3(0.5f, 0.7f, 1.0f), 0.55f, 7.5f);
        b.AddLight(Vector3(kSparkX, 1.6f, kSparkZ), Vector3(1.0f, 0.8f, 0.5f), 0.5f, 6.0f);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        renderer_ = std::make_unique<ParticleRenderer>(*S().device, 1700);

        Interactable fire;
        fire.bounds = BoundingBox(R().ToWorld(kFireX - 1.1f, 0.0f, kFireZ - 1.1f), R().ToWorld(kFireX + 1.1f, 2.4f, kFireZ + 1.1f));
        fire.maxDistance = 5.0f;
        fire.prompt = [this] { return std::string("Stoke the fire (") + kFireNames[fire_] + ", next: " + kFireNames[(fire_ + 1) % 3] + ")"; };
        fire.activate = [this] {
            fire_ = (fire_ + 1) % 3;
            for (int i = 0; i < 40; ++i) EmitEmber();
        };
        interaction.Add(std::move(fire));

        Interactable pool;
        pool.bounds = BoundingBox(R().ToWorld(kPoolX - 1.9f, 0.0f, kPoolZ - 1.9f), R().ToWorld(kPoolX + 1.9f, 2.4f, kPoolZ + 1.9f));
        pool.maxDistance = 5.5f;
        pool.prompt = [this] { return std::string("Fountain: ") + kFountainNames[fountain_] + "  (next)"; };
        pool.activate = [this] {
            fountain_ = (fountain_ + 1) % 3;
            if (S().audio != nullptr) S().audio->SetPlaying(waterVoice_, fountain_ != 0);
        };
        interaction.Add(std::move(pool));

        Interactable sparkler;
        sparkler.bounds = BoundingBox(R().ToWorld(kSparkX - 0.55f, 0.0f, kSparkZ - 0.55f), R().ToWorld(kSparkX + 0.55f, kSparkTop + 0.8f, kSparkZ + 0.55f));
        sparkler.prompt = [] { return std::string("Fire the sparks"); };
        sparkler.activate = [this] {
            pendingSparks_ += 130;
            if (S().audio != nullptr) S().audio->PlayAt("spark", R().ToWorld(kSparkX, kSparkTop, kSparkZ), 0.8f, 14.0f);
        };
        interaction.Add(std::move(sparkler));

        // Seed the dust motes through the whole room.
        for (int i = 0; i < 100; ++i) {
            Particle p;
            p.position = R().ToWorld(Rand(-R().halfW + 0.5f, R().halfW - 0.5f), Rand(0.3f, R().height - 0.4f), Rand(-R().halfD + 0.5f, R().halfD - 0.5f));
            p.velocity = Vector3(Rand(-0.06f, 0.06f), Rand(-0.02f, 0.05f), Rand(-0.06f, 0.06f));
            p.life = 1e9f;
            p.size0 = p.size1 = Rand(0.015f, 0.035f);
            p.colour0 = p.colour1 = Color(255, 235, 200, static_cast<int>(Rand(35.0f, 90.0f)));
            dust_.Emit(p);
        }

        if (S().audio != nullptr) {
            fireVoice_ = CreateLoop("fire", R().ToWorld(kFireX, 0.6f, kFireZ), 0.55f, 11.0f);
            waterVoice_ = CreateLoop("water", R().ToWorld(kPoolX, 0.6f, kPoolZ), 0.6f, 11.0f);
            S().audio->SetPlaying(fireVoice_, true);
            S().audio->SetPlaying(waterVoice_, true);
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        const float dt = ctx.dt;
        time_ += dt;
        const float level = kFireScale[fire_];

        // Flames, embers and smoke over the fire pit.
        flameBudget_ += dt * 150.0f * level;
        for (; flameBudget_ >= 1.0f; flameBudget_ -= 1.0f) EmitFlame(level);
        smokeBudget_ += dt * 15.0f * level;
        for (; smokeBudget_ >= 1.0f; smokeBudget_ -= 1.0f) EmitSmoke(level);
        emberBudget_ += dt * 9.0f * level;
        for (; emberBudget_ >= 1.0f; emberBudget_ -= 1.0f) EmitEmber();

        // Fountain jet and the splashes where it lands.
        if (fountain_ != 0) {
            waterBudget_ += dt * (fountain_ == 2 ? 300.0f : 190.0f);
            for (; waterBudget_ >= 1.0f; waterBudget_ -= 1.0f) EmitWater();
            splashBudget_ += dt * (fountain_ == 2 ? 55.0f : 35.0f);
            for (; splashBudget_ >= 1.0f; splashBudget_ -= 1.0f) EmitSplash();
        }

        // Spark bursts requested by the pedestal are released over a few frames.
        const int now = std::min(pendingSparks_, 50);
        for (int i = 0; i < now; ++i) EmitSpark();
        pendingSparks_ -= now;

        ParticleSystem::Physics flame;
        flame.gravity = Vector3(0, 0.9f, 0);
        flame.drag = 0.4f;
        flames_.Update(dt, flame);
        ParticleSystem::Physics rising;
        rising.gravity = Vector3(0.1f, 0.15f, 0.05f);
        rising.drag = 0.25f;
        smoke_.Update(dt, rising);
        ParticleSystem::Physics ember;
        ember.gravity = Vector3(0, 0.25f, 0);
        ember.drag = 0.3f;
        embers_.Update(dt, ember);
        ParticleSystem::Physics fall;
        fall.gravity = Vector3(0, -9.8f, 0);
        fall.hasFloor = true;
        fall.floorY = kWaterY;
        water_.Update(dt, fall);
        ParticleSystem::Physics spl;
        spl.gravity = Vector3(0, -7.0f, 0);
        spl.hasFloor = true;
        spl.floorY = kWaterY;
        splash_.Update(dt, spl);
        ParticleSystem::Physics spark;
        spark.gravity = Vector3(0, -9.8f, 0);
        spark.hasFloor = true;
        spark.floorY = 0.02f;
        spark.bounce = 0.42f;
        sparks_.Update(dt, spark);

        // Dust motes drift and wrap around the room.
        ParticleSystem::Physics none;
        dust_.Update(dt, none);
        const BoundingBox box = R().Interior();
        for (Particle& p : dust_.MutableParticles()) {
            if (p.position.X < box.Min.X) p.position.X = box.Max.X;
            if (p.position.X > box.Max.X) p.position.X = box.Min.X;
            if (p.position.Z < box.Min.Z) p.position.Z = box.Max.Z;
            if (p.position.Z > box.Max.Z) p.position.Z = box.Min.Z;
            if (p.position.Y < 0.2f) p.position.Y = box.Max.Y - 0.3f;
            if (p.position.Y > box.Max.Y - 0.2f) p.position.Y = 0.3f;
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        GraphicsDevice& device = ctx.device;
        RenderStates::SetOpaque3D(device);
        // Logs for the fire, lit by a flickering CNA point light placed in the flames.
        BasicEffect& fx = ctx.fx.BeginLit(S().textures->Get("wood"));
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setAmbientLightColorProperty(Vector3(0.05f, 0.04f, 0.035f));
        fx.getDirectionalLight0Property().setDiffuseColorProperty(Vector3(0.12f, 0.12f, 0.14f));
        fx.getDirectionalLight1Property().setEnabledProperty(false);
        fx.getDirectionalLight2Property().setEnabledProperty(false);
        const float flicker = 1.0f + 0.25f * std::sin(time_ * 23.0f) * std::sin(time_ * 7.3f) + 0.15f * std::sin(time_ * 41.0f);
        PunctualLightEXT glow;
        glow.Kind = PunctualLightKindEXT::Point;
        glow.Position = R().ToWorld(kFireX, 0.7f, kFireZ);
        glow.DiffuseColor = Vector3(1.0f, 0.5f, 0.18f) * (1.6f * flicker * kFireScale[fire_]);
        glow.Range = 4.5f;
        fx.setPunctualLightEXT(glow);
        fx.setDiffuseColorProperty(Vector3(0.7f, 0.55f, 0.45f));
        fx.setSpecularColorProperty(Vector3(0, 0, 0));
        fx.setEmissiveColorProperty(Vector3(0.12f, 0.03f, 0.0f) * kFireScale[fire_]);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        const Vector3 base = R().ToWorld(kFireX, 0.12f, kFireZ);
        for (int i = 0; i < 3; ++i) {
            const float a = static_cast<float>(i) * MathHelper::TwoPi / 3.0f + 0.4f;
            fx.setWorldProperty(Matrix::CreateScale(0.17f, 1.15f, 0.17f) * Matrix::CreateRotationZ(MathHelper::PiOver2) * Matrix::CreateTranslation(0.575f, 0.1f, 0) *
                                Matrix::CreateRotationY(a) * Matrix::CreateTranslation(base));
            EffectBank::Apply(fx);
            S().shapes->Cylinder().Draw(device);
        }
        fx.setPunctualLightEXT(PunctualLightEXT{});
    }

    void DrawTransparent(const DrawContext& ctx) override
    {
        GraphicsDevice& device = ctx.device;
        // Water surface: BasicEffect's Alpha scales the colour (premultiplied), so it blends with AlphaBlend.
        {
            RenderStates::SetTransparent3D(device, BlendState::AlphaBlend);
            device.setRasterizerStateProperty(RasterizerState::CullClockwise);
            BasicEffect& fx = ctx.fx.BeginLit();
            fx.EnableDefaultLighting();
            fx.setPreferPerPixelLightingProperty(true);
            fx.setDiffuseColorProperty(Vector3(0.25f, 0.5f, 0.85f));
            fx.setSpecularColorProperty(Vector3(0.9f, 0.9f, 1.0f));
            fx.setSpecularPowerProperty(60.0f);
            fx.setEmissiveColorProperty(Vector3(0, 0, 0));
            fx.setAlphaProperty(fountain_ == 0 ? 0.55f : 0.7f);
            fx.setViewProperty(ctx.view.view);
            fx.setProjectionProperty(ctx.view.projection);
            fx.setWorldProperty(Matrix::CreateScale(2.0f * kPoolHalf, 1.0f, 2.0f * kPoolHalf) * Matrix::CreateTranslation(R().ToWorld(kPoolX, kWaterY, kPoolZ)));
            EffectBank::Apply(fx);
            S().shapes->Plane().Draw(device);
            fx.setAlphaProperty(1.0f);
        }
        Texture2D* soft = S().textures->Get("particle");
        renderer_->Draw(ctx, S().textures->Get("smoke"), BlendState::NonPremultiplied, true, {&smoke_});
        renderer_->Draw(ctx, soft, BlendState::NonPremultiplied, false, {&water_, &dust_});
        renderer_->Draw(ctx, soft, BlendState::Additive, false, {&flames_, &embers_, &sparks_, &splash_});
        RenderStates::SetOpaque3D(device);
    }

private:
    float Rand(float lo, float hi) { return lo + (hi - lo) * uni_(rng_); }

    void EmitFlame(float level)
    {
        Particle p;
        const float a = Rand(0.0f, MathHelper::TwoPi);
        const float r = Rand(0.0f, 0.3f);
        p.position = R().ToWorld(kFireX + std::cos(a) * r, 0.2f, kFireZ + std::sin(a) * r);
        p.velocity = Vector3(Rand(-0.2f, 0.2f), Rand(1.3f, 2.2f) * std::sqrt(level), Rand(-0.2f, 0.2f));
        p.life = Rand(0.45f, 0.95f);
        p.size0 = Rand(0.28f, 0.42f) * std::sqrt(level);
        p.size1 = 0.04f;
        p.rotation = Rand(0.0f, MathHelper::TwoPi);
        p.spin = Rand(-2.0f, 2.0f);
        p.colour0 = Color(255, 225, 110, 235);
        p.colour1 = Color(255, 60, 8, 0);
        flames_.Emit(p);
    }

    void EmitSmoke(float level)
    {
        Particle p;
        p.position = R().ToWorld(kFireX + Rand(-0.25f, 0.25f), 1.0f, kFireZ + Rand(-0.25f, 0.25f));
        p.velocity = Vector3(Rand(-0.12f, 0.12f), Rand(0.6f, 1.0f) * std::sqrt(level), Rand(-0.12f, 0.12f));
        p.life = Rand(2.2f, 3.6f);
        p.size0 = Rand(0.22f, 0.3f);
        p.size1 = Rand(1.0f, 1.5f);
        p.rotation = Rand(0.0f, MathHelper::TwoPi);
        p.spin = Rand(-0.4f, 0.4f);
        p.colour0 = Color(70, 68, 66, 120);
        p.colour1 = Color(35, 35, 38, 0);
        smoke_.Emit(p);
    }

    void EmitEmber()
    {
        Particle p;
        p.position = R().ToWorld(kFireX + Rand(-0.3f, 0.3f), 0.5f, kFireZ + Rand(-0.3f, 0.3f));
        p.velocity = Vector3(Rand(-0.6f, 0.6f), Rand(1.4f, 2.8f), Rand(-0.6f, 0.6f));
        p.life = Rand(1.3f, 2.4f);
        p.size0 = Rand(0.035f, 0.06f);
        p.size1 = 0.015f;
        p.colour0 = Color(255, 190, 80, 255);
        p.colour1 = Color(255, 60, 0, 0);
        embers_.Emit(p);
    }

    void EmitWater()
    {
        Particle p;
        const float a = Rand(0.0f, MathHelper::TwoPi);
        const float spread = Rand(0.25f, fountain_ == 2 ? 1.25f : 0.8f);
        const float up = fountain_ == 2 ? Rand(5.4f, 6.4f) : Rand(3.7f, 4.4f);
        p.position = R().ToWorld(kPoolX, kSpoutTop + 0.15f, kPoolZ);
        p.velocity = Vector3(std::cos(a) * spread, up, std::sin(a) * spread);
        p.life = 3.0f;   // ends when it reaches the water
        p.size0 = Rand(0.05f, 0.09f);
        p.size1 = p.size0;
        p.colour0 = Color(185, 220, 255, 215);
        p.colour1 = Color(225, 242, 255, 110);
        water_.Emit(p);
    }

    void EmitSplash()
    {
        Particle p;
        const float a = Rand(0.0f, MathHelper::TwoPi);
        const float r = Rand(0.3f, fountain_ == 2 ? 1.3f : 0.9f);
        p.position = R().ToWorld(kPoolX + std::cos(a) * r, kWaterY, kPoolZ + std::sin(a) * r);
        p.velocity = Vector3(Rand(-0.3f, 0.3f), Rand(0.9f, 1.7f), Rand(-0.3f, 0.3f));
        p.life = Rand(0.35f, 0.7f);
        p.size0 = Rand(0.06f, 0.1f);
        p.size1 = 0.02f;
        p.colour0 = Color(210, 235, 255, 200);
        p.colour1 = Color(210, 235, 255, 0);
        splash_.Emit(p);
    }

    void EmitSpark()
    {
        Particle p;
        const float a = Rand(0.0f, MathHelper::TwoPi);
        const float cone = Rand(0.0f, 0.9f);
        const float speed = Rand(2.8f, 6.0f);
        p.position = R().ToWorld(kSparkX, kSparkTop + 0.05f, kSparkZ);
        p.velocity = Vector3(std::sin(cone) * std::cos(a), std::cos(cone), std::sin(cone) * std::sin(a)) * speed;
        p.life = Rand(1.0f, 1.9f);
        p.size0 = Rand(0.05f, 0.085f);
        p.size1 = 0.02f;
        p.colour0 = Color(255, 240, 160, 255);
        p.colour1 = Color(255, 90, 10, 0);
        sparks_.Emit(p);
    }

    std::mt19937 rng_;
    std::uniform_real_distribution<float> uni_{0.0f, 1.0f};
    ParticleSystem flames_, smoke_, embers_, water_, splash_, sparks_, dust_;
    std::unique_ptr<ParticleRenderer> renderer_;
    Voice3D* fireVoice_ = nullptr;
    Voice3D* waterVoice_ = nullptr;
    int fire_ = 1;
    int fountain_ = 2;
    int pendingSparks_ = 0;
    float flameBudget_ = 0, smokeBudget_ = 0, emberBudget_ = 0, waterBudget_ = 0, splashBudget_ = 0;
    float time_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeParticleRoom(const RoomDef& room, Services& services)
{
    return std::make_unique<ParticleRoom>(room, services);
}
