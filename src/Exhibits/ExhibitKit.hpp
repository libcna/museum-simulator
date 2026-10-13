#pragma once

// Small helpers shared by exhibit authors: palette, standard plates and text layout.
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Rendering/SignFactory.hpp"

namespace Kit {

/** Splits `text` into lines no wider than `maxPixels` at `scale` (greedy, on spaces). */
inline std::vector<std::string> Wrap(const Microsoft::Xna::Framework::Graphics::SpriteFont& font, const std::string& text, float maxPixels,
                                     float scale = 1.0f)
{
    std::vector<std::string> lines;
    std::string line, word;
    const auto flush = [&] {
        if (!line.empty()) lines.push_back(line);
        line.clear();
    };
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == ' ') {
            if (word.empty()) continue;
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && font.MeasureString(candidate).X * scale > maxPixels) {
                flush();
                line = word;
            } else {
                line = candidate;
            }
            word.clear();
        } else {
            word.push_back(text[i]);
        }
    }
    flush();
    return lines;
}


inline const Microsoft::Xna::Framework::Color kGold(255, 226, 150, 255);
inline const Microsoft::Xna::Framework::Color kText(222, 228, 240, 255);
inline const Microsoft::Xna::Framework::Color kDim(150, 160, 180, 255);
inline const Microsoft::Xna::Framework::Color kXna(120, 200, 255, 255);        // "XNA 4.0 compatible" badge
inline const Microsoft::Xna::Framework::Color kCnaExt(255, 160, 90, 255);      // "CNA extension" badge
inline const Microsoft::Xna::Framework::Color kApp(170, 230, 160, 255);        // application-level helper

/** A colour from ints (CNA's Color constructors are ambiguous for mixed integer types). */
inline Microsoft::Xna::Framework::Color Rgba(int r, int g, int b, int a = 255)
{
    return Microsoft::Xna::Framework::Color(static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(a));
}

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
