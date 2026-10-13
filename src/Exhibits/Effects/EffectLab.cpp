// BasicEffect super-wing and the other stock effects: one class, nine rooms ("mode"). Each room owns its effect
// instance, so nothing leaks between rooms, and every property it demonstrates has a control on the console.
#include <algorithm>
#include <cmath>
#include <exception>
#include <string>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DualTextureEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameter.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/MiniView.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Combos, Lights, Material, PerPixel, Fog, Alpha, Punctual, Dual, Reflection };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"lights", Mode::Lights}, {"material", Mode::Material}, {"perpixel", Mode::PerPixel}, {"fog", Mode::Fog},
                                                         {"alpha", Mode::Alpha}, {"punctual", Mode::Punctual}, {"dual", Mode::Dual}, {"reflection", Mode::Reflection}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Combos;
}

Vector3 Hue(float h)
{
    h = h - std::floor(h);
    return Vector3(0.5f + 0.5f * std::cos(h * MathHelper::TwoPi), 0.5f + 0.5f * std::cos((h - 0.333f) * MathHelper::TwoPi), 0.5f + 0.5f * std::cos((h - 0.667f) * MathHelper::TwoPi));
}

class EffectLab final : public Exhibit {
public:
    EffectLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        BuildStagePlinth(b, 0.0f, 0.0f, 0.4f);
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Combos:
                p.AddToggle("LightingEnabled", false, [this](bool v) { lighting_ = v; });
                p.AddSlider("DiffuseColor hue", 0.0f, 1.0f, hue_, [this](float v) { hue_ = v; });
                p.AddToggle("DiffuseColor white", true, [this](bool v) { white_ = v; });
                p.AddSlider("Alpha", 0.0f, 1.0f, alpha_, [this](float v) { alpha_ = v; });
                p.AddHeading("2 x 2: TextureEnabled x VertexColorEnabled");
                break;
            case Mode::Lights:
                p.AddToggle("Light 0 enabled", true, [this](bool v) { on_[0] = v; });
                p.AddToggle("Light 1 enabled", true, [this](bool v) { on_[1] = v; });
                p.AddToggle("Light 2 enabled", true, [this](bool v) { on_[2] = v; });
                p.AddSlider("Light 0 direction (yaw)", -180.0f, 180.0f, yaw_, [this](float v) { yaw_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("AmbientLightColor", 0.0f, 0.8f, ambient_, [this](float v) { ambient_ = v; });
                p.AddToggle("Specular highlights", true, [this](bool v) { specular_ = v; });
                p.AddToggle("EnableDefaultLighting()", false, [this](bool v) { defaultRig_ = v; });
                break;
            case Mode::Material:
                p.AddSlider("DiffuseColor", 0.1f, 1.0f, diffuse_, [this](float v) { diffuse_ = v; });
                p.AddSlider("SpecularColor", 0.0f, 1.0f, specular1_, [this](float v) { specular1_ = v; });
                p.AddSlider("EmissiveColor", 0.0f, 1.0f, emissive_, [this](float v) { emissive_ = v; });
                p.AddHeading("SpecularPower 2, 8, 24, 64, 200 (left to right)");
                break;
            case Mode::PerPixel:
                p.AddSlider("Sphere tessellation", 4.0f, 32.0f, static_cast<float>(seg_), [this](float v) { seg_ = static_cast<int>(v); dirty_ = true; }, "%.0f", 1.0f);
                p.AddSlider("SpecularPower", 4.0f, 120.0f, specPower_, [this](float v) { specPower_ = v; }, "%.0f");
                p.AddSlider("Light direction", -180.0f, 180.0f, yaw_, [this](float v) { yaw_ = v; }, "%.0f deg", 1.0f);
                p.AddHeading("Left: per-vertex.  Right: PreferPerPixelLighting");
                break;
            case Mode::Fog:
                p.AddToggle("FogEnabled", true, [this](bool v) { fog_ = v; });
                p.AddSlider("FogStart", 0.0f, 20.0f, fogStart_, [this](float v) { fogStart_ = v; }, "%.1f");
                p.AddSlider("FogEnd", 5.0f, 60.0f, fogEnd_, [this](float v) { fogEnd_ = v; }, "%.1f");
                p.AddSlider("FogColor hue", 0.0f, 1.0f, hue_, [this](float v) { hue_ = v; });
                break;
            case Mode::Alpha:
                p.AddSlider("Alpha", 0.1f, 1.0f, alpha_, [this](float v) { alpha_ = v; });
                p.AddToggle("Draw back to front", true, [this](bool v) { sorted_ = v; });
                p.AddToggle("Depth writes on while blending", false, [this](bool v) { depthWrite_ = v; });
                p.AddHeading("Translucent spheres need sorting or no depth writes");
                break;
            case Mode::Punctual:
                p.AddChoice("Light kind", {"None", "Point", "Spot"}, 1, [this](int v) { kind_ = v; });
                p.AddSlider("Range", 1.0f, 12.0f, range_, [this](float v) { range_ = v; }, "%.1f");
                p.AddSlider("Spot outer angle", 10.0f, 80.0f, outer_, [this](float v) { outer_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Light height", 0.5f, 4.0f, height_, [this](float v) { height_ = v; });
                p.AddToggle("Move the light", true, [this](bool v) { move_ = v; });
                p.AddHeading("PunctualLightEXT: CNA extension");
                break;
            case Mode::Dual:
                p.AddSlider("Lightmap offset", -1.0f, 1.0f, offset_, [this](float v) { offset_ = v; });
                p.AddReadout("DualTextureEffect", [this] { return dualStatus_; });
                break;
            case Mode::Reflection:
                p.AddReadout("GetTypeName()", [this] { return fx_ ? fx_->GetTypeName() : std::string("-"); });
                p.AddReadout("Techniques", [this] { return fx_ ? std::to_string(fx_->getTechniquesProperty().getCountProperty()) : std::string("-"); });
                p.AddReadout("Current technique", [this] { return fx_ && fx_->getCurrentTechniqueProperty() ? fx_->getCurrentTechniqueProperty()->getNameProperty() : std::string("-"); });
                p.AddReadout("Passes", [this] { return fx_ && fx_->getCurrentTechniqueProperty() ? std::to_string(fx_->getCurrentTechniqueProperty()->getPassesProperty().getCountProperty()) : std::string("-"); });
                p.AddReadout("Effect parameters", [this] { return fx_ ? std::to_string(fx_->getParametersProperty().getCountProperty()) : std::string("-"); });
                p.AddReadout("IsStockEffectEXT", [this] { return std::string(fx_ && fx_->IsStockEffectEXT() ? "true (CNA extension)" : "false"); });
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        GraphicsDevice& d = *S().device;
        fx_ = std::make_unique<BasicEffect>(d);
        if (mode_ == Mode::Fog) view_ = std::make_unique<MiniView>(S(), 640, 360);
        if (mode_ == Mode::Combos) BuildColouredCube();
        if (mode_ == Mode::Dual) {
            try {
                dual_ = std::make_unique<DualTextureEffect>(d);
                dualStatus_ = "created";
                BuildLightmap();
            } catch (const std::exception& e) {
                dualStatus_ = std::string("unavailable: ") + std::string(e.what()).substr(0, 30);
            }
        }
        dirty_ = true;
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        spin_ += ctx.dt * 0.6f;
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (mode_ != Mode::Fog || !view_) return;
        const Vector3 eye(0, 1.0f, 3.0f);
        view_->Render(ctx, Matrix::CreateLookAt(eye, Vector3(0, 0.9f, -20.0f), Vector3::Up),
                      Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(65.0f), view_->Aspect(), 0.1f, 100.0f), eye, [this](const DrawContext& sub) { DrawCorridor(sub); },
                      fog_ ? Color(static_cast<std::uint8_t>(Hue(hue_).X * 255), static_cast<std::uint8_t>(Hue(hue_).Y * 255), static_cast<std::uint8_t>(Hue(hue_).Z * 255), static_cast<std::uint8_t>(255)) : Color(30, 36, 50, 255));
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!fx_) return;
        const Vector3 o = R().ToWorld(0.0f, 1.55f, 0.0f);
        switch (mode_) {
            case Mode::Combos: DrawCombos(ctx, o); break;
            case Mode::Lights: DrawLights(ctx, o); break;
            case Mode::Material: DrawMaterial(ctx, o); break;
            case Mode::PerPixel: DrawPerPixel(ctx, o); break;
            case Mode::Fog: if (view_) view_->DrawInRoom(ctx, FarPoint0(2.3f), FarYaw(), 6.6f); break;
            case Mode::Alpha: DrawAlpha(ctx, o); break;
            case Mode::Punctual: DrawPunctual(ctx); break;
            case Mode::Dual: DrawDual(ctx, o); break;
            case Mode::Reflection: DrawReflection(ctx, o); break;
        }
        RenderStates::SetOpaque3D(ctx.device);
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Texture, vertex colour and diffuse", "Lights and ambient", "Material: diffuse, specular, emissive", "Per-vertex and per-pixel lighting", "Fog", "Alpha and sorting", "Point and spot lights (CNA extension)", "DualTextureEffect", "The Effect object model"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Combos: return {"BasicEffect multiplies up to three colours: the texture, the vertex colours and DiffuseColor. The four cubes", "have the same geometry (each face a different vertex colour) and differ only in TextureEnabled and", "VertexColorEnabled. Turn lighting on and DiffuseColor away from white to see the products."};
            case Mode::Lights: return {"BasicEffect has an ambient colour and up to three directional lights, each with a direction, a diffuse", "and a specular colour. EnableDefaultLighting sets XNA's standard three-light rig. Disable lights one by one."};
            case Mode::Material: return {"A material is DiffuseColor (the base colour), SpecularColor and SpecularPower (the highlight: a larger power", "gives a smaller, sharper spot) and EmissiveColor (light the surface gives off by itself)."};
            case Mode::PerPixel: return {"Per-vertex lighting computes light at the corners and interpolates: a coarse sphere shows facets and loses", "its highlight. PreferPerPixelLighting evaluates the light per pixel, so even eight segments look round."};
            case Mode::Fog: return {"Fog fades colours towards FogColor between FogStart and FogEnd, hiding the far clip plane and adding depth.", "The view is a corridor of columns in a render target; the clear colour matches the fog."};
            case Mode::Alpha: return {"BasicEffect.Alpha makes a surface translucent only if the BlendState blends. Overlapping translucent", "objects must be drawn back to front, or without depth writes, or the nearer one hides the farther."};
            case Mode::Punctual: return {"CNA extension: PunctualLightEXT adds one point or spot light with a range and cone angles to the stock", "BasicEffect (XNA has only directional lights). Move it, narrow the cone or shorten the range."};
            case Mode::Dual: return {"DualTextureEffect multiplies two textures per pixel, the classic way to apply a baked lightmap over a", "tiling surface texture. The lightmap here is a soft radial gradient; slide it across the bricks."};
            default: return {"Every Effect, even a stock one, is described by techniques, passes and parameters. Drawing means setting", "the effect's properties, then Apply()ing each pass of the current technique. CNA reports this for BasicEffect."};
        }
    }

