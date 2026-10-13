// Shadow mapping with CNA's shadow-receiver extension. A caster pass draws the objects from the light into a
// float render target through a small ShaderEffect (one vertex stage, one fragment stage that writes the light-space
// depth); BasicEffect then receives the map (setShadowMapEXT, setLightViewProjectionEXT) and darkens what the casters
// hide, with a bias and a percentage-closer filter of 0, 1 or 2 texels.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "CNA/GraphicsCapability.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const char* kCasterVertex = R"(#version 300 es
layout(location = 0) in vec3 aPos;
uniform mat4 uMvp;
out float vDepth;
void main() {
    vec4 p = uMvp * vec4(aPos, 1.0);
    gl_Position = p;
    vDepth = p.z / p.w * 0.5 + 0.5;      // the value the receiver compares: light-space depth in 0..1
}
)";

const char* kCasterFragment = R"(#version 300 es
precision highp float;
in float vDepth;
out vec4 FragColor;
void main() { FragColor = vec4(vDepth, vDepth, vDepth, 1.0); }
)";

class ShadowLab final : public ScreenExhibit {
public:
    ShadowLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 5.2f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Shadow mapping", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddToggle("Shadows (setShadowsEnabledEXT)", true, [this](bool v) { on_ = v; });
        p.AddSlider("Light direction (degrees)", 0.0f, 360.0f, angle_, [this](float v) { angle_ = v; animate_ = false; }, "%.0f", 1.0f);
        p.AddToggle("Move the light", true, [this](bool v) { animate_ = v; });
        p.AddSlider("Shadow bias", 0.0f, 0.03f, bias_, [this](float v) { bias_ = v; }, "%.4f");
        p.AddChoice("Filter radius (PCF)", {"0: one tap", "1: 3x3", "2: 5x5"}, 1, [this](int v) { radius_ = v; });
        p.AddChoice("Shadow map size", {"256", "512", "1024"}, 1, [this](int v) { sizeIndex_ = v; mapDirty_ = true; });
        p.AddReadout("ShadowSampling support", [this] { return std::string(supported_ ? "yes" : "NO on this renderer"); });
        p.AddReadout("Map format", [this] { return format_; });
        AddInfoSign(b, "Shadow mapping", {"The shadow map holds light-space depth from a caster pass (a ShaderEffect). BasicEffect, which implements", "IShadowReceiverEXT, compares each surface's depth with the map: hidden means shadowed. PCF softens the edge."});
    }

    void OnScreenLoaded() override
    {
        GraphicsDevice& d = *S().device;
        supported_ = d.SupportsShadowSamplingEXT();
        try {
            caster_ = std::make_unique<ShaderEffect>(d, kCasterVertex, kCasterFragment);
            if (!caster_->IsEffectValid()) {
                error_ = caster_->GetCompileErrorEXT();
                Museum::Log("shadow", "caster shader: %s", error_.c_str());
            }
        } catch (const std::exception& e) {
            error_ = e.what();
            Museum::Log("shadow", "caster shader: %s", e.what());
        }
        lit_ = std::make_unique<BasicEffect>(d);
        lit_->EnableDefaultLighting();
        lit_->setTextureEnabledProperty(true);
        lit_->setTextureProperty(S().textures->White());
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        if (animate_) angle_ = std::fmod(angle_ + ctx.dt * 12.0f, 360.0f);
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (!ctx.offscreen && supported_ && caster_ && caster_->IsEffectValid()) RenderShadowMap(ctx.device);
        ScreenExhibit::RenderOffscreen(ctx);
    }

    void DrawExtra(const DrawContext& ctx) override
    {
        if (!lit_) return;
        GraphicsDevice& d = ctx.device;
        RenderStates::SetOpaque3D(d);
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        lit_->setViewProperty(ctx.view.view);
        lit_->setProjectionProperty(ctx.view.projection);
        const Vector3 toLight = LightDirection();
        DirectionalLight& l0 = lit_->getDirectionalLight0Property();
        l0.setEnabledProperty(true);
        l0.setDirectionProperty(-toLight);
        l0.setDiffuseColorProperty(Vector3(1.0f, 0.97f, 0.9f));
        lit_->getDirectionalLight1Property().setEnabledProperty(false);
        lit_->getDirectionalLight2Property().setEnabledProperty(false);
        lit_->setAmbientLightColorProperty(Vector3(0.25f, 0.27f, 0.32f));
        lit_->setSpecularColorProperty(Vector3(0, 0, 0));
        const bool shadows = supported_ && on_ && map_ != nullptr && mapReady_;
        lit_->setShadowMapEXT(shadows ? map_.get() : nullptr);
        lit_->setShadowsEnabledEXT(shadows);
        lit_->setLightViewProjectionEXT(LightViewProjection());
        lit_->setShadowDepthBiasEXT(bias_);
        lit_->setShadowFilterRadiusEXT(radius_);
        for (const Item& item : Items()) {
            lit_->setDiffuseColorProperty(item.colour);
            lit_->setWorldProperty(item.world);
            EffectBank::Apply(*lit_);
            item.mesh->Draw(d);
        }
        lit_->setShadowMapEXT(nullptr);
        lit_->setShadowsEnabledEXT(false);
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
        sb.Draw(*S().whiteTexture, Rectangle(0, 0, 960, 540), Color(14, 18, 30, 255));
        if (map_ && mapReady_) sb.Draw(*map_, Rectangle(300, 40, 460, 460), Color::White);
        sb.End();
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        sb.DrawString(*S().font, "The shadow map (light-space depth, red channel)", Vector2(24, 8), Kit::kGold, 0.0f, Vector2(0, 0), 0.8f, SpriteEffects::None, 0.0f);
        if (!supported_) sb.DrawString(*S().font, "This renderer does not support shadow sampling: no shadows are drawn.", Vector2(24, 500), Kit::Rgba(255, 190, 150), 0.0f, Vector2(0, 0), 0.7f, SpriteEffects::None, 0.0f);
        else if (!error_.empty()) sb.DrawString(*S().font, "caster shader: " + error_.substr(0, 70), Vector2(24, 500), Kit::Rgba(255, 190, 150), 0.0f, Vector2(0, 0), 0.6f, SpriteEffects::None, 0.0f);
        sb.End();
    }

