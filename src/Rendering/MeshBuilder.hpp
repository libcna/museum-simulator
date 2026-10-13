#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

// A point light used only to bake per-vertex lighting into the museum's static
// architecture. Dynamic exhibits are lit at draw time by BasicEffect instead.
struct BakedLight {
    Microsoft::Xna::Framework::Vector3 position;
    Microsoft::Xna::Framework::Vector3 color{1.0f, 0.95f, 0.85f};
    float intensity = 1.0f;
    float radius = 9.0f;
};

enum class BoxFaces : unsigned {
    PosX = 1, NegX = 2, PosY = 4, NegY = 8, PosZ = 16, NegZ = 32,
    All = 63, Sides = 1 | 2 | 16 | 32, NoBottom = 63 & ~8, NoTop = 63 & ~4,
};
constexpr BoxFaces operator|(BoxFaces a, BoxFaces b)
{
    return static_cast<BoxFaces>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
constexpr BoxFaces operator&(BoxFaces a, BoxFaces b)
{
    return static_cast<BoxFaces>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}
constexpr BoxFaces operator~(BoxFaces a)
{
    return static_cast<BoxFaces>(~static_cast<unsigned>(a) & 63U);
}
constexpr bool Has(BoxFaces set, BoxFaces f)
{
    return (static_cast<unsigned>(set) & static_cast<unsigned>(f)) != 0;
}

/**
 * Accumulates lit, textured triangles on the CPU. Triangles are counter-clockwise
 * seen from outside (the glTF convention), so everything in the museum is drawn
 * with RasterizerState::CullClockwise.
 *
 * Surfaces are tessellated into small cells because lighting is baked per vertex:
 * a single quad per wall would light only its four corners.
 */
class MeshBuilder {
public:
    struct Vertex {
        Microsoft::Xna::Framework::Vector3 position;
        Microsoft::Xna::Framework::Vector3 normal;
        Microsoft::Xna::Framework::Vector2 uv;
        Microsoft::Xna::Framework::Vector3 albedo;  // linear 0..1 tint
        float emissive = 0.0f;                      // 1 = ignore lighting
    };

    /**
     * origin + u*U + v*V, with the front face toward cross(U, V).
     * `texMeters` > 0 maps one texture repeat to that many metres; 0 maps the whole patch to 0..1.
     */
    void AddPatch(const Microsoft::Xna::Framework::Vector3& origin,
                  const Microsoft::Xna::Framework::Vector3& U,
                  const Microsoft::Xna::Framework::Vector3& V, float texMeters,
                  const Microsoft::Xna::Framework::Vector3& tint, float maxCell = 1.0f,
                  float emissive = 0.0f);

    void AddBox(const Microsoft::Xna::Framework::Vector3& min,
                const Microsoft::Xna::Framework::Vector3& max, float texMeters,
                const Microsoft::Xna::Framework::Vector3& tint, BoxFaces faces = BoxFaces::All,
                float maxCell = 1.0f, float emissive = 0.0f);

    /** Vertical cylinder with smooth side normals. */
    void AddCylinder(const Microsoft::Xna::Framework::Vector3& baseCentre, float radius, float height,
                     int segments, float texMeters, const Microsoft::Xna::Framework::Vector3& tint,
                     bool caps = true, float emissive = 0.0f);

    /** UV sphere; normals point outward, or inward when `inside` is set (sky domes). */
    void AddSphere(const Microsoft::Xna::Framework::Vector3& centre, float radius, int slices, int stacks,
                   const Microsoft::Xna::Framework::Vector3& tint, bool inside = false,
                   float emissive = 0.0f);

    /** Vertical cone/frustum, top radius may be zero. */
    void AddCone(const Microsoft::Xna::Framework::Vector3& baseCentre, float baseRadius, float topRadius,
                 float height, int segments, const Microsoft::Xna::Framework::Vector3& tint,
                 float emissive = 0.0f);

    int AddVertex(const Vertex& v);
    void AddTriangle(int a, int b, int c);

    [[nodiscard]] bool Empty() const { return indices_.empty(); }
    [[nodiscard]] std::size_t VertexCount() const { return vertices_.size(); }
    [[nodiscard]] const std::vector<Vertex>& Vertices() const { return vertices_; }
    [[nodiscard]] const std::vector<std::uint16_t>& Indices() const { return indices_; }
    [[nodiscard]] Microsoft::Xna::Framework::BoundingBox Bounds() const;

    /** Bakes lighting into vertex colours: ambient + sum of the lights (half-Lambert, smooth falloff). */
    [[nodiscard]] std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColorTexture>
    Bake(const std::vector<BakedLight>& lights, const Microsoft::Xna::Framework::Vector3& ambient) const;

private:
    std::vector<Vertex> vertices_;
    std::vector<std::uint16_t> indices_;
};

/** One MeshBuilder per texture name, so a room draws with one call per texture. */
class MeshSet {
public:
    MeshBuilder& Opaque(const std::string& texture) { return opaque_[texture]; }
    MeshBuilder& Transparent(const std::string& texture) { return transparent_[texture]; }
    [[nodiscard]] const std::map<std::string, MeshBuilder>& OpaqueMeshes() const { return opaque_; }
    [[nodiscard]] const std::map<std::string, MeshBuilder>& TransparentMeshes() const { return transparent_; }

private:
    std::map<std::string, MeshBuilder> opaque_;
    std::map<std::string, MeshBuilder> transparent_;
};
