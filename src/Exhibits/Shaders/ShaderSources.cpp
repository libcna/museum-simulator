#include "Exhibits/Shaders/ShaderSources.hpp"

#include <map>

namespace ShaderSources {

const char* const kUniformNames[6] = {"uAmount", "uTime", "uMode", "uTexel", "uMatrix", "uOffset"};
const int kUniformOffsets[6] = {0, 4, 8, 16, 32, 96};

// The block is 112 bytes: three floats, a pad, a vec2 on its own 8-byte boundary, then a mat4 and a vec4 on 16. textureSampleLevel is used throughout
// (level 0): the source texture has no mip chain, and unlike textureSample it may follow a non-uniform branch.
const char* const kWgslFragmentPrologue = R"WGSL(
struct U { uAmount: f32, uTime: f32, uMode: f32, pad0: f32, uTexel: vec2f, pad1: vec2f, uMatrix: mat4x4f, uOffset: vec4f };
@group(0) @binding(0) var<uniform> u: U;
@group(0) @binding(1) var texSampler: sampler;
@group(0) @binding(2) var tex: texture_2d<f32>;
fn sampleTex(uv: vec2f) -> vec4f { return textureSampleLevel(tex, texSampler, uv, 0.0); }
)WGSL";

const char* const kWgslVertex = R"WGSL(
struct VOut {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) color: vec4f,
};
@vertex fn vs_main(@location(0) position: vec3f, @location(1) uv: vec2f, @location(2) color: vec4f) -> VOut {
    var o: VOut;
    o.position = vec4f(position, 1.0);
    o.uv = uv;
    o.color = color;
    return o;
}
)WGSL";

const char* const kWgslVertexWave = R"WGSL(
struct U { uAmount: f32, uTime: f32, uMode: f32, pad0: f32, uTexel: vec2f, pad1: vec2f, uMatrix: mat4x4f, uOffset: vec4f };
@group(0) @binding(0) var<uniform> u: U;
struct VOut {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) color: vec4f,
};
@vertex fn vs_main(@location(0) position: vec3f, @location(1) uv: vec2f, @location(2) color: vec4f) -> VOut {
    var o: VOut;
    var p = position;
    p.y = p.y + sin((p.x + 1.0) * 9.6 + u.uTime * 3.0) * 0.148 * u.uAmount;   // the vertex stage moves the sprite's corners
    o.position = vec4f(p, 1.0);
    o.uv = uv;
    o.color = color;
    return o;
}
)WGSL";

// SDL_GPU fixes the descriptor sets: vertex uniforms in set 1, fragment textures in set 2, fragment uniforms in set 3. The
// vertex block must carry the viewport size first (CNA fills `vpSize_pad`); the fragment block is the lab's own 32 bytes.
const char* const kVulkanGlslVertex = R"GLSL(#version 450
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;
layout(set = 1, binding = 0) uniform PC {
    vec4 vpSize_pad;
    mat4 matrix;
    vec4 color;
    vec4 slot0_pad;
} pc;
void main() {
    vec2 ndc = (aPos / pc.vpSize_pad.xy) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)GLSL";

const char* const kVulkanGlslVertexWave = R"GLSL(#version 450
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;
layout(set = 1, binding = 0) uniform PC {
    vec4 vpSize_pad;
    mat4 matrix;
    vec4 color;
    vec4 slot0_pad;
} pc;
void main() {
    vec2 p = aPos;
    p.y += sin(aPos.x * 0.02 + pc.color.z * 3.0) * 40.0 * pc.slot0_pad.x;   // time in color.z, amount in slot0
    vec2 ndc = (p / pc.vpSize_pad.xy) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)GLSL";