    // -- helpers ---------------------------------------------------------------------------------
    void Camera(BasicEffect& fx, const DrawContext& ctx, const Matrix& world)
    {
        fx.setWorldProperty(world);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
    }
    void ResetFx(BasicEffect& fx)
    {
        fx.setLightingEnabledProperty(false);
        fx.setTextureEnabledProperty(false);
        fx.setVertexColorEnabledProperty(false);
        fx.setPreferPerPixelLightingProperty(false);
        fx.setFogEnabledProperty(false);
        fx.setAlphaProperty(1.0f);
        fx.setDiffuseColorProperty(Vector3(1, 1, 1));
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setSpecularColorProperty(Vector3(0, 0, 0));
        fx.setAmbientLightColorProperty(Vector3(0, 0, 0));
        fx.setPunctualLightEXT(PunctualLightEXT{});
        for (DirectionalLight* l : {&fx.getDirectionalLight0Property(), &fx.getDirectionalLight1Property(), &fx.getDirectionalLight2Property()}) l->setEnabledProperty(false);
    }

    void BuildColouredCube()
    {
        std::vector<VertexPositionColorTexture> v;
        const Vector3 n[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        const Color cols[6] = {Color(240, 80, 70, 255), Color(80, 240, 110, 255), Color(80, 130, 255, 255), Color(250, 220, 80, 255), Color(220, 90, 240, 255), Color(80, 230, 230, 255)};
        for (int f = 0; f < 6; ++f) {
            const Vector3 up = std::abs(n[f].Y) > 0.5f ? Vector3(0, 0, -1) : Vector3(0, 1, 0);
            const Vector3 right = Vector3::Normalize(Vector3::Cross(up, n[f])), u = Vector3::Cross(n[f], right);
            const Vector3 p0 = n[f] * 0.5f - right * 0.5f - u * 0.5f, p1 = n[f] * 0.5f + right * 0.5f - u * 0.5f, p2 = n[f] * 0.5f + right * 0.5f + u * 0.5f, p3 = n[f] * 0.5f - right * 0.5f + u * 0.5f;
            v.emplace_back(p0, cols[f], Vector2(0, 1));
            v.emplace_back(p1, cols[f], Vector2(1, 1));
            v.emplace_back(p2, cols[f], Vector2(1, 0));
            v.emplace_back(p0, cols[f], Vector2(0, 1));
            v.emplace_back(p2, cols[f], Vector2(1, 0));
            v.emplace_back(p3, cols[f], Vector2(0, 0));
        }
        coloured_ = std::move(v);
    }
    void DrawCombos(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& d = ctx.device;
        for (int i = 0; i < 4; ++i) {
            ResetFx(*fx_);
            const bool tex = (i & 1) != 0, vc = (i & 2) != 0;
            fx_->setTextureEnabledProperty(tex);
            if (tex) fx_->setTextureProperty(S().textures->Get("brick"));
            fx_->setVertexColorEnabledProperty(vc);
            fx_->setDiffuseColorProperty(white_ ? Vector3(1, 1, 1) : Hue(hue_));
            fx_->setAlphaProperty(alpha_);
            if (lighting_) fx_->EnableDefaultLighting();
            fx_->setLightingEnabledProperty(lighting_);
            Camera(*fx_, ctx, Matrix::CreateRotationY(spin_) * Matrix::CreateRotationX(0.45f) * Matrix::CreateTranslation(o + Off(-1.4f + static_cast<float>(i % 2) * 2.8f, 0.9f - static_cast<float>(i / 2) * 1.9f, 0)));
            d.setBlendStateProperty(alpha_ < 0.99f ? BlendState::NonPremultiplied : BlendState::Opaque);
            d.setRasterizerStateProperty(RasterizerState::CullNone);
            EffectBank::Apply(*fx_);
            d.DrawUserPrimitives(PrimitiveType::TriangleList, coloured_.data(), 0, 12);
            gizmos_.Label(o + Off(-1.4f + static_cast<float>(i % 2) * 2.8f, 1.5f - static_cast<float>(i / 2) * 1.9f, 0), std::string(tex ? "texture" : "no texture") + ", " + (vc ? "vertex colour" : "no vertex colour"), Color(255, 230, 150, 255), 0.5f);
        }
        d.setBlendStateProperty(BlendState::Opaque);
    }

    void DrawSphere(const DrawContext& ctx, const Matrix& world, const LitMesh& mesh) 
    {
        Camera(*fx_, ctx, world);
        EffectBank::Apply(*fx_);
        mesh.Draw(ctx.device);
    }
    void SetLight0(float yawDeg, const Vector3& diffuse, const Vector3& spec)
    {
        DirectionalLight& l = fx_->getDirectionalLight0Property();
        const float y = MathHelper::ToRadians(yawDeg);
        l.setDirectionProperty(Vector3::Normalize(Vector3(std::cos(y), -0.7f, std::sin(y))));
        l.setDiffuseColorProperty(diffuse);
        l.setSpecularColorProperty(spec);
        l.setEnabledProperty(true);
    }
    void DrawLights(const DrawContext& ctx, const Vector3& o)
    {
        ResetFx(*fx_);
        fx_->setLightingEnabledProperty(true);
        fx_->setTextureEnabledProperty(true);
        fx_->setTextureProperty(S().textures->White());
        fx_->setDiffuseColorProperty(Vector3(0.85f, 0.85f, 0.85f));
        fx_->setSpecularPowerProperty(32.0f);
        if (defaultRig_) fx_->EnableDefaultLighting();
        else {
            fx_->setAmbientLightColorProperty(Vector3(ambient_, ambient_, ambient_ * 1.2f));
            const Vector3 sp = specular_ ? Vector3(1, 1, 1) : Vector3(0, 0, 0);
            const bool on[3] = {on_[0], on_[1], on_[2]};
            SetLight0(yaw_, Vector3(1.0f, 0.55f, 0.5f), sp);
            DirectionalLight& l1 = fx_->getDirectionalLight1Property();
            l1.setDirectionProperty(Vector3::Normalize(Vector3(-0.6f, -0.4f, 0.7f)));
            l1.setDiffuseColorProperty(Vector3(0.4f, 0.8f, 0.5f));
            l1.setSpecularColorProperty(sp);
            DirectionalLight& l2 = fx_->getDirectionalLight2Property();
            l2.setDirectionProperty(Vector3::Normalize(Vector3(0.1f, -0.2f, -1.0f)));
            l2.setDiffuseColorProperty(Vector3(0.5f, 0.6f, 1.0f));
            l2.setSpecularColorProperty(sp);
            fx_->getDirectionalLight0Property().setEnabledProperty(on[0]);
            l1.setEnabledProperty(on[1]);
            l2.setEnabledProperty(on[2]);
        }
        fx_->setSpecularColorProperty(specular_ ? Vector3(1, 1, 1) : Vector3(0, 0, 0));
        ctx.device.setRasterizerStateProperty(RasterizerState::CullClockwise);
        DrawSphere(ctx, Matrix::CreateScale(2.2f) * Matrix::CreateRotationY(spin_) * Matrix::CreateTranslation(o), S().shapes->Sphere());
        const float y = MathHelper::ToRadians(yaw_);
        gizmos_.Arrow(o - Off(std::cos(y), -0.7f, std::sin(y)) * 1.8f, o - Off(std::cos(y), -0.7f, std::sin(y)) * 1.2f, Color(255, 150, 130, 255), 0.15f);
        gizmos_.Label(o - Off(std::cos(y), -0.7f, std::sin(y)) * 1.9f, "light 0", Color(255, 170, 150, 255), 0.5f);
    }

    void DrawMaterial(const DrawContext& ctx, const Vector3& o)
    {
        const float powers[5] = {2.0f, 8.0f, 24.0f, 64.0f, 200.0f};
        for (int i = 0; i < 5; ++i) {
            ResetFx(*fx_);
            fx_->EnableDefaultLighting();
            fx_->setTextureEnabledProperty(true);
            fx_->setTextureProperty(S().textures->White());
            fx_->setDiffuseColorProperty(Vector3(diffuse_ * 0.9f, diffuse_ * 0.5f, diffuse_ * 0.3f));
            fx_->setSpecularColorProperty(Vector3(specular1_, specular1_, specular1_));
            fx_->setSpecularPowerProperty(powers[i]);
            fx_->setEmissiveColorProperty(Vector3(emissive_ * 0.4f, emissive_ * 0.3f, emissive_ * 0.1f));
            ctx.device.setRasterizerStateProperty(RasterizerState::CullClockwise);
            const float x = -3.2f + static_cast<float>(i) * 1.6f;
            DrawSphere(ctx, Matrix::CreateScale(1.35f) * Matrix::CreateTranslation(o + Off(x, 0, 0)), S().shapes->Sphere());
            gizmos_.Label(o + Off(x, 0.95f, 0), "power " + std::to_string(static_cast<int>(powers[i])), Color(255, 230, 150, 255), 0.5f);
        }
    }

    void DrawPerPixel(const DrawContext& ctx, const Vector3& o)
    {
        if (dirty_ || !coarse_) {
            MeshBuilder mb;
            mb.AddSphere(Vector3::Zero, 0.5f, seg_, std::max(3, seg_ / 2), Vector3(1, 1, 1));
            coarse_ = std::make_unique<LitMesh>(*S().device, mb);
            dirty_ = false;
        }
        for (int i = 0; i < 2; ++i) {
            ResetFx(*fx_);
            fx_->setLightingEnabledProperty(true);
            fx_->setTextureEnabledProperty(true);
            fx_->setTextureProperty(S().textures->White());
            fx_->setDiffuseColorProperty(Vector3(0.5f, 0.7f, 0.95f));
            fx_->setSpecularColorProperty(Vector3(1, 1, 1));
            fx_->setSpecularPowerProperty(specPower_);
            fx_->setAmbientLightColorProperty(Vector3(0.15f, 0.15f, 0.18f));
            SetLight0(yaw_ + 90.0f, Vector3(1, 1, 1), Vector3(1, 1, 1));
            fx_->setPreferPerPixelLightingProperty(i == 1);
            ctx.device.setRasterizerStateProperty(RasterizerState::CullClockwise);
            const float x = -1.6f + static_cast<float>(i) * 3.2f;
            DrawSphere(ctx, Matrix::CreateScale(2.4f) * Matrix::CreateTranslation(o + Off(x, 0, 0)), *coarse_);
            gizmos_.Label(o + Off(x, 1.4f, 0), i == 0 ? "per-vertex" : "PreferPerPixelLighting", Color(255, 230, 150, 255), 0.55f);
        }
    }

    void DrawCorridor(const DrawContext& ctx)
    {
        ResetFx(*fx_);
        fx_->setLightingEnabledProperty(true);
        fx_->EnableDefaultLighting();
        fx_->setTextureEnabledProperty(true);
        fx_->setTextureProperty(S().textures->Get("stone"));
        fx_->setFogEnabledProperty(fog_);
        const Vector3 fc = Hue(hue_);
        fx_->setFogColorProperty(fc);
        fx_->setFogStartProperty(fogStart_);
        fx_->setFogEndProperty(std::max(fogEnd_, fogStart_ + 1.0f));
        ctx.device.setRasterizerStateProperty(RasterizerState::CullClockwise);
        // Ground.
        fx_->setDiffuseColorProperty(Vector3(0.8f, 0.8f, 0.8f));
        Camera(*fx_, ctx, Matrix::CreateScale(10.0f, 1.0f, 60.0f) * Matrix::CreateTranslation(0, 0, -25.0f));
        EffectBank::Apply(*fx_);
        S().shapes->Plane().Draw(ctx.device);
        for (int i = 0; i < 16; ++i) {
            for (const float side : {-2.5f, 2.5f}) {
                fx_->setDiffuseColorProperty(Vector3(0.9f, 0.75f, 0.6f));
                Camera(*fx_, ctx, Matrix::CreateScale(0.8f, 2.4f, 0.8f) * Matrix::CreateTranslation(side, 0.0f, -static_cast<float>(i) * 3.0f));
                EffectBank::Apply(*fx_);
                S().shapes->Cylinder().Draw(ctx.device);
            }
        }
    }

    void DrawAlpha(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& d = ctx.device;
        // Three overlapping spheres; the "sorted" order is farthest first from the visitor.
        struct S3 { Vector3 pos; Vector3 colour; };
        S3 items[3] = {{o + Off(-0.9f, 0, 0.6f), {0.95f, 0.3f, 0.25f}}, {o + Vector3(0, 0, 0), {0.3f, 0.9f, 0.35f}}, {o + Off(0.9f, 0, -0.6f), {0.3f, 0.5f, 1.0f}}};
        if (sorted_ && S().mainView != nullptr)
            std::sort(items, items + 3, [&](const S3& a, const S3& b) { return Vector3::DistanceSquared(a.pos, S().mainView->eye) > Vector3::DistanceSquared(b.pos, S().mainView->eye); });
        d.setBlendStateProperty(BlendState::NonPremultiplied);
        d.setDepthStencilStateProperty(depthWrite_ ? DepthStencilState::Default : DepthStencilState::DepthRead);
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        for (const S3& s : items) {
            ResetFx(*fx_);
            fx_->EnableDefaultLighting();
            fx_->setTextureEnabledProperty(true);
            fx_->setTextureProperty(S().textures->White());
            fx_->setDiffuseColorProperty(s.colour);
            fx_->setAlphaProperty(alpha_);
            DrawSphere(ctx, Matrix::CreateScale(1.7f) * Matrix::CreateTranslation(s.pos), S().shapes->Sphere());
        }
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::Default);
    }