private:
    struct Item {
        const LitMesh* mesh;
        Matrix world;
        Vector3 colour;
    };
    [[nodiscard]] std::vector<Item> Items() const
    {
        const Vector3 c = R().centre;
        const float spin = time_ * 0.5f;
        return {
            {&S().shapes->Plane(), Matrix::CreateScale(14.0f, 1.0f, 14.0f) * Matrix::CreateTranslation(c + Vector3(0, 0.02f, 0)), Vector3(0.7f, 0.75f, 0.8f)},
            {&S().shapes->Cube(), Matrix::CreateScale(1.0f) * Matrix::CreateRotationY(spin) * Matrix::CreateTranslation(c + Vector3(-1.8f, 0.5f, -0.6f)), Vector3(0.9f, 0.4f, 0.3f)},
            {&S().shapes->Sphere(), Matrix::CreateScale(1.1f) * Matrix::CreateTranslation(c + Vector3(0.1f, 0.9f + 0.25f * std::sin(time_), 0.9f)), Vector3(0.35f, 0.6f, 0.95f)},
            {&S().shapes->Cone(), Matrix::CreateScale(1.0f, 1.8f, 1.0f) * Matrix::CreateTranslation(c + Vector3(1.9f, 0.0f, -0.9f)), Vector3(0.4f, 0.85f, 0.45f)},
        };
    }
    [[nodiscard]] Vector3 LightDirection() const   // unit vector from the scene towards the light
    {
        const float a = MathHelper::ToRadians(angle_);
        return Vector3::Normalize(Vector3(std::cos(a) * 0.6f, 0.8f, std::sin(a) * 0.6f));
    }
    [[nodiscard]] Matrix LightViewProjection() const
    {
        const Vector3 target = R().centre + Vector3(0, 0.5f, 0);
        const Vector3 eye = target + LightDirection() * 14.0f;
        return Matrix::CreateLookAt(eye, target, Vector3::Up) * Matrix::CreateOrthographic(15.0f, 15.0f, 1.0f, 30.0f);
    }

    void RenderShadowMap(GraphicsDevice& d)
    {
        static const int sizes[] = {256, 512, 1024};
        if (mapDirty_ || !map_) {
            mapReady_ = false;
            RenderStates::UnbindTextures(d);
            const bool floatTarget = d.SupportsCapability(CNA::GraphicsCapability::FloatRenderTargets);
            try {
                map_ = std::make_unique<RenderTarget2D>(d, sizes[sizeIndex_], sizes[sizeIndex_], false, floatTarget ? SurfaceFormat::Single : SurfaceFormat::Color, DepthFormat::Depth24);
                format_ = floatTarget ? "Single (32-bit float)" : "Color (8 bits: use a larger bias)";
            } catch (const std::exception& e) {
                Museum::Log("shadow", "shadow map target: %s", e.what());
                map_.reset();
                return;
            }
            mapDirty_ = false;
        }
        RenderStates::UnbindTextures(d);
        d.SetRenderTarget(map_.get());
        d.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color(255, 255, 255, 255), 1.0f, 0);
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::Default);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        const Matrix lightVP = LightViewProjection();
        for (const Item& item : Items()) {
            if (item.mesh == &S().shapes->Plane()) continue;   // the floor only receives
            const Matrix mvp = item.world * lightVP;
            caster_->Apply();
            caster_->SetUniformMat4("uMvp", &mvp.M11);
            item.mesh->Draw(d);
        }
        d.SetRenderTarget(nullptr);
        mapReady_ = true;
    }

    std::unique_ptr<ShaderEffect> caster_;
    std::unique_ptr<BasicEffect> lit_;
    std::unique_ptr<RenderTarget2D> map_;
    std::string error_, format_ = "-";
    float time_ = 0.0f, angle_ = 40.0f, bias_ = 0.006f;
    int radius_ = 1, sizeIndex_ = 1;
    bool on_ = true, animate_ = true, supported_ = true, mapDirty_ = true, mapReady_ = false;
};

}  // namespace

std::unique_ptr<Exhibit> MakeShadowLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ShadowLab>(room, services);
}
