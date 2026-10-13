# HANDOFF

## Status
- All 11 rooms are implemented and render (screenshots checked with tools/shot.sh).
- Sky room: the clock is driven by `TimeOfDay` (exposed as `Services::timeOfDay`); console buttons change hour and speed; a sundial shadow follows the sun.
- Audio room: four looping 3D speakers + a one-shot bell. Audio playback itself is only verified with the dummy driver (no real listening test yet).
- Ideas: day/night screenshots of the Sky room, scripted smoke test of every interactable, collision check in the new rooms.

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
