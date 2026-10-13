#include <cmath>
#include <string>

#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ModelUtil.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/PunctualLightEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/ModelLibrary.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SignFactory.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const Color kGold(255, 226, 150, 255);
const Color kText(222, 228, 240, 255);

constexpr float kConsoleZ0 = 2.9f;
constexpr float kConsoleZ1 = 3.5f;
constexpr float kConsoleTop = 0.9f;
constexpr float kButtonSpacing = 0.74f;
constexpr int kButtonCount = 8;
const Vector3 kSubjectCentre(0.0f, 1.55f, 0.0f);   // local coordinates

struct DirectionalSpec {
    const char* name;
    Vector3 colour;
    Vector3 source;   // where the light sits, relative to the subject
};

const DirectionalSpec kDirectional[3] = {
    {"Key light", {1.0f, 0.30f, 0.22f}, {-2.7f, 0.9f, 2.4f}},
    {"Fill light", {0.25f, 1.0f, 0.35f}, {2.9f, 0.4f, 2.0f}},
    {"Rim light", {0.30f, 0.45f, 1.0f}, {0.6f, 2.2f, -3.0f}},
};

const char* const kButtonLabels[kButtonCount] = {"Key light", "Fill light", "Rim light", "Lamp", "Lighting", "Surface", "Ambient", "Orbit"};
const char* const kSurfaceNames[4] = {"matte white", "glossy white", "emissive glow", "marble texture"};
const char* const kLampNames[3] = {"off", "point light", "spot light"};
const char* const kAmbientNames[3] = {"dark", "dim", "bright"};
const char* const kSubjectNames[4] = {"sphere", "trefoil knot", "torus", "vase"};

