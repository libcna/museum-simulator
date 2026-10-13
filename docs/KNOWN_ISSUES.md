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
| Content Pipeline / Loose Image Formats | `ContentManager` advertised `.tif`/`.tiff` as loose `Texture2D` extensions, but the image decoder has no TIFF support (`docs/texture-stream-formats.md`): a `.tif` was found and then failed with "unknown image type". | fixed in CNA (`ContentTexture2DLoaders.cpp`) |
| Platform and Renderer / The Active Renderer | The OPENGLES3/OPENGL33 (EasyGL) capability profile reported `MaxColorAttachments`, `MaxSampledTexturesPerShaderStage`, `MaxVertexInputBindings` and `MaxVertexInputAttributes` as the known value 0 (the interface default), next to a supported `MultipleRenderTargets`. | fixed in CNA (EasyGL now reports the live context's values: 8, 32, 16, 16 here) |

## Open observations (not explained, no CNA change made)

- **Multisampled render target re-bound every frame with `RenderTargetUsage.PreserveContents` shows no smoothing on OPENGLES3**
  (4 samples asked and reported). The same target drawn once and read back, or sampled through SpriteBatch, *is* smoothed
  (checks in `rv-targets`), and with `DiscardContents` re-bound every frame it is smoothed too (room `rt-msaa`). The
  museum therefore uses `DiscardContents` for that room. A minimal repro would be a per-frame re-bind loop in a validation check.
- A multisample request of 2 returns a target that reports 0 samples on this machine (Mesa llvmpipe); 8 is clamped to 4.
  This is the driver's capability, reported honestly by `RenderTarget2D.MultiSampleCount`.
- `OcclusionQuery.PixelCount` on OPENGLES3/OPENGL33 is an any-samples boolean (0 or 1), documented by CNA; the room shows
  `isPixelCountPreciseEXT()` so this is visible.

## Observations that are not bugs

- `ContentManager.Load<T>("")` raises `ContentLoadException` whose inner cause is `std::bad_alloc` (the empty name resolves
  to the content directory); harmless, caught and shown by the *When Loading Fails* room.
- `ContentManager.Unload()` does not dispose copies of assets the caller already holds (`Texture2D` copies share the
  resource but keep their own disposed flag): measured in the *Resource Lifetime* room.
- With CNA's keyboard-emulated gamepad on, WASD also drives the emulated left stick; the museum's player controller
  ignores the pad while emulation is on so the visitor is not moved twice.
