// Interpolation curves: MathHelper easing functions, CatmullRom splines, the Curve class and barycentric coordinates.
#include <algorithm>
#include <cmath>
#include <string>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Curve.hpp"
#include "Microsoft/Xna/Framework/CurveContinuity.hpp"
#include "Microsoft/Xna/Framework/CurveKey.hpp"

using namespace Microsoft::Xna::Framework;
using namespace MathKit;

namespace {

enum class Mode { Easing, Spline, CurveClass, Barycentric };

class CurveLab final : public Exhibit {
public:
    CurveLab(const RoomDef& room, Services& services) : Exhibit(room, services), gizmos_(services)
    {
        const std::string m = room.params.Get("mode");
        mode_ = m == "spline" ? Mode::Spline : (m == "curve" ? Mode::CurveClass : (m == "barycentric" ? Mode::Barycentric : Mode::Easing));
        curve_.getKeysProperty().Add(CurveKey(0.0f, 0.0f));
        curve_.getKeysProperty().Add(CurveKey(1.0f, 1.0f));
        curve_.getKeysProperty().Add(CurveKey(2.0f, 0.2f));
        curve_.getKeysProperty().Add(CurveKey(3.0f, 0.8f));
        curve_.ComputeTangents(CurveTangent::Smooth);
    }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Easing:
                p.AddSlider("t", 0.0f, 1.0f, t_, [this](float v) { t_ = v; auto_ = false; }, "%.2f");
                p.AddToggle("Animate t", true, [this](bool v) { auto_ = v; });
                p.AddHeading("Values at t");
                p.AddReadout("Lerp", [this] { return Fmt(MathHelper::Lerp(0.0f, 1.0f, t_)); });
                p.AddReadout("SmoothStep", [this] { return Fmt(MathHelper::SmoothStep(0.0f, 1.0f, t_)); });
                p.AddReadout("Hermite (tangents 0, 3)", [this] { return Fmt(MathHelper::Hermite(0.0f, 0.0f, 1.0f, 3.0f, t_)); });
                p.AddReadout("CatmullRom (-1, 0, 1, 2)", [this] { return Fmt(MathHelper::CatmullRom(-1.0f, 0.0f, 1.0f, 2.0f, t_)); });
                break;
            case Mode::Spline:
                p.AddSlider("Point 2 height", 0.0f, 2.5f, pts_[1].Y, [this](float v) { pts_[1].Y = v; });
                p.AddSlider("Point 3 height", 0.0f, 2.5f, pts_[2].Y, [this](float v) { pts_[2].Y = v; });
                p.AddSlider("Point 3 depth", -1.5f, 1.5f, pts_[2].Z, [this](float v) { pts_[2].Z = v; });
                p.AddToggle("Animate the rider", true, [this](bool v) { auto_ = v; });
                p.AddHeading("Vector3.CatmullRom through every control point");
                p.AddReadout("Rider position", [this] { return Fmt(Rider()); });
                break;
            case Mode::CurveClass:
                p.AddChoice("Post-loop type", {"Constant", "Cycle", "CycleOffset", "Oscillate", "Linear"}, 0, [this](int v) {
                    static constexpr CurveLoopType kTypes[] = {CurveLoopType::Constant, CurveLoopType::Cycle, CurveLoopType::CycleOffset, CurveLoopType::Oscillate, CurveLoopType::Linear};
                    curve_.setPostLoopProperty(kTypes[v]);
                });
                p.AddChoice("Pre-loop type (before the first key)", {"Constant", "Cycle", "CycleOffset", "Oscillate", "Linear"}, 0, [this](int v) {
                    static constexpr CurveLoopType kTypes[] = {CurveLoopType::Constant, CurveLoopType::Cycle, CurveLoopType::CycleOffset, CurveLoopType::Oscillate, CurveLoopType::Linear};
                    curve_.setPreLoopProperty(kTypes[v]);
                });
                p.AddChoice("Key 2 tangent only (ComputeTangent)", {"Smooth", "Linear", "Flat"}, 0, [this](int v) {
                    static constexpr CurveTangent kTan[] = {CurveTangent::Smooth, CurveTangent::Linear, CurveTangent::Flat};
                    curve_.ComputeTangent(2, kTan[v]);
                });
                p.AddChoice("Tangents", {"Smooth", "Linear", "Flat"}, 0, [this](int v) {
                    static constexpr CurveTangent kTan[] = {CurveTangent::Smooth, CurveTangent::Linear, CurveTangent::Flat};
                    curve_.ComputeTangents(kTan[v]);
                });
                p.AddToggle("Key 1 continuity: Step", false, [this](bool v) { curve_.getKeysProperty()[1].setContinuityProperty(v ? CurveContinuity::Step : CurveContinuity::Smooth); });
                p.AddToggle("Animate position", true, [this](bool v) { auto_ = v; });
                p.AddSlider("Position", 0.0f, 8.0f, pos_, [this](float v) { pos_ = v; auto_ = false; }, "%.2f");
                p.AddHeading("Curve.Evaluate");
                p.AddReadout("Evaluate(position)", [this] { return Fmt(curve_.Evaluate(pos_)); });
                p.AddReadout("Key 1 TangentIn / TangentOut", [this] { return Fmt(curve_.getKeysProperty()[1].getTangentInProperty()) + " / " + Fmt(curve_.getKeysProperty()[1].getTangentOutProperty()); });
                p.AddReadout("Key 1 Continuity", [this] { return std::string(curve_.getKeysProperty()[1].getContinuityProperty() == CurveContinuity::Step ? "Step" : "Smooth"); });
                p.AddReadout("IsConstant", [this] { return std::string(curve_.getIsConstantProperty() ? "true" : "false"); });
                p.AddReadout("Keys", [this] { return std::to_string(curve_.getKeysProperty().getCountProperty()) + " keys at 0, 1, 2, 3"; });
                break;
            case Mode::Barycentric:
                p.AddSlider("Weight u", 0.0f, 1.0f, u_, [this](float v) { u_ = v; if (u_ + v_ > 1.0f) v_ = 1.0f - u_; }, "%.2f");
                p.AddSlider("Weight v", 0.0f, 1.0f, v_, [this](float v) { v_ = v; if (u_ + v_ > 1.0f) u_ = 1.0f - v_; }, "%.2f");
                p.AddHeading("Vector3.Barycentric(A, B, C, u, v)");
                p.AddReadout("Weights (A, B, C)", [this] {
                    char buf[80];
                    std::snprintf(buf, sizeof buf, "%.2f  %.2f  %.2f", static_cast<double>(1 - u_ - v_), static_cast<double>(u_), static_cast<double>(v_));
                    return std::string(buf);
                });
                p.AddReadout("Point", [this] { return Fmt(Bary()); });
                p.AddReadout("Inside the triangle?", [this] { return std::string(u_ + v_ <= 1.0f ? "yes (all weights >= 0)" : "no"); });
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
        BuildStagePlinth(b, 0.0f, 3.2f, 0.4f);
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        if (auto_) {
            t_ = 0.5f - 0.5f * std::cos(time_ * 0.9f);
            pos_ = std::fmod(time_ * 0.8f, 8.0f);
            rider_ = std::fmod(time_ * 0.25f, 1.0f);
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        const Vector3 o = R().ToWorld(0.0f, 1.2f, 0.0f);
        switch (mode_) {
            case Mode::Easing: DrawEasing(ctx, o); break;
            case Mode::Spline: DrawSpline(ctx, o); break;
            case Mode::CurveClass: DrawCurveClass(ctx, o); break;
            case Mode::Barycentric: DrawBary(ctx, o); break;
        }
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        switch (mode_) {
            case Mode::Easing: return "MathHelper easing functions";
            case Mode::Spline: return "Vector3.CatmullRom spline";
            case Mode::CurveClass: return "The Curve class";
            default: return "Vector3.Barycentric";
        }
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Easing: return {"Four ways to get from 0 to 1. Lerp is a straight line, SmoothStep eases in and out,", "Hermite takes start and end tangents, CatmullRom passes through its middle points smoothly.", "The balls ride each graph at the current t."};
            case Mode::Spline: return {"Vector3.CatmullRom gives a smooth path through control points, with continuous direction.", "Move the points: the spline always passes through them and the rider follows it."};
            case Mode::CurveClass: return {"Curve stores keys (position, value, tangents) and Evaluate interpolates between them. ComputeTangents", "picks smooth, linear or flat tangents; the loop type decides what happens before and after the keys."};
            default: return {"Barycentric coordinates express a point inside a triangle as a weighted mix of its corners.", "Vector3.Barycentric(A, B, C, u, v) = A + u (B - A) + v (C - A); the weights sum to 1."};
        }
    }

