// Compiled XNA effects: Effect(device, bytes) loads the binary a Windows XNA project's effect compiler produced (an .fxb, the
// payload of an Effect asset). CNA reflects it (parameters, techniques, passes) and runs it where the renderer can: the capability
// CompiledEffects says whether this one can. The effect here is a two-tap sprite pixel shader; SpriteBatch supplies the rest.
#include <algorithm>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameter.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const char* ClassName(EffectParameterClass c)
{
    switch (c) {
        case EffectParameterClass::Scalar: return "Scalar";
        case EffectParameterClass::Vector: return "Vector";
        case EffectParameterClass::Matrix: return "Matrix";
        case EffectParameterClass::Object: return "Object";
        default: return "Struct";
    }
}
const char* TypeName(EffectParameterType t)
{
    switch (t) {
        case EffectParameterType::Single: return "Single";
        case EffectParameterType::Int32: return "Int32";
        case EffectParameterType::Bool: return "Bool";
        case EffectParameterType::Texture2D: return "Texture2D";
        case EffectParameterType::String: return "String";
        default: return "other";
    }
}

class CompiledFxLab final : public ScreenExhibit {
public:
    CompiledFxLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Compiled effect", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddToggle("Draw through the effect", true, [this](bool v) { use_ = v; });
        p.AddSlider("Weight (parameter)", 0.0f, 1.0f, weight_, [this](float v) { weight_ = v; }, "%.2f");
        p.AddSlider("Offset.X (parameter, texels)", -24.0f, 24.0f, offset_.X, [this](float v) { offset_.X = v; }, "%.1f", 0.5f);
        p.AddSlider("Offset.Y (parameter, texels)", -24.0f, 24.0f, offset_.Y, [this](float v) { offset_.Y = v; }, "%.1f", 0.5f);
        p.AddChoice("Picture", {"logo", "checker", "circuit"}, 0, [this](int v) { picture_ = v; });
        p.AddReadout("CompiledEffects capability", [this] { return capability_; });
        p.AddReadout("Effect", [this] { return status_; });
        p.AddReadout("Parameters", [this] { return parameters_; });
        p.AddReadout("Techniques", [this] { return techniques_; });
        AddInfoSign(b, "Compiled XNA Effects",
                    {"An XNA project compiles its .fx files into Effect assets. CNA loads that binary (Effect(device, bytes), or a ContentManager reading the Effect from an .xnb) and reflects it like XNA does,",
                     "but runs it only where the renderer reports CompiledEffects (FNA3D). CNA embeds no HLSL compiler: this one was built with XNA Game Studio 4.0 from Content/Effects/blur2tap.fx."});
    }

    void OnScreenLoaded() override
    {
        const bool supported = S().device->SupportsCapability(CNA::GraphicsCapability::CompiledEffects);
        capability_ = supported ? "true" : "false: this renderer cannot run compiled effects";
        try {
            std::ifstream in(S().contentRoot + "/Effects/blur2tap.fxb", std::ios::binary);
            const std::vector<char> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (raw.empty()) throw std::runtime_error("Content/Effects/blur2tap.fxb is missing");
            const std::vector<SharpRuntime::bytecs> bytes(raw.begin(), raw.end());
            effect_ = std::make_unique<Effect>(*S().device, bytes);
            status_ = std::to_string(bytes.size()) + " bytes loaded";
            Describe();
        } catch (const std::exception& e) {
            status_ = std::string("refused: ") + std::string(e.what()).substr(0, 70);
            Museum::Log("fx", "%s", e.what());
        }
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        Texture2D* tex = S().textures->Get(picture_ == 0 ? "logo" : picture_ == 1 ? "checker" : "circuit");
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        sb.DrawString(*S().font, "Through a compiled XNA effect", Vector2(24, 12), Kit::kGold, 0.0f, Vector2(0, 0), 1.0f, SpriteEffects::None, 0.0f);
        if (tex != nullptr) sb.Draw(*tex, Rectangle(40, 70, 400, 400), Color::White);
        sb.DrawString(*S().font, "plain SpriteBatch", Vector2(40, 478), Kit::kDim, 0.0f, Vector2(0, 0), 0.6f, SpriteEffects::None, 0.0f);
        sb.End();
        const bool run = use_ && effect_ != nullptr && !failed_;
        try {
            if (run) {
                effect_->getParametersProperty()["Weight"]->SetValue(weight_);
                const float w = tex != nullptr ? static_cast<float>(std::max(1, tex->getWidthProperty())) : 256.0f, h = tex != nullptr ? static_cast<float>(std::max(1, tex->getHeightProperty())) : 256.0f;
                effect_->getParametersProperty()["Offset"]->SetValue(Vector2(offset_.X / w, offset_.Y / h));
            }
            sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr, run ? effect_.get() : nullptr);
            if (tex != nullptr) sb.Draw(*tex, Rectangle(500, 70, 400, 400), Color::White);
            sb.End();
        } catch (const std::exception& e) {
            failed_ = true;
            status_ = std::string("could not run: ") + std::string(e.what()).substr(0, 60);
            if (S().font != nullptr) {
                sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
                sb.DrawString(*S().font, status_, Vector2(500, 200), Kit::Rgba(255, 190, 150), 0.0f, Vector2(0, 0), 0.6f, SpriteEffects::None, 0.0f);
            }
        }
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        sb.DrawString(*S().font, run ? "Weight * tex(uv) + Weight * tex(uv + Offset)" : (effect_ ? "effect switched off" : "no effect"), Vector2(500, 478), Kit::kDim, 0.0f, Vector2(0, 0), 0.6f, SpriteEffects::None, 0.0f);
        sb.End();
    }

private:
    void Describe()
    {
        parameters_.clear();
        techniques_.clear();
        const EffectParameterCollection& ps = effect_->getParametersProperty();
        for (int i = 0; i < ps.getCountProperty(); ++i) {
            const EffectParameter& p = *ps[i];
            if (!parameters_.empty()) parameters_ += ", ";
            parameters_ += p.getNameProperty() + " (" + ClassName(p.getParameterClassProperty()) + " " + TypeName(p.getParameterTypeProperty()) + ")";
        }
        const EffectTechniqueCollection& ts = effect_->getTechniquesProperty();
        for (int i = 0; i < ts.getCountProperty(); ++i) {
            const EffectTechnique& t = *ts[i];
            if (!techniques_.empty()) techniques_ += ", ";
            techniques_ += t.getNameProperty() + " (" + std::to_string(t.getPassesProperty().getCountProperty()) + " pass)";
        }
    }

    std::unique_ptr<Effect> effect_;
    bool use_ = true, failed_ = false;
    float weight_ = 0.5f;
    Vector2 offset_{6.0f, 0.0f};
    int picture_ = 0;
    std::string capability_ = "-", status_ = "-", parameters_ = "-", techniques_ = "-";
};

}  // namespace

std::unique_ptr<Exhibit> MakeCompiledFxLab(const RoomDef& room, Services& services)
{
    return std::make_unique<CompiledFxLab>(room, services);
}
