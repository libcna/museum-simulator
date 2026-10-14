# HANDOFF

Working notes for whoever continues this project (human or agent). Everything marked *checked* was run on the date
given; anything unverified says so. Counts are not repeated here: run `build/bin/cna-3d-museum --validate`.

## State (checked 2026-10-13, extended 2026-10-14)

- Ten floors (0-9), all built from the catalogue in `src/Catalogue/FloorN.cpp`; `docs/MUSEUM_MAP.md` lists every room and
  `build/bin/cna-3d-museum --validate` prints the current counts (they are generated, not copied here).
- **Checked on this date:** clean configure and build of `build/` from scratch (2.5 min with a warm ccache);
  `museum_tests` (4,000+ checks, includes per-floor door-graph connectivity) and `ctest`; `tools/tour.sh -f 3 -p` visits every
  room and operates every control on OPENGLES3, OPENGL33 and SOFTWARE with exit status 0; `tools/renderer_check.sh` passes
  every pixel check on the same three renderers; `tools/reference.sh compare` is within the limit on OPENGLES3;
  `tools/nav_test.sh` rides to all ten floors; `tools/leak_check.sh` shows no growth over three tours.
- **2026-10-14 additions, all run through `tools/tour.sh -f 3 -p` on OPENGLES3 (plus OPENGL33 / SOFTWARE for the graphical ones),
  `museum_tests` and the screenshots checked by eye:** `tex-io` (SaveAsPng/SaveAsJpeg/FromStream through a MemoryStream),
  `model-skinfx` (SkinnedEffect, WeightsPerVertex 1/2/4), `be-model` rebuilt as an Effect reflection lab (parameters, techniques,
  IEffect interfaces, Clone), `rt-cube` (RenderTargetCube drawn face by face, EnvironmentMapEffect mirror), `ct-resource`
  (ResourceContentManager over a ResourceManager), `sprite-hit` (per-pixel collision), `sprite-anim`, `mat-billboard`,
  `mat-reflection`, `mat-normals`, `mat-offcenter`, `rs-stencilcount` (stencil ops, write masks, two-sided), `geo-terrain`, four
  more fragment shaders (swirl, chromatic, kernel, halftone), plus extra readouts in existing rooms (Viewport.Unproject round trip,
  Buttons/IsButtonDown, adapter profile and format queries, display colour space, bone tree, pad extension queries).
- Every room entered through its north wall used to have its content behind the visitor; see "Museum bugs found" below.
- The audio floor was also run with a real audio device (PipeWire, volume 0.03): the mixer opens and every audio room's
  controls run without errors; whether it is audible cannot be judged by a test. All other runs use Xvfb and the dummy driver.
- Nothing has been run on a physical gamepad, joystick, touch screen, microphone or a machine without audio: those rooms
  report absence cleanly (checked with the hardware missing, which is the only case available here).

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

**Sanitizers (checked 2026-10-14):** a full AddressSanitizer + UBSan build (`build-asan/`, configured with `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g1 -Wno-maybe-uninitialized"`, ~4.7 GB, deleted afterwards) toured all 257 rooms with every control operated in 246 s and ran `museum_tests` (6402 checks): no reports after the directory fix above. Leak detection was off (`detect_leaks=0`).

`tools/check_all.sh` runs the whole battery (a few minutes); the individual tools are:

