// Matrix laboratory: transform order, parent/child hierarchies, view and projection matrices and
// the world -> screen pipeline. Every number shown is read from a CNA Matrix.
#include <algorithm>
#include <cmath>
#include <string>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/MiniView.hpp"
#include "Rendering/RenderStates.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Order, Hierarchy, LookAt, Perspective, Ortho, Project };

Mode ParseMode(const std::string& s)
{
    if (s == "hierarchy") return Mode::Hierarchy;
    if (s == "lookat") return Mode::LookAt;
    if (s == "perspective") return Mode::Perspective;
    if (s == "ortho") return Mode::Ortho;
    if (s == "project") return Mode::Project;
    return Mode::Order;
}

std::string Row(const Matrix& m, int r)
{
    const float v[4][4] = {{m.M11, m.M12, m.M13, m.M14}, {m.M21, m.M22, m.M23, m.M24}, {m.M31, m.M32, m.M33, m.M34}, {m.M41, m.M42, m.M43, m.M44}};
    char b[128];
    std::snprintf(b, sizeof b, "%6.2f %6.2f %6.2f %6.2f", static_cast<double>(v[r][0]), static_cast<double>(v[r][1]), static_cast<double>(v[r][2]),
                  static_cast<double>(v[r][3]));
    return b;
}

