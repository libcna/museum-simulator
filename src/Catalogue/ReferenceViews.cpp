#include "Catalogue/ReferenceViews.hpp"

const std::vector<ReferenceView>& ReferenceViews()
{
    // Room spawns moved two metres closer to their subject; one further metre preserves
    // the established reference camera (formerly 1.6 m + 3.0 m inside the door).
    constexpr float kExhibitAdvance = 1.0f;
    static const std::vector<ReferenceView> views = {
        {"lobby", "hub-0", 0.0f, "the entrance hall: glass wall, rug, kiosk, floor plan"},
        {"sculpture-garden", "grounds-sculpture", kExhibitAdvance, "open-air kinetic sculptures and exterior lamps"},
        {"fountain-plaza", "grounds-fountain", kExhibitAdvance, "particle fountain, trees and wind-blown leaves"},
        {"vectors", "vec-cross", kExhibitAdvance, "Vector3 cross product with arrows and labels"},
        {"matrices", "mat-perspective", kExhibitAdvance, "projection matrix and view frustum"},
        {"collision", "col-ray-box", kExhibitAdvance, "ray against bounding box intersection"},
        {"camera", "cam-orbit", kExhibitAdvance, "orbit camera and its view frustum"},
        {"sprites", "sprite-basics", kExhibitAdvance, "SpriteBatch drawing and transforms"},
        {"blend", "sprite-blend", kExhibitAdvance, "eight BlendState configurations on a gradient"},
        {"text", "text-align", kExhibitAdvance, "SpriteFont alignment and measuring"},
        {"geometry", "geo-primitives", kExhibitAdvance, "primitive types from one vertex set"},
        {"instancing", "geo-instancing", kExhibitAdvance, "DrawInstancedPrimitives of a cube cloud"},
        {"mipmaps", "tex-mip", kExhibitAdvance, "coloured mip chain on a receding plane"},
        {"basiceffect", "be-combos", kExhibitAdvance, "BasicEffect feature combinations"},
        {"lights", "light-types", kExhibitAdvance, "directional, point and spot lights"},
        {"shader", "fx-crt", kExhibitAdvance, "a ShaderEffect post-process (CRT look)"},
        {"stencil", "rs-portal", kExhibitAdvance, "stencil buffer window into another world"},
        {"mirror", "rt-mirror", kExhibitAdvance, "planar mirror from a render target"},
        {"pbr", "model-pbr", kExhibitAdvance, "glTF model with PbrEffect"},
        {"gears", "an-gears", kExhibitAdvance, "meshing gear train driven by one angle"},
        {"fire", "pt-fire", kExhibitAdvance, "particle fire"},
        {"keyboard", "in-keyboard", kExhibitAdvance, "keyboard visualiser (nothing held)"},
        {"formats", "ct-formats", kExhibitAdvance, "loose image formats decoded by ContentManager"},
        {"debugdraw", "cx-debugdraw", kExhibitAdvance, "CNA DebugDraw wire shapes"},
        {"validation", "rv-geometry", kExhibitAdvance, "renderer self-check results"},
        {"deck", "rf-deck", kExhibitAdvance, "open-air observation deck at the museum clock's hour"},
        {"billboards", "mat-billboard", kExhibitAdvance, "CreateBillboard sprites tilting towards a high camera"},
        {"sprite-hit", "sprite-hit", kExhibitAdvance, "per-pixel collision with shared pixels highlighted"},
        {"terrain", "geo-terrain", kExhibitAdvance, "heightmap terrain with a ball following the ground"},
        {"colorwrite", "rs-colorwrite", kExhibitAdvance, "ColorWriteChannels quads (a turned room: they once faced the wall and the room looked empty)"},
        {"skinned-effect", "model-skinfx", kExhibitAdvance, "SkinnedEffect tube bent by four bones"},
        {"stencil-count", "rs-stencilcount", kExhibitAdvance, "stencil Increment counting overlapping discs"},
        {"cube-map", "rt-cube", kExhibitAdvance, "a sphere mirroring a RenderTargetCube drawn from its centre"},
        {"skinned-model-ext", "model-skinext", kExhibitAdvance, "SkinnedModelEXT tentacle posed by a keyframed clip, skeleton drawn"},
        {"gltf-cameras", "model-cameras", kExhibitAdvance, "three cameras of a glTF scene, each drawn through and as a frustum"},
    };
    return views;
}

const ReferenceView* FindReferenceView(const std::string& name)
{
    for (const ReferenceView& v : ReferenceViews())
        if (name == v.name) return &v;
    return nullptr;
}
