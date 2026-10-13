// Quaternion laboratory: Euler angles vs quaternions (and gimbal lock), Slerp vs Lerp, axis-angle.
#include <algorithm>
#include <cmath>
#include <string>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/RenderStates.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Euler, Slerp, AxisAngle };

class QuaternionLab final : public Exhibit {
public:
    QuaternionLab(const RoomDef& room, Services& services)
        : Exhibit(room, services), gizmos_(services)
    {
        const std::string m = room.params.Get("mode");
        mode_ = m == "slerp" ? Mode::Slerp : (m == "axisangle" ? Mode::AxisAngle : Mode::Euler);
    }

    void BuildStatic(RoomBuilder& b) override
    {
        origin_ = R().ToWorld(0, 0, 0);
        BuildStagePlinth(b, 0.0f, 0.0f);
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Euler:
                p.AddSlider("Yaw (about Y)", -180.0f, 180.0f, yaw_, [this](float v) { yaw_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Pitch (about X)", -90.0f, 90.0f, pitch_, [this](float v) { pitch_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Roll (about Z)", -180.0f, 180.0f, roll_, [this](float v) { roll_ = v; }, "%.0f deg", 1.0f);
                p.AddButton("Jump to gimbal lock (pitch 90)", [this] { yaw_ = 40.0f; pitch_ = 90.0f; roll_ = 0.0f; SyncSliders(); });
                p.AddHeading("Left: quaternion.  Right: three matrix rotations");
                p.AddReadout("Quaternion xyzw", [this] { return FmtQ(Quat()); });
                p.AddReadout("Same orientation?", [this] { return Matches() ? std::string("yes: both agree") : std::string("NO"); });
                p.AddReadout("Yaw axis . roll axis", [this] { return Fmt(Vector3::Dot(YawAxis(), RollAxis())) + (std::abs(Vector3::Dot(YawAxis(), RollAxis())) > 0.99f ? "  GIMBAL LOCK" : ""); });
                break;
            case Mode::Slerp:
                p.AddSlider("t", 0.0f, 1.0f, t_, [this](float v) { t_ = v; auto_ = false; SyncSliders(); }, "%.2f");
                p.AddToggle("Animate t", true, [this](bool v) { auto_ = v; });
                p.AddSlider("Angle between A and B", 20.0f, 340.0f, span_, [this](float v) { span_ = v; }, "%.0f deg", 1.0f);
                p.AddHeading("Angular progress (should equal t)");
                p.AddReadout("Slerp", [this] { return Fmt(Progress(Quaternion::Slerp(A(), B(), t_))); });
                p.AddReadout("Lerp (normalised)", [this] { return Fmt(Progress(Quaternion::Normalize(Quaternion::Lerp(A(), B(), t_)))); });
                p.AddReadout("t", [this] { return Fmt(t_); });
                break;
            case Mode::AxisAngle:
                p.AddSlider("Axis direction (yaw)", 0.0f, 360.0f, axisYaw_, [this](float v) { axisYaw_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Axis direction (pitch)", -85.0f, 85.0f, axisPitch_, [this](float v) { axisPitch_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Angle", -180.0f, 180.0f, angle_, [this](float v) { angle_ = v; }, "%.0f deg", 1.0f);
                p.AddToggle("Then rotate again (Concatenate)", false, [this](bool v) { second_ = v; });
                p.AddHeading("Quaternion built by CreateFromAxisAngle");
                p.AddReadout("xyzw", [this] { return FmtQ(AxisQuat()); });
                p.AddReadout("Length (always 1)", [this] { return Fmt(AxisQuat().Length()); });
                p.AddReadout("Inverse * q", [this] { return FmtQ(Quaternion::Concatenate(AxisQuat(), Quaternion::Inverse(AxisQuat()))); });
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        if (mode_ == Mode::Slerp && auto_) t_ = 0.5f - 0.5f * std::cos(time_ * 0.8f);
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        const Vector3 o = origin_ + Vector3(0, 1.45f, 0);
        gizmos_.Grid(Vector3(o.X, R().centre.Y + 0.03f, o.Z), 2.5f, 0.5f, Color(70, 80, 110, 255));
        switch (mode_) {
            case Mode::Euler: {
                const Quaternion q = Quat();
                DrawBody(ctx, Matrix::CreateFromQuaternion(q) * Matrix::CreateTranslation(o + Off(-1.1f, 0, 0)), kBlue);
                DrawBody(ctx, MatrixEuler() * Matrix::CreateTranslation(o + Off(1.1f, 0, 0)), kGreen);
                gizmos_.Label(o + Off(-1.1f, 0.85f, 0), "Quaternion", ToColor(kBlue * 1.3f));
                gizmos_.Label(o + Off(1.1f, 0.85f, 0), "Matrices Z * X * Y", ToColor(kGreen * 1.2f));
                const Vector3 ya = YawAxis() * 0.9f, ra = RollAxis() * 0.9f;
                gizmos_.Arrow(o + Off(-1.1f, 0, 0), o + Off(-1.1f, 0, 0) + ya, Color(255, 220, 90, 255), 0.1f);
                gizmos_.Arrow(o + Off(-1.1f, 0, 0), o + Off(-1.1f, 0, 0) + ra, Color(255, 120, 200, 255), 0.1f);
                gizmos_.Label(o + Off(-1.1f, 0, 0) + ya + Vector3(0, 0.1f, 0), "yaw axis", Color(255, 230, 120, 255), 0.45f);
                gizmos_.Label(o + Off(-1.1f, 0, 0) + ra + Vector3(0, -0.1f, 0), "roll axis", Color(255, 150, 215, 255), 0.45f);
                break;
            }
            case Mode::Slerp: {
                DrawBody(ctx, Matrix::CreateFromQuaternion(A()) * Matrix::CreateTranslation(o + Off(-1.7f, 0, 0)), kRed);
                DrawBody(ctx, Matrix::CreateFromQuaternion(B()) * Matrix::CreateTranslation(o + Off(1.7f, 0, 0)), kBlue);
                DrawBody(ctx, Matrix::CreateFromQuaternion(Quaternion::Slerp(A(), B(), t_)) * Matrix::CreateTranslation(o + Vector3(0, 0.5f, 0)), kGold);
                DrawBody(ctx, Matrix::CreateFromQuaternion(Quaternion::Normalize(Quaternion::Lerp(A(), B(), t_))) * Matrix::CreateTranslation(o + Vector3(0, -0.5f, 0)), kViolet);
                gizmos_.Label(o + Off(-1.7f, 0.85f, 0), "A", ToColor(kRed * 1.3f));
                gizmos_.Label(o + Off(1.7f, 0.85f, 0), "B", ToColor(kBlue * 1.3f));
                gizmos_.Label(o + Off(0.9f, 0.5f, 0), "Slerp", ToColor(kGold));
                gizmos_.Label(o + Off(0.9f, -0.5f, 0), "Lerp + Normalize", ToColor(kViolet * 1.2f));
                break;
            }
            case Mode::AxisAngle: {
                const Vector3 axis = AxisVector();
                gizmos_.Arrow(o - axis * 1.1f, o + axis * 1.1f, Color(255, 220, 90, 255), 0.12f);
                gizmos_.Label(o + axis * 1.2f, "axis", Color(255, 230, 120, 255), 0.5f);
                DrawBody(ctx, Matrix::CreateFromQuaternion(Quaternion::Identity) * Matrix::CreateTranslation(o + Off(-1.8f, 0, 0)), kRed);
                DrawBody(ctx, Matrix::CreateFromQuaternion(Total()) * Matrix::CreateTranslation(o), kGold);
                gizmos_.Label(o + Off(-1.8f, 0.85f, 0), "identity", ToColor(kRed * 1.3f));
                gizmos_.Label(o + Vector3(0, 0.85f, 0), second_ ? "q then q again" : "q", ToColor(kGold));
                const Quaternion back = Quaternion::Concatenate(Total(), Quaternion::Inverse(Total()));
                DrawBody(ctx, Matrix::CreateFromQuaternion(back) * Matrix::CreateTranslation(o + Off(1.8f, 0, 0)), kGreen);
                gizmos_.Label(o + Off(1.8f, 0.85f, 0), "q then Inverse(q)", ToColor(kGreen * 1.2f));
                break;
            }
        }
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        return mode_ == Mode::Euler ? "Euler angles and gimbal lock" : (mode_ == Mode::Slerp ? "Quaternion.Slerp vs Lerp" : "Axis-angle quaternions");
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Euler: return {"Three Euler angles describe an orientation as yaw, pitch and roll applied in turn. XNA applies roll,", "then pitch, then yaw. At pitch = 90 degrees the yaw and roll axes line up: one degree of freedom", "is lost (gimbal lock). The quaternion on the left has no such singularity."};
            case Mode::Slerp: return {"Lerp on quaternions then Normalize is cheap but sweeps unevenly: fast in the middle, slow at the ends.", "Slerp moves at constant angular speed. The readouts show how far each has turned for the same t."};
            default: return {"CreateFromAxisAngle(axis, angle) stores half the angle in w and sin(half) * axis in xyz, always unit length.", "Concatenate applies one rotation after another; multiplying by Inverse undoes it."};
        }
    }

    [[nodiscard]] Quaternion Quat() const { return Quaternion::CreateFromYawPitchRoll(MathHelper::ToRadians(yaw_), MathHelper::ToRadians(pitch_), MathHelper::ToRadians(roll_)); }
    [[nodiscard]] Matrix MatrixEuler() const
    {
        return Matrix::CreateRotationZ(MathHelper::ToRadians(roll_)) * Matrix::CreateRotationX(MathHelper::ToRadians(pitch_)) * Matrix::CreateRotationY(MathHelper::ToRadians(yaw_));
    }
    [[nodiscard]] bool Matches() const
    {
        const Matrix a = Matrix::CreateFromQuaternion(Quat());
        const Matrix b = MatrixEuler();
        return Vector3::Distance(Vector3::TransformNormal(Vector3::UnitX, a), Vector3::TransformNormal(Vector3::UnitX, b)) < 1e-3f &&
               Vector3::Distance(Vector3::TransformNormal(Vector3::UnitY, a), Vector3::TransformNormal(Vector3::UnitY, b)) < 1e-3f;
    }
    [[nodiscard]] Vector3 YawAxis() const { return Vector3::UnitY; }
    // The roll axis (local Z) after pitch and yaw have been applied.
    [[nodiscard]] Vector3 RollAxis() const { return Vector3::TransformNormal(Vector3::UnitZ, Matrix::CreateRotationX(MathHelper::ToRadians(pitch_)) * Matrix::CreateRotationY(MathHelper::ToRadians(yaw_))); }
    [[nodiscard]] Quaternion A() const { return Quaternion::CreateFromAxisAngle(Vector3::UnitY, 0.0f); }
    [[nodiscard]] Quaternion B() const { return Quaternion::CreateFromAxisAngle(Vector3::Normalize(Vector3(0.3f, 1.0f, 0.2f)), MathHelper::ToRadians(span_)); }
    [[nodiscard]] float Progress(const Quaternion& q) const
    {
        const float total = 2.0f * std::acos(std::clamp(std::abs(Quaternion::Dot(A(), B())), 0.0f, 1.0f));
        const float now = 2.0f * std::acos(std::clamp(std::abs(Quaternion::Dot(A(), q)), 0.0f, 1.0f));
        return total > 1e-4f ? now / total : 0.0f;
    }
    [[nodiscard]] Vector3 AxisVector() const
    {
        const float y = MathHelper::ToRadians(axisYaw_), p = MathHelper::ToRadians(axisPitch_);
        return Vector3(std::cos(p) * std::cos(y), std::sin(p), std::cos(p) * std::sin(y));
    }
    [[nodiscard]] Quaternion AxisQuat() const { return Quaternion::CreateFromAxisAngle(AxisVector(), MathHelper::ToRadians(angle_)); }
    [[nodiscard]] Quaternion Total() const { return second_ ? Quaternion::Concatenate(AxisQuat(), AxisQuat()) : AxisQuat(); }
    static std::string FmtQ(const Quaternion& q)
    {
        char buf[96];
        std::snprintf(buf, sizeof buf, "(%.2f %.2f %.2f %.2f)", static_cast<double>(q.X), static_cast<double>(q.Y), static_cast<double>(q.Z), static_cast<double>(q.W));
        return buf;
    }
    void SyncSliders() {}

    void DrawBody(const DrawContext& ctx, const Matrix& world, const Vector3& colour)
    {
        // An aircraft-like body: fuselage, wings and a tail fin, so every axis is recognisable.
        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPunctualLightEXT(PunctualLightEXT{});
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setDiffuseColorProperty(colour);
        fx.setEmissiveColorProperty(colour * 0.12f);
        fx.setSpecularColorProperty(Vector3(0.3f, 0.3f, 0.3f));
        fx.setWorldProperty(Matrix::CreateScale(0.22f, 0.22f, 1.0f) * world);
        EffectBank::Apply(fx);
        S().shapes->Cube().Draw(ctx.device);
        fx.setWorldProperty(Matrix::CreateScale(1.1f, 0.05f, 0.3f) * Matrix::CreateTranslation(0, 0, 0.1f) * world);
        EffectBank::Apply(fx);
        S().shapes->Cube().Draw(ctx.device);
        fx.setDiffuseColorProperty(Vector3(1.0f, 0.85f, 0.3f));
        fx.setWorldProperty(Matrix::CreateScale(0.05f, 0.3f, 0.25f) * Matrix::CreateTranslation(0, 0.2f, 0.42f) * world);
        EffectBank::Apply(fx);
        S().shapes->Cube().Draw(ctx.device);
        fx.setDiffuseColorProperty(Vector3(0.95f, 0.95f, 1.0f));
        fx.setWorldProperty(Matrix::CreateScale(0.2f) * Matrix::CreateTranslation(0, 0, -0.55f) * world);   // nose
        EffectBank::Apply(fx);
        S().shapes->Sphere().Draw(ctx.device);
    }

    Mode mode_;
    Gizmos gizmos_;
    Vector3 origin_;
    float time_ = 0.0f;
    float yaw_ = 30.0f, pitch_ = 40.0f, roll_ = 20.0f;
    float t_ = 0.0f, span_ = 160.0f;
    bool auto_ = true;
    float axisYaw_ = 40.0f, axisPitch_ = 30.0f, angle_ = 70.0f;
    bool second_ = false;
};

}  // namespace

std::unique_ptr<Exhibit> MakeQuaternionLab(const RoomDef& room, Services& services)
{
    return std::make_unique<QuaternionLab>(room, services);
}
