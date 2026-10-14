# Known issues and renderer findings

Only things that were observed are listed. "Open" means not yet explained.

## SOFTWARE renderer showed the museum from the wrong camera angle (resolved: a museum bug, not CNA's)

- Observed 2026-10-13 with `CNA_GRAPHICS_RENDERER=SOFTWARE`: every room looked as if the camera pointed at the ceiling.
- Cause, found with the Renderer Validation floor and a log of the mouse state: SOFTWARE presents a CPU buffer and has no
  native window, so relative mouse mode is unavailable and the pointer can not be re-centred. `Mouse.GetState()` kept
  returning (0, 0); the player controller's fallback read that as a mouse movement of (-centre) every frame and pinned the
  pitch at its limit. CNA's renderer was never involved: all 50 pixel self-checks of the `rv-*` rooms pass on SOFTWARE.
- Fix (museum): the player controller spends the first frames after capturing the pointer verifying that re-centring works
  and turns mouse look off when it does not; arrow keys and Page Up/Down now look around on every platform.
- Result: OPENGLES3 vs OPENGL33 screenshots differ in 0.00 % of pixels, OPENGLES3 vs SOFTWARE in 4.6 % (filtering and
  rasterisation details).

## Frame loop

- `Game::Update`/`Draw` base methods must be called for `GameComponent`s to run (done in `MuseumGame`).
- A texture that is still bound to the device cannot be rewritten with `SetData` ("The texture resource is in use"):
  call `RenderStates::UnbindTextures(device)` first.

## Shaders

- `ShaderEffect` GLSL source: vertex and fragment stages must declare shared uniforms with the same precision
  (the fragment header uses `precision highp float` to match the vertex stage default).

## CNA defects found while building the museum

Found by the museum's own exhibits; fixed in the CNA repository (commit ids are in CNA's `git log`).