    void DrawPunctual(const DrawContext& ctx)
    {
        GraphicsDevice& d = ctx.device;
        const Vector3 centre = R().ToWorld(0.0f, 0.0f, 0.0f);
        const float t = move_ ? time_ : 0.0f;
        const Vector3 lamp = centre + Off(std::cos(t * 0.8f) * 3.0f, height_, std::sin(t * 0.8f) * 3.0f);
        ResetFx(*fx_);
        fx_->setLightingEnabledProperty(true);
        fx_->setTextureEnabledProperty(true);
        fx_->setTextureProperty(S().textures->Get("stone"));
        fx_->setAmbientLightColorProperty(Vector3(0.08f, 0.08f, 0.10f));
        fx_->setDiffuseColorProperty(Vector3(0.9f, 0.9f, 0.9f));
        fx_->setPreferPerPixelLightingProperty(true);   // point and spot light falloff is evaluated per pixel
        SetLight0(30.0f, Vector3(0.05f, 0.05f, 0.07f), Vector3(0, 0, 0));   // a faint directional light keeps the lit path selected
        PunctualLightEXT light;
        if (kind_ == 1) {
            light.Kind = PunctualLightKindEXT::Point;
        } else if (kind_ == 2) {
            light.Kind = PunctualLightKindEXT::Spot;
            light.Direction = Vector3::Normalize(centre + Vector3(0, 0.2f, 0) - lamp);
            light.OuterAngle = MathHelper::ToRadians(outer_);
            light.InnerAngle = light.OuterAngle * 0.6f;
        }
        light.Position = lamp;
        light.Range = range_;
        light.DiffuseColor = Vector3(1.0f, 0.85f, 0.6f) * 6.0f;
        fx_->setPunctualLightEXT(light);
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        Camera(*fx_, ctx, Matrix::CreateScale(9.0f, 1.0f, 9.0f) * Matrix::CreateTranslation(centre + Vector3(0, 0.03f, 0)));
        EffectBank::Apply(*fx_);
        S().shapes->Plane().Draw(d);
        for (int i = 0; i < 5; ++i) {
            const float a = static_cast<float>(i) / 5.0f * MathHelper::TwoPi;
            Camera(*fx_, ctx, Matrix::CreateScale(0.7f, 0.9f + 0.2f * static_cast<float>(i % 3), 0.7f) * Matrix::CreateTranslation(centre + Off(std::cos(a) * 2.4f, 0.03f, std::sin(a) * 2.4f)));
            EffectBank::Apply(*fx_);
            (i % 2 ? S().shapes->Cylinder() : S().shapes->Cone()).Draw(d);
        }
        fx_->setPunctualLightEXT(PunctualLightEXT{});
        gizmos_.SolidSphere(ctx, lamp, 0.12f, Vector3(1.0f, 0.9f, 0.5f));
        if (kind_ == 2) gizmos_.Line(lamp, centre, Color(255, 220, 120, 255));
        gizmos_.Label(lamp + Vector3(0, 0.3f, 0), kind_ == 1 ? "point light" : (kind_ == 2 ? "spot light" : "no light"), Color(255, 235, 170, 255), 0.5f);
    }

