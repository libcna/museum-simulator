# Known issues and renderer findings

Only things that were observed are listed. "Open" means not yet explained.

## SOFTWARE renderer shows the museum from the wrong camera angle (open)

- Observed 2026-10-13, `CNA_GRAPHICS_RENDERER=SOFTWARE`, every room (lobby, geo-triangle, cam-orbit, tex-mip).
- The 3D view looks as if the camera were pointed at the ceiling: the floor/ceiling lights appear huge and
  changing the camera pitch (script `at:x,z,yaw,pitch`) does not change the picture, while yaw changes it a little.
  The HUD (SpriteBatch) is correct. OPENGLES3 and OPENGL33 agree with each other.
- (VULKAN was retired from CNA on 2026-10-13, so it is no longer part of the comparison.)
- CNA documents SOFTWARE as "complete classic XNA contract" but flags it Experimental in
  `GraphicsBackendMaturity`. The museum uses only stock `BasicEffect`, `VertexBuffer`/`IndexBuffer`,
  `DrawIndexedPrimitives`, `DrawUserPrimitives` and render targets.
- Not caused by near/far planes (tried 0.5/100 and 0.2/260).
- Next step: a minimal repro (one triangle with an explicit view/projection through BasicEffect on SOFTWARE) to decide
  whether this is CNA or the museum; fix in CNA if it is CNA.

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

## Observations that are not bugs

- `ContentManager.Load<T>("")` raises `ContentLoadException` whose inner cause is `std::bad_alloc` (the empty name resolves
  to the content directory); harmless, caught and shown by the *When Loading Fails* room.
- `ContentManager.Unload()` does not dispose copies of assets the caller already holds (`Texture2D` copies share the
  resource but keep their own disposed flag): measured in the *Resource Lifetime* room.
- With CNA's keyboard-emulated gamepad on, WASD also drives the emulated left stick; the museum's player controller
  ignores the pad while emulation is on so the visitor is not moved twice.
