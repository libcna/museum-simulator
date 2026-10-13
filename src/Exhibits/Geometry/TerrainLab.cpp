// Heightmap terrain: a Texture2D read back with GetData becomes a grid of VertexPositionNormalTexture vertices and
// 16-bit indices; a ball rolls over it using bilinear height lookups. The classic XNA terrain tutorial in one room.
#include <algorithm>
#include <cmath>
#include <exception>
#include <string>
#include <vector>

#include "CNA/GraphicsCapability.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

constexpr int kGrid = 64;          // heightmap texels = vertices per side
constexpr float kSize = 8.0f;      // metres across

class TerrainLab final : public Exhibit {
public:
    TerrainLab(const RoomDef& room, Services& services) : Exhibit(room, services), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Heightmap terrain", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddSlider("Height scale", 0.2f, 3.0f, heightScale_, [this](float v) { heightScale_ = v; dirty_ = true; });
        p.AddChoice("Vertex normals", {"From the heightmap (central differences)", "All straight up"}, 0, [this](int v) { smooth_ = v == 0; dirty_ = true; });
        p.AddToggle("Wireframe (FillMode)", false, [this](bool v) { wire_ = v; });
        p.AddToggle("Show the normals", false, [this](bool v) { normals_ = v; });
        p.AddSlider("Ball speed", 0.0f, 2.0f, speed_, [this](float v) { speed_ = v; });
        p.AddReadout("Heightmap texture", [] { return std::to_string(kGrid) + " x " + std::to_string(kGrid) + " Color, red = height"; });
        p.AddReadout("Triangles", [] { return std::to_string(2 * (kGrid - 1) * (kGrid - 1)) + " from " + std::to_string(kGrid * kGrid) + " vertices"; });
        p.AddReadout("Height under the ball", [this] { return Fmt(ballHeight_) + " m (bilinear)"; });
        p.AddReadout("Slope under the ball", [this] { return Fmt(slopeDeg_) + " deg"; });
        SignSpec what = Kit::Plate("Terrain from a heightmap",
                                   {"A grey-scale image is a height field: Texture2D.GetData returns the texels, each becomes a vertex whose Y is the",
                                    "brightness, and 16-bit indices stitch the grid into triangles. Normals come from the neighbouring heights",
                                    "(central differences). Asking 'how high is the ground here?' is a bilinear blend of four texels: that is how the ball rolls."});
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        GraphicsDevice& d = *S().device;
        // The "asset": a heightmap drawn procedurally and then used exactly as a loaded texture would be.
        std::vector<Color> px(static_cast<std::size_t>(kGrid * kGrid));
        for (int z = 0; z < kGrid; ++z)
            for (int x = 0; x < kGrid; ++x) {
                const float u = static_cast<float>(x) / (kGrid - 1), v = static_cast<float>(z) / (kGrid - 1);
                float h = 0.85f * std::exp(-((u - 0.32f) * (u - 0.32f) + (v - 0.38f) * (v - 0.38f)) / 0.03f) + 0.6f * std::exp(-((u - 0.74f) * (u - 0.74f) + (v - 0.68f) * (v - 0.68f)) / 0.02f) +
                          0.12f * std::sin(u * 17.0f) * std::sin(v * 13.0f) + 0.06f * std::sin((u + v) * 40.0f);
                h = std::clamp(h * 0.9f + 0.08f, 0.0f, 1.0f);
                const auto b = static_cast<std::uint8_t>(h * 255.0f);
                px[static_cast<std::size_t>(z * kGrid + x)] = Color(b, b, b, static_cast<std::uint8_t>(255));
            }
        heightmap_ = std::make_unique<Texture2D>(d, kGrid, kGrid);
        heightmap_->SetData(px.data(), kGrid * kGrid);
        std::vector<Color> back(px.size());
        heightmap_->GetData(back.data(), kGrid * kGrid);
        heights_.resize(back.size());
        for (std::size_t i = 0; i < back.size(); ++i) heights_[i] = static_cast<float>(back[i].getRProperty()) / 255.0f;
        BuildIndices(d);
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt * speed_;
        const float x = 0.34f * kSize * std::sin(time_ * 0.6f), z = 0.30f * kSize * std::sin(time_ * 0.9f + 1.0f);
        ballLocal_ = Vector3(x, HeightAt(x, z) * heightScale_ + 0.3f, z);
        ballHeight_ = ballLocal_.Y - 0.3f;
        const float e = kSize / (kGrid - 1);
        const float dx = (HeightAt(x + e, z) - HeightAt(x - e, z)) * heightScale_ / (2.0f * e), dz = (HeightAt(x, z + e) - HeightAt(x, z - e)) * heightScale_ / (2.0f * e);
        slopeDeg_ = MathHelper::ToDegrees(std::atan(std::sqrt(dx * dx + dz * dz)));
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!ib_) return;
        GraphicsDevice& d = ctx.device;
        if (dirty_ || !vb_) {
            BuildVertices(d);
            dirty_ = false;
        }
        const Vector3 origin = R().ToWorld(0.0f, 0.02f, -0.6f);
        BasicEffect& fx = ctx.fx.BeginLit(S().textures->Get("grass"));
        fx.EnableDefaultLighting();
        fx.setPunctualLightEXT(PunctualLightEXT{});
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setWorldProperty(Matrix::CreateTranslation(origin));
        fx.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setSpecularColorProperty(Vector3(0, 0, 0));
        EffectBank::Apply(fx);
        static RasterizerState wireState = [] {
            RasterizerState r = RasterizerState::CullNone;
            r.setFillModeProperty(FillMode::WireFrame);
            return r;
        }();
        d.setRasterizerStateProperty(wire_ && d.SupportsCapability(CNA::GraphicsCapability::WireFrame) ? wireState : RasterizerState::CullClockwise);
        d.SetVertexBuffer(vb_.get());
        d.setIndicesProperty(ib_.get());
        d.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, kGrid * kGrid, 0, 2 * (kGrid - 1) * (kGrid - 1));
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);

        // The ball, resting on the ground, and the normal the ground has there.
        const Vector3 ball = origin + Off(ballLocal_.X, ballLocal_.Y, ballLocal_.Z);
        BasicEffect& bf = ctx.fx.BeginLit();
        bf.EnableDefaultLighting();
        bf.setPunctualLightEXT(PunctualLightEXT{});
        bf.setViewProperty(ctx.view.view);
        bf.setProjectionProperty(ctx.view.projection);
        bf.setWorldProperty(Matrix::CreateScale(0.6f) * Matrix::CreateTranslation(ball));
        bf.setDiffuseColorProperty(Vector3(0.9f, 0.3f, 0.25f));
        bf.setEmissiveColorProperty(Vector3(0.1f, 0.02f, 0.02f));
        bf.setSpecularColorProperty(Vector3(0.6f, 0.6f, 0.6f));
        EffectBank::Apply(bf);
        S().shapes->Sphere().Draw(d);
        if (normals_) {
            for (int z = 0; z < kGrid; z += 4)
                for (int x = 0; x < kGrid; x += 4) {
                    const Vector3 p = origin + Off(Pos(x, z));
                    gizmos_.Line(p, p + Off(NormalAt(x, z)) * 0.3f, Color(255, 230, 90, 255));
                }
        }
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] float H(int x, int z) const { return heights_[static_cast<std::size_t>(std::clamp(z, 0, kGrid - 1) * kGrid + std::clamp(x, 0, kGrid - 1))]; }
    [[nodiscard]] Vector3 Pos(int x, int z) const
    {
        return Vector3((static_cast<float>(x) / (kGrid - 1) - 0.5f) * kSize, H(x, z) * heightScale_, (static_cast<float>(z) / (kGrid - 1) - 0.5f) * kSize);
    }
    // Normal from the four neighbouring heights: the slope in x and in z, as a cross product of the two tangents.
    [[nodiscard]] Vector3 NormalAt(int x, int z) const
    {
        if (!smooth_) return Vector3(0, 1, 0);
        const float e = kSize / (kGrid - 1);
        const float hx = (H(x + 1, z) - H(x - 1, z)) * heightScale_, hz = (H(x, z + 1) - H(x, z - 1)) * heightScale_;
        return Vector3::Normalize(Vector3(-hx / (2.0f * e), 1.0f, -hz / (2.0f * e)));
    }
    // Bilinear height at a point in the terrain's own space (metres from the centre).
    [[nodiscard]] float HeightAt(float x, float z) const
    {
        const float fx = std::clamp((x / kSize + 0.5f) * (kGrid - 1), 0.0f, static_cast<float>(kGrid - 1) - 0.001f);
        const float fz = std::clamp((z / kSize + 0.5f) * (kGrid - 1), 0.0f, static_cast<float>(kGrid - 1) - 0.001f);
        const int ix = static_cast<int>(fx), iz = static_cast<int>(fz);
        const float tx = fx - static_cast<float>(ix), tz = fz - static_cast<float>(iz);
        return MathHelper::Lerp(MathHelper::Lerp(H(ix, iz), H(ix + 1, iz), tx), MathHelper::Lerp(H(ix, iz + 1), H(ix + 1, iz + 1), tx), tz);
    }

    void BuildVertices(GraphicsDevice& d)
    {
        std::vector<VertexPositionNormalTexture> v(static_cast<std::size_t>(kGrid * kGrid));
        for (int z = 0; z < kGrid; ++z)
            for (int x = 0; x < kGrid; ++x)
                v[static_cast<std::size_t>(z * kGrid + x)] = VertexPositionNormalTexture(Pos(x, z), NormalAt(x, z), Vector2(static_cast<float>(x) / 6.0f, static_cast<float>(z) / 6.0f));
        if (!vb_) vb_ = std::make_unique<VertexBuffer>(d, VertexPositionNormalTexture::getVertexDeclarationStatic(), kGrid * kGrid, BufferUsage::None);
        vb_->SetData(v.data(), kGrid * kGrid);
    }
    void BuildIndices(GraphicsDevice& d)
    {
        std::vector<std::uint16_t> idx;
        idx.reserve(static_cast<std::size_t>(6 * (kGrid - 1) * (kGrid - 1)));
        for (int z = 0; z < kGrid - 1; ++z)
            for (int x = 0; x < kGrid - 1; ++x) {
                const auto a = static_cast<std::uint16_t>(z * kGrid + x), b = static_cast<std::uint16_t>(a + 1), c = static_cast<std::uint16_t>(a + kGrid), e = static_cast<std::uint16_t>(c + 1);
                idx.insert(idx.end(), {a, c, b, b, c, e});
            }
        ib_ = std::make_unique<IndexBuffer>(d, IndexElementSize::SixteenBits, static_cast<int>(idx.size()), BufferUsage::WriteOnly);
        ib_->SetData(idx.data(), static_cast<int>(idx.size()));
    }

    Gizmos gizmos_;
    std::unique_ptr<Texture2D> heightmap_;
    std::unique_ptr<VertexBuffer> vb_;
    std::unique_ptr<IndexBuffer> ib_;
    std::vector<float> heights_;
    Vector3 ballLocal_{0, 1, 0};
    float heightScale_ = 1.6f, speed_ = 1.0f, time_ = 0.0f, ballHeight_ = 0.0f, slopeDeg_ = 0.0f;
    bool dirty_ = true, smooth_ = true, wire_ = false, normals_ = false;
};

}  // namespace

std::unique_ptr<Exhibit> MakeTerrainLab(const RoomDef& room, Services& services)
{
    return std::make_unique<TerrainLab>(room, services);
}
