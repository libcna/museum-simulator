#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/ClearOptions.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SignFactory.hpp"
#include "Rendering/SkyDome.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/MuseumWorld.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const Color kGold(255, 226, 150, 255);
const Color kText(222, 228, 240, 255);

enum Feed { Security = 0, Map = 1, Chase = 2 };

struct MonitorSpec {
    Feed feed;
    int width, height;
    float along;          // west wall: local z of the centre; north wall: local x
    bool onNorthWall;
    float w, h;           // physical size in metres
    float centreY;
};
const MonitorSpec kMonitors[3] = {
    {Security, 640, 360, 2.9f, false, 3.6f, 2.025f, 2.45f},
    {Map, 432, 576, -2.3f, false, 1.95f, 2.6f, 2.45f},
    {Chase, 480, 270, -4.2f, true, 2.6f, 1.4625f, 2.55f},
};

// Cameras only film places that are always built while the visitor is here: this room, the gallery
// outside its door, and the next room along the wing.
enum class Where { Self, Gallery, Neighbour };
constexpr int Gallery = 1;   // index into kCams of the gallery camera
struct SecurityCam {
    const char* name;
    Where where;
    Vector3 eye;       // local to the filmed cell; for the gallery, x is relative to this room's door
    Vector3 target;    // local centre of attention
    float sweep;       // metres the target sweeps from side to side
};
const SecurityCam kCams[3] = {
    {"CAM 1  THIS ROOM", Where::Self, {6.4f, 4.2f, -5.3f}, {0.0f, 0.9f, 1.5f}, 5.5f},
    {"CAM 2  GALLERY", Where::Gallery, {0.0f, 4.2f, 0.0f}, {0.0f, 1.0f, 0.0f}, 3.5f},
    {"CAM 3  NEXT ROOM", Where::Neighbour, {-5.5f, 3.9f, 5.5f}, {0.0f, 1.0f, 0.0f}, 3.0f},
};

