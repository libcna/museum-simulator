#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <string>

#include "CNA/GraphicsCapability.hpp"
#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ModelUtil.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/CullMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/FillMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LiveScreen.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/ModelLibrary.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SignFactory.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "Rendering/WireMesh.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const Color kGold(255, 226, 150, 255);
const Color kText(222, 228, 240, 255);
const Color kGreen(150, 255, 170, 255);
const Color kRed(255, 150, 140, 255);

constexpr float kPedestal = 0.9f;
constexpr float kPedestalH = 0.9f;
constexpr float kObjectLift = 0.72f;   // centre of the exhibited object above the pedestal top

enum StationId { Depth = 0, Culling = 1, Fill = 2, Blend = 3 };

struct StationPlace {
    float x, z;
    const char* title;
};
const StationPlace kStations[4] = {
    {4.4f, 4.2f, "DepthStencilState"},
    {4.4f, -4.2f, "RasterizerState.CullMode"},
    {-4.4f, 4.2f, "RasterizerState.FillMode"},
    {-4.4f, -4.2f, "BlendState"},
};

struct BlendChoice {
    const char* name;
    const char* detail;
};
const BlendChoice kBlends[4] = {
    {"BlendState.Opaque", "alpha ignored: the panes are solid"},
    {"BlendState.AlphaBlend", "premultiplied alpha: colour was multiplied by alpha already"},
    {"BlendState.NonPremultiplied", "straight alpha: the blender multiplies for us"},
    {"BlendState.Additive", "colours add up: lights, fire, glow"},
};
struct OrderChoice {
    const char* name;
    const char* detail;
};
const OrderChoice kOrders[3] = {
    {"sorted, depth writes off", "correct: far panes first, nothing hides what is behind it"},
    {"unsorted, depth writes ON", "panes drawn early block panes behind them"},
    {"unsorted, depth writes off", "no holes, but the wrong pane ends up on top"},
};
const char* const kCullNames[3] = {"CullClockwise", "CullNone", "CullCounterClockwise"};
const char* const kCullDetails[3] = {
    "default here: you see the outside of the box; its inner walls are culled",
    "both sides drawn: the inside of the box shows too",
    "inverted: only the inside is drawn, the outside vanishes",
};
const char* const kFillNames[3] = {"Solid", "WireFrame", "Solid + WireFrame"};