```bash
tools/build.sh                                   # build/ ; MUSEUM_RENDERERS="OPENGLES3;OPENGL33;SOFTWARE" for a multi-renderer tree
build/bin/museum_tests                           # needs: tools/build.sh cna-3d-museum museum_tests
tools/tour.sh -f 3 -p                            # every room, 3 frames each, every control operated (SOFTWARE needs MUSEUM_TIMEOUT=1500)
tools/nav_test.sh                                # elevator to every floor
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

Fixed in CNA while building the museum: a directory read as an image file (found with AddressSanitizer, 2026-10-14); `.tif`/`.tiff` advertised for `Texture2D` but not decodable; EasyGL capability
limits (colour attachments, sampled textures, vertex inputs) reported as 0.

Not defects, but easy to trip over: lit `BasicEffect` with `TextureEnabled=false` renders black on EasyGL (bind a 1x1 white
texture, `EffectBank::BeginLit`); PBR models are black until lit; glTF front faces are counter-clockwise (the museum uses
`CullClockwise`); `GetBackBufferData` and render-target readback need `HiDef`; WireFrame throws on OPENGLES3 (gate on
`SupportsCapability`); `ContentManager.Unload()` does not dispose copies already handed out; `BoundingFrustum.Intersects(Ray)`
throws NotImplemented; do not write `f(out n) + n` (evaluation order); never share mutable effect state between rooms;
use `Kit::Rgba` because CNA's `Color(int,int,int,uint8)` constructors are ambiguous; `Texture2D.SetData` fails while the
texture is bound (`RenderStates::UnbindTextures`).

## Museum bugs found and fixed this session

- **Half of the rooms were back to front.** Rooms alternate between the two sides of a gallery; every exhibit was written
  for "enter through the south door, screens on the north wall", so a room entered through its north door had its content
  behind the visitor, left/right captions swapped and some screens facing away. Fixed at the root: those rooms (108, see
  `--list-turned-rooms`) get a turned local frame (`RoomDef::frameYaw`, `ToWorld`, `Rotate`, `LocalWall`, `Exhibit::Off`);
  `docs/ARCHITECTURE.md` has the rules for new exhibit code, `museum_tests` (`TestRoomFrames`) guards the invariant.
- Teleports (quick travel, elevator, scripts) never called `OnLeave` on the room being left, so e.g. the Game Components
  badges stayed on screen in the next room. `MuseumWorld::Arrive` now calls it.
- The mouse-look fallback treated "no pointer" (SOFTWARE has no window) as a gigantic movement; fixed, and keyboard look added
  (Left/Right, Page Up/Down). This had been misdiagnosed as a SOFTWARE rendering bug.
- Panel text/readouts that silently depended on another room's effect state (black floors); labs now set everything they use.
- A bezel box hid a control panel quad; screens now sit on the far wall, not the north wall.
- Room banners cut long subtitles; they now shrink to fit between the screen edge and the minimap.

## What is next (priority order)

1. **Gaps listed in `docs/CNA_COVERAGE.md`**: haptic effects (SkinnedEffect has `model-skinfx`, ResourceContentManager `ct-resource`, RenderTargetCube `rt-cube`, texture encode/decode `tex-io`). (Occlusion queries,
   multisampling, image-based lighting, morph targets, material variants, shader packages, touch gestures and
   GameServiceContainer were added after the first audit.)
2. **Navigation**: stairs/escalators are not implemented (elevators and the directory move you between floors); a
   "you are here" marker exists on the map and minimap.
3. **Progress** is saved by `src/World/Progress.cpp` (CNA `StorageDevice`/`StorageContainer`, file `progress.txt` under the
   per-user data folder, one room key per line). Scripted, screenshot and `--frames` runs never write it unless `--save` is
   given. CNA does not call `Game.UnloadContent` at exit, so the save happens in the `MuseumGame` destructor.
4. **Outdoors**: floor 0 has the lawn and forecourt (they now darken with the museum clock); the observation deck is on floor 9.
   More outdoor rooms (courtyard, sculpture park, fountain plaza) and exterior lamps are not done.
5. **Polish**: legibility of long screens from the arrival pose (most labs want you to walk two or three metres in),
   the marble parallelogram in `vec-cross` shows z-fighting stripes, a few `-Wconversion` warnings remain in older exhibits.
6. More tests: a leak check on `tools/tour.sh` lifetime counters, and tests for the reference-view list.

## Intentionally not implemented

Video (needs FFmpeg: `CNA_ENABLE_VIDEO` is off), networking/GamerServices/avatars, interactive device dialogs
(`FileDialog`, `MessageBox`, `SystemTray`, `UrlLauncher`, camera), compiled `.fxb` effects, retired renderers.