| Found in | Defect | Status |
|---|---|---|
| Audio and Input / Video | Destroying a `Video` while a `VideoPlayer` still played it left the player's `CloseDecoder` writing a null pointer into the freed video: heap corruption ("corrupted size vs. prev_size") at exit, in about one run in ten to twenty. | fixed in CNA (`~Video` stops its player; a regression test was added to `VideoPlayerTests.cpp` and compiles, but the full `CnaTests` binary was not run here) |
| Platform / Exceptions CNA Throws | `TitleContainer.OpenStream` of a missing title file threw a bare `std::runtime_error`; XNA documents `FileNotFoundException`. | fixed in CNA (`5ccb70808`; its callers now catch `std::exception`) |
| Online Service / Messages | The Guide's Messages page asked the service for 50 messages; the service lists at most 32 (protocol v1), so the page always said "Messages could not be read". | fixed in CNA (`GuideSocial.cpp` asks for 32; the fake backend now enforces the service's bounds) |
| Content Pipeline / Loose Image Formats | `ContentManager` advertised `.tif`/`.tiff` as loose `Texture2D` extensions, but the image decoder has no TIFF support (`docs/texture-stream-formats.md`): a `.tif` was found and then failed with "unknown image type". | fixed in CNA (`ContentTexture2DLoaders.cpp`) |
| Platform and Renderer / The Active Renderer | The OPENGLES3/OPENGL33 (EasyGL) capability profile reported `MaxColorAttachments`, `MaxSampledTexturesPerShaderStage`, `MaxVertexInputBindings` and `MaxVertexInputAttributes` as the known value 0 (the interface default), next to a supported `MultipleRenderTargets`. | fixed in CNA (EasyGL now reports the live context's values: 8, 32, 16, 16 here) |
| Content from Memory | `ResourceContentManager.Load<T>()` never read its resource family: `Load<T>()` climbed the file-system tier ladder and ignored the virtual `OpenStream()` seam (only the protected `ReadAsset<T>()` used it), so the public call fell through to loose-file decoding and failed; `OpenStream()`'s documentation claimed every `.xnb` load went through it. Fixed in CNA (`c1006b03e`): a manager can route `Load<T>()` through `OpenStream()` (`RouteLoadsThroughOpenStreamEXT`) and `ResourceContentManager` does; three tests added. | fixed |
| Content / Failure Gallery (found by AddressSanitizer) | `Load<Texture2D>("")` resolves to the content root, a directory. On POSIX an `ifstream` opens a directory and `tellg()` reports `LONG_MAX`, so `ImageLoader::Load` asked the allocator for 8 exabytes (it only "worked" because `bad_alloc` was caught as a failed load). The font, glTF and platform file readers had the same pattern. | fixed in CNA (`49b1d3a99`: `is_regular_file` first; test added) |
| Render Targets / A Live Cube Map (found on the second renderer family) | `EnvironmentMapEffect` sampled a rendered `RenderTargetCube` upside down on EasyGL (OPENGLES3, OPENGL33): GL fills a rendered face bottom-up, a cube is sampled with the first stored row on top. The six faces read back with `GetData` were identical on every renderer, so only sampling was wrong. A known open CNA ticket (REMED-GFX-137). | fixed for `EnvironmentMapEffect` in CNA (`a6e8104d2`; shared test); open: IBL cubes, custom `ShaderEffect` samplers, a one-texel seam on GLES3's seamless cube filtering |
| Render State / BlendFactor (found by `tools/room_diff.sh`) | WEBGPU replayed every queued 3D draw with the blend factor that was current at the flush, not the one it was made with: a `Blend.BlendFactor` draw followed by `BlendState.Opaque` came out opaque. | fixed in CNA (`09c3108e5`; shared test) |
| Light rooms (found by the new `PunctualLightEXT` probe) | The SOFTWARE renderer accepted `BasicEffect.PunctualLightEXT` and ignored it. | fixed in CNA (`53aba6ef1`; shared test) |
| Light rooms (same probe) | **FNA3D ignores `BasicEffect.PunctualLightEXT`** and exposes no capability to ask: point and spot lights simply light nothing. Its stock effects are the real compiled XNA binaries. | open; the validation matrix shows it as the FNA3D row's one FAIL, the light rooms print "IGNORED by this renderer" |
| Windows build under Wine, DIRECTX9 | CNA's DirectX 9 renderer keys its vertex declaration on the stride alone and supports one BasicEffect flag combination per stride; unlit textured meshes, vertex-coloured untextured geometry, 48-byte lit meshes, position-only vertices all throw "no matching CNA vertex layout". 11 rooms/stages affected (sky, shadows, outline, render targets, lighting, models, point lists, ...). Also: `SetStringMarkerEXT` not implemented, `PunctualLightEXT` ignored, an environment-mapped `RenderTargetCube` reads black. | open in CNA (needs the vertex declaration at draw time); the museum survives it |

## Open observations (not explained, no CNA change made)

- **UBSan in a dependency:** with the FNA3D renderer, `MojoShader/mojoshader_common.c:1077` (`mult *= 10` on an `int` while printing a float
  with a long precision) reports a signed-integer overflow ("1000000000 * 10"). Upstream code CNA fetches; nothing in the museum
  or CNA's own sources is involved.

- **Multisampled render target re-bound every frame with `RenderTargetUsage.PreserveContents` shows no smoothing on OPENGLES3**
  (4 samples asked and reported). The same target drawn once and read back, or sampled through SpriteBatch, *is* smoothed
  (checks in `rv-targets`), and with `DiscardContents` re-bound every frame it is smoothed too (room `rt-msaa`). The
  museum therefore uses `DiscardContents` for that room. A minimal repro would be a per-frame re-bind loop in a validation check.
- A multisample request of 2 returns a target that reports 0 samples on this machine (Mesa llvmpipe); 8 is clamped to 4.
  This is the driver's capability, reported honestly by `RenderTarget2D.MultiSampleCount`.
- `OcclusionQuery.PixelCount` on OPENGLES3/OPENGL33 is an any-samples boolean (0 or 1), documented by CNA; the room shows
  `isPixelCountPreciseEXT()` so this is visible.

## Museum bugs found and fixed (for the record)

- **Stand-in effects outlived the model room that made them.** The glTF rooms hand BasicEffects to the parts of the *content manager's cached* model (the Effects mode, and the FNA3D fallback); reloading the same asset then found dangling effects (a crash, only seen under AddressSanitizer on FNA3D, where every model takes the fallback). `ModelLab` now puts the originals back before it drops its own effects.
- **The Windows build did not compile.** The clock room used POSIX `localtime_r`; found by cross-compiling with MinGW. It now uses `localtime_s` on Windows.
- **One room's exception ended the whole museum.** On FNA3D `DrawInstancedPrimitives` with a stock effect throws, the exception
  escaped `Game::Run` and the process died. `MuseumWorld` now guards every exhibit hook (`Guard`): the exception is logged as
  `[museum][exhibit-error]`, shown as a toast, counted in the stats line, and the room's hooks are switched off after three.
  `tools/tour.sh` exits 3 if any were caught. Two rooms also fall back deliberately and say so on their panel: the instancing
  rooms (one draw per cube) and the glTF model rooms (a lit `BasicEffect`) when the renderer refuses the stock-effect route.
- `cx-shadow` tried to compile its GLSL caster on renderers that take WGSL or SPIR-V and logged a shader error; it now checks the
  shader language first like the shader rooms do.

- **Rooms entered through their north door were back to front.** Rooms alternate between the two sides of a gallery and every
  exhibit was written for the south door, so half of them had their screens and subjects behind the visitor, left/right
  captions swapped and some quads facing away. Fixed with a per-room local frame (`RoomDef::frameYaw`, see
  `docs/ARCHITECTURE.md`); `museum_tests` checks the invariant for every room.
- Teleports never ran `OnLeave` for the room being left (stale overlays such as the Game Components badges).
- A `SkinnedEffect` cannot switch lighting off (like XNA's): the room dims its lights instead.

## Observations that are not bugs

- `ContentManager.Load<T>("")` raises `ContentLoadException` whose inner cause is `std::bad_alloc` (the empty name resolves
  to the content directory); harmless, caught and shown by the *When Loading Fails* room.
- `ContentManager.Unload()` does not dispose copies of assets the caller already holds (`Texture2D` copies share the
  resource but keep their own disposed flag): measured in the *Resource Lifetime* room.
- With CNA's keyboard-emulated gamepad on, WASD also drives the emulated left stick; the museum's player controller
  ignores the pad while emulation is on so the visitor is not moved twice.


## Unexplained: one heap corruption during a fuzz run

`tools/tour.sh -f 3 -z 30 -s 3` on OPENGLES3 aborted once with glibc's "corrupted double-linked list (not small)" late in the tour
(after the Resource room). It followed three caught exceptions from the Sprite scissor room (a scissor rectangle larger than the
surface, since fixed). The same sequence did not reproduce afterwards: not in an unfixed AddressSanitizer + UBSan build (the three
exceptions were thrown again, no report), not in a fixed ASan build, and not in a fixed normal build (exit 0, 0 exhibit errors).
It may be a fault in a system library that ASan does not instrument (llvmpipe), or CNA state left behind when an exception left a
`SpriteBatch` open. Watch for it in long fuzz runs.
