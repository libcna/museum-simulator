#include "Rendering/CompiledInstancing.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "CNA/GraphicsCapability.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace CompiledInstancing {

std::unique_ptr<Effect> Load(GraphicsDevice& device, const std::string& contentRoot)
{
    if (!device.SupportsCapability(CNA::GraphicsCapability::Instancing) ||
        !device.SupportsCapability(CNA::GraphicsCapability::CompiledEffects)) {
        return nullptr;
    }

    std::ifstream input(contentRoot + "/Effects/instancing.fxb", std::ios::binary);
    const std::vector<char> raw((std::istreambuf_iterator<char>(input)),
                                std::istreambuf_iterator<char>());
    if (raw.empty()) {
        throw std::runtime_error("Content/Effects/instancing.fxb is missing");
    }
    const std::vector<SharpRuntime::bytecs> bytes(raw.begin(), raw.end());
    return std::make_unique<Effect>(device, bytes);
}

void Apply(Effect& effect, const Matrix& worldViewProjection, const Vector4& diffuseColor)
{
    EffectParameter* transform = effect.getParametersProperty()["WorldViewProjection"];
    EffectParameter* colour = effect.getParametersProperty()["DiffuseColor"];
    if (transform == nullptr || colour == nullptr) {
        throw std::runtime_error("the compiled instancing effect is missing its parameters");
    }
    transform->SetValue(worldViewProjection);
    colour->SetValue(diffuseColor);
    for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty()) pass.Apply();
}

}  // namespace CompiledInstancing
