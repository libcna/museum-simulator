#pragma once

#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace QuadDraw {

/**
 * Draws one textured quad in world space with the shared World() effect (vertex colour times
 * texture, no lighting). `origin` is the bottom-left corner, `U` runs to the right and `V` up;
 * the front face looks along cross(U, V). The caller sets blend/depth/cull state.
 */
void Textured(const DrawContext& ctx, Microsoft::Xna::Framework::Graphics::Texture2D* texture,
              const Microsoft::Xna::Framework::Vector3& origin, const Microsoft::Xna::Framework::Vector3& U,
              const Microsoft::Xna::Framework::Vector3& V,
              const Microsoft::Xna::Framework::Color& tint = Microsoft::Xna::Framework::Color::White);

/** Same, for a flat colour (no texture). */
void Coloured(const DrawContext& ctx, const Microsoft::Xna::Framework::Vector3& origin,
              const Microsoft::Xna::Framework::Vector3& U, const Microsoft::Xna::Framework::Vector3& V,
              const Microsoft::Xna::Framework::Color& colour);

}  // namespace QuadDraw
