// SpriteBatch laboratory: thirteen rooms, one class. Each room draws a 960 x 540 screen with SpriteBatch
// into a render target, using exactly the state it demonstrates (sort mode, blend, sampler, rasterizer,
// transform), next to a control console. The live screen is an ordinary RenderTarget2D shown on the far wall.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <random>
#include <string>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Rendering/LiveScreen.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

constexpr int kW = 960, kH = 540;

enum class Mode { Basics, Origin, Sheet, Tint, Blend, Sort, Sampler, Parallax, Ui, Atlas, Stress, Scissor, Camera, Hit, Anim };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"origin", Mode::Origin}, {"sheet", Mode::Sheet}, {"tint", Mode::Tint}, {"blend", Mode::Blend}, {"sort", Mode::Sort},
                                                         {"sampler", Mode::Sampler}, {"parallax", Mode::Parallax}, {"ui", Mode::Ui}, {"atlas", Mode::Atlas},
                                                         {"stress", Mode::Stress}, {"scissor", Mode::Scissor}, {"camera", Mode::Camera}, {"hit", Mode::Hit}, {"anim", Mode::Anim}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Basics;
}

class SpriteLab final : public ScreenExhibit {
public:
    SpriteLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services), mode_(ParseMode(room.params.Get("mode"))) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Basics:
                p.AddSlider("Position X", 80.0f, 880.0f, posX_, [this](float v) { posX_ = v; }, "%.0f", 1.0f);
                p.AddSlider("Position Y", 60.0f, 480.0f, posY_, [this](float v) { posY_ = v; }, "%.0f", 1.0f);
                p.AddSlider("Rotation", -180.0f, 180.0f, rot_, [this](float v) { rot_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Scale", 0.2f, 1.5f, scale_, [this](float v) { scale_ = v; });
                p.AddToggle("Flip horizontally", false, [this](bool v) { flipH_ = v; });
                p.AddToggle("Flip vertically", false, [this](bool v) { flipV_ = v; });
                p.AddToggle("Origin at the centre", true, [this](bool v) { centre_ = v; });
                break;
            case Mode::Origin:
                p.AddSlider("Origin X", 0.0f, 1.0f, originX_, [this](float v) { originX_ = v; });
                p.AddSlider("Origin Y", 0.0f, 1.0f, originY_, [this](float v) { originY_ = v; });
                p.AddSlider("Spin speed", -3.0f, 3.0f, speed_, [this](float v) { speed_ = v; });
                break;
            case Mode::Sheet:
                p.AddSlider("Frames per second", 1.0f, 16.0f, fps_, [this](float v) { fps_ = v; }, "%.0f", 1.0f);
                p.AddToggle("Use SourceRectangle", true, [this](bool v) { useSource_ = v; });
                p.AddToggle("Flip horizontally", false, [this](bool v) { flipH_ = v; });
                p.AddChoice("Sampler", {"PointClamp", "LinearClamp"}, 0, [this](int v) { sampler_ = v; });
                p.AddReadout("Current source rectangle", [this] {
                    const int f = static_cast<int>(time_ * fps_) % 4;
                    return "(" + std::to_string((f % 2) * 8) + ", " + std::to_string((f / 2) * 8) + ", 8, 8)";
                });
                break;
            case Mode::Tint:
                p.AddSlider("Red", 0.0f, 255.0f, tint_[0], [this](float v) { tint_[0] = v; }, "%.0f", 1.0f);
                p.AddSlider("Green", 0.0f, 255.0f, tint_[1], [this](float v) { tint_[1] = v; }, "%.0f", 1.0f);
                p.AddSlider("Blue", 0.0f, 255.0f, tint_[2], [this](float v) { tint_[2] = v; }, "%.0f", 1.0f);
                p.AddSlider("Alpha", 0.0f, 255.0f, tint_[3], [this](float v) { tint_[3] = v; }, "%.0f", 1.0f);
                p.AddReadout("Color.PackedValue", [this] { char buf[32]; std::snprintf(buf, sizeof buf, "0x%08X", TintColor().getPackedValueProperty()); return std::string(buf); });
                break;
            case Mode::Blend:
                p.AddSlider("Sprite opacity", 0.1f, 1.0f, alpha_, [this](float v) { alpha_ = v; });
                p.AddToggle("Colour the sprites", true, [this](bool v) { coloured_ = v; });
                break;
            case Mode::Sort:
                p.AddChoice("SpriteSortMode", {"Deferred", "Immediate", "Texture", "BackToFront", "FrontToBack"}, 0, [this](int v) { sort_ = v; });
                p.AddSlider("Blue sprite layerDepth", 0.0f, 1.0f, layer_, [this](float v) { layer_ = v; });
                p.AddReadout("Draw order", [this] { return SortDescription(); });
                break;
            case Mode::Sampler:
                p.AddChoice("Filter", {"Point", "Linear", "Anisotropic"}, 0, [this](int v) { filter_ = v; });
                p.AddChoice("Address mode", {"Wrap", "Clamp", "Mirror"}, 0, [this](int v) { address_ = v; });
                p.AddSlider("Magnification", 4.0f, 40.0f, scale_ * 10.0f, [this](float v) { scale_ = v / 10.0f; }, "%.0fx", 1.0f);
                break;
            case Mode::Parallax:
                p.AddSlider("Camera speed", -400.0f, 400.0f, speed_ * 100.0f, [this](float v) { speed_ = v / 100.0f; }, "%.0f px/s", 1.0f);
                p.AddToggle("Parallax (layers differ)", true, [this](bool v) { parallax_ = v; });
                break;
            case Mode::Ui:
                p.AddSlider("Panel width", 60.0f, 520.0f, posX_, [this](float v) { posX_ = v; }, "%.0f px", 1.0f);
                p.AddSlider("Panel height", 40.0f, 340.0f, posY_, [this](float v) { posY_ = v; }, "%.0f px", 1.0f);
                p.AddToggle("Nine-slice (off = stretched)", true, [this](bool v) { nine_ = v; });
                break;
            case Mode::Atlas:
                p.AddToggle("Use the atlas", true, [this](bool v) { useAtlas_ = v; });
                p.AddReadout("Texture binds per frame", [this] { return useAtlas_ ? "1 (the atlas)" : "4 (one per texture)"; });
                p.AddReadout("Atlas size", [] { return std::string("128 x 128, four 64 x 64 tiles"); });
                break;
            case Mode::Stress:
                p.AddSlider("Sprites", 100.0f, 20000.0f, static_cast<float>(count_), [this](float v) { count_ = static_cast<int>(v); EnsureSprites(); }, "%.0f", 100.0f);
                p.AddReadout("Drawn per second", [this] { return std::to_string(static_cast<long long>(S().stats->drawsPerSecond * static_cast<float>(count_))) + " sprites"; });
                p.AddReadout("Frame rate", [this] { return Fmt1(S().stats->fps) + " fps"; });
                break;
            case Mode::Scissor:
                p.AddToggle("ScissorTestEnable", true, [this](bool v) { scissor_ = v; });
                p.AddSlider("Scissor X", 0.0f, 600.0f, posX_, [this](float v) { posX_ = v; }, "%.0f", 1.0f);
                p.AddSlider("Scissor width", 80.0f, 700.0f, posY_ + 100.0f, [this](float v) { posY_ = v - 100.0f; }, "%.0f", 1.0f);
                break;
            case Mode::Camera:
                p.AddSlider("Zoom", 0.4f, 3.0f, scale_, [this](float v) { scale_ = v; });
                p.AddSlider("Rotation", -90.0f, 90.0f, rot_, [this](float v) { rot_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Pan X", -300.0f, 300.0f, posX_ - 480.0f, [this](float v) { posX_ = 480.0f + v; }, "%.0f", 1.0f);
                p.AddToggle("Follow the player dot", false, [this](bool v) { follow_ = v; });
                break;
            case Mode::Hit:
                p.AddSlider("Moving sprite X", 200.0f, 760.0f, hitX_, [this](float v) { hitX_ = v; auto_ = false; }, "%.0f", 1.0f);
                p.AddSlider("Moving sprite Y", 120.0f, 420.0f, hitY_, [this](float v) { hitY_ = v; auto_ = false; }, "%.0f", 1.0f);
                p.AddSlider("Rotation", -180.0f, 180.0f, rot_, [this](float v) { rot_ = v; auto_ = false; }, "%.0f deg", 1.0f);
                p.AddSlider("Scale", 0.5f, 2.0f, scale_, [this](float v) { scale_ = v; });
                p.AddToggle("Drift on its own", true, [this](bool v) { auto_ = v; });
                p.AddReadout("Rectangle.Intersects", [this] { return std::string(rectHit_ ? "true" : "false"); });
                p.AddReadout("Per-pixel test", [this] { return std::string(pixelHits_ > 0 ? "HIT: " + std::to_string(pixelHits_) + " shared pixels" : "no shared pixel"); });
                p.AddReadout("Pixels examined", [this] { return std::to_string(pixelsTested_) + " in " + Fmt1(static_cast<float>(testMs_ * 1000.0)) + " us"; });
                break;
            case Mode::Anim:
                p.AddChoice("Clip", {"Idle (frames 0-1)", "Walk (frames 2-5)", "Jump (frames 6-7)", "All eight"}, 1, [this](int v) { clip_ = v; animTime_ = 0.0f; });
                p.AddChoice("Loop mode", {"Loop", "Ping-pong", "Play once"}, 0, [this](int v) { loop_ = v; animTime_ = 0.0f; });
                p.AddSlider("Frames per second", 1.0f, 24.0f, fps_, [this](float v) { fps_ = v; }, "%.0f", 1.0f);
                p.AddToggle("Playing", true, [this](bool v) { playing_ = v; });
                p.AddToggle("Walk across the screen", true, [this](bool v) { travel_ = v; });
                p.AddReadout("Frame / source rectangle", [this] { return AnimDescription(); });
                break;
        }
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
        if (mode_ == Mode::Ui) BuildFrameTexture();
        if (mode_ == Mode::Atlas) BuildAtlas();
        if (mode_ == Mode::Stress) EnsureSprites();
        if (mode_ == Mode::Hit) BuildHitTextures();
        if (mode_ == Mode::Anim) BuildAnimSheet();
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        if (mode_ == Mode::Stress) {
            for (Bouncer& s : sprites_) {
                s.x += s.vx * ctx.dt;
                s.y += s.vy * ctx.dt;
                if (s.x < 0 || s.x > static_cast<float>(kW - 8)) s.vx = -s.vx;
                if (s.y < 0 || s.y > static_cast<float>(kH - 8)) s.vy = -s.vy;
            }
        }
        if (mode_ == Mode::Parallax) scroll_ += speed_ * 100.0f * ctx.dt;
        if (mode_ == Mode::Hit && auto_) {
            hitX_ = 480.0f + std::cos(time_ * 0.5f) * 230.0f;
            hitY_ = 270.0f + std::sin(time_ * 0.8f) * 110.0f;
            rot_ = std::fmod(rot_ + 25.0f * ctx.dt + 180.0f, 360.0f) - 180.0f;
        }
        if (mode_ == Mode::Anim && playing_) {
            animTime_ += ctx.dt;
            if (travel_ && clip_ != 0) walkX_ += walkDir_ * (clip_ == 1 ? 160.0f : 220.0f) * ctx.dt;
            if (walkX_ > 820.0f) { walkX_ = 820.0f; walkDir_ = -1.0f; }
            if (walkX_ < 140.0f) { walkX_ = 140.0f; walkDir_ = 1.0f; }
        }
    }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* names[] = {"Sprites: position, rotation, scale and flip", "Sprite origin and rotation", "Sprite sheets and source rectangles", "Colour tint and alpha", "BlendState presets side by side", "SpriteSortMode and layer depth", "SamplerState filtering and addressing", "Scrolling and parallax", "UI panels and nine-slice", "Texture atlases", "Thousands of sprites", "Scissor rectangle clipping", "A 2D camera with a transform matrix", "Per-pixel collision", "Sprite animation: clips and loop modes"};
        return names[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Basics: return {"SpriteBatch.Draw places a texture with a position, colour, rotation (radians), origin, scale and", "SpriteEffects flips. The origin is the point inside the texture that sits at the position and is the", "centre of rotation and scaling. The screen on the far wall is a RenderTarget2D drawn with SpriteBatch."};
            case Mode::Origin: return {"The same rotation looks completely different depending on the origin. Top-left swings the sprite", "around its corner, centre spins it in place; the white dot marks where each origin lands."};
            case Mode::Sheet: return {"A sprite sheet packs frames into one texture; the SourceRectangle picks one. This 16 x 16 coin sheet", "holds four 8 x 8 frames. Point sampling keeps pixel art crisp when it is enlarged; linear blurs it."};
            case Mode::Tint: return {"The Color argument multiplies the texture: white leaves it alone, grey darkens it, a coloured tint", "filters it, and the alpha channel fades it. Behind the logo is a checkerboard so transparency shows."};
            case Mode::Blend: return {"Eight BlendState configurations draw the same two overlapping sprites over a gradient: Opaque,", "AlphaBlend (premultiplied), NonPremultiplied, Additive, a custom Multiply, ReverseSubtract and the Min", "and Max blend functions. The differences are the whole point of BlendState."};
            case Mode::Sort: return {"SpriteBatch can submit sprites in different orders: Deferred keeps call order, Texture groups by texture", "to save state changes, BackToFront and FrontToBack sort by layerDepth. Move the blue sprite's layer."};
            case Mode::Sampler: return {"A SamplerState decides how a texture is read between texels (Point, Linear, Anisotropic) and what", "happens beyond the 0..1 range (Wrap repeats, Clamp stretches the edge, Mirror flips every repeat)."};
            case Mode::Parallax: return {"Parallax scrolls distant layers slower than near ones to fake depth. Each layer here is a texture drawn", "with a SamplerState of Wrap and a SourceRectangle larger than the texture, shifted every frame."};
            case Mode::Ui: return {"Nine-slice (or nine-patch) stretches only the middle of a framed image so corners and edges keep", "their pixel size: nine Draw calls with source and destination rectangles. Stretching the whole image blurs the frame."};
            case Mode::Atlas: return {"An atlas packs several images into one texture so a whole scene needs one texture bind. The four", "tiles are copied into a 128 x 128 texture at run time with GetData and SetData, then drawn by source rectangle."};
            case Mode::Stress: return {"SpriteBatch batches sprites into a few big draw calls. Raise the count to see how many sprites per", "second this renderer can push; the frame rate on your screen is the museum's own, so go gently."};
            case Mode::Scissor: return {"With RasterizerState.ScissorTestEnable and GraphicsDevice.ScissorRectangle, drawing is clipped to a", "rectangle: how scrolling lists and UI panels hide what overflows. Slide the rectangle over a long list."};
            case Mode::Hit: return {"Two sprites collide when their opaque pixels touch, not when their rectangles do. The cheap test is Rectangle.Intersects", "on the bounding boxes; only if that passes does the exact test read both textures' pixels (GetData). Magenta shows the shared ones."};
            case Mode::Anim: return {"Animation is a SourceRectangle that changes over time: elapsed seconds times frames-per-second picks the frame. A clip", "is a range of frames; loop mode decides what happens at the end (wrap, bounce back or stop on the last frame)."};
            default: return {"The transformMatrix passed to SpriteBatch.Begin turns a 2D world into a screen: translation pans, scale zooms,", "rotation turns the view. The same sprites are drawn; only the matrix changes."};
        }
    }
    static std::string Fmt1(float v)
    {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.0f", static_cast<double>(v));
        return buf;
    }

    [[nodiscard]] Texture2D& Tex(const char* name) { return *S().textures->Get(name); }
    [[nodiscard]] Color TintColor() const
    {
        return Color(static_cast<std::uint8_t>(tint_[0]), static_cast<std::uint8_t>(tint_[1]), static_cast<std::uint8_t>(tint_[2]), static_cast<std::uint8_t>(tint_[3]));
    }
    void Text(SpriteBatch& sb, const std::string& t, float x, float y, Color c = Color::White, float scale = 0.75f)
    {
        sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
    }
    void Begin(SpriteBatch& sb, SpriteSortMode sort = SpriteSortMode::Deferred, const BlendState* blend = &BlendState::NonPremultiplied, const SamplerState* sampler = &SamplerState::LinearClamp,
               const RasterizerState* raster = nullptr, const Matrix& m = Matrix::getIdentityProperty())
    {
        sb.Begin(sort, blend, sampler, nullptr, raster, nullptr, m);
    }
    void Backdrop(SpriteBatch& sb)
    {
        Begin(sb, SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointWrap);
        sb.Draw(Tex("checker"), Rectangle(0, 0, kW, kH), Rectangle(0, 0, 480, 270), Color(80, 86, 106, 255));
        sb.End();
    }

    // ---------------------------------------------------------------------------------------------
    void DrawScreen(SpriteBatch& sb) override
    {
        switch (mode_) {
            case Mode::Basics: DrawBasics(sb); break;
            case Mode::Origin: DrawOrigin(sb); break;
            case Mode::Sheet: DrawSheet(sb); break;
            case Mode::Tint: DrawTint(sb); break;
            case Mode::Blend: DrawBlend(sb); break;
            case Mode::Sort: DrawSort(sb); break;
            case Mode::Sampler: DrawSampler(sb); break;
            case Mode::Parallax: DrawParallax(sb); break;
            case Mode::Ui: DrawUi(sb); break;
            case Mode::Atlas: DrawAtlas(sb); break;
            case Mode::Stress: DrawStress(sb); break;
            case Mode::Scissor: DrawScissor(sb); break;
            case Mode::Camera: DrawCamera(sb); break;
            case Mode::Hit: DrawHit(sb); break;
            case Mode::Anim: DrawAnim(sb); break;
        }
    }

    void DrawBasics(SpriteBatch& sb)
    {
        Backdrop(sb);
        Begin(sb);
        const Texture2D& logo = Tex("logo");
        const Vector2 origin = centre_ ? Vector2(128, 128) : Vector2(0, 0);
        SpriteEffects fx = SpriteEffects::None;
        if (flipH_ && flipV_) fx = static_cast<SpriteEffects>(3);
        else if (flipH_) fx = SpriteEffects::FlipHorizontally;
        else if (flipV_) fx = SpriteEffects::FlipVertically;
        sb.Draw(logo, Vector2(posX_, posY_), std::nullopt, Color::White, MathHelper::ToRadians(rot_), origin, scale_, fx, 0.0f);
        sb.Draw(Tex("white"), Rectangle(static_cast<int>(posX_) - 4, static_cast<int>(posY_) - 4, 8, 8), Color(255, 80, 60, 255));
        Text(sb, "sb.Draw(logo, position, null, White, rotation, origin, scale, effects, 0)", 16, 12);
        Text(sb, "origin = " + std::string(centre_ ? "(128, 128): the centre" : "(0, 0): top-left") + "   red dot = position", 16, 40, Color(255, 226, 150, 255), 0.65f);
        sb.End();
    }

    void DrawOrigin(SpriteBatch& sb)
    {
        Backdrop(sb);
        Begin(sb);
        const Texture2D& grid = Tex("uvgrid");
        const float angle = time_ * speed_;
        const Vector2 userOrigin(originX_ * 256.0f, originY_ * 256.0f);
        const struct {
            Vector2 origin;
            const char* label;
            float x;
        } cases[3] = {{Vector2(0, 0), "origin (0, 0)", 170}, {Vector2(128, 128), "origin (128, 128)", 480}, {userOrigin, "your origin", 790}};
        for (const auto& c : cases) {
            sb.Draw(grid, Vector2(c.x, 290), std::nullopt, Color(255, 255, 255, 235), angle, c.origin, 0.6f, SpriteEffects::None, 0.0f);
            sb.Draw(Tex("white"), Rectangle(static_cast<int>(c.x) - 5, 285, 10, 10), Color(255, 70, 50, 255));
            Text(sb, c.label, c.x - 80, 470, Color(255, 226, 150, 255), 0.7f);
        }
        sb.End();
    }

    void DrawSheet(SpriteBatch& sb)
    {
        Backdrop(sb);
        const SamplerState& smp = sampler_ == 0 ? SamplerState::PointClamp : SamplerState::LinearClamp;
        Begin(sb, SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &smp);
        const Texture2D& coin = Tex("coin");
        const int frame = static_cast<int>(time_ * fps_) % 4;
        const Rectangle src((frame % 2) * 8, (frame / 2) * 8, 8, 8);
        // The whole sheet, enlarged, with the active frame outlined.
        sb.Draw(coin, Rectangle(60, 120, 320, 320), Color::White);
        sb.Draw(Tex("white"), Rectangle(60 + src.X * 20, 120 + src.Y * 20, 160, 3), Color(255, 80, 60, 255));
        sb.Draw(Tex("white"), Rectangle(60 + src.X * 20, 120 + src.Y * 20 + 157, 160, 3), Color(255, 80, 60, 255));
        sb.Draw(Tex("white"), Rectangle(60 + src.X * 20, 120 + src.Y * 20, 3, 160), Color(255, 80, 60, 255));
        sb.Draw(Tex("white"), Rectangle(60 + src.X * 20 + 157, 120 + src.Y * 20, 3, 160), Color(255, 80, 60, 255));
        // The animated sprite.
        const SpriteEffects fx = flipH_ ? SpriteEffects::FlipHorizontally : SpriteEffects::None;
        if (useSource_) sb.Draw(coin, Rectangle(520, 120, 320, 320), src, Color::White, 0.0f, Vector2(0, 0), fx, 0.0f);
        else sb.Draw(coin, Rectangle(520, 120, 320, 320), std::nullopt, Color::White, 0.0f, Vector2(0, 0), fx, 0.0f);
        sb.End();
        Begin(sb);
        Text(sb, "sheet: 16 x 16, four 8 x 8 frames", 60, 80, Color(255, 226, 150, 255), 0.7f);
        Text(sb, useSource_ ? "sb.Draw(coin, dest, sourceRect, ...)" : "no source rectangle: the whole sheet", 520, 80, Color(255, 226, 150, 255), 0.7f);
        Text(sb, "source = (" + std::to_string(src.X) + ", " + std::to_string(src.Y) + ", 8, 8)", 520, 460, Color::White, 0.7f);
        sb.End();
    }

    void DrawTint(SpriteBatch& sb)
    {
        Backdrop(sb);
        Begin(sb);
        const Texture2D& logo = Tex("logo");
        const Color user = TintColor();
        const struct {
            Color c;
            const char* label;
        } rows[3] = {{Color::White, "Color.White"}, {user, "your tint"}, {Color::Lerp(Color::White, user, 0.5f), "Color.Lerp(White, tint, 0.5)"}};
        for (int i = 0; i < 3; ++i) {
            sb.Draw(logo, Rectangle(60 + i * 300, 130, 250, 250), rows[i].c);
            Text(sb, rows[i].label, static_cast<float>(60 + i * 300), 395, Color(255, 226, 150, 255), 0.65f);
        }
        Text(sb, "The Color argument multiplies every texel (and alpha blends with what is behind).", 60, 40, Color::White, 0.7f);
        sb.End();
    }

    void DrawBlend(SpriteBatch& sb)
    {
        struct Cfg {
            const char* name;
            BlendState state;
        };
        static const std::vector<Cfg> cfgs = [] {
            std::vector<Cfg> v;
            v.push_back({"Opaque", BlendState::Opaque});
            v.push_back({"AlphaBlend (premultiplied)", BlendState::AlphaBlend});
            v.push_back({"NonPremultiplied", BlendState::NonPremultiplied});
            v.push_back({"Additive", BlendState::Additive});
            BlendState mul;
            mul.setColorSourceBlendProperty(Blend::DestinationColor);
            mul.setColorDestinationBlendProperty(Blend::Zero);
            mul.setAlphaSourceBlendProperty(Blend::DestinationAlpha);
            mul.setAlphaDestinationBlendProperty(Blend::Zero);
            v.push_back({"Custom: Multiply", mul});
            BlendState sub;
            sub.setColorSourceBlendProperty(Blend::SourceAlpha);
            sub.setColorDestinationBlendProperty(Blend::One);
            sub.setColorBlendFunctionProperty(BlendFunction::ReverseSubtract);
            v.push_back({"Custom: ReverseSubtract", sub});
            BlendState mn;
            mn.setColorSourceBlendProperty(Blend::One);
            mn.setColorDestinationBlendProperty(Blend::One);
            mn.setColorBlendFunctionProperty(BlendFunction::Min);
            v.push_back({"BlendFunction.Min", mn});
            BlendState mx = mn;
            mx.setColorBlendFunctionProperty(BlendFunction::Max);
            v.push_back({"BlendFunction.Max", mx});
            return v;
        }();
        const Color tintA = coloured_ ? Color(255, 160, 90, 255) : Color::White;
        const Color tintB = coloured_ ? Color(110, 190, 255, 255) : Color::White;
        const std::uint8_t a = static_cast<std::uint8_t>(alpha_ * 255.0f);
        for (int i = 0; i < 8; ++i) {
            const int cx = 20 + (i % 4) * 235, cy = 40 + (i / 4) * 250;
            Begin(sb, SpriteSortMode::Deferred, &BlendState::Opaque);
            sb.Draw(Tex("gradient"), Rectangle(cx, cy, 220, 200), Color::White);
            sb.End();
            sb.Begin(SpriteSortMode::Deferred, &cfgs[static_cast<std::size_t>(i)].state, &SamplerState::LinearClamp, nullptr, nullptr);
            // Premultiplied blends expect colour already multiplied by alpha; the same colour values are used for all
            // eight so the differences come from the BlendState alone.
            sb.Draw(Tex("particle"), Rectangle(cx + 10, cy + 20, 150, 150), Color(tintA.getRProperty(), tintA.getGProperty(), tintA.getBProperty(), a));
            sb.Draw(Tex("logo"), Rectangle(cx + 60, cy + 40, 130, 130), Color(tintB.getRProperty(), tintB.getGProperty(), tintB.getBProperty(), a));
            sb.End();
            Begin(sb);
            Text(sb, cfgs[static_cast<std::size_t>(i)].name, static_cast<float>(cx), static_cast<float>(cy + 205), Color::White, 0.55f);
            sb.End();
        }
    }

    [[nodiscard]] std::string SortDescription() const
    {
        static const char* desc[] = {"submission order", "submission order (no batching)", "grouped by texture", "highest layerDepth first", "lowest layerDepth first"};
        return desc[sort_];
    }
    void DrawSort(SpriteBatch& sb)
    {
        Backdrop(sb);
        static const SpriteSortMode modes[] = {SpriteSortMode::Deferred, SpriteSortMode::Immediate, SpriteSortMode::Texture, SpriteSortMode::BackToFront, SpriteSortMode::FrontToBack};
        Begin(sb, modes[sort_]);
        // Submission order: red logo (layer 0.5), green grid (0.2), blue logo (user layer), orange grid (0.8), cyan logo (0.1).
        struct Item {
            const char* tex;
            Color c;
            float layer;
            int x, y;
        };
        const Item items[5] = {{"logo", Color(255, 120, 110, 255), 0.5f, 140, 150}, {"uvgrid", Color(130, 255, 140, 255), 0.2f, 230, 190},
                               {"logo", Color(120, 150, 255, 255), layer_, 320, 150}, {"uvgrid", Color(255, 190, 90, 255), 0.8f, 410, 190},
                               {"logo", Color(110, 240, 255, 255), 0.1f, 500, 150}};
        for (const Item& it : items) sb.Draw(Tex(it.tex), Rectangle(it.x, it.y, 220, 220), std::nullopt, it.c, 0.0f, Vector2(0, 0), SpriteEffects::None, it.layer);
        sb.End();
        Begin(sb);
        Text(sb, std::string("SpriteSortMode.") + std::vector<std::string>{"Deferred", "Immediate", "Texture", "BackToFront", "FrontToBack"}[static_cast<std::size_t>(sort_)], 40, 30, Color(255, 226, 150, 255), 0.9f);
        Text(sb, "submitted: red(0.5) green(0.2) BLUE(" + std::to_string(layer_).substr(0, 4) + ") orange(0.8) cyan(0.1)", 40, 80, Color::White, 0.65f);
        Text(sb, "drawn: " + SortDescription(), 40, 440, Color(150, 255, 170, 255), 0.7f);
        sb.End();
    }

    void DrawSampler(SpriteBatch& sb)
    {
        Backdrop(sb);
        SamplerState s = filter_ == 0 ? SamplerState::PointClamp : (filter_ == 1 ? SamplerState::LinearClamp : SamplerState::AnisotropicClamp);
        const TextureAddressMode mode = address_ == 0 ? TextureAddressMode::Wrap : (address_ == 1 ? TextureAddressMode::Clamp : TextureAddressMode::Mirror);
        s.setAddressUProperty(mode);
        s.setAddressVProperty(mode);
        Begin(sb, SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &s);
        // Left: a 16 x 16 coin sheet magnified. Right: the source rectangle reaches beyond the texture edges.
        const int size = static_cast<int>(16.0f * scale_ * 4.0f);
        sb.Draw(Tex("coin"), Rectangle(40, 120, size, size), Color::White);
        sb.Draw(Tex("coin"), Rectangle(500, 120, 400, 400), Rectangle(-16, -16, 48, 48), Color::White);
        sb.End();
        Begin(sb);
        Text(sb, "magnified coin sheet", 40, 80, Color(255, 226, 150, 255), 0.7f);
        Text(sb, "source rect (-16,-16, 48, 48) outside the texture", 440, 80, Color(255, 226, 150, 255), 0.7f);
        const char* f[] = {"Point", "Linear", "Anisotropic"};
        const char* a[] = {"Wrap", "Clamp", "Mirror"};
        Text(sb, std::string("filter ") + f[filter_] + ", address " + a[address_], 40, 490, Color::White, 0.75f);
        sb.End();
    }

    void DrawParallax(SpriteBatch& sb)
    {
        Begin(sb, SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearWrap);
        const float k[4] = {parallax_ ? 0.1f : 1.0f, parallax_ ? 0.3f : 1.0f, parallax_ ? 0.6f : 1.0f, 1.0f};
        const auto layer = [&](const char* tex, int y, int h, float speed, Color c, int srcScale) {
            const int off = static_cast<int>(scroll_ * speed);
            sb.Draw(Tex(tex), Rectangle(0, y, kW, h), Rectangle(off, 0, kW * srcScale / 2, h * srcScale / 2), c);
        };
        layer("clouds", 0, 300, k[0], Color(150, 190, 255, 255), 1);
        sb.End();
        Begin(sb, SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearWrap);
        layer("stone", 230, 130, k[1], Color(110, 120, 150, 255), 1);
        layer("brick", 300, 120, k[2], Color(150, 100, 90, 255), 1);
        layer("cobble", 400, 140, k[3], Color(200, 200, 190, 255), 1);
        sb.End();
        Begin(sb);
        Text(sb, parallax_ ? "four layers, four speeds: far = slow" : "parallax off: every layer scrolls at the same speed", 20, 14, Color::White, 0.75f);
        sb.End();
    }

    void BuildFrameTexture()
    {
        // A 24 x 24 framed panel: 6 px border, bevel and a lighter fill: a stand-in for a UI skin.
        std::vector<Color> px(24 * 24);
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 24; ++x) {
                const int d = std::min({x, y, 23 - x, 23 - y});
                Color c = d < 2 ? Color(230, 200, 120, 255) : (d < 6 ? Color(70, 90, 140, 255) : Color(36, 44, 70, 255));
                if (d >= 2 && d < 6 && ((x + y) % 4 == 0)) c = Color(90, 112, 170, 255);
                px[static_cast<std::size_t>(y * 24 + x)] = c;
            }
        frame_ = std::make_unique<Texture2D>(*S().device, 24, 24);
        frame_->SetData(px.data(), static_cast<int>(px.size()));
    }
    void DrawUi(SpriteBatch& sb)
    {
        Backdrop(sb);
        Begin(sb, SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::PointClamp);
        const int w = static_cast<int>(posX_), h = static_cast<int>(posY_);
        const auto nineSlice = [&](int x, int y) {
            const int b = 6, s = 3;   // 6-px border in the source, drawn three times larger
            const int bw = b * s;
            const Rectangle sx[3] = {Rectangle(0, 0, b, 24), Rectangle(b, 0, 12, 24), Rectangle(18, 0, b, 24)};
            (void)sx;
            const int sxs[3] = {0, b, 24 - b}, sws[3] = {b, 24 - 2 * b, b};
            const int dxs[3] = {x, x + bw, x + w - bw}, dws[3] = {bw, w - 2 * bw, bw};
            const int dys[3] = {y, y + bw, y + h - bw}, dhs[3] = {bw, h - 2 * bw, bw};
            for (int j = 0; j < 3; ++j)
                for (int i = 0; i < 3; ++i)
                    sb.Draw(*frame_, Rectangle(dxs[i], dys[j], std::max(1, dws[i]), std::max(1, dhs[j])), Rectangle(sxs[i], sxs[j], sws[i], sws[j]), Color::White);
        };
        if (frame_) {
            if (nine_) nineSlice(60, 70);
            else sb.Draw(*frame_, Rectangle(60, 70, w, h), Color::White);
            sb.Draw(*frame_, Rectangle(640, 70, 24 * 3, 24 * 3), Color::White);
        }
        sb.End();
        Begin(sb);
        Text(sb, nine_ ? "nine Draw calls: corners keep their size" : "one stretched Draw: the border distorts", 60, 30, Color(255, 226, 150, 255), 0.75f);
        Text(sb, "the source image (24 x 24), enlarged", 640, 30, Color(255, 226, 150, 255), 0.6f);
        sb.End();
    }

    void BuildAtlas()
    {
        const char* names[4] = {"brick", "stone", "marble", "wood"};
        std::vector<Color> atlas(128 * 128, Color(0, 0, 0, 255));
        for (int t = 0; t < 4; ++t) {
            Texture2D& src = Tex(names[t]);
            std::vector<Color> data(static_cast<std::size_t>(src.getWidthProperty()) * static_cast<std::size_t>(src.getHeightProperty()));
            src.GetData(data.data(), static_cast<int>(data.size()));
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x) {
                    const int sx = x * src.getWidthProperty() / 64, sy = y * src.getHeightProperty() / 64;
                    atlas[static_cast<std::size_t>((y + (t / 2) * 64) * 128 + x + (t % 2) * 64)] = data[static_cast<std::size_t>(sy * src.getWidthProperty() + sx)];
                }
        }
        atlas_ = std::make_unique<Texture2D>(*S().device, 128, 128);
        atlas_->SetData(atlas.data(), static_cast<int>(atlas.size()));
    }
    void DrawAtlas(SpriteBatch& sb)
    {
        Backdrop(sb);
        Begin(sb, SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp);
        const char* names[4] = {"brick", "stone", "marble", "wood"};
        if (atlas_) sb.Draw(*atlas_, Rectangle(40, 100, 320, 320), Color::White);
        for (int t = 0; t < 4; ++t) {
            const Rectangle dest(440 + (t % 2) * 220, 100 + (t / 2) * 220, 200, 200);
            if (useAtlas_ && atlas_) sb.Draw(*atlas_, dest, Rectangle((t % 2) * 64, (t / 2) * 64, 64, 64), Color::White);
            else sb.Draw(Tex(names[t]), dest, Color::White);
        }
        sb.End();
        Begin(sb);
        Text(sb, "the atlas, built at run time with GetData / SetData", 40, 60, Color(255, 226, 150, 255), 0.65f);
        Text(sb, useAtlas_ ? "four sprites, one texture: source rectangles" : "four sprites, four textures", 440, 60, Color(255, 226, 150, 255), 0.65f);
        sb.End();
    }

    struct Bouncer {
        float x, y, vx, vy;
        int frame;
    };
    void EnsureSprites()
    {
        std::mt19937 rng(7);
        std::uniform_real_distribution<float> px(0.0f, static_cast<float>(kW - 8)), py(0.0f, static_cast<float>(kH - 8)), pv(-90.0f, 90.0f);
        while (static_cast<int>(sprites_.size()) < count_) sprites_.push_back({px(rng), py(rng), pv(rng), pv(rng), static_cast<int>(rng() % 4)});
        if (static_cast<int>(sprites_.size()) > count_) sprites_.resize(static_cast<std::size_t>(count_));
    }
    void DrawStress(SpriteBatch& sb)
    {
        Begin(sb, SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::PointClamp);
        const Texture2D& coin = Tex("coin");
        for (const Bouncer& s : sprites_) {
            const int f = (s.frame + static_cast<int>(time_ * 6.0f)) % 4;
            sb.Draw(coin, Rectangle(static_cast<int>(s.x), static_cast<int>(s.y), 16, 16), Rectangle((f % 2) * 8, (f / 2) * 8, 8, 8), Color::White);
        }
        sb.End();
        Begin(sb);
        Text(sb, std::to_string(count_) + " sprites in one SpriteBatch.Begin/End", 14, 8, Color::White, 0.9f);
        sb.End();
    }

    void DrawScissor(SpriteBatch& sb)
    {
        Backdrop(sb);
        GraphicsDevice& device = *S().device;
        static RasterizerState scissorState = [] {
            RasterizerState r;
            r.setScissorTestEnableProperty(true);
            return r;
        }();
        const Rectangle rect(static_cast<int>(posX_) + 60, 120, static_cast<int>(posY_ + 100.0f), 300);
        device.setScissorRectangleProperty(scissor_ ? rect : Rectangle(0, 0, kW, kH));
        Begin(sb, SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, scissor_ ? &scissorState : nullptr);
        for (int i = 0; i < 12; ++i) {
            const int y = 100 + i * 36 - static_cast<int>(std::fmod(time_ * 30.0f, 36.0f));
            sb.Draw(Tex("white"), Rectangle(60, y, 840, 30), (i % 2) ? Color(90, 110, 170, 255) : Color(60, 80, 130, 255));
            sb.DrawString(*S().font, "list item " + std::to_string(i + 1) + " (this row is wider than the scissor rectangle and is clipped)", Vector2(70.0f, static_cast<float>(y) + 2.0f), Color::White, 0.0f, Vector2(0, 0), 0.62f, SpriteEffects::None, 0.0f);
        }
        sb.End();
        device.setScissorRectangleProperty(Rectangle(0, 0, kW, kH));
        Begin(sb, SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp);
        if (scissor_) {
            sb.Draw(Tex("white"), Rectangle(rect.X, rect.Y - 3, rect.Width, 3), Color(255, 90, 60, 255));
            sb.Draw(Tex("white"), Rectangle(rect.X, rect.Y + rect.Height, rect.Width, 3), Color(255, 90, 60, 255));
        }
        Text(sb, scissor_ ? "ScissorTestEnable = true: clipped to the red lines" : "ScissorTestEnable = false: nothing is clipped", 60, 30, Color(255, 226, 150, 255), 0.75f);
        sb.End();
    }

    void DrawCamera(SpriteBatch& sb)
    {
        Begin(sb, SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointWrap);
        sb.Draw(Tex("checker"), Rectangle(0, 0, kW, kH), Rectangle(0, 0, 1, 1), Color(20, 24, 34, 255));
        sb.End();
        const Vector2 player(480.0f + std::cos(time_ * 0.7f) * 260.0f, 270.0f + std::sin(time_ * 1.1f) * 140.0f);
        const Vector2 focus = follow_ ? player : Vector2(posX_, 270.0f);
        const Matrix m = Matrix::CreateTranslation(-focus.X, -focus.Y, 0.0f) * Matrix::CreateRotationZ(MathHelper::ToRadians(rot_)) * Matrix::CreateScale(scale_, scale_, 1.0f) *
                         Matrix::CreateTranslation(480.0f, 270.0f, 0.0f);
        Begin(sb, SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::PointClamp, nullptr, m);
        for (int y = 0; y < 18; ++y)
            for (int x = 0; x < 30; ++x) sb.Draw(Tex("white"), Rectangle(x * 64 + 2, y * 64 + 2, 60, 60), ((x + y) % 2) ? Color(60, 80, 120, 255) : Color(80, 100, 150, 255));
        sb.Draw(Tex("logo"), Rectangle(420, 120, 128, 128), Color::White);
        sb.Draw(Tex("white"), Rectangle(static_cast<int>(player.X) - 8, static_cast<int>(player.Y) - 8, 16, 16), Color(255, 90, 60, 255));
        sb.End();
        Begin(sb);
        Text(sb, "Begin(..., transformMatrix): translate * rotate * scale * translate(screen centre)", 14, 10, Color::White, 0.65f);
        sb.End();
    }

    // --- per-pixel collision -------------------------------------------------------------------
    static constexpr int kHitSize = 96;
    void BuildHitTextures()
    {
        GraphicsDevice& d = *S().device;
        std::vector<Color> a(kHitSize * kHitSize, Color(0, 0, 0, 0)), b(kHitSize * kHitSize, Color(0, 0, 0, 0));
        for (int y = 0; y < kHitSize; ++y)
            for (int x = 0; x < kHitSize; ++x) {
                const float dx = static_cast<float>(x) - 47.5f, dy = static_cast<float>(y) - 47.5f;
                const float r = std::sqrt(dx * dx + dy * dy), ang = std::atan2(dy, dx);
                // A: a five-pointed star. B: a thin ring with a gap (a crescent).
                const float star = 46.0f * (0.45f + 0.55f * std::pow(std::abs(std::cos(2.5f * (ang + 1.5708f))), 0.7f));
                if (r < star) a[static_cast<std::size_t>(y * kHitSize + x)] = Color(255, 210, 90, 255);
                const bool ring = r > 30.0f && r < 45.0f && !(ang > -0.5f && ang < 0.5f);
                if (ring) b[static_cast<std::size_t>(y * kHitSize + x)] = Color(90, 190, 255, 255);
            }
        hitA_ = std::make_unique<Texture2D>(d, kHitSize, kHitSize);
        hitB_ = std::make_unique<Texture2D>(d, kHitSize, kHitSize);
        hitA_->SetData(a.data(), kHitSize * kHitSize);
        hitB_->SetData(b.data(), kHitSize * kHitSize);
        // The CPU copy used by the exact test: read back through GetData, as a game would after loading its textures.
        maskA_.assign(a.size(), 0);
        maskB_.assign(b.size(), 0);
        std::vector<Color> back(a.size());
        hitA_->GetData(back.data(), kHitSize * kHitSize);
        for (std::size_t i = 0; i < back.size(); ++i) maskA_[i] = back[i].getAProperty() > 0;
        hitB_->GetData(back.data(), kHitSize * kHitSize);
        for (std::size_t i = 0; i < back.size(); ++i) maskB_[i] = back[i].getAProperty() > 0;
    }

    // The bounding rectangle of a sprite drawn at `pos` with origin at its centre, rotated and scaled.
    static Rectangle SpriteBounds(const Vector2& pos, float rotation, float scale)
    {
        const Matrix m = Matrix::CreateTranslation(-48.0f, -48.0f, 0.0f) * Matrix::CreateScale(scale, scale, 1.0f) * Matrix::CreateRotationZ(rotation) * Matrix::CreateTranslation(pos.X, pos.Y, 0.0f);
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (const Vector2 c : {Vector2(0, 0), Vector2(96, 0), Vector2(0, 96), Vector2(96, 96)}) {
            const Vector2 p = Vector2::Transform(c, m);
            x0 = std::min(x0, p.X); y0 = std::min(y0, p.Y); x1 = std::max(x1, p.X); y1 = std::max(y1, p.Y);
        }
        return Rectangle(static_cast<int>(std::floor(x0)), static_cast<int>(std::floor(y0)), static_cast<int>(std::ceil(x1 - x0)), static_cast<int>(std::ceil(y1 - y0)));
    }

    void DrawHit(SpriteBatch& sb)
    {
        Backdrop(sb);
        if (!hitA_ || !hitB_) return;
        const Vector2 posA(300.0f, 270.0f), posB(hitX_, hitY_);
        const float sa = 2.2f, sbScale = scale_ * 2.0f, rb = MathHelper::ToRadians(rot_);
        const Rectangle ra = SpriteBounds(posA, 0.0f, sa), rbRect = SpriteBounds(posB, rb, sbScale);
        rectHit_ = ra.Intersects(rbRect);
        hitPixels_.clear();
        pixelHits_ = 0;
        pixelsTested_ = 0;
        const auto t0 = std::chrono::steady_clock::now();
        if (rectHit_) {
            const Rectangle both = Rectangle::Intersect(ra, rbRect);
            const float cb = std::cos(-rb), sbn = std::sin(-rb);
            for (int y = both.Y; y < both.Y + both.Height; ++y)
                for (int x = both.X; x < both.X + both.Width; ++x) {
                    ++pixelsTested_;
                    const float px = static_cast<float>(x) + 0.5f, py = static_cast<float>(y) + 0.5f;
                    const int ax = static_cast<int>(std::floor((px - posA.X) / sa + 48.0f)), ay = static_cast<int>(std::floor((py - posA.Y) / sa + 48.0f));
                    if (ax < 0 || ay < 0 || ax >= kHitSize || ay >= kHitSize || !maskA_[static_cast<std::size_t>(ay * kHitSize + ax)]) continue;
                    const float vx = px - posB.X, vy = py - posB.Y;   // into B's local space: undo translation, rotation and scale
                    const int bx = static_cast<int>(std::floor((vx * cb - vy * sbn) / sbScale + 48.0f)), by = static_cast<int>(std::floor((vx * sbn + vy * cb) / sbScale + 48.0f));
                    if (bx < 0 || by < 0 || bx >= kHitSize || by >= kHitSize || !maskB_[static_cast<std::size_t>(by * kHitSize + bx)]) continue;
                    ++pixelHits_;
                    hitPixels_.emplace_back(x, y);
                }
        }
        testMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        Begin(sb, SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::PointClamp);
        sb.Draw(*hitA_, posA, std::nullopt, Color::White, 0.0f, Vector2(48, 48), sa, SpriteEffects::None, 0.0f);
        sb.Draw(*hitB_, posB, std::nullopt, Color(255, 255, 255, 230), rb, Vector2(48, 48), sbScale, SpriteEffects::None, 0.0f);
        const Color box = rectHit_ ? Color(255, 120, 100, 255) : Color(110, 230, 150, 255);
        for (const Rectangle& r : {ra, rbRect}) {
            sb.Draw(*S().whiteTexture, Rectangle(r.X, r.Y, r.Width, 2), box);
            sb.Draw(*S().whiteTexture, Rectangle(r.X, r.Y + r.Height - 2, r.Width, 2), box);
            sb.Draw(*S().whiteTexture, Rectangle(r.X, r.Y, 2, r.Height), box);
            sb.Draw(*S().whiteTexture, Rectangle(r.X + r.Width - 2, r.Y, 2, r.Height), box);
        }
        for (const auto& [x, y] : hitPixels_) sb.Draw(*S().whiteTexture, Rectangle(x, y, 1, 1), Color(255, 40, 220, 255));
        sb.End();
        Begin(sb);
        Text(sb, std::string("rectangles: ") + (rectHit_ ? "overlap" : "apart") + "      pixels: " + (pixelHits_ > 0 ? "COLLISION (" + std::to_string(pixelHits_) + " shared)" : "no collision"), 20, 10,
             pixelHits_ > 0 ? Color(255, 120, 200, 255) : (rectHit_ ? Color(255, 200, 100, 255) : Color(130, 235, 165, 255)), 0.75f);
        Text(sb, rectHit_ && pixelHits_ == 0 ? "the bounding boxes overlap, but the sprites do not touch: the case the exact test exists for" : "boxes: Rectangle.Intersects   magenta: pixels opaque in both sprites", 20, 504, Color(150, 160, 190, 255), 0.6f);
        sb.End();
    }

    // --- sprite animation ----------------------------------------------------------------------
    static constexpr int kFrame = 48, kFrames = 8;
    void BuildAnimSheet()
    {
        std::vector<Color> px(static_cast<std::size_t>(kFrame * kFrame * kFrames), Color(0, 0, 0, 0));
        const auto put = [&](int f, int x, int y, int w, int h, Color c) {
            for (int yy = y; yy < y + h; ++yy)
                for (int xx = x; xx < x + w; ++xx)
                    if (xx >= 0 && xx < kFrame && yy >= 0 && yy < kFrame) px[static_cast<std::size_t>(yy * kFrame * kFrames + f * kFrame + xx)] = c;
        };
        const Color body(120, 190, 255, 255), dark(60, 100, 170, 255), eye(255, 240, 160, 255), limb(200, 210, 230, 255);
        // 0-1 idle (a breath), 2-5 walk (legs swing), 6-7 jump (crouch, then legs tucked).
        const int bob[8] = {0, 2, 0, 1, 0, 1, 3, -4};
        const int legA[8] = {0, 0, -5, 0, 5, 0, 0, 0}, legB[8] = {0, 0, 5, 0, -5, 0, 0, 0};
        for (int f = 0; f < kFrames; ++f) {
            const int top = 10 - bob[f] + (f == 1 ? 1 : 0);
            put(f, 14, top + 22, 5, 14 - (f >= 6 ? 6 : 0), limb);           // back leg
            put(f, 14 + legA[f], top + 22, 5, 14 - (f >= 6 ? 6 : 0), limb);
            put(f, 29 + legB[f], top + 22, 5, 14 - (f >= 6 ? 6 : 0), limb);
            put(f, 12, top + 4, 24, 20, body);                                  // torso
            put(f, 12, top + 20, 24, 4, dark);
            put(f, 17, top + 9, 4, 4, eye);
            put(f, 28, top + 9, 4, 4, eye);
            put(f, 22, top - 3, 4, 7, dark);                                    // antenna
            put(f, 21, top - 6, 6, 3, Color(255, 120, 120, 255));
            put(f, 8, top + 8 + (f >= 6 ? -6 : 0), 4, 10, limb);                // arms
            put(f, 36, top + 8 + (f >= 6 ? -6 : 0), 4, 10, limb);
        }
        sheet_ = std::make_unique<Texture2D>(*S().device, kFrame * kFrames, kFrame);
        sheet_->SetData(px.data(), kFrame * kFrame * kFrames);
    }

    [[nodiscard]] int ClipStart() const { static const int s[] = {0, 2, 6, 0}; return s[clip_]; }
    [[nodiscard]] int ClipLength() const { static const int n[] = {2, 4, 2, 8}; return n[clip_]; }
    // Which frame of the clip is showing at animTime_: elapsed seconds * fps, wrapped, bounced or clamped by the loop mode.
    [[nodiscard]] int ClipFrame() const
    {
        const int n = ClipLength();
        const int tick = static_cast<int>(animTime_ * fps_);
        if (loop_ == 0) return tick % n;
        if (loop_ == 2) return std::min(tick, n - 1);
        if (n == 1) return 0;
        const int cycle = 2 * (n - 1), k = tick % cycle;
        return k < n ? k : cycle - k;
    }
    [[nodiscard]] std::string AnimDescription() const
    {
        const int f = ClipStart() + ClipFrame();
        return std::to_string(f) + " of 0-7 / (" + std::to_string(f * kFrame) + ", 0, " + std::to_string(kFrame) + ", " + std::to_string(kFrame) + ")";
    }

    void DrawAnim(SpriteBatch& sb)
    {
        Backdrop(sb);
        if (!sheet_) return;
        const int f = ClipStart() + ClipFrame();
        Begin(sb, SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::PointClamp);
        // The whole sheet as a filmstrip, the current source rectangle outlined.
        const int cell = 108;
        for (int i = 0; i < kFrames; ++i) {
            const Rectangle dest(40 + i * (cell + 6), 40, cell, cell);
            sb.Draw(*S().whiteTexture, dest, Color(22, 28, 44, 230));
            sb.Draw(*sheet_, dest, Rectangle(i * kFrame, 0, kFrame, kFrame), Color::White);
            const bool inClip = i >= ClipStart() && i < ClipStart() + ClipLength();
            const Color edge = i == f ? Color(255, 226, 120, 255) : (inClip ? Color(110, 200, 150, 255) : Color(70, 80, 110, 255));
            const int t = i == f ? 4 : 2;
            sb.Draw(*S().whiteTexture, Rectangle(dest.X, dest.Y, cell, t), edge);
            sb.Draw(*S().whiteTexture, Rectangle(dest.X, dest.Y + cell - t, cell, t), edge);
            sb.Draw(*S().whiteTexture, Rectangle(dest.X, dest.Y, t, cell), edge);
            sb.Draw(*S().whiteTexture, Rectangle(dest.X + cell - t, dest.Y, t, cell), edge);
        }
        // The animated sprite on a floor, drawn with the current frame as its SourceRectangle.
        sb.Draw(*S().whiteTexture, Rectangle(0, 440, kW, 100), Color(40, 52, 78, 255));
        const float x = (travel_ && clip_ != 0) ? walkX_ : 480.0f;
        const float y = clip_ == 2 ? 440.0f - 70.0f * std::abs(std::sin(animTime_ * fps_ * 0.35f)) : 440.0f;
        sb.Draw(*sheet_, Vector2(x, y), Rectangle(f * kFrame, 0, kFrame, kFrame), Color::White, 0.0f, Vector2(24, 44), 4.0f,
                walkDir_ < 0.0f && travel_ && clip_ != 0 ? SpriteEffects::FlipHorizontally : SpriteEffects::None, 0.0f);
        sb.End();
        Begin(sb);
        Text(sb, "frame " + std::to_string(f) + " = SourceRectangle(" + std::to_string(f * kFrame) + ", 0, 48, 48)    t = " + Fmt1(animTime_ * 10.0f) + " / 10 s    tick = " + std::to_string(static_cast<int>(animTime_ * fps_)), 20, 170, Color(255, 226, 150, 255), 0.65f);
        Text(sb, "frame = start + (floor(seconds * fps) mod length);  ping-pong bounces, play-once clamps;  FlipHorizontally when walking left", 20, 196, Color(150, 160, 190, 255), 0.55f);
        sb.End();
    }

    Mode mode_;
    std::unique_ptr<Texture2D> hitA_, hitB_, sheet_;
    std::vector<unsigned char> maskA_, maskB_;
    std::vector<std::pair<int, int>> hitPixels_;
    float hitX_ = 420.0f, hitY_ = 270.0f, animTime_ = 0.0f, walkX_ = 300.0f, walkDir_ = 1.0f, testMs_ = 0.0f;
    bool rectHit_ = false, auto_ = true, playing_ = true, travel_ = true;
    int pixelHits_ = 0, pixelsTested_ = 0, clip_ = 1, loop_ = 0;
    std::unique_ptr<Texture2D> frame_, atlas_;
    std::vector<Bouncer> sprites_;
    float time_ = 0.0f, scroll_ = 0.0f;
    float posX_ = 480.0f, posY_ = 270.0f, rot_ = 20.0f, scale_ = 0.8f, speed_ = 1.0f;
    float originX_ = 0.2f, originY_ = 0.8f, fps_ = 6.0f, alpha_ = 0.75f;
    float tint_[4] = {255, 150, 80, 200};
    float layer_ = 0.35f;
    bool flipH_ = false, flipV_ = false, centre_ = true, useSource_ = true, coloured_ = true, parallax_ = true, nine_ = true, useAtlas_ = true, scissor_ = true, follow_ = false;
    int sampler_ = 0, sort_ = 0, filter_ = 0, address_ = 0, count_ = 2000;
};

}  // namespace

std::unique_ptr<Exhibit> MakeSpriteLab(const RoomDef& room, Services& services)
{
    return std::make_unique<SpriteLab>(room, services);
}
