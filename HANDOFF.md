# HANDOFF

Working notes for whoever continues this project (human or agent). Everything marked *checked* was run on the date
given; anything unverified says so. Counts are not repeated here: run `build/bin/cna-3d-museum --validate`.

## State (checked 2026-10-13)

- Ten floors (0-9), all built from the catalogue in `src/Catalogue/FloorN.cpp`; see `docs/MUSEUM_MAP.md` for every room.
- **Checked this session:** clean configure and build; `museum_tests` and `ctest` pass; `tools/tour.sh` visits every
  room on OPENGLES3, OPENGL33 and SOFTWARE with exit status 0; `tools/renderer_check.sh` passes all pixel checks on the
  same three renderers; `tools/reference.sh compare` is within 0.02 % on OPENGLES3.
- Real audio output was opened and fed on a PipeWire machine (volume 0.03); whether it is audible cannot be judged by a
  test. Everything else ran on a private Xvfb display with the dummy audio driver.

## Where things are

| Need | Look at |
|---|---|
| Add or change a room | `src/Catalogue/FloorN.cpp` (data) and the exhibit class it names in `src/Exhibits/ExhibitRegistry.cpp` |
| Add an exhibit kind | derive from `ScreenExhibit` (live 2D screen + control panel) or `Exhibit`; register in `ExhibitRegistry.cpp`, declare in `ExhibitFactories.hpp`; sources are globbed (`CONFIGURE_DEPENDS`) |
| Layout and doors | `src/World/Layout.cpp`, `Catalogue.cpp` (`ValidateMuseum`) |
| Lazy building and visibility | `src/World/MuseumWorld.cpp` |
| Controls and HUD | `src/Camera/PlayerController.cpp`, `src/MuseumGame.cpp` (`HandleSystemKeys`), `src/UI/Hud.cpp` |
| Script language | `MuseumGame::RunScript` (`goto:` `at:` `walk:` `press:LABEL=V` `use:` `time:` `floor:` `ride:` `directory:` `shot:` `stats` `quit`) |
| Documentation that is generated | `tools/gen_docs.sh` (README counts, `docs/MUSEUM_MAP.md`, `docs/CNA_COVERAGE.md`, `docs/REFERENCE_VIEWS.md`) |

## Build, run and test

```bash
tools/build.sh                                   # build/ ; MUSEUM_RENDERERS="OPENGLES3;OPENGL33;SOFTWARE" for a multi-renderer tree
build/bin/museum_tests                           # needs: tools/build.sh cna-3d-museum museum_tests
tools/tour.sh -f 3                               # every room, 3 frames each (SOFTWARE needs MUSEUM_TIMEOUT=1500)
tools/renderer_check.sh OPENGLES3 OPENGL33 SOFTWARE
tools/reference.sh compare                       # visual regression, default renderer OPENGLES3
tools/gen_docs.sh                                # regenerate counts, map, coverage tables
tools/readme_shots.sh                            # regenerate docs/images/*.jpg
```

Project rules that matter (from `../CLAUDE.md`): one build directory `build/`, one shared ccache (`CCACHE_DIR`
is exported by the environment, never set by the repo), no building in `/tmp`, full `-j` but watch RAM, never delete
another session's files.

**CNA is a moving target.** `../cna` is edited by other agents while this project builds against it. Its configure step
sometimes fails for a minute ("non-production SDL audit failed", renderer identities changing) because someone's
work in progress is on disk; wait and retry. VULKAN was retired from CNA on 2026-10-13: the museum builds only
OPENGLES3, OPENGL33 and SOFTWARE here.

## CNA findings (details in `docs/KNOWN_ISSUES.md`)

Fixed in CNA while building the museum: `.tif`/`.tiff` advertised for `Texture2D` but not decodable; EasyGL capability
limits (colour attachments, sampled textures, vertex inputs) reported as 0.

Not defects, but easy to trip over: lit `BasicEffect` with `TextureEnabled=false` renders black on EasyGL (bind a 1x1 white
texture, `EffectBank::BeginLit`); PBR models are black until lit; glTF front faces are counter-clockwise (the museum uses
`CullClockwise`); `GetBackBufferData` and render-target readback need `HiDef`; WireFrame throws on OPENGLES3 (gate on
`SupportsCapability`); `ContentManager.Unload()` does not dispose copies already handed out; `BoundingFrustum.Intersects(Ray)`
throws NotImplemented; do not write `f(out n) + n` (evaluation order); never share mutable effect state between rooms;
use `Kit::Rgba` because CNA's `Color(int,int,int,uint8)` constructors are ambiguous; `Texture2D.SetData` fails while the
texture is bound (`RenderStates::UnbindTextures`).

## Museum bugs found and fixed this session

- The mouse-look fallback treated "no pointer" (SOFTWARE has no window) as a gigantic movement; fixed, and keyboard look added
  (Left/Right, Page Up/Down). This had been misdiagnosed as a SOFTWARE rendering bug.
- Panel text/readouts that silently depended on another room's effect state (black floors); labs now set everything they use.
- A bezel box hid a control panel quad; screens now sit on the far wall, not the north wall.
- Room banners cut long subtitles; they now shrink to fit between the screen edge and the minimap.

## What is next (priority order)

1. **Gaps listed in `docs/CNA_COVERAGE.md`**: `ShaderPackageEXT`, SkinnedEffect, morph targets and material variants,
   ResourceContentManager, haptic effects. (Occlusion queries, multisampling, image-based lighting, touch gestures and
   GameServiceContainer were added after the first audit.)
2. **Navigation**: stairs/escalators are not implemented (elevators and the directory move you between floors); a
   "you are here" marker exists on the map and minimap.
3. **Progress**: the set of visited rooms lives only for the session (`StorageContainer` is demonstrated in the *Title Files*
   room and could persist it in a few lines).
4. **Outdoors**: floor 0 has the lawn and forecourt; the observation deck is on floor 9. More outdoor rooms (courtyard,
   sculpture park, fountain plaza) and exterior lights that follow the museum clock are not done.
5. **Polish**: legibility of long screens from the arrival pose (most labs want you to walk two or three metres in),
   the marble parallelogram in `vec-cross` shows z-fighting stripes, a few `-Wconversion` warnings remain in older exhibits.
6. More tests: a leak check on `tools/tour.sh` lifetime counters, and tests for the reference-view list.

## Intentionally not implemented

Video (needs FFmpeg: `CNA_ENABLE_VIDEO` is off), networking/GamerServices/avatars, interactive device dialogs
(`FileDialog`, `MessageBox`, `SystemTray`, `UrlLauncher`, camera), compiled `.fxb` effects, retired renderers.
