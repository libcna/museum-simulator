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
| ThreadSanitizer tour / Diagnostics | SDL3_mixer can invoke a fire-and-forget track's stopped callback and then its final cooked callback in the same mixer iteration. `SoundEffect::Play()` reclaimed the stopped callback's queued pan state without synchronizing with that final read, producing a delete/read race. | fixed in CNA (`f46e29005`): pending pan state crosses `MixerLock` as a callback barrier before reclamation; short-sound stress regression added. Full focused audio suite: 748 passed, 4 skipped; targeted museum TSan tour then clean. |
| Full ASan+UBSan museum tour / Diagnostics | At process exit, the main thread's `thread_local` diagnostics context was destroyed before static graphics resources. Their destructors then emitted `ResourceDestroyed` into the freed context (`heap-use-after-free` in `EmitRawEvent`). | fixed in CNA (`e541a636f`): late events are dropped after thread-context release. All 41 focused diagnostics tests pass; reverting only the fix makes the regression death test fail with exit status 3. |
| Audio and Input / Video | Destroying a `Video` while a `VideoPlayer` still played it left the player's `CloseDecoder` writing a null pointer into the freed video: heap corruption ("corrupted size vs. prev_size") at exit, in about one run in ten to twenty. | fixed in CNA (`~Video` stops its player; a regression test was added to `VideoPlayerTests.cpp` and compiles, but the full `CnaTests` binary was not run here) |
| Platform / Exceptions CNA Throws | `TitleContainer.OpenStream` of a missing title file threw a bare `std::runtime_error`; XNA documents `FileNotFoundException`. | fixed in CNA (`5ccb70808`; its callers now catch `std::exception`) |
| Online Service / Messages | The Guide's Messages page asked the service for 50 messages; the service lists at most 32 (protocol v1), so the page always said "Messages could not be read". | fixed in CNA (`GuideSocial.cpp` asks for 32; the fake backend now enforces the service's bounds) |
| Content Pipeline / Loose Image Formats | `ContentManager` advertised `.tif`/`.tiff` as loose `Texture2D` extensions, but the image decoder has no TIFF support (`docs/texture-stream-formats.md`): a `.tif` was found and then failed with "unknown image type". | fixed in CNA (`ContentTexture2DLoaders.cpp`) |
| Platform and Renderer / The Active Renderer | The OPENGLES3/OPENGL33 (EasyGL) capability profile reported `MaxColorAttachments`, `MaxSampledTexturesPerShaderStage`, `MaxVertexInputBindings` and `MaxVertexInputAttributes` as the known value 0 (the interface default), next to a supported `MultipleRenderTargets`. | fixed in CNA (EasyGL now reports the live context's values: 8, 32, 16, 16 here) |
| Content from Memory | `ResourceContentManager.Load<T>()` never read its resource family: `Load<T>()` climbed the file-system tier ladder and ignored the virtual `OpenStream()` seam (only the protected `ReadAsset<T>()` used it), so the public call fell through to loose-file decoding and failed; `OpenStream()`'s documentation claimed every `.xnb` load went through it. Fixed in CNA (`c1006b03e`): a manager can route `Load<T>()` through `OpenStream()` (`RouteLoadsThroughOpenStreamEXT`) and `ResourceContentManager` does; three tests added. | fixed |
| Content / Failure Gallery (found by AddressSanitizer) | `Load<Texture2D>("")` resolves to the content root, a directory. On POSIX an `ifstream` opens a directory and `tellg()` reports `LONG_MAX`, so `ImageLoader::Load` asked the allocator for 8 exabytes (it only "worked" because `bad_alloc` was caught as a failed load). The font, glTF and platform file readers had the same pattern. | fixed in CNA (`49b1d3a99`: `is_regular_file` first; test added) |
| Render Targets / A Live Cube Map (found on the second renderer family) | `EnvironmentMapEffect` originally sampled an EasyGL rendered `RenderTargetCube` upside down even though `GetData` agreed across renderers. | row orientation fully fixed in CNA: `23c5557f7` normalizes `SetData`/`GetData` (W1 64/64); `a6e8104d2` covers `EnvironmentMapEffect`; `c8722e283` covers both PBR IBL cubes and the opt-in source-`ShaderEffect` contract (8/8 on both GL profiles). Open separately: a one-texel GLES3 seamless-filtering face-edge artifact. |
| Render State / BlendFactor (found by `tools/room_diff.sh`) | WEBGPU replayed every queued 3D draw with the blend factor that was current at the flush, not the one it was made with: a `Blend.BlendFactor` draw followed by `BlendState.Opaque` came out opaque. | fixed in CNA (`09c3108e5`; shared test) |
| Light rooms (found by the new `PunctualLightEXT` probe) | The SOFTWARE renderer accepted `BasicEffect.PunctualLightEXT` and ignored it. | fixed in CNA (`53aba6ef1`; shared test) |
| Light rooms (same probe) | FNA3D's real XNA stock binaries have no `PunctualLightEXT` input, so point and spot lights originally lit nothing. | fixed in CNA (`cb52126f4`): a CNA-owned six-pass Effect Framework binary is selected only for an active lamp; BasicEffect and identity-bone SkinnedEffect pixel witnesses pass 5/5 each, the FNA3D validation matrix has 0 failures and the full 293-room tour is clean. |
| Windows build under Wine, DIRECTX9 | CNA's DirectX 9 renderer keyed its vertex declaration on the stride alone and accepted one BasicEffect flag combination per stride: unlit textured meshes, vertex-coloured untextured geometry, 48-byte lit meshes all threw "no matching CNA vertex layout" (11 rooms/stages). `SetStringMarkerEXT` threw "not yet implemented". | fixed in CNA (`d3f1a9e51`, `829861cd8`): all five stock effects accept any declaration supplying the chosen shader's inputs; the debug marker uses `D3DPERF_SetMarker`. Expanded DrawEx regression 22/22 and full museum press-all tour clean. PBR point/spot lighting is fixed in `0de32e6ba`; BasicEffect and SkinnedEffect point/spot lighting are fixed in `86d85198c`/`79c513885`. `d3539f3fb` adds single/cascaded directional, point-cube and spot shadow reception across all four rigid/skinned Basic/PBR paths; its pixel oracle passes 29/29 and is mutation-verified. The `cx-shadow` room still cannot generate the map on DirectX9 because its caster is GLSL source. |
| Surface Formats under Wine, DIRECTX9 | `D3D9FormatMapping` knew the classic formats, but the texture renderers always allocated `D3DFMT_A8B8G8R8`; the `tex-formats` and `tex-volume` rooms therefore exposed only Color. | fixed in CNA (`d1a78ca32`, `6da0e4bce`): Texture2D, cube and volume resources allocate their mapped native format, each classifier probes the matching live D3D9 resource type, and cube maps carry exact DXT blocks. Both rooms pass; the graphics-profile regression passes 27/27 and bit-checks every advertised cube/volume format. |
| Full HEADLESS press-all tour | Plain `TextureCube::SetData` accepted uploads but retained no pixels, so `tex-cube` and `model-ibl` later raised four honest readback exceptions. | fixed in CNA (`d73f1aeab`): exact format-native CPU storage for all faces, mips, subrectangles and DXT blocks. Focused tests pass 11/11, 33/33 and 39/39; all 293 rooms then pass with 0 exhibit errors. |

## Open observations (not explained, no CNA change made)

- **UBSan in a dependency:** with the FNA3D renderer, `MojoShader/mojoshader_common.c:1077` (`mult *= 10` on an `int` while printing a float
  with a long precision) reports a signed-integer overflow ("1000000000 * 10"). Upstream code CNA fetches; nothing in the museum
  or CNA's own sources is involved.

- **Multisampled render target re-bound every frame with `RenderTargetUsage.PreserveContents` showed no smoothing on OPENGLES3** (observed
  earlier in the campaign; **not reproducible on 2026-10-14**). The room *Multisampled Targets* drawn with `PreserveContents` is now
  pixel-identical to the `DiscardContents` one (smoothed at 4 and 8 samples), and the validation check "PreserveContents target re-bound
  three times keeps the smoothing" (`rv-targets`: bind, clear, draw, unbind, three times over one target) passes on OPENGLES3, OPENGL33
  and SOFTWARE. The cause was never found; CNA's render-target code changed in between. The room keeps `DiscardContents`.
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


- A textured draw whose vertices all carry the same texture coordinate (a model with no UVs through `BasicEffect` with the default `LinearWrap`) is black on OPENGLES3 and OPENGL33 under Mesa's software GL (llvmpipe) and correct on SOFTWARE: the sampler sees zero derivatives and the mip-mapped filter turns that into a NaN level of detail. Found in the *Importing a Model File* room (the cottage files have no UVs); drawing with `LinearClamp` (no mip term) or giving the vertices varying UVs fixes it, so the room uses `LinearClamp` and says why. Not reproduced on a hardware GPU (none here); EasyGL keeps the mip term in the default filter on purpose (XNA's `Linear` is min, mag and mip linear), so no CNA change was made. Checked 2026-10-15.
- `Texture3D` formats other than Color: OPENGLES3, OPENGL33, SOFTWARE, FNA3D, WEBGPU, SDL_GPU and Wine DIRECTX9 round-trip `Single` and `Vector4` volumes exactly (also as a boxed `GetData`). FNA3D's missing typed-transfer bridge/classifier was fixed in CNA `b23c9bd19`; WebGPU's hard-coded RGBA8 volume storage and transfers were made format-native in `fd2b299c0`; SDL_GPU gained native R32Float/RGBA32Float volumes and format-sized typed transfers in `d410638c6`; DirectX9 gained resource-kind probes and native volume allocation in `6da0e4bce`. The focused tests preserve exact data and the live room passes on every listed renderer. Checked 2026-10-16.
- A rejected `ShaderEffect` on WEBGPU returns one diagnostic per log line, so a validation error with a source excerpt becomes several records (8 for a missing semicolon), while GLSL drivers give one or two with line and column; WEBGPU records carry no line. `ShaderDiagnosticEXT::parseCompilerLog` documents that rule. The *Shader Compile Errors* room lists the first three.

