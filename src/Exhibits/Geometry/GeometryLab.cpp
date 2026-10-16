// Geometry laboratory: vertex and index buffers, primitive types, dynamic geometry, procedural meshes,
// 32-bit indices, many small buffers and hardware instancing. One class, eight rooms ("mode").
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>

#include "Diagnostics/Log.hpp"
#include "CNA/GraphicsCapability.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicIndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SetDataOptions.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBufferBinding.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "Rendering/CompiledInstancing.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/RenderStates.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Triangle, Indexed, Primitives, Dynamic, Procedural, Index32, ManyBuffers, Instancing, DynIndex };

Mode ParseMode(const std::string& s)
{
    if (s == "indexed") return Mode::Indexed;
    if (s == "primitives") return Mode::Primitives;
    if (s == "dynamic") return Mode::Dynamic;
    if (s == "procedural") return Mode::Procedural;
    if (s == "index32") return Mode::Index32;
    if (s == "manybuffers") return Mode::ManyBuffers;
    if (s == "instancing") return Mode::Instancing;
    if (s == "dynindex") return Mode::DynIndex;
    return Mode::Triangle;
}

using Vnt = VertexPositionNormalTexture;

struct CubeData {
    std::vector<Vnt> v;
    std::vector<std::uint16_t> i;
};
CubeData MakeCube()
{
    CubeData c;
    const Vector3 n[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    for (const Vector3& nn : n) {
        const Vector3 up = std::abs(nn.Y) > 0.5f ? Vector3(0, 0, -1) : Vector3(0, 1, 0);
        const Vector3 right = Vector3::Normalize(Vector3::Cross(up, nn));
        const Vector3 u = Vector3::Cross(nn, right);
        const std::uint16_t base = static_cast<std::uint16_t>(c.v.size());
        c.v.emplace_back(nn * 0.5f - right * 0.5f - u * 0.5f, nn, Vector2(0, 1));
        c.v.emplace_back(nn * 0.5f + right * 0.5f - u * 0.5f, nn, Vector2(1, 1));
        c.v.emplace_back(nn * 0.5f + right * 0.5f + u * 0.5f, nn, Vector2(1, 0));
        c.v.emplace_back(nn * 0.5f - right * 0.5f + u * 0.5f, nn, Vector2(0, 0));
        for (const int k : {0, 1, 2, 0, 2, 3}) c.i.push_back(static_cast<std::uint16_t>(base + k));
    }
    return c;
}

class GeometryLab final : public Exhibit {
public:
    GeometryLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        BuildStagePlinth(b, 0.0f, 0.0f, 0.4f);
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Triangle:
                p.AddChoice("RasterizerState", {"CullNone", "CullClockwise", "CullCounterClockwise"}, 1, [this](int v) { cull_ = v; });
                p.AddSlider("Turn the triangles", -180.0f, 180.0f, turn_, [this](float v) { turn_ = v; }, "%.0f deg", 1.0f);
                p.AddSlider("Vertex 3 height", 0.2f, 1.4f, apex_, [this](float v) { apex_ = v; });
                p.AddReadout("Left triangle (A B C)", [this] { return Facing(true); });
                p.AddReadout("Right triangle (A C B)", [this] { return Facing(false); });
                break;
            case Mode::Indexed:
                p.AddSlider("Spin speed", 0.0f, 2.0f, speed_, [this](float v) { speed_ = v; });
                p.AddHeading("The same cube, three ways");
                p.AddReadout("Left: DrawPrimitives", [] { return std::string("36 vertices, no indices"); });
                p.AddReadout("Middle: 8 shared vertices", [] { return std::string("8 vertices + 36 indices"); });
                p.AddReadout("Right: 24 vertices + normals", [] { return std::string("24 vertices + 36 indices, lit"); });
                break;
            case Mode::Primitives:
                p.AddChoice("PrimitiveType", {"TriangleList", "TriangleStrip", "LineList", "LineStrip", "PointListEXT"}, 0, [this](int v) { prim_ = v; });
                p.AddSlider("Vertices used", 2.0f, 12.0f, static_cast<float>(count_), [this](float v) { count_ = static_cast<int>(v); }, "%.0f", 1.0f);
                p.AddReadout("Primitive count", [this] { return PrimitiveFormula(); });
                p.AddReadout("API", [this] { return std::string(prim_ == 4 ? "CNA extension" : "XNA 4.0"); });
                break;
            case Mode::Dynamic:
                p.AddSlider("Grid size", 8.0f, 128.0f, static_cast<float>(grid_), [this](float v) { grid_ = static_cast<int>(v); dirty_ = true; }, "%.0f", 1.0f);
                p.AddSlider("Wave speed", 0.0f, 4.0f, speed_, [this](float v) { speed_ = v; });
                p.AddChoice("SetDataOptions", {"Discard", "NoOverwrite", "None"}, 0, [this](int v) { dataOptions_ = v; });
                p.AddChoice("Vertices reach the GPU by", {"DynamicVertexBuffer.SetData", "DrawUserIndexedPrimitives (no buffer)"}, 0, [this](int v) { userDraw_ = v == 1; });
                p.AddReadout("Vertices rewritten per frame", [this] { return std::to_string(grid_ * grid_); });
                p.AddReadout("Upload", [this] { return Fmt(static_cast<float>(grid_ * grid_) * static_cast<float>(sizeof(Vnt)) / 1024.0f) + " KB per frame"; });
                break;
            case Mode::Procedural:
                p.AddChoice("Shape", {"Sphere", "Torus", "Cone", "Height field"}, 0, [this](int v) { shape_ = v; dirty_ = true; });
                p.AddSlider("Resolution", 4.0f, 80.0f, static_cast<float>(grid_), [this](float v) { grid_ = static_cast<int>(v); dirty_ = true; }, "%.0f", 1.0f);
                p.AddToggle("Wireframe (FillMode)", false, [this](bool v) { wire_ = v; });
                p.AddReadout("Vertices / triangles", [this] { return std::to_string(vertexCount_) + " / " + std::to_string(triangleCount_); });
                p.AddReadout("WireFrame supported", [this] { return std::string(S().device->SupportsCapability(CNA::GraphicsCapability::WireFrame) ? "yes (FillMode)" : "NO: wireframe is ignored here"); });
                break;
            case Mode::Index32:
                p.AddSlider("Grid size", 50.0f, 400.0f, static_cast<float>(grid_), [this](float v) { grid_ = static_cast<int>(v); dirty_ = true; }, "%.0f", 10.0f);
                p.AddReadout("Vertices", [this] { return std::to_string(grid_ * grid_); });
                p.AddReadout("Index size", [this] { return std::string(grid_ * grid_ > 65535 ? "ThirtyTwoBits (needed: > 65535)" : "SixteenBits"); });
                p.AddReadout("Triangles", [this] { return std::to_string((grid_ - 1) * (grid_ - 1) * 2); });
                break;
            case Mode::ManyBuffers:
                p.AddSlider("Objects", 10.0f, 2000.0f, static_cast<float>(count_ = 400), [this](float v) { count_ = static_cast<int>(v); dirty_ = true; }, "%.0f", 10.0f);
                p.AddChoice("Layout", {"One VertexBuffer each", "One merged buffer"}, 0, [this](int v) { merged_ = v == 1; dirty_ = true; });
                p.AddReadout("Draw calls per frame", [this] { return merged_ ? std::string("1") : std::to_string(count_); });
                p.AddReadout("Buffers alive", [this] { return merged_ ? std::string("2") : std::to_string(count_ + 1); });
                break;
            case Mode::DynIndex:
                p.AddSlider("Reveal radius (m)", 0.3f, 3.0f, revealRadius_, [this](float v) { revealRadius_ = v; }, "%.1f");
                p.AddToggle("Move the window", true, [this](bool v) { moveWindow_ = v; });
                p.AddReadout("Triangles drawn / in the mesh", [this] { return std::to_string(drawnTriangles_) + " / " + std::to_string((kDynGrid - 1) * (kDynGrid - 1) * 2); });
                p.AddReadout("Index data uploaded", [this] { return Fmt(static_cast<float>(drawnTriangles_ * 3 * 2) / 1024.0f) + " KB per frame"; });
                break;
            case Mode::Instancing:
                p.AddSlider("Cubes", 100.0f, 20000.0f, static_cast<float>(count_ = 2000), [this](float v) { count_ = static_cast<int>(v); dirty_ = true; }, "%.0f", 100.0f);
                p.AddChoice("Method", {"DrawInstancedPrimitives", "A draw call per cube", "DrawInstancedPrimitivesBaseInstanceEXT (skip the first cubes)"}, 0, [this](int v) { naive_ = v == 1; baseInstance_ = v == 2; });
                p.AddSlider("First instance (BaseInstanceEXT)", 0.0f, 1000.0f, 0.0f, [this](float v) { firstInstance_ = static_cast<int>(v); }, "%.0f", 50.0f);
                p.AddReadout("Draw calls per frame", [this] { return naive_ || !instanceRefusal_.empty() ? std::to_string(count_) : std::string("1"); });
                p.AddReadout("GetVertexBuffers() while drawing", [this] { return boundStreams_; });
                p.AddReadout("Instancing supported", [this] { if (!instanceRefusal_.empty()) return std::string("reported yes, draw refused (see log)"); if (instancingFx_) return std::string("yes (compiled XNA effect)"); return std::string(S().device->SupportsCapability(CNA::GraphicsCapability::Instancing) ? "yes" : "NO on this renderer"); });
                p.AddReadout("Frame rate", [this] { return Fmt(S().stats->fps) + " fps"; });
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
        const CubeData cube = MakeCube();
        cubeV_ = std::make_unique<VertexBuffer>(d, Vnt::getVertexDeclarationStatic(), static_cast<int>(cube.v.size()), BufferUsage::WriteOnly);
        cubeV_->SetData(cube.v.data(), static_cast<int>(cube.v.size()));
        cubeI_ = std::make_unique<IndexBuffer>(d, IndexElementSize::SixteenBits, static_cast<int>(cube.i.size()), BufferUsage::WriteOnly);
        cubeI_->SetData(cube.i.data(), static_cast<int>(cube.i.size()));
        // A flat 36-vertex cube for the non-indexed comparison.
        std::vector<VertexPositionColor> flat;
        for (const std::uint16_t idx : cube.i) {
            const Vector3 n = cube.v[idx].Normal;
            flat.emplace_back(cube.v[idx].Position, Color(static_cast<std::uint8_t>(127 + n.X * 100), static_cast<std::uint8_t>(127 + n.Y * 100), static_cast<std::uint8_t>(127 + n.Z * 100), static_cast<std::uint8_t>(255)));
        }
        flatCube_ = std::make_unique<VertexBuffer>(d, VertexPositionColor::getVertexDeclarationStatic(), static_cast<int>(flat.size()), BufferUsage::WriteOnly);
        flatCube_->SetData(flat.data(), static_cast<int>(flat.size()));
        // 8 shared, vertex-coloured corners.
        std::vector<VertexPositionColor> corners;
        for (int k = 0; k < 8; ++k)
            corners.emplace_back(Vector3((k & 1) ? 0.5f : -0.5f, (k & 2) ? 0.5f : -0.5f, (k & 4) ? 0.5f : -0.5f),
                                 Color((k & 1) ? 230 : 40, (k & 2) ? 230 : 40, (k & 4) ? 230 : 40, 255));
        sharedCube_ = std::make_unique<VertexBuffer>(d, VertexPositionColor::getVertexDeclarationStatic(), 8, BufferUsage::WriteOnly);
        sharedCube_->SetData(corners.data(), 8);
        // Corner numbering: bit 0 = +x, bit 1 = +y, bit 2 = +z.
        std::vector<std::uint16_t> idx;
        const auto quad = [&](int a, int b2, int c, int dd) { for (int q : {a, b2, c, a, c, dd}) idx.push_back(static_cast<std::uint16_t>(q)); };
        quad(4, 5, 7, 6);   // +z
        quad(1, 0, 2, 3);   // -z
        quad(5, 1, 3, 7);   // +x
        quad(0, 4, 6, 2);   // -x
        quad(6, 7, 3, 2);   // +y
        quad(0, 1, 5, 4);   // -y
        sharedI_ = std::make_unique<IndexBuffer>(d, IndexElementSize::SixteenBits, static_cast<int>(idx.size()), BufferUsage::WriteOnly);
        sharedI_->SetData(idx.data(), static_cast<int>(idx.size()));
        if (mode_ == Mode::Instancing) {
            instancingFx_ = CompiledInstancing::Load(d, S().contentRoot);
        }
        dirty_ = true;
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        spin_ += ctx.dt * (mode_ == Mode::Indexed ? speed_ : 0.6f);
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!cubeV_) return;
        GraphicsDevice& device = ctx.device;
        const Vector3 o = R().ToWorld(0.0f, 1.55f, 0.0f);
        switch (mode_) {
            case Mode::Triangle: DrawTriangles(ctx, o); break;
            case Mode::Indexed: DrawIndexed(ctx, o); break;
            case Mode::Primitives: DrawPrimitivesDemo(ctx, o); break;
            case Mode::Dynamic: DrawDynamic(ctx, o); break;
            case Mode::Procedural: DrawProcedural(ctx, o); break;
            case Mode::Index32: DrawIndex32(ctx, o); break;
            case Mode::ManyBuffers: DrawManyBuffers(ctx, o); break;
            case Mode::Instancing: DrawInstancing(ctx, o); break;
            case Mode::DynIndex: DrawDynIndex(ctx, o); break;
        }
        RenderStates::SetOpaque3D(device);
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"A triangle and its winding", "Vertex and index buffers", "Primitive types", "Dynamic vertex buffers", "Procedural geometry", "32-bit indices and large meshes", "Many small vertex buffers", "Hardware instancing", "Dynamic index buffers"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Triangle: return {"Triangles have a front and a back, decided by the order of their vertices. RasterizerState.CullMode", "removes one side. The left triangle lists its corners A B C, the right one A C B: they face opposite ways."};
            case Mode::Indexed: return {"Indices let triangles share vertices: a cube needs 8 corners but 36 indices. When each face needs its", "own normal or texture coordinates the corners must be split into 24 vertices. DrawIndexedPrimitives uses them."};
            case Mode::Primitives: return {"The same vertices mean different things as TriangleList, TriangleStrip, LineList, LineStrip or (CNA", "extension) PointListEXT. The primitive count passed to the draw call depends on the type."};
            case Mode::Dynamic: return {"A DynamicVertexBuffer can be rewritten every frame. SetDataOptions.Discard tells the driver the old", "contents are not needed, so it can hand out fresh memory instead of waiting for the GPU."};
            case Mode::Procedural: return {"Meshes built in code: a sphere, torus and cone from parametric formulas, and a height field. Raise the", "resolution and the triangle count climbs; wireframe shows the tessellation."};
            case Mode::Index32: return {"16-bit indices address at most 65 536 vertices. Bigger meshes need IndexElementSize.ThirtyTwoBits,", "which every 3D CNA renderer supports. This grid goes well past the 16-bit limit."};
            case Mode::ManyBuffers: return {"Each draw call has a cost. Hundreds of tiny VertexBuffers means hundreds of calls; merging them into", "one buffer draws the same triangles in one call. Compare the frame rate."};
            case Mode::DynIndex: return {"A DynamicIndexBuffer is rewritten every frame, here to draw only the part of a large height field near a moving", "point: the vertices never change, only which triangles the indices select. SetDataOptions.Discard avoids stalling the GPU."};
            default: return {"Instancing draws one mesh many times in a single call, taking a world matrix per instance from a second", "vertex stream (VertexBufferBinding with an instance frequency). Compare it with a call per cube."};
        }
    }

    BasicEffect& Flat(const DrawContext& ctx, const Matrix& world)
    {
        BasicEffect& fx = ctx.fx.Flat();
        fx.setWorldProperty(world);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        return fx;
    }
    BasicEffect& Lit(const DrawContext& ctx, const Matrix& world, const Vector3& colour)
    {
        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPunctualLightEXT(PunctualLightEXT{});
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setWorldProperty(world);
        fx.setDiffuseColorProperty(colour);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setSpecularColorProperty(Vector3(0.2f, 0.2f, 0.2f));
        return fx;
    }

    // ---- triangle ---------------------------------------------------------------------------
    [[nodiscard]] std::string Facing(bool left) const
    {
        if (S().mainView == nullptr) return "-";
        const Vector3 a(-0.5f, -0.4f, 0), b(0.5f, -0.4f, 0), c(0.0f, apex_ - 0.4f, 0);
        const Matrix m = Matrix::CreateRotationY(MathHelper::ToRadians(turn_)) * Matrix::CreateTranslation(R().ToWorld(left ? -0.9f : 0.9f, 1.55f, 0.0f));
        const Vector3 A = Vector3::Transform(a, m), B = Vector3::Transform(left ? b : c, m), C = Vector3::Transform(left ? c : b, m);
        const Vector3 n = Vector3::Cross(B - A, C - A);
        const bool ccw = Vector3::Dot(n, S().mainView->eye - A) > 0.0f;
        const bool visible = cull_ == 0 || (cull_ == 1 && ccw) || (cull_ == 2 && !ccw);
        return std::string(ccw ? "counter-clockwise" : "clockwise") + (visible ? ": drawn" : ": culled");
    }
    void DrawTriangles(const DrawContext& ctx, const Vector3&)
    {
        GraphicsDevice& device = ctx.device;
        device.setRasterizerStateProperty(cull_ == 0 ? RasterizerState::CullNone : (cull_ == 1 ? RasterizerState::CullClockwise : RasterizerState::CullCounterClockwise));
        const VertexPositionColor leftTri[3] = {{Vector3(-0.5f, -0.4f, 0), Color(240, 80, 70, 255)}, {Vector3(0.5f, -0.4f, 0), Color(80, 240, 110, 255)}, {Vector3(0, apex_ - 0.4f, 0), Color(80, 120, 255, 255)}};
        const VertexPositionColor rightTri[3] = {leftTri[0], leftTri[2], leftTri[1]};
        BasicEffect& fx = Flat(ctx, Matrix::CreateRotationY(MathHelper::ToRadians(turn_)) * Matrix::CreateTranslation(R().ToWorld(-0.9f, 1.55f, 0.0f)));
        EffectBank::Apply(fx);
        device.DrawUserPrimitives(PrimitiveType::TriangleList, leftTri, 0, 1);
        BasicEffect& fx2 = Flat(ctx, Matrix::CreateRotationY(MathHelper::ToRadians(turn_)) * Matrix::CreateTranslation(R().ToWorld(0.9f, 1.55f, 0.0f)));
        EffectBank::Apply(fx2);
        device.DrawUserPrimitives(PrimitiveType::TriangleList, rightTri, 0, 1);
        gizmos_.Label(R().ToWorld(-0.9f, 2.45f, 0.0f), "A B C", Color(255, 230, 150, 255), 0.55f);
        gizmos_.Label(R().ToWorld(0.9f, 2.45f, 0.0f), "A C B", Color(255, 230, 150, 255), 0.55f);
    }

    // ---- indexed ----------------------------------------------------------------------------
    void DrawIndexed(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& device = ctx.device;
        const Matrix rot = Matrix::CreateRotationY(spin_) * Matrix::CreateRotationX(0.4f);
        {
            BasicEffect& fx = Flat(ctx, Matrix::CreateScale(0.8f) * rot * Matrix::CreateTranslation(o + Off(-2.2f, 0, 0)));
            EffectBank::Apply(fx);
            device.SetVertexBuffer(flatCube_.get());
            device.DrawPrimitives(PrimitiveType::TriangleList, 0, 12);
        }
        {
            BasicEffect& fx = Flat(ctx, Matrix::CreateScale(0.8f) * rot * Matrix::CreateTranslation(o));
            EffectBank::Apply(fx);
            device.SetVertexBuffer(sharedCube_.get());
            device.setIndicesProperty(sharedI_.get());
            device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 8, 0, 12);
        }
        {
            BasicEffect& fx = Lit(ctx, Matrix::CreateScale(0.8f) * rot * Matrix::CreateTranslation(o + Off(2.2f, 0, 0)), Vector3(0.8f, 0.8f, 0.9f));
            EffectBank::Apply(fx);
            device.SetVertexBuffer(cubeV_.get());
            device.setIndicesProperty(cubeI_.get());
            device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 24, 0, 12);
        }
        gizmos_.Label(o + Off(-2.2f, 0.9f, 0), "36 vertices", Color(255, 230, 150, 255), 0.5f);
        gizmos_.Label(o + Vector3(0, 0.9f, 0), "8 vertices + indices", Color(255, 230, 150, 255), 0.5f);
        gizmos_.Label(o + Off(2.2f, 0.9f, 0), "24 vertices + indices", Color(255, 230, 150, 255), 0.5f);
    }

    // ---- primitive types ----------------------------------------------------------------------
    [[nodiscard]] int PrimitiveCount() const
    {
        switch (prim_) {
            case 0: return count_ / 3;
            case 1: return std::max(0, count_ - 2);
            case 2: return count_ / 2;
            case 3: return std::max(0, count_ - 1);
            default: return count_;
        }
    }
    [[nodiscard]] std::string PrimitiveFormula() const
    {
        static const char* f[] = {"vertices / 3", "vertices - 2", "vertices / 2", "vertices - 1", "vertices"};
        return std::to_string(PrimitiveCount()) + "   (" + f[prim_] + ")";
    }
    void DrawPrimitivesDemo(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& device = ctx.device;
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        std::vector<VertexPositionColor> v;
        for (int i = 0; i < count_; ++i) {
            const float x = static_cast<float>(i) * 0.38f - static_cast<float>(count_ - 1) * 0.19f;
            const float y = (i % 2 == 0) ? -0.5f : 0.5f;
            const float hue = static_cast<float>(i) / 12.0f * 6.0f;
            v.emplace_back(Vector3(x, y, 0), Color(static_cast<std::uint8_t>(127 + 127 * std::sin(hue)), static_cast<std::uint8_t>(127 + 127 * std::sin(hue + 2.1f)), static_cast<std::uint8_t>(127 + 127 * std::sin(hue + 4.2f)), static_cast<std::uint8_t>(255)));
            gizmos_.Label(o + Vector3(x, y + (y > 0 ? 0.2f : -0.2f), 0), std::to_string(i), Color(255, 255, 255, 255), 0.45f);
        }
        BasicEffect& fx = Flat(ctx, Matrix::CreateTranslation(o));
        EffectBank::Apply(fx);
        static const PrimitiveType types[] = {PrimitiveType::TriangleList, PrimitiveType::TriangleStrip, PrimitiveType::LineList, PrimitiveType::LineStrip, PrimitiveType::PointListEXT};
        if (PrimitiveCount() > 0) device.DrawUserPrimitives(types[prim_], v.data(), 0, PrimitiveCount());
    }

    // ---- dynamic ----------------------------------------------------------------------------
    void BuildGrid(int n, std::vector<Vnt>& verts, std::vector<std::uint32_t>& idx, float size)
    {
        verts.assign(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), Vnt());
        idx.clear();
        for (int z = 0; z < n; ++z)
            for (int x = 0; x < n; ++x) {
                verts[static_cast<std::size_t>(z * n + x)] = Vnt(Vector3((static_cast<float>(x) / static_cast<float>(n - 1) - 0.5f) * size, 0, (static_cast<float>(z) / static_cast<float>(n - 1) - 0.5f) * size), Vector3(0, 1, 0),
                                                                  Vector2(static_cast<float>(x) / static_cast<float>(n - 1), static_cast<float>(z) / static_cast<float>(n - 1)));
                if (x + 1 < n && z + 1 < n) {
                    const std::uint32_t i0 = static_cast<std::uint32_t>(z * n + x), i1 = i0 + 1, i2 = i0 + static_cast<std::uint32_t>(n), i3 = i2 + 1;
                    for (std::uint32_t q : {i0, i2, i1, i1, i2, i3}) idx.push_back(q);
                }
            }
    }
    static float Height(float x, float z, float t) { return std::sin(x * 2.4f + t) * 0.25f + std::cos(z * 2.9f - t * 1.3f) * 0.2f; }
    void Displace(std::vector<Vnt>& v, float t)
    {
        for (Vnt& p : v) {
            p.Position.Y = Height(p.Position.X, p.Position.Z, t);
            const float e = 0.02f;
            const float hx = Height(p.Position.X + e, p.Position.Z, t) - Height(p.Position.X - e, p.Position.Z, t);
            const float hz = Height(p.Position.X, p.Position.Z + e, t) - Height(p.Position.X, p.Position.Z - e, t);
            p.Normal = Vector3::Normalize(Vector3(-hx, 2.0f * e, -hz));
        }
    }
    void DrawDynamic(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& device = ctx.device;
        if (dirty_ || !dyn_) {
            BuildGrid(grid_, gridV_, gridI32_, 3.6f);
            gridI16_.assign(gridI32_.begin(), gridI32_.end());
            std::vector<std::uint16_t> idx16;
            for (std::uint32_t q : gridI32_) idx16.push_back(static_cast<std::uint16_t>(q));
            userIdx_ = idx16;
            dyn_ = std::make_unique<DynamicVertexBuffer>(device, Vnt::getVertexDeclarationStatic(), grid_ * grid_, BufferUsage::WriteOnly);
            gridIb_ = std::make_unique<IndexBuffer>(device, IndexElementSize::SixteenBits, static_cast<int>(idx16.size()), BufferUsage::WriteOnly);
            gridIb_->SetData(idx16.data(), static_cast<int>(idx16.size()));
            dirty_ = false;
        }
        Displace(gridV_, time_ * speed_);
        static const SetDataOptions opts[] = {SetDataOptions::Discard, SetDataOptions::NoOverwrite, SetDataOptions::None};
        if (!userDraw_) dyn_->SetData(gridV_.data(), 0, grid_ * grid_, opts[dataOptions_]);
        BasicEffect& fx = Lit(ctx, Matrix::CreateTranslation(o + Vector3(0, -0.2f, 0)), Vector3(0.3f, 0.6f, 0.95f));
        EffectBank::Apply(fx);
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        if (userDraw_) {
            // The vertex and index arrays go straight into the draw call; CNA copies them every time, so this suits
            // small or one-off geometry, not a grid redrawn each frame.
            device.DrawUserIndexedPrimitives(PrimitiveType::TriangleList, gridV_.data(), 0, grid_ * grid_, userIdx_.data(), 0, static_cast<int>(userIdx_.size()) / 3);
            return;
        }
        device.SetVertexBuffer(dyn_.get());
        device.setIndicesProperty(gridIb_.get());
        device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, grid_ * grid_, 0, static_cast<int>(gridI32_.size()) / 3);
    }

    // ---- procedural -----------------------------------------------------------------------------
    void BuildProcedural(GraphicsDevice& device)
    {
        std::vector<Vnt> v;
        std::vector<std::uint32_t> idx;
        const int n = std::max(4, grid_);
        const auto grid = [&](int nu, int nv, auto&& fn) {
            const std::size_t base = v.size();
            for (int j = 0; j <= nv; ++j)
                for (int i = 0; i <= nu; ++i) {
                    Vector3 pos, nrm;
                    fn(static_cast<float>(i) / static_cast<float>(nu), static_cast<float>(j) / static_cast<float>(nv), pos, nrm);
                    v.emplace_back(pos, nrm, Vector2(static_cast<float>(i) / static_cast<float>(nu), static_cast<float>(j) / static_cast<float>(nv)));
                }
            for (int j = 0; j < nv; ++j)
                for (int i = 0; i < nu; ++i) {
                    const std::uint32_t a = static_cast<std::uint32_t>(base) + static_cast<std::uint32_t>(j * (nu + 1) + i), b2 = a + 1, c = a + static_cast<std::uint32_t>(nu + 1), d = c + 1;
                    for (std::uint32_t q : {a, b2, c, b2, d, c}) idx.push_back(q);
                }
        };
        switch (shape_) {
            case 0: grid(n, n / 2, [](float u, float w, Vector3& p, Vector3& nn) {
                        const float th = u * MathHelper::TwoPi, ph = w * MathHelper::Pi;
                        nn = Vector3(std::sin(ph) * std::cos(th), std::cos(ph), std::sin(ph) * std::sin(th));
                        p = nn * 0.9f;
                    }); break;
            case 1: grid(n, std::max(4, n / 2), [](float u, float w, Vector3& p, Vector3& nn) {
                        const float th = u * MathHelper::TwoPi, ph = w * MathHelper::TwoPi;
                        nn = Vector3(std::cos(ph) * std::cos(th), std::sin(ph), std::cos(ph) * std::sin(th));
                        p = Vector3(std::cos(th) * 0.8f, 0, std::sin(th) * 0.8f) + nn * 0.32f;
                    }); break;
            case 2: grid(n, 2, [](float u, float w, Vector3& p, Vector3& nn) {
                        const float th = u * MathHelper::TwoPi;
                        const float r = 0.8f * (1.0f - w);
                        p = Vector3(std::cos(th) * r, w * 1.6f - 0.8f, std::sin(th) * r);
                        nn = Vector3::Normalize(Vector3(std::cos(th) * 1.6f, 0.8f, std::sin(th) * 1.6f));
                    }); break;
            default: grid(n, n, [](float u, float w, Vector3& p, Vector3& nn) {
                        const float x = (u - 0.5f) * 3.0f, z = (w - 0.5f) * 3.0f;
                        const auto h = [](float xx, float zz) { return std::sin(xx * 2.0f) * std::cos(zz * 2.0f) * 0.4f + std::exp(-(xx * xx + zz * zz)) * 0.5f; };
                        p = Vector3(x, h(x, z), z);
                        nn = Vector3::Normalize(Vector3(-(h(x + 0.01f, z) - h(x - 0.01f, z)) / 0.02f, 1.0f, -(h(x, z + 0.01f) - h(x, z - 0.01f)) / 0.02f));
                    }); break;
        }
        vertexCount_ = static_cast<int>(v.size());
        triangleCount_ = static_cast<int>(idx.size()) / 3;
        procV_ = std::make_unique<VertexBuffer>(device, Vnt::getVertexDeclarationStatic(), vertexCount_, BufferUsage::WriteOnly);
        procV_->SetData(v.data(), vertexCount_);
        const bool big = vertexCount_ > 65535;
        procI_ = std::make_unique<IndexBuffer>(device, big ? IndexElementSize::ThirtyTwoBits : IndexElementSize::SixteenBits, static_cast<int>(idx.size()), BufferUsage::WriteOnly);
        if (big) procI_->SetData(idx.data(), static_cast<int>(idx.size()));
        else {
            std::vector<std::uint16_t> i16(idx.begin(), idx.end());
            procI_->SetData(i16.data(), static_cast<int>(i16.size()));
        }
    }
    void DrawProcedural(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& device = ctx.device;
        if (dirty_ || !procV_) {
            BuildProcedural(device);
            dirty_ = false;
        }
        BasicEffect& fx = Lit(ctx, Matrix::CreateRotationY(spin_) * Matrix::CreateTranslation(o + Vector3(0, shape_ == 3 ? -0.3f : 0.0f, 0)), Vector3(0.9f, 0.75f, 0.35f));
        EffectBank::Apply(fx);
        static RasterizerState wireState = [] {
            RasterizerState r = RasterizerState::CullNone;
            r.setFillModeProperty(FillMode::WireFrame);
            return r;
        }();
        device.setRasterizerStateProperty(wire_ && device.SupportsCapability(CNA::GraphicsCapability::WireFrame) ? wireState : RasterizerState::CullNone);
        device.SetVertexBuffer(procV_.get());
        device.setIndicesProperty(procI_.get());
        device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, vertexCount_, 0, triangleCount_);
    }

    // ---- 32-bit indices -------------------------------------------------------------------------
    void DrawIndex32(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& device = ctx.device;
        if (dirty_ || !bigV_) {
            std::vector<Vnt> v;
            std::vector<std::uint32_t> idx;
            BuildGrid(grid_, v, idx, 4.0f);
            Displace(v, 1.0f);
            bigV_ = std::make_unique<VertexBuffer>(device, Vnt::getVertexDeclarationStatic(), static_cast<int>(v.size()), BufferUsage::WriteOnly);
            bigV_->SetData(v.data(), static_cast<int>(v.size()));
            const bool wide = v.size() > 65535;
            bigI_ = std::make_unique<IndexBuffer>(device, wide ? IndexElementSize::ThirtyTwoBits : IndexElementSize::SixteenBits, static_cast<int>(idx.size()), BufferUsage::WriteOnly);
            if (wide) bigI_->SetData(idx.data(), static_cast<int>(idx.size()));
            else {
                std::vector<std::uint16_t> i16(idx.begin(), idx.end());
                bigI_->SetData(i16.data(), static_cast<int>(i16.size()));
            }
            bigTris_ = static_cast<int>(idx.size()) / 3;
            bigCount_ = static_cast<int>(v.size());
            dirty_ = false;
        }
        BasicEffect& fx = Lit(ctx, Matrix::CreateRotationY(spin_ * 0.3f) * Matrix::CreateTranslation(o + Vector3(0, -0.4f, 0)), Vector3(0.45f, 0.85f, 0.5f));
        EffectBank::Apply(fx);
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        device.SetVertexBuffer(bigV_.get());
        device.setIndicesProperty(bigI_.get());
        device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, bigCount_, 0, bigTris_);
    }

    // ---- many buffers ---------------------------------------------------------------------------
    void DrawManyBuffers(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& device = ctx.device;
        if (dirty_ || (small_.empty() && !mergedV_)) {
            small_.clear();
            mergedV_.reset();
            mergedI_.reset();
            const int n = count_;
            // Each object is a little pyramid (4 vertices, 12 indices) at its own place on a spiral.
            const auto make = [&](int k, std::vector<Vnt>& v, std::vector<std::uint16_t>& idx, std::uint16_t base) {
                const float a = static_cast<float>(k) * 0.35f, r = 0.15f * std::sqrt(static_cast<float>(k)) + 0.3f;
                const Vector3 c(std::cos(a) * r, 0.0f, std::sin(a) * r);
                const Vector3 p[4] = {c + Off(-0.07f, 0, -0.07f), c + Off(0.07f, 0, -0.07f), c + Off(0, 0, 0.08f), c + Vector3(0, 0.16f, 0)};
                for (const Vector3& q : p) v.emplace_back(q, Vector3::Normalize(q - c + Vector3(0, 0.05f, 0)), Vector2(0, 0));
                for (const int q : {0, 1, 3, 1, 2, 3, 2, 0, 3, 0, 2, 1}) idx.push_back(static_cast<std::uint16_t>(base + q));
            };
            if (merged_) {
                std::vector<Vnt> v;
                std::vector<std::uint16_t> idx;
                for (int k = 0; k < n; ++k) make(k, v, idx, static_cast<std::uint16_t>(k * 4));
                // 16-bit indices limit a merged buffer to 16 383 objects; the slider stops far below.
                mergedV_ = std::make_unique<VertexBuffer>(device, Vnt::getVertexDeclarationStatic(), static_cast<int>(v.size()), BufferUsage::WriteOnly);
                mergedV_->SetData(v.data(), static_cast<int>(v.size()));
                mergedI_ = std::make_unique<IndexBuffer>(device, IndexElementSize::SixteenBits, static_cast<int>(idx.size()), BufferUsage::WriteOnly);
                mergedI_->SetData(idx.data(), static_cast<int>(idx.size()));
            } else {
                for (int k = 0; k < n; ++k) {
                    std::vector<Vnt> v;
                    std::vector<std::uint16_t> idx;
                    make(k, v, idx, 0);
                    auto vb = std::make_unique<VertexBuffer>(device, Vnt::getVertexDeclarationStatic(), 4, BufferUsage::WriteOnly);
                    vb->SetData(v.data(), 4);
                    small_.push_back(std::move(vb));
                }
                std::vector<std::uint16_t> idx = {0, 1, 3, 1, 2, 3, 2, 0, 3, 0, 2, 1};
                mergedI_ = std::make_unique<IndexBuffer>(device, IndexElementSize::SixteenBits, 12, BufferUsage::WriteOnly);
                mergedI_->SetData(idx.data(), 12);
            }
            dirty_ = false;
        }
        BasicEffect& fx = Lit(ctx, Matrix::CreateRotationY(spin_ * 0.4f) * Matrix::CreateTranslation(o + Vector3(0, -0.5f, 0)), Vector3(0.9f, 0.5f, 0.4f));
        // Every vertex here has texture coordinate (0, 0), so the screen-space derivatives are zero; anisotropic filtering of a zero
        // footprint is undefined and came out black on the software Vulkan/GL drivers. A plain linear sampler has no such case.
        device.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
        EffectBank::Apply(fx);
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        device.setIndicesProperty(mergedI_.get());
        if (merged_) {
            device.SetVertexBuffer(mergedV_.get());
            device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, count_ * 4, 0, count_ * 4);
        } else {
            for (auto& vb : small_) {
                device.SetVertexBuffer(vb.get());
                device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 4);
            }
        }
    }

    // ---- dynamic index buffer -------------------------------------------------------------------
    static constexpr int kDynGrid = 80;
    void DrawDynIndex(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& device = ctx.device;
        if (!dynIndexVertices_) {
            std::vector<std::uint32_t> unused;
            BuildGrid(kDynGrid, dynGridV_, unused, 4.4f);
            for (Vnt& p : dynGridV_) {
                p.Position.Y = Height(p.Position.X * 1.2f, p.Position.Z * 1.2f, 0.0f) * 0.9f;
                const float e = 0.02f;
                const float hx = Height((p.Position.X + e) * 1.2f, p.Position.Z * 1.2f, 0.0f) - Height((p.Position.X - e) * 1.2f, p.Position.Z * 1.2f, 0.0f);
                const float hz = Height(p.Position.X * 1.2f, (p.Position.Z + e) * 1.2f, 0.0f) - Height(p.Position.X * 1.2f, (p.Position.Z - e) * 1.2f, 0.0f);
                p.Normal = Vector3::Normalize(Vector3(-hx * 2.4f, 2.0f * e, -hz * 2.4f));
            }
            dynIndexVertices_ = std::make_unique<VertexBuffer>(device, Vnt::getVertexDeclarationStatic(), kDynGrid * kDynGrid, BufferUsage::WriteOnly);
            dynIndexVertices_->SetData(dynGridV_.data(), kDynGrid * kDynGrid);
            dynIndices_ = std::make_unique<DynamicIndexBuffer>(device, IndexElementSize::SixteenBits, (kDynGrid - 1) * (kDynGrid - 1) * 6, BufferUsage::WriteOnly);
        }
        const float cx = moveWindow_ ? std::sin(time_ * 0.7f) * 1.2f : 0.0f, cz = moveWindow_ ? std::cos(time_ * 0.5f) * 1.2f : 0.0f;
        dynIndexData_.clear();
        for (int z = 0; z + 1 < kDynGrid; ++z)
            for (int x = 0; x + 1 < kDynGrid; ++x) {
                const Vnt& p = dynGridV_[static_cast<std::size_t>(z * kDynGrid + x)];
                const float dx = p.Position.X - cx, dz = p.Position.Z - cz;
                if (dx * dx + dz * dz > revealRadius_ * revealRadius_) continue;
                const std::uint16_t i0 = static_cast<std::uint16_t>(z * kDynGrid + x), i1 = static_cast<std::uint16_t>(i0 + 1), i2 = static_cast<std::uint16_t>(i0 + kDynGrid), i3 = static_cast<std::uint16_t>(i2 + 1);
                for (const std::uint16_t i : {i0, i2, i1, i1, i2, i3}) dynIndexData_.push_back(i);
            }
        drawnTriangles_ = static_cast<int>(dynIndexData_.size()) / 3;
        if (drawnTriangles_ == 0) return;
        dynIndices_->SetData(dynIndexData_.data(), 0, static_cast<int>(dynIndexData_.size()), SetDataOptions::Discard);
        BasicEffect& fx = Lit(ctx, Matrix::CreateTranslation(o + Vector3(0, -0.4f, 0)), Vector3(0.55f, 0.85f, 0.5f));
        EffectBank::Apply(fx);
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        device.SetVertexBuffer(dynIndexVertices_.get());
        device.setIndicesProperty(dynIndices_.get());
        device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, kDynGrid * kDynGrid, 0, drawnTriangles_);
    }

    // ---- instancing -----------------------------------------------------------------------------
    void DrawInstancing(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& device = ctx.device;
        if (dirty_ || !instV_) {
            instances_.resize(static_cast<std::size_t>(count_));
            const int side = std::max(1, static_cast<int>(std::ceil(std::cbrt(static_cast<double>(count_)))));
            for (int k = 0; k < count_; ++k) {
                const int x = k % side, y = (k / side) % side, z = k / (side * side);
                const Vector3 p((static_cast<float>(x) - static_cast<float>(side) * 0.5f) * 0.22f, (static_cast<float>(y) - static_cast<float>(side) * 0.5f) * 0.22f, (static_cast<float>(z) - static_cast<float>(side) * 0.5f) * 0.22f);
                instances_[static_cast<std::size_t>(k)] = Matrix::CreateScale(0.1f) * Matrix::CreateTranslation(p);
            }
            const VertexDeclaration decl(64, {VertexElement(0, VertexElementFormat::Vector4, VertexElementUsage::BlendWeight, 0), VertexElement(16, VertexElementFormat::Vector4, VertexElementUsage::BlendWeight, 1),
                                              VertexElement(32, VertexElementFormat::Vector4, VertexElementUsage::BlendWeight, 2), VertexElement(48, VertexElementFormat::Vector4, VertexElementUsage::BlendWeight, 3)});
            instV_ = std::make_unique<VertexBuffer>(device, decl, count_, BufferUsage::WriteOnly);
            instV_->SetDataRaw(instances_.data(), count_, 64);
            dirty_ = false;
        }
        const Matrix place = Matrix::CreateRotationY(spin_ * 0.5f) * Matrix::CreateTranslation(o + Vector3(0, 0, 0));
        BasicEffect& fx = Lit(ctx, naive_ ? place : place, Vector3(0.45f, 0.8f, 1.0f));
        device.setRasterizerStateProperty(RasterizerState::CullClockwise);
        bool instanced = false;
        if (!naive_ && instanceRefusal_.empty() && device.SupportsCapability(CNA::GraphicsCapability::Instancing)) {
            if (instancingFx_) {
                CompiledInstancing::Apply(*instancingFx_, place * ctx.view.view * ctx.view.projection,
                                          Vector4(0.45f, 0.8f, 1.0f, 1.0f));
            } else {
                EffectBank::Apply(fx);
            }
            const std::vector<VertexBufferBinding> bindings = {VertexBufferBinding(cubeV_.get(), 0, 0), VertexBufferBinding(instV_.get(), 0, 1)};
            device.setIndicesProperty(cubeI_.get());
            device.SetVertexBuffers(bindings);
            {
                const std::vector<VertexBufferBinding> bound = device.GetVertexBuffers();
                boundStreams_ = std::to_string(bound.size()) + " streams: per-vertex, then per-instance (frequency " + (bound.size() == 2 ? std::to_string(bound[1].getInstanceFrequencyProperty()) : std::string("?")) + ")";
                if (!streamsLogged_) { streamsLogged_ = true; Museum::Log("geometry", "GetVertexBuffers while drawing: %s", boundStreams_.c_str()); }
            }
            try {
                if (baseInstance_ && device.SupportsRendererFeatureEXT(CNA::RendererFeature::BaseInstanceDrawing)) {
                    // The same instance stream, started part-way: the first cubes are simply not drawn.
                    const int first = std::min(firstInstance_, count_ - 1);
                    device.DrawInstancedPrimitivesBaseInstanceEXT(PrimitiveType::TriangleList, 0, 0, 24, 0, 12, count_ - first, first);
                } else {
                    device.DrawInstancedPrimitives(PrimitiveType::TriangleList, 0, 0, 24, 0, 12, count_);
                }
                instanced = true;
            } catch (const std::exception& e) {
                // FNA3D reports Instancing but its stock effects declare no instance stream, so the stock-effect route throws.
                instanceRefusal_ = e.what();
                Museum::Log("geometry", "DrawInstancedPrimitives refused: %s", e.what());
            }
            device.SetVertexBuffer(cubeV_.get());
        }
        if (!instanced) {
            device.SetVertexBuffer(cubeV_.get());
            device.setIndicesProperty(cubeI_.get());
            for (int k = 0; k < count_; ++k) {
                fx.setWorldProperty(instances_[static_cast<std::size_t>(k)] * place);
                EffectBank::Apply(fx);
                device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 24, 0, 12);
            }
        }
    }

    Mode mode_;
    Gizmos gizmos_;
    float revealRadius_ = 1.2f;
    bool moveWindow_ = true;
    int drawnTriangles_ = 0;
    std::unique_ptr<VertexBuffer> dynIndexVertices_;
    std::unique_ptr<DynamicIndexBuffer> dynIndices_;
    std::vector<Vnt> dynGridV_;
    std::vector<std::uint16_t> dynIndexData_;
    float time_ = 0.0f, spin_ = 0.0f, speed_ = 1.0f, turn_ = 25.0f, apex_ = 0.9f;
    int cull_ = 1, prim_ = 0, count_ = 8, grid_ = 32, shape_ = 0, dataOptions_ = 0;
    bool dirty_ = true, wire_ = false, merged_ = false, naive_ = false;
    bool baseInstance_ = false;
    std::string boundStreams_ = "-";
    bool streamsLogged_ = false;
    int firstInstance_ = 0;
    std::string instanceRefusal_;   // why DrawInstancedPrimitives was refused on this renderer; empty while it works
    int vertexCount_ = 0, triangleCount_ = 0, bigTris_ = 0, bigCount_ = 0;
    std::unique_ptr<VertexBuffer> cubeV_, flatCube_, sharedCube_, procV_, bigV_, mergedV_, instV_;
    std::unique_ptr<Effect> instancingFx_;
    std::unique_ptr<IndexBuffer> cubeI_, sharedI_, procI_, bigI_, mergedI_, gridIb_;
    std::unique_ptr<DynamicVertexBuffer> dyn_;
    std::vector<Vnt> gridV_;
    std::vector<std::uint32_t> gridI32_;
    std::vector<std::uint32_t> gridI16_;
    std::vector<std::uint16_t> userIdx_;
    bool userDraw_ = false;
    std::vector<std::unique_ptr<VertexBuffer>> small_;
    std::vector<Matrix> instances_;
};

}  // namespace

std::unique_ptr<Exhibit> MakeGeometryLab(const RoomDef& room, Services& services)
{
    return std::make_unique<GeometryLab>(room, services);
}
