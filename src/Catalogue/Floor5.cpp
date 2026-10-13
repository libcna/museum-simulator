// Floor 5: render state and render targets.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

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
    return f;
}
