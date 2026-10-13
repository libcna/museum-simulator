#include <cmath>
#include <string>

#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SignFactory.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const Color kGold(255, 226, 150, 255);
const Color kText(222, 228, 240, 255);

constexpr float kArmX = 3.0f, kArmZ = -3.0f;
constexpr float kOrreryX = -3.0f, kOrreryZ = -3.0f;
constexpr float kOrreryHeight = 0.9f;
constexpr float kUpperLen = 1.3f, kForeLen = 1.0f;

const float kOrrerySpeeds[3] = {1.0f, 4.0f, 0.25f};
const char* const kOrreryNames[3] = {"1x", "4x", "0.25x"};

class AnimationRoom final : public Exhibit {
public:
    AnimationRoom(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        b.AddBox(Vector3(kArmX - 0.5f, 0.0f, kArmZ - 0.5f), Vector3(kArmX + 0.5f, 0.35f, kArmZ + 0.5f), "metal", 1.0f, Vector3(0.6f, 0.65f, 0.7f));
        b.AddPedestal(kOrreryX, kOrreryZ, 0.9f, kOrreryHeight, "marble", Vector3(0.9f, 0.92f, 0.95f));

        SignSpec arm;
        arm.lines = {{"Robot arm", true, kGold}, {"world = local * parentWorld, four joints deep", false, kText}};
        b.AddWallSign(Wall::North, kArmX, 3.4f, 3.8f, 0.9f, arm);
        SignSpec orrery;
        orrery.lines = {{"Orrery", true, kGold}, {"moons orbit planets that orbit the sun: nested matrices", false, kText}};
        b.AddWallSign(Wall::North, kOrreryX, 3.4f, 3.8f, 0.9f, orrery);
        SignSpec title;
        title.lines = {{"Matrix hierarchies", true, kGold},
                       {"A child's transform is composed with its parent's every frame,", false, kText},
                       {"so moving the shoulder moves the whole arm.", false, kText}};
        b.AddWallSign(Wall::East, 0.0f, 3.6f, 4.6f, 1.2f, title);
        b.AddLight(Vector3(kArmX, 4.2f, kArmZ), Vector3(0.9f, 0.95f, 1.0f), 0.6f, 8.0f);
        b.AddLight(Vector3(kOrreryX, 4.2f, kOrreryZ), Vector3(1.0f, 0.95f, 0.85f), 0.6f, 8.0f);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        Interactable arm;
        arm.bounds = BoundingBox(R().ToWorld(kArmX - 1.2f, 0.0f, kArmZ - 1.2f), R().ToWorld(kArmX + 1.2f, 3.0f, kArmZ + 1.2f));
        arm.maxDistance = 5.0f;
        arm.prompt = [this] { return std::string(armRunning_ ? "Freeze the arm" : "Move the arm"); };
        arm.activate = [this] { armRunning_ = !armRunning_; };
        interaction.Add(std::move(arm));

        Interactable orrery;
        orrery.bounds = BoundingBox(R().ToWorld(kOrreryX - 1.6f, 0.0f, kOrreryZ - 1.6f), R().ToWorld(kOrreryX + 1.6f, 3.0f, kOrreryZ + 1.6f));
        orrery.maxDistance = 5.0f;
        orrery.prompt = [this] { return std::string("Orrery speed ") + kOrreryNames[speed_] + " (next: " + kOrreryNames[(speed_ + 1) % 3] + ")"; };
        orrery.activate = [this] { speed_ = (speed_ + 1) % 3; };
        interaction.Add(std::move(orrery));
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        if (armRunning_) armTime_ += ctx.dt;
        orreryTime_ += ctx.dt * kOrrerySpeeds[speed_];
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        GraphicsDevice& device = ctx.device;
        RenderStates::SetOpaque3D(device);
        BasicEffect& fx = ctx.fx.BeginLit(S().textures->Get("metal"));
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setSpecularColorProperty(Vector3(0.5f, 0.5f, 0.5f));
        fx.setSpecularPowerProperty(40.0f);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        DrawArm(ctx, fx);
        DrawOrrery(ctx, fx);
    }

private:
    void Part(GraphicsDevice& device, BasicEffect& fx, const LitMesh& mesh, const Matrix& world, const Vector3& colour, const Vector3& emissive = Vector3(0, 0, 0))
    {
        fx.setDiffuseColorProperty(colour);
        fx.setEmissiveColorProperty(emissive);
        fx.setWorldProperty(world);
        EffectBank::Apply(fx);
        mesh.Draw(device);
    }

