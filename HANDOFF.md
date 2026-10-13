# HANDOFF

Working notes for whoever continues this project (human or agent). Facts here were checked, not assumed;
anything unverified is labelled as such. See `docs/` for architecture and the CNA coverage matrix.

## What exists (checked 2026-10-13)

- A data-driven museum: floors → wings → rooms, described in `src/Catalogue/FloorN.cpp`, laid out by
  `src/World/Layout.cpp`, built lazily around the visitor by `src/World/MuseumWorld.cpp`.
- Every floor has the same shape: elevator + hall (hub) → concourse (spine) → wing galleries → rooms.
- Floors built so far: 0 (lobby + original tour), 1 (fundamentals). See `build/bin/cna-3d-museum --inventory`
  for current counts (generated; do not copy numbers into docs).
- Tools: `tools/build.sh`, `tools/run.sh`, `tools/shot.sh`, `tools/roomshots.sh`, `tools/tour.sh`,
  `tests/museum_tests.cpp` (`tools/build.sh cna-3d-museum museum_tests && build/bin/museum_tests`).
- `cna-3d-museum --validate` checks the catalogue structure with no window; `--list-rooms`, `--inventory` too.

## Build / run

See README.md. One build tree, `build/`, configured as a multi-renderer build
(`MUSEUM_RENDERERS="OPENGLES3;OPENGL33;VULKAN;SOFTWARE" tools/build.sh`). Pick a renderer at run time with
`CNA_GRAPHICS_RENDERER=VULKAN tools/shot.sh out.png ...`. ccache is used if present; this machine exports
`CCACHE_DIR=/rv/cnaccache`, nothing in the repo does.

## Verified results

- Clean configure + build from scratch: OK (2 m 18 s with a warm ccache).
- Start-up/shutdown on OPENGLES3, OPENGL33, VULKAN (lavapipe) and SOFTWARE: all start and exit 0.
- Renderer comparison of the lobby screenshot: OPENGLES3 vs OPENGL33 0.14 % of pixels differ by >15/255;
  OPENGLES3 vs VULKAN 0.9 %; **SOFTWARE renders the lobby wrongly** (89 % of pixels differ; the camera seems
  to look at the ceiling). Not yet investigated: see docs/KNOWN_ISSUES.md.
- Real audio device: the audio mixer opens and plays (checked with `MUSEUM_AUDIO=real`, volume 0.03);
  audibility cannot be judged by an agent.

## CNA findings (record new ones here and in docs/KNOWN_ISSUES.md)

- Lit `BasicEffect` with `TextureEnabled=false` renders BLACK on EasyGL; `EffectBank::BeginLit()` binds a 1x1 white texture.
- glTF models import with `PbrEffect`; front faces are CCW, so the museum uses `RasterizerState::CullClockwise`.
- `GetBackBufferData` / render-target readback need `GraphicsProfile::HiDef`.
- `BasicEffect` has CNAEXT point/spot lights (`punctualLightEXT_`).
- Do not write `f(out n) + n * x`: unspecified evaluation order.
- Game base `Update`/`Draw` must be called for `GameComponent`s to run (the museum does).

## Conventions that matter

- Rooms are data (`RoomSpec`) + an exhibit kind (a C++ class, `src/Exhibits/**`) selected by name in
  `src/Exhibits/ExhibitRegistry.cpp`. A kind reads `RoomDef::params` (e.g. `mode`) so one class serves many rooms.
- Every exhibit room automatically gets a room card at its door (`RoomCard.cpp`) from the catalogue's
  title/subtitle/apis/origin: fill those in honestly (`ApiOrigin::CnaExtension` for non-XNA features).
- `ControlPanel` (src/Interaction) is the in-world UI: sliders/toggles/choices/buttons/readouts on a screen.
- `Gizmos` (src/Rendering) draws lines/arrows/labels; `MiniView` gives a render-target view of a scene.
- Script language for tests: see `RunScript` in `src/MuseumGame.cpp` (`goto:`, `at:`, `shot:`, `use:`, `floor:`, `ride:`, ...).
- Never build outside `build/`; never create other build directories (see ../CLAUDE.md).

## Next steps (in priority order)

1. Floors 2-9 (see docs/MUSEUM_MAP.md for the plan), interleaved with polish passes on layout/legibility.
2. Investigate the SOFTWARE renderer difference with a minimal repro; fix in CNA if it is a CNA bug.
3. Stairs/escalators, outdoor areas, day/night in exteriors, progress persistence.
4. Documentation: README screenshots, docs/CNA_COVERAGE.md, docs/ARCHITECTURE.md, docs/RENDERER_TESTING.md.
