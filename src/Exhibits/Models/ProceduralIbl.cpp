#include "Exhibits/Models/ProceduralIbl.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/CubeMapFace.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

Vector3 Env(const Vector3& d, int kind)
{
    const float up = d.Y;
    if (kind == 2) {   // studio: a dim room with three soft boxes
        Vector3 c(0.05f, 0.055f, 0.065f);
        const auto box = [&](const Vector3& centre, float width, const Vector3& colour) {
            const Vector3 n = Vector3::Normalize(centre);
            const float cosAngle = Vector3::Dot(d, n);
            if (cosAngle > std::cos(width)) c = c + colour * std::min(1.0f, (cosAngle - std::cos(width)) / (1.0f - std::cos(width)) * 3.0f);
        };
        box(Vector3(-1.0f, 1.2f, 1.0f), 0.45f, Vector3(1.0f, 0.95f, 0.9f));
        box(Vector3(1.4f, 0.6f, 0.6f), 0.32f, Vector3(0.7f, 0.8f, 1.0f));
        box(Vector3(0.0f, 0.7f, -1.4f), 0.4f, Vector3(1.0f, 0.8f, 0.6f));
        return Vector3(std::min(c.X, 1.0f), std::min(c.Y, 1.0f), std::min(c.Z, 1.0f));
    }
    const bool sunset = kind == 1;
    const Vector3 horizon = sunset ? Vector3(1.0f, 0.55f, 0.25f) : Vector3(0.78f, 0.86f, 1.0f);
    const Vector3 zenith = sunset ? Vector3(0.18f, 0.2f, 0.5f) : Vector3(0.22f, 0.45f, 0.95f);
    const Vector3 ground = sunset ? Vector3(0.12f, 0.08f, 0.07f) : Vector3(0.28f, 0.25f, 0.22f);
    Vector3 c;
    if (up >= 0.0f) {
        const float t = std::pow(std::clamp(up, 0.0f, 1.0f), 0.55f);
        c = horizon + (zenith - horizon) * t;
    } else {
        const float t = std::pow(std::clamp(-up * 2.0f, 0.0f, 1.0f), 0.7f);
        c = horizon * 0.6f + (ground - horizon * 0.6f) * t;
    }
    const Vector3 sun = Vector3::Normalize(sunset ? Vector3(0.8f, 0.12f, 0.5f) : Vector3(0.4f, 0.65f, 0.3f));
    const float s = Vector3::Dot(d, sun);
    if (s > 0.995f) c = Vector3(1.0f, 0.97f, 0.85f);
    else if (s > 0.9f) c = c + Vector3(1.0f, 0.85f, 0.55f) * ((s - 0.9f) / 0.095f) * 0.6f;
    return Vector3(std::min(c.X, 1.0f), std::min(c.Y, 1.0f), std::min(c.Z, 1.0f));
}

Vector3 FaceDirection(CubeMapFace face, float u, float v)
{
    switch (face) {
        case CubeMapFace::PositiveX: return Vector3::Normalize(Vector3(1, -v, -u));
        case CubeMapFace::NegativeX: return Vector3::Normalize(Vector3(-1, -v, u));
        case CubeMapFace::PositiveY: return Vector3::Normalize(Vector3(u, 1, v));
        case CubeMapFace::NegativeY: return Vector3::Normalize(Vector3(u, -1, -v));
        case CubeMapFace::PositiveZ: return Vector3::Normalize(Vector3(u, -v, 1));
        default: return Vector3::Normalize(Vector3(-u, -v, -1));
    }
}

std::vector<Vector3> Sphere(int n)
{
    std::vector<Vector3> out;
    out.reserve(static_cast<std::size_t>(n));
    const float golden = MathHelper::Pi * (3.0f - std::sqrt(5.0f));
    for (int i = 0; i < n; ++i) {
        const float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
        const float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float a = golden * static_cast<float>(i);
        out.emplace_back(std::cos(a) * r, y, std::sin(a) * r);
    }
    return out;
}

Color ToColor(const Vector3& c)
{
    const auto b = [](float v) { return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return Color(b(c.X), b(c.Y), b(c.Z), 255);
}

}  // namespace

