#pragma once

#include <memory>

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/ImageBasedLightEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureCube.hpp"

/**
 * An image-based lighting environment built in code: a sky (or sunset, or studio) as a function of direction, its
 * cosine-convolved irradiance, a prefiltered specular mip chain (roughness = mip / (mips - 1), the convention of CNA's
 * PbrEffect) and the split-sum BRDF lookup table. Nothing is loaded from a file.
 */
struct ProceduralIbl {
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::TextureCube> irradiance;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::TextureCube> specular;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> brdf;
    int mipCount = 1;

    /** kind: 0 sky, 1 sunset, 2 studio. */
    static ProceduralIbl Build(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, int kind);

    [[nodiscard]] Microsoft::Xna::Framework::Graphics::ImageBasedLightEXT Light(float intensity) const;
};
