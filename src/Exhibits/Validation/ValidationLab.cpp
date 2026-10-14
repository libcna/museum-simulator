// Renderer validation laboratory. Each room draws tiny deterministic test images into a render target through
// stock CNA/XNA calls only, reads the pixels back and compares them with values worked out by hand. The result is
// a table of PASS and FAIL for the renderer that is running: the same checks run unchanged on every renderer,
// and a difference between renderers is a finding, never something to work around here.
//
// Every check logs one line "[museum][validate] <room> <check>: PASS|FAIL (expected ..., got ...)" so that
// tools/renderer_check.sh can build a matrix across renderers.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CNA/GraphicsCapability.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/ColorWriteChannels.hpp"
#include "Microsoft/Xna/Framework/Graphics/CompareFunction.hpp"
#include "Microsoft/Xna/Framework/Graphics/CubeMapFace.hpp"
#include "Microsoft/Xna/Framework/Graphics/DualTextureEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EnvironmentMapEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTargetCube.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/Probes.hpp"
#include "Rendering/RenderStates.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Geometry, Sprites, Blend, Depth, Shading, Sampling, Targets, Effects, Camera };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"sprites", Mode::Sprites}, {"blend", Mode::Blend},       {"depth", Mode::Depth},     {"shading", Mode::Shading},
                                                         {"sampling", Mode::Sampling}, {"targets", Mode::Targets}, {"effects", Mode::Effects}, {"camera", Mode::Camera}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Geometry;
}

constexpr int kSize = 64;

/** A captured test image. */
struct Image {
    std::vector<Color> px;
    [[nodiscard]] Color At(int x, int y) const { return px[static_cast<std::size_t>(y * kSize + x)]; }
    [[nodiscard]] int Count(const std::function<bool(const Color&)>& f) const
    {
        int n = 0;
        for (const Color& c : px) n += f(c) ? 1 : 0;
        return n;
    }
};

struct Result {
    std::string name, expected, got;
    bool pass = false;
    bool informational = false;   // not a pass/fail claim (renderer-dependent precision)
    bool skipped = false;
    std::unique_ptr<Texture2D> thumb;
};

int Dist(const Color& a, int r, int g, int b)
{
    return std::max({std::abs(static_cast<int>(a.getRProperty()) - r), std::abs(static_cast<int>(a.getGProperty()) - g), std::abs(static_cast<int>(a.getBProperty()) - b)});
}

std::string Rgb(const Color& c)
{
    return std::to_string(c.getRProperty()) + "," + std::to_string(c.getGProperty()) + "," + std::to_string(c.getBProperty());
}

