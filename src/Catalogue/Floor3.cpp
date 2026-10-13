// Floor 3: three-dimensional fundamentals. Vertex data and buffers, then textures on surfaces.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

RoomSpec Geo(const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis)
{
    RoomSpec r;
    r.key = key;
    r.title = title;
    r.subtitle = subtitle;
    r.exhibit = "geometry";
    r.params = {{"mode", mode}};
    r.width = 13;
    r.depth = 13;
    r.height = 5.2f;
    r.theme = "neon-lab";
    r.apis = std::move(apis);
    return r;
}

WingSpec BuffersWing()
{
    WingSpec w;
    w.name = "Vertices and Buffers";
    w.tagline = "Triangles, indices, primitive types, dynamic and large meshes, instancing";
    w.theme = "corridor-dark";
    w.accent = {0.40f, 0.85f, 0.75f};
    w.spineSide = Wall::East;
    w.rooms = {
        Geo("geo-triangle", "A Triangle and Its Winding", "Vertex order decides which side faces you; RasterizerState.CullMode removes one", "triangle",
            {"VertexPositionColor", "GraphicsDevice.DrawUserPrimitives", "RasterizerState.CullMode"}),
        Geo("geo-indexed", "Vertex and Index Buffers", "One cube drawn without indices, with 8 shared vertices and with 24 lit vertices", "indexed",
            {"VertexBuffer", "IndexBuffer", "GraphicsDevice.DrawIndexedPrimitives", "GraphicsDevice.DrawPrimitives"}),
        Geo("geo-primitives", "Primitive Types", "TriangleList, TriangleStrip, LineList, LineStrip and the CNA point list from the same vertices", "primitives",
            {"PrimitiveType", "GraphicsDevice.DrawUserPrimitives"}),
        Geo("geo-dynamic", "Dynamic Vertex Buffers", "A DynamicVertexBuffer rewritten every frame with Discard, NoOverwrite or None", "dynamic",
            {"DynamicVertexBuffer", "SetDataOptions", "VertexPositionNormalTexture"}),
        Geo("geo-procedural", "Procedural Geometry", "Sphere, torus, cone and height field generated in code, with wireframe", "procedural",
            {"VertexBuffer", "IndexBuffer", "RasterizerState.FillMode"}),
        Geo("geo-index32", "32-Bit Indices", "A grid of up to 160 000 vertices drawn with IndexElementSize.ThirtyTwoBits", "index32", {"IndexBuffer", "IndexElementSize"}),
        Geo("geo-manybuffers", "Many Small Buffers", "Hundreds of VertexBuffers against one merged buffer: the cost of draw calls", "manybuffers", {"VertexBuffer", "GraphicsDevice.DrawIndexedPrimitives"}),
        Geo("geo-instancing", "Hardware Instancing", "DrawInstancedPrimitives with a per-instance world matrix stream, against one call per cube", "instancing",
            {"VertexBufferBinding", "GraphicsDevice.DrawInstancedPrimitives", "GraphicsCapability.Instancing"}),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor3()
{
    FloorSpec f;
    f.index = 3;
    f.name = "Three-Dimensional Fundamentals";
    f.tagline = "Vertex data, buffers, primitives and textures";
    f.theme = "concourse";
    f.accent = {0.40f, 0.85f, 0.75f};
    f.wings.push_back(BuffersWing());
    return f;
}