class TransformLab final : public Exhibit {
public:
    TransformLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        const bool dioramaMode = mode_ == Mode::LookAt || mode_ == Mode::Perspective || mode_ == Mode::Ortho;
        origin_ = dioramaMode ? R().ToWorld(0, 0, 0) : BuildStagePlinth(b, 0.0f, 0.0f) + Vector3(0, 0.55f, 0);
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Order:
                p.AddSlider("Scale", 0.4f, 1.8f, scale_, [this](float v) { scale_ = v; });
                p.AddSlider("Rotate about Y", -180.0f, 180.0f, rotY_, [this](float v) { rotY_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Translate X", -1.5f, 1.5f, moveX_, [this](float v) { moveX_ = v; });
                p.AddChoice("Multiplication order", {"Scale * Rotate * Translate", "Translate * Rotate * Scale"}, 0, [this](int v) { order_ = v; });
                p.AddHeading("The combined world matrix");
                for (int r = 0; r < 4; ++r) p.AddReadout("row " + std::to_string(r + 1), [this, r] { return Row(Combined(order_), r); });
                break;
            case Mode::Hierarchy:
                p.AddSlider("Parent spin speed", -2.0f, 2.0f, spin_, [this](float v) { spin_ = v; });
                p.AddSlider("Child offset", 0.5f, 1.8f, offset_, [this](float v) { offset_ = v; });
                p.AddSlider("Child tilt", -90.0f, 90.0f, tilt_, [this](float v) { tilt_ = v; }, "%.0f deg", 1.0f);
                p.AddToggle("Inherit parent transform", true, [this](bool v) { inherit_ = v; });
                p.AddHeading("Child world position (Matrix.Translation)");
                p.AddReadout("world matrix origin", [this] { return Fmt(ChildWorld().getTranslationProperty()); });
                p.AddReadout("Decompose scale", [this] {
                    Vector3 s, t;
                    Quaternion q;
                    (void)ChildWorld().Decompose(s, q, t);
                    return Fmt(s);
                });
                p.AddReadout("Decompose rotation", [this] {
                    Vector3 s, t;
                    Quaternion q;
                    (void)ChildWorld().Decompose(s, q, t);
                    char buf[96];
                    std::snprintf(buf, sizeof buf, "(%.2f %.2f %.2f %.2f)", static_cast<double>(q.X), static_cast<double>(q.Y), static_cast<double>(q.Z), static_cast<double>(q.W));
                    return std::string(buf);
                });
                break;
            case Mode::LookAt:
                p.AddSlider("Camera X", -5.0f, 5.0f, camera_.X, [this](float v) { camera_.X = v; });
                p.AddSlider("Camera Y", 0.6f, 5.0f, camera_.Y, [this](float v) { camera_.Y = v; });
                p.AddSlider("Camera Z", 1.5f, 8.0f, camera_.Z, [this](float v) { camera_.Z = v; });
                p.AddSlider("Target X", -2.5f, 2.5f, target_.X, [this](float v) { target_.X = v; });
                p.AddHeading("View matrix = CreateLookAt(eye, target, up)");
                for (int r = 0; r < 4; ++r) p.AddReadout("row " + std::to_string(r + 1), [this, r] { return Row(ViewMatrix(), r); });
                break;
            case Mode::Perspective:
                p.AddSlider("Field of view", 20.0f, 120.0f, fov_, [this](float v) { fov_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Near plane", 0.2f, 4.0f, near_, [this](float v) { near_ = std::min(v, far_ - 0.5f); }, "%.2f");
                p.AddSlider("Far plane", 3.0f, 14.0f, far_, [this](float v) { far_ = std::max(v, near_ + 0.5f); }, "%.1f");
                p.AddSlider("Aspect ratio", 0.6f, 2.4f, aspect_, [this](float v) { aspect_ = v; }, "%.2f");
                p.AddHeading("CreatePerspectiveFieldOfView");
                p.AddReadout("M11 (x scale)", [this] { return Fmt(ProjectionMatrix().M11); });
                p.AddReadout("M22 (y scale)", [this] { return Fmt(ProjectionMatrix().M22); });
                p.AddReadout("Objects inside the frustum", [this] { return std::to_string(InsideCount()) + " of 8"; });
                break;
            case Mode::Ortho:
                p.AddSlider("View width", 2.0f, 12.0f, orthoWidth_, [this](float v) { orthoWidth_ = v; }, "%.1f m");
                p.AddSlider("Near plane", 0.1f, 6.0f, near_, [this](float v) { near_ = std::min(v, far_ - 0.5f); }, "%.2f");
                p.AddSlider("Far plane", 3.0f, 16.0f, far_, [this](float v) { far_ = std::max(v, near_ + 0.5f); }, "%.1f");
                p.AddHeading("CreateOrthographic vs CreatePerspectiveFieldOfView");
                p.AddReadout("Ortho M11 = 2 / width", [this] { return Fmt(OrthoMatrix().M11); });
                p.AddReadout("Ortho M44 (no divide)", [this] { return Fmt(OrthoMatrix().M44); });
                p.AddReadout("Persp M34 (w = -z)", [this] { return Fmt(Matrix::CreatePerspectiveFieldOfView(1.0f, 1.0f, 0.5f, 20.0f).M34); });
                break;
            case Mode::Project:
                p.AddSlider("Point X", -2.0f, 2.0f, point_.X, [this](float v) { point_.X = v; });
                p.AddSlider("Point Y", 0.0f, 3.0f, point_.Y, [this](float v) { point_.Y = v; });
                p.AddSlider("Point Z", -2.0f, 2.0f, point_.Z, [this](float v) { point_.Z = v; });
                p.AddHeading("From world space to your screen");
                p.AddReadout("World", [this] { return Fmt(point_); });
                p.AddReadout("View space", [this] { return Fmt(Vector3::Transform(WorldPoint(), View())); });
                p.AddReadout("Clip x y w", [this] {
                    const Vector4 c = Vector4::Transform(Vector4(WorldPoint(), 1.0f), View() * Projection());
                    char buf[96];
                    std::snprintf(buf, sizeof buf, "(%.2f, %.2f, %.2f)", static_cast<double>(c.X), static_cast<double>(c.Y), static_cast<double>(c.W));
                    return std::string(buf);
                });
                p.AddReadout("Screen (Viewport.Project)", [this] { return Fmt(ScreenPoint()); });
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        if (mode_ == Mode::LookAt || mode_ == Mode::Perspective || mode_ == Mode::Ortho) {
            viewA_ = std::make_unique<MiniView>(S(), 480, 300);
            if (mode_ == Mode::Ortho) viewB_ = std::make_unique<MiniView>(S(), 480, 300);
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        angle_ += ctx.dt * spin_;
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        const Vector3 c = DioramaOrigin();
        const auto scene = [&](const DrawContext& sub) { DrawDiorama(sub, S(), c, time_, 3.0f); };
        if (mode_ == Mode::LookAt && viewA_) {
            viewA_->Render(ctx, ViewMatrix(), Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(60.0f), viewA_->Aspect(), 0.1f, 40.0f), CameraWorld(), scene);
        } else if (mode_ == Mode::Perspective && viewA_) {
            viewA_->Render(ctx, PerspectiveView(), ProjectionMatrix(), PerspectiveEye(), scene);
        } else if (mode_ == Mode::Ortho && viewA_ && viewB_) {
            const Vector3 eye = c + Off(0, 3.0f, 8.0f);
            const Matrix view = Matrix::CreateLookAt(eye, c + Vector3(0, 0.5f, 0), Vector3::Up);
            viewA_->Render(ctx, view, Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(50.0f), viewA_->Aspect(), near_, far_ + 6.0f), eye, scene);
            viewB_->Render(ctx, view, OrthoMatrix(), eye, scene);
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        const Vector3 o = origin_ - R().ToWorld(0, 0, 0) + R().ToWorld(0, 0, 0);
        gizmos_.Grid(Vector3(o.X, R().centre.Y + 0.03f, o.Z), 2.5f, 0.5f, Color(70, 80, 110, 255));
        switch (mode_) {
            case Mode::Order: DrawOrder(ctx, o); break;
            case Mode::Hierarchy: DrawHierarchy(ctx, o); break;
            case Mode::LookAt: DrawLookAt(ctx); break;
            case Mode::Perspective: DrawPerspective(ctx); break;
            case Mode::Ortho: DrawOrtho(ctx); break;
            case Mode::Project: DrawProject(ctx, o); break;
        }
        gizmos_.Flush(ctx);
    }

    void DrawOverlay() override
    {
        if (mode_ == Mode::Project && S().mainView != nullptr) {
            const Vector3 sp = ScreenPoint();
            const Viewport vp = S().device->getViewportProperty();
            if (sp.Z > 0.0f && sp.Z < 1.0f && sp.X > 0 && sp.Y > 0 && sp.X < static_cast<float>(vp.getWidthProperty()) && sp.Y < static_cast<float>(vp.getHeightProperty())) {
                const SamplerState sampler = SamplerState::LinearClamp;
                S().sprites->Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &sampler, nullptr, nullptr);
                const int x = static_cast<int>(sp.X), y = static_cast<int>(sp.Y);
                Texture2D& w = *S().whiteTexture;
                S().sprites->Draw(w, Rectangle(x - 18, y - 1, 36, 3), Color(255, 230, 90, 255));
                S().sprites->Draw(w, Rectangle(x - 1, y - 18, 3, 36), Color(255, 230, 90, 255));
                S().sprites->DrawString(*S().font, "Viewport.Project -> pixel " + std::to_string(x) + ", " + std::to_string(y), Vector2(static_cast<float>(x) + 22.0f, static_cast<float>(y) - 12.0f),
                                        Color(255, 240, 150, 255), 0.0f, Vector2(0, 0), 0.6f, SpriteEffects::None, 0.0f);
                S().sprites->End();
            }
        }
        gizmos_.DrawLabels();
    }

    void DrawTransparent(const DrawContext& ctx) override { (void)ctx; }

private:
    [[nodiscard]] std::string Title() const
    {
        switch (mode_) {
            case Mode::Order: return "Matrix order: S R T";
            case Mode::Hierarchy: return "Local and world matrices";
            case Mode::LookAt: return "View matrix (CreateLookAt)";
            case Mode::Perspective: return "Perspective projection";
            case Mode::Ortho: return "Orthographic projection";
            default: return "World to screen";
        }
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Order: return {"XNA matrices multiply left to right: Scale * Rotate * Translate scales the object, turns it", "about its own origin, then moves it. Reverse the order and the same numbers give a different place:", "the move itself gets scaled and rotated. The faint copy shows the other order."};
            case Mode::Hierarchy: return {"A child's world matrix is its local matrix times its parent's world matrix. Spin the parent and", "everything attached follows. Switch inheritance off to see the child ignore its parent.", "Matrix.Decompose recovers scale, rotation and translation from the product."};
            case Mode::LookAt: return {"CreateLookAt builds the view matrix that moves the world so the camera sits at the origin looking", "down -Z. The inset is what a camera at the red marker sees. Its rows are the camera's axes."};
            case Mode::Perspective: return {"CreatePerspectiveFieldOfView squeezes the view frustum into a cube. A wider field of view shows", "more but makes things smaller; the near and far planes clip what is too close or too far.", "BoundingFrustum counts the objects inside."};
            case Mode::Ortho: return {"An orthographic projection has no vanishing point: parallel lines stay parallel and size does not", "depend on distance (left view = perspective, right = orthographic of the same scene)."};
            default: return {"A point travels world -> view -> clip -> normalised device -> screen coordinates.", "Move the point; the cross-hair on your screen is where Viewport.Project puts it."};
        }
    }

    // --- shared maths -------------------------------------------------------------------------
    [[nodiscard]] Matrix Combined(int order) const
    {
        const Matrix S = Matrix::CreateScale(scale_);
        const Matrix Rm = Matrix::CreateRotationY(MathHelper::ToRadians(rotY_));
        const Matrix T = Matrix::CreateTranslation(moveX_, 0, 0);
        return order == 0 ? S * Rm * T : T * Rm * S;
    }
    [[nodiscard]] Matrix ChildWorld() const
    {
        const Matrix parent = Matrix::CreateRotationY(angle_) * Matrix::CreateTranslation(0, 0.3f, 0);
        const Matrix local = Matrix::CreateRotationZ(MathHelper::ToRadians(tilt_)) * Matrix::CreateTranslation(offset_, 0, 0);
        return inherit_ ? local * parent : local * Matrix::CreateTranslation(0, 0.3f, 0);
    }
    [[nodiscard]] Vector3 DioramaOrigin() const { return R().ToWorld(0.0f, 0.0f, 0.0f) + Vector3(0, 0.02f, 0) + (mode_ == Mode::Order ? Vector3() : Vector3(0, 0, 0)); }
    [[nodiscard]] Vector3 CameraWorld() const { return DioramaOrigin() + camera_; }
    [[nodiscard]] Matrix ViewMatrix() const { return Matrix::CreateLookAt(CameraWorld(), DioramaOrigin() + Vector3(target_.X, 0.5f, -1.0f), Vector3::Up); }
    [[nodiscard]] Matrix ProjectionMatrix() const { return Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(fov_), aspect_, near_, far_); }
    [[nodiscard]] Matrix OrthoMatrix() const { return Matrix::CreateOrthographic(orthoWidth_, orthoWidth_ / 1.6f, near_, far_ + 6.0f); }
    [[nodiscard]] Vector3 PerspectiveEye() const { return DioramaOrigin() + Vector3(0, 1.0f, 3.2f); }
    [[nodiscard]] Matrix PerspectiveView() const { return Matrix::CreateLookAt(PerspectiveEye(), DioramaOrigin() + Vector3(0, 0.6f, -4.0f), Vector3::Up); }
    [[nodiscard]] int InsideCount() const
    {
        const BoundingFrustum f(PerspectiveView() * ProjectionMatrix());
        int count = 0;
        const DioramaItem* items = DioramaItems(count);
        int inside = 0;
        for (int i = 0; i < count; ++i)
            if (f.Contains(BoundingSphere(DioramaOrigin() + items[i].offset, items[i].radius)) != ContainmentType::Disjoint) ++inside;
        return inside;
    }
    [[nodiscard]] Vector3 WorldPoint() const { return DioramaOrigin() + point_; }
    [[nodiscard]] Matrix View() const { return S().mainView != nullptr ? S().mainView->view : Matrix::getIdentityProperty(); }
    [[nodiscard]] Matrix Projection() const { return S().mainView != nullptr ? S().mainView->projection : Matrix::getIdentityProperty(); }
    [[nodiscard]] Vector3 ScreenPoint() const
    {
        if (S().mainView == nullptr) return Vector3(0, 0, 0);
        return S().device->getViewportProperty().Project(WorldPoint(), Projection(), View(), Matrix::getIdentityProperty());
    }

