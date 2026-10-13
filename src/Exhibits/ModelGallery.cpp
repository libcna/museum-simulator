#include <cmath>
#include <string>
#include <vector>

#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ModelUtil.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/ModelLibrary.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SignFactory.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Motion { SpinY, Tumble, Wobble, Bob, FaceVisitor, SpinX };

struct Item {
    const char* model;
    const char* title;
    const char* formula;
    float x, z;
    float scale;
    float lift;          // height of the model's centre above the pedestal top
    Motion motion;
};

constexpr float kPedestalSize = 0.9f;
constexpr float kPedestalHeight = 1.0f;

// Seven glTF models, each moved by a different composition of CNA's Matrix factories.
const Item kItems[] = {
    {"torus", "Torus", "Scale * RotationX * RotationY * Translation", -5.4f, -4.5f, 1.15f, 0.62f, Motion::Tumble},
    {"knot", "Trefoil Knot", "Scale * RotationX(tilt) * RotationY * Translation", -5.4f, 0.0f, 1.0f, 0.66f, Motion::SpinY},
    {"star", "Star", "Matrix.CreateWorld(position, forward, up): faces the visitor", -5.4f, 4.5f, 1.25f, 0.62f, Motion::FaceVisitor},
    {"vase", "Vase", "Scale * RotationZ(sway) * RotationY * Translation", 4.6f, -4.6f, 0.95f, 0.05f, Motion::Wobble},
    {"cone", "Cone", "Scale * RotationX * Translation", 4.6f, 4.6f, 1.0f, 0.05f, Motion::SpinX},
    {"sphere", "Sphere", "Translation(0, sin(t), 0): a bobbing sphere", -3.3f, -5.5f, 0.95f, 0.6f, Motion::Bob},
    {"cube", "Cube", "Scale * RotationY * RotationX * Translation", 3.3f, -5.5f, 0.85f, 0.55f, Motion::Tumble},
};
constexpr int kItemCount = static_cast<int>(sizeof(kItems) / sizeof(kItems[0]));