class LightingRoom final : public Exhibit {
public:
    LightingRoom(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    float ButtonX(int i) const { return -2.59f + static_cast<float>(i) * kButtonSpacing; }

    void BuildStatic(RoomBuilder& b) override
    {
        // Control console in front of the subject.
        b.AddBox(Vector3(-3.1f, 0.0f, kConsoleZ0), Vector3(3.1f, kConsoleTop, kConsoleZ1), "metal", 1.0f, Vector3(0.45f, 0.47f, 0.55f));
        b.AddBox(Vector3(-3.15f, kConsoleTop, kConsoleZ0 - 0.04f), Vector3(3.15f, kConsoleTop + 0.04f, kConsoleZ1 + 0.04f), "metal", 1.0f,
                 Vector3(0.3f, 0.3f, 0.36f), false);
        for (int i = 0; i < kButtonCount; ++i) {
            SignSpec p;
            p.lines = {{kButtonLabels[i], true, kGold}};
            p.background = Color(18, 20, 30, 255);
            p.pixelWidth = 190;
            p.pixelHeight = 70;
            b.AddQuadFacing(Vector3(ButtonX(i), 0.45f, kConsoleZ1 + 0.012f), 0.0f, 0.68f, 0.24f, S().signs->Create(p), Vector3(1, 1, 1), 0.9f);
        }

        // Pedestals for the three test objects, and a lit disc to catch the light pools.
        b.AddPedestal(0.0f, 0.0f, 1.1f, 0.9f, "marble", Vector3(0.55f, 0.55f, 0.6f));
        b.AddPedestal(-3.7f, -1.4f, 0.8f, 0.9f, "marble", Vector3(0.55f, 0.55f, 0.6f));
        b.AddPedestal(3.7f, -1.4f, 0.8f, 0.9f, "marble", Vector3(0.55f, 0.55f, 0.6f));

        SignSpec title;
        title.lines = {{"Lighting", true, kGold},
                       {"BasicEffect: up to three directional lights, ambient colour, per-pixel or", false, kText},
                       {"per-vertex shading, specular, emissive; plus CNA's point and spot light.", false, kText},
                       {"Use the console to switch each light, the surface and the shading mode.", false, kText}};
        b.AddWallSign(Wall::West, 0.0f, 3.1f, 5.4f, 1.6f, title);
        b.AddLight(Vector3(0.0f, 4.5f, 5.0f), Vector3(0.7f, 0.75f, 1.0f), 0.35f, 8.0f);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        for (int i = 0; i < kButtonCount; ++i) {
            Interactable ia;
            const float x = ButtonX(i);
            ia.bounds = BoundingBox(R().ToWorld(x - 0.3f, kConsoleTop - 0.05f, kConsoleZ0 - 0.1f), R().ToWorld(x + 0.3f, kConsoleTop + 0.3f, kConsoleZ1 + 0.1f));
            ia.maxDistance = 4.2f;
            ia.prompt = [this, i] { return ButtonPrompt(i); };
            ia.activate = [this, i] { Press(i); };
            interaction.Add(std::move(ia));
        }
        Interactable subject;
        subject.bounds = BoundingBox(R().ToWorld(-0.6f, 0.0f, -0.6f), R().ToWorld(0.6f, 2.4f, 0.6f));
        subject.maxDistance = 6.0f;
        subject.prompt = [this] { return std::string("Change the test object (") + kSubjectNames[subject_] + ")"; };
        subject.activate = [this] { subject_ = (subject_ + 1) % 4; };
        interaction.Add(std::move(subject));
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        spin_ += ctx.dt * 0.5f;
        if (orbit_) orbitAngle_ += ctx.dt * 0.45f;
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        GraphicsDevice& device = ctx.device;
        RenderStates::SetOpaque3D(device);
        const Vector3 centre = R().ToWorld(kSubjectCentre.X, kSubjectCentre.Y, kSubjectCentre.Z);

        // Configure BasicEffect's lights from the console state.
        BasicEffect& fx = ctx.fx.BeginLit(surface_ == 3 ? S().textures->Get("marble") : nullptr);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setPreferPerPixelLightingProperty(perPixel_);
        const float ambient[3] = {0.02f, 0.14f, 0.38f};
        fx.setAmbientLightColorProperty(Vector3(ambient[ambientLevel_], ambient[ambientLevel_], ambient[ambientLevel_] * 1.1f));
        DirectionalLight* lights[3] = {&fx.getDirectionalLight0Property(), &fx.getDirectionalLight1Property(), &fx.getDirectionalLight2Property()};
        const Matrix orbit = Matrix::CreateRotationY(orbitAngle_);
        for (int i = 0; i < 3; ++i) {
            const Vector3 source = Vector3::Transform(kDirectional[i].source, orbit);
            lights[i]->setEnabledProperty(lightOn_[i]);
            lights[i]->setDirectionProperty(Vector3::Normalize(-source));
            lights[i]->setDiffuseColorProperty(kDirectional[i].colour * 0.95f);
            lights[i]->setSpecularColorProperty(kDirectional[i].colour);
        }
        PunctualLightEXT lamp;
        if (lamp_ == 1) {
            lamp.Kind = PunctualLightKindEXT::Point;
            lamp.Position = LampPosition();
            lamp.DiffuseColor = Vector3(1.0f, 0.82f, 0.5f) * 1.4f;
            lamp.Range = 9.0f;
        } else if (lamp_ == 2) {
            lamp.Kind = PunctualLightKindEXT::Spot;
            lamp.Position = SpotPosition();
            const Vector3 aim = centre + Off(std::sin(time_ * 0.8f) * 1.6f, -centre.Y + 0.1f, std::cos(time_ * 0.6f) * 1.2f);
            lamp.Direction = Vector3::Normalize(aim - lamp.Position);
            lamp.DiffuseColor = Vector3(0.95f, 0.97f, 1.0f) * 1.6f;
            lamp.Range = 12.0f;
            lamp.InnerAngle = 0.20f;
            lamp.OuterAngle = 0.36f;
        }
        fx.setPunctualLightEXT(lamp);

        const float specPower = surface_ == 1 ? 70.0f : (surface_ == 3 ? 40.0f : 12.0f);
        const Vector3 specular = (surface_ == 1) ? Vector3(1, 1, 1) : (surface_ == 3 ? Vector3(0.7f, 0.7f, 0.75f) : Vector3(0.0f, 0.0f, 0.0f));
        const Vector3 emissive = surface_ == 2 ? Vector3(0.55f, 0.42f, 0.12f) : Vector3(0, 0, 0);
        fx.setDiffuseColorProperty(surface_ == 2 ? Vector3(0.5f, 0.5f, 0.5f) : Vector3(0.92f, 0.92f, 0.92f));
        fx.setSpecularColorProperty(specular);
        fx.setSpecularPowerProperty(specPower);
        fx.setEmissiveColorProperty(emissive);

        // A lit patch of floor under the subject so the light pools are visible. BeginLit hands back the
        // same shared effect, so the lights configured above stay in place; only texture and material change.
        {
            BasicEffect& floor = ctx.fx.BeginLit(S().textures->Get("stone"));
            floor.setDiffuseColorProperty(Vector3(0.75f, 0.75f, 0.78f));
            floor.setSpecularColorProperty(Vector3(0, 0, 0));
            floor.setEmissiveColorProperty(Vector3(0, 0, 0));
            floor.setWorldProperty(Matrix::CreateScale(9.0f, 1.0f, 9.0f) * Matrix::CreateTranslation(R().ToWorld(0.0f, 0.02f, 0.0f)));
            EffectBank::Apply(floor);
            S().shapes->Plane().Draw(device);
        }

        // Subjects: rebind the surface material on the shared effect.
        ConfigureSubject(ctx, fx, centre);
        const float s = spin_;
        // Centre: selectable object.
        const Matrix centreWorld = Matrix::CreateRotationY(s) * Matrix::CreateTranslation(centre);
        switch (subject_) {
            case 0:
                fx.setWorldProperty(Matrix::CreateScale(1.15f) * centreWorld);
                EffectBank::Apply(fx);
                S().shapes->Sphere().Draw(device);
                break;
            case 1:
                DrawModel(ctx, fx, "knot", Matrix::CreateScale(1.1f) * Matrix::CreateRotationX(0.5f) * centreWorld);
                break;
            case 2:
                DrawModel(ctx, fx, "torus", Matrix::CreateScale(1.2f) * Matrix::CreateRotationX(1.0f) * centreWorld);
                break;
            default:
                DrawModel(ctx, fx, "vase", Matrix::CreateScale(1.2f) * Matrix::CreateTranslation(0, -0.55f, 0) * centreWorld);
                break;
        }
        // Left and right witnesses.
        fx.setWorldProperty(Matrix::CreateScale(0.6f) * Matrix::CreateRotationY(s * 1.3f) * Matrix::CreateRotationX(0.5f) *
                            Matrix::CreateTranslation(R().ToWorld(-3.7f, 0.9f + 0.45f, -1.4f)));
        EffectBank::Apply(fx);
        S().shapes->Cube().Draw(device);
        fx.setWorldProperty(Matrix::CreateScale(0.6f, 0.8f, 0.6f) * Matrix::CreateRotationY(s) *
                            Matrix::CreateTranslation(R().ToWorld(3.7f, 0.9f, -1.4f)));
        EffectBank::Apply(fx);
        S().shapes->Cone().Draw(device);

        DrawGizmos(ctx, centre);
        ctx.fx.Lit().setPunctualLightEXT(PunctualLightEXT{});   // never leak the lamp into other rooms
    }

private:
    void ConfigureSubject(const DrawContext& ctx, BasicEffect& fx, const Vector3&)
    {
        (void)ctx;
        // Re-bind the surface texture: the floor pass above replaced it on the shared effect.
        fx.setTextureEnabledProperty(true);
        fx.setTextureProperty(surface_ == 3 ? S().textures->Get("marble") : S().textures->White());
        const Vector3 specular = (surface_ == 1) ? Vector3(1, 1, 1) : (surface_ == 3 ? Vector3(0.7f, 0.7f, 0.75f) : Vector3(0.0f, 0.0f, 0.0f));
        fx.setDiffuseColorProperty(surface_ == 2 ? Vector3(0.5f, 0.5f, 0.5f) : Vector3(0.92f, 0.92f, 0.92f));
        fx.setSpecularColorProperty(specular);
        fx.setSpecularPowerProperty(surface_ == 1 ? 70.0f : (surface_ == 3 ? 40.0f : 12.0f));
        fx.setEmissiveColorProperty(surface_ == 2 ? Vector3(0.55f, 0.42f, 0.12f) : Vector3(0, 0, 0));
    }

    void DrawModel(const DrawContext& ctx, BasicEffect& fx, const char* name, const Matrix& world)
    {
        Model* model = S().models->Get(name);
        if (model == nullptr) return;
        const Vector3 white(0.92f, 0.92f, 0.92f);
        ModelUtil::DrawWithEffect(ctx.device, *model, fx, world, surface_ == 2 ? nullptr : &white);
    }

    [[nodiscard]] Vector3 LampPosition() const
    {
        return R().ToWorld(std::cos(time_ * 0.9f) * 2.6f, 2.3f + std::sin(time_ * 1.7f) * 0.4f, std::sin(time_ * 0.9f) * 2.6f);
    }
    [[nodiscard]] Vector3 SpotPosition() const { return R().ToWorld(3.4f, 4.5f, 3.0f); }

    void DrawGizmos(const DrawContext& ctx, const Vector3& centre)
    {
        GraphicsDevice& device = ctx.device;
        // Light sources: small emissive spheres, joined to the subject by a thin line.
        BasicEffect& fx = ctx.fx.BeginLit();
        fx.setLightingEnabledProperty(false);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        const Matrix orbit = Matrix::CreateRotationY(orbitAngle_);
        VertexPositionColor lines[8];
        int n = 0;
        const auto marker = [&](const Vector3& p, const Vector3& colour, float size, bool on) {
            fx.setDiffuseColorProperty(on ? colour : colour * 0.18f);
            fx.setWorldProperty(Matrix::CreateScale(size) * Matrix::CreateTranslation(p));
            EffectBank::Apply(fx);
            S().shapes->Sphere().Draw(device);
        };
        for (int i = 0; i < 3; ++i) {
            const Vector3 p = centre + Vector3::Transform(kDirectional[i].source, orbit);
            marker(p, kDirectional[i].colour, 0.28f, lightOn_[i]);
            if (lightOn_[i] && n + 2 <= 8) {
                const Color c(static_cast<std::uint8_t>(kDirectional[i].colour.X * 255), static_cast<std::uint8_t>(kDirectional[i].colour.Y * 255),
                              static_cast<std::uint8_t>(kDirectional[i].colour.Z * 255), static_cast<std::uint8_t>(255));
                lines[n++] = VertexPositionColor(p, c);
                lines[n++] = VertexPositionColor(centre, c);
            }
        }
        if (lamp_ == 1) {
            marker(LampPosition(), Vector3(1.0f, 0.85f, 0.5f), 0.3f, true);
        } else if (lamp_ == 2) {
            marker(SpotPosition(), Vector3(0.9f, 0.95f, 1.0f), 0.34f, true);
        }
        if (n > 0) {
            BasicEffect& flat = ctx.fx.Flat();
            flat.setWorldProperty(Matrix::getIdentityProperty());
            flat.setViewProperty(ctx.view.view);
            flat.setProjectionProperty(ctx.view.projection);
            EffectBank::Apply(flat);
            device.DrawUserPrimitives(PrimitiveType::LineList, lines, 0, n / 2);
        }
        // Console buttons: lit when their feature is active.
        const bool active[kButtonCount] = {lightOn_[0], lightOn_[1], lightOn_[2], lamp_ != 0, perPixel_, surface_ != 0, ambientLevel_ != 0, orbit_};
        const Vector3 colours[kButtonCount] = {kDirectional[0].colour, kDirectional[1].colour, kDirectional[2].colour,
                                               {1.0f, 0.85f, 0.4f}, {0.5f, 0.9f, 1.0f}, {0.9f, 0.5f, 1.0f}, {0.8f, 0.8f, 0.8f}, {1.0f, 0.6f, 0.2f}};
        for (int i = 0; i < kButtonCount; ++i) {
            fx.setDiffuseColorProperty(active[i] ? colours[i] : colours[i] * 0.16f);
            fx.setWorldProperty(Matrix::CreateScale(0.48f, 0.10f, 0.4f) *
                                Matrix::CreateTranslation(R().ToWorld(ButtonX(i), kConsoleTop + 0.09f, (kConsoleZ0 + kConsoleZ1) / 2.0f)));
            EffectBank::Apply(fx);
            S().shapes->Cube().Draw(device);
        }
    }

    [[nodiscard]] std::string ButtonPrompt(int i) const
    {
        switch (i) {
            case 0: case 1: case 2:
                return std::string(lightOn_[i] ? "Switch off the " : "Switch on the ") + kDirectional[i].name;
            case 3: return std::string("Lamp: ") + kLampNames[lamp_] + "  (next: " + kLampNames[(lamp_ + 1) % 3] + ")";
            case 4: return std::string("Shading: ") + (perPixel_ ? "per-pixel" : "per-vertex") + "  (switch)";
            case 5: return std::string("Surface: ") + kSurfaceNames[surface_] + "  (next)";
            case 6: return std::string("Ambient light: ") + kAmbientNames[ambientLevel_] + "  (next)";
            default: return std::string(orbit_ ? "Stop the lights orbiting" : "Let the lights orbit");
        }
    }

    void Press(int i)
    {
        switch (i) {
            case 0: case 1: case 2: lightOn_[i] = !lightOn_[i]; break;
            case 3: lamp_ = (lamp_ + 1) % 3; break;
            case 4: perPixel_ = !perPixel_; break;
            case 5: surface_ = (surface_ + 1) % 4; break;
            case 6: ambientLevel_ = (ambientLevel_ + 1) % 3; break;
            default: orbit_ = !orbit_; break;
        }
    }

    bool lightOn_[3] = {true, true, true};
    int lamp_ = 0;
    bool perPixel_ = true;
    int surface_ = 1;
    int ambientLevel_ = 1;
    bool orbit_ = true;
    int subject_ = 0;
    float time_ = 0.0f;
    float spin_ = 0.0f;
    float orbitAngle_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeLightingRoom(const RoomDef& room, Services& services)
{
    return std::make_unique<LightingRoom>(room, services);
}