class RenderStateRoom final : public Exhibit {
public:
    RenderStateRoom(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        for (int i = 0; i < 4; ++i) {
            const StationPlace& st = kStations[i];
            // A low dark stage under each pedestal.
            b.AddBox(Vector3(st.x - 1.3f, 0.0f, st.z - 1.3f), Vector3(st.x + 1.3f, 0.1f, st.z + 1.3f), "metal", 1.0f, Vector3(0.45f, 0.45f, 0.5f), false);
            b.AddPedestal(st.x, st.z, kPedestal, kPedestalH, "stone", Vector3(0.85f, 0.85f, 0.9f));
        }
        SignSpec title;
        title.lines = {{"Render States", true, kGold},
                       {"The same triangles, drawn with different DepthStencilState,", false, kText},
                       {"RasterizerState and BlendState. Press E on a pedestal to change its state.", false, kText}};
        b.AddWallSign(Wall::East, 0.0f, 3.2f, 5.6f, 1.45f, title);
        SignSpec hint;
        hint.lines = {{"CNA restores every state it changes", true, kGold},
                      {"Each exhibit sets the complete render state it needs, so none of them can", false, kText},
                      {"corrupt the others: nothing here relies on what was drawn before.", false, kText}};
        b.AddWallSign(Wall::South, 0.0f, 3.2f, 5.6f, 1.45f, hint);
        b.AddLight(Vector3(0.0f, 4.5f, 0.0f), Vector3(1.0f, 0.95f, 0.85f), 0.5f, 10.0f);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        for (int i = 0; i < 4; ++i) {
            screens_[i] = std::make_unique<LiveScreen>(S(), 512, 192);
            dirty_[i] = true;
            Interactable ia;
            ia.room = R().id;
            const StationPlace& st = kStations[i];
            ia.bounds = BoundingBox(R().ToWorld(st.x - 0.55f, 0.0f, st.z - 0.55f), R().ToWorld(st.x + 0.55f, kPedestalH + 1.5f, st.z + 0.55f));
            ia.maxDistance = 4.5f;
            ia.prompt = [this, i] { return StationPrompt(i); };
            ia.activate = [this, i] { Press(i); };
            interaction.Add(std::move(ia));
        }
        MeshBuilder open;
        open.AddBox(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f), 0.0f, Vector3(1, 1, 1), BoxFaces::NoTop, 10.0f);
        openBox_ = std::make_unique<LitMesh>(*S().device, open);
        nativeWireframe_ = S().device->SupportsCapability(CNA::GraphicsCapability::WireFrame);
        if (!nativeWireframe_) {
            // FillMode.WireFrame is unavailable (e.g. OpenGL ES): fall back to drawing the model's edges as lines.
            if (Model* knot = S().models->Get("knot")) {
                wireBright_ = WireMesh::FromModel(*S().device, *knot, Color(235, 242, 255, 255));
                wireDark_ = WireMesh::FromModel(*S().device, *knot, Color(12, 12, 20, 255));
            }
        }
        wireframeSupported_ = nativeWireframe_ || wireBright_ != nullptr;
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        angle_ += ctx.dt * 0.6f;
        eye_ = ctx.playerPosition;
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (ctx.offscreen || S().font == nullptr) return;
        for (int i = 0; i < 4; ++i) {
            if (screens_[i] == nullptr || (screens_[i]->HasContent() && !dirty_[i])) continue;
            dirty_[i] = false;
            screens_[i]->Render(Color(16, 20, 32, 255), [this, i](SpriteBatch& sb, int w, int h) { DrawScreen(sb, w, h, i); });
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        GraphicsDevice& device = ctx.device;
        RenderStates::SetOpaque3D(device);
        DrawScreens(ctx);
        DrawDepthStation(ctx);
        DrawCullStation(ctx);
        DrawFillStation(ctx);
        RenderStates::SetOpaque3D(device);   // leave nothing of ours behind
    }

    void DrawTransparent(const DrawContext& ctx) override
    {
        DrawBlendStation(ctx);
    }

private:
    [[nodiscard]] Vector3 ObjectCentre(int i) const
    {
        return R().ToWorld(kStations[i].x, kPedestalH + kObjectLift, kStations[i].z);
    }

    [[nodiscard]] std::string StationPrompt(int i) const
    {
        switch (i) {
            case Depth: return std::string(depthTest_ ? "Turn the depth test off" : "Turn the depth test on");
            case Culling: return std::string("Culling: ") + kCullNames[cull_] + "  (next)";
            case Fill: return wireframeSupported_ ? std::string("Fill mode: ") + kFillNames[fill_] + "  (next)"
                                                  : std::string("No wireframe available on this renderer");
            default: return std::string("Blend: ") + kBlends[blend_].name + "  (next)";
        }
    }

    void Press(int i)
    {
        switch (i) {
            case Depth: depthTest_ = !depthTest_; break;
            case Culling: cull_ = (cull_ + 1) % 3; break;
            case Fill: if (wireframeSupported_) fill_ = (fill_ + 1) % 3; break;
            default:
                // Blend modes first; once the blend wraps around, the draw-order variant advances.
                blend_ = (blend_ + 1) % 4;
                if (blend_ == 0) order_ = (order_ + 1) % 3;
                break;
        }
        dirty_[i] = true;
    }

