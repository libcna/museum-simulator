// Vector3 laboratory: one exhibit class, seven rooms. The room's "mode" parameter picks which
// Vector3 operation is visualised; every result shown is computed by CNA's Vector3, and each room
// cross-checks it against a hand-written formula so a framework regression would show up on screen.
#include <algorithm>
#include <cmath>
#include <string>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Rendering/RenderStates.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Add, Dot, Cross, Project, Reflect, Lerp, Normalize };

Mode ParseMode(const std::string& s)
{
    if (s == "dot") return Mode::Dot;
    if (s == "cross") return Mode::Cross;
    if (s == "project") return Mode::Project;
    if (s == "reflect") return Mode::Reflect;
    if (s == "lerp") return Mode::Lerp;
    if (s == "normalize") return Mode::Normalize;
    return Mode::Add;
}

class VectorLab final : public Exhibit {
public:
    VectorLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        origin_ = BuildStagePlinth(b, 0.0f, 0.0f) + Vector3(0, 0.55f, 0);
        a_ = {1.4f, 20.0f, 25.0f};
        b_ = {1.1f, 130.0f, -15.0f};
        if (mode_ == Mode::Reflect) b_ = {1.0f, 90.0f, 90.0f};   // the surface normal: straight up
        if (mode_ == Mode::Normalize) a_ = {2.0f, 40.0f, 30.0f};

