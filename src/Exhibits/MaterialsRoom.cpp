#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "Audio/AudioSystem.hpp"
#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureAddressMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureFilter.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/ImageUtil.hpp"
#include "Rendering/LiveScreen.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/SignFactory.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const Color kGold(255, 226, 150, 255);
const Color kText(222, 228, 240, 255);

struct MaterialSpec {
    const char* name;
    const char* detail;
    Vector3 diffuse;
    Vector3 specular;
    float power;
    Vector3 emissive;
    const char* texture;   // null = plain colour
};

const MaterialSpec kMaterials[] = {
    {"Matte", "DiffuseColor only", {0.85f, 0.26f, 0.22f}, {0, 0, 0}, 1.0f, {0, 0, 0}, nullptr},
    {"Glossy", "SpecularPower 80", {0.20f, 0.36f, 0.92f}, {1, 1, 1}, 80.0f, {0, 0, 0}, nullptr},
    {"Satin metal", "SpecularPower 14", {0.66f, 0.68f, 0.74f}, {0.8f, 0.82f, 0.9f}, 14.0f, {0, 0, 0}, nullptr},
    {"Emissive", "EmissiveColor", {0.08f, 0.22f, 0.10f}, {0, 0, 0}, 1.0f, {0.10f, 1.0f, 0.35f}, nullptr},
    {"Wood", "Texture2D + diffuse", {1, 1, 1}, {0.25f, 0.25f, 0.25f}, 20.0f, {0, 0, 0}, "wood"},
    {"Marble", "Texture2D + specular", {1, 1, 1}, {0.9f, 0.9f, 0.95f}, 48.0f, {0, 0, 0}, "marble"},
};
constexpr int kMaterialCount = static_cast<int>(sizeof(kMaterials) / sizeof(kMaterials[0]));

struct FilterMode {
    const char* name;
    const char* detail;
    TextureFilter filter;
};

const FilterMode kFilters[] = {
    {"Point", "TextureFilter.Point: nearest texel, nearest mip. Shimmers and crawls.", TextureFilter::Point},
    {"Bilinear, mip bands", "LinearMipPoint: smooth inside a level, hard steps between mip colours.", TextureFilter::LinearMipPoint},
    {"Trilinear", "Linear: blends neighbouring mip levels, so the colours melt into each other.", TextureFilter::Linear},
    {"Anisotropic 16x", "Anisotropic: keeps detail along the strip, so it stays sharp towards the horizon.", TextureFilter::Anisotropic},
};
constexpr int kFilterCount = static_cast<int>(sizeof(kFilters) / sizeof(kFilters[0]));

struct AddressMode {
    const char* name;
    TextureAddressMode mode;
};
const AddressMode kAddress[] = {
    {"Wrap", TextureAddressMode::Wrap},
    {"Clamp", TextureAddressMode::Clamp},
    {"Mirror", TextureAddressMode::Mirror},
};

const char* const kLibrary[9] = {"brick", "stone", "marble", "wood", "tiles", "carpet", "metal", "cobble", "grass"};

// Lectern, strip, panels and shelf positions (room-local metres).
constexpr float kStripX = -4.6f;
constexpr float kStripHalfWidth = 1.4f;
constexpr float kStripNorth = -5.2f;
constexpr float kStripSouth = 4.6f;
constexpr float kLecternX = -2.3f;   // beside the strip's near end, clear of the south door lane
constexpr float kLecternZ = 4.3f;
constexpr float kShelfY = 0.9f;

void DrawUvQuad(const DrawContext& ctx, Texture2D* texture, const Vector3& origin, const Vector3& U, const Vector3& V,
                const Vector2& uvMin, const Vector2& uvMax)
{
    // origin is the bottom-left corner; texture V grows downward, so uvMax.Y belongs to the bottom edge.
    const Color c = Color::White;
    const Vector3 a = origin, b = origin + U, d = origin + U + V, e = origin + V;
    const VertexPositionColorTexture verts[6] = {
        {a, c, Vector2(uvMin.X, uvMax.Y)}, {b, c, Vector2(uvMax.X, uvMax.Y)}, {d, c, Vector2(uvMax.X, uvMin.Y)},
        {a, c, Vector2(uvMin.X, uvMax.Y)}, {d, c, Vector2(uvMax.X, uvMin.Y)}, {e, c, Vector2(uvMin.X, uvMin.Y)},
    };
    BasicEffect& fx = ctx.fx.World();
    fx.setTextureProperty(texture);
    fx.setDiffuseColorProperty(Vector3(1, 1, 1));
    fx.setWorldProperty(Matrix::getIdentityProperty());
    fx.setViewProperty(ctx.view.view);
    fx.setProjectionProperty(ctx.view.projection);
    EffectBank::Apply(fx);
    ctx.device.DrawUserPrimitives(PrimitiveType::TriangleList, verts, 0, 2);
}