    void DrawScreen(SpriteBatch& sb, int w, int h, int i)
    {
        Texture2D& white = *S().textures->White();
        sb.Draw(white, Rectangle(0, 0, w, 4), Color(200, 170, 90, 255));
        sb.Draw(white, Rectangle(0, h - 4, w, 4), Color(200, 170, 90, 255));
        const auto line = [&](const std::string& text, float y, const Color& c, float scale) {
            sb.DrawString(*S().font, text, Vector2(14, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
        };
        line(kStations[i].title, 8.0f, kGold, 1.05f);
        switch (i) {
            case Depth:
                line(depthTest_ ? "DepthStencilState.Default" : "DepthStencilState.None", 52.0f, depthTest_ ? kGreen : kRed, 1.2f);
                line(depthTest_ ? "Each pixel keeps the nearest surface:" : "No depth test: the last cube drawn wins,", 100.0f, kText, 0.95f);
                line(depthTest_ ? "the cubes cut through each other cleanly." : "so the blue cube is painted over the red.", 128.0f, kText, 0.95f);
                break;
            case Culling:
                line(kCullNames[cull_], 52.0f, kGreen, 1.2f);
                line(kCullDetails[cull_], 100.0f, kText, 0.82f);
                line("The box has no lid: look into it from above.", 128.0f, kText, 0.82f);
                break;
            case Fill:
                line(wireframeSupported_ ? kFillNames[fill_] : "unsupported", 52.0f, wireframeSupported_ ? kGreen : kRed, 1.2f);
                line(nativeWireframe_ ? "FillMode.WireFrame draws edges, not triangles." : "WireFrame capability is false on this renderer:", 100.0f, kText, 0.85f);
                line(nativeWireframe_ ? "Same mesh, same vertices, other rasteriser mode." : "the edges are drawn as a LineList instead.", 128.0f, kText, 0.85f);
                break;
            default:
                line(kBlends[blend_].name, 52.0f, kGreen, 1.1f);
                line(kBlends[blend_].detail, 98.0f, kText, 0.78f);
                line(std::string("Order: ") + kOrders[order_].name, 126.0f, kGold, 0.9f);
                line(kOrders[order_].detail, 154.0f, kText, 0.78f);
                break;
        }
    }

    // Each station's live plaque faces the middle of the room, on the pedestal's side.
    void DrawScreens(const DrawContext& ctx)
    {
        for (int i = 0; i < 4; ++i) {
            if (screens_[i] == nullptr || !screens_[i]->HasContent()) continue;
            const StationPlace& st = kStations[i];
            const Vector3 n(-(st.x > 0 ? 1.0f : -1.0f), 0.0f, 0.0f);    // face the room's centre line (x)
            const Vector3 right = Vector3::Normalize(Vector3::Cross(-n, Vector3::Up));
            const float w = 0.86f, h = w * 192.0f / 512.0f;
            const Vector3 centre = R().ToWorld(st.x + n.X * (kPedestal * 0.5f + 0.012f), 0.45f, st.z);
            QuadDraw::Textured(ctx, screens_[i]->Texture(), centre - right * (w / 2) - Vector3(0, h / 2, 0), right * w, Vector3(0, h, 0));
        }
    }

    void DrawDepthStation(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        device.setDepthStencilStateProperty(depthTest_ ? DepthStencilState::Default : DepthStencilState::None);
        BasicEffect& fx = ctx.fx.BeginLit(S().textures->Get("checker"));
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setSpecularColorProperty(Vector3(0.2f, 0.2f, 0.2f));
        fx.setSpecularPowerProperty(16.0f);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        const Vector3 c = ObjectCentre(Depth);
        const Matrix spin = Matrix::CreateRotationY(angle_);
        // Red first, then blue: with the depth test off, whichever is drawn second covers the other.
        fx.setDiffuseColorProperty(Vector3(1.0f, 0.35f, 0.3f));
        fx.setWorldProperty(Matrix::CreateScale(0.85f) * Matrix::CreateRotationY(0.3f) * spin * Matrix::CreateTranslation(c));
        EffectBank::Apply(fx);
        S().shapes->Cube().Draw(device);
        fx.setDiffuseColorProperty(Vector3(0.3f, 0.5f, 1.0f));
        fx.setWorldProperty(Matrix::CreateScale(0.85f) * Matrix::CreateRotationY(MathHelper::PiOver4 + 0.3f) * spin * Matrix::CreateTranslation(c));
        EffectBank::Apply(fx);
        S().shapes->Cube().Draw(device);
        device.setDepthStencilStateProperty(DepthStencilState::Default);
    }

    void DrawCullStation(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        const RasterizerState* modes[3] = {&RasterizerState::CullClockwise, &RasterizerState::CullNone, &RasterizerState::CullCounterClockwise};
        device.setRasterizerStateProperty(*modes[cull_]);
        BasicEffect& fx = ctx.fx.BeginLit(S().textures->Get("checker"));
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setDiffuseColorProperty(Vector3(1.0f, 0.9f, 0.55f));
        fx.setSpecularColorProperty(Vector3(0, 0, 0));
        fx.setEmissiveColorProperty(Vector3(0.12f, 0.1f, 0.05f));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setWorldProperty(Matrix::CreateScale(1.05f) * Matrix::CreateRotationY(angle_ * 0.5f) *
                            Matrix::CreateTranslation(ObjectCentre(Culling) + Vector3(0, -0.08f, 0)));
        EffectBank::Apply(fx);
        openBox_->Draw(device);
        device.setRasterizerStateProperty(RasterizerState::CullClockwise);
    }

    void DrawFillStation(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        Model* knot = S().models->Get("knot");
        if (knot == nullptr) return;
        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setSpecularColorProperty(Vector3(0.4f, 0.4f, 0.4f));
        fx.setSpecularPowerProperty(30.0f);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        const Matrix world = Matrix::CreateScale(1.15f) * Matrix::CreateRotationX(0.4f) * Matrix::CreateRotationY(angle_) *
                             Matrix::CreateTranslation(ObjectCentre(Fill));
        const bool wire = wireframeSupported_ && (fill_ == 1 || fill_ == 2);
        const bool solid = !wire || fill_ == 2;
        const Vector3 body(0.75f, 0.45f, 0.9f);
        if (solid) {
            ModelUtil::DrawWithEffect(device, *knot, fx, world, &body);
        }
        if (wire) {
            // Overlaid on the solid pass, the wire is a hair larger so it never z-fights with the surface.
            const Matrix wireWorld = fill_ == 2 ? Matrix::CreateScale(1.012f) * world : world;
            const Vector3 lineColour = fill_ == 2 ? Vector3(0.05f, 0.05f, 0.08f) : Vector3(0.9f, 0.95f, 1.0f);
            if (nativeWireframe_) {
                RasterizerState wireState;
                wireState.setFillModeProperty(FillMode::WireFrame);
                wireState.setCullModeProperty(CullMode::None);
                device.setRasterizerStateProperty(wireState);
                fx.setLightingEnabledProperty(false);
                fx.setEmissiveColorProperty(Vector3(0, 0, 0));
                ModelUtil::DrawWithEffect(device, *knot, fx, wireWorld, &lineColour);
            } else {
                device.setRasterizerStateProperty(RasterizerState::CullNone);
                BasicEffect& flat = ctx.fx.Flat();
                flat.setWorldProperty(wireWorld);
                flat.setViewProperty(ctx.view.view);
                flat.setProjectionProperty(ctx.view.projection);
                EffectBank::Apply(flat);
                (fill_ == 2 ? wireDark_ : wireBright_)->Draw(device);
            }
            device.setRasterizerStateProperty(RasterizerState::CullClockwise);
        }
    }

    void DrawBlendStation(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        const BlendState* blends[4] = {&BlendState::Opaque, &BlendState::AlphaBlend, &BlendState::NonPremultiplied, &BlendState::Additive};
        const Vector3 c = ObjectCentre(Blend);

        // A solid yellow sphere in the middle: the thing the panes are in front of.
        {
            device.setBlendStateProperty(BlendState::Opaque);
            device.setDepthStencilStateProperty(DepthStencilState::Default);
            device.setRasterizerStateProperty(RasterizerState::CullClockwise);
            BasicEffect& fx = ctx.fx.BeginLit();
            fx.EnableDefaultLighting();
            fx.setPreferPerPixelLightingProperty(true);
            fx.setDiffuseColorProperty(Vector3(1.0f, 0.85f, 0.25f));
            fx.setSpecularColorProperty(Vector3(0.5f, 0.5f, 0.5f));
            fx.setSpecularPowerProperty(30.0f);
            fx.setEmissiveColorProperty(Vector3(0.25f, 0.2f, 0.04f));
            fx.setViewProperty(ctx.view.view);
            fx.setProjectionProperty(ctx.view.projection);
            fx.setWorldProperty(Matrix::CreateScale(0.42f) * Matrix::CreateTranslation(c));
            EffectBank::Apply(fx);
            S().shapes->Sphere().Draw(device);
        }

        // Three crossing panes, rotating about the pedestal axis.
        struct Pane {
            Vector3 centre;
            Vector3 right;
            Vector3 colour;
            float distance;
        };
        std::array<Pane, 3> panes;
        const Vector3 colours[3] = {Vector3(1.0f, 0.2f, 0.2f), Vector3(0.2f, 1.0f, 0.3f), Vector3(0.25f, 0.4f, 1.0f)};
        for (int k = 0; k < 3; ++k) {
            const float a = angle_ * 0.45f + static_cast<float>(k) * MathHelper::TwoPi / 3.0f;
            const Vector3 dir(std::cos(a), 0.0f, std::sin(a));
            panes[static_cast<std::size_t>(k)] = {c + dir * 0.0f, dir, colours[k], 0.0f};
            // Panes hang off the axis: each one is centred half a width away from it.
            panes[static_cast<std::size_t>(k)].centre = c + dir * 0.5f;
            panes[static_cast<std::size_t>(k)].distance = Vector3::Distance(panes[static_cast<std::size_t>(k)].centre, ctx.view.eye);
        }
        const bool sorted = order_ == 0;
        if (sorted) std::sort(panes.begin(), panes.end(), [](const Pane& a, const Pane& b) { return a.distance > b.distance; });

        device.setBlendStateProperty(*blends[blend_]);
        device.setDepthStencilStateProperty(order_ == 1 ? DepthStencilState::Default : DepthStencilState::DepthRead);
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        BasicEffect& flat = ctx.fx.Flat();
        flat.setWorldProperty(Matrix::getIdentityProperty());
        flat.setViewProperty(ctx.view.view);
        flat.setProjectionProperty(ctx.view.projection);
        EffectBank::Apply(flat);
        const float alpha = 0.5f;
        for (const Pane& p : panes) {
            // Premultiplied blending needs the colour pre-scaled by alpha; the other modes want it straight.
            const bool premultiplied = blend_ == 1;
            const bool opaque = blend_ == 0;
            const float scale = premultiplied ? alpha : 1.0f;
            const auto to8 = [](float v) { return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
            const Color col(to8(p.colour.X * scale), to8(p.colour.Y * scale), to8(p.colour.Z * scale), to8(opaque ? 1.0f : alpha));
            const Vector3 up(0, 0.95f, 0);
            const Vector3 half = p.right * 0.5f;
            const Vector3 base = p.centre - half - up * 0.5f;
            const Vector3 a = base, b = base + p.right, d = base + p.right + up, e = base + up;
            const VertexPositionColor verts[6] = {{a, col}, {b, col}, {d, col}, {a, col}, {d, col}, {e, col}};
            device.DrawUserPrimitives(PrimitiveType::TriangleList, verts, 0, 2);
        }
        RenderStates::SetOpaque3D(device);
    }

    std::array<std::unique_ptr<LiveScreen>, 4> screens_;
    std::array<bool, 4> dirty_{};
    std::unique_ptr<LitMesh> openBox_;
    bool depthTest_ = true;
    int cull_ = 0;
    int fill_ = 0;
    int blend_ = 2;
    int order_ = 0;
    bool wireframeSupported_ = true;   // native FillMode.WireFrame or the LineList emulation
    bool nativeWireframe_ = true;
    std::unique_ptr<WireMesh> wireBright_;
    std::unique_ptr<WireMesh> wireDark_;
    float angle_ = 0.0f;
    Vector3 eye_{0, 1.65f, 0};
};

}  // namespace

std::unique_ptr<Exhibit> MakeRenderStateRoom(const RoomDef& room, Services& services)
{
    return std::make_unique<RenderStateRoom>(room, services);
}
