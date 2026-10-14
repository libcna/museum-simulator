# Renderer testing

The museum contains no renderer-specific code. It asks CNA what the active renderer supports
(`GraphicsDevice.SupportsCapability`, `GetRendererCapabilityProfileEXT`) and says so in the room when a feature is
missing. A multi-renderer build lets the same binary run on several renderers:

```bash
MUSEUM_RENDERERS="OPENGLES3;OPENGL33;SOFTWARE" tools/build.sh     # first entry is the default
CNA_GRAPHICS_RENDERER=OPENGL33 build/bin/cna-3d-museum
```

Six renderers have run the museum here: OPENGLES3, OPENGL33, SOFTWARE, and (since 2026-10-14) FNA3D, WEBGPU and SDL_GPU
(the last two on Mesa's llvmpipe software Vulkan driver, `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`).
Direct3D 9 runs only through Wine (below); Metal and WebGL2 are untested (no suitable device). Two non-visual renderers were tried for robustness: **STUB**
(no 3D pipeline: the museum says so and exits 0) and **HEADLESS** (all 257 rooms run; four exceptions are caught and reported
because it stores no cube-map data: `tex-cube` and the image-based-lighting model rooms switch themselves off).
VULKAN was retired from CNA on 2026-10-13 and has been removed from the museum's documentation and build lines.

The second group needs CNA's optional dependencies from the shared checkouts (nothing is downloaded):

```bash
BUILD_DIR=build-probe MUSEUM_RENDERERS="OPENGLES3;FNA3D;WEBGPU;SDL_GPU" \
  MUSEUM_CMAKE_ARGS="-DFETCHCONTENT_SOURCE_DIR_FNA3D=$HOME/deps/FNA3D -DFETCHCONTENT_UPDATES_DISCONNECTED=ON \
                     -DCNA_WEBGPU_ROOT=$HOME/deps/wgpu-native-v29.0.1.1 -DCNA_WEBGPU_AUTO_DOWNLOAD=OFF" tools/build.sh
CNA_GRAPHICS_RENDERER=WEBGPU VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json BUILD_DIR=build-probe tools/tour.sh -f 3 -p
```

## The tools

| Tool | What it does |
|---|---|
| `tools/renderer_check.sh [R...]` | runs the floor 9 *Renderer Validation* rooms on each renderer; prints PASS/FAIL counts and leaves every check in `build/renderer_check/<R>.checks` |
| `tools/tour.sh` | walks into every room and runs a few frames: catches exceptions and unsupported-call failures |
| `tools/room_diff.sh RENDERER [-b BASE]` | the arrival view of every room on two renderers, ranked by differing pixels: the quickest way to find a renderer divergence among all 257 rooms (below) |
| `tools/reference.sh capture\|compare` | stored images for 30 named views (OPENGLES3 only) (`tests/reference/<R>`) |
| `tools/roomshots.sh`, `tools/shot.sh` | contact sheets and single screenshots for comparing renderers by eye |

## The Renderer Validation rooms (floor 9)

Each room draws deterministic images into a 64 x 64 render target through stock CNA/XNA calls, reads the pixels back
with `GetData` and compares them with values worked out by hand: no tolerance beyond one or two levels of 255 except
where filtering makes an exact value meaningless. Rooms: geometry and coverage, SpriteBatch pixels, blending, depth,
interpolation/lighting/fog, sampling, render targets, camera matrices, stock effects. The *Depth* room also reports (as
information, not pass/fail) the smallest depth gap the renderer resolves at 100 m.

Result on 2026-10-14 (CNA at the commit shown by `git -C ../cna log -1`; the SOFTWARE renderer is flagged Experimental
by CNA):

| Renderer | Checks | Pass | Fail | Info | Skip |
|---|---|---|---|---|---|
| OPENGLES3 | 85 | 84 | 0 | 1 | 0 |
| OPENGL33 | 85 | 84 | 0 | 1 | 0 |
| SOFTWARE | 85 | 84 | 0 | 1 | 0 |
| FNA3D | 85 | 59 | 1 (`PunctualLightEXT` ignored) | 1 | 24 (half-float targets; 12 surface formats; their sampled twins) |
| WEBGPU | 85 | 63 | 0 | 1 | 21 (the same, fewer: it has Color, HalfVector4 and the float formats' targets) |
| SDL_GPU | 85 | 84 | 0 | 1 | 0 |

The "Surface Formats" room (`rv-formats`) writes 16 values through every uncompressed `SurfaceFormat`, reads them back bit for bit,
and draws ten of the unsigned formats through SpriteBatch to check what the GPU samples. All of it passes on the three OpenGL-family
and SDL_GPU renderers; FNA3D can create only `Color` (plus DXT), WEBGPU only `Color`, `HalfVector4` and the 32-bit float formats.
One expectation worth knowing: for the channels a format does not store, Direct3D 9, XNA and every CNA renderer sample **1**
(`R16F` gives (r, 1, 1), `G16R16F` gives (r, g, 1)), while `PackedVector`'s own `ToVector4()` says 0.

Run `tools/renderer_check.sh` for the current figures; they are not copied anywhere else.

## Whole-museum results

| Renderer | Tour of every room | Screenshot vs OPENGLES3 (lobby) |
|---|---|---|
| OPENGLES3 | clean | - |
| OPENGL33 | clean | 0.00 % of pixels differ by more than 15/255 |
| SOFTWARE | clean (about 25 minutes for the whole museum under Xvfb) | 4.6 % |
| FNA3D | clean, 0 exhibit errors (44 s) after two room fallbacks, below | 30 reference views: 22 within 1 %; `lights` 7.8 % (point and spot lights ignored), `pbr` 9.9 % (BasicEffect stand-in), `shader` 5.6 % (no custom shaders), `debugdraw` 4.6 %, `skinned-effect` 2.1 %, `matrices` 1.6 %, `deck` 1.1 %, `billboards` 1.0 % |
| WEBGPU | clean (78 s) | 30 reference views: 26 within 1 %; `debugdraw` 4.7 %, `matrices` 1.6 %, `deck` 1.1 %, `billboards` 1.0 % |
| SDL_GPU | clean (173 s) | 30 reference views: all within 1 % |

Latest (2026-10-14 evening, 285 rooms, after the shader-error, sampler, colour, log and XACT-variable rooms): `tools/tour.sh -f 3 -p` exits 0 with 0 exhibit errors on FNA3D (67 s), WEBGPU (111 s) and SDL_GPU (269 s); ASan+UBSan on OPENGLES3 over all 283 rooms exit 0, no reports.

Earlier (2026-10-14, 280 rooms, after the Online Service wing, Exceptions, Vertex Types, Compiled Effects and the other new rooms): `tools/tour.sh -f 3 -p` exits 0 with 0 exhibit errors on FNA3D (76 s), WEBGPU (133 s) and SDL_GPU (274 s) in the multi-renderer tree (renderer chosen with `CNA_GRAPHICS_RENDERER`); the OPENGLES3 battery (`tools/check_all.sh`) is green.

Re-checked on 2026-10-14 with 272 rooms (after the Avatars, Leaderboards, Pipeline, Processor, Converters, Video and network-search rooms and the dressed screen rooms): `tools/tour.sh -f 3 -p` exits 0 with 0 exhibit errors on OPENGLES3, OPENGL33 and SOFTWARE (`SLOW=1 tools/check_all.sh`, all steps ok) and, in the multi-renderer tree, on FNA3D (75 s), WEBGPU (94 s) and SDL_GPU (175 s). HEADLESS exits 3 with the known exceptions (no cube data). The Avatars, Video and Processing rooms were also looked at on those three: same picture; FNA3D has no DXT surface formats, and the Processing room says so on the level it cannot upload.

Re-checked on 2026-10-14 with 250 rooms after the room-frame change and the new rooms: `tools/tour.sh -f 3 -p` exits 0 on all three
renderers (OPENGLES3 65 s, OPENGL33 62 s, SOFTWARE 1,501 s); the stencil, cube-map, SkinnedEffect, terrain and off-centre rooms
look the same on all three.

## Windows: the MinGW build under Wine (DIRECTX9 and SOFTWARE)

The museum cross-compiles with CNA's MinGW toolchain and runs under Wine on the same Xvfb. This found a real portability bug
(`localtime_r` in the clock room does not exist on Windows) and is the only way to run CNA's DIRECTX9 renderer here:

```bash
BUILD_DIR=build-consumer MUSEUM_RENDERERS="DIRECTX9;SOFTWARE" \
  MUSEUM_CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$PWD/../cna/cmake/toolchains/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release -DMUSEUM_NETWORKING=OFF" tools/build.sh
cp -r build/bin/Content/Compiled build-consumer/bin/Content/        # the content tool is a foreign executable: reuse the native output
cp /usr/lib/gcc/x86_64-w64-mingw32/*-win32/{libgcc_s_seh-1,libstdc++-6}.dll /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll build-consumer/bin/
WINEPREFIX=$HOME/.cache/museum-wine SDL_VIDEODRIVER=windows CNA_GRAPHICS_RENDERER=DIRECTX9 \
  xvfb-run -a wine build-consumer/bin/cna-3d-museum.exe --script "goto:info;wait:5;quit" --no-audio
```

The Windows tree is configured with `-DMUSEUM_NETWORKING=OFF`: CNA's GamerServices needs libcurl, which the MinGW sysroot does not have, so the Players and Networking rooms there say they are not built in (the Avatars, Leaderboards and Net rooms are Linux-only for now). The content pipeline room (`MUSEUM_PIPELINE`) does build and run under Wine.

Under Wine the SOFTWARE renderer draws the lobby identically to Linux and tours all rooms with every control operated (272 rooms on 2026-10-14: exit 0, 0 exhibit errors, 1,313 s; the networking, avatar and video rooms say they are not built in on this tree; 257 rooms earlier took 1,413 s). **DIRECTX9** (Wine's Direct3D on llvmpipe OpenGL) runs
the whole tour without a crash (58 s) but 11 rooms or stages are switched off by the museum's guard, all with one CNA message:
`DirectX9Renderer::DrawPrimitivesEx (BasicEffect): stride N with lighting=... vertexColor=... texture=... has no matching CNA
vertex layout`. The DirectX 9 renderer keys its vertex declaration on the *stride alone* (16, 20, 24, 32 or 52 bytes) and supports
only the one BasicEffect flag combination each stride "naturally" carries, so an unlit textured `VertexPositionNormalTexture`
(32), a vertex-coloured untextured `VertexPositionColorTexture` (24, the sky dome and debug geometry), a 48-byte lit mesh or a
position-only 12-byte vertex are all refused although XNA's own Direct3D 9 accepts them. The renderer also leaves
`SetStringMarkerEXT` unimplemented, ignores `PunctualLightEXT`, and the cube-map orientation check reads black. Validation:
48 pass, 3 fail (point lights, the BasicEffect check run aborted by the layout message, the cube target), 1 skip (half-float
targets) of the 52 checks that ran. Noted in `docs/KNOWN_ISSUES.md`; the museum was **not** bent to fit (no white-texture
workarounds): the place to fix it is the renderer, which would need the vertex declaration (not the stride) at draw time.

## Every room against OPENGLES3 (`tools/room_diff.sh`, 2026-10-14)

Rooms whose arrival view differs from OPENGLES3's by more than 2 % of pixels (640 x 360, fixed clock and step):

| Renderer | Rooms over 2 % | What they are |
|---|---|---|
| OPENGL33 | 1 of 256 | `rs-fill` (it can draw wireframe, OPENGLES3 cannot) |
| SOFTWARE | 20 | no shader dialect (`fx-*`, `cx-crt`), `PbrEffect` is CNA's base-colour fallback here (`model-*`: flat, no lighting or metal, documented in CNA's `docs/software-renderer.md`), `rs-fill`, `rs-depthbias`, `materials`, `cx-shadow`, `pf-dashboard`; and one museum bug the others had hidden (below) |
| SDL_GPU | 7 | `rs-depthbias` (z-fighting at zero bias resolves differently), `fx-plasma` and `fx-swirl` (floating-point trig in the shader), `rs-fill`, `cx-shadow`, `cx-package`, `pf-dashboard` (live numbers) |
| WEBGPU | 28 | those, plus `tex-formats` (Bgr565, Bgra4444 and Alpha8 are not offered), `cx-debugdraw`, and about twenty `vec-*`, `mat-*`, `quat-*` rooms whose 1-pixel grid lines sit one pixel off (a rasterisation difference, not a bug) |
| FNA3D | 42 | no custom shaders (`fx-*`, `cx-crt`, `cx-depth`...), no point or spot lights (`light-*`, `be-punctual`), PBR models drawn as a lit `BasicEffect`, `tex-formats`, wireframe and the same grid-line offset |

Repeated on 2026-10-14 evening over 284 rooms: OPENGL33 vs OPENGLES3 differs by more than 1 % only in `rs-fill` (wireframe); SOFTWARE vs OPENGLES3 differs by more than 8 % only in `model-instances` (24 %: PbrEffect's flat base-colour fallback on thousands of pixels), `fx-kernel` and `fx-plasma` (SOFTWARE has no shader dialect, so the plain scene is shown).

This pass found two real defects that the 30 stored views and the pixel checks had missed, one in the museum and one in CNA. The museum's: `geo-manybuffers` drew black cones on OPENGLES3 and every GPU renderer (only SOFTWARE lit them): all its vertices have texture coordinate (0, 0), so the screen-space derivatives are zero and anisotropic filtering of a zero footprint is undefined (black on Mesa's software GL and Vulkan). It now samples with a linear sampler. CNA's: `rs-blendfactor` was opaque on WEBGPU
(fixed in CNA, see finding 12 and the new "Blend.BlendFactor on a 3D draw after an ordinary one" check).

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
6. Depth precision (information): all six renderers resolved a 5 mm gap at 100 m with near 0.1 / far 1000.
7. **CNA: EasyGL (OPENGLES3, OPENGL33) sampled a rendered `RenderTargetCube` upside down** (the live cube-map room showed
   the floor in both hemispheres of the mirror ball; FNA3D, WEBGPU, SDL_GPU and SOFTWARE agreed with each other, and the six
   face images read back with `GetData` were identical on all of them). A known open ticket on CNA's side (REMED-GFX-137);
   the museum's new check "Cube target sampled as an environment map keeps up as up" failed on exactly the two EasyGL
   renderers. Fixed in CNA (`a6e8104d2`: the stock env-map program mirrors the lookup for a rendered cube; shared example
   test added). Not fixed there: IBL cubes and custom `ShaderEffect` samplers, and a one-texel seam where GLES3's seamless
   cube filtering reads across a face edge.
8. **FNA3D** reports `Instancing` but its stock effects declare no per-instance stream, so `DrawInstancedPrimitives` with a
   stock effect throws; and it has no PBR shading path, so every glTF model (PbrEffect) throws when drawn. The instancing
   rooms fall back to one draw per cube and the model rooms to a lit `BasicEffect`, each saying so on its panel. The
   shader rooms already used `CustomEffects`. (CNA documents both limits in `docs/fna3d-renderer.md`.)
9. **Custom shaders need source in the renderer's own dialect** (`GetShaderDialectEXT`). The shader rooms carry three: GLSL ES 3.00
   (OPENGLES3, OPENGL33 and SOFTWARE-adapted), WGSL (WEBGPU, `Exhibits/Shaders/ShaderSources.cpp`: the same twelve algorithms
   line for line) and Vulkan GLSL 450 (SDL_GPU: the GLSL ES bodies behind another header; the renderer compiles it to SPIR-V
   with shaderc). FNA3D takes none (`CustomEffects` false), so its shader rooms show the scene unprocessed and say so. The
   shader package room (`cx-package`) now holds a WGSL variant too, and "selects WGSL" on WEBGPU. Two behaviours of CNA to know
   when writing these: SDL_GPU's `ShaderEffect` has **one fixed 128-byte uniform block whatever the uniform names say** (a
   viewport size, a matrix, one vec4 "color", one float "slot0"; every `SetUniformVec2/3/4` writes the same vec4), so the lab
   packs its four uniforms into those two slots; and a packaged `ShaderEffect` must name its entry point `main` while the WGSL
   inside it is still looked up as `vs_main`/`fs_main`. WEBGPU's SpriteBatch route writes **colour attachment 0 only**, so the
   multiple-render-target room's edge map stays empty there (the room measures it and says so); the shadow-map caster of
   `cx-shadow` is GLSL-only and reports that.
10. **CNA: the SOFTWARE renderer ignored `BasicEffect.PunctualLightEXT`** (point and spot lights): a lit sphere stayed black, and
    there is no capability flag to ask first, so the museum added `Rendering/Probes.cpp`, a one-off render test whose answer
    the light rooms print on their panels and the validation matrix counts. Fixed in CNA (`53aba6ef1`: EasyGL's falloff and
    cone, no shadow map; shared example test). **FNA3D still ignores it** (its stock effects are compiled XNA binaries with no
    point light; open, listed in `docs/KNOWN_ISSUES.md`), which is the one FAIL in the FNA3D row.
11. **CNA: WEBGPU applied the blend factor current at the flush to every queued 3D draw**, so a draw made with `Blend.BlendFactor` and
    followed by `BlendState.Opaque` (which resets the factor to white) came out opaque. Found by `room_diff` (the render-state room
    looked right on every other renderer), reproduced by a new validation check, fixed in CNA (`09c3108e5`: each queued draw carries
    its factor; shared example test, fails without the change).
12. **An exception inside one room no longer takes the museum down** (a museum fix found by FNA3D's instancing exception, which
    escaped `Game::Run`): the world catches exceptions from each room's hooks, logs `[museum][exhibit-error]`, toasts the
    message and switches the room's hooks off after three. `tools/tour.sh` reports them and exits 3.

## Adding a check

Open `src/Exhibits/Validation/ValidationLab.cpp`, add a block to the mode that fits (`Add(name, expected, got, pass, image)`
logs one `[museum][validate]` line per check and shows a thumbnail of the picture). If it fails on one renderer and not
another, that is a finding about the renderer: reproduce it in a minimal check, record it here, and fix it in CNA if it is
clearly CNA's. Do not add a workaround to the museum.