ProceduralIbl ProceduralIbl::Build(GraphicsDevice& device, int kind)
{
    ProceduralIbl ibl;
    constexpr int kSpecularSize = 64, kMips = 5, kIrradianceSize = 16;
    ibl.mipCount = kMips;
    const std::vector<Vector3> dense = Sphere(300);
    const CubeMapFace faces[6] = {CubeMapFace::PositiveX, CubeMapFace::NegativeX, CubeMapFace::PositiveY, CubeMapFace::NegativeY, CubeMapFace::PositiveZ, CubeMapFace::NegativeZ};

    // The environment sampled once per sample direction (the convolutions below reuse it).
    std::vector<Vector3> envOfDense;
    envOfDense.reserve(dense.size());
    for (const Vector3& s : dense) envOfDense.push_back(Env(s, kind));

    ibl.irradiance = std::make_unique<TextureCube>(device, kIrradianceSize, false, SurfaceFormat::Color);
    for (const CubeMapFace face : faces) {
        std::vector<Color> px(static_cast<std::size_t>(kIrradianceSize * kIrradianceSize));
        for (int y = 0; y < kIrradianceSize; ++y)
            for (int x = 0; x < kIrradianceSize; ++x) {
                const Vector3 n = FaceDirection(face, (static_cast<float>(x) + 0.5f) / kIrradianceSize * 2.0f - 1.0f, (static_cast<float>(y) + 0.5f) / kIrradianceSize * 2.0f - 1.0f);
                Vector3 sum(0, 0, 0);
                float weight = 0.0f;
                for (std::size_t i = 0; i < dense.size(); ++i) {
                    const float w = std::max(0.0f, Vector3::Dot(n, dense[i]));
                    sum = sum + envOfDense[i] * w;
                    weight += w;
                }
                px[static_cast<std::size_t>(y * kIrradianceSize + x)] = ToColor(sum / std::max(weight, 1e-4f));
            }
        ibl.irradiance->SetData(face, px.data(), static_cast<int>(px.size()));
    }

    ibl.specular = std::make_unique<TextureCube>(device, kSpecularSize, true, SurfaceFormat::Color);
    for (int mip = 0; mip < kMips; ++mip) {
        const int size = std::max(1, kSpecularSize >> mip);
        const float roughness = static_cast<float>(mip) / static_cast<float>(kMips - 1);
        const float shininess = mip == 0 ? 1.0e6f : std::max(2.0f, 2.0f / std::max(1e-4f, roughness * roughness * roughness * roughness) - 2.0f);
        for (const CubeMapFace face : faces) {
            std::vector<Color> px(static_cast<std::size_t>(size * size));
            for (int y = 0; y < size; ++y)
                for (int x = 0; x < size; ++x) {
                    const Vector3 r = FaceDirection(face, (static_cast<float>(x) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f, (static_cast<float>(y) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f);
                    if (mip == 0) {
                        px[static_cast<std::size_t>(y * size + x)] = ToColor(Env(r, kind));
                        continue;
                    }
                    Vector3 sum(0, 0, 0);
                    float weight = 0.0f;
                    for (std::size_t i = 0; i < dense.size(); ++i) {
                        const float c = Vector3::Dot(r, dense[i]);
                        if (c <= 0.0f) continue;
                        const float w = std::pow(c, std::min(shininess, 2000.0f));
                        sum = sum + envOfDense[i] * w;
                        weight += w;
                    }
                    px[static_cast<std::size_t>(y * size + x)] = weight > 1e-6f ? ToColor(sum / weight) : ToColor(Env(r, kind));
                }
            ibl.specular->SetData(face, mip, nullptr, px.data(), 0, static_cast<int>(px.size()));
        }
    }

    // Split-sum BRDF table (Karis' analytic fit): x = N.V, y = roughness, red = scale, green = bias.
    constexpr int kLut = 32;
    ibl.brdf = std::make_unique<Texture2D>(device, kLut, kLut);
    std::vector<Color> lut(static_cast<std::size_t>(kLut * kLut));
    for (int y = 0; y < kLut; ++y)
        for (int x = 0; x < kLut; ++x) {
            const float nv = std::max(0.02f, (static_cast<float>(x) + 0.5f) / kLut);
            const float rough = (static_cast<float>(y) + 0.5f) / kLut;
            const float r0 = rough * -1.0f + 1.0f, r1 = rough * -0.0275f + 0.0425f, r2 = rough * -0.572f + 1.04f, r3 = rough * 0.022f + -0.04f;
            const float a004 = std::min(r0 * r0, std::exp2(-9.28f * nv)) * r0 + r1;
            const float a = std::clamp(-1.04f * a004 + r2, 0.0f, 1.0f), b = std::clamp(1.04f * a004 + r3, 0.0f, 1.0f);
            lut[static_cast<std::size_t>(y * kLut + x)] = ToColor(Vector3(a, b, 0.0f));
        }
    ibl.brdf->SetData(lut.data(), static_cast<int>(lut.size()));
    return ibl;
}

ImageBasedLightEXT ProceduralIbl::Light(float intensity) const
{
    ImageBasedLightEXT l;
    l.Irradiance = irradiance.get();
    l.PrefilteredSpecular = specular.get();
    l.BrdfLut = brdf.get();
    l.PrefilteredMipCount = mipCount;
    l.Intensity = intensity;
    return l;
}