    void DrawArm(const DrawContext& ctx, BasicEffect& fx)
    {
        GraphicsDevice& device = ctx.device;
        const float t = armTime_;
        const float yaw = std::sin(t * 0.5f) * 1.2f;
        const float shoulder = 0.7f + std::sin(t * 0.9f) * 0.4f;
        const float elbow = 0.9f + std::sin(t * 1.3f + 1.0f) * 0.6f;
        const float grip = 0.25f + 0.2f * std::sin(t * 2.4f);

        // Each joint matrix is (rotation * offset to the joint) * parent joint: the hierarchy.
        const Matrix baseJoint = Matrix::CreateTranslation(R().ToWorld(kArmX, 0.35f, kArmZ));
        const Matrix shoulderJoint = Matrix::CreateRotationY(yaw) * baseJoint;
        const Matrix upperJoint = Matrix::CreateRotationZ(shoulder) * Matrix::CreateTranslation(0, 0.25f, 0) * shoulderJoint;
        const Matrix elbowJoint = Matrix::CreateRotationZ(-elbow) * Matrix::CreateTranslation(0, kUpperLen, 0) * upperJoint;
        const Matrix wristJoint = Matrix::CreateTranslation(0, kForeLen, 0) * elbowJoint;

        Part(device, fx, S().shapes->Cylinder(), Matrix::CreateScale(0.7f, 0.25f, 0.7f) * shoulderJoint, Vector3(0.35f, 0.4f, 0.5f));
        Part(device, fx, S().shapes->Cube(), Matrix::CreateScale(0.22f, kUpperLen, 0.22f) * Matrix::CreateTranslation(0, kUpperLen * 0.5f, 0) * upperJoint, Vector3(0.9f, 0.5f, 0.15f));
        Part(device, fx, S().shapes->Sphere(), Matrix::CreateScale(0.32f) * elbowJoint, Vector3(0.7f, 0.7f, 0.75f));
        Part(device, fx, S().shapes->Cube(), Matrix::CreateScale(0.18f, kForeLen, 0.18f) * Matrix::CreateTranslation(0, kForeLen * 0.5f, 0) * elbowJoint, Vector3(0.95f, 0.65f, 0.2f));
        for (int side = -1; side <= 1; side += 2) {
            const Matrix finger = Matrix::CreateRotationZ(static_cast<float>(side) * grip) * Matrix::CreateTranslation(static_cast<float>(side) * 0.08f, 0, 0) * wristJoint;
            Part(device, fx, S().shapes->Cube(), Matrix::CreateScale(0.05f, 0.34f, 0.12f) * Matrix::CreateTranslation(0, 0.17f, 0) * finger, Vector3(0.8f, 0.8f, 0.85f));
        }
    }

    void DrawOrrery(const DrawContext& ctx, BasicEffect& fx)
    {
        GraphicsDevice& device = ctx.device;
        const Matrix root = Matrix::CreateTranslation(R().ToWorld(kOrreryX, kOrreryHeight + 1.0f, kOrreryZ));
        const float t = orreryTime_;
        const LitMesh& ball = S().shapes->Sphere();

        Part(device, fx, ball, Matrix::CreateScale(0.42f) * Matrix::CreateRotationY(t * 0.3f) * root, Vector3(1.0f, 0.8f, 0.3f), Vector3(0.9f, 0.6f, 0.15f));
        struct Planet { float orbit, size, rate; Vector3 colour; bool moon; };
        const Planet planets[3] = {
            {0.75f, 0.14f, 1.6f, Vector3(0.6f, 0.5f, 0.45f), false},
            {1.15f, 0.2f, 1.0f, Vector3(0.25f, 0.5f, 0.9f), true},
            {1.5f, 0.17f, 0.6f, Vector3(0.85f, 0.35f, 0.2f), true},
        };
        for (int i = 0; i < 3; ++i) {
            const Planet& p = planets[i];
            const Matrix orbit = Matrix::CreateTranslation(p.orbit, 0, 0) * Matrix::CreateRotationY(t * p.rate + static_cast<float>(i)) * root;
            Part(device, fx, ball, Matrix::CreateScale(p.size * 2.0f) * Matrix::CreateRotationY(t * 3.0f) * orbit, p.colour);
            if (p.moon) {
                const Matrix moonMatrix = Matrix::CreateScale(p.size * 0.7f) * Matrix::CreateTranslation(p.size * 2.2f, 0, 0) * Matrix::CreateRotationY(t * 4.0f) * orbit;
                Part(device, fx, ball, moonMatrix, Vector3(0.8f, 0.8f, 0.8f));
            }
        }
    }

    bool armRunning_ = true;
    int speed_ = 0;
    float armTime_ = 0.0f;
    float orreryTime_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeAnimationRoom(const RoomDef& room, Services& services)
{
    return std::make_unique<AnimationRoom>(room, services);
}