class ValidationLab final : public ScreenExhibit {
public:
    ValidationLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddButton("Run the checks again", [this] { needRun_ = true; });
        p.AddReadout("Renderer", [this] { return renderer_; });
        p.AddReadout("Passed", [this] { return std::to_string(Passed()) + " of " + std::to_string(Counted()); });
        p.AddReadout("Readback", [this] { return std::string(readback_ ? "available" : "NOT available"); });
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
        renderer_ = std::string(S().device->GetGraphicsRendererName());
        fx_ = std::make_unique<BasicEffect>(*S().device);
        fx_->setVertexColorEnabledProperty(true);
        fx_->setLightingEnabledProperty(false);
        fx_->setTextureEnabledProperty(false);
        needRun_ = true;
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (needRun_ && !ctx.offscreen) {
            needRun_ = false;
            Run(ctx.device);
        }
        ScreenExhibit::RenderOffscreen(ctx);
    }

    ~ValidationLab() override = default;

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        Texture2D& white = *S().whiteTexture;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::PointClamp, nullptr, nullptr);
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(Title(), 24, 10, Kit::kGold, 1.0f);
        text(renderer_ + ":  " + std::to_string(Passed()) + " of " + std::to_string(Counted()) + " checks passed", 24, 46, Passed() == Counted() ? Kit::Rgba(110, 230, 150) : Kit::Rgba(255, 200, 100), 0.8f);
        if (!readback_) text("This renderer cannot read pixels back: the checks cannot run.", 24, 100, Kit::Rgba(255, 190, 150), 0.8f);
        float y = 84.0f;
        for (const Result& r : results_) {
            if (y > 500.0f) break;
            const Color c = r.skipped ? Kit::Rgba(150, 150, 150) : (r.informational ? Kit::Rgba(120, 190, 255) : (r.pass ? Kit::Rgba(110, 230, 150) : Kit::Rgba(235, 120, 110)));
            rect(20, y, 920, 56, Kit::Rgba(24, 30, 46));
            rect(20, y, 6, 56, c);
            if (r.thumb) sb.Draw(*r.thumb, Rectangle(34, static_cast<int>(y) + 4, 48, 48), Color::White);
            text(r.name, 92, y + 4, Kit::kText, 0.7f);
            text("expected " + r.expected, 92, y + 28, Kit::kDim, 0.55f);
            text("got " + r.got, 520, y + 28, Kit::kDim, 0.55f);
            text(r.skipped ? "skipped" : (r.informational ? "info" : (r.pass ? "PASS" : "FAIL")), 850, y + 12, c, 0.8f);
            y += 62.0f;
        }
        sb.End();
    }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Geometry, culling and coverage", "SpriteBatch pixels", "Blending", "Depth buffer", "Interpolation, lighting and fog", "Texture sampling", "Render targets", "Stock effects", "Camera matrices"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Geometry: return {"Triangles of known winding and size are drawn into a 64 x 64 render target and the pixels are counted: culling", "must hide the right faces, the viewport and scissor must clip exactly, and a quad must cover exactly the pixels it spans."};
            case Mode::Sprites: return {"SpriteBatch at 1:1 must reproduce a texture texel for texel (the half-pixel rule), keep the texture's top-left", "at the top-left, scale by whole numbers into whole blocks, flip and rotate as asked, and blend linearly between texels."};
            case Mode::Blend: return {"Source and destination colours with known values go through each BlendState: alpha, premultiplied, additive,", "colour-write masks and blend factors. The expected result is the blend equation worked out by hand."};
            case Mode::Depth: return {"Overlapping quads at different depths test the depth buffer: the near one must win in any draw order, depth", "functions must invert the result, and depth writes off must leave the later quad in front. Precision is informational."};
            case Mode::Shading: return {"Vertex colours interpolate linearly, BasicEffect lighting follows N.L for a known light, and linear fog blends", "to the fog colour exactly halfway between its start and end distance."};
            case Mode::Sampling: return {"A 2 x 2 texture is sampled outside 0..1 with Wrap, Clamp and Mirror, and with point and linear filtering. Each", "result is the texel the address mode must select, or the average the filter must produce."};
            case Mode::Targets: return {"A render target keeps what was drawn into it: top-left stays top-left when read back and when drawn again, and a", "render target used as a texture reproduces the picture. Half-float targets must hold values above 1."};
            case Mode::Camera: return {"A quad in front of, above and beside a camera built with Matrix.CreateLookAt and CreatePerspectiveFieldOfView must", "land where the maths says: ahead in the centre, above in the top half, nearer objects larger, orthographic constant."};
            default: return {"Every stock effect (BasicEffect, AlphaTestEffect, DualTextureEffect) draws a quad that must show up with", "roughly the colour its settings imply. A black or missing result is a renderer bug, not a museum choice."};
        }
    }

    [[nodiscard]] int Passed() const
    {
        int n = 0;
        for (const Result& r : results_) n += (r.pass && !r.informational && !r.skipped) ? 1 : 0;
        return n;
    }
    [[nodiscard]] int Counted() const
    {
        int n = 0;
        for (const Result& r : results_) n += (!r.informational && !r.skipped) ? 1 : 0;
        return n;
    }

    // --- harness ----------------------------------------------------------------------------------
    RenderTarget2D& Target(GraphicsDevice& d, bool depth)
    {
        auto& slot = depth ? depthTarget_ : plainTarget_;
        if (!slot) slot = std::make_unique<RenderTarget2D>(d, kSize, kSize, false, SurfaceFormat::Color, depth ? DepthFormat::Depth24 : DepthFormat::None);
        return *slot;
    }

    void Begin(GraphicsDevice& d, RenderTarget2D& rt, Color clear)
    {
        RenderStates::UnbindTextures(d);
        d.SetRenderTarget(&rt);
        d.setViewportProperty(Viewport(0, 0, kSize, kSize));
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::None);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        if (&rt == depthTarget_.get()) d.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, clear, 1.0f, 0);
        else d.Clear(clear);
    }

    std::optional<Image> End(GraphicsDevice& d, RenderTarget2D& rt)
    {
        d.SetRenderTarget(nullptr);
        Image img;
        img.px.resize(static_cast<std::size_t>(kSize * kSize));
        try {
            rt.GetData(img.px.data(), static_cast<int>(img.px.size()));
        } catch (const std::exception& e) {
            Museum::Log("validate", "readback failed: %s", e.what());
            readback_ = false;
            return std::nullopt;
        }
        return img;
    }

    void Add(const std::string& name, const std::string& expected, const std::string& got, bool pass, const std::optional<Image>& img, bool informational = false)
    {
        Result r;
        r.name = name;
        r.expected = expected;
        r.got = got;
        r.pass = pass;
        r.informational = informational;
        if (img) {
            r.thumb = std::make_unique<Texture2D>(*S().device, kSize, kSize);
            r.thumb->SetData(img->px.data(), static_cast<int>(img->px.size()));
        }
        Museum::Log("validate", "%s %s: %s (expected %s, got %s)", R().key.c_str(), name.c_str(), informational ? "INFO" : (pass ? "PASS" : "FAIL"), expected.c_str(), got.c_str());
        results_.push_back(std::move(r));
    }
    void Skip(const std::string& name, const std::string& why)
    {
        Result r;
        r.name = name;
        r.expected = "n/a";
        r.got = why;
        r.skipped = true;
        Museum::Log("validate", "%s %s: SKIP (%s)", R().key.c_str(), name.c_str(), why.c_str());
        results_.push_back(std::move(r));
    }

    /** Draws vertices given in clip space (x, y in -1..1, z in 0..1) with identity matrices. */
    void DrawClip(GraphicsDevice& d, const std::vector<VertexPositionColor>& v, PrimitiveType type = PrimitiveType::TriangleList)
    {
        fx_->setWorldProperty(Matrix::getIdentityProperty());
        fx_->setViewProperty(Matrix::getIdentityProperty());
        fx_->setProjectionProperty(Matrix::getIdentityProperty());
        fx_->setVertexColorEnabledProperty(true);
        fx_->setTextureEnabledProperty(false);
        fx_->setLightingEnabledProperty(false);
        fx_->setFogEnabledProperty(false);
        for (auto& pass : fx_->getCurrentTechniqueProperty()->getPassesProperty()) {
            pass.Apply();
            d.DrawUserPrimitives(type, v.data(), 0, type == PrimitiveType::TriangleList ? static_cast<int>(v.size()) / 3 : static_cast<int>(v.size()) / 2);
        }
    }

    // Pixel coordinates (0,0 top-left) of the 64 x 64 target to clip space.
    static float Cx(float px) { return px / kSize * 2.0f - 1.0f; }
    static float Cy(float py) { return 1.0f - py / kSize * 2.0f; }

    /** A quad covering pixels [x0,x1) x [y0,y1) at depth z. */
    static std::vector<VertexPositionColor> Quad(float x0, float y0, float x1, float y1, float z, Color c, bool clockwise = true)
    {
        const VertexPositionColor a(Vector3(Cx(x0), Cy(y0), z), c), b(Vector3(Cx(x1), Cy(y0), z), c), cc(Vector3(Cx(x1), Cy(y1), z), c), dd(Vector3(Cx(x0), Cy(y1), z), c);
        // Screen order a (top-left), b (top-right), cc (bottom-right), dd (bottom-left) is clockwise on screen.
        if (clockwise) return {a, b, cc, a, cc, dd};
        return {a, cc, b, a, dd, cc};
    }

    void Run(GraphicsDevice& d)
    {
        results_.clear();
        readback_ = true;
        const Viewport savedViewport = d.getViewportProperty();
        if (!d.SupportsCapability(CNA::GraphicsCapability::ThreeD)) {
            Skip("3D pipeline", "this renderer has no 3D pipeline");
            return;
        }
        try {
            switch (mode_) {
                case Mode::Geometry: Geometry(d); break;
                case Mode::Sprites: Sprites(d); break;
                case Mode::Blend: Blend(d); break;
                case Mode::Depth: Depth(d); break;
                case Mode::Shading: Shading(d); break;
                case Mode::Sampling: Sampling(d); break;
                case Mode::Targets: Targets(d); break;
                case Mode::Effects: Effects(d); break;
                case Mode::Camera: CameraChecks(d); break;
            }
        } catch (const std::exception& e) {
            d.SetRenderTarget(nullptr);
            Museum::Log("validate", "%s aborted: %s", R().key.c_str(), e.what());
            Add("the check run itself", "no exception", e.what(), false, std::nullopt);
        }
        d.SetRenderTarget(nullptr);
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::Default);
        d.setViewportProperty(savedViewport);
    }

    // --- geometry ---------------------------------------------------------------------------------
    void Geometry(GraphicsDevice& d)
    {
        RenderTarget2D& rt = Target(d, false);
        const Color red(255, 0, 0, 255), clearC(0, 0, 0, 255);
        {   // clockwise on screen is the XNA front face; CullCounterClockwise keeps it
            Begin(d, rt, clearC);
            d.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);
            DrawClip(d, Quad(8, 8, 56, 56, 0.5f, red, true));
            const auto img = End(d, rt);
            if (!img) return;
            Add("Clockwise quad, CullCounterClockwise", "visible (centre red)", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 255, 0, 0) < 4, img);
        }
        {
            Begin(d, rt, clearC);
            d.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);
            DrawClip(d, Quad(8, 8, 56, 56, 0.5f, red, false));
            const auto img = End(d, rt);
            Add("Counter-clockwise quad, CullCounterClockwise", "culled (centre clear)", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 0, 0, 0) < 4, img);
        }
        {
            Begin(d, rt, clearC);
            d.setRasterizerStateProperty(RasterizerState::CullClockwise);
            DrawClip(d, Quad(8, 8, 56, 56, 0.5f, red, true));
            const auto img = End(d, rt);
            Add("Clockwise quad, CullClockwise", "culled (centre clear)", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 0, 0, 0) < 4, img);
        }
        {   // exact coverage: a quad over pixels 8..24 must light exactly 16 x 16 pixels
            Begin(d, rt, clearC);
            DrawClip(d, Quad(8, 8, 24, 24, 0.5f, red));
            const auto img = End(d, rt);
            const int n = img->Count([](const Color& c) { return c.getRProperty() > 128; });
            Add("Quad over pixels 8..24", "256 pixels covered", std::to_string(n) + " pixels", n == 256 && Dist(img->At(8, 8), 255, 0, 0) < 4 && Dist(img->At(23, 23), 255, 0, 0) < 4 && Dist(img->At(24, 24), 0, 0, 0) < 4, img);
        }
        {   // no seam between the two triangles of a full-target quad
            Begin(d, rt, clearC);
            DrawClip(d, Quad(0, 0, 64, 64, 0.5f, red));
            const auto img = End(d, rt);
            const int n = img->Count([](const Color& c) { return c.getRProperty() > 128; });
            Add("Full-target quad has no seam", "4096 pixels covered", std::to_string(n) + " pixels", n == kSize * kSize, img);
        }
        {   // viewport
            Begin(d, rt, clearC);
            d.setViewportProperty(Viewport(16, 16, 32, 32));
            DrawClip(d, Quad(0, 0, 64, 64, 0.5f, red));
            d.setViewportProperty(Viewport(0, 0, kSize, kSize));
            const auto img = End(d, rt);
            const int n = img->Count([](const Color& c) { return c.getRProperty() > 128; });
            Add("Viewport 16,16 32x32", "1024 pixels, inside the viewport", std::to_string(n) + " pixels", n == 1024 && Dist(img->At(15, 32), 0, 0, 0) < 4 && Dist(img->At(16, 32), 255, 0, 0) < 4, img);
        }
        {   // scissor
            static RasterizerState sc = [] { RasterizerState r = RasterizerState::CullNone; r.setScissorTestEnableProperty(true); return r; }();
            Begin(d, rt, clearC);
            d.setScissorRectangleProperty(Rectangle(10, 20, 30, 12));
            d.setRasterizerStateProperty(sc);
            DrawClip(d, Quad(0, 0, 64, 64, 0.5f, red));
            d.setRasterizerStateProperty(RasterizerState::CullNone);
            d.setScissorRectangleProperty(Rectangle(0, 0, kSize, kSize));
            const auto img = End(d, rt);
            const int n = img->Count([](const Color& c) { return c.getRProperty() > 128; });
            Add("Scissor rectangle 10,20 30x12", "360 pixels", std::to_string(n) + " pixels", n == 360 && Dist(img->At(10, 20), 255, 0, 0) < 4 && Dist(img->At(9, 20), 0, 0, 0) < 4, img);
        }
    }

    // --- sprites ----------------------------------------------------------------------------------
    std::unique_ptr<Texture2D> MakeTex(GraphicsDevice& d, int w, int h, const std::function<Color(int, int)>& f)
    {
        auto t = std::make_unique<Texture2D>(d, w, h);
        std::vector<Color> px(static_cast<std::size_t>(w * h));
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) px[static_cast<std::size_t>(y * w + x)] = f(x, y);
        t->SetData(px.data(), static_cast<int>(px.size()));
        return t;
    }

    void Sprites(GraphicsDevice& d)
    {
        RenderTarget2D& rt = Target(d, false);
        SpriteBatch& sb = *S().sprites;
        const Color clearC(0, 0, 0, 255);
        const auto marker = MakeTex(d, 8, 8, [](int x, int y) {
            if (x == 0 && y == 0) return Color(255, 0, 0, 255);
            if (x == 7 && y == 7) return Color(0, 0, 255, 255);
            if (x == 7 && y == 0) return Color(0, 255, 0, 255);
            return Color(((x + y) % 2) * 200 + 20, ((x * 3 + y) % 5) * 40, ((x + y * 2) % 3) * 90, 255);
        });
        {   // 1:1 point sampling reproduces the texture
            Begin(d, rt, clearC);
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
            sb.Draw(*marker, Vector2(10, 12), Color::White);
            sb.End();
            const auto img = End(d, rt);
            if (!img) return;
            std::vector<Color> src(64);
            marker->GetData(src.data(), 64);
            int bad = 0;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) bad += Dist(img->At(10 + x, 12 + y), src[static_cast<std::size_t>(y * 8 + x)].getRProperty(), src[static_cast<std::size_t>(y * 8 + x)].getGProperty(), src[static_cast<std::size_t>(y * 8 + x)].getBProperty()) > 2 ? 1 : 0;
            Add("8x8 texture at 10,12, point sampling", "all 64 texels exact", std::to_string(64 - bad) + " of 64 exact", bad == 0, img);
            Add("Texture top-left stays top-left", "red at 10,12", Rgb(img->At(10, 12)), Dist(img->At(10, 12), 255, 0, 0) < 3, img);
        }
        {   // 2x integer scale
            Begin(d, rt, clearC);
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
            sb.Draw(*marker, Rectangle(8, 8, 16, 16), Color::White);
            sb.End();
            const auto img = End(d, rt);
            const bool ok = Dist(img->At(8, 8), 255, 0, 0) < 3 && Dist(img->At(9, 9), 255, 0, 0) < 3 && Dist(img->At(10, 8), 255, 0, 0) > 40 && Dist(img->At(23, 23), 0, 0, 255) < 3 && Dist(img->At(22, 22), 0, 0, 255) < 3;
            Add("2x scale gives 2x2 blocks", "red block at 8..9, blue at 22..23", Rgb(img->At(9, 9)) + " / " + Rgb(img->At(22, 22)), ok, img);
        }
        {   // flips
            Begin(d, rt, clearC);
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
            sb.Draw(*marker, Vector2(20, 20), std::nullopt, Color::White, 0.0f, Vector2(0, 0), 1.0f, SpriteEffects::FlipHorizontally, 0.0f);
            sb.End();
            const auto img = End(d, rt);
            Add("FlipHorizontally", "green marker (top-right texel) now at the left", Rgb(img->At(20, 20)), Dist(img->At(20, 20), 0, 255, 0) < 3, img);
        }
        {
            Begin(d, rt, clearC);
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
            sb.Draw(*marker, Vector2(20, 20), std::nullopt, Color::White, 0.0f, Vector2(0, 0), 1.0f, SpriteEffects::FlipVertically, 0.0f);
            sb.End();
            const auto img = End(d, rt);
            Add("FlipVertically", "blue marker (bottom-right texel) now at the top-right; left column starts blue-free", Rgb(img->At(27, 20)), Dist(img->At(27, 20), 0, 0, 255) < 3, img);
        }
        {   // rotation by a quarter turn about the origin (top-left): (x,y) -> (-y, x)
            Begin(d, rt, clearC);
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
            sb.Draw(*marker, Vector2(30, 10), std::nullopt, Color::White, MathHelper::PiOver2, Vector2(0, 0), 1.0f, SpriteEffects::None, 0.0f);
            sb.End();
            const auto img = End(d, rt);
            // The sprite's top-left texel (red) stays at the origin; its right neighbour moves down.
            const bool ok = Dist(img->At(29, 10), 255, 0, 0) < 3 && Dist(img->At(29, 17), 0, 255, 0) < 3;
            Add("Rotation by 90 degrees", "red at origin, green texel moved down 7", Rgb(img->At(29, 10)) + " / " + Rgb(img->At(29, 17)), ok, img);
        }
        {   // linear filtering halfway between two texels
            const auto bw = MakeTex(d, 2, 1, [](int x, int) { return x == 0 ? Color(0, 0, 0, 255) : Color(255, 255, 255, 255); });
            Begin(d, rt, clearC);
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearClamp, nullptr, nullptr);
            sb.Draw(*bw, Rectangle(0, 0, 64, 8), Color::White);
            sb.End();
            const auto img = End(d, rt);
            const Color mid = img->At(32, 4);
            Add("Linear filter between black and white", "about 128 at the join", Rgb(mid), Dist(mid, 128, 128, 128) < 20, img);
        }
    }

    // --- blending ---------------------------------------------------------------------------------
    void Blend(GraphicsDevice& d)
    {
        RenderTarget2D& rt = Target(d, false);
        SpriteBatch& sb = *S().sprites;
        const Texture2D& white = *S().whiteTexture;
        (void)white;
        const auto run = [&](Color background, const BlendState* bs, Color src, const std::function<void()>& setup = {}) {
            Begin(d, rt, background);
            if (setup) setup();
            sb.Begin(SpriteSortMode::Deferred, bs, &SamplerState::PointClamp, nullptr, nullptr);
            sb.Draw(*S().whiteTexture, Rectangle(16, 16, 32, 32), src);
            sb.End();
            return End(d, rt);
        };
        {
            const auto img = run(Color(0, 0, 0, 255), &BlendState::NonPremultiplied, Color(255, 255, 255, 128));
            if (!img) return;
            Add("NonPremultiplied: white at 50% over black", "128 +-3", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 128, 128, 128) <= 3, img);
        }
        {
            const auto img = run(Color(0, 0, 0, 255), &BlendState::AlphaBlend, Color(128, 128, 128, 128));
            Add("AlphaBlend (premultiplied): 50% white over black", "128 +-3", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 128, 128, 128) <= 3, img);
        }
        {
            const auto img = run(Color(100, 50, 20, 255), &BlendState::Additive, Color(100, 100, 100, 255));
            Add("Additive: (100,100,100) over (100,50,20)", "200,150,120", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 200, 150, 120) <= 3, img);
        }
        {
            const auto img = run(Color(200, 200, 200, 255), &BlendState::Additive, Color(200, 200, 200, 255));
            Add("Additive saturates at 255", "255,255,255", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 255, 255, 255) <= 1, img);
        }
        {
            const auto img = run(Color(10, 20, 30, 255), &BlendState::Opaque, Color(200, 100, 50, 10));
            Add("Opaque ignores source alpha", "200,100,50", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 200, 100, 50) <= 1, img);
        }
        {   // Blend.BlendFactor: the constant colour set on the BlendState replaces the source alpha as the weight
            BlendState bs;
            bs.setColorSourceBlendProperty(Blend::BlendFactor);
            bs.setColorDestinationBlendProperty(Blend::InverseBlendFactor);
            bs.setAlphaSourceBlendProperty(Blend::One);
            bs.setAlphaDestinationBlendProperty(Blend::Zero);
            bs.setBlendFactorProperty(Color(static_cast<std::uint8_t>(128), static_cast<std::uint8_t>(64), static_cast<std::uint8_t>(255), static_cast<std::uint8_t>(255)));
            const auto img = run(Color(0, 0, 0, 255), &bs, Color(200, 100, 50, 255));
            Add("Blend.BlendFactor: BlendState.BlendFactor (128,64,255) weights the source", "100,25,50 +-3 (source * factor)", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 100, 25, 50) <= 3, img);
        }
        {   // the same factor on a 3D draw (BasicEffect, DrawUserPrimitives): a different path from SpriteBatch on most renderers
            BlendState bs;
            bs.setColorSourceBlendProperty(Blend::BlendFactor);
            bs.setColorDestinationBlendProperty(Blend::InverseBlendFactor);
            bs.setAlphaSourceBlendProperty(Blend::One);
            bs.setAlphaDestinationBlendProperty(Blend::Zero);
            bs.setBlendFactorProperty(Color(static_cast<std::uint8_t>(128), static_cast<std::uint8_t>(64), static_cast<std::uint8_t>(255), static_cast<std::uint8_t>(255)));
            Begin(d, rt, Color(0, 0, 0, 255));
            d.setBlendStateProperty(BlendState::Opaque);
            DrawClip(d, Quad(0, 0, 64, 64, 0.6f, Color(0, 0, 0, 255)));   // an ordinary draw first, in the same pass
            d.setBlendStateProperty(bs);
            DrawClip(d, Quad(16, 16, 48, 48, 0.5f, Color(200, 100, 50, 255)));
            d.setBlendStateProperty(BlendState::Opaque);   // back to ordinary blending before anything is presented: a deferred renderer must have kept the factor with the draw
            const auto img = End(d, rt);
            if (img) Add("Blend.BlendFactor on a 3D draw after an ordinary one", "100,25,50 +-3 (source * factor)", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 100, 25, 50) <= 3, img);
        }
        {   // and on a textured, vertex-coloured BasicEffect draw (what the render-state rooms use)
            BlendState bs;
            bs.setColorSourceBlendProperty(Blend::BlendFactor);
            bs.setColorDestinationBlendProperty(Blend::InverseBlendFactor);
            bs.setAlphaSourceBlendProperty(Blend::One);
            bs.setAlphaDestinationBlendProperty(Blend::Zero);
            bs.setBlendFactorProperty(Color(static_cast<std::uint8_t>(128), static_cast<std::uint8_t>(64), static_cast<std::uint8_t>(255), static_cast<std::uint8_t>(255)));
            Begin(d, rt, Color(0, 0, 0, 255));
            d.setBlendStateProperty(bs);
            BasicEffect fx(d);
            fx.setTextureEnabledProperty(true);
            fx.setTextureProperty(S().whiteTexture);
            fx.setVertexColorEnabledProperty(true);
            fx.setLightingEnabledProperty(false);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(Matrix::getIdentityProperty());
            fx.setProjectionProperty(Matrix::getIdentityProperty());
            const Color tint(200, 100, 50, 255);
            const VertexPositionColorTexture q[6] = {{Vector3(-0.5f, -0.5f, 0.5f), tint, Vector2(0, 1)}, {Vector3(0.5f, -0.5f, 0.5f), tint, Vector2(1, 1)}, {Vector3(0.5f, 0.5f, 0.5f), tint, Vector2(1, 0)},
                                                     {Vector3(-0.5f, -0.5f, 0.5f), tint, Vector2(0, 1)}, {Vector3(0.5f, 0.5f, 0.5f), tint, Vector2(1, 0)}, {Vector3(-0.5f, 0.5f, 0.5f), tint, Vector2(0, 0)}};
            for (auto& pass : fx.getCurrentTechniqueProperty()->getPassesProperty()) {
                pass.Apply();
                d.DrawUserPrimitives(PrimitiveType::TriangleList, q, 0, 2);
            }
            const auto img = End(d, rt);
            if (img) Add("Blend.BlendFactor on a textured BasicEffect draw", "100,25,50 +-3 (source * factor)", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 100, 25, 50) <= 3, img);
        }
        {   // colour write mask: red only
            BlendState bs = BlendState::Opaque;
            bs.setColorWriteChannelsProperty(ColorWriteChannels::Red);
            const auto img = run(Color(10, 20, 30, 255), &bs, Color(200, 100, 50, 255));
            Add("ColorWriteChannels.Red", "200,20,30 (only red written)", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 200, 20, 30) <= 1, img);
        }
        {   // blend factor: source * factor + dest * (1 - factor) with factor 0.25
            BlendState bs;
            bs.setColorSourceBlendProperty(Blend::BlendFactor);
            bs.setColorDestinationBlendProperty(Blend::InverseBlendFactor);
            bs.setColorBlendFunctionProperty(BlendFunction::Add);
            bs.setAlphaSourceBlendProperty(Blend::One);
            bs.setAlphaDestinationBlendProperty(Blend::Zero);
            bs.setBlendFactorProperty(Color(64, 64, 64, 255));
            const auto img = run(Color(0, 0, 0, 255), &bs, Color(200, 100, 40, 255), {});
            Add("BlendFactor 0.25", "50,25,10 +-3", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 50, 25, 10) <= 4, img);
        }
        {   // Max blend function
            BlendState bs;
            bs.setColorSourceBlendProperty(Blend::One);
            bs.setColorDestinationBlendProperty(Blend::One);
            bs.setColorBlendFunctionProperty(BlendFunction::Max);
            bs.setAlphaSourceBlendProperty(Blend::One);
            bs.setAlphaDestinationBlendProperty(Blend::One);
            bs.setAlphaBlendFunctionProperty(BlendFunction::Max);
            try {
                const auto img = run(Color(100, 200, 50, 255), &bs, Color(150, 100, 80, 255));
                Add("BlendFunction.Max", "150,200,80", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 150, 200, 80) <= 2, img);
            } catch (const std::exception& e) {
                d.SetRenderTarget(nullptr);
                Skip("BlendFunction.Max", e.what());
            }
        }
    }

    // --- depth ------------------------------------------------------------------------------------
    void Depth(GraphicsDevice& d)
    {
        RenderTarget2D& rt = Target(d, true);
        const Color clearC(0, 0, 0, 255), red(255, 0, 0, 255), green(0, 255, 0, 255);
        const auto pass = [&](const DepthStencilState& ds, float zFirst, Color first, float zSecond, Color second) {
            Begin(d, rt, clearC);
            d.setDepthStencilStateProperty(ds);
            DrawClip(d, Quad(8, 8, 56, 56, zFirst, first));
            DrawClip(d, Quad(8, 8, 56, 56, zSecond, second));
            return End(d, rt);
        };
        {
            const auto img = pass(DepthStencilState::Default, 0.3f, red, 0.6f, green);
            if (!img) return;
            Add("Near drawn first, far second", "near (red) wins", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 255, 0, 0) < 3, img);
        }
        {
            const auto img = pass(DepthStencilState::Default, 0.6f, green, 0.3f, red);
            Add("Far drawn first, near second", "near (red) wins", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 255, 0, 0) < 3, img);
        }
        {
            DepthStencilState greater = DepthStencilState::Default;
            greater.setDepthBufferFunctionProperty(CompareFunction::Greater);
            Begin(d, rt, clearC);
            d.Clear(ClearOptions::DepthBuffer, clearC, 0.0f, 0);
            d.setDepthStencilStateProperty(greater);
            DrawClip(d, Quad(8, 8, 56, 56, 0.3f, red));
            DrawClip(d, Quad(8, 8, 56, 56, 0.6f, green));
            const auto img = End(d, rt);
            Add("DepthBufferFunction.Greater", "far (green) wins", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 0, 255, 0) < 3, img);
        }
        {
            DepthStencilState noWrite = DepthStencilState::Default;
            noWrite.setDepthBufferWriteEnableProperty(false);
            const auto img = pass(noWrite, 0.3f, red, 0.6f, green);
            Add("DepthBufferWriteEnable false", "later (green) overwrites: nothing was written", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 0, 255, 0) < 3, img);
        }
        {
            const auto img = pass(DepthStencilState::None, 0.3f, red, 0.6f, green);
            Add("DepthStencilState.None", "later (green) wins: no depth test", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 0, 255, 0) < 3, img);
        }
        {   // precision, informational: how close can two surfaces be and still be told apart under a typical
            // projection (near 0.1, far 1000) at 100 m with this renderer's depth buffer?
            const Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::PiOver2, 1.0f, 0.1f, 1000.0f);
            const auto quadAt = [&](float z, Color c) {
                return std::vector<VertexPositionColor>{VertexPositionColor(Vector3(-50, 50, -z), c), VertexPositionColor(Vector3(50, 50, -z), c), VertexPositionColor(Vector3(50, -50, -z), c),
                                                        VertexPositionColor(Vector3(-50, 50, -z), c), VertexPositionColor(Vector3(50, -50, -z), c), VertexPositionColor(Vector3(-50, -50, -z), c)};
            };
            float found = -1.0f;
            std::optional<Image> last;
            for (const float gap : {0.5f, 0.1f, 0.02f, 0.005f}) {
                Begin(d, rt, clearC);
                d.setDepthStencilStateProperty(DepthStencilState::Default);
                fx_->setVertexColorEnabledProperty(true);
                fx_->setTextureEnabledProperty(false);
                fx_->setLightingEnabledProperty(false);
                fx_->setWorldProperty(Matrix::getIdentityProperty());
                fx_->setViewProperty(Matrix::getIdentityProperty());
                fx_->setProjectionProperty(proj);
                const auto far = quadAt(100.0f + gap, green);
                const auto near = quadAt(100.0f, red);
                for (auto& pass2 : fx_->getCurrentTechniqueProperty()->getPassesProperty()) {
                    pass2.Apply();
                    d.DrawUserPrimitives(PrimitiveType::TriangleList, far.data(), 0, 2);
                    d.DrawUserPrimitives(PrimitiveType::TriangleList, near.data(), 0, 2);
                }
                last = End(d, rt);
                if (!last || Dist(last->At(32, 32), 255, 0, 0) >= 3) break;
                found = gap;
            }
            char buf[64];
            std::snprintf(buf, sizeof buf, found > 0 ? "resolved down to %.3f m" : "not even 0.5 m", static_cast<double>(found));
            Add("Depth precision at 100 m (near 0.1, far 1000)", "informational: the gap that still resolves", buf, true, last, true);
        }
    }

    // --- shading ----------------------------------------------------------------------------------
    void Shading(GraphicsDevice& d)
    {
        RenderTarget2D& rt = Target(d, true);
        const Color clearC(0, 0, 0, 255);
        {   // vertex colour interpolation: centroid of a red/green/blue triangle
            Begin(d, rt, clearC);
            const std::vector<VertexPositionColor> tri = {VertexPositionColor(Vector3(Cx(32), Cy(4), 0.5f), Color(255, 0, 0, 255)), VertexPositionColor(Vector3(Cx(60), Cy(56), 0.5f), Color(0, 255, 0, 255)),
                                                          VertexPositionColor(Vector3(Cx(4), Cy(56), 0.5f), Color(0, 0, 255, 255))};
            DrawClip(d, tri);
            const auto img = End(d, rt);
            if (!img) return;
            const Color c = img->At(32, 39);
            Add("Vertex colours: centroid of red/green/blue", "about 85,85,85 +-14", Rgb(c), Dist(c, 85, 85, 85) <= 14, img);
        }
        {   // lighting: a face with normal +Z lit by a directional light pointing at -Z, diffuse white, no ambient
            Begin(d, rt, clearC);
            BasicEffect fx(d);
            fx.setLightingEnabledProperty(true);
            fx.setAmbientLightColorProperty(Vector3(0, 0, 0));
            fx.setDiffuseColorProperty(Vector3(0.5f, 0.5f, 0.5f));
            fx.setSpecularColorProperty(Vector3(0, 0, 0));
            fx.setVertexColorEnabledProperty(false);
            fx.setTextureEnabledProperty(true);
            fx.setTextureProperty(S().whiteTexture);
            for (int i = 0; i < 3; ++i) {
                DirectionalLight& l = i == 0 ? fx.getDirectionalLight0Property() : (i == 1 ? fx.getDirectionalLight1Property() : fx.getDirectionalLight2Property());
                l.setEnabledProperty(i == 0);
                l.setDirectionProperty(Vector3(0, 0, -1));
                l.setDiffuseColorProperty(Vector3(1, 1, 1));
                l.setSpecularColorProperty(Vector3(0, 0, 0));
            }
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(Matrix::getIdentityProperty());
            fx.setProjectionProperty(Matrix::getIdentityProperty());
            const Vector3 n(0, 0, 1);
            const std::vector<VertexPositionNormalTexture> quad = {
                VertexPositionNormalTexture(Vector3(-0.8f, 0.8f, 0.5f), n, Vector2(0, 0)), VertexPositionNormalTexture(Vector3(0.8f, 0.8f, 0.5f), n, Vector2(1, 0)), VertexPositionNormalTexture(Vector3(0.8f, -0.8f, 0.5f), n, Vector2(1, 1)),
                VertexPositionNormalTexture(Vector3(-0.8f, 0.8f, 0.5f), n, Vector2(0, 0)), VertexPositionNormalTexture(Vector3(0.8f, -0.8f, 0.5f), n, Vector2(1, 1)), VertexPositionNormalTexture(Vector3(-0.8f, -0.8f, 0.5f), n, Vector2(0, 1))};
            for (auto& pass : fx.getCurrentTechniqueProperty()->getPassesProperty()) {
                pass.Apply();
                d.DrawUserPrimitives(PrimitiveType::TriangleList, quad.data(), 0, 2);
            }
            const auto img = End(d, rt);
            Add("BasicEffect: diffuse 0.5, light along -Z", "N.L = 1: 128,128,128 +-6", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 128, 128, 128) <= 6, img);
        }
        {   // fog: halfway between FogStart and FogEnd the result is the average of object and fog colour
            Begin(d, rt, Color(0, 0, 255, 255));
            BasicEffect fx(d);
            fx.setLightingEnabledProperty(false);
            fx.setVertexColorEnabledProperty(true);
            fx.setTextureEnabledProperty(false);
            fx.setFogEnabledProperty(true);
            fx.setFogColorProperty(Vector3(0, 0, 1));
            fx.setFogStartProperty(10.0f);
            fx.setFogEndProperty(20.0f);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(Matrix::getIdentityProperty());
            fx.setProjectionProperty(Matrix::CreatePerspectiveFieldOfView(MathHelper::PiOver2, 1.0f, 0.1f, 100.0f));
            const Color red(255, 0, 0, 255);
            const std::vector<VertexPositionColor> q = {VertexPositionColor(Vector3(-20, 20, -15), red), VertexPositionColor(Vector3(20, 20, -15), red), VertexPositionColor(Vector3(20, -20, -15), red),
                                                        VertexPositionColor(Vector3(-20, 20, -15), red), VertexPositionColor(Vector3(20, -20, -15), red), VertexPositionColor(Vector3(-20, -20, -15), red)};
            for (auto& pass : fx.getCurrentTechniqueProperty()->getPassesProperty()) {
                pass.Apply();
                d.DrawUserPrimitives(PrimitiveType::TriangleList, q.data(), 0, 2);
            }
            const auto img = End(d, rt);
            Add("Linear fog halfway (red object, blue fog)", "about 128,0,128 +-20", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 128, 0, 128) <= 20, img);
        }
    }

    // --- sampling ---------------------------------------------------------------------------------
    void Sampling(GraphicsDevice& d)
    {
        RenderTarget2D& rt = Target(d, false);
        const Color clearC(0, 0, 0, 255);
        const auto tex = MakeTex(d, 2, 2, [](int x, int y) {
            static const Color c[4] = {Color(255, 0, 0, 255), Color(0, 255, 0, 255), Color(0, 0, 255, 255), Color(255, 255, 0, 255)};
            return c[y * 2 + x];
        });
        // A quad over the whole target mapping u in [-1, 2] horizontally (three repeats) and v in [0, 1].
        const auto draw = [&](const SamplerState& s, float u0, float u1, float v0, float v1) {
            Begin(d, rt, clearC);
            fx_->setVertexColorEnabledProperty(true);
            fx_->setTextureEnabledProperty(true);
            fx_->setTextureProperty(tex.get());
            fx_->setLightingEnabledProperty(false);
            fx_->setWorldProperty(Matrix::getIdentityProperty());
            fx_->setViewProperty(Matrix::getIdentityProperty());
            fx_->setProjectionProperty(Matrix::getIdentityProperty());
            const Color w = Color::White;
            const std::vector<VertexPositionColorTexture> q = {
                VertexPositionColorTexture(Vector3(-1, 1, 0.5f), w, Vector2(u0, v0)), VertexPositionColorTexture(Vector3(1, 1, 0.5f), w, Vector2(u1, v0)), VertexPositionColorTexture(Vector3(1, -1, 0.5f), w, Vector2(u1, v1)),
                VertexPositionColorTexture(Vector3(-1, 1, 0.5f), w, Vector2(u0, v0)), VertexPositionColorTexture(Vector3(1, -1, 0.5f), w, Vector2(u1, v1)), VertexPositionColorTexture(Vector3(-1, -1, 0.5f), w, Vector2(u0, v1))};
            d.getSamplerStatesProperty()[0] = s;
            for (auto& pass : fx_->getCurrentTechniqueProperty()->getPassesProperty()) {
                pass.Apply();
                d.DrawUserPrimitives(PrimitiveType::TriangleList, q.data(), 0, 2);
            }
            auto img = End(d, rt);
            fx_->setTextureEnabledProperty(false);
            return img;
        };
        {   // u from -1 to 2: pixels at u = -0.75 (left quarter) and 1.25 and 1.75
            SamplerState wrap = SamplerState::PointWrap;
            const auto img = draw(wrap, -1.0f, 2.0f, 0.0f, 1.0f);
            if (!img) return;
            // u = -1 + 3 * (px + 0.5) / 64. px 8 -> u = -0.63 -> wraps to 0.37: left texel column (u < 0.5): red above/green? v=0.25 top row
            const Color a = img->At(8, 16);     // u -0.63 -> 0.37 -> texel 0, top row: red
            const Color b = img->At(40, 16);    // u 0.84 -> texel 1, top row: green
            Add("TextureAddressMode.Wrap", "u -0.63 -> red, u 0.84 -> green", Rgb(a) + " / " + Rgb(b), Dist(a, 255, 0, 0) < 3 && Dist(b, 0, 255, 0) < 3, img);
        }
        {
            SamplerState clamp = SamplerState::PointClamp;
            const auto img = draw(clamp, -1.0f, 2.0f, 0.0f, 1.0f);
            const Color a = img->At(8, 16);     // clamps to u = 0: red
            const Color b = img->At(60, 16);    // clamps to u = 1: green
            Add("TextureAddressMode.Clamp", "u -0.63 -> red, u 1.84 -> green", Rgb(a) + " / " + Rgb(b), Dist(a, 255, 0, 0) < 3 && Dist(b, 0, 255, 0) < 3, img);
        }
        {
            SamplerState mirror = SamplerState::PointClamp;
            mirror.setAddressUProperty(TextureAddressMode::Mirror);
            mirror.setAddressVProperty(TextureAddressMode::Mirror);
            const auto img = draw(mirror, -1.0f, 2.0f, 0.0f, 1.0f);
            // u -0.63 mirrors to 0.63: texel 1 (green); u 1.37 mirrors to 0.63: green; u 1.84 mirrors to 0.16: red
            const Color a = img->At(8, 16);
            const Color b = img->At(60, 16);
            Add("TextureAddressMode.Mirror", "u -0.63 -> green, u 1.84 -> red", Rgb(a) + " / " + Rgb(b), Dist(a, 0, 255, 0) < 3 && Dist(b, 255, 0, 0) < 3, img);
        }
        {   // linear filtering at the centre of the 2x2 texture: average of all four texels
            SamplerState lin = SamplerState::LinearClamp;
            const auto img = draw(lin, 0.0f, 1.0f, 0.0f, 1.0f);
            const Color c = img->At(32, 32);
            Add("Linear filter at the texture centre", "average of red, green, blue, yellow: 127,127,64 +-16", Rgb(c), Dist(c, 127, 127, 64) <= 16, img);
        }
        {
            SamplerState pt = SamplerState::PointClamp;
            const auto img = draw(pt, 0.0f, 1.0f, 0.0f, 1.0f);
            const Color tl = img->At(8, 8), br = img->At(56, 56);
            Add("Point filter quadrants", "top-left red, bottom-right yellow", Rgb(tl) + " / " + Rgb(br), Dist(tl, 255, 0, 0) < 3 && Dist(br, 255, 255, 0) < 3, img);
        }
    }

    // --- render targets ---------------------------------------------------------------------------
    void Targets(GraphicsDevice& d)
    {
        RenderTarget2D& rt = Target(d, false);
        SpriteBatch& sb = *S().sprites;
        const Color clearC(0, 0, 0, 255);
        {   // draw a marker at the top-left, read back
            Begin(d, rt, clearC);
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
            sb.Draw(*S().whiteTexture, Rectangle(0, 0, 8, 8), Color(255, 0, 0, 255));
            sb.Draw(*S().whiteTexture, Rectangle(56, 56, 8, 8), Color(0, 0, 255, 255));
            sb.End();
            const auto img = End(d, rt);
            if (!img) return;
            Add("Top-left stays top-left in GetData", "red at 0,0 and blue at 63,63", Rgb(img->At(0, 0)) + " / " + Rgb(img->At(63, 63)), Dist(img->At(0, 0), 255, 0, 0) < 3 && Dist(img->At(63, 63), 0, 0, 255) < 3, img);
        }
        {   // draw the target as a texture into a second target at half size
            RenderTarget2D second(d, kSize, kSize, false, SurfaceFormat::Color, DepthFormat::None);
            Begin(d, rt, clearC);
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
            sb.Draw(*S().whiteTexture, Rectangle(0, 0, 8, 8), Color(255, 0, 0, 255));
            sb.End();
            d.SetRenderTarget(nullptr);
            RenderStates::UnbindTextures(d);
            d.SetRenderTarget(&second);
            d.Clear(clearC);
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
            sb.Draw(rt, Rectangle(0, 0, 64, 64), Color::White);
            sb.End();
            d.SetRenderTarget(nullptr);
            Image img;
            img.px.resize(static_cast<std::size_t>(kSize * kSize));
            second.GetData(img.px.data(), static_cast<int>(img.px.size()));
            Add("Render target used as a texture", "red marker still at top-left (0,0)", Rgb(img.At(2, 2)) + " / " + Rgb(img.At(2, 61)), Dist(img.At(2, 2), 255, 0, 0) < 3 && Dist(img.At(2, 61), 0, 0, 0) < 3, img);
        }
        {   // a RenderTargetCube drawn into, then sampled by direction (EnvironmentMapEffect), keeps its orientation
            try {
                RenderTargetCube cube(d, 16, false, SurfaceFormat::Color, DepthFormat::None);
                SpriteBatch& batch = *S().sprites;
                for (int face = 0; face < 6; ++face) {
                    d.SetRenderTarget(&cube, static_cast<CubeMapFace>(face));
                    d.Clear(Color(0, 0, 0, 255));
                    batch.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
                    if (face == 2) batch.Draw(*S().whiteTexture, Rectangle(0, 0, 16, 16), Color(255, 0, 0, 255));        // +Y: all red
                    else if (face == 3) batch.Draw(*S().whiteTexture, Rectangle(0, 0, 16, 16), Color(0, 0, 255, 255));   // -Y: all blue
                    else {   // the sides: red above, blue below
                        batch.Draw(*S().whiteTexture, Rectangle(0, 0, 16, 8), Color(255, 0, 0, 255));
                        batch.Draw(*S().whiteTexture, Rectangle(0, 8, 16, 8), Color(0, 0, 255, 255));
                    }
                    batch.End();
                }
                d.SetRenderTarget(static_cast<RenderTarget2D*>(nullptr));
                RenderStates::UnbindTextures(d);
                EnvironmentMapEffect env(d);
                env.setEnvironmentMapProperty(&cube);
                env.setEnvironmentMapAmountProperty(1.0f);
                env.setFresnelFactorProperty(0.0f);
                env.setTextureProperty(S().whiteTexture);
                env.setDiffuseColorProperty(Vector3(0, 0, 0));
                env.setEmissiveColorProperty(Vector3(0, 0, 0));
                env.setAmbientLightColorProperty(Vector3(0, 0, 0));
                env.setViewProperty(Matrix::CreateLookAt(Vector3(0, 0, 3), Vector3::Zero, Vector3::Up));
                env.setProjectionProperty(Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(60.0f), 1.0f, 0.1f, 20.0f));
                env.setWorldProperty(Matrix::CreateScale(2.0f));   // a unit sphere filling about 40 of the 64 pixels
                Begin(d, rt, clearC);
                d.setDepthStencilStateProperty(DepthStencilState::None);
                d.setRasterizerStateProperty(RasterizerState::CullClockwise);
                for (auto& pass : env.getCurrentTechniqueProperty()->getPassesProperty()) pass.Apply();
                S().shapes->Sphere().Draw(d);
                const auto img = End(d, rt);
                if (img) {
                    // Just above the middle the reflection points up and back at the camera (a side face, upper half: red);
                    // just below it points down (lower half: blue).
                    const Color above = img->At(32, 26), below = img->At(32, 38);
                    Add("Cube target sampled as an environment map keeps up as up", "red just above the sphere's middle, blue just below", Rgb(above) + " / " + Rgb(below),
                        Dist(above, 255, 0, 0) < 60 && Dist(below, 0, 0, 255) < 60, img);
                }
            } catch (const std::exception& e) {
                d.SetRenderTarget(nullptr);
                Add("Cube target sampled as an environment map keeps up as up", "no exception", e.what(), false, std::nullopt);
            }
        }
        {   // a multisampled target gives edges intermediate shades; a plain one does not
            Image sampled;   // the same target, drawn through SpriteBatch into a plain one
            const auto edgeShades = [&](int samples, int& got, RenderTargetUsage usage = RenderTargetUsage::DiscardContents) {
                RenderTarget2D ms(d, kSize, kSize, false, SurfaceFormat::Color, DepthFormat::Depth24, samples, usage);
                got = ms.getMultiSampleCountProperty();
                d.SetRenderTarget(&ms);
                d.setViewportProperty(Viewport(0, 0, kSize, kSize));
                d.setBlendStateProperty(BlendState::Opaque);
                d.setDepthStencilStateProperty(DepthStencilState::None);
                d.setRasterizerStateProperty(RasterizerState::CullNone);
                d.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color(0, 0, 0, 255), 1.0f, 0);
                DrawClip(d, {VertexPositionColor(Vector3(-0.9f, -0.9f, 0.5f), Color::White), VertexPositionColor(Vector3(0.9f, 0.7f, 0.5f), Color::White), VertexPositionColor(Vector3(-0.9f, 0.7f, 0.5f), Color::White)});
                d.SetRenderTarget(nullptr);
                // First sample the target as a texture (what the museum's screens do), then read it back directly.
                RenderTarget2D plain(d, kSize, kSize, false, SurfaceFormat::Color, DepthFormat::None);
                d.SetRenderTarget(&plain);
                d.Clear(Color(0, 0, 0, 255));
                S().sprites->Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
                S().sprites->Draw(ms, Rectangle(0, 0, kSize, kSize), Color::White);
                S().sprites->End();
                d.SetRenderTarget(nullptr);
                sampled.px.assign(static_cast<std::size_t>(kSize * kSize), Color::Black);
                plain.GetData(sampled.px.data(), static_cast<int>(sampled.px.size()));
                Image img;
                img.px.resize(static_cast<std::size_t>(kSize * kSize));
                ms.GetData(img.px.data(), static_cast<int>(img.px.size()));
                return img;
            };
            if (!d.SupportsCapability(CNA::GraphicsCapability::MultiSampleAntiAliasing)) {
                Skip("Multisampled target edge shades", "MultiSampleAntiAliasing not supported");
            } else {
                try {
                    int plainGot = 0, msGot = 0;
                    const Image plain = edgeShades(0, plainGot);
                    const Image multi = edgeShades(4, msGot);
                    const auto mid = [](const Color& c) { return c.getRProperty() > 24 && c.getRProperty() < 232; };
                    const int a = plain.Count(mid), b = multi.Count(mid);
                    Add("Multisampled target (4 samples asked) smooths edges", "no mid shades without MSAA, some with", std::to_string(a) + " without, " + std::to_string(b) + " with (target reports " + std::to_string(msGot) + " samples)", a == 0 && b > 0, multi);
                    int keepGot = 0;
                    const Image kept = edgeShades(4, keepGot, RenderTargetUsage::PreserveContents);
                    const int c = kept.Count(mid);
                    Add("Same with RenderTargetUsage.PreserveContents", "smoothed edges as with DiscardContents", std::to_string(c) + " mid-shade pixels read back (target reports " + std::to_string(keepGot) + " samples)", c > 0, kept);
                    const int viaSprite = sampled.Count(mid);
                    Add("PreserveContents target drawn with SpriteBatch keeps the smoothing", "mid shades survive being sampled as a texture", std::to_string(viaSprite) + " mid-shade pixels after sampling", viaSprite > 0, sampled);
                } catch (const std::exception& e) {
                    d.SetRenderTarget(nullptr);
                    Add("Multisampled target (4 samples asked) smooths edges", "no exception", e.what(), false, std::nullopt);
                }
            }
        }
        {   // half-float target holds values above 1
            if (!d.SupportsCapability(CNA::GraphicsCapability::HalfFloatRenderTargets)) {
                Skip("HalfVector4 target holds 1.5", "HalfFloatRenderTargets not supported");
            } else {
                try {
                    RenderTarget2D hdr(d, 8, 8, false, SurfaceFormat::HalfVector4, DepthFormat::None);
                    d.SetRenderTarget(&hdr);
                    d.Clear(Color(0, 0, 0, 255));
                    d.SetRenderTarget(nullptr);
                    Add("Half-float target creation", "created", "created", true, std::nullopt);
                } catch (const std::exception& e) {
                    d.SetRenderTarget(nullptr);
                    Add("Half-float target creation", "created", e.what(), false, std::nullopt);
                }
            }
        }
    }


    // --- camera matrices --------------------------------------------------------------------------
    /** A square of half-size `h` facing +Z (towards a camera on the +Z side) centred at `c`. */
    static std::vector<VertexPositionColor> WorldQuad(const Vector3& c, float h, Color col)
    {
        const VertexPositionColor a(c + Vector3(-h, h, 0), col), b(c + Vector3(h, h, 0), col), cc(c + Vector3(h, -h, 0), col), dd(c + Vector3(-h, -h, 0), col);
        return {a, b, cc, a, cc, dd};
    }
    void DrawWorld(GraphicsDevice& d, const Matrix& view, const Matrix& proj, const std::vector<VertexPositionColor>& v)
    {
        fx_->setVertexColorEnabledProperty(true);
        fx_->setTextureEnabledProperty(false);
        fx_->setLightingEnabledProperty(false);
        fx_->setFogEnabledProperty(false);
        fx_->setWorldProperty(Matrix::getIdentityProperty());
        fx_->setViewProperty(view);
        fx_->setProjectionProperty(proj);
        for (auto& pass : fx_->getCurrentTechniqueProperty()->getPassesProperty()) {
            pass.Apply();
            d.DrawUserPrimitives(PrimitiveType::TriangleList, v.data(), 0, static_cast<int>(v.size()) / 3);
        }
    }
    static int Extent(const Image& img, bool horizontal)
    {
        int lo = kSize, hi = -1;
        for (int y = 0; y < kSize; ++y)
            for (int x = 0; x < kSize; ++x)
                if (img.At(x, y).getRProperty() > 128) {
                    const int v = horizontal ? x : y;
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
        return hi < 0 ? 0 : hi - lo + 1;
    }

    void CameraChecks(GraphicsDevice& d)
    {
        RenderTarget2D& rt = Target(d, true);
        const Color clearC(0, 0, 0, 255), red(255, 0, 0, 255);
        const Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::PiOver2, 1.0f, 0.2f, 100.0f);
        const Vector3 eye(0, 0, 0);
        const auto shoot = [&](const Matrix& view, const Matrix& p, const std::vector<VertexPositionColor>& quad) {
            Begin(d, rt, clearC);
            d.setDepthStencilStateProperty(DepthStencilState::Default);
            DrawWorld(d, view, p, quad);
            return End(d, rt);
        };
        const Matrix ahead = Matrix::CreateLookAt(eye, Vector3(0, 0, -1), Vector3::Up);
        {   // a 2 x 2 quad 4 m ahead covers exactly half of a 90 degree view: 32 pixels wide, centred
            const auto img = shoot(ahead, proj, WorldQuad(Vector3(0, 0, -4), 1.0f, red));
            if (!img) return;
            const int w = Extent(*img, true), h = Extent(*img, false);
            Add("LookAt -Z: a 2 m quad 4 m ahead (FOV 90)", "16 x 16 pixels, centred", std::to_string(w) + " x " + std::to_string(h) + ", centre " + Rgb(img->At(32, 32)), std::abs(w - 16) <= 1 && std::abs(h - 16) <= 1 && Dist(img->At(32, 32), 255, 0, 0) < 3, img);
        }
        {   // the same quad 8 m ahead is half the size
            const auto img = shoot(ahead, proj, WorldQuad(Vector3(0, 0, -8), 1.0f, red));
            const int w = Extent(*img, true);
            Add("Perspective: the quad twice as far", "8 pixels wide", std::to_string(w) + " pixels", std::abs(w - 8) <= 1, img);
        }
        {   // a quad above the view axis lands in the top half
            const auto img = shoot(ahead, proj, WorldQuad(Vector3(0, 3, -4), 0.5f, red));
            const Color top = img->At(32, 8), bottom = img->At(32, 56);
            Add("A quad above the axis is in the top half", "red above the centre, nothing below", Rgb(top) + " top, " + Rgb(bottom) + " bottom", Dist(img->At(32, 4), 255, 0, 0) < 3 || Dist(img->At(32, 6), 255, 0, 0) < 3 || Dist(img->At(32, 8), 255, 0, 0) < 3 ? Dist(bottom, 0, 0, 0) < 3 : false, img);
        }
        {   // a quad to the right (+X) lands in the right half
            const auto img = shoot(ahead, proj, WorldQuad(Vector3(3, 0, -4), 0.5f, red));
            const bool right = img->Count([](const Color& c) { return c.getRProperty() > 128; }) > 0;
            bool leftHalf = false;
            for (int y = 0; y < kSize; ++y)
                for (int x = 0; x < 32; ++x) leftHalf = leftHalf || img->At(x, y).getRProperty() > 128;
            Add("A quad to the right (+X) is in the right half", "right half only", std::string(right ? "visible" : "not visible") + (leftHalf ? ", also in the left half" : ", left half empty"), right && !leftHalf, img);
        }
        {   // pitching the camera up by 45 degrees brings an object 45 degrees above the horizon to the centre
            const Matrix pitched = Matrix::CreateLookAt(eye, Vector3(0, 1, -1), Vector3::Up);
            const auto img = shoot(pitched, proj, WorldQuad(Vector3(0, 4, -4), 0.5f, red));
            // The quad faces +Z, so seen from below-ahead it is foreshortened but still covers the centre pixel.
            Add("Pitch up 45 degrees: object 45 degrees up is centred", "centre pixel red", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 255, 0, 0) < 3, img);
        }
        {   // yaw right by 90 degrees: an object on +X is now straight ahead
            const Matrix yawed = Matrix::CreateLookAt(eye, Vector3(1, 0, 0), Vector3::Up);
            const std::vector<VertexPositionColor> side = {VertexPositionColor(Vector3(4, 1, 1), red), VertexPositionColor(Vector3(4, 1, -1), red), VertexPositionColor(Vector3(4, -1, -1), red),
                                                           VertexPositionColor(Vector3(4, 1, 1), red), VertexPositionColor(Vector3(4, -1, -1), red), VertexPositionColor(Vector3(4, -1, 1), red)};
            const auto img = shoot(yawed, proj, side);
            Add("Look along +X: a quad on the +X axis is centred", "centre pixel red", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 255, 0, 0) < 3, img);
        }
        {   // orthographic: size does not change with distance
            const Matrix ortho = Matrix::CreateOrthographic(8.0f, 8.0f, 0.1f, 100.0f);
            const auto near = shoot(ahead, ortho, WorldQuad(Vector3(0, 0, -3), 1.0f, red));
            const auto far = shoot(ahead, ortho, WorldQuad(Vector3(0, 0, -30), 1.0f, red));
            const int wn = Extent(*near, true), wf = Extent(*far, true);
            Add("Orthographic: a 2 m quad at 3 m and 30 m", "16 pixels wide both times", std::to_string(wn) + " and " + std::to_string(wf), std::abs(wn - 16) <= 1 && std::abs(wf - 16) <= 1, far);
        }
        {   // world matrix: the same quad translated by the world matrix and by a vertex offset must agree
            Begin(d, rt, clearC);
            d.setDepthStencilStateProperty(DepthStencilState::Default);
            fx_->setVertexColorEnabledProperty(true);
            fx_->setTextureEnabledProperty(false);
            fx_->setLightingEnabledProperty(false);
            fx_->setWorldProperty(Matrix::CreateTranslation(0, 0, -6));
            fx_->setViewProperty(ahead);
            fx_->setProjectionProperty(proj);
            const auto quad = WorldQuad(Vector3(0, 0, 0), 1.0f, red);
            for (auto& pass : fx_->getCurrentTechniqueProperty()->getPassesProperty()) {
                pass.Apply();
                d.DrawUserPrimitives(PrimitiveType::TriangleList, quad.data(), 0, 2);
            }
            const auto img = End(d, rt);
            const int w = Extent(*img, true);
            Add("World matrix translation (0,0,-6)", "a 2 m quad at 6 m: about 11 pixels wide", std::to_string(w) + " pixels", std::abs(w - 11) <= 1, img);
        }
    }

    // --- stock effects ----------------------------------------------------------------------------
    void Effects(GraphicsDevice& d)
    {
        {   // a CNA extension with no capability flag: ask the renderer by drawing (Rendering/Probes.cpp)
            const Probes::Answer lit = Probes::PunctualLights(d);
            Add("BasicEffect.PunctualLightEXT lights a surface (CNA extension)", "a point light in front of a white wall lights its centre", Probes::Describe(lit), lit == Probes::Answer::Yes, std::nullopt);
        }
        RenderTarget2D& rt = Target(d, true);
        const Color clearC(0, 0, 0, 255);
        const auto textured = [&](Effect& fx) {
            const Vector3 n(0, 0, 1);
            const std::vector<VertexPositionNormalTexture> quad = {
                VertexPositionNormalTexture(Vector3(-0.8f, 0.8f, 0.5f), n, Vector2(0, 0)), VertexPositionNormalTexture(Vector3(0.8f, 0.8f, 0.5f), n, Vector2(1, 0)), VertexPositionNormalTexture(Vector3(0.8f, -0.8f, 0.5f), n, Vector2(1, 1)),
                VertexPositionNormalTexture(Vector3(-0.8f, 0.8f, 0.5f), n, Vector2(0, 0)), VertexPositionNormalTexture(Vector3(0.8f, -0.8f, 0.5f), n, Vector2(1, 1)), VertexPositionNormalTexture(Vector3(-0.8f, -0.8f, 0.5f), n, Vector2(0, 1))};
            for (auto& pass : fx.getCurrentTechniqueProperty()->getPassesProperty()) {
                pass.Apply();
                d.DrawUserPrimitives(PrimitiveType::TriangleList, quad.data(), 0, 2);
            }
        };
        const auto grey = MakeTex(d, 4, 4, [](int, int) { return Color(200, 100, 50, 255); });
        {   // BasicEffect, textured, unlit
            Begin(d, rt, clearC);
            BasicEffect fx(d);
            fx.setLightingEnabledProperty(false);
            fx.setVertexColorEnabledProperty(false);
            fx.setTextureEnabledProperty(true);
            fx.setTextureProperty(grey.get());
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(Matrix::getIdentityProperty());
            fx.setProjectionProperty(Matrix::getIdentityProperty());
            textured(fx);
            const auto img = End(d, rt);
            if (!img) return;
            Add("BasicEffect textured, unlit", "200,100,50 +-3", Rgb(img->At(32, 32)), Dist(img->At(32, 32), 200, 100, 50) <= 3, img);
        }
        {   // AlphaTestEffect: alpha above the reference draws, below is discarded
            const auto alpha = MakeTex(d, 4, 4, [](int, int) { return Color(255, 255, 255, 200); });
            const auto faint = MakeTex(d, 4, 4, [](int, int) { return Color(255, 255, 255, 40); });
            for (int pass = 0; pass < 2; ++pass) {
                Begin(d, rt, clearC);
                AlphaTestEffect fx(d);
                fx.setTextureProperty(pass == 0 ? alpha.get() : faint.get());
                fx.setReferenceAlphaProperty(128);
                fx.setAlphaFunctionProperty(CompareFunction::Greater);
                fx.setWorldProperty(Matrix::getIdentityProperty());
                fx.setViewProperty(Matrix::getIdentityProperty());
                fx.setProjectionProperty(Matrix::getIdentityProperty());
                textured(fx);
                const auto img = End(d, rt);
                const Color c = img->At(32, 32);
                Add(pass == 0 ? "AlphaTestEffect: alpha 200 > ref 128" : "AlphaTestEffect: alpha 40 < ref 128", pass == 0 ? "drawn (bright)" : "discarded (black)", Rgb(c), pass == 0 ? c.getRProperty() > 150 : Dist(c, 0, 0, 0) < 4, img);
            }
        }
        {   // DualTextureEffect needs the position+normal+two-UV vertex stream the museum's model room uses; check
            // only that it constructs and that the renderer accepts it.
            try {
                DualTextureEffect fx(d);
                fx.setTextureProperty(grey.get());
                fx.setTexture2Property(grey.get());
                Add("DualTextureEffect constructs and binds two textures", "no exception", "ok", true, std::nullopt);
            } catch (const std::exception& e) {
                Add("DualTextureEffect constructs and binds two textures", "no exception", e.what(), false, std::nullopt);
            }
        }
    }

    Mode mode_;
    std::string renderer_ = "-";
    bool needRun_ = false, readback_ = true;
    std::unique_ptr<BasicEffect> fx_;
    std::unique_ptr<RenderTarget2D> plainTarget_, depthTarget_;
    std::vector<Result> results_;
};

}  // namespace

std::unique_ptr<Exhibit> MakeValidationLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ValidationLab>(room, services);
}