class RenderTargetRoom final : public Exhibit {
public:
    RenderTargetRoom(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    [[nodiscard]] Vector3 MonitorCentre(const MonitorSpec& m, Vector3& right, Vector3& normal) const
    {
        if (m.onNorthWall) {
            normal = Off(0, 0, 1);
            right = Off(1, 0, 0);
            return R().ToWorld(m.along, m.centreY, -R().halfD + 0.06f);
        }
        normal = Off(1, 0, 0);
        right = Off(0, 0, -1);   // looking east at the west wall, "right" is -Z... see below
        return R().ToWorld(-R().halfW + 0.06f, m.centreY, m.along);
    }

    void BuildStatic(RoomBuilder& b) override
    {
        for (int i = 0; i < 3; ++i) {
            const MonitorSpec& m = kMonitors[i];
            // Bezel: a dark slab behind the screen quad.
            const float pad = 0.12f;
            if (m.onNorthWall)
                b.AddBox(Vector3(m.along - m.w / 2 - pad, m.centreY - m.h / 2 - pad, -R().halfD + 0.01f),
                         Vector3(m.along + m.w / 2 + pad, m.centreY + m.h / 2 + pad, -R().halfD + 0.05f), "metal", 1.0f,
                         Vector3(0.18f, 0.18f, 0.22f), false);
            else
                b.AddBox(Vector3(-R().halfW + 0.01f, m.centreY - m.h / 2 - pad, m.along - m.w / 2 - pad),
                         Vector3(-R().halfW + 0.05f, m.centreY + m.h / 2 + pad, m.along + m.w / 2 + pad), "metal", 1.0f,
                         Vector3(0.18f, 0.18f, 0.22f), false);
        }
        // Control desk facing the monitors.
        b.AddBox(Vector3(-4.6f, 0.0f, -1.0f), Vector3(-3.9f, 0.95f, 1.4f), "metal", 1.0f, Vector3(0.45f, 0.5f, 0.62f));
        SignSpec desk;
        desk.lines = {{"Security desk", true, kGold}, {"Press E on a monitor", false, kText}};
        desk.background = Color(18, 20, 30, 255);
        b.AddQuadFacing(Vector3(-3.88f, 0.55f, 0.2f), MathHelper::PiOver2, 1.6f, 0.42f, S().signs->Create(Plate(desk, 1.6f, 0.42f)), Vector3(1, 1, 1), 0.7f);

        SignSpec title;
        title.lines = {{"Render targets", true, kGold},
                       {"RenderTarget2D: the museum is drawn into textures, and the textures are shown", false, kText},
                       {"on screens. Camera 1 is a ceiling camera in the lobby; look for yourself on it.", false, kText}};
        b.AddWallSign(Wall::South, 0.0f, 3.1f, 6.0f, 1.45f, title);
        SignSpec cams;
        cams.lines = {{"Three feeds", true, kGold},
                      {"1  switchable fixed cameras    2  overhead map    3  chase camera", false, kText}};
        b.AddWallSign(Wall::East, -3.5f, 3.4f, 4.6f, 0.95f, cams);
    }

    void Load(InteractionSystem& interaction, MuseumWorld& world) override
    {
        world_ = &world;
        for (int i = 0; i < 3; ++i) {
            const MonitorSpec& m = kMonitors[i];
            targets_[static_cast<std::size_t>(i)] = std::make_unique<RenderTarget2D>(
                *S().device, m.width, m.height, false, SurfaceFormat::Color, DepthFormat::Depth24, 0, RenderTargetUsage::PreserveContents);

            Vector3 right, normal;
            const Vector3 centre = MonitorCentre(m, right, normal);
            Interactable ia;
            ia.bounds = BoundingBox(centre - Off(m.onNorthWall ? m.w / 2 : 0.5f, m.h / 2, m.onNorthWall ? 0.5f : m.w / 2),
                                    centre + Off(m.onNorthWall ? m.w / 2 : 0.5f, m.h / 2, m.onNorthWall ? 0.5f : m.w / 2));
            ia.maxDistance = 9.0f;
            ia.prompt = [this, i] { return Prompt(i); };
            ia.activate = [this, i] { Press(i); };
            interaction.Add(std::move(ia));
        }
        Interactable freeze;
        freeze.bounds = BoundingBox(R().ToWorld(-4.75f, 0.0f, -1.1f), R().ToWorld(-3.8f, 1.2f, 1.5f));
        freeze.prompt = [this] { return std::string(frozen_ ? "Resume the live feeds" : "Freeze the feeds (the render targets keep their last frame)"); };
        freeze.activate = [this] { frozen_ = !frozen_; };
        interaction.Add(std::move(freeze));
    }

    void Update(const UpdateContext& ctx) override
    {
        time_ = static_cast<float>(ctx.time);
        eye_ = ctx.playerPosition;
        forward_ = ctx.playerForward;
        near_ = ctx.nearby && Vector3::Distance(Vector3(ctx.playerPosition.X, 0, ctx.playerPosition.Z),
                                                 Vector3(R().Center().X, 0, R().Center().Z)) < 18.0f;
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (ctx.offscreen || world_ == nullptr || !near_ || frozen_ || targets_[0] == nullptr) return;
        ++frame_;
        for (int i = 0; i < 3; ++i) {
            // Each feed refreshes every third frame, and only while its monitor is in the visitor's view.
            const bool due = (frame_ % 3) == i || !rendered_[static_cast<std::size_t>(i)];
            if (!due) continue;
            Vector3 right, normal;
            const MonitorSpec& m = kMonitors[i];
            const Vector3 centre = MonitorCentre(m, right, normal);
            const BoundingBox box(centre - Off(1.9f, 1.4f, 1.9f), centre + Off(1.9f, 1.4f, 1.9f));
            if (rendered_[static_cast<std::size_t>(i)] && !ctx.view.frustum.Intersects(box)) continue;
            RenderFeed(ctx, i);
            rendered_[static_cast<std::size_t>(i)] = true;
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        GraphicsDevice& device = ctx.device;
        RenderStates::SetOpaque3D(device);
        if (ctx.offscreen) DrawAvatar(ctx);
        DrawSecurityCamera(ctx);
        if (ctx.offscreen) return;

        device.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
        for (int i = 0; i < 3; ++i) {
            if (!rendered_[static_cast<std::size_t>(i)]) continue;
            const MonitorSpec& m = kMonitors[i];
            Vector3 right, normal;
            const Vector3 centre = MonitorCentre(m, right, normal);
            Vector3 u, v(0, m.h, 0);
            Vector3 origin;
            if (m.onNorthWall) {
                u = Off(m.w, 0, 0);
                origin = centre - Off(m.w / 2, m.h / 2, 0);
            } else {
                // The west wall faces +X; a visitor looking at it has +Z on their left, so the quad runs towards -Z... flipped here:
                u = Off(0, 0, -m.w);
                origin = centre - Off(0, m.h / 2, -m.w / 2);
            }
            QuadDraw::Textured(ctx, targets_[static_cast<std::size_t>(i)].get(), origin, u, v);
        }
    }

private:
    static SignSpec Plate(SignSpec spec, float w, float h)
    {
        spec.pixelWidth = static_cast<int>(w * 260.0f);
        spec.pixelHeight = static_cast<int>(h * 260.0f);
        return spec;
    }

    [[nodiscard]] std::string Prompt(int i) const
    {
        switch (i) {
            case Security: return std::string("Switch camera (now: ") + kCams[cam_].name + ")";
            case Map: return std::string(orthographic_ ? "Map: switch to a perspective camera" : "Map: switch to an orthographic camera");
            default: return std::string("Chase camera: ") + (chaseHigh_ ? "high" : "low") + "  (switch)";
        }
    }

    void Press(int i)
    {
        if (i == Security) cam_ = (cam_ + 1) % 3;
        else if (i == Map) orthographic_ = !orthographic_;
        else chaseHigh_ = !chaseHigh_;
        rendered_ = {false, false, false};   // refresh immediately even while the visitor looks elsewhere
        rendered_[static_cast<std::size_t>(i)] = false;
    }

    [[nodiscard]] const RoomDef& CamRoom(const SecurityCam& c) const
    {
        const MuseumPlan& m = *S().museum;
        if (c.where == Where::Self || R().wing < 0) return R();
        const WingDef& wing = m.wings[static_cast<std::size_t>(R().wing)];
        if (c.where == Where::Gallery) return m.Room(wing.gallery);
        for (std::size_t i = 0; i < wing.rooms.size(); ++i)
            if (wing.rooms[i] == R().index) return m.Room(wing.rooms[(i + 1) % wing.rooms.size()]);
        return R();
    }

    // Eye and target (world space) of the security camera for feed `cam_`.
    void SecurityPose(Vector3& eye, Vector3& target) const
    {
        const SecurityCam& c = kCams[cam_];
        const RoomDef& room = CamRoom(c);
        float ex = c.eye.X;
        if (c.where == Where::Gallery) ex += R().centre.X - room.centre.X;   // above this room's door
        eye = room.ToWorld(ex, c.eye.Y, c.eye.Z);
        const float sweep = std::sin(time_ * 0.35f) * c.sweep;
        const Vector3 toCentre = Vector3(-ex, 0, -c.eye.Z);
        Vector3 side = Vector3::Cross(toCentre, Vector3::Up);
        if (side.LengthSquared() > 1e-4f) side = Vector3::Normalize(side);
        target = room.ToWorld(c.target.X + (c.where == Where::Gallery ? ex : 0.0f), c.target.Y, c.target.Z) + side * sweep;
    }

    void RenderFeed(const DrawContext& main, int index)
    {
        GraphicsDevice& device = main.device;
        RenderTarget2D& rt = *targets_[static_cast<std::size_t>(index)];
        const MonitorSpec& m = kMonitors[index];
        const float aspect = static_cast<float>(m.width) / static_cast<float>(m.height);

        Matrix view, projection;
        Vector3 eye;
        const char* label = "";
        char footer[96];
        footer[0] = '\0';
        if (index == Security) {
            Vector3 target;
            SecurityPose(eye, target);
            view = Matrix::CreateLookAt(eye, target, Vector3::Up);
            projection = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(62.0f), aspect, 0.2f, 140.0f);
            label = kCams[cam_].name;
        } else if (index == Map) {
            const RoomDef& gallery = CamRoom(kCams[Gallery]);
            const Vector3 centre = gallery.centre;
            const float span = std::max(46.0f, gallery.halfW * 2.0f + 12.0f);
            eye = centre + (orthographic_ ? Vector3(0, 70, 0) : Vector3(0, 70, span * 0.35f));
            view = Matrix::CreateLookAt(eye, centre, orthographic_ ? Vector3(0, 0, -1) : Vector3::Up);
            projection = orthographic_ ? Matrix::CreateOrthographic(span, span / aspect, 1.0f, 200.0f)
                                       : Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(50.0f), aspect, 1.0f, 220.0f);
            label = orthographic_ ? "MAP  ORTHOGRAPHIC" : "MAP  PERSPECTIVE";
        } else {
            const Vector3 flat = Vector3::Normalize(Vector3(forward_.X, 0.0f, forward_.Z) + Vector3(1e-4f, 0, 0));
            eye = eye_ - flat * (chaseHigh_ ? 3.2f : 2.6f) + Vector3(0, chaseHigh_ ? 2.3f : 0.6f, 0);
            view = Matrix::CreateLookAt(eye, eye_ + flat * 2.0f + Vector3(0, -1.0f, 0), Vector3::Up);
            projection = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(70.0f), aspect, 0.15f, 120.0f);
            label = "CAM 4  CHASE";
        }
        std::snprintf(footer, sizeof(footer), "REC %02d:%02d", static_cast<int>(time_) / 60 % 60, static_cast<int>(time_) % 60);