    void DrawEasing(const DrawContext&, const Vector3& o)
    {
        const Vector3 base = o + Off(-1.5f, -0.7f, 0);
        const auto at = [&](float x, float y) { return base + Off(x * 3.0f, y * 1.6f, 0); };
        gizmos_.Line(at(0, 0), at(1, 0), Color(160, 160, 190, 255));
        gizmos_.Line(at(0, 0), at(0, 1), Color(160, 160, 190, 255));
        gizmos_.Line(at(1, 0), at(1, 1), Color(70, 70, 90, 255));
        gizmos_.Line(at(0, 1), at(1, 1), Color(70, 70, 90, 255));
        struct Fn {
            float (*f)(float);
            Color c;
            const char* name;
        };
        const Fn fns[4] = {
            {[](float t) { return MathHelper::Lerp(0.0f, 1.0f, t); }, Color(120, 200, 255, 255), "Lerp"},
            {[](float t) { return MathHelper::SmoothStep(0.0f, 1.0f, t); }, Color(255, 200, 90, 255), "SmoothStep"},
            {[](float t) { return MathHelper::Hermite(0.0f, 0.0f, 1.0f, 3.0f, t); }, Color(150, 255, 150, 255), "Hermite"},
            {[](float t) { return MathHelper::CatmullRom(-1.0f, 0.0f, 1.0f, 2.0f, t); }, Color(255, 140, 200, 255), "CatmullRom"},
        };
        for (const Fn& fn : fns) {
            Vector3 prev = at(0, fn.f(0));
            for (int i = 1; i <= 40; ++i) {
                const float t = static_cast<float>(i) / 40.0f;
                const Vector3 p = at(t, fn.f(t));
                gizmos_.Line(prev, p, fn.c);
                prev = p;
            }
            const Vector3 ball = at(t_, fn.f(t_));
            gizmos_.Box(BoundingBox(ball - Vector3(0.05f, 0.05f, 0.05f), ball + Vector3(0.05f, 0.05f, 0.05f)), fn.c);
            gizmos_.Label(at(1.0f, fn.f(1.0f)) + Vector3(0.45f, 0, 0), fn.name, fn.c, 0.5f);
        }
        gizmos_.Label(at(t_, -0.08f), "t", Color(255, 255, 255, 255), 0.5f);
    }

