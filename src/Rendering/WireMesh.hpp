#pragma once

#include <memory>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"

/**
 * The unique edges of a model as a static LineList. This is the renderer-independent stand-in
 * for FillMode.WireFrame, used when the active renderer reports no polygon wireframe mode
 * (GraphicsCapability::WireFrame is false on the OpenGL ES renderers).
 */
class WireMesh {
public:
    /** Reads the model's vertices and indices back from the GPU. Returns null if the model has no usable part. */
    static std::unique_ptr<WireMesh> FromModel(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                               Microsoft::Xna::Framework::Graphics::Model& model,
                                               const Microsoft::Xna::Framework::Color& colour);

    /** Draws with whatever effect the caller applied (it must expect VertexPositionColor). */
    void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device) const;
    [[nodiscard]] int EdgeCount() const { return edges_; }

private:
    WireMesh() = default;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> buffer_;
    int edges_ = 0;
};
