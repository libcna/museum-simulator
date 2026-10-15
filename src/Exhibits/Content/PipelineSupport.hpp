#pragma once

// What the content-pipeline rooms (processors, font build, mesh build, model import) need to run CNA's pipeline classes inside the game: a
// build logger that stays quiet, an importer and a processor context (the two things a build normally provides), and a reader that turns a
// processed ModelContent part back into vertices the museum can draw. All of it is guarded by MUSEUM_PIPELINE like the rooms themselves.
#ifdef MUSEUM_PIPELINE

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentBuildLogger.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentImporterContext.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentProcessorContext.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/NodeContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/StockMaterials.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/ModelContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/ModelProcessor.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/VertexBufferContent.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "System/IO/MemoryStream.hpp"
#include "System/NotSupportedException.hpp"

namespace PipelineSupport {

namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;

class QuietLogger final : public Xna::ContentBuildLogger {
protected:
    void LogMessage(const std::string&) override {}
    void LogImportantMessage(const std::string&) override {}
    void LogWarning(const std::string& help, const Xna::ContentIdentity&, const std::string& message) override
    {
        (void)help;
        ++warnings;
        lastWarning = message;
    }

public:
    int warnings = 0;
    std::string lastWarning;
};

// What an importer asks of the build: directories, a logger, nothing nested.
class ImporterContext final : public Xna::ContentImporterContext {
public:
    [[nodiscard]] std::string getIntermediateDirectoryProperty() const override { return "obj"; }
    [[nodiscard]] Xna::ContentBuildLogger& getLoggerProperty() const override { return const_cast<QuietLogger&>(logger_); }
    [[nodiscard]] std::string getOutputDirectoryProperty() const override { return "bin"; }
    void AddDependency(const std::string&) override {}
    [[nodiscard]] int warnings() const { return logger_.warnings; }
    [[nodiscard]] const std::string& lastWarning() const { return logger_.lastWarning; }

private:
    QuietLogger logger_;
};

// What a processor asks of the build. Building other assets is not offered; Convert (which ModelProcessor calls for every material) answers
// with the material it was given and counts the request.
class ProcessorContext final : public Xna::ContentProcessorContext {
public:
    // A processor may write a file (SongProcessor writes its converted song): that goes to a folder of its own under the system's
    // temporary directory, never into the folder the museum was started from.
    ProcessorContext() { std::error_code ignored; std::filesystem::create_directories(OutputFolder(), ignored); }
    [[nodiscard]] static std::string OutputFolder() { return (std::filesystem::temp_directory_path() / "cna-museum-pipeline").string(); }
    [[nodiscard]] std::string getBuildConfigurationProperty() const override { return "Release"; }
    [[nodiscard]] std::string getIntermediateDirectoryProperty() const override { return OutputFolder(); }
    [[nodiscard]] Xna::ContentBuildLogger& getLoggerProperty() const override { return const_cast<QuietLogger&>(logger_); }
    [[nodiscard]] std::string getOutputDirectoryProperty() const override { return OutputFolder(); }
    [[nodiscard]] std::string getOutputFilenameProperty() const override { return OutputFolder() + "/asset.xnb"; }
    [[nodiscard]] const Xna::OpaqueDataDictionary& getParametersProperty() const override { return parameters_; }
    [[nodiscard]] Xna::TargetPlatform getTargetPlatformProperty() const override { return Xna::TargetPlatform::Windows; }
    [[nodiscard]] Microsoft::Xna::Framework::Graphics::GraphicsProfile getTargetProfileProperty() const override { return Microsoft::Xna::Framework::Graphics::GraphicsProfile::HiDef; }
    void AddDependency(const std::string&) override {}
    void AddOutputFile(const std::string&) override {}
    [[nodiscard]] Xna::ContentObject BuildAndLoadAssetCore(const std::string&, const Xna::ContentIdentity&, const std::string&, const Xna::OpaqueDataDictionary&, const std::string&,
                                                           const std::string&, const std::string&) override
    {
        throw System::NotSupportedException("BuildAndLoadAsset");
    }
    [[nodiscard]] std::string BuildAssetCore(const std::string&, const Xna::ContentIdentity&, const std::string&, const Xna::OpaqueDataDictionary&, const std::string&, const std::string&,
                                             const std::string&, const std::string&) override
    {
        throw System::NotSupportedException("BuildAsset");
    }
    [[nodiscard]] Xna::ContentObject ConvertCore(const Xna::ContentObject& input, const std::string& processor, const Xna::OpaqueDataDictionary&, const std::string&, const std::string&) override
    {
        ++conversions;
        lastProcessor = processor;
        return input;
    }
    [[nodiscard]] int warnings() const { return logger_.warnings; }