    [[nodiscard]] Vector3 Rider() const
    {
        const float seg = rider_ * 3.0f;
        const int i = std::min(2, static_cast<int>(seg));
        const float t = seg - static_cast<float>(i);
        return Vector3::CatmullRom(pts_[static_cast<std::size_t>(std::max(0, i - 1))], pts_[static_cast<std::size_t>(i)], pts_[static_cast<std::size_t>(i + 1)],
                                   pts_[static_cast<std::size_t>(std::min(3, i + 2))], t);
    }
    void DrawSpline(const DrawContext& ctx, const Vector3& o)
    {
        for (int i = 0; i < 4; ++i) {
            gizmos_.SolidSphere(ctx, o + pts_[static_cast<std::size_t>(i)], 0.07f, kRed);
            gizmos_.Label(o + pts_[static_cast<std::size_t>(i)] + Vector3(0, 0.2f, 0), std::to_string(i + 1), ToColor(kRed * 1.3f), 0.5f);
        }
        for (int s = 0; s < 3; ++s) {
            Vector3 prev = o + pts_[static_cast<std::size_t>(s)];
            for (int k = 1; k <= 20; ++k) {
                const float t = static_cast<float>(k) / 20.0f;
                const Vector3 p = o + Vector3::CatmullRom(pts_[static_cast<std::size_t>(std::max(0, s - 1))], pts_[static_cast<std::size_t>(s)], pts_[static_cast<std::size_t>(s + 1)],
                                                         pts_[static_cast<std::size_t>(std::min(3, s + 2))], t);
                gizmos_.Line(prev, p, Color(255, 220, 110, 255));
                prev = p;
            }
        }
        gizmos_.SolidSphere(ctx, o + Rider(), 0.1f, kGold);
    }

