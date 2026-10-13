# HANDOFF

## Status
- All 11 rooms are implemented and render (screenshots checked with tools/shot.sh).
- Sky room: the clock is driven by `TimeOfDay` (exposed as `Services::timeOfDay`); console buttons change hour and speed; a sundial shadow follows the sun.
- Audio room: four looping 3D speakers + a one-shot bell. Audio playback itself is only verified with the dummy driver (no real listening test yet).
- Known limitation: Sky room walls/floor use baked vertex lighting, so they stay bright at night; only dynamic objects (sculpture, sundial shadow) follow the sun.
- Script tip: `tools/run.sh "pos:...;look:-90,-15;e;..."` presses E on the focused item (yaw -90 faces +X).
- Verified (2026-10-13): all 45 interactables activated via `use:N` with and without audio (dummy driver) without errors; walking into the walls of the Animation and Audio rooms stops at the collider; night sky checked.
- Ideas: listen-test audio on a real device; per-room unit-style script tests in tools/.

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