// SDL_GPU's ShaderEffect has one fixed 128-byte uniform block whatever the names say: a viewport size, a matrix, one vec4
// ("color", SetUniformVec2/3/4 all land there) and one float ("slot0"). The lab's four uniforms are packed into the last two and
// given back their own names with macros, so the shader bodies stay the same text as the GLSL ES ones.
const char* const kVulkanGlslFragmentHeader = R"GLSL(#version 450
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 0) uniform sampler2D texture1;
layout(set = 3, binding = 0) uniform PC {
    vec4 vpSize_pad;
    mat4 matrix;
    vec4 color;
    vec4 slot0_pad;
} pc;
#define uTexel pc.color.xy
#define uTime pc.color.z
#define uMode pc.color.w
#define uAmount pc.slot0_pad.x
#define uMatrix pc.matrix
#define uOffset pc.color
)GLSL";

const char* const kMrtWgslFragment = R"WGSL(
struct U { uTexel: vec2f, pad: vec2f };
@group(0) @binding(0) var<uniform> u: U;
@group(0) @binding(1) var texSampler: sampler;
@group(0) @binding(2) var tex: texture_2d<f32>;
struct FOut { @location(0) gray: vec4f, @location(1) edge: vec4f, @location(2) color: vec4f, @location(3) poster: vec4f };
fn lum(uv: vec2f) -> f32 { return dot(textureSampleLevel(tex, texSampler, uv, 0.0).rgb, vec3f(0.299, 0.587, 0.114)); }
@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> FOut {
    let g = lum(uv);
    let gx = lum(uv + vec2f(u.uTexel.x, 0.0)) - lum(uv - vec2f(u.uTexel.x, 0.0));
    let gy = lum(uv + vec2f(0.0, u.uTexel.y)) - lum(uv - vec2f(0.0, u.uTexel.y));
    let source = textureSampleLevel(tex, texSampler, uv, 0.0).rgb;
    var o: FOut;
    o.gray = vec4f(vec3f(g), 1.0);                                              // target 0: grayscale
    o.edge = vec4f(vec3f(clamp(length(vec2f(gx, gy)) * 4.0, 0.0, 1.0)), 1.0);   // target 1: edge map
    o.color = vec4f(source, 1.0);                                                // target 2: source colour
    o.poster = vec4f(floor(source * 3.0 + 0.5) / 3.0, 1.0);                     // target 3: posterized
    return o;
}
)WGSL";

const char* const kMrtVulkanFragment = R"GLSL(#version 450
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outGray;
layout(location = 1) out vec4 outEdge;
layout(location = 2) out vec4 outColor;
layout(location = 3) out vec4 outPoster;
layout(set = 2, binding = 0) uniform sampler2D texture1;
layout(set = 3, binding = 0) uniform PC {
    vec4 vpSize_pad;
    mat4 matrix;
    vec4 color;
    vec4 slot0_pad;
} pc;
#define uTexel pc.color.xy
float lum(vec2 uv) { return dot(texture(texture1, uv).rgb, vec3(0.299, 0.587, 0.114)); }
void main() {
    float g = lum(vTexCoord);
    float gx = lum(vTexCoord + vec2(uTexel.x, 0.0)) - lum(vTexCoord - vec2(uTexel.x, 0.0));
    float gy = lum(vTexCoord + vec2(0.0, uTexel.y)) - lum(vTexCoord - vec2(0.0, uTexel.y));
    vec3 source = texture(texture1, vTexCoord).rgb;
    outGray = vec4(vec3(g), 1.0);
    outEdge = vec4(vec3(clamp(length(vec2(gx, gy)) * 4.0, 0.0, 1.0)), 1.0);
    outColor = vec4(source, 1.0);
    outPoster = vec4(floor(source * 3.0 + 0.5) / 3.0, 1.0);
}
)GLSL";

