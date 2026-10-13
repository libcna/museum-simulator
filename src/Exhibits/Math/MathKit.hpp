#pragma once

// Shared pieces of the mathematics exhibits: a plinth with a stage above it, spherical vector
// controls, and number formatting for the live read-outs.
#include <cmath>
#include <cstdio>
#include <string>

#include "Exhibits/Exhibit.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Rendering/Gizmos.hpp"
#include "World/RoomBuilder.hpp"

namespace MathKit {

inline std::string Fmt(const Microsoft::Xna::Framework::Vector3& v)
{
    char b[96];
    std::snprintf(b, sizeof b, "(%.2f, %.2f, %.2f)", static_cast<double>(v.X), static_cast<double>(v.Y), static_cast<double>(v.Z));
    return b;
}
inline std::string Fmt(float v, const char* unit = "")
{
    char b[48];
    std::snprintf(b, sizeof b, "%.3f%s", static_cast<double>(v), unit);
    return b;
}
inline std::string FmtDeg(float radians) { return Fmt(Microsoft::Xna::Framework::MathHelper::ToDegrees(radians), " deg"); }

/** A vector described by length, heading (yaw, degrees) and elevation (pitch, degrees) so a slider can drive it. */
struct SphericalVector {
    float length = 1.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 Value() const
    {
        const float y = Microsoft::Xna::Framework::MathHelper::ToRadians(yaw);
        const float p = Microsoft::Xna::Framework::MathHelper::ToRadians(pitch);
        return Microsoft::Xna::Framework::Vector3(std::cos(p) * std::cos(y), std::sin(p), std::cos(p) * std::sin(y)) * length;
    }
};

/** Colours used consistently across the maths rooms. */
inline const Microsoft::Xna::Framework::Vector3 kRed(0.95f, 0.25f, 0.22f), kBlue(0.25f, 0.5f, 0.98f), kGreen(0.3f, 0.85f, 0.35f),
    kGold(0.98f, 0.8f, 0.25f), kViolet(0.75f, 0.4f, 0.95f), kWhite(0.92f, 0.92f, 0.95f);

inline Microsoft::Xna::Framework::Color ToColor(const Microsoft::Xna::Framework::Vector3& v)
{
    return Microsoft::Xna::Framework::Color(static_cast<std::uint8_t>(std::min(1.0f, v.X) * 255), static_cast<std::uint8_t>(std::min(1.0f, v.Y) * 255),
                                            static_cast<std::uint8_t>(std::min(1.0f, v.Z) * 255), static_cast<std::uint8_t>(255));
}

/** The plinth the stage floats above, and a plaque with the API and what to try. */
inline Microsoft::Xna::Framework::Vector3 BuildStagePlinth(RoomBuilder& b, float x, float z, float height = 0.9f)
{
    return b.AddPedestal(x, z, 1.4f, height, "marble");
}

}  // namespace MathKit
