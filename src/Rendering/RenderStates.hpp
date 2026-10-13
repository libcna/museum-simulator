#pragma once

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"

// SpriteBatch.End() leaves the device in whatever state it needed, and every
// exhibit changes states on purpose. These helpers set the complete 3D state
// explicitly so nothing depends on what ran before.
namespace RenderStates {

/** Opaque 3D: depth test + write, counter-clockwise front faces (CullClockwise), anisotropic wrap sampling. */
inline void SetOpaque3D(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device)
{
    using namespace Microsoft::Xna::Framework::Graphics;
    device.setBlendStateProperty(BlendState::Opaque);
    device.setDepthStencilStateProperty(DepthStencilState::Default);
    device.setRasterizerStateProperty(RasterizerState::CullClockwise);
    device.getSamplerStatesProperty()[0] = SamplerState::AnisotropicWrap;
}

/** Transparent 3D: depth test but no depth write, so overlapping panes blend instead of occluding. */
inline void SetTransparent3D(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                             const Microsoft::Xna::Framework::Graphics::BlendState& blend)
{
    using namespace Microsoft::Xna::Framework::Graphics;
    device.setBlendStateProperty(blend);
    device.setDepthStencilStateProperty(DepthStencilState::DepthRead);
    device.setRasterizerStateProperty(RasterizerState::CullNone);
    device.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
}

}  // namespace RenderStates
