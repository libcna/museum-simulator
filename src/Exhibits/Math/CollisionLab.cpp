// Collision and geometry tests: every verdict on screen is computed by CNA's BoundingBox, BoundingSphere,
// BoundingFrustum, Plane and Ray. One class, many rooms (the "mode" parameter).
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/ContainmentType.hpp"
#include "Microsoft/Xna/Framework/Plane.hpp"
#include "Microsoft/Xna/Framework/PlaneIntersectionType.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"

using namespace Microsoft::Xna::Framework;
using namespace MathKit;

namespace {

enum class Mode { BoxBox, SphereBox, RayBox, RaySphere, RayPlane, PlaneBox, Merge, Frustum, Pick };

Mode ParseMode(const std::string& s)
{
    if (s == "sphere-box") return Mode::SphereBox;
    if (s == "ray-box") return Mode::RayBox;
    if (s == "ray-sphere") return Mode::RaySphere;
    if (s == "ray-plane") return Mode::RayPlane;
    if (s == "plane-box") return Mode::PlaneBox;
    if (s == "merge") return Mode::Merge;
    if (s == "frustum") return Mode::Frustum;
    if (s == "pick") return Mode::Pick;
    return Mode::BoxBox;
}

const char* Name(ContainmentType c)
{
    return c == ContainmentType::Disjoint ? "Disjoint" : (c == ContainmentType::Contains ? "Contains" : "Intersects");
}
const char* Name(PlaneIntersectionType c)
{
    return c == PlaneIntersectionType::Front ? "Front" : (c == PlaneIntersectionType::Back ? "Back" : "Intersecting");
}
Color ForContainment(ContainmentType c)
{
    return c == ContainmentType::Disjoint ? Color(110, 160, 255, 255) : (c == ContainmentType::Contains ? Color(110, 255, 140, 255) : Color(255, 110, 100, 255));
}

class CollisionLab final : public Exhibit {
public:
    CollisionLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services)
    {
        // Deterministic point cloud for the merge room.
        unsigned s = 12345u;
        const auto rnd = [&s] {
            s = s * 1664525u + 1013904223u;
            return static_cast<float>((s >> 8) & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
        };
        for (int i = 0; i < 64; ++i) cloud_.emplace_back(rnd(), rnd() * 0.6f, rnd());
    }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::BoxBox:
                p.AddSlider("Box B  x", -2.5f, 2.5f, mx_, [this](float v) { mx_ = v; });
                p.AddSlider("Box B  y", -1.2f, 1.2f, my_, [this](float v) { my_ = v; });
                p.AddSlider("Box B  size", 0.3f, 1.6f, size_, [this](float v) { size_ = v; });
                p.AddHeading("Box A is the big grey box");
                p.AddReadout("A.Intersects(B)", [this] { return A().Intersects(Bb()) ? "true" : "false"; });
                p.AddReadout("A.Contains(B)", [this] { return Name(A().Contains(Bb())); });
                p.AddReadout("B.Contains(A)", [this] { return Name(Bb().Contains(A())); });
                break;
            case Mode::SphereBox:
                p.AddSlider("Sphere  x", -2.5f, 2.5f, mx_, [this](float v) { mx_ = v; });
                p.AddSlider("Sphere  y", -1.2f, 1.2f, my_, [this](float v) { my_ = v; });
                p.AddSlider("Radius", 0.2f, 1.2f, size_, [this](float v) { size_ = v; });
                p.AddHeading("Sphere vs the grey box");
                p.AddReadout("sphere.Intersects(box)", [this] { return Sph().Intersects(A()) ? "true" : "false"; });
                p.AddReadout("sphere.Contains(box)", [this] { return Name(Sph().Contains(A())); });
                p.AddReadout("box.Contains(sphere)", [this] { return Name(A().Contains(Sph())); });
                break;
            case Mode::RayBox:
            case Mode::RaySphere:
            case Mode::RayPlane:
                p.AddSlider("Ray origin height", -1.0f, 1.5f, my_, [this](float v) { my_ = v; });
                p.AddSlider("Ray aim (yaw)", -40.0f, 40.0f, aimYaw_, [this](float v) { aimYaw_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Ray aim (pitch)", -35.0f, 35.0f, aimPitch_, [this](float v) { aimPitch_ = v; }, "%.0f deg", 1.0f);
                if (mode_ == Mode::RayPlane) p.AddSlider("Plane tilt", -60.0f, 60.0f, tilt_, [this](float v) { tilt_ = v; }, "%.0f deg", 1.0f);
                p.AddHeading(mode_ == Mode::RayBox ? "ray.Intersects(BoundingBox)" : (mode_ == Mode::RaySphere ? "ray.Intersects(BoundingSphere)" : "ray.Intersects(Plane)"));
                p.AddReadout("Distance along the ray", [this] {
                    const std::optional<float> d = Cast();
                    return d ? Fmt(*d) + " m" : std::string("no hit");
                });
                p.AddReadout("Hit point", [this] {
                    const std::optional<float> d = Cast();
                    return d ? Fmt(Hit(*d) - Origin()) : std::string("-");
                });
                break;
            case Mode::PlaneBox:
                p.AddSlider("Plane height", -1.2f, 1.2f, my_, [this](float v) { my_ = v; });
                p.AddSlider("Plane tilt", -60.0f, 60.0f, tilt_, [this](float v) { tilt_ = v; }, "%.0f deg", 1.0f);
                p.AddHeading("Plane.Intersects(box) for five boxes");
                for (int i = 0; i < 5; ++i) p.AddReadout("Box " + std::to_string(i + 1), [this, i] { return Name(PlaneNow().Intersects(Row(i))); });
                p.AddReadout("DotCoordinate(origin)", [this] { return Fmt(PlaneNow().DotCoordinate(Vector3(0, 0, 0))); });
                p.AddReadout("DotNormal(up)", [this] { return Fmt(PlaneNow().DotNormal(Vector3::Up)); });
                break;
            case Mode::Merge:
                p.AddSlider("Points used", 4.0f, 64.0f, static_cast<float>(count_), [this](float v) { count_ = static_cast<int>(v); }, "%.0f", 1.0f);
                p.AddSlider("Scale", 0.4f, 1.6f, size_, [this](float v) { size_ = v; });
                p.AddHeading("Bounds fitted to the cloud");
                p.AddReadout("Box size", [this] { return Fmt(Fit().Max - Fit().Min); });
                p.AddReadout("Sphere radius", [this] { return Fmt(FitSphere().Radius); });
                p.AddReadout("Merged sphere radius", [this] { return Fmt(BoundingSphere::CreateMerged(FitSphere(), BoundingSphere(Vector3(1.5f, 0, 0), 0.4f)).Radius); });
                p.AddReadout("Sphere around the box (CreateFromBoundingBox)", [this] { return Fmt(BoundingSphere::CreateFromBoundingBox(Fit()).Radius); });
                p.AddReadout("Box around the sphere (CreateFromSphere)", [this] { return Fmt(BoundingBox::CreateFromSphere(FitSphere()).Max - BoundingBox::CreateFromSphere(FitSphere()).Min); });
                break;
            case Mode::Frustum:
                p.AddSlider("Camera yaw", -90.0f, 90.0f, aimYaw_, [this](float v) { aimYaw_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Field of view", 25.0f, 100.0f, fov_, [this](float v) { fov_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Far plane", 2.0f, 8.0f, far_, [this](float v) { far_ = v; }, "%.1f");
                p.AddToggle("Show the corners and the frustum's own sphere", true, [this](bool v) { showFrustumBounds_ = v; });
                p.AddReadout("GetCorners / CreateFromFrustum radius", [this] { return std::to_string(View().GetCorners().size()) + " corners / " + Fmt(BoundingSphere::CreateFromFrustum(View()).Radius); });
                p.AddHeading("BoundingFrustum.Contains(sphere) for 25 spheres");
                p.AddReadout("Fully inside", [this] { return std::to_string(Count(ContainmentType::Contains)); });
                p.AddReadout("Partly inside", [this] { return std::to_string(Count(ContainmentType::Intersects)); });
                p.AddReadout("Culled (outside)", [this] { return std::to_string(Count(ContainmentType::Disjoint)); });
                break;
            case Mode::Pick:
                p.AddHeading("Look at an object: your view ray is tested");
                p.AddReadout("Nearest hit", [this] { return pickName_; });
                p.AddReadout("Distance", [this] { return pickDist_ < 0 ? std::string("-") : Fmt(pickDist_) + " m"; });
                p.AddReadout("Tests run", [this] { return std::string("5 x Ray.Intersects"); });
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
        BuildStagePlinth(b, 0.0f, 0.0f, 0.3f);
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        if (mode_ == Mode::Pick && ctx.playerInRoom) {
            // The visitor's own view ray, tested against five objects with Ray.Intersects.
            const Ray ray(ctx.playerPosition, Vector3::Normalize(ctx.playerForward));
            pickName_ = "none";
            pickDist_ = -1.0f;
            for (int i = 0; i < 5; ++i) {
                std::optional<float> d;
                if (i % 2 == 0) d = ray.Intersects(PickBox(i));
                else d = ray.Intersects(PickSphere(i));
                if (d && (pickDist_ < 0 || *d < pickDist_)) {
                    pickDist_ = *d;
                    pickIndex_ = i;
                    pickName_ = std::string(i % 2 == 0 ? "box " : "sphere ") + std::to_string(i + 1);
                }
            }
            if (pickDist_ < 0) pickIndex_ = -1;
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        switch (mode_) {
            case Mode::BoxBox: {
                const BoundingBox a = A(), bb = Bb();
                gizmos_.Box(a, Color(190, 190, 200, 255));
                gizmos_.Box(bb, ForContainment(a.Contains(bb)));
                gizmos_.Label(Vec(a.Max) + Vector3(0, 0.15f, 0), "A", Color(220, 220, 230, 255));
                gizmos_.Label(Vec(bb.Max) + Vector3(0, 0.15f, 0), "B: " + std::string(Name(a.Contains(bb))), ForContainment(a.Contains(bb)));
                break;
            }
            case Mode::SphereBox: {
                gizmos_.Box(A(), Color(190, 190, 200, 255));
                const ContainmentType c = Sph().Contains(A());
                const Color col = Sph().Intersects(A()) ? Color(255, 110, 100, 255) : Color(110, 160, 255, 255);
                gizmos_.Sphere(Sph(), col);
                gizmos_.Label(Sph().Center + Vector3(0, Sph().Radius + 0.15f, 0), Sph().Intersects(A()) ? "overlapping" : "apart", col);
                (void)c;
                break;
            }
            case Mode::RayBox: DrawRay(ctx, &BoxTarget, nullptr, nullptr); break;
            case Mode::RaySphere: DrawRay(ctx, nullptr, &SphereTarget, nullptr); break;
            case Mode::RayPlane: DrawRay(ctx, nullptr, nullptr, &PlaneTarget); break;
            case Mode::PlaneBox: {
                const Plane pl = PlaneNow();
                gizmos_.PlaneQuad(pl, Vec(Vector3(0, my_, 0)), 2.6f, Color(120, 220, 255, 255));
                for (int i = 0; i < 5; ++i) {
                    const PlaneIntersectionType t = pl.Intersects(Row(i));
                    const Color c = t == PlaneIntersectionType::Front ? Color(110, 255, 140, 255) : (t == PlaneIntersectionType::Back ? Color(110, 160, 255, 255) : Color(255, 110, 100, 255));
                    gizmos_.Box(Row(i), c);
                    gizmos_.Label(Vec(Row(i).Max) + Vector3(0, 0.2f, 0), Name(t), c, 0.45f);
                }
                break;
            }
            case Mode::Merge: {
                for (int i = 0; i < count_; ++i) gizmos_.SolidSphere(ctx, Vec(cloud_[static_cast<std::size_t>(i)] * size_), 0.035f, kWhite);
                gizmos_.Box(Fit(), Color(255, 220, 90, 255));
                const BoundingSphere fs = FitSphere();
                gizmos_.Sphere(BoundingSphere(Vec(fs.Center), fs.Radius), Color(120, 200, 255, 255));
                const BoundingSphere extra(Vec(Vector3(1.5f, 0, 0)), 0.4f);
                gizmos_.Sphere(extra, Color(255, 140, 200, 255), 24);
                gizmos_.Sphere(BoundingSphere::CreateMerged(BoundingSphere(Vec(fs.Center), fs.Radius), extra), Color(150, 255, 150, 255), 40);
                const BoundingSphere around = BoundingSphere::CreateFromBoundingBox(Fit());
                gizmos_.Sphere(BoundingSphere(Vec(around.Center), around.Radius), Color(110, 255, 240, 120), 24);
                gizmos_.Label(Vec(Vector3(0, 1.3f, 0)), "yellow: CreateFromPoints box   blue: sphere   green: CreateMerged   cyan: sphere around the box", Color(230, 230, 240, 255), 0.45f);
                break;
            }
            case Mode::Frustum: DrawFrustum(ctx); break;
            case Mode::Pick: {
                for (int i = 0; i < 5; ++i) {
                    const Color c = i == pickIndex_ ? Color(255, 230, 90, 255) : Color(150, 190, 255, 255);
                    if (i % 2 == 0) gizmos_.Box(PickBox(i), c);
                    else gizmos_.Sphere(PickSphere(i), c, 28);
                    gizmos_.Label((i % 2 == 0 ? PickBox(i).Max : PickSphere(i).Center + Vector3(0, PickSphere(i).Radius, 0)) + Vector3(0, 0.15f, 0), std::to_string(i + 1), c, 0.5f);
                }
                break;
            }
        }
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        switch (mode_) {
            case Mode::BoxBox: return "BoundingBox vs BoundingBox";
            case Mode::SphereBox: return "BoundingSphere vs BoundingBox";
            case Mode::RayBox: return "Ray vs BoundingBox";
            case Mode::RaySphere: return "Ray vs BoundingSphere";
            case Mode::RayPlane: return "Ray vs Plane";
            case Mode::PlaneBox: return "Plane classification";
            case Mode::Merge: return "Fitting and merging bounds";
            case Mode::Frustum: return "BoundingFrustum culling";
            default: return "Picking with a view ray";
        }
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::BoxBox: return {"Axis-aligned boxes: Intersects says whether they touch, Contains says whether one is wholly", "inside the other (green), merely overlapping (red) or apart (blue). Slide box B through box A."};
            case Mode::SphereBox: return {"A sphere touches a box when the sphere's centre is closer to the box than its radius.", "Contains is stricter: the whole sphere has to be inside."};
            case Mode::RayBox: return {"A Ray has an origin and a direction. Ray.Intersects(BoundingBox) returns the distance to the entry", "point, or nothing. Aim the ray with the sliders; the cross marks the hit."};
            case Mode::RaySphere: return {"Ray.Intersects(BoundingSphere) solves a quadratic: no hit, tangent or two crossings; CNA returns", "the nearest. Aim the ray and watch the distance."};
            case Mode::RayPlane: return {"Ray.Intersects(Plane) returns the distance where the ray crosses the plane, or nothing when it is", "parallel or pointing away. Tilt the plane and aim the ray."};
            case Mode::PlaneBox: return {"A Plane splits space into a front and a back half. Plane.Intersects(BoundingBox) classifies a box", "as Front, Back or Intersecting. Slide and tilt the plane through the five boxes."};
            case Mode::Merge: return {"BoundingBox.CreateFromPoints and BoundingSphere.CreateFromPoints wrap a cloud of points;", "CreateMerged grows a bound to include another. Use fewer points to see the bounds shrink."};
            case Mode::Frustum: return {"A BoundingFrustum is the camera's view volume. Contains classifies each object as fully inside,", "partly inside or outside: the test behind frustum culling, which this museum itself uses for rooms."};
            default: return {"Mouse picking casts a ray through the screen point under the cursor and tests it against bounds.", "Here the ray is your view: look at an object and it lights up, with the distance to it."};
        }
    }

    [[nodiscard]] Vector3 Vec(const Vector3& local) const { return R().ToWorld(0, 1.3f, 0) + local; }
    [[nodiscard]] BoundingBox A() const { return BoundingBox(Vec(Vector3(-0.9f, -0.7f, -0.7f)), Vec(Vector3(0.9f, 0.7f, 0.7f))); }
    [[nodiscard]] BoundingBox Bb() const { return BoundingBox(Vec(Vector3(mx_ - size_ * 0.5f, my_ - size_ * 0.5f, -size_ * 0.5f)), Vec(Vector3(mx_ + size_ * 0.5f, my_ + size_ * 0.5f, size_ * 0.5f))); }
    [[nodiscard]] BoundingSphere Sph() const { return BoundingSphere(Vec(Vector3(mx_, my_, 0)), size_); }
    [[nodiscard]] Vector3 Origin() const { return Vec(Vector3(-2.4f, my_, 0)); }
    [[nodiscard]] Vector3 Dir() const
    {
        const float y = MathHelper::ToRadians(aimYaw_), p = MathHelper::ToRadians(aimPitch_);
        return Vector3(std::cos(p) * std::cos(y), std::sin(p), std::cos(p) * std::sin(y));
    }
    static BoundingBox BoxTarget(const CollisionLab& l) { return BoundingBox(l.Vec(Vector3(-0.5f, -0.5f, -0.5f)), l.Vec(Vector3(0.5f, 0.5f, 0.5f))); }
    static BoundingSphere SphereTarget(const CollisionLab& l) { return BoundingSphere(l.Vec(Vector3(0.3f, 0, 0)), 0.7f); }
    static Plane PlaneTarget(const CollisionLab& l)
    {
        const Vector3 n = Vector3::Normalize(Vector3(-std::cos(MathHelper::ToRadians(l.tilt_)), std::sin(MathHelper::ToRadians(l.tilt_)), 0.0f));
        return Plane(n, -Vector3::Dot(n, l.Vec(Vector3(0.6f, 0, 0))));
    }
    [[nodiscard]] std::optional<float> Cast() const
    {
        const Ray ray(Origin(), Dir());
        if (mode_ == Mode::RayBox) return ray.Intersects(BoxTarget(*this));
        if (mode_ == Mode::RaySphere) return ray.Intersects(SphereTarget(*this));
        return ray.Intersects(PlaneTarget(*this));
    }
    [[nodiscard]] Vector3 Hit(float d) const { return Origin() + Dir() * d; }

    void DrawRay(const DrawContext& ctx, BoundingBox (*box)(const CollisionLab&), BoundingSphere (*sph)(const CollisionLab&), Plane (*pl)(const CollisionLab&))
    {
        const std::optional<float> d = Cast();
        if (box != nullptr) gizmos_.Box(box(*this), d ? Color(255, 110, 100, 255) : Color(160, 190, 255, 255));
        if (sph != nullptr) gizmos_.Sphere(sph(*this), d ? Color(255, 110, 100, 255) : Color(160, 190, 255, 255));
        if (pl != nullptr) gizmos_.PlaneQuad(pl(*this), Vec(Vector3(0.6f, 0, 0)), 1.6f, d ? Color(255, 110, 100, 255) : Color(160, 190, 255, 255));
        gizmos_.SolidSphere(ctx, Origin(), 0.06f, kBlue);
        const float len = d ? *d : 6.0f;
        gizmos_.Arrow(Origin(), Origin() + Dir() * len, d ? Color(255, 230, 90, 255) : Color(150, 150, 170, 255), 0.12f);
        if (d) {
            gizmos_.SolidSphere(ctx, Hit(*d), 0.08f, kGold);
            gizmos_.Label(Hit(*d) + Vector3(0, 0.2f, 0), "hit " + Fmt(*d) + " m", Color(255, 240, 150, 255), 0.5f);
        }
        gizmos_.Label(Origin() + Vector3(0, 0.2f, 0), "ray origin", ToColor(kBlue * 1.3f), 0.45f);
    }

    [[nodiscard]] BoundingBox Row(int i) const
    {
        const float x = -2.0f + static_cast<float>(i) * 1.0f;
        return BoundingBox(Vec(Vector3(x - 0.3f, -0.4f, -0.3f)), Vec(Vector3(x + 0.3f, 0.4f, 0.3f)));
    }
    [[nodiscard]] Plane PlaneNow() const
    {
        const Vector3 n = Vector3::Normalize(Vector3(std::sin(MathHelper::ToRadians(tilt_)), std::cos(MathHelper::ToRadians(tilt_)), 0.0f));
        return Plane(n, -Vector3::Dot(n, Vec(Vector3(0, my_, 0))));
    }
    [[nodiscard]] std::vector<Vector3> Points() const
    {
        std::vector<Vector3> p;
        for (int i = 0; i < count_; ++i) p.push_back(Vec(cloud_[static_cast<std::size_t>(i)] * size_));
        return p;
    }
    [[nodiscard]] BoundingBox Fit() const { return BoundingBox::CreateFromPoints(Points()); }
    [[nodiscard]] BoundingSphere FitSphere() const { return BoundingSphere::CreateFromPoints(Points()); }

    [[nodiscard]] BoundingFrustum View() const
    {
        const Vector3 eye = Vec(Vector3(-2.6f, 0.2f, 0.0f));
        const float y = MathHelper::ToRadians(aimYaw_);
        const Vector3 dir(std::cos(y), 0.0f, std::sin(y));
        return BoundingFrustum(Matrix::CreateLookAt(eye, eye + dir, Vector3::Up) *
                               Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(fov_), 1.6f, 0.3f, far_));
    }
    [[nodiscard]] BoundingSphere Ball(int i) const
    {
        const int cx = i % 5, cz = i / 5;
        return BoundingSphere(Vec(Vector3(static_cast<float>(cx) * 0.9f - 1.2f, 0.2f, static_cast<float>(cz) * 0.9f - 1.8f)), 0.22f);
    }
    [[nodiscard]] int Count(ContainmentType t) const
    {
        const BoundingFrustum f = View();
        int n = 0;
        for (int i = 0; i < 25; ++i) n += f.Contains(Ball(i)) == t ? 1 : 0;
        return n;
    }
    void DrawFrustum(const DrawContext& ctx)
    {
        const BoundingFrustum f = View();
        gizmos_.Frustum(f, Color(255, 200, 90, 255));
        if (showFrustumBounds_) {
            const std::vector<Vector3> corners = f.GetCorners();
            for (std::size_t i = 0; i < corners.size(); ++i) gizmos_.SolidSphere(ctx, corners[i], 0.045f, kGold);
            gizmos_.Sphere(BoundingSphere::CreateFromFrustum(f), Color(255, 160, 220, 140), 32);
        }
        gizmos_.SolidSphere(ctx, Vec(Vector3(-2.6f, 0.2f, 0.0f)), 0.08f, kRed);
        for (int i = 0; i < 25; ++i) {
            const ContainmentType c = f.Contains(Ball(i));
            const Vector3 col = c == ContainmentType::Contains ? kGreen : (c == ContainmentType::Intersects ? kGold : kBlue * 0.55f);
            gizmos_.SolidSphere(ctx, Ball(i).Center, Ball(i).Radius, col);
        }
    }

    [[nodiscard]] BoundingBox PickBox(int i) const
    {
        const float x = -2.2f + static_cast<float>(i) * 1.1f, y = (i % 2 == 0 ? 0.0f : 0.3f);
        return BoundingBox(Vec(Vector3(x - 0.35f, y - 0.35f, -0.35f)), Vec(Vector3(x + 0.35f, y + 0.35f, 0.35f)));
    }
    [[nodiscard]] BoundingSphere PickSphere(int i) const { return BoundingSphere(Vec(Vector3(-2.2f + static_cast<float>(i) * 1.1f, 0.3f, 0)), 0.4f); }

    Mode mode_;
    Gizmos gizmos_;
    std::vector<Vector3> cloud_;
    float time_ = 0.0f;
    float mx_ = 0.9f, my_ = 0.3f, size_ = 0.8f;
    float aimYaw_ = 0.0f, aimPitch_ = 0.0f, tilt_ = 20.0f;
    int count_ = 40;
    float fov_ = 55.0f, far_ = 5.0f;
    bool showFrustumBounds_ = true;
    std::string pickName_ = "none";
    float pickDist_ = -1.0f;
    int pickIndex_ = -1;
};

}  // namespace

std::unique_ptr<Exhibit> MakeCollisionLab(const RoomDef& room, Services& services)
{
    return std::make_unique<CollisionLab>(room, services);
}
