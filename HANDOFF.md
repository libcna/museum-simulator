# HANDOFF

## Status
- Done: Lobby, Info, Models, Materials, Lighting, RenderStates, RenderTarget, Particles.
- In progress: Animation, Sky, Audio rooms (`CreateExhibit` returns nullptr for them until added).
- `TimeOfDay` is owned by `MuseumGame` and not yet exposed to exhibits (Sky room needs it via `Services`).

## Build / run
See README.md. Build trees: only `build/` (shared ccache `/rv/cnaccache`).

## CNA findings
- Lit `BasicEffect` with `TextureEnabled=false` renders BLACK on EasyGL (OPENGLES3); `EffectBank::BeginLit()` binds a 1x1 white texture.
- glTF models import with `PbrEffect`; front faces are CCW, so the museum uses `RasterizerState::CullClockwise`. Models are drawn via `ModelUtil::DrawWithEffect`.
- `GetBackBufferData` / RT readback need `GraphicsProfile::HiDef`.
- `BasicEffect` has CNAEXT point/spot lights (`punctualLightEXT_`); WireFrame is a `GraphicsCapability`.
- Do not write `f(out n) + n * x`: unspecified evaluation order.

## Script actions
`--script "a;b;c"` — see `src/MuseumGame.cpp`.
