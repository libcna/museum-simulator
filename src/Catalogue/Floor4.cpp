// Floor 4: effects and lighting. BasicEffect and the other stock effects, then custom shaders.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec ShaderWing()
{
    WingSpec w;
    w.name = "Shader Laboratory";
    w.tagline = "CNA extension: GLSL fragment and vertex shaders through ShaderEffect and SpriteBatch";
    w.theme = "corridor-dark";
    w.accent = {0.70f, 0.45f, 1.0f};
    w.spineSide = Wall::East;
    const auto sh = [](const char* key, const char* title, const char* subtitle, const char* shader) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "shaders";
        r.params = {{"shader", shader}};
        r.width = 14;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "neon-lab";
        r.apis = {"ShaderEffect", "ShaderEffect.SetUniformFloat", "SpriteBatch.Begin(effect)", "RenderTarget2D"};
        r.origin = ApiOrigin::CnaExtension;
        return r;
    };
    w.rooms = {
        sh("fx-grade", "Colour Grading", "A fragment shader that grays, tints, inverts or two-tones the whole scene", "grade"),
        sh("fx-blur", "Blur", "A 5 x 5 box blur driven by one uniform, applied to a rendered 3D scene", "blur"),
        sh("fx-pixelate", "Pixelate and Posterise", "Snap texture coordinates and quantise colours for a retro look", "pixelate"),
        sh("fx-ripple", "Water Ripple", "UV distortion animated by a time uniform", "ripple"),
        sh("fx-edge", "Edge Detection", "A Sobel filter built from eight extra texture reads per pixel", "edge"),
        sh("fx-crt", "CRT Look", "Scanlines, vignette and barrel distortion in one pass", "crt"),
        sh("fx-plasma", "Procedural Shader", "A colour computed from math alone, mixed over the scene", "plasma"),
        sh("fx-vertexwave", "Vertex Shader", "The vertex stage moving a sprite's corners with a sine wave", "vertexwave"),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor4()
{
    FloorSpec f;
    f.index = 4;
    f.name = "Effects and Lighting";
    f.tagline = "BasicEffect, the stock effects, lighting and custom shaders";
    f.theme = "concourse";
    f.accent = {0.70f, 0.45f, 1.0f};
    f.wings.push_back(ShaderWing());
    return f;
}
