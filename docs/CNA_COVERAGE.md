# CNA coverage

Which parts of CNA the museum demonstrates, which it does not, and why. The table at the end is **generated**
(`tools/gen_docs.sh`) from the `apis` list of every room in `src/Catalogue/`; a room only claims an API it actually
calls. Rooms are labelled **XNA 4.0** (a feature XNA 4.0 has) or **CNA extension** (CNAEXT: not in XNA 4.0).

## Coverage audit (hand-written, 2026-10-13)

| Area | Where | State |
|---|---|---|
| Game, GameTime, components, GameServiceContainer, content, viewport | floor 1 *Application Model* | covered |
| Vector, Matrix, Quaternion, Curve, barycentric, projection | floor 1 | covered |
| BoundingBox/Sphere/Frustum, Ray, Plane, picking | floor 1 *Collision* | covered (`BoundingFrustum.Intersects(Ray)` throws NotImplemented in CNA and is avoided) |
| Cameras: orbit, chase, top-down, security, FOV, clip, transition, split, mirror | floor 1 *Cameras* | covered |
| SpriteBatch (blend, sort, sampler, scissor, camera matrix), SpriteFont | floor 2 | covered |
| VertexBuffer, IndexBuffer (16/32 bit), primitive types, DynamicVertexBuffer, user primitives, instancing | floor 3 | covered |
| Texture2D (mips, filter, address, formats, dynamic), TextureCube, Texture3D | floor 3 | covered |
| BasicEffect (all features), AlphaTestEffect, DualTextureEffect, EnvironmentMapEffect | floors 3-4 | covered |
| Punctual point/spot lights (CNAEXT) | floor 4 | covered |
| Shadow mapping with `IShadowReceiverEXT`, PCF filtering and cascaded shadow maps (`ShadowCascadeStateEXT`) | floor 8 | covered |
| ShaderEffect (GLSL source at run time) | floor 4 *Shader Lab* | covered; run-time shader source only (no compiled effects) |
| RasterizerState, DepthStencilState (incl. stencil), BlendState, SamplerState | floor 5 | covered |
| RenderTarget2D, MRT, float formats, mirrors, multisampling, OcclusionQuery | floor 5 | covered |
| glTF models, PbrEffect, image-based lighting, morph targets, material variants, rigid and skinned animation, instancing, import report | floor 6 | covered |
| Particle systems (application code on CNA dynamic buffers) | floor 6 | covered |
| SoundEffect, instances, 3D audio, DynamicSoundEffectInstance, Song/MediaPlayer, Microphone | floor 7 | covered |
| Keyboard, Mouse (relative mode, cursors), GamePad (+ keyboard emulation), PlayerIndex slots, TextInputEXT, Clipboard, Joysticks, TouchPanel gestures, devices | floor 7 | covered; every room works with no pad, joystick or touch screen |
| ContentManager (cache, Unload, loose formats, failures), cna-content (.cnb/.xnb), TitleContainer, StorageDevice | floor 8 | covered |
| DebugDraw, CRTEffect, DepthEffect, AsciiPostProcessEffect, ShaderPackageEXT (CNA_CNAEXT) | floor 8 | covered |
| Diagnostics counters and resource list (CNA_DIAGNOSTICS=STATS) | floors 8-9 | covered |
| CNA_DEVICES: SystemInfo, Locale, PowerInfo, DisplayInfo | floor 8 | covered |
| Renderer capability profile, platform capabilities, GraphicsAdapter | floor 8 | covered |
| Renderer self-checks (50 pixel checks) | floor 9 | covered |
| Time of day, sky dome | floors 0 and 9 | application code |

### Gaps (not demonstrated yet) and non-goals

| Feature | Status |
|---|---|
| haptic effects (devices are listed, none played) | planned: see HANDOFF.md |
| XACT audio (`AudioEngine`, `SoundBank`, `WaveBank`, `Cue`) | not implemented: it needs `.xgs`/`.xwb`/`.xsb` files from Microsoft's XACT authoring tool; no sample files ship with CNA and none can be generated here |
| `MediaLibrary`, albums, playlists, pictures (`Microsoft.Xna.Framework.Media`) | not implemented: they describe a device's media store, which a desktop does not have |
| Video playback (`VideoPlayer`) | not implemented: needs CNA_ENABLE_VIDEO (FFmpeg), which this build leaves off |
| GamerServices, Net (network sessions), avatars | not implemented: networking and online services are outside the museum's scope |
| `FileDialog`, `MessageBox`, `SystemTray`, `UrlLauncher`, `Camera` (CNA_DEVICES) | not implemented: they open real dialogs or hardware; the room reports whether each is supported |
| Compiled XNA effects (`.fxb`), C API, Inspector | not implemented: disabled in CNA's default build for the renderers used here |
| `ColorMatrixEffect` | not shown: CNA implements it on the SOFTWARE renderer only |
| Retired renderers and removed CNA features (for example VULKAN, the post-process chain) | deliberately not demonstrated |