    // --- drawing --------------------------------------------------------------------------------
    void DrawBlock(const DrawContext& ctx, const Matrix& world, const Vector3& colour, float alpha)
    {
        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPunctualLightEXT(PunctualLightEXT{});
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setDiffuseColorProperty(colour);
        fx.setEmissiveColorProperty(colour * (alpha < 1.0f ? 0.35f : 0.1f));
        fx.setSpecularColorProperty(Vector3(0.2f, 0.2f, 0.2f));
        fx.setAlphaProperty(alpha);
        // A box with a nose: a small cube on the +X side shows which way the object points.
        fx.setWorldProperty(Matrix::CreateScale(0.7f, 0.5f, 0.45f) * world);
        EffectBank::Apply(fx);
        S().shapes->Cube().Draw(ctx.device);
        fx.setDiffuseColorProperty(Vector3(1.0f, 0.85f, 0.3f));
        fx.setWorldProperty(Matrix::CreateScale(0.22f) * Matrix::CreateTranslation(0.42f, 0, 0) * world);
        EffectBank::Apply(fx);
        S().shapes->Cube().Draw(ctx.device);
        fx.setAlphaProperty(1.0f);
    }

    void DrawOrder(const DrawContext& ctx, const Vector3& o)
    {
        const Matrix base = Matrix::CreateTranslation(o);
        gizmos_.Axes(base, 0.9f);
        gizmos_.Label(o + Off(0.95f, 0, 0), "world X", Color(255, 130, 120, 255), 0.5f);
        DrawBlock(ctx, Combined(order_) * base, kBlue, 1.0f);
        gizmos_.Label(Vector3::Transform(Vector3(0, 0.75f, 0), Combined(order_) * base), order_ == 0 ? "S * R * T" : "T * R * S", ToColor(kGold));
        RenderStates::SetTransparent3D(ctx.device, BlendState::NonPremultiplied);
        DrawBlock(ctx, Combined(1 - order_) * base, kRed, 0.35f);
        RenderStates::SetOpaque3D(ctx.device);
        gizmos_.Label(Vector3::Transform(Vector3(0, 0.75f, 0), Combined(1 - order_) * base), order_ == 0 ? "T * R * S" : "S * R * T", ToColor(kRed));
    }

