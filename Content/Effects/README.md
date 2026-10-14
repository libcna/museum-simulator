<!-- SPDX-License-Identifier: MS-PL -->

# Compiled XNA effect

`blur2tap.fx` is a pixel shader written for CNA's own tests (MS-PL); `blur2tap.fxb` is the exact payload that
Microsoft XNA Game Studio 4.0's effect compiler produced from it (Windows, HiDef), 720 bytes, SHA-256
`f1828e7eb9d2b2fa432f23dc941886b53aae2c011f28f14263b572435872d5d4`. Both are copied unchanged from CNA's
`tests/fixtures/compiled-effects/variance-moments-xna4.*`. The *Compiled XNA Effects* room loads the `.fxb` with
`Effect(device, bytes)`. CNA runs such effects only on renderers that report `GraphicsCapability::CompiledEffects`
(FNA3D); the museum cannot build one itself, because CNA embeds no HLSL compiler.
