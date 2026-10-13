// Floor 5: render state and render targets.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {


RoomSpec QueryRoom(const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis)
{
    RoomSpec r;
    r.key = key;
    r.title = title;
    r.subtitle = subtitle;
    r.exhibit = "query-lab";
    r.params = {{"mode", mode}};
    r.width = 14;
    r.depth = 13;
    r.height = 5.2f;
    r.theme = "white-cube";
    r.apis = std::move(apis);
    return r;
}

WingSpec StateWing()
{
    WingSpec w;
    w.name = "Render State Gallery";
    w.tagline = "RasterizerState, DepthStencilState and BlendState, one idea per room";
    w.theme = "corridor-cool";
    w.accent = {0.95f, 0.55f, 0.55f};
    w.spineSide = Wall::East;
    const auto rs = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "renderstate-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "white-cube";
        r.apis = std::move(apis);
        return r;
    };
    w.rooms = {
        rs("rs-cull", "Culling", "CullMode on a cube you can stand inside: outside, inside or both", "cull", {"RasterizerState.CullMode", "CullMode"}),
        rs("rs-fill", "Fill Mode", "Solid against WireFrame, with the renderer's native or emulated wireframe", "fill", {"RasterizerState.FillMode", "GraphicsCapability.WireFrame"}),
        rs("rs-depthbias", "Depth Bias and Z-Fighting", "Coplanar decals with DepthBias and SlopeScaleDepthBias", "depthbias", {"RasterizerState.DepthBias", "RasterizerState.SlopeScaleDepthBias"}),
        rs("rs-scissor", "Scissor Rectangle", "ScissorTestEnable clipping a 3D scene inside a render target", "scissor", {"RasterizerState.ScissorTestEnable", "GraphicsDevice.ScissorRectangle", "RenderTarget2D"}),
        rs("rs-depthfunc", "Depth Test", "Every DepthBufferFunction with enable and write switches and draw order", "depthfunc", {"DepthStencilState.DepthBufferFunction", "DepthStencilState.DepthBufferEnable", "CompareFunction"}),
        rs("rs-depthread", "Depth Read-Only for Glass", "Translucent panes with and without depth writes, sorted and unsorted", "depthread", {"DepthStencilState.DepthRead", "DepthStencilState.DepthBufferWriteEnable"}),
        rs("rs-portal", "Stencil: a Portal", "A window cut with the stencil buffer into another world", "portal", {"DepthStencilState.StencilEnable", "DepthStencilState.StencilFunction", "StencilOperation", "ColorWriteChannels"}),
        rs("rs-outline", "Stencil: an Outline", "A rim around objects made with two stencil passes", "outline", {"DepthStencilState.ReferenceStencil", "DepthStencilState.StencilPass", "CompareFunction.NotEqual"}),
        rs("rs-shadow", "Stencil: a Planar Shadow", "Matrix.CreateShadow flattened shadows, darkened once thanks to the stencil test", "shadow", {"Matrix.CreateShadow", "DepthStencilState.StencilPass", "StencilOperation.Increment"}),
        rs("rs-blend", "BlendState Gallery", "Sixteen blend factor and function combinations on the same sprites", "blendgallery", {"BlendState", "Blend", "BlendFunction"}),
        rs("rs-colorwrite", "Color Write Channels", "Masking red, green, blue and alpha writes", "colorwrite", {"BlendState.ColorWriteChannels", "ColorWriteChannels"}),
        rs("rs-blendfactor", "Blend Factor", "Blend.BlendFactor mixing by a constant colour from the BlendState", "blendfactor", {"BlendState.BlendFactor", "Blend.BlendFactor", "Blend.InverseBlendFactor"}),
        QueryRoom("rs-occlusion", "Occlusion Query", "Counting the pixels of an object that survive the depth test, and skipping what cannot be seen", "occlusion", {"OcclusionQuery", "OcclusionQuery.PixelCount", "OcclusionQuery.IsComplete", "GraphicsCapability.OcclusionQuery"}),
    };
    return w;
}

WingSpec TargetWing()
{
    WingSpec w;
    w.name = "Render Target Complex";
    w.tagline = "Off-screen rendering: screens, mirrors, feedback, low resolution and MRT";
    w.theme = "control-room";
    w.accent = {0.45f, 0.85f, 0.95f};
    w.spineSide = Wall::West;
    const auto rt = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin = ApiOrigin::Xna) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "targets";
        r.params = {{"mode", mode}};
        r.width = 14;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "white-cube";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        rt("rt-basic", "Render Target Basics", "RenderTarget2D size, depth format, mip levels and content state", "basic", {"RenderTarget2D", "GraphicsDevice.SetRenderTarget", "DepthFormat"}),
        rt("rt-lowres", "Low-Resolution Rendering", "A scene drawn tiny and scaled up with Point or Linear sampling", "lowres", {"RenderTarget2D", "SamplerState.PointClamp"}),
        rt("rt-control", "Control Room", "Six cameras, six render targets, switched to and from the back buffer every frame", "control", {"RenderTarget2D", "GraphicsDevice.SetRenderTarget"}),
        rt("rt-recursive", "Feedback", "Two render targets ping-pong so a screen shows itself", "recursive", {"RenderTarget2D", "RenderTargetUsage.PreserveContents", "SpriteBatch.Draw"}),
        rt("rt-mirror", "A Planar Mirror", "A reflected camera rendered to a texture and projected back onto the wall", "mirror", {"RenderTarget2D", "Matrix.CreateLookAt", "RasterizerState.CullMode"}),
        rt("rt-mrt", "Multiple Render Targets", "One pass writing a grayscale image and an edge map at once", "mrt", {"GraphicsDevice.SetRenderTargets", "RenderTargetBinding", "ShaderEffect"}, ApiOrigin::CnaExtension),
        rt("rt-formats", "Render Target Formats", "Which SurfaceFormats can be rendered to on this renderer, tested by doing it", "formats", {"SurfaceFormat", "RenderTarget2D.Format", "GraphicsDevice.SupportsSurfaceFormatAsRenderTargetEXT"}, ApiOrigin::CnaExtension),
        QueryRoom("rt-msaa", "Multisampled Targets", "The same edges at 0, 2, 4 and 8 samples, magnified", "msaa", {"RenderTarget2D.MultiSampleCount", "GraphicsCapability.MultiSampleAntiAliasing", "RenderTarget2D"}),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor5()
{
    FloorSpec f;
    f.index = 5;
    f.name = "Render State and Render Targets";
    f.tagline = "RasterizerState, DepthStencilState, BlendState and off-screen rendering";
    f.theme = "concourse";
    f.accent = {0.95f, 0.55f, 0.55f};
    f.wings.push_back(StateWing());
    f.wings.push_back(TargetWing());
    return f;
}
