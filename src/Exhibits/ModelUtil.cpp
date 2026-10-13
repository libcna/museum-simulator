#include "Exhibits/ModelUtil.hpp"

#include <set>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelBone.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/PbrEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Rendering/EffectBank.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

void ModelUtil::DescribeOnce(const std::string& name, Model& model)
{
    static std::set<std::string> seen;
    if (!seen.insert(name).second) return;
    for (ModelMesh* mesh : model.getMeshesProperty()) {
        for (int p = 0; p < mesh->getMeshPartsProperty().getCountProperty(); ++p) {
            ModelMeshPart* part = mesh->getMeshPartsProperty()[p];
            Effect* fx = part->getEffectProperty();
            if (part->getVertexBufferProperty() != nullptr) {
                const VertexDeclaration& decl = part->getVertexBufferProperty()->getVertexDeclarationProperty();
                std::string layout = "stride " + std::to_string(decl.getVertexStrideProperty()) + ":";
                for (const VertexElement& e : decl.GetVertexElements())
                    layout += " [off " + std::to_string(e.getOffsetProperty()) + " fmt " +
                              std::to_string(static_cast<int>(e.getVertexElementFormatProperty())) + " usage " +
                              std::to_string(static_cast<int>(e.getVertexElementUsageProperty())) + "]";
                Museum::Log("models", "%s: %s", name.c_str(), layout.c_str());
            }
            Museum::Log("models", "%s: mesh '%s' part %d uses %s (%d verts, %d tris)", name.c_str(),
                        mesh->getNameProperty().c_str(), p, fx ? fx->GetTypeName().c_str() : "no effect",
                        part->getNumVerticesProperty(), part->getPrimitiveCountProperty());
        }
    }
}

void ModelUtil::DrawWithEffect(GraphicsDevice& device, Model& model, BasicEffect& effect, const Matrix& world,
                               const Vector3* diffuseOverride)
{
    std::vector<Matrix> bones(static_cast<std::size_t>(model.getBonesProperty().getCountProperty()));
    model.CopyAbsoluteBoneTransformsTo(bones);
    for (ModelMesh* mesh : model.getMeshesProperty()) {
        const ModelBone* parent = mesh->getParentBoneProperty();
        const Matrix bone = parent != nullptr ? bones[static_cast<std::size_t>(parent->getIndexProperty())] : Matrix::getIdentityProperty();
        effect.setWorldProperty(bone * world);
        for (int p = 0; p < mesh->getMeshPartsProperty().getCountProperty(); ++p) {
            ModelMeshPart* part = mesh->getMeshPartsProperty()[p];
            if (diffuseOverride != nullptr) {
                effect.setDiffuseColorProperty(*diffuseOverride);
            } else if (auto* pbr = dynamic_cast<PbrEffect*>(part->getEffectProperty())) {
                effect.setDiffuseColorProperty(pbr->getDiffuseColorProperty());
            } else if (auto* basic = dynamic_cast<BasicEffect*>(part->getEffectProperty())) {
                effect.setDiffuseColorProperty(basic->getDiffuseColorProperty());
            }
            EffectBank::Apply(effect);
            device.SetVertexBuffer(part->getVertexBufferProperty(), part->getVertexOffsetProperty());
            device.setIndicesProperty(part->getIndexBufferProperty());
            device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, part->getNumVerticesProperty(),
                                         part->getStartIndexProperty(), part->getPrimitiveCountProperty());
        }
    }
}
