// Simulation laboratory: small physical and behavioural simulations built from Vector3 / Matrix maths and drawn with
// BasicEffect. Four rooms: flocking (boids), bouncing balls with sphere collisions, four numerical integrators on
// the same spring, and two-bone inverse kinematics.
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/Gizmos.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Flock, Balls, Integrators, Ik };

Mode ParseMode(const std::string& s)
{
    if (s == "balls") return Mode::Balls;
    if (s == "integrators") return Mode::Integrators;
    if (s == "ik") return Mode::Ik;
    return Mode::Flock;
}

class SimLab final : public Exhibit {
public:
    SimLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Flock:
                p.AddSlider("Separation", 0.0f, 3.0f, wSep_, [this](float v) { wSep_ = v; });
                p.AddSlider("Alignment", 0.0f, 3.0f, wAli_, [this](float v) { wAli_ = v; });
                p.AddSlider("Cohesion", 0.0f, 3.0f, wCoh_, [this](float v) { wCoh_ = v; });
                p.AddSlider("Neighbour radius", 0.5f, 3.0f, radius_, [this](float v) { radius_ = v; });
                p.AddToggle("Flee from the visitor", true, [this](bool v) { flee_ = v; });
                p.AddReadout("Boids", [this] { return std::to_string(boids_.size()); });
                p.AddReadout("Average speed", [this] { return Fmt(avgSpeed_) + " m/s"; });
                p.AddReadout("Neighbours per boid", [this] { return Fmt(avgNeighbours_); });
                break;
            case Mode::Balls:
                p.AddSlider("Gravity", 0.0f, 20.0f, gravity_, [this](float v) { gravity_ = v; });
                p.AddSlider("Restitution (bounciness)", 0.1f, 1.0f, restitution_, [this](float v) { restitution_ = v; });
                p.AddSlider("Air drag", 0.0f, 1.0f, drag_, [this](float v) { drag_ = v; });
                p.AddButton("Kick every ball", [this] { Kick(); });
                p.AddButton("Reset", [this] { ResetBalls(); });
                p.AddReadout("Ball-ball collisions", [this] { return std::to_string(collisions_); });
                p.AddReadout("Kinetic + potential energy", [this] { return Fmt(Energy()) + " J (per kg)"; });
                break;
            case Mode::Integrators:
                p.AddSlider("Time step", 0.005f, 0.12f, step_, [this](float v) { step_ = v; ResetSprings(); }, "%.3f s");
                p.AddButton("Restart", [this] { ResetSprings(); });
                for (int i = 0; i < 4; ++i) p.AddReadout(kNames[i], [this, i] { return "energy " + Fmt(100.0f * springs_[static_cast<std::size_t>(i)].Energy() / initialEnergy_, "%") ; });
                p.AddHeading("A spring of k = 40, m = 1: the exact energy stays 100 %");
                break;
            case Mode::Ik:
                p.AddSlider("Target X", -2.5f, 2.5f, target_.X, [this](float v) { target_.X = v; auto_ = false; });
                p.AddSlider("Target Y", 0.2f, 3.8f, target_.Y, [this](float v) { target_.Y = v; auto_ = false; });
                p.AddSlider("Target Z", -2.0f, 2.0f, target_.Z, [this](float v) { target_.Z = v; auto_ = false; });
                p.AddSlider("Upper arm", 0.6f, 1.8f, upper_, [this](float v) { upper_ = v; });
                p.AddSlider("Forearm", 0.6f, 1.8f, lower_, [this](float v) { lower_ = v; });
                p.AddSlider("Elbow points", -180.0f, 180.0f, poleAngle_, [this](float v) { poleAngle_ = v; }, "%.0f deg", 1.0f);
                p.AddToggle("Move the target on its own", true, [this](bool v) { auto_ = v; });
                p.AddReadout("Reach", [this] { return reachNote_; });
                p.AddReadout("Shoulder / elbow angle", [this] { return Fmt(MathHelper::ToDegrees(shoulderAngle_)) + " / " + Fmt(MathHelper::ToDegrees(elbowAngle_)) + " deg"; });
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        if (mode_ == Mode::Flock) ResetFlock();
        if (mode_ == Mode::Balls) ResetBalls();
        if (mode_ == Mode::Integrators) ResetSprings();
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        const float dt = std::min(ctx.dt, 0.05f);
        time_ += dt;
        visitor_ = ctx.playerInRoom ? Local(ctx.playerPosition) : Vector3(0, -50, 0);
        switch (mode_) {
            case Mode::Flock: StepFlock(dt); break;
            case Mode::Balls: StepBalls(dt); break;
            case Mode::Integrators: StepSprings(dt); break;
            case Mode::Ik:
                if (auto_) target_ = Vector3(1.5f * std::sin(time_ * 0.7f), 2.1f + 1.3f * std::sin(time_ * 0.45f), 1.4f * std::cos(time_ * 0.6f));
                SolveIk();
                break;
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        switch (mode_) {
            case Mode::Flock: DrawFlock(ctx); break;
            case Mode::Balls: DrawBalls(ctx); break;
            case Mode::Integrators: DrawSprings(ctx); break;
            case Mode::Ik: DrawIk(ctx); break;
        }
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    static constexpr const char* kNames[4] = {"Explicit Euler", "Semi-implicit Euler", "Velocity Verlet", "Runge-Kutta 4"};

    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Flocking", "Bouncing balls", "Numerical integrators", "Two-bone inverse kinematics"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Flock: return {"Three simple rules per bird give a flock: steer away from close neighbours (separation), towards their average", "heading (alignment) and towards their centre (cohesion). The sliders weigh the rules; step towards the flock and it scatters."};
            case Mode::Balls: return {"Each ball integrates gravity, bounces off the walls and floor with a restitution factor, and collides with others when", "BoundingSphere.Intersects says they overlap (an impulse along the line of centres). Energy shrinks with every bounce."};
            case Mode::Integrators: return {"The same spring, the same start, four ways to step it. Explicit Euler gains energy and blows up, semi-implicit Euler and", "Verlet keep it bounded, RK4 is the most accurate. Raise the time step and watch the first one explode."};
            default: return {"Given a target, a two-bone arm's elbow follows from the law of cosines: the distance fixes the angle between the bones.", "Move the target out of reach and the arm stretches straight. The pole angle chooses which way the elbow bends."};
        }
    }
    [[nodiscard]] Vector3 Local(const Vector3& world) const { return R().Rotate(world - R().centre); }
    [[nodiscard]] Vector3 W(const Vector3& local) const { return R().ToWorld(local.X, local.Y, local.Z); }

    void Solid(const DrawContext& ctx, const LitMesh& mesh, const Matrix& world, const Vector3& colour)
    {
        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPunctualLightEXT(PunctualLightEXT{});
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setWorldProperty(world);
        fx.setDiffuseColorProperty(colour);
        fx.setEmissiveColorProperty(colour * 0.08f);
        fx.setSpecularColorProperty(Vector3(0.3f, 0.3f, 0.3f));
        EffectBank::Apply(fx);
        mesh.Draw(ctx.device);
    }
    void Bar(const DrawContext& ctx, const Vector3& from, const Vector3& to, float thick, const Vector3& colour)
    {
        const Vector3 d = to - from;
        const float len = d.Length();
        if (len < 1e-4f) return;
        Solid(ctx, S().shapes->Cylinder(), Matrix::CreateScale(thick, len, thick) * AlignYTo(d / len) * Matrix::CreateTranslation(from), colour);
    }

    // ---- flocking -------------------------------------------------------------------------------
    struct Boid { Vector3 p, v; };
    static constexpr float kHalfX = 3.4f, kHalfZ = 3.0f, kFloor = 0.5f, kCeil = 3.6f;
    void ResetFlock()
    {
        std::mt19937 rng(11);
        std::uniform_real_distribution<float> u(-1.0f, 1.0f);
        boids_.clear();
        for (int i = 0; i < 70; ++i) boids_.push_back({Vector3(u(rng) * kHalfX, 2.0f + u(rng), u(rng) * kHalfZ), Vector3(u(rng), u(rng) * 0.3f, u(rng)) * 1.2f});
    }
    void StepFlock(float dt)
    {
        const float r2 = radius_ * radius_;
        std::vector<Vector3> acc(boids_.size());
        float speedSum = 0.0f;
        long long neighbours = 0;
        for (std::size_t i = 0; i < boids_.size(); ++i) {
            Vector3 sep, ali, coh;
            int n = 0;
            for (std::size_t j = 0; j < boids_.size(); ++j) {
                if (i == j) continue;
                const Vector3 d = boids_[i].p - boids_[j].p;
                const float dist2 = d.LengthSquared();
                if (dist2 > r2) continue;
                ++n;
                sep += d / std::max(0.05f, dist2);
                ali += boids_[j].v;
                coh += boids_[j].p;
            }
            Vector3 a;
            if (n > 0) {
                const float inv = 1.0f / static_cast<float>(n);
                a += sep * wSep_ * 0.4f;
                a += (ali * inv - boids_[i].v) * wAli_;
                a += (coh * inv - boids_[i].p) * wCoh_ * 0.6f;
            }
            // Stay in the box: steer back from each wall.
            const Vector3& p = boids_[i].p;
            if (p.X > kHalfX) a.X -= (p.X - kHalfX) * 4.0f; else if (p.X < -kHalfX) a.X -= (p.X + kHalfX) * 4.0f;
            if (p.Z > kHalfZ) a.Z -= (p.Z - kHalfZ) * 4.0f; else if (p.Z < -kHalfZ) a.Z -= (p.Z + kHalfZ) * 4.0f;
            if (p.Y > kCeil) a.Y -= (p.Y - kCeil) * 4.0f; else if (p.Y < kFloor) a.Y -= (p.Y - kFloor) * 4.0f;
            if (flee_) {
                const Vector3 away = boids_[i].p - (visitor_ + Vector3(0, 1.0f, 0));
                const float d2 = away.LengthSquared();
                if (d2 < 9.0f) a += away / std::max(0.2f, d2) * 6.0f;
            }
            acc[i] = a;
            neighbours += n;
        }
        for (std::size_t i = 0; i < boids_.size(); ++i) {
            Boid& b = boids_[i];
            b.v += acc[i] * dt;
            const float sp = b.v.Length();
            if (sp > 2.6f) b.v *= 2.6f / sp;
            if (sp < 0.9f && sp > 1e-4f) b.v *= 0.9f / sp;
            b.p += b.v * dt;
            speedSum += b.v.Length();
        }
        avgSpeed_ = speedSum / static_cast<float>(boids_.size());
        avgNeighbours_ = static_cast<float>(neighbours) / static_cast<float>(boids_.size());
    }
    void DrawFlock(const DrawContext& ctx)
    {
        const Vector3 centre = W(Vector3(0, 0, -0.5f));
        gizmos_.Box(BoundingBox(centre + Off(-kHalfX, kFloor, -kHalfZ), centre + Off(kHalfX, kCeil, kHalfZ)), Color(90, 110, 150, 255));
        for (const Boid& b : boids_) {
            const Vector3 dir = b.v.LengthSquared() > 1e-6f ? Vector3::Normalize(R().Rotate(b.v)) : Vector3::Up;
            const float hue = 0.5f + 0.5f * std::sin(b.p.X * 0.7f + b.p.Z * 0.5f);
            Solid(ctx, S().shapes->Cone(), Matrix::CreateScale(0.14f, 0.34f, 0.14f) * AlignYTo(dir) * Matrix::CreateTranslation(centre + R().Rotate(b.p)),
                  Vector3(0.95f - 0.5f * hue, 0.55f + 0.3f * hue, 0.35f + 0.6f * hue));
        }
    }

    // ---- bouncing balls -------------------------------------------------------------------------
    struct Ball { Vector3 p, v; float r; Vector3 colour; };
    static constexpr float kBoxX = 3.6f, kBoxZ = 2.8f, kBoxY = 4.0f;
    void ResetBalls()
    {
        std::mt19937 rng(5);
        std::uniform_real_distribution<float> u(-1.0f, 1.0f);
        balls_.clear();
        for (int i = 0; i < 11; ++i) {
            const float r = 0.25f + 0.12f * static_cast<float>(i % 3);
            balls_.push_back({Vector3(u(rng) * (kBoxX - 0.5f), 1.5f + 2.0f * std::abs(u(rng)), u(rng) * (kBoxZ - 0.5f)), Vector3(u(rng) * 3.0f, 0.0f, u(rng) * 3.0f), r,
                              Vector3(0.5f + 0.5f * std::sin(static_cast<float>(i) * 1.1f), 0.5f + 0.5f * std::sin(static_cast<float>(i) * 1.7f + 2.0f), 0.5f + 0.5f * std::sin(static_cast<float>(i) * 0.7f + 4.0f))});
        }
        collisions_ = 0;
    }
    void Kick()
    {
        std::mt19937 rng(static_cast<unsigned>(time_ * 1000.0f));
        std::uniform_real_distribution<float> u(-1.0f, 1.0f);
        for (Ball& b : balls_) b.v += Vector3(u(rng) * 4.0f, 3.0f + 3.0f * std::abs(u(rng)), u(rng) * 4.0f);
    }
    [[nodiscard]] float Energy() const
    {
        float e = 0.0f;
        for (const Ball& b : balls_) e += 0.5f * b.v.LengthSquared() + gravity_ * b.p.Y;
        return balls_.empty() ? 0.0f : e / static_cast<float>(balls_.size());
    }
    void StepBalls(float dt)
    {
        const int sub = 4;
        const float h = dt / static_cast<float>(sub);
        for (int s = 0; s < sub; ++s) {
            for (Ball& b : balls_) {
                b.v.Y -= gravity_ * h;
                b.v *= 1.0f - drag_ * h;
                b.p += b.v * h;
                if (b.p.Y < b.r) { b.p.Y = b.r; if (b.v.Y < 0) b.v.Y = -b.v.Y * restitution_; b.v.X *= 0.995f; b.v.Z *= 0.995f; }
                if (b.p.Y > kBoxY - b.r) { b.p.Y = kBoxY - b.r; if (b.v.Y > 0) b.v.Y = -b.v.Y * restitution_; }
                if (b.p.X > kBoxX - b.r) { b.p.X = kBoxX - b.r; if (b.v.X > 0) b.v.X = -b.v.X * restitution_; }
                if (b.p.X < -kBoxX + b.r) { b.p.X = -kBoxX + b.r; if (b.v.X < 0) b.v.X = -b.v.X * restitution_; }
                if (b.p.Z > kBoxZ - b.r) { b.p.Z = kBoxZ - b.r; if (b.v.Z > 0) b.v.Z = -b.v.Z * restitution_; }
                if (b.p.Z < -kBoxZ + b.r) { b.p.Z = -kBoxZ + b.r; if (b.v.Z < 0) b.v.Z = -b.v.Z * restitution_; }
            }
            for (std::size_t i = 0; i < balls_.size(); ++i)
                for (std::size_t j = i + 1; j < balls_.size(); ++j) {
                    if (!BoundingSphere(balls_[i].p, balls_[i].r).Intersects(BoundingSphere(balls_[j].p, balls_[j].r))) continue;
                    Vector3 n = balls_[j].p - balls_[i].p;
                    const float dist = n.Length();
                    if (dist < 1e-5f) continue;
                    n /= dist;
                    const float overlap = balls_[i].r + balls_[j].r - dist;
                    const float mi = balls_[i].r * balls_[i].r * balls_[i].r, mj = balls_[j].r * balls_[j].r * balls_[j].r;
                    balls_[i].p -= n * (overlap * mj / (mi + mj));
                    balls_[j].p += n * (overlap * mi / (mi + mj));
                    const float rel = Vector3::Dot(balls_[j].v - balls_[i].v, n);
                    if (rel >= 0.0f) continue;
                    const float j1 = -(1.0f + restitution_) * rel / (1.0f / mi + 1.0f / mj);
                    balls_[i].v -= n * (j1 / mi);
                    balls_[j].v += n * (j1 / mj);
                    ++collisions_;
                }
        }
    }
    void DrawBalls(const DrawContext& ctx)
    {
        const Vector3 centre = W(Vector3(0, 0, -0.5f));
        gizmos_.Box(BoundingBox(centre + Off(-kBoxX, 0, -kBoxZ), centre + Off(kBoxX, kBoxY, kBoxZ)), Color(90, 110, 150, 255));
        for (const Ball& b : balls_) Solid(ctx, S().shapes->Sphere(), Matrix::CreateScale(b.r * 2.0f) * Matrix::CreateTranslation(centre + R().Rotate(b.p)), b.colour);
    }

    // ---- integrators ----------------------------------------------------------------------------
    struct Spring {
        int method = 0;
        float y = 1.0f, v = 0.0f, a = 0.0f;   // displacement from rest, velocity, acceleration
        static constexpr float k = 40.0f, m = 1.0f;
        [[nodiscard]] float Energy() const { return 0.5f * m * v * v + 0.5f * k * y * y; }
        void Step(float h)
        {
            const auto acc = [](float x) { return -k / m * x; };
            switch (method) {
                case 0: { const float an = acc(y); y += v * h; v += an * h; break; }                                  // explicit Euler
                case 1: { v += acc(y) * h; y += v * h; break; }                                                          // semi-implicit Euler
                case 2: { const float a0 = acc(y); y += v * h + 0.5f * a0 * h * h; const float a1 = acc(y); v += 0.5f * (a0 + a1) * h; break; }   // velocity Verlet
                default: {                                                                                               // RK4
                    const float k1y = v, k1v = acc(y);
                    const float k2y = v + 0.5f * h * k1v, k2v = acc(y + 0.5f * h * k1y);
                    const float k3y = v + 0.5f * h * k2v, k3v = acc(y + 0.5f * h * k2y);
                    const float k4y = v + h * k3v, k4v = acc(y + h * k3y);
                    y += h / 6.0f * (k1y + 2 * k2y + 2 * k3y + k4y);
                    v += h / 6.0f * (k1v + 2 * k2v + 2 * k3v + k4v);
                    break;
                }
            }
        }
    };
    void ResetSprings()
    {
        for (int i = 0; i < 4; ++i) springs_[static_cast<std::size_t>(i)] = Spring{i, 1.0f, 0.0f, 0.0f};
        initialEnergy_ = springs_[0].Energy();
        simTime_ = 0.0f;
        leftover_ = 0.0f;
    }
    void StepSprings(float dt)
    {
        leftover_ += dt;
        int guard = 0;
        while (leftover_ >= step_ && guard++ < 400) {
            for (Spring& s : springs_) s.Step(step_);
            leftover_ -= step_;
            simTime_ += step_;
        }
    }
    void DrawSprings(const DrawContext& ctx)
    {
        const Vector3 top = W(Vector3(0, 4.0f, -0.5f));
        const float omega = std::sqrt(Spring::k / Spring::m);
        for (int i = 0; i < 4; ++i) {
            const float x = -3.0f + static_cast<float>(i) * 2.0f;
            const Spring& s = springs_[static_cast<std::size_t>(i)];
            const float yy = std::clamp(s.y, -3.0f, 3.0f);
            const Vector3 anchor = top + Off(x, 0, 0);
            const Vector3 mass = anchor + Vector3(0, -2.0f + yy, 0);
            Bar(ctx, anchor, mass, 0.03f, Vector3(0.75f, 0.75f, 0.8f));
            Solid(ctx, S().shapes->Sphere(), Matrix::CreateScale(0.45f) * Matrix::CreateTranslation(mass), i == 0 ? Vector3(0.95f, 0.35f, 0.3f) : (i == 1 ? Vector3(0.95f, 0.8f, 0.3f) : (i == 2 ? Vector3(0.35f, 0.85f, 0.45f) : Vector3(0.35f, 0.6f, 0.95f))));
            // The exact motion as a ghost ring.
            gizmos_.Circle(anchor + Vector3(0, -2.0f + std::cos(omega * simTime_), 0), Vector3(0, 0, 1), 0.27f, Color(240, 240, 255, 180));
            gizmos_.Label(anchor + Vector3(0, -4.4f, 0), kNames[i], Color(255, 230, 150, 255), 0.5f);
            gizmos_.Label(anchor + Vector3(0, -4.75f, 0), Fmt(100.0f * s.Energy() / initialEnergy_, " %"), Color(200, 235, 255, 255), 0.55f);
        }
        gizmos_.Line(top + Off(-3.8f, 0, 0), top + Off(3.8f, 0, 0), Color(180, 180, 200, 255));
    }

    // ---- two-bone IK ----------------------------------------------------------------------------
    void SolveIk()
    {
        const Vector3 shoulder(0.0f, 2.0f, -1.5f);
        const Vector3 toTarget = target_ - shoulder;
        const float reach = upper_ + lower_;
        float dist = toTarget.Length();
        const bool unreachable = dist > reach - 0.001f;
        const bool tooClose = dist < std::abs(upper_ - lower_) + 0.001f;
        dist = std::clamp(dist, std::abs(upper_ - lower_) + 0.001f, reach - 0.001f);
        reachNote_ = unreachable ? "out of reach: the arm stretches" : (tooClose ? "too close: the arm folds" : "reachable");
        const Vector3 dir = toTarget.LengthSquared() > 1e-6f ? Vector3::Normalize(toTarget) : Vector3::Forward;
        // Law of cosines: the angle at the shoulder between the upper arm and the line to the target.
        const float cosShoulder = std::clamp((upper_ * upper_ + dist * dist - lower_ * lower_) / (2.0f * upper_ * dist), -1.0f, 1.0f);
        const float cosElbow = std::clamp((upper_ * upper_ + lower_ * lower_ - dist * dist) / (2.0f * upper_ * lower_), -1.0f, 1.0f);
        shoulderAngle_ = std::acos(cosShoulder);
        elbowAngle_ = std::acos(cosElbow);
        // The elbow lies in the plane that contains the line to the target and the pole direction.
        const float pa = MathHelper::ToRadians(poleAngle_);
        Vector3 pole = Vector3(std::sin(pa), std::cos(pa), 0.0f);
        pole -= dir * Vector3::Dot(pole, dir);
        if (pole.LengthSquared() < 1e-6f) pole = Vector3::Cross(dir, Vector3::Right);
        pole = Vector3::Normalize(pole);
        shoulder_ = shoulder;
        elbow_ = shoulder + (dir * std::cos(shoulderAngle_) + pole * std::sin(shoulderAngle_)) * upper_;
        hand_ = shoulder + dir * dist;
    }
    void DrawIk(const DrawContext& ctx)
    {
        const Vector3 base = W(Vector3(0, 0, 0));
        const auto P = [&](const Vector3& local) { return base + R().Rotate(local); };
        Solid(ctx, S().shapes->Cylinder(), Matrix::CreateScale(0.5f, 1.9f, 0.5f) * Matrix::CreateTranslation(P(Vector3(0, 0, -1.5f))), Vector3(0.4f, 0.42f, 0.5f));
        Bar(ctx, P(shoulder_), P(elbow_), 0.2f, Vector3(0.9f, 0.55f, 0.25f));
        Bar(ctx, P(elbow_), P(hand_), 0.16f, Vector3(0.95f, 0.75f, 0.3f));
        for (const Vector3& j : {shoulder_, elbow_, hand_}) Solid(ctx, S().shapes->Sphere(), Matrix::CreateScale(0.3f) * Matrix::CreateTranslation(P(j)), Vector3(0.3f, 0.35f, 0.45f));
        gizmos_.SolidSphere(ctx, P(target_), 0.1f, Vector3(0.9f, 0.2f, 0.25f));
        gizmos_.Circle(P(shoulder_), R().Rotate(Vector3(0, 0, 1)), upper_ + lower_, Color(120, 130, 170, 120), 48);
        gizmos_.Label(P(target_) + Vector3(0, 0.3f, 0), "target", Color(255, 150, 150, 255), 0.5f);
        gizmos_.Label(P(elbow_) + Vector3(0, 0.35f, 0), "elbow", Color(255, 220, 150, 255), 0.5f);
    }

    Mode mode_;
    Gizmos gizmos_;
    float time_ = 0.0f;
    Vector3 visitor_{0, -50, 0};
    // flock
    std::vector<Boid> boids_;
    float wSep_ = 1.4f, wAli_ = 1.0f, wCoh_ = 1.0f, radius_ = 1.5f, avgSpeed_ = 0.0f, avgNeighbours_ = 0.0f;
    bool flee_ = true;
    // balls
    std::vector<Ball> balls_;
    float gravity_ = 9.8f, restitution_ = 0.8f, drag_ = 0.05f;
    int collisions_ = 0;
    // integrators
    std::array<Spring, 4> springs_{};
    float step_ = 0.04f, initialEnergy_ = 1.0f, simTime_ = 0.0f, leftover_ = 0.0f;
    // ik
    Vector3 target_{1.0f, 2.0f, 0.8f}, shoulder_, elbow_, hand_;
    float upper_ = 1.3f, lower_ = 1.2f, poleAngle_ = 0.0f, shoulderAngle_ = 0.0f, elbowAngle_ = 0.0f;
    bool auto_ = true;
    std::string reachNote_ = "-";
};

}  // namespace

std::unique_ptr<Exhibit> MakeSimLab(const RoomDef& room, Services& services)
{
    return std::make_unique<SimLab>(room, services);
}
