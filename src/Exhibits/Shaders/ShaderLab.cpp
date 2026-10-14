// Shader laboratory (CNA extension): custom GLSL fragment and vertex shaders run through CNA's ShaderEffect and
// SpriteBatch. A 3D scene is rendered into a render target, then drawn to the screen through the effect: the
// post-processing workflow, written entirely against CNA's public API. Needs a renderer that executes GLSL
// source (the OpenGL family); other renderers show the unprocessed scene and say why.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "CNA/ShaderLanguageEXT.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Exhibits/Shaders/ShaderSources.hpp"
#include "Microsoft/Xna/Framework/Graphics/ColorMatrixEffect.hpp"
#include "Rendering/Probes.hpp"
#include "Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Rendering/MiniView.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

// SpriteBatch's vertex layout: location 0 = pixel position, 1 = texcoord, 2 = colour; `projection` is set by CNA.
const char* kVertex = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
uniform mat4 projection;
out vec2 vTexCoord;
out vec4 vColor;
void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)";

const char* kVertexWave = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
uniform mat4 projection;
uniform float uTime;
uniform float uAmount;
out vec2 vTexCoord;
out vec4 vColor;
void main() {
    vec2 p = aPos;
    p.y += sin(aPos.x * 0.02 + uTime * 3.0) * 40.0 * uAmount;   // the vertex stage moves the sprite's corners
    gl_Position = projection * vec4(p, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)";

const char* kHeader = R"(#version 300 es
precision highp float;
in vec2 vTexCoord;
in vec4 vColor;
uniform sampler2D texture1;
uniform float uAmount;
uniform float uTime;
uniform float uMode;
uniform vec2 uTexel;
uniform mat4 uMatrix;
uniform vec4 uOffset;
out vec4 FragColor;
)";

struct ShaderDef {
    const char* key;
    const char* title;
    const char* vertex;
    const char* body;   // the part after the header: a complete main()
};