        const SceneView sceneView(view, projection, eye);
        DrawContext sub{device, main.fx, sceneView, main.time, true};
        main.fx.SetCamera(view, projection);

        device.SetRenderTarget(&rt);
        device.Clear(Color(index == Map ? 18 : 90, index == Map ? 22 : 120, index == Map ? 34 : 170, 255), 1.0f);
        RenderStates::SetOpaque3D(device);
        if (index != Map && S().sky != nullptr) S().sky->Draw(sub);
        world_->Draw(sub);

        // On-screen display, drawn with SpriteBatch into the same target.
        if (S().font != nullptr) {
            const SamplerState sampler = SamplerState::LinearClamp;
            S().sprites->Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &sampler, nullptr, nullptr);
            Texture2D& white = *S().textures->White();
            S().sprites->Draw(white, Rectangle(0, 0, m.width, 30), Color(0, 0, 0, 150));
            S().sprites->DrawString(*S().font, label, Vector2(10, 3), Color::White, 0.0f, Vector2(0, 0), 0.85f, SpriteEffects::None, 0.0f);
            const bool blink = (static_cast<int>(time_ * 2.0f) % 2) == 0;
            if (blink) S().sprites->Draw(white, Rectangle(m.width - 120, 9, 12, 12), Color(240, 40, 40, 255));
            S().sprites->DrawString(*S().font, footer, Vector2(static_cast<float>(m.width - 100), 3), Color::White, 0.0f, Vector2(0, 0), 0.85f,
                                    SpriteEffects::None, 0.0f);
            S().sprites->End();
        }
        device.SetRenderTarget(nullptr);
        main.fx.SetCamera(main.view.view, main.view.projection);
    }

    // The visitor as seen by the cameras: a capsule-ish figure with a nose showing where they look.
    void DrawAvatar(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setSpecularColorProperty(Vector3(0.2f, 0.2f, 0.2f));
        fx.setSpecularPowerProperty(16.0f);
        fx.setEmissiveColorProperty(Vector3(0.05f, 0.1f, 0.1f));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        const float yaw = std::atan2(-forward_.X, -forward_.Z);
        const Vector3 feet(eye_.X, 0.0f, eye_.Z);
        const Matrix turn = Matrix::CreateRotationY(yaw);
        fx.setDiffuseColorProperty(Vector3(0.2f, 0.75f, 0.8f));
        fx.setWorldProperty(Matrix::CreateScale(0.5f, 1.3f, 0.5f) * Matrix::CreateTranslation(feet + Vector3(0, 0.05f, 0)));
        EffectBank::Apply(fx);
        S().shapes->Cylinder().Draw(device);
        fx.setDiffuseColorProperty(Vector3(0.95f, 0.8f, 0.65f));
        fx.setWorldProperty(Matrix::CreateScale(0.3f) * Matrix::CreateTranslation(feet + Vector3(0, 1.5f, 0)));
        EffectBank::Apply(fx);
        S().shapes->Sphere().Draw(device);
        fx.setDiffuseColorProperty(Vector3(1.0f, 0.4f, 0.3f));
        // The cone points along +Y; tip it onto the forward (-Z) axis, then turn with the visitor.
        fx.setWorldProperty(Matrix::CreateScale(0.12f, 0.25f, 0.12f) * Matrix::CreateRotationX(-MathHelper::PiOver2) * turn *
                            Matrix::CreateTranslation(feet + Vector3(0, 1.5f, 0) + Vector3(-std::sin(yaw), 0, -std::cos(yaw)) * 0.14f));
        EffectBank::Apply(fx);
        S().shapes->Cone().Draw(device);
    }

    // The ceiling camera whose picture appears on monitor 1; drawn wherever the active feed lives.
    void DrawSecurityCamera(const DrawContext& ctx)
    {
        Vector3 eye, target;
        SecurityPose(eye, target);
        const Vector3 forward = Vector3::Normalize(target - eye);
        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setSpecularColorProperty(Vector3(0.6f, 0.6f, 0.6f));
        fx.setSpecularPowerProperty(30.0f);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        const Matrix orient = Matrix::CreateWorld(eye, forward, Vector3::Up);
        fx.setDiffuseColorProperty(Vector3(0.3f, 0.3f, 0.34f));
        fx.setWorldProperty(Matrix::CreateScale(0.3f, 0.26f, 0.55f) * orient);
        EffectBank::Apply(fx);
        S().shapes->Cube().Draw(ctx.device);
        fx.setDiffuseColorProperty(Vector3(0.05f, 0.08f, 0.12f));
        fx.setWorldProperty(Matrix::CreateScale(0.17f, 0.12f, 0.17f) * Matrix::CreateRotationX(-MathHelper::PiOver2) *
                            Matrix::CreateTranslation(0, 0, -0.27f) * orient);
        EffectBank::Apply(fx);
        S().shapes->Cylinder().Draw(ctx.device);
        // Mount arm up to the ceiling.
        fx.setDiffuseColorProperty(Vector3(0.2f, 0.2f, 0.22f));
        const float ceiling = CamRoom(kCams[cam_]).centre.Y + CamRoom(kCams[cam_]).height - R().centre.Y;
        fx.setWorldProperty(Matrix::CreateScale(0.08f, std::max(0.1f, ceiling - eye.Y), 0.08f) * Matrix::CreateTranslation(eye));
        EffectBank::Apply(fx);
        S().shapes->Cylinder().Draw(ctx.device);
    }

    std::array<std::unique_ptr<RenderTarget2D>, 3> targets_;
    std::array<bool, 3> rendered_{false, false, false};
    MuseumWorld* world_ = nullptr;
    int cam_ = 0;
    long long frame_ = 0;
    bool orthographic_ = true;
    bool chaseHigh_ = true;
    bool frozen_ = false;
    bool near_ = false;
    float time_ = 0.0f;
    Vector3 eye_{0, 1.65f, 0};
    Vector3 forward_{0, 0, -1};
};

}  // namespace

std::unique_ptr<Exhibit> MakeRenderTargetRoom(const RoomDef& room, Services& services)
{
    return std::make_unique<RenderTargetRoom>(room, services);
}