namespace {

const std::map<std::string, const char*>& Bodies()
{
    static const std::map<std::string, const char*> bodies = {
        {"grade", R"WGSL(@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let c = sampleTex(uv) * color;
    let g = dot(c.rgb, vec3f(0.299, 0.587, 0.114));
    var o = c.rgb;
    if (u.uMode < 0.5)      { o = vec3f(g); }                                      // grayscale
    else if (u.uMode < 1.5) { o = vec3f(g * 1.2, g, g * 0.8); }                    // sepia
    else if (u.uMode < 2.5) { o = vec3f(1.0) - c.rgb; }                            // invert
    else                    { o = mix(vec3f(0.1, 0.05, 0.3), vec3f(1.0, 0.8, 0.3), g); }   // duotone
    return vec4f(mix(c.rgb, o, u.uAmount), c.a);
})WGSL"},
        {"blur", R"WGSL(@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    var sum = vec4f(0.0);
    for (var y = -2; y <= 2; y++) {
        for (var x = -2; x <= 2; x++) {
            sum += sampleTex(uv + vec2f(f32(x), f32(y)) * u.uTexel * u.uAmount);
        }
    }
    return sum / 25.0 * color;      // 5 x 5 box blur, radius from the slider
})WGSL"},
        {"pixelate", R"WGSL(@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let cells = mix(480.0, 24.0, u.uAmount);
    let snapped = floor(uv * cells) / cells;                          // snap texture coordinates to a coarse grid
    let c = sampleTex(snapped).rgb;
    let levels = mix(256.0, 4.0, u.uAmount * u.uMode);                // optional colour quantisation
    return vec4f(floor(c * levels) / levels, 1.0) * color;
})WGSL"},
        {"ripple", R"WGSL(@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let d = uv - vec2f(0.5);
    let r = length(d);
    let moved = uv + normalize(d + vec2f(1e-5)) * sin(r * 40.0 - u.uTime * 4.0) * 0.02 * u.uAmount;
    return sampleTex(moved) * color;        // the animation comes from a uniform: uTime
})WGSL"},
        {"edge", R"WGSL(fn lum(uv: vec2f) -> f32 { return dot(sampleTex(uv).rgb, vec3f(0.299, 0.587, 0.114)); }
@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let tl = lum(uv + u.uTexel * vec2f(-1.0, -1.0));
    let t = lum(uv + u.uTexel * vec2f(0.0, -1.0));
    let tr = lum(uv + u.uTexel * vec2f(1.0, -1.0));
    let l = lum(uv + u.uTexel * vec2f(-1.0, 0.0));
    let r = lum(uv + u.uTexel * vec2f(1.0, 0.0));
    let bl = lum(uv + u.uTexel * vec2f(-1.0, 1.0));
    let b = lum(uv + u.uTexel * vec2f(0.0, 1.0));
    let br = lum(uv + u.uTexel * vec2f(1.0, 1.0));
    let gx = -tl - 2.0 * l - bl + tr + 2.0 * r + br;
    let gy = -tl - 2.0 * t - tr + bl + 2.0 * b + br;
    let e = clamp(length(vec2f(gx, gy)) * 2.0, 0.0, 1.0);
    let c = sampleTex(uv).rgb;
    return vec4f(mix(c, vec3f(e), u.uAmount), 1.0);
})WGSL"},
        {"crt", R"WGSL(@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    var p = uv * 2.0 - 1.0;
    p = p * (1.0 + dot(p, p) * 0.12 * u.uAmount);                       // barrel distortion
    let st = p * 0.5 + 0.5;
    if (st.x < 0.0 || st.x > 1.0 || st.y < 0.0 || st.y > 1.0) { return vec4f(0.0, 0.0, 0.0, 1.0); }
    var c = sampleTex(st).rgb;
    c = c * (1.0 - 0.35 * u.uAmount * (0.5 + 0.5 * sin(st.y * 540.0 * 3.14159)));   // scanlines
    c = c * (1.0 - 0.6 * u.uAmount * dot(p, p) * 0.5);                               // vignette
    return vec4f(c, 1.0);
})WGSL"},
        {"plasma", R"WGSL(@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let p = uv * 8.0;
    let v = sin(p.x + u.uTime) + sin(p.y * 1.3 - u.uTime * 0.7) + sin((p.x + p.y) * 0.8 + u.uTime * 0.5) + sin(length(p - vec2f(4.0)) * 1.5 - u.uTime * 2.0);
    let plasma = 0.5 + 0.5 * vec3f(sin(v), sin(v + 2.094), sin(v + 4.188));
    let scene = sampleTex(uv).rgb;
    return vec4f(mix(scene, plasma, u.uAmount), 1.0);   // no texture needed: the colour is computed per pixel
})WGSL"},
        {"swirl", R"WGSL(@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let d = uv - vec2f(0.5);
    let r = length(d);
    let a = u.uAmount * 6.0 * (1.0 - smoothstep(0.0, 0.55, r)) * (0.65 + 0.35 * sin(u.uTime));   // twist most at the centre
    let s = sin(a);
    let c = cos(a);
    let moved = vec2f(0.5) + vec2f(c * d.x - s * d.y, s * d.x + c * d.y);
    return sampleTex(moved) * color;
})WGSL"},
        {"chromatic", R"WGSL(@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let off = (uv - vec2f(0.5)) * 0.045 * u.uAmount;     // the lens bends each colour a little differently
    let r = sampleTex(uv + off).r;
    let g = sampleTex(uv).g;
    let b = sampleTex(uv - off).b;
    return vec4f(r, g, b, 1.0) * color;
})WGSL"},
        {"kernel", R"WGSL(fn at(uv: vec2f, o: vec2f) -> vec3f { return sampleTex(uv + o * u.uTexel).rgb; }
@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let c = at(uv, vec2f(0.0));
    let n = at(uv, vec2f(0.0, -1.0));
    let s = at(uv, vec2f(0.0, 1.0));
    let e = at(uv, vec2f(1.0, 0.0));
    let w = at(uv, vec2f(-1.0, 0.0));
    var k: vec3f;
    if (u.uMode < 0.5)      { k = c * 5.0 - (n + s + e + w); }                                                              // sharpen
    else if (u.uMode < 1.5) { k = at(uv, vec2f(-1.0, -1.0)) * -2.0 - n - w + c + e + s + at(uv, vec2f(1.0, 1.0)) * 2.0 + vec3f(0.5); }   // emboss
    else                    { k = vec3f(1.0) - abs(c * 4.0 - (n + s + e + w)) * 3.0; }                                      // Laplacian outline
    return vec4f(mix(c, clamp(k, vec3f(0.0), vec3f(1.0)), u.uAmount), 1.0);
})WGSL"},
        {"halftone", R"WGSL(@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let grid = vec2f(144.0, 90.0);                          // cells across and down
    let g = uv * grid;
    let f = fract(g) - vec2f(0.5);                          // position inside the cell, centred
    let col = sampleTex((floor(g) + vec2f(0.5)) / grid).rgb;     // one sample per cell
    let lum = dot(col, vec3f(0.299, 0.587, 0.114));
    let radius = sqrt(lum) * 0.72;                          // brighter cells get bigger dots
    let dotMask = 1.0 - smoothstep(radius - 0.1, radius, length(f));
    let scene = sampleTex(uv).rgb;
    return vec4f(mix(scene, col * dotMask, u.uAmount), 1.0);
})WGSL"},
        {"colormatrix", R"WGSL(@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    let c = sampleTex(uv) * color;
    return clamp(c * u.uMatrix + u.uOffset, vec4f(0.0), vec4f(1.0));   // the row-major Matrix arrives as its transpose, so c * M is row . (r, g, b, a)
})WGSL"},
        {"vertexwave", R"WGSL(@fragment fn fs_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
    return sampleTex(uv) * color;      // a plain fragment shader; the vertex stage does the work
})WGSL"},
    };
    return bodies;
}

}  // namespace

const char* WgslBody(const std::string& key)
{
    const auto it = Bodies().find(key);
    return it == Bodies().end() ? nullptr : it->second;
}

}  // namespace ShaderSources