const ShaderDef kShaders[] = {
    {"grade", "Colour grading", kVertex, R"(void main() {
    vec4 c = texture(texture1, vTexCoord) * vColor;
    float g = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    vec3 o = c.rgb;
    if (uMode < 0.5)      o = vec3(g);                                  // grayscale
    else if (uMode < 1.5) o = vec3(g * 1.2, g, g * 0.8);               // sepia
    else if (uMode < 2.5) o = 1.0 - c.rgb;                             // invert
    else                  o = mix(vec3(0.1, 0.05, 0.3), vec3(1.0, 0.8, 0.3), g);   // duotone
    FragColor = vec4(mix(c.rgb, o, uAmount), c.a);
})"},
    {"blur", "Blur", kVertex, R"(void main() {
    vec4 sum = vec4(0.0);
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x)
            sum += texture(texture1, vTexCoord + vec2(float(x), float(y)) * uTexel * uAmount);
    FragColor = sum / 25.0 * vColor;      // 5 x 5 box blur, radius from the slider
})"},
    {"pixelate", "Pixelate and posterise", kVertex, R"(void main() {
    float cells = mix(480.0, 24.0, uAmount);
    vec2 uv = floor(vTexCoord * cells) / cells;                          // snap texture coordinates to a coarse grid
    vec3 c = texture(texture1, uv).rgb;
    float levels = mix(256.0, 4.0, uAmount * uMode);                       // optional colour quantisation
    FragColor = vec4(floor(c * levels) / levels, 1.0) * vColor;
})"},
    {"ripple", "Water ripple distortion", kVertex, R"(void main() {
    vec2 d = vTexCoord - vec2(0.5);
    float r = length(d);
    vec2 uv = vTexCoord + normalize(d + 1e-5) * sin(r * 40.0 - uTime * 4.0) * 0.02 * uAmount;
    FragColor = texture(texture1, uv) * vColor;        // the animation comes from a uniform: uTime
})"},
    {"edge", "Edge detection (Sobel)", kVertex, R"(float lum(vec2 uv) { return dot(texture(texture1, uv).rgb, vec3(0.299, 0.587, 0.114)); }
void main() {
    float tl = lum(vTexCoord + uTexel * vec2(-1.0, -1.0)), t = lum(vTexCoord + uTexel * vec2(0.0, -1.0)), tr = lum(vTexCoord + uTexel * vec2(1.0, -1.0));
    float l = lum(vTexCoord + uTexel * vec2(-1.0, 0.0)), r = lum(vTexCoord + uTexel * vec2(1.0, 0.0));
    float bl = lum(vTexCoord + uTexel * vec2(-1.0, 1.0)), b = lum(vTexCoord + uTexel * vec2(0.0, 1.0)), br = lum(vTexCoord + uTexel * vec2(1.0, 1.0));
    float gx = -tl - 2.0 * l - bl + tr + 2.0 * r + br, gy = -tl - 2.0 * t - tr + bl + 2.0 * b + br;
    float e = clamp(length(vec2(gx, gy)) * 2.0, 0.0, 1.0);
    vec3 c = texture(texture1, vTexCoord).rgb;
    FragColor = vec4(mix(c, vec3(e), uAmount), 1.0);
})"},
    {"crt", "CRT scanlines, vignette and curvature", kVertex, R"(void main() {
    vec2 uv = vTexCoord * 2.0 - 1.0;
    uv *= 1.0 + dot(uv, uv) * 0.12 * uAmount;                           // barrel distortion
    vec2 st = uv * 0.5 + 0.5;
    if (st.x < 0.0 || st.x > 1.0 || st.y < 0.0 || st.y > 1.0) { FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
    vec3 c = texture(texture1, st).rgb;
    c *= 1.0 - 0.35 * uAmount * (0.5 + 0.5 * sin(st.y * 540.0 * 3.14159));   // scanlines
    c *= 1.0 - 0.6 * uAmount * dot(uv, uv) * 0.5;                            // vignette
    FragColor = vec4(c, 1.0);
})"},
    {"plasma", "Procedural fragment shader", kVertex, R"(void main() {
    vec2 p = vTexCoord * 8.0;
    float v = sin(p.x + uTime) + sin(p.y * 1.3 - uTime * 0.7) + sin((p.x + p.y) * 0.8 + uTime * 0.5) + sin(length(p - 4.0) * 1.5 - uTime * 2.0);
    vec3 plasma = 0.5 + 0.5 * vec3(sin(v), sin(v + 2.094), sin(v + 4.188));
    vec3 scene = texture(texture1, vTexCoord).rgb;
    FragColor = vec4(mix(scene, plasma, uAmount), 1.0);   // no texture needed: the colour is computed per pixel
})"},
    {"swirl", "Swirl: rotating the UVs by radius", kVertex, R"(void main() {
    vec2 d = vTexCoord - vec2(0.5);
    float r = length(d);
    float a = uAmount * 6.0 * smoothstep(0.55, 0.0, r) * (0.65 + 0.35 * sin(uTime));   // twist most at the centre
    float s = sin(a), c = cos(a);
    vec2 uv = vec2(0.5) + vec2(c * d.x - s * d.y, s * d.x + c * d.y);
    FragColor = texture(texture1, uv) * vColor;
})"},
    {"chromatic", "Chromatic aberration", kVertex, R"(void main() {
    vec2 off = (vTexCoord - vec2(0.5)) * 0.045 * uAmount;     // the lens bends each colour a little differently
    float r = texture(texture1, vTexCoord + off).r;
    float g = texture(texture1, vTexCoord).g;
    float b = texture(texture1, vTexCoord - off).b;
    FragColor = vec4(r, g, b, 1.0) * vColor;
})"},
    {"kernel", "3 x 3 convolution kernels", kVertex, R"(vec3 at(vec2 o) { return texture(texture1, vTexCoord + o * uTexel).rgb; }
void main() {
    vec3 c = at(vec2(0.0)), n = at(vec2(0.0, -1.0)), s = at(vec2(0.0, 1.0)), e = at(vec2(1.0, 0.0)), w = at(vec2(-1.0, 0.0));
    vec3 k;
    if (uMode < 0.5)      k = c * 5.0 - (n + s + e + w);                                          // sharpen
    else if (uMode < 1.5) k = at(vec2(-1.0, -1.0)) * -2.0 - n - w + c + e + s + at(vec2(1.0, 1.0)) * 2.0 + 0.5;   // emboss
    else                  k = vec3(1.0) - abs(c * 4.0 - (n + s + e + w)) * 3.0;                    // Laplacian outline
    FragColor = vec4(mix(c, clamp(k, 0.0, 1.0), uAmount), 1.0);
})"},
    {"halftone", "Halftone dots", kVertex, R"(void main() {
    vec2 grid = vec2(144.0, 90.0);                          // cells across and down
    vec2 g = vTexCoord * grid;
    vec2 f = fract(g) - 0.5;                                  // position inside the cell, centred
    vec3 col = texture(texture1, (floor(g) + 0.5) / grid).rgb;     // one sample per cell
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    float radius = sqrt(lum) * 0.72;                          // brighter cells get bigger dots
    float dotMask = smoothstep(radius, radius - 0.1, length(f));
    vec3 scene = texture(texture1, vTexCoord).rgb;
    FragColor = vec4(mix(scene, col * dotMask, uAmount), 1.0);
})"},
    {"colormatrix", "Colour matrix", kVertex, R"(void main() {
    vec4 c = texture(texture1, vTexCoord) * vColor;
    FragColor = clamp(c * uMatrix + uOffset, 0.0, 1.0);     // SetUniformMat4 hands over the row-major Matrix as is, which GLSL reads as its transpose: c * M is row . (r, g, b, a)
})"},
    {"vertexwave", "Vertex shader: waving the corners", kVertexWave, R"(void main() {
    FragColor = texture(texture1, vTexCoord) * vColor;      // a plain fragment shader; the vertex stage does the work
})"},
};

