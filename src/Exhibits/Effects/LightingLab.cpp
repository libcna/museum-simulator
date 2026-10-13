// Lighting laboratory: directional, point and spot lights compared; attenuation; coloured lights and many lights
// by additive multi-pass (BasicEffect takes one punctual light per draw); a flashlight; emissive materials.
#include <algorithm>
#include <cmath>
#include <string>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Types, Falloff, Coloured, Many, Flashlight, Emissive };

Mode ParseMode(const std::string& s)
{
    if (s == "falloff") return Mode::Falloff;
    if (s == "coloured") return Mode::Coloured;
    if (s == "many") return Mode::Many;
    if (s == "flashlight") return Mode::Flashlight;
    if (s == "emissive") return Mode::Emissive;
    return Mode::Types;
}

struct Lamp {
    Vector3 position;
    Vector3 colour;
    float range;
    bool spot = false;
    Vector3 direction{0, -1, 0};
    float outer = 0.5f;
};

class LightingLab final : public Exhibit {
public:
    LightingLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Types:
                p.AddSlider("Intensity", 0.3f, 3.0f, intensity_, [this](float v) { intensity_ = v; });
                p.AddSlider("Light height", 1.0f, 4.0f, height_, [this](float v) { height_ = v; });
                p.AddToggle("Move the lights", true, [this](bool v) { move_ = v; });
                p.AddHeading("Left: directional.  Middle: point.  Right: spot");
                break;
            case Mode::Falloff:
                p.AddSlider("Range", 1.0f, 14.0f, range_, [this](float v) { range_ = v; }, "%.1f");
                p.AddSlider("Intensity", 0.5f, 6.0f, intensity_, [this](float v) { intensity_ = v; });
                p.AddToggle("Show the range sphere", true, [this](bool v) { showRange_ = v; });
                break;
            case Mode::Coloured:
                p.AddSlider("Orbit speed", 0.0f, 2.0f, speed_, [this](float v) { speed_ = v; });
                p.AddSlider("Intensity", 0.5f, 6.0f, intensity_, [this](float v) { intensity_ = v; });
                p.AddToggle("Red lamp", true, [this](bool v) { on_[0] = v; });
                p.AddToggle("Green lamp", true, [this](bool v) { on_[1] = v; });
                p.AddToggle("Blue lamp", true, [this](bool v) { on_[2] = v; });
                break;
            case Mode::Many:
                p.AddSlider("Lights", 1.0f, 24.0f, static_cast<float>(count_), [this](float v) { count_ = static_cast<int>(v); }, "%.0f", 1.0f);
                p.AddReadout("Passes per frame", [this] { return std::to_string(count_ + 1) + " (ambient + one per light)"; });
                p.AddReadout("Frame rate", [this] { return Fmt(S().stats->fps) + " fps"; });
                break;
            case Mode::Flashlight:
                p.AddToggle("Flashlight on", true, [this](bool v) { torch_ = v; });
                p.AddSlider("Beam angle", 8.0f, 60.0f, outer_, [this](float v) { outer_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Range", 3.0f, 20.0f, range_, [this](float v) { range_ = v; }, "%.0f");
                p.AddHeading("A spot light attached to your head");
                break;
            case Mode::Emissive:
                p.AddSlider("EmissiveColor", 0.0f, 1.0f, emissive_, [this](float v) { emissive_ = v; });
                p.AddToggle("Add a glow halo (additive blend)", true, [this](bool v) { halo_ = v; });
                p.AddHeading("Emissive surfaces glow without any light");
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override { fx_ = std::make_unique<BasicEffect>(*S().device); }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt * (mode_ == Mode::Coloured ? speed_ : 1.0f);
        if (ctx.playerInRoom) {
            eye_ = ctx.playerPosition;
            forward_ = ctx.playerForward;
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!fx_) return;
        GraphicsDevice& d = ctx.device;
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        const Vector3 centre = R().ToWorld(0, 0, 0);
        switch (mode_) {
            case Mode::Types: DrawTypes(ctx, centre); break;
            case Mode::Falloff: {
                Lamp l{centre + Off(-4.0f, 1.4f, 0), Vector3(1.0f, 0.9f, 0.7f) * intensity_, range_};
                DrawLit(ctx, centre, {l}, 0.05f);
                if (showRange_) gizmos_.Sphere(BoundingSphere(l.position, range_), Color(255, 220, 130, 255), 40);
                gizmos_.SolidSphere(ctx, l.position, 0.12f, Vector3(1, 0.9f, 0.5f));
                break;
            }
            case Mode::Coloured: {
                std::vector<Lamp> lamps;
                const Vector3 cols[3] = {{1.0f, 0.15f, 0.15f}, {0.15f, 1.0f, 0.2f}, {0.2f, 0.3f, 1.0f}};
                for (int i = 0; i < 3; ++i)
                    if (on_[i]) {
                        const float a = time_ + static_cast<float>(i) * 2.094f;
                        lamps.push_back({centre + Off(std::cos(a) * 2.2f, 1.6f, std::sin(a) * 2.2f), cols[i] * intensity_, 6.0f});
                        gizmos_.SolidSphere(ctx, lamps.back().position, 0.12f, cols[i]);
                    }
                DrawLit(ctx, centre, lamps, 0.03f);
                break;
            }
            case Mode::Many: {
                std::vector<Lamp> lamps;
                for (int i = 0; i < count_; ++i) {
                    const float a = time_ * 0.5f + static_cast<float>(i) * 0.8f, r = 1.0f + 0.14f * static_cast<float>(i % 12);
                    const Vector3 c = Vector3(0.5f + 0.5f * std::sin(static_cast<float>(i) * 1.7f), 0.5f + 0.5f * std::sin(static_cast<float>(i) * 2.3f + 1.0f), 0.5f + 0.5f * std::sin(static_cast<float>(i) * 3.1f + 2.0f));
                    lamps.push_back({centre + Off(std::cos(a) * r * 1.9f, 1.0f + 0.5f * std::sin(a * 1.3f), std::sin(a) * r * 1.9f), c * 1.6f, 3.2f});
                }
                DrawLit(ctx, centre, lamps, 0.03f);
                break;
            }
            case Mode::Flashlight: {
                std::vector<Lamp> lamps;
                if (torch_) {
                    Lamp l{eye_ + forward_ * 0.2f - Vector3(0, 0.15f, 0), Vector3(1.0f, 0.97f, 0.85f) * 2.5f, range_, true, Vector3::Normalize(forward_ + Vector3(1e-4f, 0, 0)), MathHelper::ToRadians(outer_)};
                    lamps.push_back(l);
                }
                DrawLit(ctx, centre, lamps, 0.01f);
                break;
            }
            case Mode::Emissive: DrawEmissive(ctx, centre); break;
        }
        RenderStates::SetOpaque3D(d);
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Directional, point and spot lights", "Attenuation and range", "Coloured lights", "Many lights by multi-pass", "A flashlight in the dark", "Emissive materials"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Types: return {"A directional light has only a direction (the sun): the same everywhere. A point light radiates from a place", "and fades with distance. A spot light adds a cone. Point and spot (PunctualLightEXT) are CNA extensions."};
            case Mode::Falloff: return {"Light from a point fades to nothing at its Range. The sphere shows that range; the boxes are lit less", "the farther they are. A large range with low intensity and a small one with high intensity look different."};
            case Mode::Coloured: return {"BasicEffect takes one punctual light per draw, so several lights are drawn in several passes added", "together with BlendState.Additive. Where the red, green and blue lamps overlap the surface turns white."};
            case Mode::Many: return {"The same multi-pass technique with up to 24 lights: one ambient pass plus one additive pass per light.", "Cost grows with lights x objects: watch the frame rate. Real engines batch lights; this is the plain way."};
            case Mode::Flashlight: return {"A spot light placed at your eye and aimed where you look: a flashlight. In a dark room only the beam", "shows the objects. The light is updated every frame from the camera position and direction."};
            default: return {"EmissiveColor adds light the surface gives off by itself: it needs no lamp. The left cubes have emissive", "colour and glow in the dark; the right ones are only diffuse. An additive halo fakes bloom."};
        }
    }

