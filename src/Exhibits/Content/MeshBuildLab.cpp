// Mesh build laboratory: XNA's content-pipeline MeshBuilder and ModelProcessor running in the game. A bipyramid ("gem") is described
// in code (positions, a normal channel, a texture-coordinate channel, triangles), MeshBuilder.FinishMesh turns it into MeshContent, and
// ModelProcessor.Process turns that into ModelContent: a bone tree, a mesh and a part with a vertex buffer, an index list and a bounding
// sphere. The museum reads the part back and draws it, so every option changes the model that stands on the plinth.
// Without CNA's pipeline library (-DMUSEUM_PIPELINE=OFF) the room says so.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Exhibits/ExhibitFactories.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/RenderStates.hpp"

#ifdef MUSEUM_PIPELINE
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/MeshBuilder.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/NodeContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/VertexChannelNames.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/ModelContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/ModelProcessor.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/VertexBufferContent.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "System/NotSupportedException.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

#ifdef MUSEUM_PIPELINE
namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace PipeGfx = Microsoft::Xna::Framework::Content::Pipeline::Graphics;
namespace Processors = Microsoft::Xna::Framework::Content::Pipeline::Processors;
using PipelineSupport::ProcessorContext;

#endif

class MeshBuildLab final : public Exhibit {
public:
    MeshBuildLab(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("MeshBuilder and ModelProcessor", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_PIPELINE
        p.AddSlider("Sides of the gem", 3.0f, 24.0f, sides_, [this](float v) { sides_ = v; Build(); }, "%.0f", 1.0f);
        p.AddToggle("MeshBuilder.MergeDuplicatePositions", false, [this](bool v) { merge_ = v; Build(); });
        p.AddToggle("MeshBuilder.SwapWindingOrder", false, [this](bool v) { swapBuilder_ = v; Build(); });
        p.AddSlider("ModelProcessor.Scale", 0.5f, 2.0f, scale_, [this](float v) { scale_ = v; Build(); }, "%.2f", 0.05f);
        p.AddSlider("ModelProcessor.RotationY (degrees)", 0.0f, 90.0f, rotation_, [this](float v) { rotation_ = v; Build(); }, "%.0f", 5.0f);
        p.AddToggle("ModelProcessor.GenerateTangentFrames", false, [this](bool v) { tangents_ = v; Build(); });
        p.AddToggle("ModelProcessor.SwapWindingOrder", false, [this](bool v) { swapProcessor_ = v; Build(); });
        p.AddReadout("Positions created", [this] { return created_; });
        p.AddReadout("Positions kept", [this] { return finished_; });
        p.AddReadout("Bones/meshes/parts", [this] { return shape_; });
        p.AddReadout("Part", [this] { return part_; });
        p.AddReadout("Elements", [this] { return elements_; });
        p.AddReadout("Sphere radius", [this] { return sphere_; });
        p.AddReadout("Convert calls", [this] { return converted_; });
        p.AddReadout("Build", [this] { return status_; });
#else
        p.AddReadout("Content pipeline", [] { return std::string("not built into this museum"); });
#endif
        MathKit::BuildStagePlinth(b, 0.0f, 0.0f, 0.4f);
        SignSpec what = Kit::Plate("Building a Model in Code",
                                   {"XNA's content pipeline can make a model without any file: MeshBuilder collects positions, vertex channels and triangles, FinishMesh gives MeshContent, and ModelProcessor",
                                    "turns that into ModelContent (vertex buffer, indices, bounding sphere) with the same options a content project has. Here that runs in the game and the result is drawn."});
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void OnEnter() override
    {
#ifdef MUSEUM_PIPELINE
        if (!built_) Build();
#endif
    }

    void Update(const UpdateContext& ctx) override
    {
        if (ctx.nearby) time_ += ctx.dt;
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
#ifdef MUSEUM_PIPELINE
        if (!built_) Build();
        if (vertices_.empty() || indices_.empty()) return;
        GraphicsDevice& d = ctx.device;
        RenderStates::SetOpaque3D(d);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        BasicEffect& fx = ctx.fx.BeginLit();
        fx.EnableDefaultLighting();
        fx.setPunctualLightEXT(PunctualLightEXT{});
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setWorldProperty(Matrix::CreateRotationY(time_ * 0.6f) * Matrix::CreateTranslation(R().ToWorld(0.0f, 1.45f, 0.0f)));
        fx.setDiffuseColorProperty(Vector3(0.45f, 0.75f, 0.95f));
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setSpecularColorProperty(Vector3(0.5f, 0.5f, 0.5f));
        EffectBank::Apply(fx);
        d.DrawUserIndexedPrimitives(PrimitiveType::TriangleList, vertices_.data(), 0, static_cast<int>(vertices_.size()), indices_.data(), 0, static_cast<int>(indices_.size()) / 3);
        RenderStates::SetOpaque3D(d);
#endif
    }

private:
#ifdef MUSEUM_PIPELINE
    // The gem: `sides` triangles around an upper apex and `sides` around a lower one. Every triangle gets its own three positions (a
    // flat-shaded model), so MergeDuplicatePositions has something to merge.
    void Build()
    {
        built_ = true;
        vertices_.clear();
        indices_.clear();
        try {
            const int n = static_cast<int>(sides_);
            const std::shared_ptr<PipeGfx::MeshBuilder> builder = PipeGfx::MeshBuilder::StartMesh("Gem");
            builder->setMergeDuplicatePositionsProperty(merge_);
            builder->setSwapWindingOrderProperty(swapBuilder_);
            const int normals = builder->CreateVertexChannel<Vector3>(PipeGfx::VertexChannelNames::Normal());
            const int coords = builder->CreateVertexChannel<Vector2>(PipeGfx::VertexChannelNames::TextureCoordinate(0));
            int created = 0;
            const auto corner = [&](const Vector3& position, const Vector3& normal, const Vector2& uv) {
                builder->SetVertexChannelData(normals, Xna::Box<Vector3>(normal));
                builder->SetVertexChannelData(coords, Xna::Box<Vector2>(uv));
                builder->AddTriangleVertex(builder->CreatePosition(position));
                ++created;
            };
            const Vector3 top(0.0f, 1.0f, 0.0f), bottom(0.0f, -1.0f, 0.0f);
            for (int i = 0; i < n; ++i) {
                const float a0 = MathHelper::TwoPi * static_cast<float>(i) / static_cast<float>(n), a1 = MathHelper::TwoPi * static_cast<float>((i + 1) % n) / static_cast<float>(n);
                const Vector3 p0(std::cos(a0) * 0.8f, 0.0f, std::sin(a0) * 0.8f), p1(std::cos(a1) * 0.8f, 0.0f, std::sin(a1) * 0.8f);
                // Counter-clockwise seen from outside; the face normal is the cross product of two edges.
                const Vector3 upper = Vector3::Normalize(Vector3::Cross(p1 - top, p0 - top));
                corner(top, upper, Vector2(0.5f, 0.0f));
                corner(p1, upper, Vector2(1.0f, 1.0f));
                corner(p0, upper, Vector2(0.0f, 1.0f));
                const Vector3 lower = Vector3::Normalize(Vector3::Cross(p0 - bottom, p1 - bottom));
                corner(bottom, lower, Vector2(0.5f, 0.0f));
                corner(p0, lower, Vector2(0.0f, 1.0f));
                corner(p1, lower, Vector2(1.0f, 1.0f));
            }
            const std::shared_ptr<PipeGfx::MeshContent> mesh = builder->FinishMesh();
            created_ = std::to_string(created) + " (per corner)";
            finished_ = std::to_string(mesh->getPositionsProperty().getCountProperty()) + (merge_ ? " (merged)" : " (not merged)");

            Processors::ModelProcessor processor;
            processor.setScaleProperty(scale_);
            processor.setRotationYProperty(rotation_);
            processor.setGenerateTangentFramesProperty(tangents_);
            processor.setSwapWindingOrderProperty(swapProcessor_);
            ProcessorContext context;
            const std::shared_ptr<Processors::ModelContent> model = processor.Process(mesh, context);
            int parts = 0;
            for (const auto& m : model->getMeshesProperty()) parts += static_cast<int>(m->getMeshPartsProperty().size());
            converted_ = std::to_string(context.conversions) + (context.conversions > 0 ? " (" + context.lastProcessor + ")" : std::string());
            shape_ = std::to_string(model->getBonesProperty().size()) + " / " + std::to_string(model->getMeshesProperty().size()) + " / " + std::to_string(parts);
            const PipelineSupport::ProcessedGeometry geometry = PipelineSupport::ReadFirstPart(*model);
            part_ = std::to_string(geometry.vertices.size()) + " vertices, " + std::to_string(geometry.triangles) + " triangles, stride " + std::to_string(geometry.stride) + " bytes";
            elements_ = geometry.elements;
            char radius[32];
            std::snprintf(radius, sizeof radius, "%.3f", static_cast<double>(geometry.radius));
            sphere_ = radius;
            vertices_ = geometry.vertices;
            indices_ = geometry.indices;
            status_ = "drawn from ModelContent";
        } catch (const std::exception& e) {
            vertices_.clear();
            indices_.clear();
            status_ = std::string("could not build: ") + std::string(e.what()).substr(0, 80);
            Museum::Log("meshbuild", "%s", e.what());
        }
    }

    std::vector<VertexPositionNormalTexture> vertices_;
    std::vector<std::uint16_t> indices_;
    float sides_ = 8.0f, scale_ = 1.0f, rotation_ = 0.0f;
    bool merge_ = false, swapBuilder_ = false, tangents_ = false, swapProcessor_ = false, built_ = false;
    std::string created_ = "-", finished_ = "-", shape_ = "-", part_ = "-", elements_ = "-", sphere_ = "-", converted_ = "-", status_ = "-";
#endif
    float time_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeMeshBuildLab(const RoomDef& room, Services& services)
{
    return std::make_unique<MeshBuildLab>(room, services);
}
