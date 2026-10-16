#pragma once

#include <memory>
#include <string>

#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"

namespace CompiledInstancing {

/** Loads the museum's XNA Effect Framework instancing shader when the renderer can run it. */
std::unique_ptr<Microsoft::Xna::Framework::Graphics::Effect> Load(
    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
    const std::string& contentRoot);

/** Applies one instanced draw's global transform and colour. */
void Apply(Microsoft::Xna::Framework::Graphics::Effect& effect,
           const Microsoft::Xna::Framework::Matrix& worldViewProjection,
           const Microsoft::Xna::Framework::Vector4& diffuseColor);

}  // namespace CompiledInstancing
