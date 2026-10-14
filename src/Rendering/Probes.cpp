#include "Rendering/Probes.hpp"

#include <exception>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/ColorMatrixEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/PunctualLightEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Rendering/RenderStates.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace Probes {

Answer PunctualLights(GraphicsDevice& d)
{
    static Answer cached = Answer::Unknown;
    if (cached != Answer::Unknown) return cached;
    try {
        constexpr int kSize = 16;
        RenderTarget2D rt(d, kSize, kSize, false, SurfaceFormat::Color, DepthFormat::None);
        RenderStates::UnbindTextures(d);
        d.SetRenderTarget(&rt);
        d.setViewportProperty(Viewport(0, 0, kSize, kSize));
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::None);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        d.Clear(Color(0, 0, 0, 255));

        // A white wall facing the camera, lit by nothing but one point light in front of it.
        BasicEffect fx(d);
        fx.setLightingEnabledProperty(true);
        fx.setTextureEnabledProperty(false);
        fx.setVertexColorEnabledProperty(false);
        fx.setFogEnabledProperty(false);
        fx.setPreferPerPixelLightingProperty(true);
        fx.setAmbientLightColorProperty(Vector3(0, 0, 0));
        fx.setDiffuseColorProperty(Vector3(1, 1, 1));
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setSpecularColorProperty(Vector3(0, 0, 0));
        // A barely-there directional light keeps the lit shader selected on renderers that skip lighting when no light is on.
        fx.getDirectionalLight0Property().setEnabledProperty(true);
        fx.getDirectionalLight0Property().setDirectionProperty(Vector3(0, -1, 0));
        fx.getDirectionalLight0Property().setDiffuseColorProperty(Vector3(0.02f, 0.02f, 0.02f));
        fx.getDirectionalLight1Property().setEnabledProperty(false);
        fx.getDirectionalLight2Property().setEnabledProperty(false);
        PunctualLightEXT light;
        light.Kind = PunctualLightKindEXT::Point;
        light.Position = Vector3(0, 0, 1.0f);
        light.Range = 10.0f;
        light.DiffuseColor = Vector3(1, 1, 1) * 6.0f;
        fx.setPunctualLightEXT(light);
        fx.setWorldProperty(Matrix::getIdentityProperty());
        fx.setViewProperty(Matrix::CreateLookAt(Vector3(0, 0, 3), Vector3::Zero, Vector3::Up));
        fx.setProjectionProperty(Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(60.0f), 1.0f, 0.1f, 20.0f));
        const Vector3 n(0, 0, 1);
        const VertexPositionNormalTexture tl(Vector3(-1.5f, 1.5f, 0), n, Vector2(0, 0)), tr(Vector3(1.5f, 1.5f, 0), n, Vector2(1, 0)),
            bl(Vector3(-1.5f, -1.5f, 0), n, Vector2(0, 1)), br(Vector3(1.5f, -1.5f, 0), n, Vector2(1, 1));
        const VertexPositionNormalTexture quad[6] = {tl, bl, br, tl, br, tr};
        for (auto& pass : fx.getCurrentTechniqueProperty()->getPassesProperty()) pass.Apply();
        d.DrawUserPrimitives(PrimitiveType::TriangleList, quad, 0, 2);
        d.SetRenderTarget(nullptr);

        std::vector<Color> pixels(static_cast<std::size_t>(kSize * kSize));
        rt.GetData(pixels.data(), static_cast<int>(pixels.size()));
        const Color centre = pixels[static_cast<std::size_t>(kSize / 2 * kSize + kSize / 2)];
        cached = centre.getRProperty() > 100 ? Answer::Yes : Answer::No;
        Museum::Log("probe", "PunctualLightEXT: centre pixel %d,%d,%d -> %s", centre.getRProperty(), centre.getGProperty(), centre.getBProperty(),
                    cached == Answer::Yes ? "lights the surface" : "ignored by this renderer");
    } catch (const std::exception& e) {
        d.SetRenderTarget(nullptr);
        Museum::Log("probe", "PunctualLightEXT probe failed: %s", e.what());
        cached = Answer::No;
    }
    return cached;
}

Answer ColorMatrix(GraphicsDevice& d)
{
    static Answer cached = Answer::Unknown;
    if (cached != Answer::Unknown) return cached;
    try {
        constexpr int kSize = 8;
        RenderTarget2D rt(d, kSize, kSize, false, SurfaceFormat::Color, DepthFormat::None);
        const std::vector<std::uint8_t> whitePixel = {255, 255, 255, 255};
        Texture2D white = Texture2D::CreateFromPixels(d, 1, 1, whitePixel);
        RenderStates::UnbindTextures(d);
        d.SetRenderTarget(&rt);
        d.setViewportProperty(Viewport(0, 0, kSize, kSize));
        d.Clear(Color(0, 0, 0, 255));
        // A matrix that keeps only blue: a renderer that runs the effect turns white into (0,0,255); one that ignores it leaves white.
        ColorMatrixEffect effect(d);
        effect.SetColorMatrix({0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1});
        SpriteBatch batch(d);
        batch.Begin(SpriteSortMode::Immediate, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr, &effect);
        batch.Draw(white, Rectangle(0, 0, kSize, kSize), Color::White);
        batch.End();
        d.SetRenderTarget(nullptr);
        std::vector<Color> pixels(static_cast<std::size_t>(kSize * kSize));
        rt.GetData(pixels.data(), static_cast<int>(pixels.size()));
        const Color c = pixels[static_cast<std::size_t>(kSize / 2 * kSize + kSize / 2)];
        cached = (c.getRProperty() < 40 && c.getGProperty() < 40 && c.getBProperty() > 200) ? Answer::Yes : Answer::No;
        Museum::Log("probe", "ColorMatrixEffect: centre pixel %d,%d,%d -> %s", c.getRProperty(), c.getGProperty(), c.getBProperty(), cached == Answer::Yes ? "applied" : "ignored by this renderer");
    } catch (const std::exception& e) {
        d.SetRenderTarget(nullptr);
        Museum::Log("probe", "ColorMatrixEffect probe failed: %s", e.what());
        cached = Answer::No;
    }
    return cached;
}

const char* Describe(Answer answer)
{
    switch (answer) {
        case Answer::Yes: return "lights surfaces";
        case Answer::No: return "IGNORED by this renderer";
        default: return "not tested yet";
    }
}

}  // namespace Probes
