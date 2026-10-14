#pragma once

#include <string>

/**
 * The shader lab's fragment shaders in WGSL, for the renderers whose ShaderEffect takes that dialect (WEBGPU). The GLSL ES
 * originals live beside the lab; each entry here is the same algorithm, line for line, so the two listings can be compared.
 * Sprite vertices reach a ShaderEffect already in clip space, so these shaders have no projection.
 */
namespace ShaderSources {

/** Everything a fragment body may use: the uniform block (std140 layout, see `kUniformNames`), the sprite texture and sampler. */
extern const char* const kWgslFragmentPrologue;
extern const char* const kWgslVertex;
extern const char* const kWgslVertexWave;

/**
 * Vulkan-flavoured GLSL 450, for the renderers that compile GLSL to SPIR-V (SDL_GPU): the same fragment bodies as the GLSL ES
 * originals behind a different header (explicit locations, sets and bindings, a uniform block in place of loose uniforms), and a
 * vertex shader that turns SpriteBatch's pixel positions into clip space with the viewport size CNA pushes in the first block.
 */
extern const char* const kVulkanGlslVertex;
extern const char* const kVulkanGlslVertexWave;   // the vertex-stage lab: time and amount arrive in the same fixed block
extern const char* const kVulkanGlslFragmentHeader;

/**
 * The two-output fragment shader of the multiple-render-target room (grayscale to target 0, an edge map to target 1), for the
 * dialects that are not GLSL ES. `kMrtWgslBlock` is the size of the uniform block it declares: one vec2, `uTexel`, at offset 0.
 */
extern const char* const kMrtWgslFragment;
extern const char* const kMrtVulkanFragment;
constexpr int kMrtBlockSize = 16;

/** The WGSL body for a lab shader key ("grade", "blur", ...), or null when there is none. */
const char* WgslBody(const std::string& key);

/** Names and byte offsets of the members the prologue's block declares (the colour-matrix room adds `uMatrix` and `uOffset`); `kUniformBlockSize` is its size. */
extern const char* const kUniformNames[6];
extern const int kUniformOffsets[6];
constexpr int kUniformCount = 6;
constexpr int kUniformBlockSize = 112;

}  // namespace ShaderSources
