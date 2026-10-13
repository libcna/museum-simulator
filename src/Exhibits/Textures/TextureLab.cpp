// Texture laboratory: mip levels, filtering, addressing, UV transforms, dynamic textures, surface formats,
// alpha, cube maps, volume textures and image encode/decode. One class, ten rooms ("mode").
#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <string>

#include "CNA/GraphicsCapability.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EnvironmentMapEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Alpha8.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Bgr565.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/Bgra4444.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/HalfVector4.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture3D.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureCube.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/MiniView.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "System/IO/MemoryStream.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Mip, Filter, Address, Uv, Dynamic, Formats, Alpha, Cube, Volume, Io };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"filter", Mode::Filter}, {"address", Mode::Address}, {"uv", Mode::Uv}, {"dynamic", Mode::Dynamic}, {"formats", Mode::Formats},
                                                         {"alpha", Mode::Alpha}, {"cube", Mode::Cube}, {"volume", Mode::Volume}, {"io", Mode::Io}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Mip;
}

class TextureLab final : public Exhibit {
public:
    TextureLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Mip:
                p.AddChoice("TextureFilter", {"Linear", "Point", "Anisotropic", "LinearMipPoint", "PointMipLinear"}, 0, [this](int v) { filter_ = v; });
                p.AddSlider("MipMapLevelOfDetailBias", -3.0f, 3.0f, bias_, [this](float v) { bias_ = v; });
                p.AddSlider("MaxMipLevel (0 = full detail)", 0.0f, 8.0f, maxMip_, [this](float v) { maxMip_ = v; }, "%.0f", 1.0f);
                p.AddSlider("MaxAnisotropy", 1.0f, 16.0f, aniso_, [this](float v) { aniso_ = v; }, "%.0f", 1.0f);
                p.AddToggle("Coloured mip levels", true, [this](bool v) { colouredMips_ = v; dirty_ = true; });
                break;
            case Mode::Filter:
                p.AddSlider("Tilt of the quads", 0.0f, 80.0f, tilt_, [this](float v) { tilt_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Zoom", 1.0f, 6.0f, zoom_, [this](float v) { zoom_ = v; });
                p.AddReadout("Texture", [] { return std::string("8 x 8 texels, with mip chain"); });
                break;
            case Mode::Address:
                p.AddSlider("UV range", 1.0f, 4.0f, zoom_, [this](float v) { zoom_ = v; });
                p.AddSlider("UV offset", -1.0f, 1.0f, offset_, [this](float v) { offset_ = v; });
                p.AddReadout("UV rectangle", [this] { return "(" + Fmt(offset_) + ", " + Fmt(offset_ + zoom_) + ")"; });
                break;
            case Mode::Uv:
                p.AddSlider("Tiling", 1.0f, 8.0f, zoom_, [this](float v) { zoom_ = v; });
                p.AddSlider("Scroll U", -1.0f, 1.0f, scrollU_, [this](float v) { scrollU_ = v; });
                p.AddSlider("Scroll V", -1.0f, 1.0f, scrollV_, [this](float v) { scrollV_ = v; });
                p.AddSlider("Rotate UVs", -180.0f, 180.0f, rot_, [this](float v) { rot_ = v; }, "%.0f deg", 1.0f);
                break;
            case Mode::Dynamic:
                p.AddChoice("Size", {"64 x 64", "128 x 128", "256 x 256"}, 1, [this](int v) { size_ = 64 << v; dirty_ = true; });
                p.AddToggle("Update every frame", true, [this](bool v) { animate_ = v; });
                p.AddReadout("SetData time", [this] { return Fmt(static_cast<float>(setMs_)) + " ms"; });
                p.AddReadout("GetData: centre texel", [this] { return readback_; });
                break;
            case Mode::Formats:
                p.AddReadout("Same 64x64 gradient, uploaded in each SurfaceFormat", [] { return std::string(""); });
                p.AddReadout("Colour (32-bit)", [this] { return status_[0]; });
                p.AddReadout("Bgr565 (16-bit)", [this] { return status_[1]; });
                p.AddReadout("Bgra4444 (16-bit)", [this] { return status_[2]; });
                p.AddReadout("Alpha8 (8-bit)", [this] { return status_[3]; });
                p.AddReadout("HalfVector4 (64-bit)", [this] { return status_[4]; });
                break;
            case Mode::Alpha:
                p.AddSlider("ReferenceAlpha", 0.0f, 255.0f, ref_, [this](float v) { ref_ = v; }, "%.0f", 1.0f);
                p.AddChoice("AlphaFunction", {"Greater", "GreaterEqual", "Less", "Equal"}, 0, [this](int v) { fn_ = v; });
                p.AddHeading("Left: AlphaTestEffect (cut-out).  Middle: blended.  Right: opaque");
                break;
            case Mode::Cube:
                p.AddSlider("EnvironmentMapAmount", 0.0f, 1.0f, amount_, [this](float v) { amount_ = v; });
                p.AddSlider("FresnelFactor", 0.0f, 4.0f, fresnel_, [this](float v) { fresnel_ = v; });
                p.AddSlider("Spin", 0.0f, 2.0f, speed_, [this](float v) { speed_ = v; });
                p.AddReadout("TextureCube", [] { return std::string("64 px per face, 6 faces"); });
                break;
            case Mode::Volume:
                p.AddSlider("Slice", 0.0f, 1.0f, slice_, [this](float v) { slice_ = v; });
                p.AddReadout("Texture3D", [] { return std::string("32 x 32 x 32 Color texels"); });
                p.AddReadout("Round trip checksum", [this] { return checksum_; });
                p.AddReadout("Sampled by stock effects?", [] { return std::string("no: store and read back only"); });
                break;
            case Mode::Io:
                p.AddChoice("Encoder", {"SaveAsPng (lossless)", "SaveAsJpeg (lossy)"}, 0, [this](int v) { jpeg_ = v == 1; ioDirty_ = true; });
                p.AddChoice("Encoded size", {"64 x 64", "128 x 128 (as is)", "256 x 256"}, 1, [this](int v) { encodeSize_ = 64 << v; ioDirty_ = true; });
                p.AddChoice("FromStream", {"original size", "fit in 96 x 48", "zoom to 96 x 48"}, 0, [this](int v) { decodeMode_ = v; ioDirty_ = true; });
                p.AddToggle("Source has transparency", true, [this](bool v) { alphaSource_ = v; ioDirty_ = true; });
                p.AddButton("Encode and decode again", [this] { ioDirty_ = true; });
                p.AddReadout("Encoded bytes", [this] { return ioBytes_; });
                p.AddReadout("Decoded texture", [this] { return ioSize_; });
                p.AddReadout("Round trip", [this] { return ioError_; });
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        GraphicsDevice& d = *S().device;
        if (mode_ == Mode::Mip) view_ = std::make_unique<MiniView>(S(), 640, 360);
        if (mode_ == Mode::Filter || mode_ == Mode::Mip) {
            std::vector<Color> px(64);
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) px[static_cast<std::size_t>(y * 8 + x)] = ((x + y) % 2) ? Color(235, 235, 240, 255) : Color(30, 60, 160, 255);
            small_ = std::make_unique<Texture2D>(d, 8, 8, true, SurfaceFormat::Color);
            small_->SetData(0, nullptr, px.data(), 0, 64);
            for (int level = 1; level < 4; ++level) {
                const int n = 8 >> level;
                std::vector<Color> lv(static_cast<std::size_t>(n * n), Color(130, 148, 200, 255));
                small_->SetData(level, nullptr, lv.data(), 0, n * n);
            }
        }
        if (mode_ == Mode::Cube) BuildCube();
        if (mode_ == Mode::Volume) BuildVolume();
        if (mode_ == Mode::Formats) BuildFormats();
        dirty_ = true;
        ioDirty_ = true;
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        if (mode_ == Mode::Uv) {
            scrollX_ += scrollU_ * ctx.dt * 0.5f;
            scrollY_ += scrollV_ * ctx.dt * 0.5f;
        }
        if (mode_ == Mode::Dynamic && (animate_ || dirty_)) UpdateDynamic();
        if (mode_ == Mode::Io && ioDirty_) RoundTrip();
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (mode_ != Mode::Mip || !view_) return;
        if (dirty_ || !mipTexture_) BuildMipTexture();
        const Vector3 eye(0, 0.55f, 0);
        const Matrix view = Matrix::CreateLookAt(eye, Vector3(0, 0.15f, -10), Vector3::Up);
        const Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(60.0f), view_->Aspect(), 0.1f, 200.0f);
        view_->Render(ctx, view, proj, eye, [this](const DrawContext& sub) {
            SamplerState s = Sampler();
            sub.device.getSamplerStatesProperty()[0] = s;
            BasicEffect& fx = sub.fx.World();
            fx.setTextureProperty(mipTexture_.get());
            fx.setTextureEnabledProperty(true);
            fx.setVertexColorEnabledProperty(true);
            fx.setDiffuseColorProperty(Vector3(1, 1, 1));
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(sub.view.view);
            fx.setProjectionProperty(sub.view.projection);
            EffectBank::Apply(fx);
            const VertexPositionColorTexture q[6] = {{Vector3(-4, 0, 0), Color::White, Vector2(0, 0)},  {Vector3(4, 0, 0), Color::White, Vector2(8, 0)},
                                                     {Vector3(4, 0, -120), Color::White, Vector2(8, 120)}, {Vector3(-4, 0, 0), Color::White, Vector2(0, 0)},
                                                     {Vector3(4, 0, -120), Color::White, Vector2(8, 120)}, {Vector3(-4, 0, -120), Color::White, Vector2(0, 120)}};
            sub.device.setRasterizerStateProperty(RasterizerState::CullNone);
            sub.device.DrawUserPrimitives(PrimitiveType::TriangleList, q, 0, 2);
        }, Color(150, 190, 240, 255));
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        switch (mode_) {
            case Mode::Mip: if (view_) view_->DrawInRoom(ctx, FarPoint0(2.2f), FarYaw(), 6.4f); break;
            case Mode::Filter: DrawFilter(ctx); break;
            case Mode::Address: DrawAddress(ctx); break;
            case Mode::Uv: DrawUv(ctx); break;
            case Mode::Dynamic: DrawDynamic(ctx); break;
            case Mode::Formats: DrawFormats(ctx); break;
            case Mode::Alpha: DrawAlpha(ctx); break;
            case Mode::Cube: DrawCube(ctx); break;
            case Mode::Volume: DrawVolume(ctx); break;
            case Mode::Io: DrawIo(ctx); break;
        }
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Mip levels and filtering", "Magnification filters", "Texture addressing modes", "UV transforms", "Dynamic textures and readback", "Surface formats", "Alpha: test, blend, opaque", "Cube maps and environment maps", "Volume textures", "Encoding and decoding images"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Mip: return {"A mip chain stores smaller copies of a texture so distant surfaces do not shimmer. Each level here has its own", "colour, so the screen shows which level the sampler picks as the ground recedes. Change the filter,", "the level-of-detail bias, the maximum level and anisotropy: every SamplerState property is live."};
            case Mode::Filter: return {"Magnifying an 8 x 8 texture: Point shows square texels, Linear blends them, Anisotropic keeps detail", "on surfaces seen at a steep angle. Tilt the quads to see the difference between Linear and Anisotropic."};
            case Mode::Address: return {"UV coordinates outside 0..1 are handled by TextureAddressMode: Wrap repeats the image, Clamp stretches the", "edge texels, Mirror flips every other repeat. The same uv grid is drawn on all three quads."};
            case Mode::Uv: return {"UV transforms need no shader: change the texture coordinates you submit. Tiling scales them, scrolling", "adds an offset every frame and rotation turns the sampling window. The vertices never move."};
            case Mode::Dynamic: return {"Texture2D.SetData rewrites a texture's pixels at run time (here an animated plasma) and GetData reads", "them back (the centre texel is shown). Readbacks stall the GPU, so games use them sparingly."};
            case Mode::Formats: return {"A texture can store colour in many SurfaceFormats. Fewer bits mean banding (Bgr565, Bgra4444), Alpha8 keeps", "only transparency and HalfVector4 stores 16-bit floats. Support depends on the renderer: unsupported ones say so."};
            case Mode::Alpha: return {"Three ways to treat a texture's alpha: AlphaTestEffect discards pixels below a reference value (cut-out", "foliage, no sorting needed), alpha blending mixes with the background, and opaque ignores alpha."};
            case Mode::Io: return {"Texture2D.SaveAsPng and SaveAsJpeg encode a texture into any Stream (here a MemoryStream, so nothing touches", "the disk) and Texture2D.FromStream decodes it again, optionally fitting or cropping it. PNG is lossless; JPEG is not and has no alpha."};
            case Mode::Cube: return {"A TextureCube stores six faces. EnvironmentMapEffect samples it by reflection direction, so the sphere", "mirrors a (procedural) sky. Amount blends the reflection in, Fresnel strengthens it at grazing angles."};
            default: return {"A Texture3D has width, height and depth. CNA stores it and reads it back (SetData / GetData); no stock effect", "samples it, so the slices shown are read back into ordinary textures."};
        }
    }

    [[nodiscard]] SamplerState Sampler() const
    {
        SamplerState s = filter_ == 2 ? SamplerState::AnisotropicWrap : SamplerState::LinearWrap;
        static const TextureFilter f[] = {TextureFilter::Linear, TextureFilter::Point, TextureFilter::Anisotropic, TextureFilter::LinearMipPoint, TextureFilter::PointMipLinear};
        s.setFilterProperty(f[filter_]);
        s.setMipMapLevelOfDetailBiasProperty(bias_);
        s.setMaxMipLevelProperty(static_cast<int>(maxMip_));
        s.setMaxAnisotropyProperty(static_cast<int>(aniso_));
        return s;
    }

    void BuildMipTexture()
    {
        GraphicsDevice& d = *S().device;
        const int n = 256;
        mipTexture_ = std::make_unique<Texture2D>(d, n, n, true, SurfaceFormat::Color);
        const Color colours[9] = {Color(230, 60, 60, 255), Color(240, 150, 50, 255), Color(240, 220, 60, 255), Color(90, 210, 90, 255), Color(60, 200, 200, 255),
                                  Color(70, 120, 240, 255), Color(150, 90, 230, 255), Color(230, 90, 200, 255), Color(240, 240, 240, 255)};
        for (int level = 0, size = n; size >= 1; ++level, size /= 2) {
            std::vector<Color> px(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
            for (int y = 0; y < size; ++y)
                for (int x = 0; x < size; ++x) {
                    const bool checker = ((x * 8 / size) + (y * 8 / size)) % 2 == 0;
                    Color c = colouredMips_ ? colours[std::min(level, 8)] : Color(235, 235, 235, 255);
                    if (!checker) c = Color(static_cast<std::uint8_t>(c.getRProperty() / 2), static_cast<std::uint8_t>(c.getGProperty() / 2), static_cast<std::uint8_t>(c.getBProperty() / 2), static_cast<std::uint8_t>(255));
                    px[static_cast<std::size_t>(y * size + x)] = c;
                }
            mipTexture_->SetData(level, nullptr, px.data(), 0, size * size);
        }
        dirty_ = false;
    }

    // Draws a textured quad with explicit UVs and a given sampler.
    void Quad(const DrawContext& ctx, Texture2D* tex, const Matrix& world, float w, float h, Vector2 uv0, Vector2 uv1, const SamplerState& sampler, float rotation = 0.0f)
    {
        GraphicsDevice& device = ctx.device;
        device.getSamplerStatesProperty()[0] = sampler;
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        BasicEffect& fx = ctx.fx.World();
        fx.setTextureProperty(tex);
        fx.setTextureEnabledProperty(true);
        fx.setVertexColorEnabledProperty(true);
        fx.setDiffuseColorProperty(Vector3(1, 1, 1));
        fx.setWorldProperty(world);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        EffectBank::Apply(fx);
        const Vector2 centre = (uv0 + uv1) * 0.5f;
        const auto rot = [&](Vector2 uv) {
            const float c = std::cos(rotation), s = std::sin(rotation);
            const Vector2 d = uv - centre;
            return centre + Vector2(d.X * c - d.Y * s, d.X * s + d.Y * c);
        };
        const float hw = w * 0.5f, hh = h * 0.5f;
        const VertexPositionColorTexture q[6] = {{Vector3(-hw, -hh, 0), Color::White, rot(Vector2(uv0.X, uv1.Y))}, {Vector3(hw, -hh, 0), Color::White, rot(Vector2(uv1.X, uv1.Y))},
                                                 {Vector3(hw, hh, 0), Color::White, rot(Vector2(uv1.X, uv0.Y))},   {Vector3(-hw, -hh, 0), Color::White, rot(Vector2(uv0.X, uv1.Y))},
                                                 {Vector3(hw, hh, 0), Color::White, rot(Vector2(uv1.X, uv0.Y))},   {Vector3(-hw, hh, 0), Color::White, rot(Vector2(uv0.X, uv0.Y))}};
        device.DrawUserPrimitives(PrimitiveType::TriangleList, q, 0, 2);
    }
    [[nodiscard]] Matrix At(float x, float y, float z, float yaw = 0.0f, float pitch = 0.0f) const
    {
        return Matrix::CreateRotationX(pitch) * Matrix::CreateRotationY(yaw + R().frameYaw) * Matrix::CreateTranslation(R().ToWorld(x, y, z));
    }

    void DrawFilter(const DrawContext& ctx)
    {
        const SamplerState ss[3] = {SamplerState::PointClamp, SamplerState::LinearClamp, SamplerState::AnisotropicClamp};
        const char* names[3] = {"Point", "Linear", "Anisotropic"};
        for (int i = 0; i < 3; ++i) {
            const float x = -2.6f + static_cast<float>(i) * 2.6f;
            Quad(ctx, small_.get(), At(x, 1.8f, 0.0f, 0.0f, -MathHelper::ToRadians(tilt_)), 2.3f, 2.3f, Vector2(0, 0), Vector2(1, 1), ss[i]);
            gizmos_.Label(R().ToWorld(x, 3.25f, 0.0f), names[i], Color(255, 230, 150, 255), 0.6f);
        }
        (void)zoom_;
    }
    void DrawAddress(const DrawContext& ctx)
    {
        const TextureAddressMode modes[3] = {TextureAddressMode::Wrap, TextureAddressMode::Clamp, TextureAddressMode::Mirror};
        const char* names[3] = {"Wrap", "Clamp", "Mirror"};
        for (int i = 0; i < 3; ++i) {
            SamplerState s = SamplerState::LinearClamp;
            s.setAddressUProperty(modes[i]);
            s.setAddressVProperty(modes[i]);
            const float x = -2.7f + static_cast<float>(i) * 2.7f;
            Quad(ctx, S().textures->Get("uvgrid"), At(x, 1.9f, 0.0f), 2.4f, 2.4f, Vector2(offset_, offset_), Vector2(offset_ + zoom_, offset_ + zoom_), s);
            gizmos_.Label(R().ToWorld(x, 3.35f, 0.0f), names[i], Color(255, 230, 150, 255), 0.6f);
        }
    }
    void DrawUv(const DrawContext& ctx)
    {
        Quad(ctx, S().textures->Get("brick"), At(0.0f, 1.9f, 0.0f), 3.6f, 3.6f, Vector2(scrollX_, scrollY_), Vector2(scrollX_ + zoom_, scrollY_ + zoom_), SamplerState::LinearWrap, MathHelper::ToRadians(rot_));
        gizmos_.Label(R().ToWorld(0.0f, 3.95f, 0.0f), "UV window: offset (" + Fmt(scrollX_) + ", " + Fmt(scrollY_) + "), size " + Fmt(zoom_), Color(255, 230, 150, 255), 0.55f);
    }

    void UpdateDynamic()
    {
        GraphicsDevice& d = *S().device;
        if (dirty_ || !dyn_ || dyn_->getWidthProperty() != size_) {
            dyn_ = std::make_unique<Texture2D>(d, size_, size_);
            pixels_.assign(static_cast<std::size_t>(size_) * static_cast<std::size_t>(size_), Color::Black);
            dirty_ = false;
        }
        const float t = time_;
        for (int y = 0; y < size_; ++y)
            for (int x = 0; x < size_; ++x) {
                const float u = static_cast<float>(x) / static_cast<float>(size_), v = static_cast<float>(y) / static_cast<float>(size_);
                const float val = std::sin(u * 10.0f + t) + std::sin(v * 8.0f - t * 1.3f) + std::sin((u + v) * 6.0f + t * 0.7f) + std::sin(std::sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f)) * 14.0f - t * 2.0f);
                pixels_[static_cast<std::size_t>(y * size_ + x)] = Color(static_cast<std::uint8_t>(127 + 127 * std::sin(val * 1.5f)), static_cast<std::uint8_t>(127 + 127 * std::sin(val * 1.5f + 2.1f)),
                                                                          static_cast<std::uint8_t>(127 + 127 * std::sin(val * 1.5f + 4.2f)), static_cast<std::uint8_t>(255));
            }
        RenderStates::UnbindTextures(d);
        const auto t0 = std::chrono::steady_clock::now();
        dyn_->SetData(pixels_.data(), static_cast<int>(pixels_.size()));
        setMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        Color centre;
        const Rectangle r(size_ / 2, size_ / 2, 1, 1);
        try {
            dyn_->GetData(0, &r, &centre, 0, 1);
            char buf[64];
            std::snprintf(buf, sizeof buf, "rgb(%d, %d, %d)", centre.getRProperty(), centre.getGProperty(), centre.getBProperty());
            readback_ = buf;
        } catch (const std::exception& e) {
            readback_ = std::string("failed: ") + e.what();
        }
    }
    void DrawDynamic(const DrawContext& ctx)
    {
        if (dyn_) Quad(ctx, dyn_.get(), At(0.0f, 1.9f, 0.0f), 3.6f, 3.6f, Vector2(0, 0), Vector2(1, 1), SamplerState::PointClamp);
    }

    void BuildFormats()
    {
        GraphicsDevice& d = *S().device;
        const int n = 64;
        const auto grad = [&](int x, int y, float& r, float& g, float& b, float& a) {
            r = static_cast<float>(x) / (n - 1);
            g = static_cast<float>(y) / (n - 1);
            b = 1.0f - r;
            a = 0.3f + 0.7f * g;
        };
        for (int f = 0; f < 5; ++f) {
            try {
                std::unique_ptr<Texture2D> t;
                switch (f) {
                    case 0: {
                        std::vector<Color> px(n * n);
                        for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) { float r, g, b, a; grad(x, y, r, g, b, a); px[static_cast<std::size_t>(y * n + x)] = Color(static_cast<std::uint8_t>(r * 255), static_cast<std::uint8_t>(g * 255), static_cast<std::uint8_t>(b * 255), static_cast<std::uint8_t>(255)); }
                        t = std::make_unique<Texture2D>(d, n, n, false, SurfaceFormat::Color);
                        t->SetData(px.data(), n * n);
                        break;
                    }
                    case 1: {
                        std::vector<PackedVector::Bgr565> px(n * n);
                        for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) { float r, g, b, a; grad(x, y, r, g, b, a); px[static_cast<std::size_t>(y * n + x)] = PackedVector::Bgr565(r, g, b); }
                        t = std::make_unique<Texture2D>(d, n, n, false, SurfaceFormat::Bgr565);
                        t->SetData(px.data(), n * n);
                        break;
                    }
                    case 2: {
                        std::vector<PackedVector::Bgra4444> px(n * n);
                        for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) { float r, g, b, a; grad(x, y, r, g, b, a); px[static_cast<std::size_t>(y * n + x)] = PackedVector::Bgra4444(r, g, b, 1.0f); }
                        t = std::make_unique<Texture2D>(d, n, n, false, SurfaceFormat::Bgra4444);
                        t->SetData(px.data(), n * n);
                        break;
                    }
                    case 3: {
                        std::vector<PackedVector::Alpha8> px(n * n);
                        for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) { float r, g, b, a; grad(x, y, r, g, b, a); px[static_cast<std::size_t>(y * n + x)] = PackedVector::Alpha8(r); }
                        t = std::make_unique<Texture2D>(d, n, n, false, SurfaceFormat::Alpha8);
                        t->SetData(px.data(), n * n);
                        break;
                    }
                    default: {
                        std::vector<PackedVector::HalfVector4> px(n * n);
                        for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) { float r, g, b, a; grad(x, y, r, g, b, a); px[static_cast<std::size_t>(y * n + x)] = PackedVector::HalfVector4(r, g, b, 1.0f); }
                        t = std::make_unique<Texture2D>(d, n, n, false, SurfaceFormat::HalfVector4);
                        t->SetData(px.data(), n * n);
                        break;
                    }
                }
                formats_[static_cast<std::size_t>(f)] = std::move(t);
                status_[static_cast<std::size_t>(f)] = "created and uploaded";
            } catch (const std::exception& e) {
                std::string m = e.what();
                status_[static_cast<std::size_t>(f)] = "not supported: " + m.substr(0, 28);
            }
        }
    }
    void DrawFormats(const DrawContext& ctx)
    {
        const char* names[5] = {"Color", "Bgr565", "Bgra4444", "Alpha8", "HalfVector4"};
        for (int f = 0; f < 5; ++f) {
            const float x = -4.2f + static_cast<float>(f) * 2.1f;
            if (formats_[static_cast<std::size_t>(f)]) {
                if (f == 3) {   // Alpha8 stores only transparency: draw it blended over a pale backdrop so it shows
                    Quad(ctx, S().textures->White(), At(x, 1.9f, -0.02f), 1.9f, 1.9f, Vector2(0, 0), Vector2(1, 1), SamplerState::PointClamp);
                    ctx.device.setBlendStateProperty(BlendState::NonPremultiplied);
                    ctx.device.setDepthStencilStateProperty(DepthStencilState::DepthRead);
                }
                Quad(ctx, formats_[static_cast<std::size_t>(f)].get(), At(x, 1.9f, 0.0f), 1.9f, 1.9f, Vector2(0, 0), Vector2(1, 1), SamplerState::PointClamp);
                ctx.device.setBlendStateProperty(BlendState::Opaque);
                ctx.device.setDepthStencilStateProperty(DepthStencilState::Default);
            }
            gizmos_.Label(R().ToWorld(x, 3.05f, 0.0f), names[f], formats_[static_cast<std::size_t>(f)] ? Color(255, 230, 150, 255) : Color(255, 140, 130, 255), 0.55f);
        }
    }

    void DrawAlpha(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        Texture2D* tex = S().textures->Get("particle");
        // Backdrop stripes so transparency is visible.
        for (int i = 0; i < 3; ++i)
            Quad(ctx, S().textures->Get("checker"), At(-2.6f + static_cast<float>(i) * 2.6f, 1.9f, -0.05f), 2.4f, 2.4f, Vector2(0, 0), Vector2(3, 3), SamplerState::PointWrap);
        static const CompareFunction fns[] = {CompareFunction::Greater, CompareFunction::GreaterEqual, CompareFunction::Less, CompareFunction::Equal};
        if (!alphaFx_) alphaFx_ = std::make_unique<AlphaTestEffect>(device);
        alphaFx_->setTextureProperty(tex);
        alphaFx_->setAlphaFunctionProperty(fns[fn_]);
        alphaFx_->setReferenceAlphaProperty(static_cast<int>(ref_));
        alphaFx_->setViewProperty(ctx.view.view);
        alphaFx_->setProjectionProperty(ctx.view.projection);
        const Color white = Color::White;
        const VertexPositionColorTexture q[6] = {{Vector3(-1.2f, -1.2f, 0), white, Vector2(0, 1)}, {Vector3(1.2f, -1.2f, 0), white, Vector2(1, 1)}, {Vector3(1.2f, 1.2f, 0), white, Vector2(1, 0)},
                                                 {Vector3(-1.2f, -1.2f, 0), white, Vector2(0, 1)}, {Vector3(1.2f, 1.2f, 0), white, Vector2(1, 0)}, {Vector3(-1.2f, 1.2f, 0), white, Vector2(0, 0)}};
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        device.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
        device.setBlendStateProperty(BlendState::Opaque);
        alphaFx_->setWorldProperty(At(-2.6f, 1.9f, 0.0f));
        alphaFx_->setVertexColorEnabledProperty(true);
        for (auto& pass : alphaFx_->getCurrentTechniqueProperty()->getPassesProperty()) pass.Apply();
        device.DrawUserPrimitives(PrimitiveType::TriangleList, q, 0, 2);
        // Middle: alpha blending.
        device.setBlendStateProperty(BlendState::NonPremultiplied);
        device.setDepthStencilStateProperty(DepthStencilState::DepthRead);
        Quad(ctx, tex, At(0.0f, 1.9f, 0.0f), 2.4f, 2.4f, Vector2(0, 0), Vector2(1, 1), SamplerState::LinearClamp);
        device.setBlendStateProperty(BlendState::Opaque);
        device.setDepthStencilStateProperty(DepthStencilState::Default);
        // Right: opaque (alpha ignored; the texture's colour channels show).
        Quad(ctx, tex, At(2.6f, 1.9f, 0.0f), 2.4f, 2.4f, Vector2(0, 0), Vector2(1, 1), SamplerState::LinearClamp);
        gizmos_.Label(R().ToWorld(-2.6f, 3.3f, 0.0f), "AlphaTestEffect", Color(255, 230, 150, 255), 0.55f);
        gizmos_.Label(R().ToWorld(0.0f, 3.3f, 0.0f), "NonPremultiplied blend", Color(255, 230, 150, 255), 0.55f);
        gizmos_.Label(R().ToWorld(2.6f, 3.3f, 0.0f), "Opaque", Color(255, 230, 150, 255), 0.55f);
    }

    void BuildCube()
    {
        GraphicsDevice& d = *S().device;
        const int n = 64;
        cube_ = std::make_unique<TextureCube>(d, n, false, SurfaceFormat::Color);
        const CubeMapFace faces[6] = {CubeMapFace::PositiveX, CubeMapFace::NegativeX, CubeMapFace::PositiveY, CubeMapFace::NegativeY, CubeMapFace::PositiveZ, CubeMapFace::NegativeZ};
        const Color tints[6] = {Color(230, 90, 90, 255), Color(90, 230, 140, 255), Color(130, 190, 255, 255), Color(70, 60, 50, 255), Color(240, 220, 100, 255), Color(200, 120, 230, 255)};
        for (int f = 0; f < 6; ++f) {
            std::vector<Color> px(static_cast<std::size_t>(n * n));
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x) {
                    const bool stripe = ((x / 8) + (y / 8)) % 2 == 0;
                    const float k = stripe ? 1.0f : 0.65f;
                    const float g = 0.6f + 0.4f * static_cast<float>(y) / n;
                    px[static_cast<std::size_t>(y * n + x)] = Color(static_cast<std::uint8_t>(tints[f].getRProperty() * k * g), static_cast<std::uint8_t>(tints[f].getGProperty() * k * g),
                                                                      static_cast<std::uint8_t>(tints[f].getBProperty() * k * g), static_cast<std::uint8_t>(255));
                }
            cube_->SetData(faces[f], px.data(), n * n);
        }
        envFx_ = std::make_unique<EnvironmentMapEffect>(d);
        envFx_->EnableDefaultLighting();
        envFx_->setEnvironmentMapProperty(cube_.get());
    }
    void DrawCube(const DrawContext& ctx)
    {
        if (!envFx_) return;
        GraphicsDevice& device = ctx.device;
        envFx_->setTextureProperty(S().textures->White());
        envFx_->setDiffuseColorProperty(Vector3(0.85f, 0.85f, 0.9f));
        envFx_->setEnvironmentMapAmountProperty(amount_);
        envFx_->setFresnelFactorProperty(fresnel_);
        envFx_->setViewProperty(ctx.view.view);
        envFx_->setProjectionProperty(ctx.view.projection);
        envFx_->setWorldProperty(Matrix::CreateScale(2.2f) * Matrix::CreateRotationY(time_ * speed_) * Matrix::CreateTranslation(R().ToWorld(0.0f, 1.9f, 0.0f)));
        RenderStates::SetOpaque3D(device);
        for (auto& pass : envFx_->getCurrentTechniqueProperty()->getPassesProperty()) pass.Apply();
        S().shapes->Sphere().Draw(device);
    }

    void BuildVolume()
    {
        GraphicsDevice& d = *S().device;
        const int n = 32;
        std::vector<Color> data(static_cast<std::size_t>(n * n * n));
        for (int z = 0; z < n; ++z)
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x) {
                    const float fx = static_cast<float>(x), fy = static_cast<float>(y), fz = static_cast<float>(z);
                    const float v = 0.5f + 0.5f * std::sin(fx * 0.4f) * std::cos(fy * 0.35f) * std::sin(fz * 0.3f + 1.0f);
                    const float dx = fx - 16.0f, dy = fy - 16.0f, dz = fz - 16.0f;
                    const float ring = std::exp(-std::pow(std::sqrt(dx * dx + dy * dy + dz * dz) - 9.0f, 2.0f) * 0.12f);
                    data[static_cast<std::size_t>((z * n + y) * n + x)] = Color(static_cast<std::uint8_t>(255 * ring), static_cast<std::uint8_t>(255 * v), static_cast<std::uint8_t>(255 * (1 - v)), static_cast<std::uint8_t>(255));
                }
        try {
            volume_ = std::make_unique<Texture3D>(d, n, n, n, false, SurfaceFormat::Color);
            volume_->SetData(data.data(), static_cast<int>(data.size()));
            std::vector<Color> back(data.size());
            volume_->GetData(back.data(), static_cast<int>(back.size()));
            unsigned long long a = 0, b = 0;
            for (std::size_t i = 0; i < data.size(); ++i) {
                a += data[i].getPackedValueProperty();
                b += back[i].getPackedValueProperty();
            }
            checksum_ = a == b ? "identical after GetData (OK)" : "MISMATCH after GetData";
            volumeData_ = back;
        } catch (const std::exception& e) {
            checksum_ = std::string("Texture3D not available: ") + std::string(e.what()).substr(0, 24);
        }
    }
    void DrawVolume(const DrawContext& ctx)
    {
        if (volumeData_.empty()) return;
        const int n = 32;
        const int z = std::clamp(static_cast<int>(slice_ * (n - 1)), 0, n - 1);
        if (z != shownSlice_ || !sliceTex_) {
            std::vector<Color> px(volumeData_.begin() + z * n * n, volumeData_.begin() + (z + 1) * n * n);
            sliceTex_ = std::make_unique<Texture2D>(*S().device, n, n);
            sliceTex_->SetData(px.data(), n * n);
            shownSlice_ = z;
        }
        Quad(ctx, sliceTex_.get(), At(0.0f, 1.9f, 0.0f), 3.2f, 3.2f, Vector2(0, 0), Vector2(1, 1), SamplerState::PointClamp);
        gizmos_.Label(R().ToWorld(0.0f, 3.7f, 0.0f), "slice " + std::to_string(z) + " of 32 (read back with GetData)", Color(255, 230, 150, 255), 0.55f);
    }

    // Source image: a colour wheel, fine diagonal lines (hard for JPEG) and, optionally, a transparent corner.
    [[nodiscard]] std::vector<Color> MakeIoSource(int n) const
    {
        std::vector<Color> px(static_cast<std::size_t>(n * n));
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                const float u = static_cast<float>(x) / static_cast<float>(n - 1) - 0.5f, v = static_cast<float>(y) / static_cast<float>(n - 1) - 0.5f;
                const float hue = std::atan2(v, u) * 0.9549297f + 3.0f;   // 0..6
                const float r = std::sqrt(u * u + v * v) * 2.0f;
                const auto ch = [&](float off) { return std::clamp(std::abs(std::fmod(hue + off, 6.0f) - 3.0f) - 1.0f, 0.0f, 1.0f); };
                float red = ch(0.0f), grn = ch(4.0f), blu = ch(2.0f);
                const float mix = std::clamp(1.2f - r, 0.0f, 1.0f);
                red = 1.0f - mix * (1.0f - red); grn = 1.0f - mix * (1.0f - grn); blu = 1.0f - mix * (1.0f - blu);
                if (((x + y) % 8) < 1) { red *= 0.2f; grn *= 0.2f; blu *= 0.2f; }
                const bool hole = alphaSource_ && (x > n * 3 / 4) && (y < n / 4);
                px[static_cast<std::size_t>(y * n + x)] = Color(static_cast<std::uint8_t>(red * 255), static_cast<std::uint8_t>(grn * 255), static_cast<std::uint8_t>(blu * 255), static_cast<std::uint8_t>(hole ? 0 : 255));
            }
        return px;
    }

    void RoundTrip()
    {
        ioDirty_ = false;
        GraphicsDevice& d = *S().device;
        try {
            RenderStates::UnbindTextures(d);
            constexpr int n = 128;
            const std::vector<Color> src = MakeIoSource(n);
            ioSource_ = std::make_unique<Texture2D>(d, n, n);
            ioSource_->SetData(src.data(), n * n);
            System::IO::MemoryStream encoded;
            if (jpeg_) ioSource_->SaveAsJpeg(&encoded, encodeSize_, encodeSize_);
            else ioSource_->SaveAsPng(&encoded, encodeSize_, encodeSize_);
            const std::vector<SharpRuntime::bytecs> bytes = encoded.ToArray();
            char buf[96];
            std::snprintf(buf, sizeof buf, "%zu bytes (raw %d), %.1f%%", bytes.size(), encodeSize_ * encodeSize_ * 4, 100.0 * static_cast<double>(bytes.size()) / (encodeSize_ * encodeSize_ * 4.0));
            ioBytes_ = buf;
            System::IO::MemoryStream in(bytes.data(), static_cast<int>(bytes.size()), false);
            if (decodeMode_ == 0) ioDecoded_ = std::make_unique<Texture2D>(Texture2D::FromStream(d, in));
            else ioDecoded_ = std::make_unique<Texture2D>(Texture2D::FromStream(d, in, 96, 48, decodeMode_ == 2));
            ioSize_ = std::to_string(ioDecoded_->getWidthProperty()) + " x " + std::to_string(ioDecoded_->getHeightProperty());
            if (ioDecoded_->getWidthProperty() == n && ioDecoded_->getHeightProperty() == n) {
                std::vector<Color> back(src.size());
                ioDecoded_->GetData(back.data(), n * n);
                double sum = 0.0;
                int worst = 0, exact = 0;
                std::vector<Color> diff(src.size());
                for (std::size_t i = 0; i < src.size(); ++i) {
                    const int dr = std::abs(src[i].getRProperty() - back[i].getRProperty()), dg = std::abs(src[i].getGProperty() - back[i].getGProperty());
                    const int db = std::abs(src[i].getBProperty() - back[i].getBProperty()), da = std::abs(src[i].getAProperty() - back[i].getAProperty());
                    // Colour under a fully transparent pixel carries no information, so it is not compared.
                    const bool invisible = src[i].getAProperty() == 0 && back[i].getAProperty() == 0;
                    const int m = invisible ? 0 : std::max(std::max(dr, dg), std::max(db, da));
                    if (!invisible) sum += (dr + dg + db + da) / 4.0;
                    worst = std::max(worst, m);
                    exact += m == 0;
                    const auto amp = static_cast<std::uint8_t>(std::min(255, m * 8));
                    diff[i] = Color(amp, amp, amp, static_cast<std::uint8_t>(255));
                }
                std::snprintf(buf, sizeof buf, "%s: mean error %.2f, worst %d", worst == 0 ? "bit-exact" : "differs", sum / static_cast<double>(src.size()), worst);
                ioError_ = buf;
                Museum::Log("textures", "image round trip (%s, %d px, decode mode %d): %s, %s", jpeg_ ? "JPEG" : "PNG", encodeSize_, decodeMode_, ioBytes_.c_str(), buf);
                ioDiff_ = std::make_unique<Texture2D>(d, n, n);
                ioDiff_->SetData(diff.data(), n * n);
            } else {
                ioError_ = "sizes differ: no pixel comparison";
                Museum::Log("textures", "image round trip (%s, %d px, decode mode %d): %s, decoded %s", jpeg_ ? "JPEG" : "PNG", encodeSize_, decodeMode_, ioBytes_.c_str(), ioSize_.c_str());
                ioDiff_.reset();
            }
        } catch (const std::exception& e) {
            ioError_ = std::string("failed: ") + std::string(e.what()).substr(0, 60);
            Museum::Log("textures", "image round trip failed: %s", e.what());
        }
    }
    void DrawIo(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        const auto draw = [&](Texture2D* t, float x, float w, float h) {
            if (!t) return;
            Quad(ctx, S().textures->Get("checker"), At(x, 1.9f, -0.02f), w, h, Vector2(0, 0), Vector2(3, 3), SamplerState::PointWrap);
            device.setBlendStateProperty(BlendState::NonPremultiplied);
            device.setDepthStencilStateProperty(DepthStencilState::DepthRead);
            Quad(ctx, t, At(x, 1.9f, 0.0f), w, h, Vector2(0, 0), Vector2(1, 1), SamplerState::PointClamp);
            device.setBlendStateProperty(BlendState::Opaque);
            device.setDepthStencilStateProperty(DepthStencilState::Default);
        };
        draw(ioSource_.get(), -2.7f, 2.4f, 2.4f);
        if (ioDecoded_) {
            const float aspect = static_cast<float>(ioDecoded_->getWidthProperty()) / static_cast<float>(ioDecoded_->getHeightProperty());
            draw(ioDecoded_.get(), 0.0f, aspect >= 1.0f ? 2.4f : 2.4f * aspect, aspect >= 1.0f ? 2.4f / aspect : 2.4f);
        }
        if (ioDiff_) draw(ioDiff_.get(), 2.7f, 2.4f, 2.4f);
        const Color c(255, 230, 150, 255);
        gizmos_.Label(R().ToWorld(-2.7f, 3.4f, 0.0f), "source (128 x 128)", c, 0.5f);
        gizmos_.Label(R().ToWorld(0.0f, 3.4f, 0.0f), std::string(jpeg_ ? "SaveAsJpeg" : "SaveAsPng") + " -> FromStream", c, 0.5f);
        gizmos_.Label(R().ToWorld(2.7f, 3.4f, 0.0f), "difference x8", c, 0.5f);
    }

    Mode mode_;
    Gizmos gizmos_;
    std::unique_ptr<MiniView> view_;
    std::unique_ptr<Texture2D> small_, mipTexture_, dyn_, sliceTex_, ioSource_, ioDecoded_, ioDiff_;
    std::array<std::unique_ptr<Texture2D>, 5> formats_;
    std::array<std::string, 5> status_{"-", "-", "-", "-", "-"};
    std::unique_ptr<TextureCube> cube_;
    std::unique_ptr<EnvironmentMapEffect> envFx_;
    std::unique_ptr<AlphaTestEffect> alphaFx_;
    std::unique_ptr<Texture3D> volume_;
    std::vector<Color> pixels_, volumeData_;
    std::string readback_ = "-", checksum_ = "-", ioBytes_ = "-", ioSize_ = "-", ioError_ = "-";
    double setMs_ = 0.0;
    float time_ = 0.0f, scrollX_ = 0.0f, scrollY_ = 0.0f;
    int filter_ = 0, size_ = 128, fn_ = 0, shownSlice_ = -1;
    float bias_ = 0.0f, maxMip_ = 0.0f, aniso_ = 4.0f, tilt_ = 55.0f, zoom_ = 2.0f, offset_ = 0.0f, scrollU_ = 0.3f, scrollV_ = 0.0f, rot_ = 0.0f, ref_ = 128.0f;
    float amount_ = 0.8f, fresnel_ = 1.0f, speed_ = 0.5f, slice_ = 0.5f;
    int encodeSize_ = 128, decodeMode_ = 0;
    bool dirty_ = true, colouredMips_ = true, animate_ = true, jpeg_ = false, alphaSource_ = true, ioDirty_ = true;
};

}  // namespace

std::unique_ptr<Exhibit> MakeTextureLab(const RoomDef& room, Services& services)
{
    return std::make_unique<TextureLab>(room, services);
}