    BasicEffect& Prepare(const DrawContext& ctx, const Matrix& world, const Vector3& diffuse, const Vector3& ambient)
    {
        BasicEffect& fx = *fx_;
        fx.setLightingEnabledProperty(true);
        fx.setTextureEnabledProperty(true);
        fx.setVertexColorEnabledProperty(false);
        fx.setTextureProperty(S().textures->White());
        fx.setPreferPerPixelLightingProperty(true);
        fx.setAmbientLightColorProperty(ambient);
        fx.setDiffuseColorProperty(diffuse);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setSpecularColorProperty(Vector3(0.15f, 0.15f, 0.15f));
        fx.setSpecularPowerProperty(20.0f);
        fx.setWorldProperty(world);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        for (DirectionalLight* l : {&fx.getDirectionalLight0Property(), &fx.getDirectionalLight1Property(), &fx.getDirectionalLight2Property()}) l->setEnabledProperty(false);
        return fx;
    }

    // The scene every pass draws: a floor, columns, boxes and a sphere.
    void DrawScene(const DrawContext& ctx, const Vector3& centre, const Vector3& ambient, const Lamp* lamp)
    {
        BasicEffect& fx = *fx_;
        GraphicsDevice& d = ctx.device;
        const auto lampSetup = [&] {
            PunctualLightEXT pl;
            if (lamp != nullptr) {
                pl.Kind = lamp->spot ? PunctualLightKindEXT::Spot : PunctualLightKindEXT::Point;
                pl.Position = lamp->position;
                pl.Direction = lamp->direction;
                pl.DiffuseColor = lamp->colour;
                pl.Range = lamp->range;
                pl.OuterAngle = lamp->outer;
                pl.InnerAngle = lamp->outer * 0.6f;
            }
            fx.setPunctualLightEXT(pl);
        };
        const auto item = [&](const Matrix& m, const Vector3& colour, const LitMesh& mesh, Texture2D* tex = nullptr) {
            Prepare(ctx, m, colour, ambient);
            if (tex != nullptr) fx.setTextureProperty(tex);
            lampSetup();
            EffectBank::Apply(fx);
            mesh.Draw(d);
        };
        item(Matrix::CreateScale(10.0f, 1.0f, 10.0f) * Matrix::CreateTranslation(centre + Vector3(0, 0.03f, 0)), Vector3(0.85f, 0.85f, 0.85f), S().shapes->Plane(), S().textures->Get("stone"));
        for (int i = 0; i < 6; ++i) {
            const float x = -4.0f + static_cast<float>(i) * 1.6f;
            const float z = (i % 2 == 0) ? -2.2f : 2.2f;
            const Matrix m = Matrix::CreateScale(0.8f, 1.0f + 0.5f * static_cast<float>(i % 3), 0.8f) * Matrix::CreateTranslation(centre + Off(x, 0.03f, z));
            const LitMesh& mesh = (i % 3 == 0) ? S().shapes->Cylinder() : (i % 3 == 1 ? S().shapes->Cone() : S().shapes->Cylinder());
            item(m, Vector3(0.9f, 0.8f - 0.1f * static_cast<float>(i % 3), 0.7f), mesh);
        }
        item(Matrix::CreateScale(1.1f) * Matrix::CreateTranslation(centre + Vector3(0, 0.65f, 0)), Vector3(0.8f, 0.85f, 0.95f), S().shapes->Sphere());
        item(Matrix::CreateScale(0.9f) * Matrix::CreateRotationY(0.6f) * Matrix::CreateTranslation(centre + Off(2.4f, 0.48f, 0.6f)), Vector3(0.9f, 0.9f, 0.6f), S().shapes->Cube());
        fx.setPunctualLightEXT(PunctualLightEXT{});
    }

