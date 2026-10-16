#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "Audio/AudioSystem.hpp"
#include "CNA/GraphicsCapability.hpp"
#include "CNA/Version.hpp"
#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LiveScreen.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SignFactory.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const Color kGold(255, 226, 150, 255);
const Color kText(222, 228, 240, 255);
const Color kDim(150, 162, 186, 255);
const Color kGreen(150, 255, 170, 255);

constexpr float kScreenW = 4.6f;
constexpr float kScreenH = 2.6f;
constexpr float kScreenY = 3.1f;     // centre height
constexpr float kScreenAlong = 5.3f;
constexpr int kHistory = 160;

SignSpec TextPanel(const std::string& title, const std::vector<std::string>& lines)
{
    SignSpec s;
    s.pixelWidth = 1012;
    s.pixelHeight = 572;
    s.leftAlign = true;
    s.textScale = 1.5f;
    s.lines.push_back({title, true, kGold});
    s.lines.push_back({" ", false, kText});
    for (const std::string& l : lines) s.lines.push_back({l, false, kText});
    return s;
}

class InfoHall final : public Exhibit {
public:
    InfoHall(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        for (const float x : {-6.6f, 6.6f})
            for (const float z : {-6.6f, 6.6f}) b.AddColumn(x, z, 0.42f, "marble", Vector3(0.95f, 0.95f, 1.0f));

        // The central dais: two stone steps and a marble top. Solid, so visitors walk around it.
        const Vector3 c = b.World(0, 0, 0);
        b.Meshes().Opaque("stone").AddCylinder(c, 3.7f, 0.16f, 40, 2.0f, Vector3(0.8f, 0.8f, 0.85f), true);
        b.Meshes().Opaque("marble").AddCylinder(c, 3.2f, 0.34f, 40, 3.0f, Vector3(1.0f, 1.0f, 1.05f), true);
        b.AddCollider(BoundingBox(c + Vector3(-3.4f, 0, -3.4f), c + Vector3(3.4f, 0.4f, 3.4f)));

        // Eight wall screens; the east-left one is live (drawn each frame), the rest are rendered once here.
        const RoomDef& r = R();
        SignSpec about = TextPanel("About CNA", {
            "CNA is a C++ reimplementation of Microsoft XNA 4.0.",
            "It keeps XNA's classes and their behaviour: Game, GraphicsDevice,",
            "ContentManager, SpriteBatch, BasicEffect, Model, SoundEffect,",
            "Matrix, BoundingBox, Keyboard, Mouse ...",
            " ",
            "It runs them on SDL3 and a pluggable renderer layer: OpenGL ES 3,",
            "OpenGL 3.3, Vulkan, Direct3D, Metal, WebGPU, a software rasteriser",
            "and more. This museum is an ordinary CNA application: every pixel",
            "and every sound goes through CNA's public API."});
        b.AddWallSign(Wall::North, -kScreenAlong, kScreenY, kScreenW, kScreenH, about);

        SignSpec controls = TextPanel("Controls", {
            "W A S D / arrow keys    move            Shift    toggle walk / run",
            "Mouse                   look around     E / click    interact",
            "Esc                     pause menu      F1 / H       help overlay",
            "M                       museum map      F3           statistics",
            "F11                     fullscreen      R            back to the entrance",
            "F2                      mute            [ ]  mouse sensitivity",
            " ",
            "Gamepad: left stick walks, right stick looks, A interacts, Start pauses."});
        b.AddWallSign(Wall::North, kScreenAlong, kScreenY, kScreenW, kScreenH, controls);

        std::vector<std::string> guide;
        for (const WingDef& wing : S().museum->wings) {
            if (wing.floor != R().floor || wing.index != R().wing) continue;   // the rooms of this wing
            for (const int index : wing.rooms) {
                const RoomDef& room = S().museum->Room(index);
                if (room.index != R().index) guide.push_back(room.title + "  -  " + room.subtitle);
            }
        }
        SignSpec rooms = TextPanel("Room guide", guide);
        for (SignLine& l : rooms.lines) l.color = (&l == &rooms.lines.front()) ? kGold : kText;
        b.AddWallSign(Wall::South, -kScreenAlong, kScreenY, kScreenW, kScreenH, rooms);

        SignSpec features = TextPanel("CNA APIs on show", {
            "Game, GameTime, GraphicsDeviceManager, GraphicsDevice, ContentManager",
            "Model, ModelMesh, BasicEffect (lights, fog, textures, per-pixel)",
            "Texture2D (mip levels, SetData), SamplerState, RasterizerState,",
            "BlendState, DepthStencilState, RenderTarget2D, DynamicVertexBuffer",
            "VertexBuffer, IndexBuffer, DrawUserPrimitives, SpriteBatch, SpriteFont",
            "Matrix, Quaternion, Vector2/3, BoundingBox, BoundingSphere,",
            "BoundingFrustum, Ray, MathHelper",
            "Keyboard, Mouse (relative mode), GamePad",
            "SoundEffect, SoundEffectInstance, AudioListener, AudioEmitter"});
        b.AddWallSign(Wall::South, kScreenAlong, kScreenY, kScreenW, kScreenH, features);

        // Assets and licences.
        SignSpec credits = TextPanel("Assets and licences", {
            "Every texture, model and sound is generated by tools/gen_assets.py,",
            "so there are no third-party assets to attribute.",
            "The text font is DejaVu Sans (Bitstream Vera / public domain licence),",
            "rasterised by tools/gen_font.py.",
            " ",
            "CNA is licensed under the Microsoft Public License (Ms-PL).",
            "The museum's own code is MIT licensed.",
            " ",
            std::string("Built against CNA ") + std::string(CNA::getVersionString()) + "."});
        b.AddWallSign(Wall::West, -kScreenAlong, kScreenY, kScreenW, kScreenH, credits);

        SignSpec tips = TextPanel("Things to try", {
            "Press E on almost anything you can walk up to.",
            "Lighting Room: switch the three directional lights on and off.",
            "Render State Room: turn depth testing and culling off.",
            "Render Target Room: watch yourself on the security monitors.",
            "Sky Courtyard: change the time of day.",
            "Audio Room: walk between the speakers.",
            "F3 shows statistics; M shows the museum map."});
        b.AddWallSign(Wall::West, kScreenAlong, kScreenY, kScreenW, kScreenH, tips);

        // The renderer screen: what the active renderer reports it can do.
        b.AddWallSign(Wall::East, kScreenAlong, kScreenY, kScreenW, kScreenH, CapabilityPanel());

        // Frame for the live screen on the east wall.
        b.AddBox(Vector3(r.halfW - 0.08f, kScreenY - kScreenH / 2 - 0.1f, -kScreenAlong - kScreenW / 2 - 0.1f),
                 Vector3(r.halfW - 0.005f, kScreenY + kScreenH / 2 + 0.1f, -kScreenAlong + kScreenW / 2 + 0.1f), "metal", 1.0f,
                 Vector3(0.3f, 0.3f, 0.34f), false);
        b.AddLight(Vector3(0.0f, 5.5f, 0.0f), Vector3(0.95f, 0.97f, 1.0f), 0.9f, 14.0f);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        screen_ = std::make_unique<LiveScreen>(S(), 1024, 576);
        Interactable logo;
        logo.bounds = BoundingBox(R().ToWorld(-1.6f, 0.0f, -1.6f), R().ToWorld(1.6f, 4.3f, 1.6f));
        logo.maxDistance = 6.0f;
        logo.prompt = [this] { return std::string(spinning_ ? "Stop the logo" : "Spin the logo"); };
        logo.activate = [this] { spinning_ = !spinning_; };
        interaction.Add(std::move(logo));

        Interactable stats;
        stats.bounds = BoundingBox(R().ToWorld(R().halfW - 0.5f, kScreenY - kScreenH / 2, -kScreenAlong - kScreenW / 2),
                                   R().ToWorld(R().halfW, kScreenY + kScreenH / 2, -kScreenAlong + kScreenW / 2));
        stats.maxDistance = 7.0f;
        stats.prompt = [this] { return std::string(graphMode_ ? "Show the numbers" : "Show the frame-time graph"); };
        stats.activate = [this] { graphMode_ = !graphMode_; dirty_ = true; };
        interaction.Add(std::move(stats));
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        history_[static_cast<std::size_t>(historyPos_ % kHistory)] = ctx.dt * 1000.0f;
        ++historyPos_;
        uptime_ = ctx.time;
        eye_ = ctx.playerPosition;
        if (spinning_) angle_ += ctx.dt * 0.8f;
        refresh_ += ctx.dt;
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (ctx.offscreen || screen_ == nullptr || S().font == nullptr) return;
        if (!screen_->HasContent() || refresh_ >= 0.25f || dirty_) {
            refresh_ = 0.0f;
            dirty_ = false;
            RenderStatsScreen();
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        GraphicsDevice& device = ctx.device;
        RenderStates::SetOpaque3D(device);
        const Vector3 c = R().ToWorld(0, 0, 0);

        // The live stats screen on the east wall (quad faces -X, i.e. into the room).
        if (screen_ != nullptr && screen_->HasContent()) {
            const Vector3 origin = R().ToWorld(R().halfW - 0.02f, kScreenY - kScreenH / 2, -kScreenAlong - kScreenW / 2);
            QuadDraw::Textured(ctx, screen_->Texture(), origin, Vector3(0, 0, kScreenW), Vector3(0, kScreenH, 0));
        }

        // The logo: a double-sided textured quad turning on the dais, with three lit cubes orbiting it.
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        const Matrix spin = Matrix::CreateRotationY(angle_);
        const Vector3 centre = c + Vector3(0, 2.3f, 0);
        const Vector3 right = Vector3::TransformNormal(Vector3(1, 0, 0), spin);
        QuadDraw::Textured(ctx, S().textures->Get("logo"), centre - right * 1.1f - Vector3(0, 1.1f, 0), right * 2.2f, Vector3(0, 2.2f, 0));
        device.setRasterizerStateProperty(RasterizerState::CullClockwise);

        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setSpecularColorProperty(Vector3(0.5f, 0.5f, 0.5f));
        fx.setSpecularPowerProperty(32.0f);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        const Vector3 colours[3] = {Vector3(0.93f, 0.38f, 0.31f), Vector3(0.38f, 0.75f, 0.47f), Vector3(0.35f, 0.59f, 0.94f)};
        for (int i = 0; i < 3; ++i) {
            const float a = angle_ * 1.4f + static_cast<float>(i) * MathHelper::TwoPi / 3.0f;
            const Vector3 p = c + Vector3(std::cos(a) * 2.2f, 1.2f + std::sin(a * 2.0f) * 0.25f, std::sin(a) * 2.2f);
            fx.setDiffuseColorProperty(colours[i]);
            fx.setWorldProperty(Matrix::CreateScale(0.55f) * Matrix::CreateRotationY(a * 2.0f) * Matrix::CreateRotationX(a) * Matrix::CreateTranslation(p));
            EffectBank::Apply(fx);
            S().shapes->Cube().Draw(device);
        }
    }

private:
    SignSpec CapabilityPanel()
    {
        GraphicsDevice& device = *S().device;
        const auto yes = [&](CNA::GraphicsCapability cap) { return device.SupportsCapability(cap) ? "yes" : "no"; };
        char line[160];
        std::vector<std::string> lines;
        lines.push_back(std::string("Active renderer: ") + std::string(device.GetGraphicsRendererName()));
        std::snprintf(line, sizeof(line), "3D pipeline: %s     Custom effects: %s     Wireframe: %s",
                      yes(CNA::GraphicsCapability::ThreeD), yes(CNA::GraphicsCapability::CustomEffects),
                      yes(CNA::GraphicsCapability::WireFrame));
        lines.push_back(line);
        std::snprintf(line, sizeof(line), "Depth/stencil: %s     Anisotropic: %s     MSAA: %s",
                      yes(CNA::GraphicsCapability::DepthStencilBuffer), yes(CNA::GraphicsCapability::AnisotropicFiltering),
                      yes(CNA::GraphicsCapability::MultiSampleAntiAliasing));
        lines.push_back(line);
        std::snprintf(line, sizeof(line), "Render targets (MRT): %s     Float targets: %s     Instancing: %s",
                      yes(CNA::GraphicsCapability::MultipleRenderTargets), yes(CNA::GraphicsCapability::FloatRenderTargets),
                      yes(CNA::GraphicsCapability::Instancing));
        lines.push_back(line);
        std::snprintf(line, sizeof(line), "Additive blending: %s     Occlusion queries: %s     3D textures: %s",
                      yes(CNA::GraphicsCapability::AdditiveBlending), yes(CNA::GraphicsCapability::OcclusionQuery),
                      yes(CNA::GraphicsCapability::Texture3D));
        lines.push_back(line);
        std::snprintf(line, sizeof(line), "Largest texture: %d x %d", device.GetMaxTextureDimension(), device.GetMaxTextureDimension());
        lines.push_back(line);
        lines.push_back(" ");
        lines.push_back("The museum asks the device what it supports and adapts;");
        lines.push_back("it never tests the renderer's name.");
        return TextPanel("What this renderer reports", lines);
    }

    void RenderStatsScreen()
    {
        const FrameStats* st = S().stats;
        const std::string rendererName = std::string(S().device->GetGraphicsRendererName());
        screen_->Render(Color(12, 16, 28, 255), [&](SpriteBatch& sb, int w, int h) {
            Texture2D& white = *S().textures->White();
            sb.Draw(white, Rectangle(0, 0, w, 6), Color(200, 170, 90, 255));
            sb.Draw(white, Rectangle(0, h - 6, w, 6), Color(200, 170, 90, 255));
            const SpriteFont& title = *S().titleFont;
            const SpriteFont& font = *S().font;
            sb.DrawString(title, graphMode_ ? "Frame times" : "CNA 3D Museum - live", Vector2(34, 22), kGold, 0.0f, Vector2(0, 0), 0.9f,
                          SpriteEffects::None, 0.0f);
            char buf[200];
            if (!graphMode_) {
                const auto row = [&](int i, const char* key, const std::string& value) {
                    const float y = 112.0f + static_cast<float>(i) * 40.0f;
                    sb.DrawString(font, key, Vector2(40, y), kDim, 0.0f, Vector2(0, 0), 1.25f, SpriteEffects::None, 0.0f);
                    sb.DrawString(font, value, Vector2(330, y), kGreen, 0.0f, Vector2(0, 0), 1.25f, SpriteEffects::None, 0.0f);
                };
                int i = 0;
                row(i++, "CNA version", std::string(CNA::getVersionString()));
                row(i++, "Renderer", rendererName);
                std::snprintf(buf, sizeof(buf), "%d x %d  (%s)", st ? st->backBufferWidth : 0, st ? st->backBufferHeight : 0,
                              st && st->fullscreen ? "fullscreen" : "windowed");
                row(i++, "Resolution", buf);
                std::snprintf(buf, sizeof(buf), "%.0f fps   (%.2f ms)", st ? static_cast<double>(st->fps) : 0.0,
                              st ? static_cast<double>(st->frameMilliseconds) : 0.0);
                row(i++, "Frame rate", buf);
                std::snprintf(buf, sizeof(buf), "%.1f, %.1f, %.1f", static_cast<double>(eye_.X), static_cast<double>(eye_.Y), static_cast<double>(eye_.Z));
                row(i++, "Visitor at", buf);
                std::snprintf(buf, sizeof(buf), "%d zones in view", st ? st->roomsDrawn : 0);
                row(i++, "Culling", buf);
                std::snprintf(buf, sizeof(buf), "%d voices", st ? st->activeVoices : 0);
                row(i++, "Audio", buf);
                const int total = static_cast<int>(uptime_);
                std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", total / 3600, (total / 60) % 60, total % 60);
                row(i++, "Museum time", buf);
            } else {
                // Bars for the last kHistory frames; the line marks 16.7 ms (60 fps).
                const int gx = 40, gy = 120, gw = w - 80, gh = h - 190;
                sb.Draw(white, Rectangle(gx, gy, gw, gh), Color(24, 30, 48, 255));
                float peak = 33.4f;
                for (const float t : history_) peak = std::max(peak, t);
                const int n = std::min(historyPos_, kHistory);
                const float barW = static_cast<float>(gw) / static_cast<float>(kHistory);
                for (int k = 0; k < n; ++k) {
                    const int idx = (historyPos_ - n + k) % kHistory;
                    const float t = history_[static_cast<std::size_t>(idx)];
                    const int bh = static_cast<int>(std::min(t / peak, 1.0f) * static_cast<float>(gh));
                    const Color c = t < 17.5f ? Color(110, 220, 130, 255) : (t < 34.0f ? Color(240, 200, 90, 255) : Color(240, 100, 90, 255));
                    sb.Draw(white, Rectangle(gx + static_cast<int>(static_cast<float>(k) * barW), gy + gh - bh, std::max(1, static_cast<int>(barW)), bh), c);
                }
                const int line = gy + gh - static_cast<int>(16.7f / peak * static_cast<float>(gh));
                sb.Draw(white, Rectangle(gx, line, gw, 2), Color(255, 255, 255, 180));
                sb.DrawString(font, "16.7 ms", Vector2(static_cast<float>(gx + 6), static_cast<float>(line - 26)), Color::White, 0.0f, Vector2(0, 0), 0.9f,
                              SpriteEffects::None, 0.0f);
                std::snprintf(buf, sizeof(buf), "%.0f fps   peak %.1f ms", st ? static_cast<double>(st->fps) : 0.0, static_cast<double>(peak));
                sb.DrawString(font, buf, Vector2(static_cast<float>(gx), static_cast<float>(gy + gh + 14)), kGreen, 0.0f, Vector2(0, 0), 1.2f,
                              SpriteEffects::None, 0.0f);
            }
        });
    }

    std::unique_ptr<LiveScreen> screen_;
    std::array<float, kHistory> history_{};
    int historyPos_ = 0;
    bool spinning_ = true;
    bool graphMode_ = false;
    bool dirty_ = false;
    float angle_ = 0.0f;
    float refresh_ = 1.0f;
    double uptime_ = 0.0;
    Vector3 eye_{0, 1.65f, 0};
};

}  // namespace

std::unique_ptr<Exhibit> MakeInfoHall(const RoomDef& room, Services& services)
{
    return std::make_unique<InfoHall>(room, services);
}