class MaterialsRoom final : public Exhibit {
public:
    MaterialsRoom(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        // --- Material shelf, north wall, east of the door -------------------------------------------
        b.AddBox(Vector3(2.4f, 0.0f, -6.95f), Vector3(6.9f, kShelfY, -6.05f), "wood", 1.0f, Vector3(0.7f, 0.58f, 0.48f));
        for (int i = 0; i < kMaterialCount; ++i) {
            const float x = ShelfX(i);
            SignSpec p;
            p.lines = {{kMaterials[i].name, true, kGold}, {kMaterials[i].detail, false, kText}};
            p.background = Color(26, 24, 30, 255);
            b.AddQuadFacing(Vector3(x, 0.45f, -6.04f), 0.0f, 0.66f, 0.3f, S().signs->Create(MakePlaque(p, 0.66f, 0.3f)), Vector3(1, 1, 1), 0.55f);
        }
        SignSpec shelfTitle;
        shelfTitle.lines = {{"BasicEffect materials", true, kGold},
                            {"DiffuseColor, SpecularColor/Power, EmissiveColor and Texture", false, kText}};
        b.AddWallSign(Wall::North, 4.65f, 3.3f, 4.6f, 1.0f, shelfTitle);

        // --- Address-mode frames, north wall, west of the door ----------------------------------------
        for (int i = 0; i < 3; ++i) {
            const float x = PanelX(i);
            b.AddBox(Vector3(x - 0.72f, 1.38f, -6.99f), Vector3(x + 0.72f, 2.82f, -6.93f), "wood", 1.0f, Vector3(0.45f, 0.32f, 0.22f), false);
            SignSpec p;
            p.lines = {{std::string("TextureAddressMode.") + kAddress[i].name, false, kGold}};
            p.background = Color(26, 24, 30, 255);
            b.AddQuadFacing(Vector3(x, 1.15f, -6.94f), 0.0f, 1.44f, 0.24f, S().signs->Create(MakePlaque(p, 1.44f, 0.24f)), Vector3(1, 1, 1), 0.55f);
        }
        SignSpec panelTitle;
        panelTitle.lines = {{"Addressing", true, kGold}, {"The same texture, UVs from -0.5 to 1.5: what lies outside 0..1?", false, kText}};
        b.AddWallSign(Wall::North, -4.1f, 3.5f, 4.4f, 0.9f, panelTitle);

        // --- Texture library, east wall, north part ------------------------------------------------------
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                const float z = -5.5f + static_cast<float>(col) * 1.25f;
                const float y = 1.55f + static_cast<float>(row) * 1.2f;
                b.AddBox(Vector3(6.96f, y - 0.56f, z - 0.56f), Vector3(7.0f, y + 0.56f, z + 0.56f), "wood", 1.0f, Vector3(0.45f, 0.32f, 0.22f), false);
                b.AddQuadFacing(Vector3(6.955f, y, z), -MathHelper::PiOver2, 1.0f, 1.0f, kLibrary[row * 3 + col], Vector3(1, 1, 1), 0.6f);
            }
        }
        SignSpec lib;
        lib.lines = {{"Content/Textures", true, kGold}, {"brick stone marble / wood tiles carpet / metal cobble grass", false, kText}};
        b.AddWallSign(Wall::East, -3.0f, 0.78f, 3.9f, 0.62f, lib);

        // --- Mip strip lectern ---------------------------------------------------------------------------
        b.AddBox(Vector3(kLecternX - 0.55f, 0.0f, kLecternZ - 0.28f), Vector3(kLecternX + 0.55f, 1.05f, kLecternZ + 0.28f), "metal", 1.0f,
                 Vector3(0.55f, 0.58f, 0.7f));
        SignSpec stripTitle;
        stripTitle.lines = {{"Mipmaps and filtering", true, kGold},
                            {"Every mip level of this texture has its own colour.", false, kText},
                            {"Look along the strip and use the lectern to change the filter.", false, kText}};
        b.AddWallSign(Wall::West, 1.2f, 3.3f, 4.4f, 1.15f, stripTitle);

        // --- UV pedestals ---------------------------------------------------------------------------------
        b.AddPedestal(4.8f, 3.0f, 0.9f, 0.9f, "stone", Vector3(0.9f, 0.9f, 0.95f));
        b.AddPedestal(4.8f, 5.4f, 0.9f, 0.9f, "stone", Vector3(0.9f, 0.9f, 0.95f));
        SignSpec uv1;
        uv1.lines = {{"UV mapping: cube", true, kGold}, {"Each face maps the whole 0..1 square", false, kText}};
        uv1.background = Color(26, 24, 30, 255);
        b.AddPedestalPlaque(4.8f, 3.0f, 0.9f, 0.45f, uv1, 0.86f, 0.3f, -MathHelper::PiOver2);
        SignSpec uv2;
        uv2.lines = {{"UV mapping: sphere", true, kGold}, {"U wraps around, V runs pole to pole", false, kText}};
        uv2.background = Color(26, 24, 30, 255);
        b.AddPedestalPlaque(4.8f, 5.4f, 0.9f, 0.45f, uv2, 0.86f, 0.3f, -MathHelper::PiOver2);
        b.AddLight(Vector3(-4.0f, 4.2f, 2.0f), Vector3(1.0f, 0.97f, 0.9f), 0.7f, 9.0f);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        mipTexture_ = BuildMipTexture();
        lecternScreen_ = std::make_unique<LiveScreen>(S(), 512, 192);

        Interactable shelf;
        shelf.room = R().id;
        shelf.bounds = BoundingBox(R().ToWorld(2.2f, 0.0f, -7.0f), R().ToWorld(7.0f, 1.5f, -5.9f));
        shelf.prompt = [this] { return std::string(perPixel_ ? "Switch the spheres to per-vertex lighting" : "Switch the spheres to per-pixel lighting"); };
        shelf.activate = [this] { perPixel_ = !perPixel_; };
        interaction.Add(std::move(shelf));

        Interactable panels;
        panels.room = R().id;
        panels.bounds = BoundingBox(R().ToWorld(-6.5f, 1.2f, -7.0f), R().ToWorld(-1.9f, 3.0f, -6.6f));
        panels.prompt = [this] { return std::string("Change how far the UVs reach (") + std::to_string(kRanges[rangeIndex_]).substr(0, 3) + ")"; };
        panels.activate = [this] { rangeIndex_ = (rangeIndex_ + 1) % kRangeCount; };
        interaction.Add(std::move(panels));

        Interactable lectern;
        lectern.room = R().id;
        lectern.bounds = BoundingBox(R().ToWorld(kLecternX - 0.75f, 0.0f, kLecternZ - 0.5f), R().ToWorld(kLecternX + 0.75f, 1.6f, kLecternZ + 0.5f));
        lectern.prompt = [this] { return std::string("Next texture filter (") + kFilters[filter_].name + ")"; };
        lectern.activate = [this] { filter_ = (filter_ + 1) % kFilterCount; screenDirty_ = true; };
        interaction.Add(std::move(lectern));

        for (int i = 0; i < 2; ++i) {
            Interactable uv;
            uv.room = R().id;
            const float z = i == 0 ? 3.0f : 5.4f;
            uv.bounds = BoundingBox(R().ToWorld(4.3f, 0.0f, z - 0.5f), R().ToWorld(5.3f, 1.8f, z + 0.5f));
            uv.prompt = [this] { return std::string(useGrid_ ? "Swap the UV grid for a checker" : "Swap the checker for the UV grid"); };
            uv.activate = [this] { useGrid_ = !useGrid_; };
            interaction.Add(std::move(uv));
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        angle_ += ctx.dt * 0.6f;
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (ctx.offscreen || lecternScreen_ == nullptr || S().font == nullptr) return;
        if (!lecternScreen_->HasContent() || screenDirty_) {
            screenDirty_ = false;
            lecternScreen_->Render(Color(16, 20, 32, 255), [this](SpriteBatch& sb, int w, int h) {
                Texture2D& white = *S().textures->White();
                sb.Draw(white, Rectangle(0, 0, w, 4), Color(200, 170, 90, 255));
                sb.Draw(white, Rectangle(0, h - 4, w, 4), Color(200, 170, 90, 255));
                sb.DrawString(*S().font, "Filter", Vector2(16, 10), Color(150, 162, 186, 255), 0.0f, Vector2(0, 0), 0.9f, SpriteEffects::None, 0.0f);
                sb.DrawString(*S().titleFont, kFilters[filter_].name, Vector2(16, 34), kGold, 0.0f, Vector2(0, 0), 0.85f, SpriteEffects::None, 0.0f);
                // Wrap the description at about 44 characters.
                std::string text = kFilters[filter_].detail;
                float y = 100.0f;
                while (!text.empty()) {
                    std::size_t cut = text.size() <= 44 ? text.size() : text.rfind(' ', 44);
                    if (cut == std::string::npos || cut == 0) cut = std::min<std::size_t>(44, text.size());
                    sb.DrawString(*S().font, text.substr(0, cut), Vector2(16, y), Color(222, 228, 240, 255), 0.0f, Vector2(0, 0), 0.9f, SpriteEffects::None, 0.0f);
                    text.erase(0, cut);
                    while (!text.empty() && text.front() == ' ') text.erase(0, 1);
                    y += 27.0f;
                }
            });
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        GraphicsDevice& device = ctx.device;
        RenderStates::SetOpaque3D(device);
        DrawMipStrip(ctx);
        DrawAddressPanels(ctx);
        DrawLectern(ctx);
        DrawLitObjects(ctx);
        device.getSamplerStatesProperty()[0] = SamplerState::AnisotropicWrap;
    }

private:
    static constexpr float kRanges[] = {1.5f, 2.0f, 3.0f, 5.0f};
    static constexpr int kRangeCount = 4;

    static float ShelfX(int i) { return 3.0f + static_cast<float>(i) * 0.78f; }
    static float PanelX(int i) { return -5.5f + static_cast<float>(i) * 1.55f; }

    static SignSpec MakePlaque(SignSpec spec, float w, float h)
    {
        spec.pixelWidth = static_cast<int>(w * 260.0f);
        spec.pixelHeight = static_cast<int>(h * 260.0f);
        return spec;
    }

    // A texture whose every mip level has its own colour and a checker, so level changes are visible.
    std::unique_ptr<Texture2D> BuildMipTexture()
    {
        const Color tints[9] = {Color(235, 235, 235, 255), Color(235, 80, 70, 255), Color(240, 150, 50, 255),
                                Color(235, 215, 60, 255), Color(90, 200, 90, 255), Color(60, 200, 210, 255),
                                Color(70, 110, 235, 255), Color(150, 90, 230, 255), Color(230, 80, 200, 255)};
        std::vector<std::vector<Color>> chain;
        for (int level = 0; level < 9; ++level) {
            const int size = 256 >> level;
            const int cell = std::max(1, size / 8);
            std::vector<Color> pixels(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    const bool light = (((x / cell) + (y / cell)) % 2) == 0;
                    const Color& t = tints[level];
                    const float k = light ? 1.0f : 0.42f;
                    pixels[static_cast<std::size_t>(y * size + x)] = Color(
                        static_cast<std::uint8_t>(static_cast<float>(t.getRProperty()) * k), static_cast<std::uint8_t>(static_cast<float>(t.getGProperty()) * k),
                        static_cast<std::uint8_t>(static_cast<float>(t.getBProperty()) * k), static_cast<std::uint8_t>(255));
                }
            }
            chain.push_back(std::move(pixels));
        }
        return ImageUtil::CreateFromMipChain(*S().device, chain, 256, 256);
    }

    void DrawMipStrip(const DrawContext& ctx)
    {
        if (mipTexture_ == nullptr) return;
        SamplerState sampler;
        sampler.setFilterProperty(kFilters[filter_].filter);
        sampler.setAddressUProperty(TextureAddressMode::Wrap);
        sampler.setAddressVProperty(TextureAddressMode::Wrap);
        sampler.setMaxAnisotropyProperty(16);
        ctx.device.getSamplerStatesProperty()[0] = sampler;
        const float length = kStripSouth - kStripNorth;
        const Vector3 origin = R().ToWorld(kStripX - kStripHalfWidth, 0.015f, kStripSouth);
        DrawUvQuad(ctx, mipTexture_.get(), origin, Vector3(2.0f * kStripHalfWidth, 0, 0), Vector3(0, 0, -length),
                   Vector2(0, 0), Vector2(2.0f, length / 1.2f));
    }

    void DrawAddressPanels(const DrawContext& ctx)
    {
        Texture2D* grid = S().textures->Get("uvgrid");
        const float r = kRanges[rangeIndex_];
        for (int i = 0; i < 3; ++i) {
            SamplerState sampler;
            sampler.setFilterProperty(TextureFilter::Linear);
            sampler.setAddressUProperty(kAddress[i].mode);
            sampler.setAddressVProperty(kAddress[i].mode);
            ctx.device.getSamplerStatesProperty()[0] = sampler;
            const Vector3 origin = R().ToWorld(PanelX(i) - 0.65f, 1.45f, -6.9f);
            DrawUvQuad(ctx, grid, origin, Vector3(1.3f, 0, 0), Vector3(0, 1.3f, 0), Vector2(0.5f - r / 2, 0.5f - r / 2),
                       Vector2(0.5f + r / 2, 0.5f + r / 2));
        }
    }

    void DrawLectern(const DrawContext& ctx)
    {
        if (lecternScreen_ == nullptr || !lecternScreen_->HasContent()) return;
        ctx.device.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
        // An inclined plate on top of the lectern, tilted towards the visitor standing south of it.
        const float tilt = MathHelper::ToRadians(38.0f);
        const Vector3 origin = R().ToWorld(kLecternX - 0.5f, 1.07f, kLecternZ + 0.26f);
        const Vector3 U(1.0f, 0, 0);
        const Vector3 V(0, std::sin(tilt) * 0.375f, -std::cos(tilt) * 0.375f);
        QuadDraw::Textured(ctx, lecternScreen_->Texture(), origin, U, V);
    }

    void DrawLitObjects(const DrawContext& ctx)
    {
        GraphicsDevice& device = ctx.device;
        device.getSamplerStatesProperty()[0] = SamplerState::AnisotropicWrap;
        // Spheres on the shelf: one BasicEffect, reconfigured per material.
        for (int i = 0; i < kMaterialCount; ++i) {
            const MaterialSpec& m = kMaterials[i];
            BasicEffect& fx = ctx.fx.BeginLit(m.texture != nullptr ? S().textures->Get(m.texture) : nullptr);
            fx.EnableDefaultLighting();
            fx.setPreferPerPixelLightingProperty(perPixel_);
            fx.setDiffuseColorProperty(m.diffuse);
            fx.setSpecularColorProperty(m.specular);
            fx.setSpecularPowerProperty(m.power);
            fx.setEmissiveColorProperty(m.emissive);
            fx.setViewProperty(ctx.view.view);
            fx.setProjectionProperty(ctx.view.projection);
            fx.setWorldProperty(Matrix::CreateScale(0.6f) * Matrix::CreateRotationY(angle_ + static_cast<float>(i)) *
                                Matrix::CreateTranslation(R().ToWorld(ShelfX(i), kShelfY + 0.3f, -6.5f)));
            EffectBank::Apply(fx);
            S().shapes->Sphere().Draw(device);
        }

        // The UV test cube and sphere.
        Texture2D* tex = S().textures->Get(useGrid_ ? "uvgrid" : "checker");
        BasicEffect& fx = ctx.fx.BeginLit(tex);
        fx.EnableDefaultLighting();
        fx.setPreferPerPixelLightingProperty(true);
        fx.setDiffuseColorProperty(Vector3(1, 1, 1));
        fx.setSpecularColorProperty(Vector3(0.15f, 0.15f, 0.15f));
        fx.setSpecularPowerProperty(16.0f);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setWorldProperty(Matrix::CreateScale(0.7f) * Matrix::CreateRotationX(0.4f) * Matrix::CreateRotationY(angle_) *
                            Matrix::CreateTranslation(R().ToWorld(4.8f, 0.9f + 0.55f, 3.0f)));
        EffectBank::Apply(fx);
        S().shapes->Cube().Draw(device);
        fx.setWorldProperty(Matrix::CreateScale(0.8f) * Matrix::CreateRotationY(angle_ * 0.7f) *
                            Matrix::CreateTranslation(R().ToWorld(4.8f, 0.9f + 0.42f, 5.4f)));
        EffectBank::Apply(fx);
        S().shapes->Sphere().Draw(device);
    }

    std::unique_ptr<Texture2D> mipTexture_;
    std::unique_ptr<LiveScreen> lecternScreen_;
    bool perPixel_ = true;
    bool useGrid_ = true;
    bool screenDirty_ = true;
    int filter_ = 3;
    int rangeIndex_ = 1;
    float angle_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeMaterialsRoom(const RoomDef& room, Services& services)
{
    return std::make_unique<MaterialsRoom>(room, services);
}
