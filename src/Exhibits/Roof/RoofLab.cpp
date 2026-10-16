// Open-air exhibits under the museum's sky. They use the museum clock (TimeOfDay: sun, moon, daylight, ambient),
// animated sculptures, a particle fountain and drifting leaves, a telescope that renders the sky and a distant
// skyline through a narrow field of view into a render target, and a weather station with rain and snow.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "Audio/AudioSystem.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/Gizmos.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/MiniView.hpp"
#include "Rendering/Particles.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SkyDome.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Deck, Garden, Sculpture, Telescope, Weather };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {
        {"garden", Mode::Garden}, {"sculpture", Mode::Sculpture}, {"telescope", Mode::Telescope}, {"weather", Mode::Weather}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Deck;
}

std::string Clock(float hours)
{
    const int total = static_cast<int>(hours * 60.0f) % (24 * 60);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", total / 60, total % 60);
    return buf;
}
std::string Num(double v, const char* f = "%.1f")
{
    char buf[48];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

class RoofLab final : public ScreenExhibit {
public:
    RoofLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))), rng_(2024), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Deck:
                p.AddSlider("Museum time (hours)", 0.0f, 24.0f, S().timeOfDay->Hours(), [this](float v) { S().timeOfDay->SetHours(v); }, "%.1f");
                p.AddButton("One hour later", [this] { S().timeOfDay->Advance(1.0f); });
                p.AddButton("One hour earlier", [this] { S().timeOfDay->Advance(-1.0f); });
                p.AddReadout("Clock", [this] { return Clock(S().timeOfDay->Hours()); });
                p.AddReadout("Daylight (0..1)", [this] { return Num(static_cast<double>(S().timeOfDay->Daylight()), "%.2f"); });
                p.AddReadout("Sun elevation", [this] { return Num(Elevation()) + " degrees"; });
                p.AddReadout("Key T / Y", [] { return std::string("hour later / day runs"); });
                break;
            case Mode::Garden:
                p.AddSlider("Wind (m/s)", 0.0f, 12.0f, wind_, [this](float v) { wind_ = v; }, "%.1f");
                p.AddSlider("Falling leaves per second", 0.0f, 40.0f, leafRate_, [this](float v) { leafRate_ = v; }, "%.0f");
                p.AddSlider("Fountain height", 0.4f, 1.6f, jet_, [this](float v) { jet_ = v; }, "%.2f");
                p.AddReadout("Water particles", [this] { return std::to_string(water_ ? water_->Size() : 0); });
                p.AddReadout("Leaves", [this] { return std::to_string(leaves_ ? leaves_->Size() : 0); });
                break;
            case Mode::Sculpture:
                p.AddSlider("Rotation speed", -2.0f, 2.0f, sculptureSpeed_, [this](float v) { sculptureSpeed_ = v; }, "%.2f");
                p.AddChoice("Finish", {"Bronze", "Marble", "Painted"}, 0, [this](int v) { sculptureFinish_ = v; });
                p.AddToggle("Rotate sculptures", true, [this](bool v) { sculpturesRotate_ = v; });
                p.AddReadout("Garden lamps", [this] { return S().timeOfDay->Daylight() < 0.35f ? std::string("on") : std::string("off until dusk"); });
                break;
            case Mode::Telescope:
                p.AddSlider("Heading (degrees)", 0.0f, 360.0f, heading_, [this](float v) { heading_ = v; }, "%.0f", 1.0f);
                p.AddSlider("Elevation (degrees)", -10.0f, 80.0f, elevation_, [this](float v) { elevation_ = v; }, "%.0f", 1.0f);
                p.AddSlider("Field of view (degrees)", 2.0f, 40.0f, fov_, [this](float v) { fov_ = v; }, "%.1f");
                p.AddButton("Aim at the sun", [this] { AimAt(S().timeOfDay->SunDirection()); });
                p.AddButton("Aim at the moon", [this] { AimAt(-S().timeOfDay->SunDirection()); });
                p.AddReadout("Magnification", [this] { return Num(static_cast<double>(40.0f / fov_)) + "x"; });
                break;
            case Mode::Weather:
                p.AddChoice("Weather", {"Clear", "Rain", "Snow", "Storm"}, 0, [this](int v) { weather_ = v; });
                p.AddSlider("Intensity", 0.1f, 1.0f, intensity_, [this](float v) { intensity_ = v; });
                p.AddSlider("Wind (m/s)", 0.0f, 14.0f, wind_, [this](float v) { wind_ = v; }, "%.1f");
                p.AddReadout("Precipitation", [this] { return std::to_string(drops_.size()) + " streaks, " + std::to_string(snow_ ? snow_->Size() : 0) + " flakes"; });
                p.AddReadout("Visibility", [this] { return Num(Visibility(), "%.0f") + " m"; });
                break;
        }
        // Scenery of the room itself.
        if (mode_ == Mode::Garden) {
            b.AddPedestal(0.0f, 0.0f, 2.8f, 0.5f, "stone");
            for (int i = 0; i < 4; ++i) {
                const float a = static_cast<float>(i) * MathHelper::PiOver2 + MathHelper::PiOver4;
                const float x = std::cos(a) * 4.6f, z = std::sin(a) * 4.6f;
                b.Meshes().Opaque("wood").AddCylinder(b.World(x, 0.0f, z), 0.18f, 1.8f, 10, 1.0f, Vector3(0.7f, 0.55f, 0.4f), true);
                b.Meshes().Opaque("grass").AddSphere(b.World(x, 2.7f, z), 1.1f, 14, 8, Vector3(0.5f, 0.8f, 0.45f));
                b.AddCollider(BoundingBox(b.World(x - 0.25f, 0, z - 0.25f), b.World(x + 0.25f, 2.0f, z + 0.25f)));
            }
            b.AddBox(Vector3(-5.2f, 0.0f, -1.4f), Vector3(-4.5f, 1.0f, 1.4f), "hedge", 1.0f, Vector3(1, 1, 1));
            b.AddBox(Vector3(4.5f, 0.0f, -1.4f), Vector3(5.2f, 1.0f, 1.4f), "hedge", 1.0f, Vector3(1, 1, 1));
        }
        if (mode_ == Mode::Sculpture) {
            sculptureBases_.push_back(b.AddPedestal(-3.4f, -2.5f, 2.2f, 0.55f, "stone"));
            sculptureBases_.push_back(b.AddPedestal(3.4f, -2.5f, 2.2f, 0.55f, "stone"));
            sculptureBases_.push_back(b.AddPedestal(0.0f, 0.6f, 2.2f, 0.55f, "stone"));
            b.AddBox(Vector3(-6.8f, 0.0f, -0.7f), Vector3(-6.2f, 1.0f, 3.8f), "hedge", 1.0f, Vector3(1, 1, 1));
            b.AddBox(Vector3(6.2f, 0.0f, -0.7f), Vector3(6.8f, 1.0f, 3.8f), "hedge", 1.0f, Vector3(1, 1, 1));
        }
        if (mode_ == Mode::Deck || mode_ == Mode::Garden || mode_ == Mode::Sculpture) AddLampPosts(b);
        if (mode_ == Mode::Telescope) {
            telescopeBase_ = b.AddPedestal(0.0f, 0.0f, 0.5f, 1.2f, "metal");
        }
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
        GraphicsDevice& d = *S().device;
        if (mode_ == Mode::Garden) {
            water_ = std::make_unique<ParticleSystem>(3000);
            leaves_ = std::make_unique<ParticleSystem>(600);
            renderer_ = std::make_unique<ParticleRenderer>(d, 3600);
            waterVoice_ = CreateLoop("water", R().ToWorld(0, 0.6f, 0), 0.35f, 10.0f);
            if (waterVoice_ != nullptr && S().audio != nullptr) S().audio->SetPlaying(waterVoice_, true);
        }
        if (mode_ == Mode::Weather) {
            snow_ = std::make_unique<ParticleSystem>(5000);
            splashes_ = std::make_unique<ParticleSystem>(2000);
            renderer_ = std::make_unique<ParticleRenderer>(d, 7000);
            streaks_ = std::make_unique<DynamicVertexBuffer>(d, VertexPositionColor::getVertexDeclarationStatic(), 2 * kMaxDrops, BufferUsage::WriteOnly);
            flat_ = std::make_unique<BasicEffect>(d);
            flat_->setVertexColorEnabledProperty(true);
            flat_->setLightingEnabledProperty(false);
            flat_->setTextureEnabledProperty(false);
        }
        if (mode_ == Mode::Telescope) {
            view_ = std::make_unique<MiniView>(S(), 960, 540);
            BuildSkyline();
            lit_ = std::make_unique<BasicEffect>(d);
            lit_->EnableDefaultLighting();
            lit_->setTextureEnabledProperty(true);
            lit_->setTextureProperty(S().textures->White());
            lit_->setFogEnabledProperty(true);
            lit_->setFogStartProperty(500.0f);
            lit_->setFogEndProperty(2500.0f);
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        if (mode_ != Mode::Sculpture) time_ += ctx.dt;
        else if (sculpturesRotate_) time_ += ctx.dt * sculptureSpeed_;
        const float dt = std::min(ctx.dt, 0.05f);
        if (mode_ == Mode::Garden) StepGarden(dt);
        if (mode_ == Mode::Weather) StepWeather(dt);
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (mode_ == Mode::Telescope && view_ && !ctx.offscreen) {
            const float h = MathHelper::ToRadians(heading_), e = MathHelper::ToRadians(elevation_);
            const Vector3 dir(std::sin(h) * std::cos(e), std::sin(e), -std::cos(h) * std::cos(e));
            const Vector3 eye(0, 0, 0);
            const Matrix view = Matrix::CreateLookAt(eye, dir, Vector3::Up);
            const Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(fov_), view_->Aspect(), 0.5f, 6000.0f);
            view_->Render(ctx, view, proj, eye, [this](const DrawContext& sub) { DrawTelescopeScene(sub); }, Color(60, 90, 140, 255));
        }
        ScreenExhibit::RenderOffscreen(ctx);
    }

    void DrawExtra(const DrawContext& ctx) override
    {
        if (mode_ == Mode::Deck) DrawDeck(ctx);
        if (mode_ == Mode::Garden) {
            DrawGarden(ctx);
            DrawLamps(ctx);
        }
        if (mode_ == Mode::Sculpture) {
            DrawSculptures(ctx);
            DrawLamps(ctx);
        }
        if (mode_ == Mode::Telescope) DrawTelescope(ctx);
        if (mode_ == Mode::Weather) DrawWeather(ctx);
    }

    void DrawTransparent(const DrawContext& ctx) override
    {
        if (!renderer_) return;
        if (mode_ == Mode::Garden) {
            renderer_->Draw(ctx, S().textures->Get("particle"), BlendState::NonPremultiplied, true, {water_.get(), leaves_.get()});
        } else if (mode_ == Mode::Weather) {
            renderer_->Draw(ctx, S().textures->Get("particle"), BlendState::NonPremultiplied, true, {snow_.get(), splashes_.get()});
        }
    }

    void DrawOverlay() override { gizmos_.DrawLabels(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        Texture2D& white = *S().whiteTexture;
        if (mode_ == Mode::Telescope && view_ && view_->HasContent()) {
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearClamp, nullptr, nullptr);
            sb.Draw(*view_->Texture(), Rectangle(0, 0, 960, 540), Color::White);
            sb.End();
        }
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        if (mode_ == Mode::Telescope) {
            rect(0, 0, 960, 38, Color(0, 0, 0, 150));
            text("heading " + Num(static_cast<double>(heading_), "%.0f") + "   elevation " + Num(static_cast<double>(elevation_), "%.0f") + "   field of view " + Num(static_cast<double>(fov_)) + " degrees   " + Num(static_cast<double>(40.0f / fov_)) + "x", 14, 6, Kit::kText, 0.7f);
            rect(478, 250, 4, 40, Color(255, 255, 255, 120));
            rect(458, 268, 44, 4, Color(255, 255, 255, 120));
        } else {
            rect(0, 0, 960, 540, Color(14, 18, 30, 255));
            text(Title(), 24, 12, Kit::kGold, 1.0f);
            if (mode_ == Mode::Deck) DrawSunPath(rect, text);
            else if (mode_ == Mode::Garden) {
                text("Wind " + Num(static_cast<double>(wind_)) + " m/s from the west", 30, 90, Kit::kText, 0.8f);
                text("Water: " + std::to_string(water_ ? water_->Size() : 0) + " particles under gravity, killed on the basin floor", 30, 130, Kit::kText, 0.7f);
                text("Leaves: " + std::to_string(leaves_ ? leaves_->Size() : 0) + " particles pushed sideways by the wind and spun", 30, 165, Kit::kText, 0.7f);
                text("Both systems are drawn together as camera-facing quads in one call.", 30, 215, Kit::kDim, 0.65f);
            } else if (mode_ == Mode::Sculpture) {
                static const char* finishes[] = {"bronze", "marble", "painted"};
                text(std::string("Three sculptures, finish: ") + finishes[sculptureFinish_], 30, 90, Kit::kText, 0.8f);
                text("Their world matrices combine scale, local animation, the room frame and a pedestal translation.", 30, 135, Kit::kText, 0.7f);
                text("The four exterior lamps share the museum clock and become emissive after dusk.", 30, 175, Kit::kDim, 0.65f);
            } else {
                const char* names[] = {"Clear", "Rain", "Snow", "Storm"};
                text(std::string("Conditions: ") + names[weather_] + ", wind " + Num(static_cast<double>(wind_)) + " m/s, intensity " + Num(static_cast<double>(intensity_), "%.2f"), 30, 90, Kit::kText, 0.8f);
                text("Visibility " + Num(Visibility(), "%.0f") + " m, " + std::to_string(drops_.size()) + " rain streaks (a line each) and " + std::to_string(snow_ ? snow_->Size() : 0) + " snowflakes (quads)", 30, 135, Kit::kText, 0.7f);
                text("Rain is a LineList in one DynamicVertexBuffer; splashes and snow are particles.", 30, 175, Kit::kDim, 0.65f);
                if (flash_ > 0.0f) rect(0, 0, 960, 540, Color(255, 255, 255, static_cast<int>(std::min(1.0f, flash_) * 160.0f)));
            }
        }
        sb.End();
    }

