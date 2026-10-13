#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ModelUtil.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/ModelLibrary.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SignFactory.hpp"
#include "Rendering/SkyDome.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const Color kGold(255, 226, 150, 255);
const Color kText(222, 228, 240, 255);

constexpr float kSundialX = 0.0f, kSundialZ = -1.0f;
constexpr float kGnomonHeight = 1.4f;
constexpr float kPlinthX = -4.6f, kPlinthZ = -4.6f;
constexpr float kPlinthHeight = 0.9f;

const float kSpeeds[4] = {0.0f, 0.15f, 0.6f, 2.0f};   // hours of museum-day per real second
const char* const kSpeedNames[4] = {"paused", "slow", "fast", "very fast"};

std::string ClockText(float hours)
{
    const int total = static_cast<int>(hours * 60.0f) % (24 * 60);
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%02d:%02d", total / 60, total % 60);
    return buffer;
}

class SkyRoom final : public Exhibit {
public:
    SkyRoom(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        // Sundial: a round stone dial with a vertical gnomon. Its shadow is drawn dynamically.
        b.Meshes().Opaque("stone").AddCylinder(b.World(kSundialX, 0.0f, kSundialZ), 1.9f, 0.08f, 32, 1.0f, Vector3(0.85f, 0.83f, 0.8f), true);
        b.AddCollider(BoundingBox(b.World(kSundialX - 0.15f, 0.0f, kSundialZ - 0.15f), b.World(kSundialX + 0.15f, kGnomonHeight, kSundialZ + 0.15f)));
        b.Meshes().Opaque("metal").AddCylinder(b.World(kSundialX, 0.08f, kSundialZ), 0.06f, kGnomonHeight, 10, 1.0f, Vector3(0.8f, 0.7f, 0.4f), true);
        for (int i = 0; i < 12; ++i) {
            const float a = static_cast<float>(i) / 12.0f * MathHelper::TwoPi;
            b.Meshes().Opaque("white").AddCylinder(b.World(kSundialX + std::cos(a) * 1.65f, 0.08f, kSundialZ + std::sin(a) * 1.65f), 0.05f, 0.02f, 8, 1.0f,
                                                     Vector3(0.1f, 0.1f, 0.1f), true);
        }

        // Plinth for a model that shows the directional light.
        b.AddPedestal(kPlinthX, kPlinthZ, 0.9f, kPlinthHeight, "marble", Vector3(0.95f, 0.95f, 0.95f));
        SignSpec plinth;
        plinth.lines = {{"Sunlit sculpture", true, kGold}, {"a DirectionalLight driven by TimeOfDay", false, kText}};
        plinth.background = Color(26, 24, 30, 255);
        b.AddPedestalPlaque(kPlinthX, kPlinthZ, 0.9f, 0.5f, plinth);

        // Control console on the east side, clear of the door at the middle of the east wall.
        b.AddBox(Vector3(4.6f, 0.0f, -5.4f), Vector3(5.4f, 0.9f, -2.4f), "wood", 1.0f, Vector3(0.8f, 0.7f, 0.6f));

        SignSpec title;
        title.lines = {{"The sky", true, kGold},
                       {"A dome of vertex-coloured triangles rebuilt every frame in a DynamicVertexBuffer,", false, kText},
                       {"with sun and moon billboards, stars and a scrolling cloud layer.", false, kText}};
        b.AddWallSign(Wall::North, -3.0f, 3.2f, 5.4f, 1.3f, title);
        SignSpec sun;
        sun.lines = {{"Time of day", true, kGold}, {"press the console to change the hour or the speed", false, kText}};
        b.AddWallSign(Wall::North, 3.6f, 3.2f, 3.8f, 0.9f, sun);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        sculpture_ = S().models != nullptr ? S().models->Get("knot") : nullptr;

        auto button = [&](float z, std::function<std::string()> prompt, std::function<void()> action) {
            Interactable ia;
            ia.bounds = BoundingBox(R().ToWorld(4.5f, 0.0f, z - 0.45f), R().ToWorld(5.5f, 1.4f, z + 0.45f));
            ia.maxDistance = 3.6f;
            ia.prompt = std::move(prompt);
            ia.activate = std::move(action);
            interaction.Add(std::move(ia));
        };
        button(-5.0f, [this] { return "Next hour (now " + ClockText(Time().Hours()) + ")"; }, [this] { Time().Advance(1.0f); });
        button(-4.0f, [this] { return "Previous hour (now " + ClockText(Time().Hours()) + ")"; }, [this] { Time().Advance(-1.0f); });
        button(-3.0f, [this] { return std::string("Clock speed: ") + kSpeedNames[speed_] + " (next: " + kSpeedNames[(speed_ + 1) % 4] + ")"; },
               [this] { speed_ = (speed_ + 1) % 4; });
    }

