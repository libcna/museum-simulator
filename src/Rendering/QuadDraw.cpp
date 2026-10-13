#include "Rendering/QuadDraw.hpp"

#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Rendering/EffectBank.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

void Submit(const DrawContext& ctx, BasicEffect& fx, const Vector3& o, const Vector3& U, const Vector3& V, const Color& c)
{
    const Vector3 a = o, b = o + U, d = o + U + V, e = o + V;
    const VertexPositionColorTexture verts[6] = {
        {a, c, Vector2(0, 1)}, {b, c, Vector2(1, 1)}, {d, c, Vector2(1, 0)},
        {a, c, Vector2(0, 1)}, {d, c, Vector2(1, 0)}, {e, c, Vector2(0, 0)},
    };
    fx.setWorldProperty(Matrix::getIdentityProperty());
    fx.setViewProperty(ctx.view.view);
    fx.setProjectionProperty(ctx.view.projection);
    EffectBank::Apply(fx);
    ctx.device.DrawUserPrimitives(PrimitiveType::TriangleList, verts, 0, 2);
}

}  // namespace

void QuadDraw::Textured(const DrawContext& ctx, Texture2D* texture, const Vector3& origin, const Vector3& U,
                        const Vector3& V, const Color& tint)
{
    BasicEffect& fx = ctx.fx.World();
    fx.setTextureProperty(texture);
    fx.setDiffuseColorProperty(Vector3(1, 1, 1));
    Submit(ctx, fx, origin, U, V, tint);
}

void QuadDraw::Coloured(const DrawContext& ctx, const Vector3& origin, const Vector3& U, const Vector3& V,
                        const Color& colour)
{
    BasicEffect& fx = ctx.fx.Flat();
    Submit(ctx, fx, origin, U, V, colour);
}
