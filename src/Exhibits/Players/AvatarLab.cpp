// Avatar laboratory: CNA's implementation of XNA's avatar API (AvatarDescription, AvatarRenderer, AvatarAnimation) on the
// original avatars compiled into every CNA release. The avatar stands on a plinth and is drawn by AvatarRenderer into the
// museum's own scene: the renderer brings its own shaders, so the room only supplies the world, view and projection.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Rendering/RenderStates.hpp"
#include "World/RoomBuilder.hpp"

#ifdef MUSEUM_NETWORKING
#include "Microsoft/Xna/Framework/GamerServices/AvatarAnimation.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarAnimationPreset.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBodyType.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarDescription.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarExpression.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarRenderer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarRendererState.hpp"
#include "Microsoft/Xna/Framework/GamerServices/Gamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamerCollection.hpp"
#include "System/TimeSpan.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

#ifdef MUSEUM_NETWORKING
using namespace Microsoft::Xna::Framework::GamerServices;

struct Preset {
    AvatarAnimationPreset value;
    const char* name;
};
// Every avatar has the same 31 standard animations; the female and male idles and emotes only suit their own body type, as in XNA.
constexpr Preset kPresets[] = {
    {AvatarAnimationPreset::Stand0, "Stand0"}, {AvatarAnimationPreset::Stand1, "Stand1"}, {AvatarAnimationPreset::Stand2, "Stand2"}, {AvatarAnimationPreset::Stand3, "Stand3"},
    {AvatarAnimationPreset::Stand4, "Stand4"}, {AvatarAnimationPreset::Stand5, "Stand5"}, {AvatarAnimationPreset::Stand6, "Stand6"}, {AvatarAnimationPreset::Stand7, "Stand7"},
    {AvatarAnimationPreset::Clap, "Clap"}, {AvatarAnimationPreset::Wave, "Wave"}, {AvatarAnimationPreset::Celebrate, "Celebrate"},
    {AvatarAnimationPreset::FemaleIdleLookAround, "FemaleIdleLookAround"}, {AvatarAnimationPreset::FemaleLaugh, "FemaleLaugh"}, {AvatarAnimationPreset::FemaleYawn, "FemaleYawn"},
    {AvatarAnimationPreset::MaleIdleLookAround, "MaleIdleLookAround"}, {AvatarAnimationPreset::MaleLaugh, "MaleLaugh"}, {AvatarAnimationPreset::MaleYawn, "MaleYawn"},
};
constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
#endif