## Generated: API names claimed by rooms

<!-- BEGIN COVERAGE -->
502 distinct API names are claimed by at least one room.

| API | Rooms |
|---|---|
| AlphaModeEXT | `model-material` |
| AlphaTestEffect | `tex-alpha`, `rv-effects` |
| AnimationPlayer | `model-skinned` |
| ApiOrigin | `hist-compare` |
| ApplyClipToBonesEXT | `model-rigid` |
| AsciiQuantizeMode | `cx-ascii` |
| AudioChannels | `au-synth`, `au-generate` |
| AudioEmitter | `audio`, `au-spatial` |
| AudioListener | `audio`, `au-spatial` |
| BasicEffect | `lighting`, `light-types`, `light-falloff`, `model-effects`, `rv-effects` |
| BasicEffect.Alpha | `be-combos`, `be-alpha` |
| BasicEffect.AmbientLightColor | `be-lights` |
| BasicEffect.DiffuseColor | `be-combos` |
| BasicEffect.DirectionalLight0 | `be-lights`, `rv-shading` |
| BasicEffect.EmissiveColor | `be-material`, `light-emissive`, `rf-deck` |
| BasicEffect.EnableDefaultLighting | `be-lights` |
| BasicEffect.FogColor | `be-fog` |
| BasicEffect.FogEnabled | `be-fog`, `rv-shading`, `rf-telescope` |
| BasicEffect.FogEnd | `be-fog` |
| BasicEffect.FogStart | `be-fog` |
| BasicEffect.LightViewProjectionEXT | `cx-shadow` |
| BasicEffect.LightingEnabled | `be-lights` |
| BasicEffect.PreferPerPixelLighting | `be-perpixel` |
| BasicEffect.ShadowCascadesEXT | `cx-shadow` |
| BasicEffect.ShadowDepthBiasEXT | `cx-shadow` |
| BasicEffect.ShadowFilterRadiusEXT | `cx-shadow` |
| BasicEffect.ShadowMapEXT | `cx-shadow` |
| BasicEffect.SpecularColor | `be-material` |
| BasicEffect.SpecularPower | `be-material` |
| BasicEffect.Texture | `pf-state` |
| BasicEffect.TextureEnabled | `be-combos` |
| BasicEffect.VertexColorEnabled | `be-combos`, `rv-shading` |
| BasicEffect.World | `rv-camera` |
| BasicEffect.setPunctualLightEXT | `be-punctual` |
| Bgr565 | `vt-packed` |
| Bgra4444 | `vt-packed` |
| Bgra5551 | `vt-packed` |
| Blend | `sprite-blend`, `rs-blend` |
| Blend.BlendFactor | `rs-blendfactor`, `rv-blend` |
| Blend.InverseBlendFactor | `rs-blendfactor` |
| BlendFunction | `sprite-blend`, `rs-blend`, `rv-blend` |
| BlendState | `sky`, `renderstates`, `particles`, `sprite-blend`, `rs-blend`, `rv-blend` |
| BlendState.Additive | `sprite-blend`, `light-coloured`, `light-many`, `light-emissive`, `pt-fire`, `pt-smoke`, `pt-sparks`, `pt-snow`, `pt-rain`, `pt-fountain`, `pt-dust`, `pt-fireworks`, `pt-bubbles`, `pt-leaves`, `pt-magic` |
| BlendState.AlphaBlend | `sprite-blend` |
| BlendState.BlendFactor | `rs-blendfactor` |
| BlendState.ColorWriteChannels | `rs-colorwrite` |
| BlendState.NonPremultiplied | `sprite-tint`, `tex-alpha`, `be-alpha`, `pf-overdraw`, `rf-garden` |
| BoundingBox | `col-ray-box`, `col-pick`, `an-conveyor` |
| BoundingBox.Contains | `col-box-box` |
| BoundingBox.CreateFromPoints | `col-merge` |
| BoundingBox.CreateFromSphere | `col-merge` |
| BoundingBox.Intersects | `col-box-box` |
| BoundingFrustum | `mat-view`, `col-frustum`, `cam-orbit`, `cam-security` |
| BoundingFrustum.Contains | `mat-perspective`, `col-frustum`, `model-instances` |
| BoundingFrustum.GetCorners | `col-frustum` |
| BoundingSphere | `col-ray-sphere`, `col-pick` |
| BoundingSphere.Contains | `col-sphere-box` |
| BoundingSphere.CreateFromBoundingBox | `col-merge` |
| BoundingSphere.CreateFromFrustum | `col-frustum` |
| BoundingSphere.CreateFromPoints | `col-merge` |
| BoundingSphere.CreateMerged | `col-merge` |
| BoundingSphere.Intersects | `col-sphere-box` |
| BoundingSphere.Transform | `model-bounds` |
| ButtonState | `in-mouse` |
| Buttons | `in-pad` |
| Byte4 | `vt-packed` |
| CNA::Devices::DisplayInfo | `cx-devices` |
| CNA::Devices::Locale | `cx-devices` |
| CNA::Devices::PowerInfo | `cx-devices` |
| CNA::Devices::SystemInfo | `cx-devices` |
| CNA::Diagnostics::GetProvider | `cx-stats`, `pf-dashboard` |
| CNA::Graphics::AsciiPostProcessEffect | `cx-ascii` |
| CNA::Graphics::CRTEffect | `cx-crt` |
| CNA::Graphics::DebugDraw | `cx-debugdraw` |
| CNA::Graphics::DepthEffect | `cx-depth` |
| CNA::Graphics::ShaderPackageEXT | `cx-package` |
| CNA::GraphicsRendererSelection.GetAvailable | `hist-cna` |
| CNA::getGraphicsBackendMaturity | `hist-cna` |
| CNA::getGraphicsRendererName | `hist-cna` |
| CNAEXT | `hist-compare` |
| CRTMaskType | `cx-crt` |
| Clipboard.GetTextEXT | `in-text` |
| Clipboard.SetTextEXT | `in-text` |
| Color | `vt-color`, `sprite-tint` |
| Color.FromNonPremultiplied | `vt-color` |
| Color.Lerp | `vt-color`, `sprite-tint` |
| Color.Multiply | `vt-color` |
| Color.PackFromVector4 | `vt-color` |
| Color.PackedValue | `vt-color` |
| Color.ToVector3 | `vt-color` |
| ColorWriteChannels | `rs-portal`, `rs-colorwrite`, `rv-blend` |
| CompareFunction | `tex-alpha`, `rs-depthfunc`, `rv-depth` |
| CompareFunction.NotEqual | `rs-outline` |
| ContainmentType | `col-box-box` |
| ContentLoadException | `app-content`, `ct-failure`, `ct-custom`, `ct-resource` |
| ContentManager | `models`, `app-content` |
| ContentManager.GetContentManifest | `ct-pipeline` |
| ContentManager.Load | `app-content`, `ct-cache`, `ct-failure`, `ct-pipeline`, `ct-resource` |
| ContentManager.Load<Model> | `model-load`, `ct-load` |
| ContentManager.Load<SoundEffect> | `au-basics`, `ct-load` |
| ContentManager.Load<Texture2D> | `ct-load`, `ct-formats` |
| ContentManager.RegisterCnjLoader | `ct-custom` |
| ContentManager.RegisterTypeReader | `ct-custom` |
| ContentManager.RootDirectory | `ct-load` |
| ContentManager.Unload | `app-content`, `ct-cache`, `ct-lifetime` |
| CubeMapFace | `tex-cube`, `rt-cube` |
| CullMode | `rs-cull` |
| Curve | `curve-class` |
| Curve.IsConstant | `curve-class` |
| CurveContinuity | `curve-class` |
| CurveKey | `curve-class` |
| CurveKey.TangentIn | `curve-class` |
| CurveKey.TangentOut | `curve-class` |
| CurveKeyCollection | `curve-class` |
| CurveLoopType | `curve-class` |
| CurveTangent | `curve-class` |
| DebugDraw.addBox | `cx-debugdraw` |
| DebugDraw.addFrustum | `cx-debugdraw` |
| DebugDraw.addSphere | `cx-debugdraw` |
| DebugDraw.setDepthTested | `cx-debugdraw` |
| DepthEffectMode | `cx-depth` |
| DepthFormat | `cam-clip`, `rt-basic` |
| DepthFormat.Depth24 | `rv-depth` |
| DepthStencilState | `sky`, `renderstates`, `cam-clip`, `rv-depth` |
| DepthStencilState.DepthBufferEnable | `rs-depthfunc` |
| DepthStencilState.DepthBufferFunction | `light-many`, `rs-depthfunc` |
| DepthStencilState.DepthBufferWriteEnable | `rs-depthread` |
| DepthStencilState.DepthRead | `be-alpha`, `rs-depthread` |
| DepthStencilState.ReferenceStencil | `rs-outline` |
| DepthStencilState.StencilEnable | `rs-portal` |
| DepthStencilState.StencilFunction | `rs-portal` |
| DepthStencilState.StencilPass | `rs-outline`, `rs-shadow` |
| DirectionalLight | `lighting`, `be-lights`, `light-types` |
| DisplayMode | `pl-adapter` |
| DitherMode | `cx-depth` |
| DrawableGameComponent | `app-components` |
| DualTextureEffect | `be-dual`, `rv-effects` |
| DualTextureEffect.Texture2 | `be-dual` |
| DynamicIndexBuffer | `geo-dynindex` |
| DynamicSoundEffectInstance | `au-synth` |
| DynamicSoundEffectInstance.SubmitBuffer | `au-synth` |
| DynamicVertexBuffer | `sky`, `particles`, `geo-dynamic`, `pt-fire`, `pt-smoke`, `pt-sparks`, `pt-snow`, `pt-rain`, `pt-fountain`, `pt-dust`, `pt-fireworks`, `pt-bubbles`, `pt-leaves`, `pt-magic`, `rf-garden`, `rf-weather` |
| DynamicVertexBuffer.SetData | `pf-buffers` |
| Effect | `be-model` |
| Effect.Clone | `be-model` |
| EffectAnnotation | `be-model` |
| EffectParameter | `be-model` |
| EffectParameterClass | `be-model` |
| EffectParameterCollection | `be-model` |
| EffectParameterType | `be-model` |
| EffectPass | `be-model` |
| EffectPass.Apply | `pf-state` |
| EffectTechnique | `be-model` |
| EnableDefaultLighting | `lighting` |
| EnvironmentMapEffect | `tex-cube`, `rt-cube` |
| EnvironmentMapEffect.EnvironmentMap | `rt-cube` |
| EvaluateMorphWeightsEXT | `model-morph` |
| Game | `info`, `app-lifecycle`, `hist-xna` |
| Game.Activated | `pl-window` |
| Game.Components | `app-components` |
| Game.GetPlatformCapabilitiesEXT | `pl-platform` |
| Game.IsFixedTimeStep | `app-lifecycle`, `pf-dashboard` |
| Game.IsRunningSlowly | `pf-dashboard` |
| Game.LaunchParameters | `app-lifecycle` |
| Game.Services | `app-services` |
| Game.TargetElapsedTime | `app-lifecycle` |
| GameComponent | `app-components` |
| GameComponentCollection.ComponentAdded | `app-components` |
| GameComponentCollectionEventArgs | `app-components` |
| GamePad.GetCapabilities | `in-players` |
| GamePad.GetNameEXT | `in-players` |
| GamePad.GetState | `in-pad` |
| GamePad.KeyboardEmulationEnabledEXT | `in-pad` |
| GamePad.SetVibration | `in-pad` |
| GamePadCapabilities | `in-players` |
| GamePadDeadZone | `in-pad` |
| GamePadState | `in-pad` |
| GamePadState.IsButtonDown | `in-pad` |
| GameServiceContainer | `app-services` |
| GameServiceContainer.AddService | `app-services` |
| GameServiceContainer.GetService | `app-services` |
| GameServiceContainer.RemoveService | `app-services` |
| GameTime | `app-lifecycle`, `cam-chase`, `an-pendulum` |
| GameTime.ElapsedGameTime | `app-gametime`, `pf-dashboard` |
| GameTime.TotalGameTime | `app-gametime` |
| GameWindow.AllowUserResizing | `pl-window` |
| GameWindow.ClientBounds | `app-viewport`, `pl-window` |
| GameWindow.ClientSizeChanged | `pl-window` |
| GameWindow.FileDropEXT | `pl-window` |
| GameWindow.Title | `pl-window` |
| GestureType | `in-touch` |
| GltfImportDiagnosticEXT | `model-report` |
| GltfImportReportEXT | `model-report` |
| Graphics/RenderTargetChanges | `pf-targets` |
| Graphics/TextureBindingChanges | `pf-state` |
| GraphicsAdapter | `info`, `app-device`, `pl-adapter` |
| GraphicsCapability | `app-device` |
| GraphicsCapability.Instancing | `geo-instancing`, `pf-objects` |
| GraphicsCapability.MultiSampleAntiAliasing | `rt-msaa` |
| GraphicsCapability.OcclusionQuery | `rs-occlusion` |
| GraphicsCapability.WireFrame | `rs-fill` |
| GraphicsDevice | `info`, `app-device` |
| GraphicsDevice.DrawIndexedPrimitives | `geo-indexed`, `geo-manybuffers`, `geo-dynindex`, `pf-objects` |
| GraphicsDevice.DrawInstancedPrimitives | `geo-instancing`, `pf-objects` |
| GraphicsDevice.DrawPrimitives | `geo-indexed` |
| GraphicsDevice.DrawUserPrimitives | `geo-triangle`, `geo-primitives`, `cx-points`, `pf-buffers`, `pf-particles`, `rv-geometry` |
| GraphicsDevice.GetBackBufferData | `vr-index` |
| GraphicsDevice.GetGraphicsRendererName | `pl-renderer` |
| GraphicsDevice.GetRendererCapabilityProfileEXT | `pl-renderer` |
| GraphicsDevice.GraphicsProfile | `pl-adapter` |
| GraphicsDevice.ResourceCreated | `ct-lifetime` |
| GraphicsDevice.ResourceDestroyed | `ct-lifetime` |
| GraphicsDevice.ScissorRectangle | `sprite-scissor`, `rs-scissor`, `rv-geometry` |
| GraphicsDevice.SetRenderTarget | `rendertarget`, `rt-basic`, `rt-control`, `rt-cube`, `pf-targets`, `rv-targets` |
| GraphicsDevice.SetRenderTargets | `rt-mrt` |
| GraphicsDevice.SetStringMarkerEXT | `cx-points` |
| GraphicsDevice.SupportsSurfaceFormatAsRenderTargetEXT | `rt-formats` |
| GraphicsDevice.Viewport | `cam-split`, `rv-geometry` |
| GraphicsDeviceManager | `app-viewport` |
| GraphicsDeviceManager.DeviceReset | `app-viewport` |
| GraphicsDeviceManager.DeviceResetting | `app-viewport` |
| GraphicsDeviceStatus | `app-viewport` |
| GraphicsProfile | `hist-xna` |
| GraphicsResource.Dispose | `ct-lifetime` |
| GraphicsResource.Disposing | `ct-lifetime` |
| GraphicsResource.IsDisposed | `ct-cache`, `ct-lifetime` |
| HalfVector4 | `vt-packed` |
| IEffectFog | `be-model` |
| IEffectLights | `be-model` |
| IEffectMatrices | `be-model` |
| IGraphicsDeviceService | `app-services` |
| IPackedVector | `vt-packed` |
| IShadowReceiverEXT | `cx-shadow` |
| ImageBasedLightEXT | `model-ibl` |
| IndexBuffer | `particles`, `geo-indexed`, `geo-procedural`, `geo-index32`, `geo-dynindex`, `model-skinfx`, `pt-fire`, `pt-smoke`, `pt-sparks`, `pt-snow`, `pt-rain`, `pt-fountain`, `pt-dust`, `pt-fireworks`, `pt-bubbles`, `pt-leaves`, `pt-magic` |
| IndexElementSize | `geo-index32` |
| InputDevices.GetMiceEXT | `in-devices` |
| Joysticks.GetCapabilitiesEXT | `in-joystick` |
| Joysticks.GetJoysticksEXT | `in-joystick` |
| Joysticks.GetStateEXT | `in-joystick` |
| Keyboard.GetKeyNameEXT | `in-keyboard` |
| Keyboard.GetModStateEXT | `in-keyboard` |
| Keyboard.GetState | `in-keyboard` |
| KeyboardState | `in-edges` |
| KeyboardState.GetPressedKeys | `in-keyboard` |
| KeyboardState.IsKeyDown | `in-keyboard` |
| Keys | `in-edges` |
| LaunchParameters | `app-lifecycle` |
| LooseFileContentTypeReader | `ct-custom` |
| MathHelper | `an-pendulum`, `rf-deck` |
| MathHelper.CatmullRom | `curve-easing` |
| MathHelper.Hermite | `curve-easing` |
| MathHelper.Lerp | `curve-easing`, `an-doors` |
| MathHelper.SmoothStep | `curve-easing`, `an-doors` |
| MathHelper.WrapAngle | `an-walker` |
| Matrix | `models`, `animation` |
| Matrix.CreateBillboard | `mat-billboard` |
| Matrix.CreateConstrainedBillboard | `mat-billboard` |
| Matrix.CreateFromQuaternion | `quat-euler` |
| Matrix.CreateLookAt | `mat-view`, `cam-orbit`, `cam-chase`, `cam-topdown`, `cam-mirror`, `rt-mirror`, `rv-camera`, `rf-deck`, `rf-telescope`, `vr-index` |
| Matrix.CreateOrthographic | `mat-ortho`, `cam-topdown`, `cx-shadow`, `rv-camera` |
| Matrix.CreateOrthographicOffCenter | `mat-offcenter` |
| Matrix.CreatePerspective | `mat-offcenter` |
| Matrix.CreatePerspectiveFieldOfView | `mat-perspective`, `mat-ortho`, `col-frustum`, `cam-security`, `cam-fov`, `cam-clip`, `rv-camera`, `rf-telescope` |
| Matrix.CreatePerspectiveOffCenter | `mat-offcenter` |
| Matrix.CreateReflection | `mat-reflection` |
| Matrix.CreateRotationX | `quat-euler`, `an-gyroscope` |
| Matrix.CreateRotationY | `mat-order`, `an-gears`, `an-gyroscope` |
| Matrix.CreateRotationZ | `sprite-camera`, `an-clock`, `an-kinetic`, `an-gyroscope`, `an-walker` |
| Matrix.CreateScale | `mat-order`, `sprite-camera` |
| Matrix.CreateShadow | `rs-shadow` |
| Matrix.CreateTranslation | `mat-order`, `sprite-camera` |
| Matrix.CreateWorld | `cam-transition` |
| Matrix.Decompose | `mat-hierarchy` |
| Matrix.Determinant | `mat-reflection` |
| Matrix.Invert | `mat-view`, `mat-normals` |
| Matrix.Multiply | `mat-order`, `mat-hierarchy`, `mat-screen`, `an-clock`, `an-gears`, `an-kinetic`, `an-walker` |
| Matrix.Translation | `mat-hierarchy` |
| Matrix.Transpose | `mat-normals` |
| MediaPlayer.Play | `au-music` |
| MediaPlayer.Volume | `au-music` |
| MediaState | `au-music` |
| MeshBuilder | `an-gears` |
| Microphone | `au-mic` |
| Microphone.GetData | `au-mic` |
| Model | `models`, `animation`, `model-load` |
| Model.BoundingSphereEXT | `model-load`, `model-instances`, `model-bounds` |
| Model.CopyAbsoluteBoneTransformsTo | `model-rigid` |
| Model.Draw | `model-instances` |
| Model.MaterialVariantEXT | `model-variants` |
| Model.MaterialVariantNamesEXT | `model-variants` |
| ModelAnimationsEXT | `model-rigid` |
| ModelBone | `model-rigid` |
| ModelMesh | `models`, `model-load` |
| ModelMesh.BoundingSphere | `model-bounds` |
| ModelMeshPart | `model-load` |
| ModelMeshPart.Effect | `model-effects` |
| ModelMeshPart.Tag | `model-morph` |
| MorphTargetDataEXT | `model-morph` |
| Mouse.GetState | `in-mouse` |
| Mouse.IsRelativeMouseModeEXT | `in-mouse` |
| Mouse.SetCursor | `in-mouse` |
| MouseCursor | `in-mouse` |
| MouseState | `in-mouse` |
| NormalizedByte4 | `vt-packed` |
| NormalizedShort4 | `vt-packed` |
| OcclusionQuery | `rs-occlusion` |
| OcclusionQuery.IsComplete | `rs-occlusion` |
| OcclusionQuery.PixelCount | `rs-occlusion` |
| PackedValue | `vt-packed` |
| PackedVector.Alpha8 | `tex-formats` |
| PackedVector.Bgr565 | `tex-formats` |
| PackedVector.Bgra4444 | `tex-formats` |
| PackedVector.HalfVector4 | `tex-formats` |
| PbrEffect | `model-pbr`, `model-effects`, `model-variants` |
| PbrEffect.AlphaCutoff | `model-material` |
| PbrEffect.EmissiveFactor | `model-pbr` |
| PbrEffect.EmissiveMap | `model-material` |
| PbrEffect.ImageBasedLight | `model-ibl` |
| PbrEffect.MetallicFactor | `model-pbr`, `model-ibl` |
| PbrEffect.NormalMap | `model-material` |
| PbrEffect.RoughnessFactor | `model-pbr`, `model-ibl` |
| Plane | `vec-reflect`, `col-ray-plane` |
| Plane.DotCoordinate | `col-plane-box` |
| Plane.DotNormal | `col-plane-box` |
| Plane.Intersects | `col-plane-box` |
| Plane.Transform | `mat-reflection` |
| PlaneIntersectionType | `col-plane-box` |
| PlatformCapabilities | `pl-platform` |
| PlayerIndex | `in-players` |
| Point | `vt-rectangle` |
| Power.GetInfoEXT | `in-devices` |
| PreferPerPixelLighting | `lighting` |
| PresentInterval | `app-viewport` |
| PresentationParameters | `app-device`, `app-viewport`, `pl-adapter` |
| PrimitiveType | `geo-primitives` |
| PrimitiveType.LineList | `rf-weather` |
| PrimitiveType.PointListEXT | `cx-points`, `pf-particles` |
| PunctualLightEXT | `be-punctual`, `light-types`, `light-many` |
| PunctualLightEXT.DiffuseColor | `light-coloured` |
| PunctualLightEXT.OuterAngle | `light-flashlight` |
| PunctualLightEXT.Range | `light-falloff` |
| PunctualLightKindEXT.Spot | `light-flashlight` |
| Quaternion | `animation` |
| Quaternion.Concatenate | `quat-axis` |
| Quaternion.Conjugate | `quat-axis` |
| Quaternion.CreateFromAxisAngle | `quat-axis` |
| Quaternion.CreateFromYawPitchRoll | `quat-euler` |
| Quaternion.Dot | `quat-slerp` |
| Quaternion.Inverse | `quat-axis` |
| Quaternion.Lerp | `quat-slerp` |
| Quaternion.Normalize | `quat-slerp` |
| Quaternion.Slerp | `quat-slerp`, `cam-transition` |
| RasterizerState | `renderstates` |
| RasterizerState.CullMode | `geo-triangle`, `rs-cull`, `rt-mirror`, `rv-geometry` |
| RasterizerState.DepthBias | `rs-depthbias` |
| RasterizerState.FillMode | `geo-procedural`, `rs-fill` |
| RasterizerState.ScissorTestEnable | `sprite-scissor`, `rs-scissor` |
| RasterizerState.SlopeScaleDepthBias | `rs-depthbias` |
| Ray.Intersects | `col-ray-box`, `col-ray-sphere`, `col-ray-plane`, `col-pick` |
| Rectangle | `vt-rectangle`, `sprite-sheet`, `sprite-ui`, `sprite-anim` |
| Rectangle.Bottom | `vt-rectangle` |
| Rectangle.Contains | `vt-rectangle` |
| Rectangle.Inflate | `vt-rectangle` |
| Rectangle.Intersect | `vt-rectangle`, `sprite-hit` |
| Rectangle.Intersects | `vt-rectangle`, `sprite-hit` |
| Rectangle.IsEmpty | `vt-rectangle` |
| Rectangle.Location | `vt-rectangle` |
| Rectangle.Union | `vt-rectangle` |
| RenderTarget2D | `rendertarget`, `cam-orbit`, `cam-fov`, `cam-split`, `cam-mirror`, `sprite-basics`, `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-vertexwave`, `rs-scissor`, `rt-basic`, `rt-lowres`, `rt-control`, `rt-recursive`, `rt-mirror`, `rt-msaa`, `pf-targets`, `rv-targets`, `rf-telescope` |
| RenderTarget2D.Format | `rt-formats` |
| RenderTarget2D.MultiSampleCount | `rt-msaa` |
| RenderTargetBinding | `rt-mrt` |
| RenderTargetCube | `rt-cube` |
| RenderTargetUsage.PreserveContents | `rt-recursive` |
| RendererCapabilityProfile | `app-device` |
| RendererFeature | `pl-renderer` |
| RendererFormatUsage | `pl-renderer` |
| RendererLimit | `pl-renderer` |
| ResourceContentManager | `ct-resource` |
| ResourceCreatedEventArgs | `ct-lifetime` |
| ResourceDestroyedEventArgs | `ct-lifetime` |
| Rg32 | `vt-packed` |
| Rgba1010102 | `vt-packed` |
| Rgba64 | `vt-packed` |
| SamplerState | `materials`, `sprite-sampler` |
| SamplerState.AddressU | `tex-address`, `rv-sampling` |
| SamplerState.AnisotropicClamp | `tex-filter` |
| SamplerState.Filter | `tex-mip` |
| SamplerState.LinearClamp | `tex-filter`, `rv-sprites` |
| SamplerState.LinearWrap | `sprite-parallax`, `tex-uv`, `an-conveyor` |
| SamplerState.MaxAnisotropy | `tex-mip` |
| SamplerState.MaxMipLevel | `tex-mip` |
| SamplerState.MipMapLevelOfDetailBias | `tex-mip` |
| SamplerState.PointClamp | `sprite-sheet`, `tex-filter`, `rt-lowres`, `rv-sprites` |
| SamplerStateCollection | `materials` |
| Sensors.GetSensorsEXT | `in-devices` |
| SetDataOptions | `geo-dynamic` |
| SetDataOptions.Discard | `geo-dynindex`, `pt-fire`, `pt-smoke`, `pt-sparks`, `pt-snow`, `pt-rain`, `pt-fountain`, `pt-dust`, `pt-fireworks`, `pt-bubbles`, `pt-leaves`, `pt-magic`, `pf-buffers`, `rf-weather` |
| SetMorphWeightsEXT | `model-morph` |
| SetRuntimeMode | `cx-stats` |
| ShaderCodeEXT | `cx-package` |
| ShaderEffect | `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-vertexwave`, `rt-mrt`, `cx-shadow` |
| ShaderEffect(device, package) | `cx-package` |
| ShaderEffect.SetUniformFloat | `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-vertexwave` |
| ShaderLanguageEXT | `cx-package` |
| ShaderPackageEXT.selectFor | `cx-package` |
| ShadowCascadeStateEXT | `cx-shadow` |
| Short4 | `vt-packed` |
| SkinnedEffect | `be-model`, `model-skinfx` |
| SkinnedEffect.SetBoneTransforms | `model-skinfx` |
| SkinnedEffect.WeightsPerVertex | `model-skinfx` |
| SkinnedPbrEffect.SetBoneTransforms | `model-skinned` |
| SkinningData | `model-skinned` |
| Snapshot.metrics | `cx-stats` |
| Snapshot.resources | `cx-stats` |
| Song | `au-music` |
| SoundEffect | `audio`, `au-basics` |
| SoundEffect(buffer, sampleRate, channels) | `au-generate` |
| SoundEffect.DistanceScale | `au-spatial`, `au-master` |
| SoundEffect.DopplerScale | `au-spatial`, `au-master` |
| SoundEffect.MasterVolume | `au-master` |
| SoundEffect.Play | `au-basics` |
| SoundEffect.SpeedOfSound | `au-spatial`, `au-master` |
| SoundEffectInstance | `audio`, `au-layers`, `rf-garden` |
| SoundEffectInstance.Apply3D | `au-spatial` |
| SoundEffectInstance.IsLooped | `au-layers` |
| SoundEffectInstance.Pitch | `au-layers` |
| SoundEffectInstance.Volume | `au-layers` |
| SoundState | `au-layers` |
| SpriteBatch | `sprite-stress`, `pt-fire`, `pt-smoke`, `pt-sparks`, `pt-snow`, `pt-rain`, `pt-fountain`, `pt-dust`, `pt-fireworks`, `pt-bubbles`, `pt-leaves`, `pt-magic`, `rf-weather` |
| SpriteBatch.Begin | `sprite-basics`, `sprite-camera`, `pf-sprites` |
| SpriteBatch.Begin(effect) | `fx-grade`, `fx-blur`, `fx-pixelate`, `fx-ripple`, `fx-edge`, `fx-crt`, `fx-plasma`, `fx-vertexwave`, `cx-crt` |
| SpriteBatch.Draw | `sprite-basics`, `sprite-origin`, `sprite-sheet`, `sprite-sort`, `sprite-parallax`, `sprite-ui`, `sprite-atlas`, `sprite-hit`, `sprite-anim`, `sprite-stress`, `rt-recursive`, `pf-sprites`, `pf-overdraw`, `rv-sprites` |
| SpriteBatch.DrawString | `text-align`, `text-transform`, `text-hud`, `text-labels` |
| SpriteEffect | `be-model` |
| SpriteEffects | `sprite-basics`, `text-transform`, `rv-sprites` |
| SpriteEffects.FlipHorizontally | `sprite-anim` |
| SpriteFont.Characters | `text-glyphs` |
| SpriteFont.DefaultCharacter | `text-glyphs` |
| SpriteFont.LineSpacing | `text-measure` |
| SpriteFont.MeasureString | `text-align`, `text-measure` |
| SpriteFont.Spacing | `text-measure` |
| SpriteSortMode | `sprite-sort`, `pf-sprites` |
| StencilOperation | `rs-portal` |
| StencilOperation.Increment | `rs-shadow` |
| StorageContainer | `ct-storage` |
| StorageDevice | `ct-storage` |
| StorageDevice.GetStorageRootEXT | `ct-storage` |
| StringBuilder | `text-hud` |
| SurfaceFormat | `tex-formats`, `rt-formats` |
| System.IO.MemoryStream | `tex-io` |
| System.Resources.ResourceManager | `ct-resource` |
| TextInputEXT.StartTextInput | `in-text` |
| TextInputEXT.TextInput | `in-text` |
| Texture2D | `materials`, `app-content`, `tex-mip`, `rf-garden` |
| Texture2D.FromStream | `tex-io` |
| Texture2D.GetData | `sprite-atlas`, `sprite-hit`, `tex-dynamic`, `ct-formats`, `rv-targets` |
| Texture2D.SaveAsJpeg | `tex-io` |
| Texture2D.SaveAsPng | `tex-io` |
| Texture2D.SetData | `sprite-ui`, `sprite-atlas`, `sprite-anim`, `tex-dynamic` |
| Texture3D | `tex-volume` |
| Texture3D.GetData | `tex-volume` |
| Texture3D.SetData | `tex-volume` |
| TextureAddressMode | `sprite-sampler`, `tex-address`, `rv-sampling` |
| TextureCube | `tex-cube`, `model-ibl` |
| TextureFilter | `sprite-sampler`, `rv-sampling` |
| TextureTransformEXT | `model-material` |
| TitleContainer.OpenStream | `ct-storage` |
| TouchPanel.EnabledGestures | `in-touch` |
| TouchPanel.GetState | `in-touch`, `in-devices` |
| TouchPanel.MouseTouchEmulationEnabledEXT | `in-touch`, `in-devices` |
| TouchPanel.ReadGesture | `in-touch` |
| Vector2 | `sprite-origin` |
| Vector3 | `an-pendulum`, `rf-deck` |
| Vector3.Add | `vec-add` |
| Vector3.Barycentric | `curve-barycentric` |
| Vector3.CatmullRom | `vec-lerp`, `curve-spline` |
| Vector3.Cross | `vec-cross` |
| Vector3.Distance | `vec-normalize`, `an-doors` |
| Vector3.Dot | `vec-dot`, `vec-project` |
| Vector3.Length | `vec-cross`, `vec-normalize` |
| Vector3.Lerp | `vec-lerp`, `cam-chase` |
| Vector3.Multiply | `vec-project` |
| Vector3.Normalize | `vec-reflect`, `vec-normalize` |
| Vector3.Reflect | `vec-reflect` |
| Vector3.SmoothStep | `vec-lerp`, `cam-transition` |
| Vector3.Subtract | `vec-add` |
| Vector3.TransformNormal | `mat-normals` |
| Vector4.Transform | `mat-screen` |
| VertexBuffer | `geo-indexed`, `geo-procedural`, `geo-manybuffers`, `model-skinfx`, `pf-buffers` |
| VertexBufferBinding | `geo-instancing`, `pf-objects` |
| VertexPositionColor | `geo-triangle` |
| VertexPositionColorTexture | `tex-uv`, `an-conveyor` |
| VertexPositionNormalTexture | `geo-dynamic` |
| VertexPositionNormalTextureSkinned | `model-skinfx` |
| Viewport | `app-viewport`, `cam-split` |
| Viewport.Project | `mat-screen`, `text-labels` |
| Viewport.Unproject | `mat-screen` |
| XNA 4.0 | `hist-xna`, `hist-compare` |
| cna-content | `ct-pipeline` |
| layerDepth | `sprite-sort` |
<!-- END COVERAGE -->
