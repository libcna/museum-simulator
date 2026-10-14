// Shader compile errors (CNA extension): a ShaderEffect given a broken shader does not throw; it reports through IsEffectValid(),
// GetCompileErrorEXT() (the raw log) and GetShaderDiagnosticsEXT() (the same log parsed into records: severity, stage, line,
// column, message). The room feeds the active renderer one deliberate mistake at a time, in the dialect it takes (GLSL ES, WGSL or
// Vulkan GLSL), and lists what CNA hands back next to the offending source. ShaderDiagnosticEXT::parseCompilerLog and
// ShaderCompilationExceptionEXT are plain value types, so the lower half of the screen exercises them on any renderer.
#include <algorithm>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "CNA/ShaderDiagnosticEXT.hpp"
#include "CNA/ShaderLanguageEXT.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Exhibits/Shaders/ShaderSources.hpp"
#include "Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Dialect { None, GlslEs, Wgsl, VulkanGlsl };

// One mistake: a title, the fragment body for each dialect (empty = no body, which makes the case a control) and which stage is broken.
struct Mistake {
    const char* title;
    const char* hint;
    const char* glsl;     // fragment body after the header, GLSL ES 3.00 and Vulkan GLSL share it
    const char* wgsl;
    bool vertexStage;
};

const Mistake kMistakes[] = {
    {"A valid shader (the control)", "Nothing to report: IsEffectValid() is true and the diagnostics list is empty.",
     "void main() {\n    FragColor = texture(texture1, vTexCoord) * vColor;\n}\n",
     "@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {\n    return sampleTex(uv) * color;\n}\n", false},
    {"A missing semicolon", "The most common mistake: the compiler names the line after the one that is wrong.",
     "void main() {\n    vec4 c = texture(texture1, vTexCoord)\n    FragColor = c * vColor;\n}\n",
     "@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {\n    let c = sampleTex(uv)\n    return c * color;\n}\n", false},
    {"An undeclared identifier", "A typo in a variable name: the log names the identifier it could not find.",
     "void main() {\n    vec4 c = texture(texture1, vTexCoord);\n    FragColor = c * tint;\n}\n",
     "@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {\n    let c = sampleTex(uv);\n    return c * tint;\n}\n", false},
    {"A type mismatch", "A vec3 assigned where a vec4 is needed.",
     "void main() {\n    vec3 rgb = texture(texture1, vTexCoord).rgb;\n    FragColor = rgb;\n}\n",
     "@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {\n    let rgb = sampleTex(uv).rgb;\n    return rgb;\n}\n", false},
    {"No entry point", "The shader is valid text but defines nothing to run.",
     "float helper(float x) {\n    return x * 2.0;\n}\n",
     "fn helper(x: f32) -> f32 {\n    return x * 2.0;\n}\n", false},
    {"A broken vertex stage", "The error is in the vertex shader: the stage field says so.",
     "void main() {\n    FragColor = texture(texture1, vTexCoord) * vColor;\n}\n",
     "@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {\n    return sampleTex(uv) * color;\n}\n", true},
};
constexpr int kMistakeCount = static_cast<int>(sizeof kMistakes / sizeof kMistakes[0]);

// Compiler logs in the shapes parseCompilerLog recognises, to show the parser without a renderer.
struct SampleLog { const char* title; const char* log; CNA::ShaderStageEXT stage; };
const SampleLog kSampleLogs[] = {
    {"GLSL driver", "ERROR: tint.frag:7:12: 'tint' : undeclared identifier\nWARNING: tint.frag:3:1: 'precision' : unused declaration", CNA::ShaderStageEXT::Fragment},
    {"HLSL compiler", "wave.hlsl(12,5): error X3004: undeclared identifier 'uv'\nVS wave.hlsl(4,1): warning X3206: implicit truncation", CNA::ShaderStageEXT::Fragment},
    {"Text without a place", "the program failed to link", CNA::ShaderStageEXT::Vertex},
};

const char* SeverityName(CNA::ShaderDiagnosticSeverityEXT s)
{
    switch (s) {
        case CNA::ShaderDiagnosticSeverityEXT::Information: return "info";
        case CNA::ShaderDiagnosticSeverityEXT::Warning: return "warning";
        default: return "error";
    }
}
const char* StageName(CNA::ShaderStageEXT s)
{
    return s == CNA::ShaderStageEXT::Vertex ? "vertex" : s == CNA::ShaderStageEXT::Fragment ? "fragment" : "unknown";
}

std::string Describe(const CNA::ShaderDiagnosticEXT& d)
{
    char head[96];
    std::snprintf(head, sizeof head, "%s, %s, line %d col %d: ", SeverityName(d.getSeverity()), StageName(d.getStage()), d.getLine(), d.getColumn());
    std::string text = head + d.getMessage();
    if (text.size() > 100) text = text.substr(0, 97) + "...";
    return text;
}