    void DrawHierarchy(const DrawContext& ctx, const Vector3& o)
    {
        const Matrix base = Matrix::CreateTranslation(o);
        const Matrix parent = Matrix::CreateRotationY(angle_) * Matrix::CreateTranslation(0, 0.3f, 0) * base;
        gizmos_.Axes(parent, 0.7f);
        DrawBlock(ctx, Matrix::CreateScale(0.9f) * parent, kBlue, 1.0f);
        const Matrix child = inherit_ ? ChildWorld() * base : Matrix::CreateRotationZ(MathHelper::ToRadians(tilt_)) * Matrix::CreateTranslation(offset_, 0, 0) * Matrix::CreateTranslation(0, 0.3f, 0) * base;
        gizmos_.Line(parent.getTranslationProperty(), child.getTranslationProperty(), Color(255, 255, 255, 255));
        gizmos_.Axes(child, 0.4f);
        DrawBlock(ctx, Matrix::CreateScale(0.55f) * child, kGreen, 1.0f);
        gizmos_.Label(parent.getTranslationProperty() + Vector3(0, 0.7f, 0), "parent", ToColor(kBlue * 1.3f));
        gizmos_.Label(child.getTranslationProperty() + Vector3(0, 0.55f, 0), inherit_ ? "child (local * parent)" : "child (local only)", ToColor(kGreen * 1.2f));
    }