    mutable int conversions = 0;
    mutable std::string lastProcessor;

private:
    QuietLogger logger_;
    Xna::OpaqueDataDictionary parameters_;
};

// A ContentManager whose one asset lives in a byte vector instead of a file: the OpenStream seam every XNA manager reads through, so compiled
// bytes can be loaded back without touching the disk.
class MemoryContentManager final : public Microsoft::Xna::Framework::Content::ContentManager {
public:
    explicit MemoryContentManager(System::IServiceProvider* services) : ContentManager(services) { RouteLoadsThroughOpenStreamEXT(true); }
    std::vector<std::uint8_t> bytes;

protected:
    [[nodiscard]] std::unique_ptr<System::IO::Stream> OpenStream(const std::string&) override
    {
        return std::make_unique<System::IO::MemoryStream>(reinterpret_cast<const SharpRuntime::bytecs*>(bytes.data()), static_cast<SharpRuntime::intcs>(bytes.size()), false);
    }
};

inline const char* UsageName(Microsoft::Xna::Framework::Graphics::VertexElementUsage u)
{
    using Microsoft::Xna::Framework::Graphics::VertexElementUsage;
    switch (u) {
        case VertexElementUsage::Position: return "Position";
        case VertexElementUsage::Normal: return "Normal";
        case VertexElementUsage::TextureCoordinate: return "TexCoord";
        case VertexElementUsage::Tangent: return "Tangent";
        case VertexElementUsage::Binormal: return "Binormal";
        case VertexElementUsage::Color: return "Color";
        default: return "other";
    }
}

// The first part of the first mesh of a processed model, copied into the museum's vertex type through the offsets the part's own declaration
// gives, plus the numbers a room shows about it. Throws with a readable message when the layout cannot be read.
struct ProcessedGeometry {
    std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionNormalTexture> vertices;
    std::vector<std::uint16_t> indices;
    int bones = 0, meshes = 0, parts = 0, stride = 0, triangles = 0;
    float radius = 0.0f;
    std::string elements;
};

inline ProcessedGeometry ReadFirstPart(const Xna::Processors::ModelContent& model)
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Graphics;
    ProcessedGeometry out;
    out.bones = static_cast<int>(model.getBonesProperty().size());
    out.meshes = static_cast<int>(model.getMeshesProperty().size());
    for (const auto& m : model.getMeshesProperty()) out.parts += static_cast<int>(m->getMeshPartsProperty().size());
    if (model.getMeshesProperty().empty() || model.getMeshesProperty()[0]->getMeshPartsProperty().empty()) throw std::runtime_error("the processor produced no mesh part");
    const auto& mesh = model.getMeshesProperty()[0];
    const auto& part = mesh->getMeshPartsProperty()[0];
    const auto& buffer = *part->getVertexBufferProperty();
    const auto& declaration = *buffer.getVertexDeclarationProperty();
    out.stride = declaration.getVertexStrideProperty().value_or(0);
    out.triangles = part->getPrimitiveCountProperty();
    out.radius = mesh->getBoundingSphereProperty().Radius;
    int positionAt = -1, normalAt = -1, coordAt = -1;
    for (const VertexElement& e : declaration.getVertexElementsProperty()) {
        out.elements += (out.elements.empty() ? "" : ", ") + std::string(UsageName(e.getVertexElementUsageProperty())) + "@" + std::to_string(e.getOffsetProperty());
        if (e.getUsageIndexProperty() != 0) continue;
        if (e.getVertexElementUsageProperty() == VertexElementUsage::Position) positionAt = e.getOffsetProperty();
        if (e.getVertexElementUsageProperty() == VertexElementUsage::Normal) normalAt = e.getOffsetProperty();
        if (e.getVertexElementUsageProperty() == VertexElementUsage::TextureCoordinate) coordAt = e.getOffsetProperty();
    }
    if (positionAt < 0 || out.stride <= 0) throw std::runtime_error("the vertex layout has no Position");
    const std::vector<SharpRuntime::bytecs>& bytes = buffer.getVertexDataProperty();
    const int first = part->getVertexOffsetProperty(), count = part->getNumVerticesProperty();
    for (int v = 0; v < count; ++v) {
        const std::size_t base = static_cast<std::size_t>(first + v) * static_cast<std::size_t>(out.stride);
        if (base + static_cast<std::size_t>(out.stride) > bytes.size()) throw std::runtime_error("the vertex data is shorter than its declaration says");
        float f[8] = {0, 1, 0, 0, 0, 0, 0, 0};
        std::memcpy(f, bytes.data() + base + static_cast<std::size_t>(positionAt), 12);
        if (normalAt >= 0) std::memcpy(f + 3, bytes.data() + base + static_cast<std::size_t>(normalAt), 12);
        if (coordAt >= 0) std::memcpy(f + 6, bytes.data() + base + static_cast<std::size_t>(coordAt), 8);
        out.vertices.emplace_back(Vector3(f[0], f[1], f[2]), Vector3(f[3], f[4], f[5]), Vector2(f[6], f[7]));
    }
    const auto& index = *part->getIndexBufferProperty();
    for (int i = part->getStartIndexProperty(); i < part->getStartIndexProperty() + out.triangles * 3; ++i) out.indices.push_back(static_cast<std::uint16_t>(index[i]));
    return out;
}