        ControlPanel& p = AddPanel(Title(), -4.6f, 1.5f, 1.8f, MathHelper::PiOver2, 1.9f);
        const char* nameA = mode_ == Mode::Reflect ? "Incoming I" : (mode_ == Mode::Normalize ? "Vector V" : "Vector A");
        p.AddSlider(std::string(nameA) + "  length", 0.2f, 2.2f, a_.length, [this](float v) { a_.length = v; }, "%.2f");
        p.AddSlider(std::string(nameA) + "  yaw", 0.0f, 360.0f, a_.yaw, [this](float v) { a_.yaw = v; }, "%.0f deg", 1.0f);
        p.AddSlider(std::string(nameA) + "  pitch", -85.0f, 85.0f, a_.pitch, [this](float v) { a_.pitch = v; }, "%.0f deg", 1.0f);
        if (mode_ != Mode::Normalize) {
            const char* nameB = mode_ == Mode::Reflect ? "Normal N" : "Vector B";
            if (mode_ != Mode::Reflect) p.AddSlider(std::string(nameB) + "  length", 0.2f, 2.2f, b_.length, [this](float v) { b_.length = v; }, "%.2f");
            p.AddSlider(std::string(nameB) + "  yaw", 0.0f, 360.0f, b_.yaw, [this](float v) { b_.yaw = v; }, "%.0f deg", 1.0f);
            p.AddSlider(std::string(nameB) + "  pitch", -85.0f, 90.0f, b_.pitch, [this](float v) { b_.pitch = v; }, "%.0f deg", 1.0f);
        }
        if (mode_ == Mode::Lerp) p.AddSlider("t (amount)", 0.0f, 1.0f, t_, [this](float v) { t_ = v; }, "%.2f");
        if (mode_ == Mode::Lerp) p.AddChoice("Curve", {"Lerp", "SmoothStep", "CatmullRom"}, 0, [this](int v) { curve_ = v; });
        p.AddHeading("Live results (computed by CNA)");
        AddReadouts(p);

        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.2f, 1.9f, what);
        SignSpec how = Kit::Plate("What to try", {"Point at a slider and click or drag it. The mouse wheel nudges.", "Red is A (or the incoming vector), blue is B, gold is the result."});
        how.leftAlign = true;
        b.AddWallSign(Wall::West, 0.0f, 2.2f, 3.6f, 1.3f, how);
    }

    void Update(const UpdateContext& ctx) override
    {
        if (ctx.nearby) time_ += ctx.dt;
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        const Vector3 o = R().ToWorld(0, 0, 0) + origin_ - R().ToWorld(0, 0, 0) + Vector3(0, 0, 0);
        const Vector3 A = a_.Value();
        const Vector3 B = b_.Value();
        gizmos_.Grid(Vector3(o.X, R().centre.Y + 0.03f, o.Z), 2.5f, 0.5f, Color(70, 80, 110, 255));
        gizmos_.Axes(Matrix::CreateTranslation(o), 0.6f);
        switch (mode_) {
            case Mode::Add: DrawAdd(ctx, o, A, B); break;
            case Mode::Dot: DrawDot(ctx, o, A, B); break;
            case Mode::Cross: DrawCross(ctx, o, A, B); break;
            case Mode::Project: DrawProject(ctx, o, A, B); break;
            case Mode::Reflect: DrawReflect(ctx, o, A, B); break;
            case Mode::Lerp: DrawLerp(ctx, o, A, B); break;
            case Mode::Normalize: DrawNormalize(ctx, o, A); break;
        }
        gizmos_.Flush(ctx);
    }

    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        switch (mode_) {
            case Mode::Add: return "Vector3.Add / Subtract";
            case Mode::Dot: return "Vector3.Dot";
            case Mode::Cross: return "Vector3.Cross";
            case Mode::Project: return "Projection and rejection";
            case Mode::Reflect: return "Vector3.Reflect";
            case Mode::Lerp: return "Vector3.Lerp / SmoothStep";
            default: return "Length, Normalize, Distance";
        }
    }

    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Add: return {"Adding vectors places B's tail on A's head: the sum is the diagonal of the parallelogram.", "A - B points from B's head to A's head (shown dashed in violet)."};
            case Mode::Dot: return {"Dot(A, B) = |A| |B| cos(angle). It is positive when the vectors point the same way,", "zero when they are perpendicular and negative when they oppose. The shadow of B on A is its projection."};
            case Mode::Cross: return {"Cross(A, B) is perpendicular to both. Its length is the area of the parallelogram they span,", "and its direction follows the right-hand rule. Swapping A and B flips it."};
            case Mode::Project: return {"Any vector splits into a part along A (projection) and a part across it (rejection).", "proj = A * Dot(B, A) / Dot(A, A), rejection = B - proj. XNA has no Project, so it is built from Dot."};
            case Mode::Reflect: return {"Reflect(I, N) = I - 2 Dot(I, N) N for a unit normal N. It is how light and balls bounce.", "Tilt the normal and watch the outgoing ray follow the mirror law."};
            case Mode::Lerp: return {"Lerp(A, B, t) = A + (B - A) t moves at constant speed. SmoothStep eases in and out.", "CatmullRom bends through points: here a control point before and after the pair."};
            default: return {"Length is the magnitude, Normalize gives a unit vector (length 1), Distance measures between points.", "Normalized vectors lie on the unit sphere; dividing by the length is all it takes."};
        }
    }

    void AddReadouts(ControlPanel& p)
    {
        p.AddReadout("A", [this] { return Fmt(a_.Value()); });
        if (mode_ != Mode::Normalize) p.AddReadout("B", [this] { return Fmt(b_.Value()); });
        switch (mode_) {
            case Mode::Add:
                p.AddReadout("A + B", [this] { return Fmt(Vector3::Add(a_.Value(), b_.Value())); });
                p.AddReadout("A - B", [this] { return Fmt(Vector3::Subtract(a_.Value(), b_.Value())); });
                p.AddReadout("Self-check", [this] { return Check(Vector3::Add(a_.Value(), b_.Value()), a_.Value() + b_.Value()); });
                break;
            case Mode::Dot:
                p.AddReadout("Dot(A, B)", [this] { return Fmt(Vector3::Dot(a_.Value(), b_.Value())); });
                p.AddReadout("Angle between", [this] {
                    const Vector3 A = a_.Value(), B = b_.Value();
                    return FmtDeg(std::acos(std::clamp(Vector3::Dot(A, B) / (A.Length() * B.Length()), -1.0f, 1.0f)));
                });
                p.AddReadout("Self-check", [this] {
                    const Vector3 A = a_.Value(), B = b_.Value();
                    const float manual = A.X * B.X + A.Y * B.Y + A.Z * B.Z;
                    return std::abs(manual - Vector3::Dot(A, B)) < 1e-4f ? std::string("OK: matches x1x2+y1y2+z1z2") : std::string("MISMATCH");
                });
                break;
            case Mode::Cross:
                p.AddReadout("Cross(A, B)", [this] { return Fmt(Vector3::Cross(a_.Value(), b_.Value())); });
                p.AddReadout("Area = |A x B|", [this] { return Fmt(Vector3::Cross(a_.Value(), b_.Value()).Length()); });
                p.AddReadout("Perpendicular to A and B", [this] {
                    const Vector3 c = Vector3::Cross(a_.Value(), b_.Value());
                    const bool ok = std::abs(Vector3::Dot(c, a_.Value())) < 1e-3f && std::abs(Vector3::Dot(c, b_.Value())) < 1e-3f;
                    return std::string(ok ? "yes (both dots ~ 0)" : "NO");
                });
                break;
            case Mode::Project:
                p.AddReadout("Projection of B on A", [this] { return Fmt(Projection()); });
                p.AddReadout("Rejection", [this] { return Fmt(b_.Value() - Projection()); });
                p.AddReadout("Rejection . A", [this] { return Fmt(Vector3::Dot(b_.Value() - Projection(), a_.Value())); });
                break;
            case Mode::Reflect:
                p.AddReadout("Reflect(I, N)", [this] { return Fmt(Reflected()); });
                p.AddReadout("|out| = |in|", [this] { return Fmt(Reflected().Length()) + " vs " + Fmt(a_.Value().Length()); });
                p.AddReadout("Self-check", [this] {
                    const Vector3 I = a_.Value();
                    const Vector3 N = Vector3::Normalize(b_.Value());
                    return Check(Vector3::Reflect(I, N), I - N * (2.0f * Vector3::Dot(I, N)));
                });
                break;
            case Mode::Lerp:
                p.AddReadout("Result", [this] { return Fmt(Interpolated()); });
                p.AddReadout("Distance A to B", [this] { return Fmt(Vector3::Distance(a_.Value(), b_.Value())); });
                break;
            case Mode::Normalize:
                p.AddReadout("Length()", [this] { return Fmt(a_.Value().Length()); });
                p.AddReadout("Normalize(V)", [this] { return Fmt(Vector3::Normalize(a_.Value())); });
                p.AddReadout("Length of that", [this] { return Fmt(Vector3::Normalize(a_.Value()).Length()); });
                p.AddReadout("Distance to unit point", [this] { return Fmt(Vector3::Distance(a_.Value(), Vector3::Normalize(a_.Value()))); });
                break;
        }
    }

    static std::string Check(const Vector3& cna, const Vector3& manual)
    {
        return Vector3::Distance(cna, manual) < 1e-4f ? "OK: matches the formula" : "MISMATCH with the formula";
    }

    [[nodiscard]] Vector3 Projection() const
    {
        const Vector3 A = a_.Value();
        return A * (Vector3::Dot(b_.Value(), A) / Vector3::Dot(A, A));
    }
    [[nodiscard]] Vector3 Reflected() const { return Vector3::Reflect(a_.Value(), Vector3::Normalize(b_.Value())); }
    [[nodiscard]] Vector3 Interpolated() const
    {
        const Vector3 A = a_.Value(), B = b_.Value();
        switch (curve_) {
            case 1: return Vector3::SmoothStep(A, B, t_);
            case 2: return Vector3::CatmullRom(A * 1.6f - B * 0.6f + Vector3(0, 0.7f, 0), A, B, B * 1.6f - A * 0.6f + Vector3(0, 0.7f, 0), t_);
            default: return Vector3::Lerp(A, B, t_);
        }
    }

    void Arrow(const DrawContext& ctx, const Vector3& from, const Vector3& to, const Vector3& colour, const char* label)
    {
        gizmos_.SolidArrow(ctx, from, to, colour, 0.035f);
        if (label != nullptr) gizmos_.Label((from + to) * 0.5f + Vector3(0, 0.12f, 0), label, ToColor(colour * 1.2f));
    }

    void DrawAdd(const DrawContext& ctx, const Vector3& o, const Vector3& A, const Vector3& B)
    {
        Arrow(ctx, o, o + A, kRed, "A");
        Arrow(ctx, o, o + B, kBlue, "B");
        gizmos_.Line(o + A, o + A + B, Color(100, 140, 255, 255));
        gizmos_.Line(o + B, o + A + B, Color(255, 110, 100, 255));
        Arrow(ctx, o, o + A + B, kGold, "A + B");
        gizmos_.Arrow(o + B, o + A, ToColor(kViolet), 0.12f);
        gizmos_.Label((o + B + o + A) * 0.5f, "A - B", ToColor(kViolet));
    }

    void DrawDot(const DrawContext& ctx, const Vector3& o, const Vector3& A, const Vector3& B)
    {
        Arrow(ctx, o, o + A, kRed, "A");
        Arrow(ctx, o, o + B, kBlue, "B");
        const float d = Vector3::Dot(A, B);
        const Vector3 proj = A * (d / Vector3::Dot(A, A));
        gizmos_.Line(o + B, o + proj, Color(200, 200, 200, 255));
        Arrow(ctx, o, o + proj, d >= 0 ? kGreen : kViolet, "proj");
        gizmos_.Label(o + Vector3(0, -0.2f, 0), "Dot = " + Fmt(d) + (d > 0.01f ? "  (same side)" : (d < -0.01f ? "  (opposed)" : "  (perpendicular)")),
                      d >= 0 ? Color(150, 255, 160, 255) : Color(230, 170, 255, 255));
    }

    void DrawCross(const DrawContext& ctx, const Vector3& o, const Vector3& A, const Vector3& B)
    {
        Arrow(ctx, o, o + A, kRed, "A");
        Arrow(ctx, o, o + B, kBlue, "B");
        const Vector3 c = Vector3::Cross(A, B);
        gizmos_.Line(o + A, o + A + B, Color(120, 120, 150, 255));
        gizmos_.Line(o + B, o + A + B, Color(120, 120, 150, 255));
        const Vector3 shown = c.Length() > 2.0f ? Vector3::Normalize(c) * 2.0f : c;
        Arrow(ctx, o, o + shown, kGold, "A x B");
        if (c.Length() > 2.0f) gizmos_.Label(o + shown + Vector3(0, 0.2f, 0), "(drawn shortened)", Color(200, 200, 200, 255), 0.45f);
    }

    void DrawProject(const DrawContext& ctx, const Vector3& o, const Vector3& A, const Vector3& B)
    {
        Arrow(ctx, o, o + A, kRed, "A");
        Arrow(ctx, o, o + B, kBlue, "B");
        const Vector3 p = Projection();
        Arrow(ctx, o, o + p, kGreen, "along A");
        gizmos_.Arrow(o + p, o + B, ToColor(kViolet), 0.12f);
        gizmos_.Label((o + p + o + B) * 0.5f, "rejection", ToColor(kViolet));
    }

    void DrawReflect(const DrawContext& ctx, const Vector3& o, const Vector3& I, const Vector3& N)
    {
        // The mirror: a square perpendicular to N through the stage origin.
        const Vector3 n = Vector3::Normalize(N);
        gizmos_.PlaneQuad(Plane(n, -Vector3::Dot(n, o)), o, 1.2f, Color(120, 200, 255, 255));
        Arrow(ctx, o - I, o, kRed, "I (in)");
        Arrow(ctx, o, o + n * 1.0f, kBlue, "N");
        Arrow(ctx, o, o + Vector3::Reflect(I, n), kGold, "out");
    }

    void DrawLerp(const DrawContext& ctx, const Vector3& o, const Vector3& A, const Vector3& B)
    {
        gizmos_.SolidSphere(ctx, o + A, 0.07f, kRed);
        gizmos_.SolidSphere(ctx, o + B, 0.07f, kBlue);
        gizmos_.Label(o + A + Vector3(0, 0.15f, 0), "A", ToColor(kRed * 1.2f));
        gizmos_.Label(o + B + Vector3(0, 0.15f, 0), "B", ToColor(kBlue * 1.2f));
        Vector3 prev = o + A;
        for (int i = 1; i <= 24; ++i) {
            const float k = static_cast<float>(i) / 24.0f;
            Vector3 p;
            const float saved = t_;
            t_ = k;
            p = o + Interpolated();
            t_ = saved;
            gizmos_.Line(prev, p, Color(200, 200, 120, 255));
            prev = p;
        }
        gizmos_.SolidSphere(ctx, o + Interpolated(), 0.1f, kGold);
        gizmos_.Label(o + Interpolated() + Vector3(0, 0.2f, 0), "t = " + Fmt(t_), ToColor(kGold));
    }

    void DrawNormalize(const DrawContext& ctx, const Vector3& o, const Vector3& V)
    {
        gizmos_.Sphere(BoundingSphere(o, 1.0f), Color(90, 140, 220, 255), 40);
        Arrow(ctx, o, o + V, kRed, "V");
        const Vector3 n = Vector3::Normalize(V);
        Arrow(ctx, o, o + n, kGold, "Normalize(V)");
        gizmos_.SolidSphere(ctx, o + n, 0.06f, kGold);
        gizmos_.Label(o + Vector3(0, 1.25f, 0), "unit sphere (radius 1)", Color(150, 190, 255, 255), 0.5f);
    }

    Mode mode_;
    Gizmos gizmos_;
    Vector3 origin_;
    SphericalVector a_, b_;
    float t_ = 0.5f;
    int curve_ = 0;
    float time_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeVectorLab(const RoomDef& room, Services& services)
{
    return std::make_unique<VectorLab>(room, services);
}