class ShaderLab final : public ScreenExhibit {
public:
    ShaderLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services)
    {
        const std::string key = room.params.Get("shader");
        for (const ShaderDef& d : kShaders)
            if (key == d.key) def_ = &d;
        if (def_ == nullptr) def_ = &kShaders[0];
    }

    // Which dialect this renderer takes, from what it says it executes: GLSL ES / desktop GLSL, else the museum's WGSL, else none.
    void ChooseDialect()
    {
        GraphicsDevice& d = *S().device;
        const bool glsl = d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::GlslEs, CNA::ShaderStageEXT::Fragment) ||
                          d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::GlslDesktop, CNA::ShaderStageEXT::Fragment);
        wgsl_ = !glsl && ShaderSources::WgslBody(def_->key) != nullptr && d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::Wgsl, CNA::ShaderStageEXT::Fragment);
        // A renderer whose dialect is SPIR-V (SDL_GPU) compiles Vulkan-flavoured GLSL text at run time.
        vulkan_ = !glsl && !wgsl_ && d.GetShaderDialectEXT() == CNA::ShaderDialectEXT::SpirV && d.ExecutesShaderEffectSourceEXT();
        supported_ = glsl || wgsl_ || vulkan_;
    }

    void BuildStatic(RoomBuilder& b) override
    {
        ChooseDialect();
        ControlPanel& p = AddPanel(def_->title, -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        const std::string key = def_->key;
        p.AddSlider(key == "colormatrix" ? "Blend: identity to preset" : "uAmount", 0.0f, 1.0f, amount_, [this](float v) { amount_ = v; });
        if (key == "colormatrix") {
            p.AddChoice("Preset", {"Grayscale (Rec. 709)", "Sepia", "Invert", "Swap red and blue", "Hue rotation", "Saturation", "Contrast"}, 1, [this](int v) { preset_ = v; });
            p.AddSlider("Hue (degrees) / saturation / contrast", 0.0f, 1.0f, param_, [this](float v) { param_ = v; });
            for (int row = 0; row < 4; ++row)
                p.AddReadout("Matrix row " + std::to_string(row) + (row == 0 ? " (R')" : row == 1 ? " (G')" : row == 2 ? " (B')" : " (A')"), [this, row] { return MatrixRowText(row); });
            p.AddReadout("Offset", [this] { Vector4 o; (void)BuildMatrix(o); char text[64]; std::snprintf(text, sizeof text, "% .2f % .2f % .2f % .2f", static_cast<double>(o.X), static_cast<double>(o.Y), static_cast<double>(o.Z), static_cast<double>(o.W)); return std::string(text); });
            p.AddReadout("Run by", [this] { return native_ ? std::string("ColorMatrixEffect (CNAEXT class)") : (effect_ && effect_->IsEffectValid() ? std::string("a ShaderEffect holding the same matrix") : std::string("nothing: no class, no shader dialect here")); });
        }
        if (key == "grade") p.AddChoice("uMode", {"Grayscale", "Sepia", "Invert", "Duotone"}, 1, [this](int v) { mode_ = static_cast<float>(v); });
        if (key == "pixelate") p.AddToggle("Posterise colours (uMode)", false, [this](bool v) { mode_ = v ? 1.0f : 0.0f; });
        if (key == "kernel") p.AddChoice("uMode", {"Sharpen", "Emboss", "Laplacian outline"}, 1, [this](int v) { mode_ = static_cast<float>(v); });
        if (key == "blur" || key == "edge" || key == "kernel") p.AddSlider("Texel size scale", 0.5f, 3.0f, texelScale_, [this](float v) { texelScale_ = v; });
        p.AddReadout("Shader compiled", [this] { return effect_ && effect_->IsEffectValid() ? std::string("yes") : (supported_ ? std::string("NO") : std::string("not run: no GLSL or WGSL here")); });
        p.AddReadout("Shader language", [this] { return LanguageName(); });
        p.AddReadout("Renderer", [this] { return std::string(S().device->GetGraphicsRendererName()); });
        AddInfoSign(b, std::string("Shader lab: ") + def_->title,
                    {"CNA extension: ShaderEffect compiles GLSL (or, on WebGPU, WGSL) source at run time and plugs into SpriteBatch.Begin(..., effect).", "A 3D scene is rendered to a texture, then drawn through the effect: the post-processing workflow.",
                     "Uniforms are set by name (SetUniformFloat); the source of this shader is on the wall to the left."});
        // The source listing, one text line per source line.
        SignSpec src;
        src.leftAlign = true;
        src.pixelWidth = 1400;
        src.pixelHeight = 900;
        src.background = Color(10, 14, 22, 255);
        src.lines.push_back({std::string(def_->title) + (wgsl_ ? "  -  fragment shader (WGSL, this renderer's dialect)" : (vulkan_ ? "  -  fragment shader body (GLSL; this renderer's header is Vulkan GLSL 450)" : "  -  fragment shader (GLSL ES 3.00)")), true, Kit::kCnaExt});
        std::string body = wgsl_ ? ShaderSources::WgslBody(def_->key) : def_->body, line;
        for (char c : body + "\n") {
            if (c == '\n') {
                src.lines.push_back({line, false, Color(190, 235, 200, 255)});
                line.clear();
            } else {
                line.push_back(c);
            }
        }
        b.AddWallSign(Wall::West, 0.0f, 2.4f, 4.6f, 2.95f, src);
    }

    void OnScreenLoaded() override
    {
        GraphicsDevice& d = *S().device;
        source_ = std::make_unique<MiniView>(S(), 960, 540);
        // The sources are written in two dialects: GLSL ES 3.00 (the OpenGL renderers) and WGSL (WebGPU). Anything else,
        // SPIR-V or no source execution at all (FNA3D), gets the scene without the effect and a line saying so.
        ChooseDialect();
        if (std::string(def_->key) == "colormatrix" && Probes::ColorMatrix(d) == Probes::Answer::Yes) {
            // The CNA class does the work (only SOFTWARE runs it); no shader is needed.
            colorEffect_ = std::make_unique<ColorMatrixEffect>(d);
            native_ = true;
            supported_ = true;
        }
        const char* wgsl = ShaderSources::WgslBody(def_->key);
        if (supported_ && !native_) {
            try {
                if (wgsl_) {
                    const bool wave = std::string(def_->key) == "vertexwave";
                    effect_ = std::make_unique<ShaderEffect>(d, wave ? ShaderSources::kWgslVertexWave : ShaderSources::kWgslVertex,
                                                             std::string(ShaderSources::kWgslFragmentPrologue) + wgsl);
                    effect_->DeclareUniformBlockEXT(ShaderSources::kUniformBlockSize, ShaderSources::kUniformNames, ShaderSources::kUniformOffsets, ShaderSources::kUniformCount);
                } else if (vulkan_) {
                    effect_ = std::make_unique<ShaderEffect>(d, std::string(def_->key) == "vertexwave" ? ShaderSources::kVulkanGlslVertexWave : ShaderSources::kVulkanGlslVertex, std::string(ShaderSources::kVulkanGlslFragmentHeader) + def_->body);
                } else {
                    effect_ = std::make_unique<ShaderEffect>(d, def_->vertex, std::string(kHeader) + def_->body);
                }
                if (!effect_->IsEffectValid()) {
                    error_ = effect_->GetCompileErrorEXT();
                    Museum::Log("shader", "%s did not compile: %s", def_->key, error_.c_str());
                }
            } catch (const std::exception& e) {
                error_ = e.what();
                Museum::Log("shader", "%s: %s", def_->key, e.what());
            }
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (ctx.nearby) time_ += ctx.dt;
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (!source_ || !screen_) return;
        const float a = time_ * 0.35f;
        const Vector3 c = R().centre + Vector3(0, 0, 0);
        const Vector3 eye = c + Off(std::cos(a) * 5.0f, 2.4f, std::sin(a) * 5.0f);
        source_->Render(ctx, Matrix::CreateLookAt(eye, c + Vector3(0, 0.5f, 0), Vector3::Up),
                        Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(55.0f), source_->Aspect(), 0.1f, 60.0f), eye,
                        [this, c](const DrawContext& sub) { DrawDiorama(sub, S(), c + Vector3(0, 0.02f, 0), time_, 3.0f); }, Color(40, 60, 100, 255));
        ScreenExhibit::RenderOffscreen(ctx);
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (!source_ || !source_->HasContent()) return;
        const bool valid = (effect_ && effect_->IsEffectValid()) || (native_ && colorEffect_);
        const bool colorMatrix = std::string(def_->key) == "colormatrix";
        if (colorMatrix && valid) {
            Vector4 offset;
            const std::array<float, 16> m = BuildMatrix(offset);
            if (native_) {
                colorEffect_->SetColorMatrix(m);
                colorEffect_->SetColorOffset(offset);
            } else {
                effect_->Apply();
                const Matrix mx(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
                effect_->SetUniformMat4("uMatrix", &mx.M11);
                if (vulkan_) effect_->SetUniformVec4("color", offset.X, offset.Y, offset.Z, offset.W);   // SDL_GPU: the one vec4 slot holds the offset
                else effect_->SetUniformVec4("uOffset", offset.X, offset.Y, offset.Z, offset.W);
            }
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearClamp, nullptr, nullptr, native_ ? static_cast<Effect*>(colorEffect_.get()) : static_cast<Effect*>(effect_.get()));
            sb.Draw(*source_->Texture(), Rectangle(0, 0, 960, 540), Color::White);
            sb.End();
            return;
        }
        if (valid) {
            effect_->Apply();
            if (vulkan_) {   // one vec4 and one float, names ignored (see ShaderSources.cpp)
                effect_->SetUniformVec4("color", texelScale_ / 960.0f, texelScale_ / 540.0f, time_, mode_);
                effect_->SetUniformFloat("slot0", amount_);
            } else {
                effect_->SetUniformFloat("uAmount", amount_);
                effect_->SetUniformFloat("uTime", time_);
                effect_->SetUniformFloat("uMode", mode_);
                effect_->SetUniformVec2("uTexel", texelScale_ / 960.0f, texelScale_ / 540.0f);
            }
        }
        const std::string key = def_->key;
        if (key == "vertexwave" && valid) {
            // Many thin strips so the vertex shader has vertices to move along the sprite's width.
            sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearClamp, nullptr, nullptr, effect_.get());
            const int strips = 48;
            for (int i = 0; i < strips; ++i) {
                const int x0 = i * 960 / strips, x1 = (i + 1) * 960 / strips;
                sb.Draw(*source_->Texture(), Rectangle(x0, 60, x1 - x0, 420), Rectangle(x0, 0, x1 - x0, 540), Color::White);
            }
            (void)strips;
            sb.End();
            return;
        }
        sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearClamp, nullptr, nullptr, valid ? effect_.get() : nullptr);
        sb.Draw(*source_->Texture(), Rectangle(0, 0, 960, 540), Color::White);
        sb.End();
        if (!valid) {
            sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
            sb.Draw(*S().whiteTexture, Rectangle(0, 480, 960, 60), Color(0, 0, 0, 200));
            sb.DrawString(*S().font, supported_ ? "shader failed to compile: " + error_.substr(0, 60) : "this renderer takes neither GLSL ES nor the museum's WGSL: showing the unprocessed scene", Vector2(14, 496), Color(255, 190, 150, 255), 0.0f, Vector2(0, 0), 0.7f, SpriteEffects::None, 0.0f);
            sb.End();
        }
    }

