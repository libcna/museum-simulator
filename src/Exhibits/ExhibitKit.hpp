#pragma once

// Small helpers shared by exhibit authors: palette, standard plates and text layout.
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Rendering/SignFactory.hpp"

namespace Kit {

inline const Microsoft::Xna::Framework::Color kGold(255, 226, 150, 255);
inline const Microsoft::Xna::Framework::Color kText(222, 228, 240, 255);
inline const Microsoft::Xna::Framework::Color kDim(150, 160, 180, 255);
inline const Microsoft::Xna::Framework::Color kXna(120, 200, 255, 255);        // "XNA 4.0 compatible" badge
inline const Microsoft::Xna::Framework::Color kCnaExt(255, 160, 90, 255);      // "CNA extension" badge
inline const Microsoft::Xna::Framework::Color kApp(170, 230, 160, 255);        // application-level helper

/** A wing/floor accent as a sign colour. */
inline Microsoft::Xna::Framework::Color Accent(const Microsoft::Xna::Framework::Vector3& v, float k = 1.0f)
{
    const auto b = [&](float x) { return static_cast<std::uint8_t>(std::min(1.0f, x * k) * 255.0f); };
    return Microsoft::Xna::Framework::Color(b(v.X), b(v.Y), b(v.Z), static_cast<std::uint8_t>(255));
}

/** A title line followed by body lines: the standard look of an information plate. */
inline SignSpec Plate(const std::string& title, const std::vector<std::string>& body,
                      const Microsoft::Xna::Framework::Color& titleColor = kGold)
{
    SignSpec spec;
    spec.lines.push_back({title, true, titleColor});
    for (const std::string& line : body) spec.lines.push_back({line, false, kText});
    return spec;
}

}  // namespace Kit