    void DrawLookAt(const DrawContext& ctx)
    {
        const Vector3 c = DioramaOrigin();
        DrawDiorama(ctx, S(), c, time_, 3.0f);
        const Matrix view = ViewMatrix();
        const Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(60.0f), 1.6f, 0.5f, 6.0f);
        gizmos_.Frustum(BoundingFrustum(view * proj), Color(255, 200, 80, 255));
        gizmos_.SolidSphere(ctx, CameraWorld(), 0.12f, kRed);
        gizmos_.Label(CameraWorld() + Vector3(0, 0.3f, 0), "camera (eye)", ToColor(kRed * 1.3f));
        // The camera's own axes are the rows of the view matrix's rotation part (transposed).
        Matrix inv;
        Matrix::Invert(view, inv);
        gizmos_.Axes(inv, 0.6f);
        viewA_->DrawInRoom(ctx, R().ToWorld(3.8f, 1.9f, -3.2f), -MathHelper::PiOver2 * 0.6f + R().frameYaw, 2.6f);
    }

    void DrawPerspective(const DrawContext& ctx)
    {
        const Vector3 c = DioramaOrigin();
        DrawDiorama(ctx, S(), c, time_, 3.0f);
        gizmos_.Frustum(BoundingFrustum(PerspectiveView() * ProjectionMatrix()), Color(255, 200, 80, 255));
        gizmos_.SolidSphere(ctx, PerspectiveEye(), 0.1f, kRed);
        viewA_->DrawInRoom(ctx, R().ToWorld(3.8f, 1.9f, -3.2f), -MathHelper::PiOver2 * 0.6f + R().frameYaw, 2.6f);
    }

    void DrawOrtho(const DrawContext& ctx)
    {
        DrawDiorama(ctx, S(), DioramaOrigin(), time_, 3.0f);
        viewA_->DrawInRoom(ctx, R().ToWorld(2.6f, 2.0f, -4.6f), -MathHelper::PiOver2 * 0.35f + R().frameYaw, 2.3f);
        viewB_->DrawInRoom(ctx, R().ToWorld(4.7f, 2.0f, -0.4f), -MathHelper::PiOver2 * 0.8f + R().frameYaw, 2.3f);
    }

    void DrawProject(const DrawContext& ctx, const Vector3& o)
    {
        gizmos_.Axes(Matrix::CreateTranslation(o), 0.8f);
        gizmos_.SolidSphere(ctx, WorldPoint() + Vector3(0, 0.6f, 0), 0.1f, kGold);
        gizmos_.Line(o, WorldPoint() + Vector3(0, 0.6f, 0), Color(255, 230, 120, 255));
        gizmos_.Label(WorldPoint() + Vector3(0, 0.9f, 0), "P", ToColor(kGold));
    }

    Mode mode_;
    Gizmos gizmos_;
    Vector3 origin_;
    std::unique_ptr<MiniView> viewA_, viewB_;
    float time_ = 0.0f, angle_ = 0.0f;
    // order
    float scale_ = 1.0f, rotY_ = 35.0f, moveX_ = 1.0f;
    int order_ = 0;
    // hierarchy
    float spin_ = 0.6f, offset_ = 1.2f, tilt_ = 25.0f;
    bool inherit_ = true;
    // view / projection
    Vector3 camera_{3.0f, 2.2f, 4.5f}, target_{0.0f, 0.0f, 0.0f};
    float fov_ = 60.0f, near_ = 0.5f, far_ = 10.0f, aspect_ = 1.6f, orthoWidth_ = 8.0f;
    Vector3 point_{0.5f, 1.2f, 0.0f};
};

}  // namespace

std::unique_ptr<Exhibit> MakeTransformLab(const RoomDef& room, Services& services)
{
    return std::make_unique<TransformLab>(room, services);
}
