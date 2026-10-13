#pragma once

#include <string>

#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"

namespace ModelUtil {

/** Logs (once per model) which effect classes the model's meshes use. Diagnostics only. */
void DescribeOnce(const std::string& name, Microsoft::Xna::Framework::Graphics::Model& model);

/**
 * Draws every mesh part of `model` through `effect` instead of the effects the importer
 * created. The part's own diffuse colour (when it uses a BasicEffect) is used as the
 * effect's diffuse colour, so glTF material colours survive the substitution.
 */
void DrawWithEffect(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                    Microsoft::Xna::Framework::Graphics::Model& model,
                    Microsoft::Xna::Framework::Graphics::BasicEffect& effect,
                    const Microsoft::Xna::Framework::Matrix& world,
                    const Microsoft::Xna::Framework::Vector3* diffuseOverride = nullptr);

}  // namespace ModelUtil
