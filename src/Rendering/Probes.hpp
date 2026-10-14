#pragma once

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"

/**
 * Tiny render tests the museum runs once to learn what a renderer really does with a CNA extension that has no
 * capability flag. Each draws into a small render target, reads one pixel back and caches the answer, so asking again
 * is free. They must run outside a frame's own render-target work (Load or Update, not while a panel is drawing).
 */
namespace Probes {

enum class Answer { Unknown, Yes, No };

/** Does BasicEffect.PunctualLightEXT (a point light) actually light a surface on this renderer? */
Answer PunctualLights(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

/** Does SpriteBatch.Begin(..., ColorMatrixEffect) transform the sprite's colours on this renderer? (A CNA extension: only SOFTWARE runs it.) */
Answer ColorMatrix(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

/** The text for a panel readout: "lights surfaces", "ignored by this renderer" or "not tested". */
const char* Describe(Answer answer);

}  // namespace Probes
