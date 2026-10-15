<!-- SPDX-License-Identifier: MS-PL -->

# Compiled XNA effect

`blur2tap.fx` is a pixel shader written for CNA's own tests (MS-PL); `blur2tap.fxb` is the exact payload that
Microsoft XNA Game Studio 4.0's effect compiler produced from it (Windows, HiDef), 720 bytes, SHA-256
`f1828e7eb9d2b2fa432f23dc941886b53aae2c011f28f14263b572435872d5d4`. Both are copied unchanged from CNA's
`tests/fixtures/compiled-effects/variance-moments-xna4.*`. The *Compiled XNA Effects* room loads the `.fxb` with
`Effect(device, bytes)`. CNA runs such effects only on renderers that report `GraphicsCapability::CompiledEffects`
(FNA3D); the museum cannot build one itself, because CNA embeds no HLSL compiler.

`vertextexture.fx` is the museum's vertex-texture demonstration. Its sampler at register 2 deliberately has no
texture or sampler-state assignment, so the application-provided `GraphicsDevice.VertexTextures[2]` and
`VertexSamplerStates[2]` drive the draw. `vertextexture.fxb` was built from that source with the Microsoft DirectX
SDK June 2010 `fxc` (`/T fx_2_0`, the legacy Effect Framework profile), is 972 bytes, and has SHA-256
`79b83e71ec0463bcdedc01222838f7693b67660186082ce8adec1d1fbaa962ca`.
