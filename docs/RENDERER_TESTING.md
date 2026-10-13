# Renderer testing

The museum contains no renderer-specific code. It asks CNA what the active renderer supports
(`GraphicsDevice.SupportsCapability`, `GetRendererCapabilityProfileEXT`) and says so in the room when a feature is
missing. A multi-renderer build lets the same binary run on several renderers:

```bash
MUSEUM_RENDERERS="OPENGLES3;OPENGL33;SOFTWARE" tools/build.sh     # first entry is the default
CNA_GRAPHICS_RENDERER=OPENGL33 build/bin/cna-3d-museum
```

Renderers that exist in CNA but cannot run here (Direct3D, Metal, WebGL2/WebGPU, SDL_GPU...) are untested.
VULKAN was retired from CNA on 2026-10-13 and has been removed from the museum's documentation and build lines.

## The tools

| Tool | What it does |
|---|---|
| `tools/renderer_check.sh [R...]` | runs the floor 9 *Renderer Validation* rooms on each renderer; prints PASS/FAIL counts and leaves every check in `build/renderer_check/<R>.checks` |
| `tools/tour.sh` | walks into every room and runs a few frames: catches exceptions and unsupported-call failures |
| `tools/reference.sh capture\|compare` | stored images for 24 named views per renderer (`tests/reference/<R>`) |
| `tools/roomshots.sh`, `tools/shot.sh` | contact sheets and single screenshots for comparing renderers by eye |

## The Renderer Validation rooms (floor 9)

Each room draws deterministic images into a 64 x 64 render target through stock CNA/XNA calls, reads the pixels back
with `GetData` and compares them with values worked out by hand: no tolerance beyond one or two levels of 255 except
where filtering makes an exact value meaningless. Rooms: geometry and coverage, SpriteBatch pixels, blending, depth,
interpolation/lighting/fog, sampling, render targets, camera matrices, stock effects. The *Depth* room also reports (as
information, not pass/fail) the smallest depth gap the renderer resolves at 100 m.

Result on 2026-10-13 (CNA at the commit shown by `git -C ../cna log -1`; the SOFTWARE renderer is flagged Experimental
by CNA):

| Renderer | Checks | Pass | Fail | Info |
|---|---|---|---|---|
| OPENGLES3 | 51 | 50 | 0 | 1 |
| OPENGL33 | 51 | 50 | 0 | 1 |
| SOFTWARE | 51 | 50 | 0 | 1 |

Run `tools/renderer_check.sh` for the current figures; they are not copied anywhere else.

## Whole-museum results

| Renderer | Tour of every room | Screenshot vs OPENGLES3 (lobby) |
|---|---|---|
| OPENGLES3 | clean | - |
| OPENGL33 | clean | 0.00 % of pixels differ by more than 15/255 |
| SOFTWARE | clean (about 10 minutes for the whole museum) | 4.6 % |

## Findings

1. **SOFTWARE appeared to render the museum from the wrong camera angle.** It was not a renderer problem. SOFTWARE has
   no native window, so relative mouse mode and pointer warping do not work and `Mouse.GetState()` kept returning
   (0, 0); the player controller turned that into a huge mouse movement every frame. Fixed in the museum (the controller
   verifies pointer re-centring first and falls back to the keyboard). Found by the Renderer Validation rooms, which proved
   the rendering itself correct.
2. **CNA: loose `.tif`/`.tiff` were advertised as `Texture2D` formats but cannot be decoded** (the decoder is
   dependency-free and has no TIFF). Fixed in CNA: the extensions are no longer offered.
3. **CNA: the OPENGLES3/OPENGL33 capability profile reported 0 for colour attachments, sampled textures per stage and
   vertex inputs** (the interface's "unclassified" default) next to a supported `MultipleRenderTargets`. Fixed in
   CNA: EasyGL now reports its context's values.
4. OPENGLES3 and OPENGL33 do not support `FillMode.WireFrame` (CNA throws); the museum asks `SupportsCapability(WireFrame)`
   and shows solid geometry with a note.
5. `GetBackBufferData` and render-target readback need `GraphicsProfile.HiDef`.
6. Depth precision (information): all three renderers resolved a 5 mm gap at 100 m with near 0.1 / far 1000.

## Adding a check

Open `src/Exhibits/Validation/ValidationLab.cpp`, add a block to the mode that fits (`Add(name, expected, got, pass, image)`
logs one `[museum][validate]` line per check and shows a thumbnail of the picture). If it fails on one renderer and not
another, that is a finding about the renderer: reproduce it in a minimal check, record it here, and fix it in CNA if it is
clearly CNA's. Do not add a workaround to the museum.
