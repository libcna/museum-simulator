# Known issues and renderer findings

Only things that were observed are listed. "Open" means not yet explained.

## SOFTWARE renderer shows the museum from the wrong camera angle (open)

- Observed 2026-10-13, `CNA_GRAPHICS_RENDERER=SOFTWARE`, every room (lobby, geo-triangle, cam-orbit, tex-mip).
- The 3D view looks as if the camera were pointed at the ceiling: the floor/ceiling lights appear huge and
  changing the camera pitch (script `at:x,z,yaw,pitch`) does not change the picture, while yaw changes it a little.
  The HUD (SpriteBatch) is correct. OPENGLES3, OPENGL33 and VULKAN (lavapipe) agree with each other.
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