private:
    static constexpr int kMaxDrops = 6000;

    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Sun, moon and the museum clock", "Terrace garden", "Sculpture garden", "Telescope", "Weather station"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Deck: return {"TimeOfDay is the museum's clock: it turns the hour into a sun direction, daylight and ambient colours that", "light the sky, the window views and these lamps, which come on at dusk. Move the slider or press T and Y anywhere."};
            case Mode::Garden: return {"A fountain and drifting leaves are two ParticleSystems drawn by one ParticleRenderer: camera-facing quads in a", "DynamicVertexBuffer. The wind pushes the leaves sideways; the water falls back into the basin and dies on its floor."};
            case Mode::Sculpture: return {"Three animated sculptures combine primitive meshes with world matrices and material colours; each sits on a", "collidable stone plinth. The garden lamps become emissive when the shared museum clock reaches dusk."};
            case Mode::Telescope: return {"The telescope renders the real sky dome and a far skyline into a render target through a very narrow field of", "view: a small FOV is magnification. Aim it at the sun or the moon, then zoom in."};
            default: return {"Rain is one line per drop in a dynamic vertex buffer, snow and splashes are particles, lightning flashes the", "screen. Intensity and wind change the count and the slant; the sky and lamps keep following the museum clock."};
        }
    }
    [[nodiscard]] double Elevation() const { return static_cast<double>(MathHelper::ToDegrees(std::asin(std::clamp(S().timeOfDay->SunDirection().Y, -1.0f, 1.0f)))); }
    [[nodiscard]] double Visibility() const
    {
        const float base = 3000.0f;
        const float factor = weather_ == 0 ? 1.0f : (weather_ == 1 ? 0.35f : (weather_ == 2 ? 0.2f : 0.12f));
        return static_cast<double>(base * (1.0f - (1.0f - factor) * intensity_));
    }
    void AimAt(const Vector3& dir)
    {
        const Vector3 n = Vector3::Normalize(dir);
        elevation_ = std::clamp(MathHelper::ToDegrees(std::asin(std::clamp(n.Y, -1.0f, 1.0f))), -10.0f, 80.0f);
        float h = MathHelper::ToDegrees(std::atan2(n.X, -n.Z));
        if (h < 0.0f) h += 360.0f;
        heading_ = h;
    }
    [[nodiscard]] float Rand(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng_); }

    // --- deck ---------------------------------------------------------------------------------------
    template <typename Rect, typename Text>
    void DrawSunPath(Rect& rect, Text& text)
    {
        const float cx = 480.0f, cy = 400.0f, r = 300.0f;
        rect(cx - r - 10, cy, 2 * r + 20, 3, Kit::Rgba(110, 120, 150));
        for (int i = 0; i <= 48; ++i) {
            const float hours = static_cast<float>(i) * 0.5f;
            const float a = (hours - 6.0f) / 12.0f * MathHelper::Pi;
            const float x = cx - std::cos(a) * r, y = cy - std::sin(a) * r * 0.9f;
            rect(x - 2, y - 2, 5, 5, hours >= 6.0f && hours <= 18.0f ? Kit::Rgba(255, 210, 120) : Kit::Rgba(70, 80, 120));
        }
        const float h = S().timeOfDay->Hours();
        const float a = (h - 6.0f) / 12.0f * MathHelper::Pi;
        const float x = cx - std::cos(a) * r, y = std::min(cy + 60.0f, cy - std::sin(a) * r * 0.9f);
        rect(x - 14, y - 14, 28, 28, h > 6.0f && h < 18.0f ? Kit::Rgba(255, 230, 120) : Kit::Rgba(200, 210, 240));
        text("sunrise 06:00", cx - r - 30, cy + 12, Kit::kDim, 0.6f);
        text("sunset 18:00", cx + r - 70, cy + 12, Kit::kDim, 0.6f);
        text("It is " + Clock(h) + ": daylight " + Num(static_cast<double>(S().timeOfDay->Daylight()), "%.2f") + ", sun " + Num(Elevation()) + " degrees above the horizon", 30, 70, Kit::kText, 0.8f);
        const Vector3 sc = S().timeOfDay->SunColor(), am = S().timeOfDay->AmbientColor();
        rect(30, 460, 40, 40, Color(static_cast<int>(sc.X * 255), static_cast<int>(sc.Y * 255), static_cast<int>(sc.Z * 255), 255));
        text("SunColor()", 80, 468, Kit::kText, 0.6f);
        rect(300, 460, 40, 40, Color(static_cast<int>(am.X * 255), static_cast<int>(am.Y * 255), static_cast<int>(am.Z * 255), 255));
        text("AmbientColor()", 350, 468, Kit::kText, 0.6f);
    }
    void DrawDeck(const DrawContext& ctx)
    {
        const Vector3 c = R().centre;
        // Compass rose on the floor: N is -Z of the museum.
        const Vector3 o = c + Vector3(0, 0.03f, 0);
        gizmos_.Circle(o, Vector3::Up, 2.4f, Color(200, 190, 150, 255));
        gizmos_.Circle(o, Vector3::Up, 2.0f, Color(200, 190, 150, 255));
        gizmos_.Arrow(o, o + Vector3(0, 0, -2.4f), Color(255, 120, 100, 255));
        gizmos_.Line(o, o + Vector3(0, 0, 2.0f), Color(180, 180, 200, 255));
        gizmos_.Line(o - Vector3(2.0f, 0, 0), o + Vector3(2.0f, 0, 0), Color(180, 180, 200, 255));
        gizmos_.Label(o + Vector3(0, 0.1f, -2.7f), "N", Color(255, 150, 130, 255), 0.7f);
        gizmos_.Label(o + Vector3(0, 0.1f, 2.7f), "S", Color(220, 220, 230, 255), 0.7f);
        gizmos_.Label(o + Vector3(2.7f, 0.1f, 0), "E", Color(220, 220, 230, 255), 0.7f);
        gizmos_.Label(o + Vector3(-2.7f, 0.1f, 0), "W", Color(220, 220, 230, 255), 0.7f);
        // Sun and moon directions as arrows from the room centre.
        const Vector3 sun = S().timeOfDay->SunDirection();
        gizmos_.Arrow(c + Vector3(0, 1.4f, 0), c + Vector3(0, 1.4f, 0) + sun * 2.2f, Color(255, 220, 90, 255));
        gizmos_.Label(c + Vector3(0, 1.4f, 0) + sun * 2.5f, sun.Y > 0 ? "sun" : "sun (below the horizon)", Color(255, 230, 140, 255), 0.55f);
        gizmos_.Flush(ctx);
        DrawLamps(ctx);
    }

    void AddLampPosts(RoomBuilder& b)
    {
        const float x = R().halfW - 2.3f, z = R().halfD - 2.3f;
        for (int i = 0; i < 4; ++i) {
            const float lx = (i % 2 == 0 ? -1.0f : 1.0f) * x;
            const float lz = (i < 2 ? -1.0f : 1.0f) * z;
            b.Meshes().Opaque("metal").AddCylinder(b.World(lx, 0.0f, lz), 0.07f, 2.8f, 8, 1.0f,
                                                    Vector3(0.25f, 0.27f, 0.3f), true);
            b.AddCollider(BoundingBox(b.World(lx - 0.1f, 0, lz - 0.1f), b.World(lx + 0.1f, 2.8f, lz + 0.1f)));
            lamps_.push_back(R().ToWorld(lx, 2.95f, lz));
        }
    }

    void DrawLamps(const DrawContext& ctx)
    {
        // A glowing head that comes on after dusk.
        const bool night = S().timeOfDay->Daylight() < 0.35f;
        RenderStates::SetOpaque3D(ctx.device);
        for (const Vector3& p : lamps_) {
            BasicEffect& fx = ctx.fx.BeginLit(nullptr);
            fx.setDiffuseColorProperty(night ? Vector3(1.0f, 0.9f, 0.6f) : Vector3(0.35f, 0.35f, 0.38f));
            fx.setEmissiveColorProperty(night ? Vector3(1.0f, 0.85f, 0.5f) : Vector3(0, 0, 0));
            fx.setWorldProperty(Matrix::CreateScale(0.32f) * Matrix::CreateTranslation(p));
            fx.setViewProperty(ctx.view.view);
            fx.setProjectionProperty(ctx.view.projection);
            EffectBank::Apply(fx);
            S().shapes->Sphere().Draw(ctx.device);
            fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        }
    }

    void DrawSculptures(const DrawContext& ctx)
    {
        if (sculptureBases_.size() != 3) return;
        static const Vector3 palettes[3][3] = {
            {{0.55f, 0.32f, 0.12f}, {0.75f, 0.48f, 0.18f}, {0.42f, 0.24f, 0.10f}},
            {{0.86f, 0.86f, 0.82f}, {0.70f, 0.74f, 0.78f}, {0.94f, 0.91f, 0.84f}},
            {{0.88f, 0.22f, 0.16f}, {0.18f, 0.48f, 0.88f}, {0.95f, 0.72f, 0.12f}},
        };
        const Vector3* colours = palettes[sculptureFinish_];
        const Matrix roomFrame = Matrix::CreateRotationY(R().frameYaw);
        const auto draw = [&](const LitMesh& mesh, const Matrix& world, const Vector3& colour) {
            BasicEffect& fx = ctx.fx.BeginLit(S().textures->White());
            fx.setWorldProperty(world);
            fx.setViewProperty(ctx.view.view);
            fx.setProjectionProperty(ctx.view.projection);
            fx.setDiffuseColorProperty(colour);
            fx.setAmbientLightColorProperty(Vector3(0.42f, 0.42f, 0.46f));
            fx.setEmissiveColorProperty(colour * 0.12f);
            EffectBank::Apply(fx);
            mesh.Draw(ctx.device);
        };
        RenderStates::SetOpaque3D(ctx.device);

        const float spin = time_;
        for (int level = 0; level < 4; ++level) {
            const float y = 0.32f + static_cast<float>(level) * 0.55f;
            draw(S().shapes->Cube(), Matrix::CreateScale(1.25f, 0.32f, 0.42f) *
                                             Matrix::CreateRotationY(spin + static_cast<float>(level) * 0.72f) * roomFrame *
                                             Matrix::CreateTranslation(sculptureBases_[0] + Vector3(0, y, 0)), colours[0]);
        }
        draw(S().shapes->Sphere(), Matrix::CreateScale(1.75f, 0.55f, 1.75f) * Matrix::CreateRotationY(-spin * 0.7f) * roomFrame *
                                         Matrix::CreateTranslation(sculptureBases_[1] + Vector3(0, 1.15f, 0)), colours[1]);
        draw(S().shapes->Sphere(), Matrix::CreateScale(0.65f) *
                                         Matrix::CreateTranslation(sculptureBases_[1] + R().Rotate(Vector3(std::cos(spin) * 1.25f, 2.05f,
                                                                                                        std::sin(spin) * 1.25f))), colours[2]);
        draw(S().shapes->Cone(), Matrix::CreateScale(1.25f, 2.55f, 1.25f) * Matrix::CreateRotationY(spin * 0.45f) * roomFrame *
                                       Matrix::CreateTranslation(sculptureBases_[2]), colours[2]);
        draw(S().shapes->Sphere(), Matrix::CreateScale(0.68f) * Matrix::CreateTranslation(sculptureBases_[2] + Vector3(0, 2.55f, 0)), colours[0]);
    }

    // --- garden -------------------------------------------------------------------------------------
    void StepGarden(float dt)
    {
        if (!water_ || !leaves_) return;
        waterEmit_ += 260.0f * dt;
        const int n = static_cast<int>(waterEmit_);
        waterEmit_ -= static_cast<float>(n);
        for (int i = 0; i < n; ++i) {
            Particle q;
            q.position = R().ToWorld(0, 0.9f, 0) + Vector3(Rand(-0.1f, 0.1f), 0, Rand(-0.1f, 0.1f));
            q.velocity = Vector3(Rand(-0.5f, 0.5f), Rand(4.2f, 5.2f) * jet_, Rand(-0.5f, 0.5f));
            q.life = 2.4f;
            q.size0 = 0.1f;
            q.size1 = 0.06f;
            q.colour0 = Color(150, 200, 255, 230);
            q.colour1 = Color(200, 230, 255, 100);
            water_->Emit(q);
        }
        leafEmit_ += leafRate_ * dt;
        const int m = static_cast<int>(leafEmit_);
        leafEmit_ -= static_cast<float>(m);
        for (int i = 0; i < m; ++i) {
            Particle q;
            q.position = R().ToWorld(Rand(-5.5f, 5.5f), Rand(3.2f, 4.2f), Rand(-5.5f, 5.5f));
            q.velocity = Vector3(Rand(-0.2f, 0.2f), -Rand(0.3f, 0.7f), Rand(-0.2f, 0.2f));
            q.life = 6.0f;
            q.size0 = 0.14f;
            q.size1 = 0.14f;
            q.rotation = Rand(0, 6.28f);
            q.spin = Rand(-2.0f, 2.0f);
            q.colour0 = Color(120 + static_cast<int>(Rand(0, 100)), 150, 50, 255);
            q.colour1 = Color(180, 130, 40, 0);
            leaves_->Emit(q);
        }
        for (Particle& q : leaves_->MutableParticles()) {
            q.velocity.X += (wind_ - q.velocity.X) * 0.8f * dt;
            q.velocity.Z += std::sin(time_ * 1.7f + q.rotation) * 0.6f * dt;
        }
        ParticleSystem::Physics water;
        water.gravity = Vector3(0, -9.8f, 0);
        water.hasFloor = true;
        water.floorY = R().centre.Y + 0.5f;
        water_->Update(dt, water);
        ParticleSystem::Physics leaf;
        leaf.gravity = Vector3(0, -0.2f, 0);
        leaf.drag = 0.4f;
        leaf.hasFloor = true;
        leaf.floorY = R().centre.Y + 0.02f;
        leaves_->Update(dt, leaf);
    }
    void DrawGarden(const DrawContext&) {}

    // --- telescope ----------------------------------------------------------------------------------
    void BuildSkyline()
    {
        std::mt19937 g(77);
        std::uniform_real_distribution<float> u(0.0f, 1.0f);
        for (int i = 0; i < 90; ++i) {
            const float a = u(g) * MathHelper::TwoPi, r = 900.0f + u(g) * 1400.0f;
            Tower t;
            t.pos = Vector3(std::cos(a) * r, 0.0f, std::sin(a) * r);
            t.width = 40.0f + u(g) * 90.0f;
            t.height = 60.0f + u(g) * 380.0f * (i % 5 == 0 ? 1.8f : 1.0f);
            t.shade = 0.35f + u(g) * 0.25f;
            towers_.push_back(t);
        }
    }
    void DrawTelescopeScene(const DrawContext& ctx)
    {
        GraphicsDevice& d = ctx.device;
        if (S().sky != nullptr) S().sky->Draw(ctx);
        RenderStates::SetOpaque3D(d);
        const Vector3 sky = S().timeOfDay->AmbientColor();
        lit_->setFogColorProperty(Vector3(std::min(1.0f, sky.X * 1.5f), std::min(1.0f, sky.Y * 1.5f), std::min(1.0f, sky.Z * 1.6f)));
        lit_->setViewProperty(ctx.view.view);
        lit_->setProjectionProperty(ctx.view.projection);
        const float day = S().timeOfDay->Daylight();
        for (const Tower& t : towers_) {
            lit_->setDiffuseColorProperty(Vector3(t.shade, t.shade, t.shade + 0.05f) * (0.35f + 0.65f * day));
            lit_->setWorldProperty(Matrix::CreateScale(t.width, t.height, t.width) * Matrix::CreateTranslation(t.pos + Vector3(0, -420.0f + t.height * 0.5f, 0)));
            EffectBank::Apply(*lit_);
            S().shapes->Cube().Draw(d);
        }
        // Ground far below.
        lit_->setDiffuseColorProperty(Vector3(0.18f, 0.25f, 0.18f) * (0.35f + 0.65f * day));
        lit_->setWorldProperty(Matrix::CreateScale(8000.0f, 1.0f, 8000.0f) * Matrix::CreateTranslation(0, -420.0f, 0));
        EffectBank::Apply(*lit_);
        S().shapes->Plane().Draw(d);
    }
    void DrawTelescope(const DrawContext& ctx)
    {
        // The instrument itself: a tube on the pedestal pointing the way the lens looks.
        const float h = MathHelper::ToRadians(heading_), e = MathHelper::ToRadians(elevation_);
        const Vector3 dir(std::sin(h) * std::cos(e), std::sin(e), -std::cos(h) * std::cos(e));
        gizmos_.Arrow(telescopeBase_, telescopeBase_ + dir * 1.4f, Color(255, 220, 120, 255));
        gizmos_.Flush(ctx);
    }

    // --- weather ------------------------------------------------------------------------------------
    void StepWeather(float dt)
    {
        if (!snow_ || !splashes_) return;
        const bool rain = weather_ == 1 || weather_ == 3, snow = weather_ == 2;
        const int target = rain ? static_cast<int>(static_cast<float>(kMaxDrops) * intensity_ * (weather_ == 3 ? 1.0f : 0.6f)) : 0;
        if (static_cast<int>(drops_.size()) > target) drops_.resize(static_cast<std::size_t>(target));
        const Vector3 c = R().centre;
        while (static_cast<int>(drops_.size()) < target) drops_.push_back({c + Vector3(Rand(-6.5f, 6.5f), Rand(0.0f, 8.0f), Rand(-6.5f, 6.5f)), Rand(14.0f, 20.0f)});
        for (Drop& dr : drops_) {
            dr.pos += Vector3(wind_ * 0.5f, -dr.speed, 0) * dt;
            if (dr.pos.Y < c.Y) {
                if (splashes_->Size() < 1500 && Rand(0, 1) < 0.25f) {
                    Particle q;
                    q.position = Vector3(dr.pos.X, c.Y + 0.03f, dr.pos.Z);
                    q.velocity = Vector3(Rand(-0.5f, 0.5f), Rand(0.8f, 1.6f), Rand(-0.5f, 0.5f));
                    q.life = 0.35f;
                    q.size0 = 0.05f;
                    q.size1 = 0.02f;
                    q.colour0 = Color(200, 220, 255, 200);
                    q.colour1 = Color(200, 220, 255, 0);
                    splashes_->Emit(q);
                }
                dr.pos = Vector3(c.X + Rand(-6.5f, 6.5f), c.Y + 8.0f, c.Z + Rand(-6.5f, 6.5f));
            }
        }
        if (snow) {
            snowEmit_ += 260.0f * intensity_ * dt;
            const int n = static_cast<int>(snowEmit_);
            snowEmit_ -= static_cast<float>(n);
            for (int i = 0; i < n; ++i) {
                Particle q;
                q.position = c + Vector3(Rand(-6.5f, 6.5f), 7.5f, Rand(-6.5f, 6.5f));
                q.velocity = Vector3(wind_ * 0.35f + Rand(-0.2f, 0.2f), -Rand(0.7f, 1.3f), Rand(-0.2f, 0.2f));
                q.life = 9.0f;
                q.size0 = 0.07f;
                q.size1 = 0.07f;
                q.colour0 = Color(245, 248, 255, 255);
                q.colour1 = Color(245, 248, 255, 200);
                snow_->Emit(q);
            }
        }
        ParticleSystem::Physics snowPhysics;
        snowPhysics.drag = 0.1f;
        snowPhysics.hasFloor = true;
        snowPhysics.floorY = c.Y + 0.02f;
        snow_->Update(dt, snowPhysics);
        ParticleSystem::Physics splash;
        splash.gravity = Vector3(0, -9.8f, 0);
        splash.hasFloor = true;
        splash.floorY = c.Y;
        splashes_->Update(dt, splash);
        flash_ = std::max(0.0f, flash_ - dt * 2.5f);
        if (weather_ == 3) {
            nextLightning_ -= dt;
            if (nextLightning_ <= 0.0f) {
                flash_ = 1.0f;
                nextLightning_ = Rand(3.0f, 8.0f);
            }
        }
    }
    void DrawWeather(const DrawContext& ctx)
    {
        if (drops_.empty() || !streaks_) return;
        std::vector<VertexPositionColor> v;
        v.reserve(drops_.size() * 2);
        const Vector3 slant = Vector3(wind_ * 0.5f, -16.0f, 0) * 0.02f;
        for (const Drop& dr : drops_) {
            v.emplace_back(dr.pos, Color(200, 215, 255, 255));
            v.emplace_back(dr.pos + slant, Color(120, 140, 200, 255));
        }
        RenderStates::SetOpaque3D(ctx.device);
        ctx.device.setRasterizerStateProperty(RasterizerState::CullNone);
        flat_->setWorldProperty(Matrix::getIdentityProperty());
        flat_->setViewProperty(ctx.view.view);
        flat_->setProjectionProperty(ctx.view.projection);
        EffectBank::Apply(*flat_);
        streaks_->SetData(v.data(), 0, static_cast<int>(v.size()), SetDataOptions::Discard);
        ctx.device.SetVertexBuffer(streaks_.get());
        ctx.device.DrawPrimitives(PrimitiveType::LineList, 0, static_cast<int>(v.size()) / 2);
    }

    struct Tower {
        Vector3 pos;
        float width = 50.0f, height = 100.0f, shade = 0.4f;
    };
    struct Drop {
        Vector3 pos;
        float speed = 16.0f;
    };

    Mode mode_;
    std::mt19937 rng_;
    Gizmos gizmos_;
    float time_ = 0.0f;
    std::vector<Vector3> lamps_;
    // sculpture garden
    std::vector<Vector3> sculptureBases_;
    float sculptureSpeed_ = 0.55f;
    int sculptureFinish_ = 0;
    bool sculpturesRotate_ = true;
    // garden
    std::unique_ptr<ParticleSystem> water_, leaves_, snow_, splashes_;
    std::unique_ptr<ParticleRenderer> renderer_;
    Voice3D* waterVoice_ = nullptr;
    float wind_ = 2.5f, leafRate_ = 8.0f, jet_ = 1.0f, waterEmit_ = 0.0f, leafEmit_ = 0.0f;
    // telescope
    std::unique_ptr<MiniView> view_;
    std::unique_ptr<BasicEffect> lit_;
    std::vector<Tower> towers_;
    Vector3 telescopeBase_;
    float heading_ = 90.0f, elevation_ = 20.0f, fov_ = 25.0f;
    // weather
    int weather_ = 0;
    float intensity_ = 0.6f, flash_ = 0.0f, nextLightning_ = 2.0f, snowEmit_ = 0.0f;
    std::vector<Drop> drops_;
    std::unique_ptr<DynamicVertexBuffer> streaks_;
    std::unique_ptr<BasicEffect> flat_;
};

}  // namespace

std::unique_ptr<Exhibit> MakeRoofLab(const RoomDef& room, Services& services)
{
    return std::make_unique<RoofLab>(room, services);
}
