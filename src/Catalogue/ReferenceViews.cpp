#include "Catalogue/ReferenceViews.hpp"

const std::vector<ReferenceView>& ReferenceViews()
{
    static const std::vector<ReferenceView> views = {
        {"lobby", "hub-0", 0.0f, "the entrance hall: glass wall, rug, kiosk, floor plan"},
        {"vectors", "vec-cross", 3.0f, "Vector3 cross product with arrows and labels"},
        {"matrices", "mat-perspective", 3.0f, "projection matrix and view frustum"},
        {"collision", "col-ray-box", 3.0f, "ray against bounding box intersection"},
        {"camera", "cam-orbit", 3.0f, "orbit camera and its view frustum"},
        {"sprites", "sprite-basics", 3.0f, "SpriteBatch drawing and transforms"},
        {"blend", "sprite-blend", 3.0f, "eight BlendState configurations on a gradient"},
        {"text", "text-align", 3.0f, "SpriteFont alignment and measuring"},
        {"geometry", "geo-primitives", 3.0f, "primitive types from one vertex set"},
        {"instancing", "geo-instancing", 3.0f, "DrawInstancedPrimitives of a cube cloud"},
        {"mipmaps", "tex-mip", 3.0f, "coloured mip chain on a receding plane"},
        {"basiceffect", "be-combos", 3.0f, "BasicEffect feature combinations"},
        {"lights", "light-types", 3.0f, "directional, point and spot lights"},
        {"shader", "fx-crt", 3.0f, "a ShaderEffect post-process (CRT look)"},
        {"stencil", "rs-portal", 3.0f, "stencil buffer window into another world"},
        {"mirror", "rt-mirror", 3.0f, "planar mirror from a render target"},
        {"pbr", "model-pbr", 3.0f, "glTF model with PbrEffect"},
        {"gears", "an-gears", 3.0f, "meshing gear train driven by one angle"},
        {"fire", "pt-fire", 3.0f, "particle fire"},
        {"keyboard", "in-keyboard", 3.0f, "keyboard visualiser (nothing held)"},
        {"formats", "ct-formats", 3.0f, "loose image formats decoded by ContentManager"},
        {"debugdraw", "cx-debugdraw", 3.0f, "CNA DebugDraw wire shapes"},
        {"validation", "rv-geometry", 3.0f, "renderer self-check results"},
        {"deck", "rf-deck", 3.0f, "open-air observation deck at the museum clock's hour"},
        {"billboards", "mat-billboard", 3.0f, "CreateBillboard sprites tilting towards a high camera"},
        {"sprite-hit", "sprite-hit", 3.0f, "per-pixel collision with shared pixels highlighted"},
        {"terrain", "geo-terrain", 3.0f, "heightmap terrain with a ball following the ground"},
        {"skinned-effect", "model-skinfx", 3.0f, "SkinnedEffect tube bent by four bones"},
        {"stencil-count", "rs-stencilcount", 3.0f, "stencil Increment counting overlapping discs"},
        {"cube-map", "rt-cube", 3.0f, "a sphere mirroring a RenderTargetCube drawn from its centre"},
        {"skinned-model-ext", "model-skinext", 3.0f, "SkinnedModelEXT tentacle posed by a keyframed clip, skeleton drawn"},
        {"gltf-cameras", "model-cameras", 3.0f, "three cameras of a glTF scene, each drawn through and as a frustum"},
    };
    return views;
}

const ReferenceView* FindReferenceView(const std::string& name)
{
    for (const ReferenceView& v : ReferenceViews())
        if (name == v.name) return &v;
    return nullptr;
}