    void Update(const UpdateContext& ctx) override
    {
        if (S().timeOfDay == nullptr) return;
        if (speed_ != 0) S().timeOfDay->Advance(ctx.dt * kSpeeds[speed_]);
        spin_ += ctx.dt * 0.6f;
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (S().timeOfDay == nullptr) return;
        GraphicsDevice& device = ctx.device;
        const TimeOfDay& tod = *S().timeOfDay;
        const Vector3 sunDir = tod.SunDirection();   // points towards the sun
        RenderStates::SetOpaque3D(device);

        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setAmbientLightColorProperty(tod.AmbientColor());
        fx.getDirectionalLight0Property().setEnabledProperty(true);
        fx.getDirectionalLight0Property().setDirectionProperty(-sunDir);
        fx.getDirectionalLight0Property().setDiffuseColorProperty(tod.SunColor());
        fx.getDirectionalLight0Property().setSpecularColorProperty(tod.SunColor() * 0.5f);
        fx.getDirectionalLight1Property().setEnabledProperty(false);
        fx.getDirectionalLight2Property().setEnabledProperty(false);
        fx.setSpecularColorProperty(Vector3(0.4f, 0.4f, 0.4f));
        fx.setSpecularPowerProperty(32.0f);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);

        if (sculpture_ != nullptr) {
            const Vector3 top = R().ToWorld(kPlinthX, kPlinthHeight + 0.7f, kPlinthZ);
            ModelUtil::DrawWithEffect(device, *sculpture_, fx,
                                      Matrix::CreateScale(1.0f) * Matrix::CreateRotationX(0.45f) * Matrix::CreateRotationY(spin_) * Matrix::CreateTranslation(top));
        }

        // Gnomon shadow: a thin dark slab lying on the dial, pointing away from the sun. It fades at night.
        if (sunDir.Y > 0.03f) {
            Vector3 flat(-sunDir.X, 0.0f, -sunDir.Z);
            const float len = std::min(flat.Length() / sunDir.Y * kGnomonHeight, 3.4f);
            if (flat.LengthSquared() > 1e-6f) {
                flat = Vector3::Normalize(flat);
                const float yaw = std::atan2(-flat.Z, flat.X);
                const Vector3 centre = R().ToWorld(kSundialX, 0.095f, kSundialZ) + flat * (len * 0.5f);
                fx.setAmbientLightColorProperty(Vector3(0, 0, 0));
                fx.getDirectionalLight0Property().setEnabledProperty(false);
                fx.setDiffuseColorProperty(Vector3(0.02f, 0.02f, 0.02f));
                fx.setSpecularColorProperty(Vector3(0, 0, 0));
                fx.setWorldProperty(Matrix::CreateScale(len, 0.01f, 0.12f) * Matrix::CreateRotationY(yaw) * Matrix::CreateTranslation(centre));
                EffectBank::Apply(fx);
                S().shapes->Cube().Draw(device);
            }
        }
    }

private:
    [[nodiscard]] TimeOfDay& Time() { return *S().timeOfDay; }

    Model* sculpture_ = nullptr;
    int speed_ = 1;
    float spin_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeSkyRoom(const RoomDef& room, Services& services)
{
    return std::make_unique<SkyRoom>(room, services);
}