    void BuildLightmap()
    {
        const int n = 64;
        std::vector<Color> px(static_cast<std::size_t>(n * n));
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                const float dx = static_cast<float>(x - n / 2) / (n / 2), dy = static_cast<float>(y - n / 2) / (n / 2);
                const float v = std::max(0.0f, 1.0f - std::sqrt(dx * dx + dy * dy));
                const std::uint8_t g = static_cast<std::uint8_t>(40 + 215 * v * v);
                px[static_cast<std::size_t>(y * n + x)] = Color(g, g, static_cast<std::uint8_t>(g * 0.85f), static_cast<std::uint8_t>(255));
            }
        lightmap_ = std::make_unique<Texture2D>(*S().device, n, n);
        lightmap_->SetData(px.data(), n * n);
    }
    void DrawDual(const DrawContext& ctx, const Vector3& o)
    {
        if (!dual_ || !lightmap_) return;
        GraphicsDevice& d = ctx.device;
        dual_->setTextureProperty(S().textures->Get("brick"));
        dual_->setTexture2Property(lightmap_.get());
        dual_->setViewProperty(ctx.view.view);
        dual_->setProjectionProperty(ctx.view.projection);
        dual_->setWorldProperty(Matrix::CreateTranslation(o));
        dual_->setVertexColorEnabledProperty(false);
        const float u0 = offset_, u1 = 1.0f + offset_;
        // DualTextureEffect reads a second texture-coordinate set; with one set the vertices supply the same UVs to both.
        // The stock dual-texture layout: position, normal and two independent texture-coordinate sets (40 bytes). No vertex colour.
        struct DualVertex {
            Vector3 position, normal;
            Vector2 uv0, uv1;
        };
        static const VertexDeclaration decl(40, {VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0), VertexElement(12, VertexElementFormat::Vector3, VertexElementUsage::Normal, 0),
                                                 VertexElement(24, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0), VertexElement(32, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 1)});
        const Vector3 nz(0, 0, 1);
        // Texture 0 tiles the bricks; texture 1 (the lightmap) covers the quad once, shifted by the slider.
        const float t = 3.0f;
        const DualVertex q[6] = {{Vector3(-1.9f, -1.9f, 0), nz, Vector2(0, t), Vector2(u0, u1)}, {Vector3(1.9f, -1.9f, 0), nz, Vector2(t, t), Vector2(u1, u1)}, {Vector3(1.9f, 1.9f, 0), nz, Vector2(t, 0), Vector2(u1, u0)},
                                 {Vector3(-1.9f, -1.9f, 0), nz, Vector2(0, t), Vector2(u0, u1)}, {Vector3(1.9f, 1.9f, 0), nz, Vector2(t, 0), Vector2(u1, u0)}, {Vector3(-1.9f, 1.9f, 0), nz, Vector2(0, 0), Vector2(u0, u0)}};
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        d.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
        d.getSamplerStatesProperty()[1] = SamplerState::LinearClamp;
        try {
            for (auto& pass : dual_->getCurrentTechniqueProperty()->getPassesProperty()) pass.Apply();
            d.DrawUserPrimitives(PrimitiveType::TriangleList, q, 0, 2, decl);
            dualStatus_ = "drawing, two UV sets";
        } catch (const std::exception& e) {
            dualStatus_ = std::string("draw failed: ") + std::string(e.what()).substr(0, 24);
        }
    }

    void DrawReflection(const DrawContext& ctx, const Vector3& o)
    {
        ResetFx(*fx_);
        fx_->EnableDefaultLighting();
        fx_->setTextureEnabledProperty(true);
        fx_->setTextureProperty(S().textures->Get("marble"));
        ctx.device.setRasterizerStateProperty(RasterizerState::CullClockwise);
        DrawSphere(ctx, Matrix::CreateScale(2.0f) * Matrix::CreateRotationY(spin_) * Matrix::CreateTranslation(o), S().shapes->Sphere());
        // The parameters the effect reports, as labels.
        EffectParameterCollection& params = fx_->getParametersProperty();
        for (int i = 0; i < std::min(params.getCountProperty(), 8); ++i)
            gizmos_.Label(o + Off(2.0f, 1.2f - static_cast<float>(i) * 0.3f, 0), params[i]->getNameProperty(), Color(180, 230, 255, 255), 0.45f);
    }

    Mode mode_;
    Gizmos gizmos_;
    std::unique_ptr<BasicEffect> fx_;
    std::unique_ptr<DualTextureEffect> dual_;
    std::unique_ptr<MiniView> view_;
    std::unique_ptr<LitMesh> coarse_;
    std::unique_ptr<Texture2D> lightmap_;
    std::vector<VertexPositionColorTexture> coloured_;
    std::string dualStatus_ = "-";
    float time_ = 0.0f, spin_ = 0.0f;
    bool lighting_ = false, white_ = true, defaultRig_ = false, specular_ = true, fog_ = true, sorted_ = true, depthWrite_ = false, move_ = true, dirty_ = true;
    bool on_[3] = {true, true, true};
    float hue_ = 0.6f, alpha_ = 1.0f, yaw_ = 40.0f, ambient_ = 0.15f, diffuse_ = 0.9f, specular1_ = 1.0f, emissive_ = 0.0f, specPower_ = 40.0f, fogStart_ = 4.0f, fogEnd_ = 30.0f;
    float range_ = 7.0f, outer_ = 35.0f, height_ = 2.6f, offset_ = 0.0f;
    int seg_ = 10, kind_ = 1;
};

}  // namespace

std::unique_ptr<Exhibit> MakeEffectLab(const RoomDef& room, Services& services)
{
    return std::make_unique<EffectLab>(room, services);
}