    void DrawCurveClass(const DrawContext&, const Vector3& o)
    {
        const Vector3 base = o + Off(-1.8f, -0.7f, 0);
        const auto at = [&](float x, float y) { return base + Off(x * 0.45f, y * 1.4f, 0); };
        gizmos_.Line(at(0, 0), at(8, 0), Color(140, 140, 170, 255));
        gizmos_.Line(at(0, -0.3f), at(0, 1.3f), Color(140, 140, 170, 255));
        Vector3 prev = at(-1.0f, curve_.Evaluate(-1.0f));
        for (int i = 1; i <= 90; ++i) {
            const float x = -1.0f + 9.0f * static_cast<float>(i) / 90.0f;
            const Vector3 p = at(x, curve_.Evaluate(x));
            gizmos_.Line(prev, p, Color(120, 200, 255, 255));
            prev = p;
        }
        for (int k = 0; k < 4; ++k) {
            const Vector3 kp = at(static_cast<float>(k), curve_.getKeysProperty()[k].getValueProperty());
            gizmos_.Box(BoundingBox(kp - Vector3(0.04f, 0.04f, 0.04f), kp + Vector3(0.04f, 0.04f, 0.04f)), Color(255, 120, 100, 255));
        }
        const Vector3 ball = at(pos_, curve_.Evaluate(pos_));
        gizmos_.Box(BoundingBox(ball - Vector3(0.07f, 0.07f, 0.07f), ball + Vector3(0.07f, 0.07f, 0.07f)), Color(255, 230, 90, 255));
        gizmos_.Line(at(pos_, 0), ball, Color(255, 230, 90, 255));
        gizmos_.Label(at(pos_, -0.15f), "position", Color(255, 240, 150, 255), 0.5f);
    }

    [[nodiscard]] Vector3 Bary() const { return Vector3::Barycentric(tri_[0], tri_[1], tri_[2], u_, v_); }
    void DrawBary(const DrawContext& ctx, const Vector3& o)
    {
        const Vector3 c[3] = {o + tri_[0], o + tri_[1], o + tri_[2]};
        for (int i = 0; i < 3; ++i) {
            gizmos_.Line(c[i], c[(i + 1) % 3], Color(150, 200, 255, 255));
            gizmos_.SolidSphere(ctx, c[i], 0.07f, i == 0 ? kRed : (i == 1 ? kGreen : kBlue));
        }
        gizmos_.Label(c[0] + Vector3(0, 0.2f, 0), "A", ToColor(kRed * 1.3f));
        gizmos_.Label(c[1] + Vector3(0, 0.2f, 0), "B", ToColor(kGreen * 1.3f));
        gizmos_.Label(c[2] + Vector3(0, 0.2f, 0), "C", ToColor(kBlue * 1.3f));
        const Vector3 p = o + Bary();
        for (int i = 0; i < 3; ++i) gizmos_.Line(p, c[i], Color(110, 110, 130, 255));
        gizmos_.SolidSphere(ctx, p, 0.09f, kGold);
    }

    Mode mode_;
    Gizmos gizmos_;
    Curve curve_;
    float time_ = 0.0f, t_ = 0.0f, pos_ = 0.0f, rider_ = 0.0f;
    bool auto_ = true;
    float u_ = 0.3f, v_ = 0.3f;
    Vector3 tri_[3] = {{-1.0f, -0.6f, 0.0f}, {1.2f, -0.4f, 0.5f}, {0.1f, 0.9f, -0.4f}};
    Vector3 pts_[4] = {{-1.6f, -0.3f, 0.0f}, {-0.5f, 0.8f, 0.0f}, {0.6f, -0.2f, 0.4f}, {1.6f, 0.7f, 0.0f}};
};

}  // namespace

std::unique_ptr<Exhibit> MakeCurveLab(const RoomDef& room, Services& services)
{
    return std::make_unique<CurveLab>(room, services);
}
