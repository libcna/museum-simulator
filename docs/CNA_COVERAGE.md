# CNA coverage

Which parts of CNA the museum demonstrates, which it does not, and why. The table at the end is **generated**
(`tools/gen_docs.sh`) from the `apis` list of every room in `src/Catalogue/`; a room only claims an API it actually
calls. Rooms are labelled **XNA 4.0** (a feature XNA 4.0 has) or **CNA extension** (CNAEXT: not in XNA 4.0).

## Coverage audit (hand-written, 2026-10-13)

| Area | Where | State |
|---|---|---|
| Game, GameTime, components, services, content, viewport | floor 1 *Application Model* | covered (GameServiceContainer has no room of its own: see gaps) |
| Vector, Matrix, Quaternion, Curve, barycentric, projection | floor 1 | covered |
| BoundingBox/Sphere/Frustum, Ray, Plane, picking | floor 1 *Collision* | covered (`BoundingFrustum.Intersects(Ray)` throws NotImplemented in CNA and is avoided) |
| Cameras: orbit, chase, top-down, security, FOV, clip, transition, split, mirror | floor 1 *Cameras* | covered |
| SpriteBatch (blend, sort, sampler, scissor, camera matrix), SpriteFont | floor 2 | covered |
| VertexBuffer, IndexBuffer (16/32 bit), primitive types, DynamicVertexBuffer, user primitives, instancing | floor 3 | covered |
| Texture2D (mips, filter, address, formats, dynamic), TextureCube, Texture3D | floor 3 | covered |
| BasicEffect (all features), AlphaTestEffect, DualTextureEffect, EnvironmentMapEffect | floors 3-4 | covered |
| Punctual point/spot lights (CNAEXT) | floor 4 | covered |
| ShaderEffect (GLSL source at run time) | floor 4 *Shader Lab* | covered; run-time shader source only (no compiled effects) |
| RasterizerState, DepthStencilState (incl. stencil), BlendState, SamplerState | floor 5 | covered |
| RenderTarget2D, MRT, float formats, mirrors | floor 5 | covered |
| glTF models, PbrEffect, rigid and skinned animation, instancing, import report | floor 6 | covered |
| Particle systems (application code on CNA dynamic buffers) | floor 6 | covered |
| SoundEffect, instances, 3D audio, DynamicSoundEffectInstance, Song/MediaPlayer, Microphone | floor 7 | covered |
| Keyboard, Mouse (relative mode, cursors), GamePad (+ keyboard emulation), PlayerIndex slots, TextInputEXT, Clipboard, Joysticks, devices | floor 7 | covered; every room works with no pad, joystick or touch screen |
| ContentManager (cache, Unload, loose formats, failures), cna-content (.cnb/.xnb), TitleContainer, StorageDevice | floor 8 | covered |
| DebugDraw, CRTEffect, DepthEffect, AsciiPostProcessEffect (CNA_CNAEXT) | floor 8 | covered |
| Diagnostics counters and resource list (CNA_DIAGNOSTICS=STATS) | floors 8-9 | covered |
| CNA_DEVICES: SystemInfo, Locale, PowerInfo, DisplayInfo | floor 8 | covered |
| Renderer capability profile, platform capabilities, GraphicsAdapter | floor 8 | covered |
| Renderer self-checks (50 pixel checks) | floor 9 | covered |
| Time of day, sky dome | floors 0 and 9 | application code |

### Gaps (not demonstrated yet) and non-goals

| Feature | Status |
|---|---|
| OcclusionQuery, multisampled render targets, image-based lighting (`ImageBasedLightEXT`), `ShaderPackageEXT`, SkinnedEffect, morph targets and material variants, ResourceContentManager, GameServiceContainer room, touch gestures, haptics | planned: see HANDOFF.md |
| Video playback (`VideoPlayer`) | not implemented: needs CNA_ENABLE_VIDEO (FFmpeg), which this build leaves off |
| GamerServices, Net (network sessions), avatars | not implemented: networking and online services are outside the museum's scope |
| `FileDialog`, `MessageBox`, `SystemTray`, `UrlLauncher`, `Camera` (CNA_DEVICES) | not implemented: they open real dialogs or hardware; the room reports whether each is supported |
| Compiled XNA effects (`.fxb`), C API, Inspector | not implemented: disabled in CNA's default build for the renderers used here |
| Retired renderers and removed CNA features (for example VULKAN, the post-process chain) | deliberately not demonstrated |

## Generated: API names claimed by rooms

<!-- BEGIN COVERAGE -->
<!-- END COVERAGE -->