class AvatarLab final : public Exhibit {
public:
    AvatarLab(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        plinth_ = BuildStagePlinth(b, 0.0f, 0.0f, 0.4f);
        ControlPanel& p = AddPanel("Avatar", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_NETWORKING
        p.AddChoice("AvatarAnimation preset", {kPresets[0].name, kPresets[1].name, kPresets[2].name, kPresets[3].name, kPresets[4].name, kPresets[5].name, kPresets[6].name, kPresets[7].name, kPresets[8].name,
                                               kPresets[9].name, kPresets[10].name, kPresets[11].name, kPresets[12].name, kPresets[13].name, kPresets[14].name, kPresets[15].name, kPresets[16].name},
                    9, [this](int v) { StartPreset(v); });
        p.AddToggle("Loop the animation", true, [this](bool v) { loop_ = v; });
        p.AddChoice("Body type", {"Female", "Male"}, 1, [this](int v) { bodyType_ = v == 0 ? AvatarBodyType::Female : AvatarBodyType::Male; NewAvatar(); });
        p.AddButton("AvatarDescription.CreateRandom", [this] { NewAvatar(); });
        p.AddButton("The Visitor's own (BeginGetFromGamer)", [this] { FromGamer(); });
        p.AddChoice("Face override (AvatarExpression)", {"none (the animation's)", "Neutral", "Sad", "Angry", "Confused", "Laughing", "Shocked", "Happy", "Yawning"}, 0, [this](int v) { expression_ = v - 1; });
        p.AddSlider("Turntable speed", 0.0f, 2.0f, spin_, [this](float v) { spin_ = v; });
        p.AddReadout("AvatarRenderer.State", [this] { return state_; });
        p.AddReadout("Body / height", [this] { return summary_; });
        p.AddReadout("Description", [this] { return std::string("1021 bytes, ") + (description_ && description_->getIsValidProperty() ? "valid" : "invalid"); });
        p.AddReadout("Animation", [this] { return animationText_; });
#else
        p.AddReadout("GamerServices", [] { return std::string("not built into this museum"); });
#endif
        SignSpec what = Kit::Plate("Avatars", {"CNA implements XNA's avatar API on original avatars: an AvatarDescription (1021 bytes) says who it is, an AvatarAnimation",
                                               "poses the 71-bone skeleton, and AvatarRenderer draws it with the world, view and projection you give it. Nothing here is a model file."});
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
#ifdef MUSEUM_NETWORKING
        NewAvatar();
        StartPreset(9);
#endif
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
#ifdef MUSEUM_NETWORKING
        if (animation_) {
            animation_->Update(System::TimeSpan::FromSeconds(static_cast<double>(ctx.dt)), loop_);
            char buf[96];
            std::snprintf(buf, sizeof buf, "%.1f of %.1f s", animation_->getCurrentPositionProperty().getTotalSecondsProperty(), animation_->getLengthProperty().getTotalSecondsProperty());
            animationText_ = buf;
        }
        if (renderer_) {
            switch (renderer_->getStateProperty()) {
                case AvatarRendererState::Loading: state_ = "Loading"; break;
                case AvatarRendererState::Ready: state_ = "Ready"; break;
                default: state_ = "Unavailable"; break;
            }
        }
        turn_ += ctx.dt * spin_ * 0.8f;
#endif
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
#ifdef MUSEUM_NETWORKING
        if (!renderer_ || !animation_) return;
        // Left-handed XNA avatars face +Z; the turntable turns them around the plinth, and the visitor looks from where the door is.
        renderer_->setWorldProperty(Matrix::CreateRotationY(turn_ + R().frameYaw + MathHelper::Pi) * Matrix::CreateTranslation(plinth_));
        renderer_->setViewProperty(ctx.view.view);
        renderer_->setProjectionProperty(ctx.view.projection);
        if (expression_ < 0) {
            renderer_->Draw(animation_.get());
        } else {
            AvatarExpression face;
            const int e = std::clamp(expression_, 0, 8);
            face.setLeftEyeProperty(static_cast<AvatarEye>(e));
            face.setRightEyeProperty(static_cast<AvatarEye>(e));
            face.setMouthProperty(static_cast<AvatarMouth>(e));
            const auto bones = animation_->getBoneTransformsProperty();
            renderer_->Draw(std::vector<Matrix>(bones.begin(), bones.end()), face);
        }
        RenderStates::SetOpaque3D(ctx.device);   // the renderer set its own states
#else
        (void)ctx;
#endif
    }

private:
#ifdef MUSEUM_NETWORKING
    void NewAvatar()
    {
        try {
            description_ = std::make_unique<AvatarDescription>(AvatarDescription::CreateRandom(bodyType_));
            MakeRenderer();
        } catch (const std::exception& e) {
            Museum::Log("avatar", "CreateRandom: %s", e.what());
        }
    }
    void FromGamer()
    {
        try {
            auto* gamers = Gamer::getSignedInGamersProperty();
            if (gamers == nullptr || gamers->getCountProperty() == 0) return;
            System::IAsyncResult* r = AvatarDescription::BeginGetFromGamer((*gamers)[0], {}, std::any{});
            description_ = std::make_unique<AvatarDescription>(AvatarDescription::EndGetFromGamer(r));
            delete r;
            if (description_->getBodyTypeProperty() != bodyType_) StartPreset(9);
            MakeRenderer();
        } catch (const std::exception& e) {
            Museum::Log("avatar", "BeginGetFromGamer: %s", e.what());
        }
    }
    void MakeRenderer()
    {
        renderer_ = std::make_unique<AvatarRenderer>(description_.get(), true);
        renderer_->setLightDirectionProperty(Vector3(-0.4f, -0.5f, -0.75f));
        renderer_->setLightColorProperty(Vector3(0.75f, 0.72f, 0.68f));
        renderer_->setAmbientLightColorProperty(Vector3(0.42f, 0.42f, 0.46f));
        char buf[96];
        std::snprintf(buf, sizeof buf, "%s, %.2f m", description_->getBodyTypeProperty() == AvatarBodyType::Male ? "male" : "female", static_cast<double>(description_->getHeightProperty()));
        summary_ = buf;
    }
    void StartPreset(int index)
    {
        animation_ = std::make_unique<AvatarAnimation>(kPresets[std::clamp(index, 0, kPresetCount - 1)].value);
    }

    std::unique_ptr<AvatarDescription> description_;
    std::unique_ptr<AvatarRenderer> renderer_;
    std::unique_ptr<AvatarAnimation> animation_;
    AvatarBodyType bodyType_ = AvatarBodyType::Male;
    int expression_ = -1;
    bool loop_ = true;
    float spin_ = 0.3f, turn_ = 0.0f;
    std::string state_ = "-", summary_ = "-", animationText_ = "-";
#endif
    Vector3 plinth_{0, 0, 0};
};

}  // namespace

std::unique_ptr<Exhibit> MakeAvatarLab(const RoomDef& room, Services& services)
{
    return std::make_unique<AvatarLab>(room, services);
}