class ModelGallery final : public Exhibit {
public:
    ModelGallery(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        for (int i = 0; i < kItemCount; ++i) {
            const Item& it = kItems[i];
            b.AddPedestal(it.x, it.z, kPedestalSize, kPedestalHeight, "marble", Vector3(0.95f, 0.92f, 0.88f));
            SignSpec plaque;
            plaque.lines = {{it.title, true, Color(255, 232, 160, 255)}, {"ContentManager.Load<Model>(\"Models/" + std::string(it.model) + "\")", false, Color(210, 220, 235, 255)}};
            plaque.background = Color(26, 24, 30, 255);
            b.AddPedestalPlaque(it.x, it.z, kPedestalSize, 0.5f, plaque);
        }
        SignSpec wall;
        wall.lines = {{"Model Gallery", true, Color(255, 232, 160, 255)},
                      {"glTF models loaded through CNA's ContentManager and drawn with", false, Color(220, 226, 240, 255)},
                      {"Model.CopyAbsoluteBoneTransformsTo, per-mesh world matrices and BasicEffect lighting.", false, Color(220, 226, 240, 255)}};
        b.AddWallSign(Wall::West, -2.2f, 3.5f, 4.6f, 1.15f, wall);
        // Warm spotlights over the pedestal groups (baked into the wood floor and walls).
        b.AddLight(Vector3(-5.2f, 4.0f, 0.0f), Vector3(1.0f, 0.9f, 0.7f), 0.8f, 8.0f);
        b.AddLight(Vector3(4.2f, 4.0f, 0.0f), Vector3(1.0f, 0.9f, 0.7f), 0.7f, 8.0f);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        ModelLibrary& library = *S().models;
        for (int i = 0; i < kItemCount; ++i) {
            models_[i] = library.Get(kItems[i].model);
            if (models_[i] != nullptr) ModelUtil::DescribeOnce(kItems[i].model, *models_[i]);
            spinning_[i] = true;
            angle_[i] = static_cast<float>(i) * 0.9f;
            const Item& it = kItems[i];
            Interactable ia;
            ia.room = R().id;
            const Vector3 lo = R().ToWorld(it.x - 0.58f, 0.0f, it.z - 0.58f);
            const Vector3 hi = R().ToWorld(it.x + 0.58f, kPedestalHeight + 1.35f, it.z + 0.58f);
            ia.bounds = BoundingBox(lo, hi);
            ia.prompt = [this, i] { return std::string(spinning_[i] ? "Stop the " : "Spin the ") + kItems[i].title; };
            ia.activate = [this, i] { spinning_[i] = !spinning_[i]; };
            interaction.Add(std::move(ia));
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        eye_ = ctx.playerPosition;
        time_ = static_cast<float>(ctx.time);
        for (int i = 0; i < kItemCount; ++i)
            if (spinning_[i]) angle_[i] += ctx.dt * (0.9f + 0.15f * static_cast<float>(i % 3));
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        GraphicsDevice& device = ctx.device;
        BasicEffect& fx = ctx.fx.BeginLit();
        RenderStates::SetOpaque3D(device);
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setSpecularColorProperty(Vector3(0.35f, 0.35f, 0.35f));
        fx.setSpecularPowerProperty(24.0f);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setFogEnabledProperty(false);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);

        for (int i = 0; i < kItemCount; ++i) {
            if (models_[i] == nullptr) continue;
            const Item& it = kItems[i];
            const Vector3 top = R().ToWorld(it.x, kPedestalHeight + it.lift, it.z);
            const BoundingSphere cull(top, 1.1f);
            if (!ctx.view.frustum.Intersects(cull)) continue;
            ModelUtil::DrawWithEffect(device, *models_[i], fx, WorldFor(i, top));
        }
    }

private:
    Matrix WorldFor(int i, const Vector3& position) const
    {
        const Item& it = kItems[i];
        const float a = angle_[i];
        const Matrix scale = Matrix::CreateScale(it.scale);
        switch (it.motion) {
            case Motion::SpinY: return scale * Matrix::CreateRotationX(0.45f) * Matrix::CreateRotationY(a) * Matrix::CreateTranslation(position);
            case Motion::Tumble: return scale * Matrix::CreateRotationX(a * 0.7f) * Matrix::CreateRotationY(a) * Matrix::CreateTranslation(position);
            case Motion::Wobble: return scale * Matrix::CreateRotationZ(std::sin(a * 1.3f) * 0.18f) * Matrix::CreateRotationY(a) * Matrix::CreateTranslation(position);
            case Motion::Bob: return scale * Matrix::CreateTranslation(position + Vector3(0, std::sin(a * 2.0f) * 0.12f, 0));
            case Motion::SpinX: return scale * Matrix::CreateRotationX(a) * Matrix::CreateTranslation(position);
            case Motion::FaceVisitor: {
                // CreateWorld points the object's -Z axis along `forward`; the star's front is +Z, so look away from the visitor.
                Vector3 away = position - eye_;
                away.Y = 0.0f;
                if (away.LengthSquared() < 1e-4f) away = Vector3(0, 0, -1);
                away = Vector3::Normalize(away);
                const float spin = spinning_[i] ? std::sin(a * 1.1f) * 0.35f : 0.0f;
                return scale * Matrix::CreateRotationZ(spin) * Matrix::CreateWorld(position, away, Vector3::Up);
            }
        }
        return scale * Matrix::CreateTranslation(position);
    }

    Model* models_[kItemCount] = {};
    bool spinning_[kItemCount] = {};
    float angle_[kItemCount] = {};
    Vector3 eye_{0, 1.65f, 0};
    float time_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeModelGallery(const RoomDef& room, Services& services)
{
    return std::make_unique<ModelGallery>(room, services);
}
