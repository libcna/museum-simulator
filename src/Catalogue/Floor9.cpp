// Floor 9: performance, validation and the rooftop.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec PerformanceWing()
{
    WingSpec w;
    w.name = "Performance Lab";
    w.tagline = "Stress levels for draw calls, sprites, state changes, uploads, render targets, overdraw and particles";
    w.theme = "industrial";
    w.accent = {0.95f, 0.30f, 0.30f};
    w.spineSide = Wall::East;
    const auto pf = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin = ApiOrigin::Xna) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "perf-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "industrial";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        pf("pf-dashboard", "Frame-Time Dashboard", "Update-time history, 1% lows and the engine's own counters", "dashboard", {"GameTime.ElapsedGameTime", "Game.IsRunningSlowly", "CNA::Diagnostics::GetProvider", "Game.IsFixedTimeStep"}, ApiOrigin::CnaExtension),
        pf("pf-objects", "Draw Calls and Instancing", "10 to 20,000 cubes: a call each, or DrawInstancedPrimitives", "objects", {"GraphicsDevice.DrawIndexedPrimitives", "GraphicsDevice.DrawInstancedPrimitives", "VertexBufferBinding", "GraphicsCapability.Instancing"}),
        pf("pf-sprites", "SpriteBatch Volume", "100 to 60,000 sprites with sort modes and texture counts", "sprites", {"SpriteBatch.Draw", "SpriteSortMode", "SpriteBatch.Begin"}),
        pf("pf-state", "State Changes and Batching", "The same quads drawn with and without sorting by texture", "state", {"BasicEffect.Texture", "EffectPass.Apply", "Graphics/TextureBindingChanges"}),
        pf("pf-buffers", "Buffer Uploads", "DynamicVertexBuffer, DrawUserPrimitives and static buffers compared", "buffers", {"DynamicVertexBuffer.SetData", "SetDataOptions.Discard", "GraphicsDevice.DrawUserPrimitives", "VertexBuffer"}),
        pf("pf-targets", "Render-Target Switches", "1 to 32 small render targets drawn every frame", "targets", {"GraphicsDevice.SetRenderTarget", "RenderTarget2D", "Graphics/RenderTargetChanges"}),
        pf("pf-overdraw", "Overdraw", "1 to 32 full-screen translucent layers", "overdraw", {"SpriteBatch.Draw", "BlendState.NonPremultiplied"}),
        pf("pf-particles", "CPU Particles", "1,000 to 250,000 particles updated on the CPU and drawn as points", "particles", {"PrimitiveType.PointListEXT", "GraphicsDevice.DrawUserPrimitives"}, ApiOrigin::CnaExtension),
    };
    return w;
}


WingSpec ValidationWing()
{
    WingSpec w;
    w.name = "Renderer Validation";
    w.tagline = "Self-checking test images: every renderer must pass the same checks with the same stock calls";
    w.theme = "neon-lab";
    w.accent = {0.95f, 0.30f, 0.30f};
    w.spineSide = Wall::West;
    const auto rv = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "validation-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "neon-lab";
        r.apis = std::move(apis);
        r.origin = ApiOrigin::Xna;
        return r;
    };
    w.rooms = {
        rv("rv-geometry", "Geometry and Coverage", "Culling, viewport, scissor and exact pixel coverage of quads", "geometry", {"RasterizerState.CullMode", "GraphicsDevice.Viewport", "GraphicsDevice.ScissorRectangle", "GraphicsDevice.DrawUserPrimitives"}),
        rv("rv-sprites", "SpriteBatch Pixels", "Texel-exact 1:1 drawing, scaling, flips, rotation and filtering", "sprites", {"SpriteBatch.Draw", "SpriteEffects", "SamplerState.PointClamp", "SamplerState.LinearClamp"}),
        rv("rv-blend", "Blending", "Alpha, premultiplied, additive, masks, blend factor and Max", "blend", {"BlendState", "BlendFunction", "ColorWriteChannels", "Blend.BlendFactor"}),
        rv("rv-depth", "Depth Buffer", "Depth test, functions, write masks and precision", "depth", {"DepthStencilState", "CompareFunction", "DepthFormat.Depth24"}),
        rv("rv-shading", "Interpolation, Lighting and Fog", "Vertex colours, N.L diffuse lighting and linear fog", "shading", {"BasicEffect.VertexColorEnabled", "BasicEffect.DirectionalLight0", "BasicEffect.FogEnabled"}),
        rv("rv-sampling", "Texture Sampling", "Wrap, Clamp, Mirror, point and linear filtering", "sampling", {"SamplerState.AddressU", "TextureAddressMode", "TextureFilter"}),
        rv("rv-targets", "Render Targets", "Orientation, reuse as a texture and half-float targets", "targets", {"RenderTarget2D", "GraphicsDevice.SetRenderTarget", "Texture2D.GetData"}),
        rv("rv-camera", "Camera Matrices", "LookAt, perspective and orthographic projection, world translation", "camera", {"Matrix.CreateLookAt", "Matrix.CreatePerspectiveFieldOfView", "Matrix.CreateOrthographic", "BasicEffect.World"}),
        rv("rv-effects", "Stock Effects", "BasicEffect, AlphaTestEffect and DualTextureEffect produce the expected pixels", "effects", {"BasicEffect", "AlphaTestEffect", "DualTextureEffect"}),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor9()
{
    FloorSpec f;
    f.index = 9;
    f.name = "Performance and Validation";
    f.tagline = "Stress tests, renderer self-checks, reference scenes and the observation deck";
    f.theme = "concourse";
    f.accent = {0.95f, 0.30f, 0.30f};
    f.wings.push_back(PerformanceWing());
    f.wings.push_back(ValidationWing());
    return f;
}