## Unexplained: one heap corruption during a fuzz run

`tools/tour.sh -f 3 -z 30 -s 3` on OPENGLES3 aborted once with glibc's "corrupted double-linked list (not small)" late in the tour
(after the Resource room). It followed three caught exceptions from the Sprite scissor room (a scissor rectangle larger than the
surface, since fixed). The same sequence did not reproduce afterwards: not in an unfixed AddressSanitizer + UBSan build (the three
exceptions were thrown again, no report), not in a fixed ASan build, and not in a fixed normal build (exit 0, 0 exhibit errors).
It may be a fault in a system library that ASan does not instrument (llvmpipe), or CNA state left behind when an exception left a
`SpriteBatch` open. Watch for it in long fuzz runs.

**Second occurrence (2026-10-14, seed 8 over 285 rooms):** `tools/tour.sh -f 3 -z 30 -s 8` aborted after 994 s with "malloc(): largebin double
linked list corrupted (nextsize)" (SIGABRT). The core (systemd-coredump) shows the main thread inside `malloc`, called by Mesa's
libgallium (llvmpipe) from `glDrawElements` under `EasyGLSpriteBatchRenderer::FlushBatch`, from `ControlPanel::Redraw` -> `LiveScreen::Render`
-> `SpriteBatch::End`; the other threads were idle (the avatar loader and the gamer-services queue waiting, the dummy audio thread
sleeping). The same seed and rooms did not fail again: not under AddressSanitizer + UBSan (exit 0, 1166 s, no report), and not in the
normal build with `GLIBC_TUNABLES=glibc.malloc.check=3` and `MALLOC_PERTURB_=165` (exit 0, 1101 s). Both incidents are late in a long
OPENGLES3 run on Mesa's software rasteriser and both show the allocator's own check firing inside libgallium, so a fault in that
library (or a write by one of its rasteriser threads) is the leading suspect, but nothing proves it. `goto <room>` is now logged
(`[museum][script]`) so that the next occurrence names the room it happened in.