    // One ambient pass, then one additive pass per lamp.
    void DrawLit(const DrawContext& ctx, const Vector3& centre, const std::vector<Lamp>& lamps, float ambient)
    {
        GraphicsDevice& d = ctx.device;
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::Default);
        DrawScene(ctx, centre, Vector3(ambient, ambient, ambient * 1.3f), nullptr);
        static const DepthStencilState equalDepth = [] {
            DepthStencilState s;
            s.setDepthBufferWriteEnableProperty(false);
            s.setDepthBufferFunctionProperty(CompareFunction::LessEqual);
            return s;
        }();
        d.setBlendStateProperty(BlendState::Additive);
        d.setDepthStencilStateProperty(equalDepth);
        for (const Lamp& l : lamps) DrawScene(ctx, centre, Vector3(0, 0, 0), &l);
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::Default);
    }

    void DrawTypes(const DrawContext& ctx, const Vector3& centre)
    {
        GraphicsDevice& d = ctx.device;
        for (int i = 0; i < 3; ++i) {
            const float x = -3.4f + static_cast<float>(i) * 3.4f;
            const Vector3 base = centre + Off(x, 0, 0);
            const float a = move_ ? time_ : 0.0f;
            const Vector3 lamp = base + Off(std::cos(a) * 1.8f, height_, std::sin(a) * 1.8f);
            for (int pass = 0; pass < 2; ++pass) {
                BasicEffect& fx = Prepare(ctx, Matrix::CreateScale(pass == 0 ? Vector3(3.0f, 1.0f, 3.0f) : Vector3(1.2f)) * Matrix::CreateTranslation(base + Vector3(0, pass == 0 ? 0.03f : 0.9f, 0)),
                                          Vector3(0.85f, 0.85f, 0.9f), Vector3(0.05f, 0.05f, 0.07f));
                fx.setTextureProperty(pass == 0 ? S().textures->Get("stone") : S().textures->White());
                if (i == 0) {
                    DirectionalLight& l = fx.getDirectionalLight0Property();
                    l.setDirectionProperty(Vector3::Normalize(Vector3(-std::cos(a), -1.0f, -std::sin(a))));
                    l.setDiffuseColorProperty(Vector3(1.0f, 0.95f, 0.85f) * intensity_ * 0.6f);
                    l.setSpecularColorProperty(Vector3(0.5f, 0.5f, 0.5f));
                    l.setEnabledProperty(true);
                } else {
                    PunctualLightEXT pl;
                    pl.Kind = i == 1 ? PunctualLightKindEXT::Point : PunctualLightKindEXT::Spot;
                    pl.Position = lamp;
                    pl.Direction = Vector3::Normalize(base + Vector3(0, 0.5f, 0) - lamp);
                    pl.DiffuseColor = Vector3(1.0f, 0.9f, 0.7f) * intensity_ * 3.0f;
                    pl.Range = 7.0f;
                    pl.OuterAngle = 0.55f;
                    pl.InnerAngle = 0.3f;
                    fx.setPunctualLightEXT(pl);
                }
                EffectBank::Apply(fx);
                (pass == 0 ? S().shapes->Plane() : S().shapes->Sphere()).Draw(d);
                fx.setPunctualLightEXT(PunctualLightEXT{});
            }
            if (i > 0) gizmos_.SolidSphere(ctx, lamp, 0.1f, Vector3(1, 0.9f, 0.5f));
            gizmos_.Label(base + Vector3(0, 2.7f, 0), i == 0 ? "directional" : (i == 1 ? "point (CNA ext.)" : "spot (CNA ext.)"), Color(255, 230, 150, 255), 0.55f);
        }
    }

    void DrawEmissive(const DrawContext& ctx, const Vector3& centre)
    {
        GraphicsDevice& d = ctx.device;
        const Vector3 cols[3] = {{1.0f, 0.4f, 0.2f}, {0.3f, 1.0f, 0.5f}, {0.4f, 0.5f, 1.0f}};
        for (int i = 0; i < 6; ++i) {
            const bool glow = i < 3;
            const Vector3 p = centre + Off(-3.6f + static_cast<float>(i) * 1.45f, 0.7f, 0.0f);
            BasicEffect& fx = Prepare(ctx, Matrix::CreateScale(1.0f) * Matrix::CreateRotationY(time_ * 0.5f) * Matrix::CreateTranslation(p), cols[i % 3] * 0.6f, Vector3(0.04f, 0.04f, 0.06f));
            if (glow) fx.setEmissiveColorProperty(cols[i % 3] * emissive_);
            EffectBank::Apply(fx);
            S().shapes->Cube().Draw(d);
            if (glow && halo_) {
                d.setBlendStateProperty(BlendState::Additive);
                d.setDepthStencilStateProperty(DepthStencilState::DepthRead);
                BasicEffect& hf = Prepare(ctx, Matrix::CreateScale(1.7f + 0.15f * std::sin(time_ * 2.0f + static_cast<float>(i))) * Matrix::CreateTranslation(p), cols[i % 3] * (0.25f * emissive_), Vector3(0, 0, 0));
                hf.setEmissiveColorProperty(cols[i % 3] * (0.25f * emissive_));
                EffectBank::Apply(hf);
                S().shapes->Sphere().Draw(d);
                d.setBlendStateProperty(BlendState::Opaque);
                d.setDepthStencilStateProperty(DepthStencilState::Default);
            }
        }
        gizmos_.Label(centre + Off(-2.2f, 1.9f, 0), "EmissiveColor set", Color(255, 230, 150, 255), 0.55f);
        gizmos_.Label(centre + Off(2.2f, 1.9f, 0), "diffuse only", Color(255, 230, 150, 255), 0.55f);
    }

    Mode mode_;
    Gizmos gizmos_;
    std::unique_ptr<BasicEffect> fx_;
    Vector3 eye_{0, 1.65f, 0}, forward_{0, 0, -1};
    float time_ = 0.0f, intensity_ = 1.5f, height_ = 2.4f, range_ = 7.0f, speed_ = 0.8f, outer_ = 28.0f, emissive_ = 0.9f;
    bool move_ = true, showRange_ = true, torch_ = true, halo_ = true;
    bool on_[3] = {true, true, true};
    int count_ = 6;
};

}  // namespace

std::unique_ptr<Exhibit> MakeLightingLab(const RoomDef& room, Services& services)
{
    return std::make_unique<LightingLab>(room, services);
}