class ShaderErrorLab final : public ScreenExhibit {
public:
    ShaderErrorLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ChooseDialect();
        ControlPanel& p = AddPanel("Shader errors", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        std::vector<std::string> names;
        for (const Mistake& m : kMistakes) names.push_back(m.title);
        p.AddChoice("Mistake", names, 1, [this](int v) { choice_ = v; dirty_ = true; });
        p.AddChoice("Sample log (parser)", {kSampleLogs[0].title, kSampleLogs[1].title, kSampleLogs[2].title}, 0, [this](int v) { sample_ = v; });
        p.AddReadout("Dialect this renderer takes", [this] { return DialectName(); });
        p.AddReadout("ExecutesShaderEffectSource", [this] { return executes_; });
        p.AddReadout("IsEffectValid()", [this] { return valid_; });
        p.AddReadout("Diagnostics", [this] { return std::to_string(diagnostics_.size()); });
        p.AddReadout("Exception", [this] { return exception_; });
        AddInfoSign(b, "When a shader does not compile",
                    {"ShaderEffect does not throw for a shader the renderer rejects. IsEffectValid() says that it failed, GetCompileErrorEXT() returns the raw log, and GetShaderDiagnosticsEXT() returns it parsed:",
                     "severity, stage, source label, line and column, message. Where a renderer never compiles source (SOFTWARE, HEADLESS) every shader is accepted and the room says so instead of inventing errors."});
    }

    void OnScreenLoaded() override { dirty_ = true; }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        if (dirty_) { dirty_ = false; Compile(); }
        const Mistake& m = kMistakes[choice_];
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Text(sb, std::string("Shader: ") + m.title, 24, 8, 0.85f, Kit::kGold);
        Text(sb, std::string(m.vertexStage ? "vertex" : "fragment") + " source given to the renderer (" + DialectName() + "), real line numbers", 24, 52, 0.58f, Kit::kDim);
        // The source around the line a diagnostic points at (or the start of the body when there is none), that line marked.
        const int target = FirstErrorLine();
        const int focus = target > 0 ? target : (m.vertexStage ? 1 : headerLines_ + 1);
        const int first = std::max(1, focus - 3);
        float y = 80.0f;
        int n = 0;
        std::string line;
        for (char c : shown_ + "\n") {
            if (c != '\n') { line.push_back(c); continue; }
            ++n;
            if (n >= first && n < first + 7) {
                const bool hit = n == target;
                Text(sb, (hit ? "> " : "  ") + std::to_string(n) + "  " + line.substr(0, 78), 34, y, 0.56f, hit ? Kit::Rgba(255, 150, 130) : Kit::Rgba(190, 235, 200));
                y += 22.0f;
            }
            line.clear();
        }
        // What the effect reported.
        y = 250.0f;
        Text(sb, "GetShaderDiagnosticsEXT()", 24, y, 0.62f, Kit::kGold);
        y += 28.0f;
        if (!supported_) { Text(sb, "no dialect the museum writes: nothing compiled", 34, y, 0.56f, Kit::kDim); y += 22.0f; }
        else if (!executesSource_) { Text(sb, "this renderer accepts source without compiling it: no errors", 34, y, 0.56f, Kit::kDim); y += 22.0f; }
        else if (diagnostics_.empty()) { Text(sb, "(empty: the shader compiled)", 34, y, 0.56f, Kit::Rgba(150, 230, 160)); y += 22.0f; }
        for (size_t i = 0; i < diagnostics_.size() && i < 3; ++i) {
            Text(sb, Describe(diagnostics_[i]), 34, y, 0.54f, Kit::Rgba(255, 190, 150));
            y += 22.0f;
        }
        // ShaderDiagnosticEXT::parseCompilerLog on a fixed log: no renderer needed.
        const SampleLog& sl = kSampleLogs[sample_];
        Text(sb, std::string("parseCompilerLog, ") + sl.title, 24, 372, 0.62f, Kit::kGold);
        std::string raw = sl.log;
        std::replace(raw.begin(), raw.end(), '\n', '|');
        Text(sb, "log: " + raw.substr(0, 92), 34, 400, 0.5f, Kit::kDim);
        const std::vector<CNA::ShaderDiagnosticEXT> parsed = CNA::ShaderDiagnosticEXT::parseCompilerLog(sl.log, sl.stage, "sample");
        float py = 424.0f;
        for (size_t i = 0; i < parsed.size() && i < 3; ++i) {
            Text(sb, Describe(parsed[i]), 34, py, 0.54f, Kit::Rgba(200, 220, 255));
            py += 22.0f;
        }
        sb.End();
    }