// Every part of every mesh of a processed model, each with the colour of its material and the transform of the bone its mesh hangs on.
struct ProcessedPart {
    std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionNormalTexture> vertices;
    std::vector<std::uint16_t> indices;
    Microsoft::Xna::Framework::Vector3 diffuse{0.7f, 0.7f, 0.7f};
    Microsoft::Xna::Framework::Matrix world = Microsoft::Xna::Framework::Matrix::getIdentityProperty();
    std::string mesh;
};

inline std::vector<ProcessedPart> ReadAllParts(const Xna::Processors::ModelContent& model)
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Graphics;
    std::vector<ProcessedPart> result;
    for (const auto& mesh : model.getMeshesProperty()) {
        Matrix world = Matrix::getIdentityProperty();
        for (std::shared_ptr<Xna::Processors::ModelBoneContent> bone = mesh->getParentBoneProperty(); bone != nullptr; bone = bone->getParentProperty()) world = world * bone->getTransformProperty();
        for (const auto& part : mesh->getMeshPartsProperty()) {
            ProcessedPart p;
            p.world = world;
            p.mesh = mesh->getNameProperty();
            if (const auto basic = std::dynamic_pointer_cast<Xna::Graphics::BasicMaterialContent>(part->getMaterialProperty()); basic != nullptr && basic->getDiffuseColorProperty().has_value()) p.diffuse = *basic->getDiffuseColorProperty();
            const auto& buffer = *part->getVertexBufferProperty();
            const auto& declaration = *buffer.getVertexDeclarationProperty();
            const int stride = declaration.getVertexStrideProperty().value_or(0);
            int positionAt = -1, normalAt = -1, coordAt = -1;
            for (const VertexElement& e : declaration.getVertexElementsProperty()) {
                if (e.getUsageIndexProperty() != 0) continue;
                if (e.getVertexElementUsageProperty() == VertexElementUsage::Position) positionAt = e.getOffsetProperty();
                if (e.getVertexElementUsageProperty() == VertexElementUsage::Normal) normalAt = e.getOffsetProperty();
                if (e.getVertexElementUsageProperty() == VertexElementUsage::TextureCoordinate) coordAt = e.getOffsetProperty();
            }
            if (positionAt < 0 || stride <= 0) continue;
            const std::vector<SharpRuntime::bytecs>& bytes = buffer.getVertexDataProperty();
            for (int v = 0; v < part->getNumVerticesProperty(); ++v) {
                const std::size_t base = static_cast<std::size_t>(part->getVertexOffsetProperty() + v) * static_cast<std::size_t>(stride);
                if (base + static_cast<std::size_t>(stride) > bytes.size()) throw std::runtime_error("the vertex data is shorter than its declaration says");
                float f[8] = {0, 1, 0, 0, 0, 0, 0, 0};
                std::memcpy(f, bytes.data() + base + static_cast<std::size_t>(positionAt), 12);
                if (normalAt >= 0) std::memcpy(f + 3, bytes.data() + base + static_cast<std::size_t>(normalAt), 12);
                if (coordAt >= 0) std::memcpy(f + 6, bytes.data() + base + static_cast<std::size_t>(coordAt), 8);
                p.vertices.emplace_back(Vector3(f[0], f[1], f[2]), Vector3(f[3], f[4], f[5]), Vector2(f[6], f[7]));
            }
            const auto& index = *part->getIndexBufferProperty();
            for (int i = part->getStartIndexProperty(); i < part->getStartIndexProperty() + part->getPrimitiveCountProperty() * 3; ++i) p.indices.push_back(static_cast<std::uint16_t>(index[i]));
            result.push_back(std::move(p));
        }
    }
    return result;
}

// The node tree an importer returned, one line per node: name, kind and, for a mesh, how many positions and geometry batches it holds.
inline void DescribeTree(const std::shared_ptr<Xna::Graphics::NodeContent>& node, int depth, std::vector<std::string>& lines, int& meshes, int& geometries, int& positions)
{
    std::string kind = "node";
    if (const auto mesh = std::dynamic_pointer_cast<Xna::Graphics::MeshContent>(node); mesh != nullptr) {
        kind = "mesh, " + std::to_string(mesh->getPositionsProperty().getCountProperty()) + " positions, " + std::to_string(mesh->getGeometryProperty().getCountProperty()) + " batches";
        ++meshes;
        geometries += mesh->getGeometryProperty().getCountProperty();
        positions += mesh->getPositionsProperty().getCountProperty();
    }
    lines.push_back(std::string(static_cast<std::size_t>(depth) * 2, ' ') + (node->getNameProperty().empty() ? "(unnamed)" : node->getNameProperty()) + " (" + kind + ")");
    for (int i = 0; i < node->getChildrenProperty().getCountProperty(); ++i) DescribeTree(node->getChildrenProperty()[i], depth + 1, lines, meshes, geometries, positions);
}

}  // namespace PipelineSupport

#endif  // MUSEUM_PIPELINE