private:
    // The colour-matrix room: the preset as a row-major 4 x 4 (output channel = row . (r, g, b, a)) and an offset, blended with the
    // identity by the amount slider. The maths is plain matrix algebra on colours: a hue rotation is a rotation about the grey axis.
    [[nodiscard]] std::array<float, 16> BuildMatrix(Vector4& offset) const
    {
        const float lr = 0.2126f, lg = 0.7152f, lb = 0.0722f;
        std::array<float, 16> p = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        Vector4 po(0, 0, 0, 0);
        switch (preset_) {
            case 0: p = {lr, lg, lb, 0, lr, lg, lb, 0, lr, lg, lb, 0, 0, 0, 0, 1}; break;
            case 1: p = {0.393f, 0.769f, 0.189f, 0, 0.349f, 0.686f, 0.168f, 0, 0.272f, 0.534f, 0.131f, 0, 0, 0, 0, 1}; break;
            case 2: p = {-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1}; po = Vector4(1, 1, 1, 0); break;
            case 3: p = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1}; break;
            case 4: {   // the W3C feColorMatrix hueRotate: a rotation about the grey axis, here by param * 360 degrees
                const float a = param_ * MathHelper::TwoPi, c = std::cos(a), s = std::sin(a);
                p = {0.213f + c * 0.787f - s * 0.213f, 0.715f - c * 0.715f - s * 0.715f, 0.072f - c * 0.072f + s * 0.928f, 0,
                     0.213f - c * 0.213f + s * 0.143f, 0.715f + c * 0.285f + s * 0.140f, 0.072f - c * 0.072f - s * 0.283f, 0,
                     0.213f - c * 0.213f - s * 0.787f, 0.715f - c * 0.715f + s * 0.715f, 0.072f + c * 0.928f + s * 0.072f, 0,
                     0, 0, 0, 1};
                break;
            }
            case 5: {
                const float t = param_ * 2.0f, k = 1.0f - t;
                p = {lr * k + t, lg * k, lb * k, 0, lr * k, lg * k + t, lb * k, 0, lr * k, lg * k, lb * k + t, 0, 0, 0, 0, 1};
                break;
            }
            default: {
                const float c = 0.2f + param_ * 2.8f, b = 0.5f * (1.0f - c);
                p = {c, 0, 0, 0, 0, c, 0, 0, 0, 0, c, 0, 0, 0, 0, 1};
                po = Vector4(b, b, b, 0);
                break;
            }
        }
        std::array<float, 16> m{};
        for (int i = 0; i < 16; ++i) m[static_cast<std::size_t>(i)] = ((i % 5) == 0 ? 1.0f : 0.0f) * (1.0f - amount_) + p[static_cast<std::size_t>(i)] * amount_;
        offset = po * amount_;
        return m;
    }
    [[nodiscard]] std::string MatrixRowText(int row) const
    {
        Vector4 o;
        const std::array<float, 16> m = BuildMatrix(o);
        char b[96];
        std::snprintf(b, sizeof b, "% .2f % .2f % .2f % .2f", static_cast<double>(m[static_cast<std::size_t>(row * 4)]), static_cast<double>(m[static_cast<std::size_t>(row * 4 + 1)]),
                      static_cast<double>(m[static_cast<std::size_t>(row * 4 + 2)]), static_cast<double>(m[static_cast<std::size_t>(row * 4 + 3)]));
        return b;
    }

    [[nodiscard]] std::string LanguageName() const
    {
        if (native_) return "none needed: the CNA ColorMatrixEffect class";
        if (!supported_) return "none";
        if (wgsl_) return "WGSL";
        if (vulkan_) return "GLSL 450 for Vulkan (compiled to SPIR-V)";
        return S().device->GetShaderDialectEXT() == CNA::ShaderDialectEXT::GlslDesktop ? "GLSL (desktop, adapted)" : "GLSL ES 3.00";
    }

    const ShaderDef* def_ = nullptr;
    std::unique_ptr<MiniView> source_;
    std::unique_ptr<ShaderEffect> effect_;
    std::string error_;
    std::unique_ptr<ColorMatrixEffect> colorEffect_;
    bool native_ = false;
    int preset_ = 1;
    float param_ = 0.25f;
    bool supported_ = false, wgsl_ = false, vulkan_ = false;
    float time_ = 0.0f, amount_ = 0.7f, mode_ = 1.0f, texelScale_ = 1.5f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeShaderLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ShaderLab>(room, services);
}