private:
    void Text(SpriteBatch& sb, const std::string& s, float x, float y, float scale, Color c)
    {
        sb.DrawString(*S().font, s, Vector2(x, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
    }

    void ChooseDialect()
    {
        GraphicsDevice& d = *S().device;
        const bool glsl = d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::GlslEs, CNA::ShaderStageEXT::Fragment) ||
                          d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::GlslDesktop, CNA::ShaderStageEXT::Fragment);
        const bool wgsl = !glsl && d.SupportsShaderLanguageEXT(CNA::ShaderLanguageEXT::Wgsl, CNA::ShaderStageEXT::Fragment);
        executesSource_ = d.ExecutesShaderEffectSourceEXT();
        const bool vulkan = !glsl && !wgsl && d.GetShaderDialectEXT() == CNA::ShaderDialectEXT::SpirV && executesSource_;
        dialect_ = glsl ? Dialect::GlslEs : wgsl ? Dialect::Wgsl : vulkan ? Dialect::VulkanGlsl : Dialect::None;
        supported_ = dialect_ != Dialect::None;
        executes_ = executesSource_ ? "true" : "false: source is accepted, not compiled";
    }

    std::string DialectName() const
    {
        switch (dialect_) {
            case Dialect::GlslEs: return "GLSL ES 3.00";
            case Dialect::Wgsl: return "WGSL";
            case Dialect::VulkanGlsl: return "Vulkan GLSL 450";
            default: return "none the museum writes";
        }
    }

    // The first line any diagnostic points at, in the full stage source.
    int FirstErrorLine() const
    {
        for (const CNA::ShaderDiagnosticEXT& d : diagnostics_)
            if (d.getLine() > 0) return d.getLine();
        return 0;
    }

    static std::string Replace(std::string s, const std::string& from, const std::string& to)
    {
        const size_t at = s.find(from);
        if (at != std::string::npos) s.replace(at, from.size(), to);
        return s;
    }

    static int CountLines(const std::string& s)
    {
        return static_cast<int>(std::count(s.begin(), s.end(), '\n'));
    }

    void Compile()
    {
        effect_.reset();
        diagnostics_.clear();
        rawLog_.clear();
        exception_ = "none";
        valid_ = "-";
        headerLines_ = 0;
        shown_.clear();
        if (!supported_) return;
        const Mistake& m = kMistakes[choice_];
        std::string vertex, fragment;
        switch (dialect_) {
            case Dialect::Wgsl:
                vertex = ShaderSources::kWgslVertex;
                fragment = std::string(ShaderSources::kWgslFragmentPrologue) + m.wgsl;
                if (m.vertexStage) vertex = Replace(vertex, "o.position = vec4f(position, 1.0);", "o.position = vec4f(positon, 1.0);");
                headerLines_ = CountLines(ShaderSources::kWgslFragmentPrologue);
                break;
            case Dialect::VulkanGlsl:
                vertex = ShaderSources::kVulkanGlslVertex;
                fragment = std::string(ShaderSources::kVulkanGlslFragmentHeader) + m.glsl;
                if (m.vertexStage) vertex = Replace(vertex, "gl_Position =", "gl_Positon =");
                headerLines_ = CountLines(ShaderSources::kVulkanGlslFragmentHeader);
                break;
            default:
                vertex = kVertexEs;
                fragment = std::string(kHeaderEs) + m.glsl;
                if (m.vertexStage) vertex = Replace(vertex, "gl_Position =", "gl_Positon =");
                headerLines_ = CountLines(kHeaderEs);
                break;
        }
        shown_ = m.vertexStage ? vertex : fragment;
        try {
            effect_ = std::make_unique<ShaderEffect>(*S().device, vertex, fragment);
            if (dialect_ == Dialect::Wgsl)
                effect_->DeclareUniformBlockEXT(ShaderSources::kUniformBlockSize, ShaderSources::kUniformNames, ShaderSources::kUniformOffsets, ShaderSources::kUniformCount);
            valid_ = effect_->IsEffectValid() ? "true" : "false";
            diagnostics_ = effect_->GetShaderDiagnosticsEXT();
            rawLog_ = effect_->GetCompileErrorEXT();
        } catch (const CNA::ShaderCompilationExceptionEXT& e) {
            exception_ = "ShaderCompilationExceptionEXT, " + std::to_string(e.getDiagnostics().size()) + " record(s)";
            diagnostics_ = e.getDiagnostics();
            rawLog_ = e.what();
            valid_ = "threw";
        } catch (const std::exception& e) {
            exception_ = std::string("std::exception: ") + std::string(e.what()).substr(0, 50);
            rawLog_ = e.what();
            valid_ = "threw";
        }
        Museum::Log("shader", "error lab, %s: valid=%s, %zu diagnostic(s)", m.title, valid_.c_str(), diagnostics_.size());
    }

    static constexpr const char* kVertexEs = R"(#version 300 es
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
    static constexpr const char* kHeaderEs = R"(#version 300 es
precision highp float;
in vec2 vTexCoord;
in vec4 vColor;
uniform sampler2D texture1;
out vec4 FragColor;
)";

    std::unique_ptr<ShaderEffect> effect_;
    std::vector<CNA::ShaderDiagnosticEXT> diagnostics_;
    std::string rawLog_, shown_, valid_ = "-", exception_ = "none", executes_ = "-";
    Dialect dialect_ = Dialect::None;
    bool supported_ = false, executesSource_ = false, dirty_ = true;
    int choice_ = 1, sample_ = 0, headerLines_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeShaderErrorLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ShaderErrorLab>(room, services);
}
