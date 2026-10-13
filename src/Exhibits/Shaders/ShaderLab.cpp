// Shader laboratory (CNA extension): custom GLSL fragment and vertex shaders run through CNA's ShaderEffect and
// SpriteBatch. A 3D scene is rendered into a render target, then drawn to the screen through the effect: the
// post-processing workflow, written entirely against CNA's public API. Needs a renderer that executes GLSL
// source (the OpenGL family); other renderers show the unprocessed scene and say why.
#include <algorithm>
#include <cmath>
#include <string>

#include "CNA/ShaderLanguageEXT.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
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

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(def_->title, -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddSlider("uAmount", 0.0f, 1.0f, amount_, [this](float v) { amount_ = v; });
        const std::string key = def_->key;
        if (key == "grade") p.AddChoice("uMode", {"Grayscale", "Sepia", "Invert", "Duotone"}, 1, [this](int v) { mode_ = static_cast<float>(v); });
        if (key == "pixelate") p.AddToggle("Posterise colours (uMode)", false, [this](bool v) { mode_ = v ? 1.0f : 0.0f; });
        if (key == "blur" || key == "edge") p.AddSlider("Texel size scale", 0.5f, 3.0f, texelScale_, [this](float v) { texelScale_ = v; });
        p.AddReadout("Shader compiled", [this] { return effect_ && effect_->IsEffectValid() ? std::string("yes") : (supported_ ? std::string("NO") : std::string("not run: no GLSL here")); });
        p.AddReadout("Shader language", [this] { return LanguageName(); });
        p.AddReadout("Renderer", [this] { return std::string(S().device->GetGraphicsRendererName()); });
        AddInfoSign(b, std::string("Shader lab: ") + def_->title,
                    {"CNA extension: ShaderEffect compiles GLSL source at run time and plugs into SpriteBatch.Begin(..., effect).", "A 3D scene is rendered to a texture, then drawn through the effect: the post-processing workflow.",
                     "Uniforms are set by name (SetUniformFloat); the source of this shader is on the wall to the left."});
        // The source listing, one text line per source line.
        SignSpec src;
        src.leftAlign = true;
        src.pixelWidth = 1400;
        src.pixelHeight = 900;
        src.background = Color(10, 14, 22, 255);
        src.lines.push_back({std::string(def_->title) + "  -  fragment shader (GLSL ES 3.00)", true, Kit::kCnaExt});
        std::string body = def_->body, line;
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
        supported_ = d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::GlslEs, CNA::ShaderStageEXT::Fragment) ||
                     d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::GlslDesktop, CNA::ShaderStageEXT::Fragment);
        if (supported_) {
            try {
                effect_ = std::make_unique<ShaderEffect>(d, def_->vertex, std::string(kHeader) + def_->body);
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
        const Vector3 eye = c + Vector3(std::cos(a) * 5.0f, 2.4f, std::sin(a) * 5.0f);
        source_->Render(ctx, Matrix::CreateLookAt(eye, c + Vector3(0, 0.5f, 0), Vector3::Up),
                        Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(55.0f), source_->Aspect(), 0.1f, 60.0f), eye,
                        [this, c](const DrawContext& sub) { DrawDiorama(sub, S(), c + Vector3(0, 0.02f, 0), time_, 3.0f); }, Color(40, 60, 100, 255));
        ScreenExhibit::RenderOffscreen(ctx);
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (!source_ || !source_->HasContent()) return;
        const bool valid = effect_ && effect_->IsEffectValid();
        if (valid) {
            effect_->Apply();
            effect_->SetUniformFloat("uAmount", amount_);
            effect_->SetUniformFloat("uTime", time_);
            effect_->SetUniformFloat("uMode", mode_);
            effect_->SetUniformVec2("uTexel", texelScale_ / 960.0f, texelScale_ / 540.0f);
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
            sb.DrawString(*S().font, supported_ ? "shader failed to compile: " + error_.substr(0, 60) : "this renderer does not execute GLSL source: showing the unprocessed scene", Vector2(14, 496), Color(255, 190, 150, 255), 0.0f, Vector2(0, 0), 0.7f, SpriteEffects::None, 0.0f);
            sb.End();
        }
    }

private:
    [[nodiscard]] std::string LanguageName() const
    {
        if (!supported_) return "none";
        return S().device->GetShaderDialectEXT() == CNA::ShaderDialectEXT::GlslDesktop ? "GLSL (desktop, adapted)" : "GLSL ES 3.00";
    }

    const ShaderDef* def_ = nullptr;
    std::unique_ptr<MiniView> source_;
    std::unique_ptr<ShaderEffect> effect_;
    std::string error_;
    bool supported_ = false;
    float time_ = 0.0f, amount_ = 0.7f, mode_ = 1.0f, texelScale_ = 1.5f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeShaderLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ShaderLab>(room, services);
}
