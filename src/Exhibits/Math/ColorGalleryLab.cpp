// Colour gallery: the 141 named colours of XNA's Color struct (Color.Transparent, Color.AliceBlue ... Color.YellowGreen), laid out in
// their declaration order, by hue or by brightness. Every swatch is the real static member read from CNA; the selected one is
// described through the struct's own accessors (R, G, B, A, PackedValue, ToString).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

struct Named {
    const char* name;
    const Color* color;
    float hue, brightness;
};

#define MUSEUM_NAMED_COLORS(X) \
    X(Transparent) X(AliceBlue) X(AntiqueWhite) X(Aqua) X(Aquamarine) X(Azure) X(Beige) X(Bisque) X(Black) X(BlanchedAlmond) X(Blue) \
    X(BlueViolet) X(Brown) X(BurlyWood) X(CadetBlue) X(Chartreuse) X(Chocolate) X(Coral) X(CornflowerBlue) X(Cornsilk) X(Crimson) X(Cyan) \
    X(DarkBlue) X(DarkCyan) X(DarkGoldenrod) X(DarkGray) X(DarkGreen) X(DarkKhaki) X(DarkMagenta) X(DarkOliveGreen) X(DarkOrange) \
    X(DarkOrchid) X(DarkRed) X(DarkSalmon) X(DarkSeaGreen) X(DarkSlateBlue) X(DarkSlateGray) X(DarkTurquoise) X(DarkViolet) X(DeepPink) \
    X(DeepSkyBlue) X(DimGray) X(DodgerBlue) X(Firebrick) X(FloralWhite) X(ForestGreen) X(Fuchsia) X(Gainsboro) X(GhostWhite) X(Gold) \
    X(Goldenrod) X(Gray) X(Green) X(GreenYellow) X(Honeydew) X(HotPink) X(IndianRed) X(Indigo) X(Ivory) X(Khaki) X(Lavender) \
    X(LavenderBlush) X(LawnGreen) X(LemonChiffon) X(LightBlue) X(LightCoral) X(LightCyan) X(LightGoldenrodYellow) X(LightGray) \
    X(LightGreen) X(LightPink) X(LightSalmon) X(LightSeaGreen) X(LightSkyBlue) X(LightSlateGray) X(LightSteelBlue) X(LightYellow) X(Lime) \
    X(LimeGreen) X(Linen) X(Magenta) X(Maroon) X(MediumAquamarine) X(MediumBlue) X(MediumOrchid) X(MediumPurple) X(MediumSeaGreen) \
    X(MediumSlateBlue) X(MediumSpringGreen) X(MediumTurquoise) X(MediumVioletRed) X(MidnightBlue) X(MintCream) X(MistyRose) X(Moccasin) \
    X(NavajoWhite) X(Navy) X(OldLace) X(Olive) X(OliveDrab) X(Orange) X(OrangeRed) X(Orchid) X(PaleGoldenrod) X(PaleGreen) X(PaleTurquoise) \
    X(PaleVioletRed) X(PapayaWhip) X(PeachPuff) X(Peru) X(Pink) X(Plum) X(PowderBlue) X(Purple) X(Red) X(RosyBrown) X(RoyalBlue) \
    X(SaddleBrown) X(Salmon) X(SandyBrown) X(SeaGreen) X(SeaShell) X(Sienna) X(Silver) X(SkyBlue) X(SlateBlue) X(SlateGray) X(Snow) \
    X(SpringGreen) X(SteelBlue) X(Tan) X(Teal) X(Thistle) X(Tomato) X(Turquoise) X(Violet) X(Wheat) X(White) X(WhiteSmoke) X(Yellow) \
    X(YellowGreen)

std::vector<Named> MakeColors()
{
    std::vector<Named> all;
#define X(n) all.push_back({#n, &Color::n, 0.0f, 0.0f});
    MUSEUM_NAMED_COLORS(X)
#undef X
    for (Named& c : all) {
        const float r = c.color->getRProperty() / 255.0f, g = c.color->getGProperty() / 255.0f, b = c.color->getBProperty() / 255.0f;
        const float hi = std::max({r, g, b}), lo = std::min({r, g, b}), d = hi - lo;
        float h = 0.0f;
        if (d > 0.0f) {
            if (hi == r) h = std::fmod((g - b) / d, 6.0f);
            else if (hi == g) h = (b - r) / d + 2.0f;
            else h = (r - g) / d + 4.0f;
            h /= 6.0f;
            if (h < 0.0f) h += 1.0f;
        }
        // Greys and white sort before the coloured ones, black first, so each order reads as a sweep.
        c.hue = d < 0.04f ? -1.0f + hi : h;
        c.brightness = 0.299f * r + 0.587f * g + 0.114f * b;
    }
    return all;
}

class ColorGalleryLab final : public ScreenExhibit {
public:
    ColorGalleryLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), colors_(MakeColors()) { Reorder(); }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Named colours", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddChoice("Order", {"declaration order", "by hue", "by brightness"}, 0, [this](int v) { order_ = v; Reorder(); });
        p.AddSlider("Selected swatch", 0.0f, static_cast<float>(colors_.size() - 1), 0.0f, [this](float v) { selected_ = static_cast<int>(v); }, "%.0f", 1.0f);
        p.AddReadout("Color.", [this] { return std::string(Current().name); });
        p.AddReadout("R G B A", [this] { return Components(); });
        p.AddReadout("PackedValue", [this] { return Packed(); });
        p.AddReadout("Closest other name", [this] { return Closest(); });
        AddInfoSign(b, "XNA's named colours",
                    {"Color has 141 static members: Transparent and the 140 colours of the web's CSS list (AliceBlue to YellowGreen), each a straight-alpha 32-bit value.",
                     "Pick a swatch with the slider; reorder by hue or brightness to see how the list is built. Every swatch below is read from CNA's own Color members."});
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text("Color.* in XNA", 24, 8, Kit::kGold, 0.85f);
        constexpr int columns = 12;
        constexpr float cell = 52.0f;
        for (std::size_t slot = 0; slot < order_list_.size(); ++slot) {
            const float x = 24.0f + static_cast<float>(static_cast<int>(slot) % columns) * cell, y = 52.0f + static_cast<float>(static_cast<int>(slot) / columns) * 40.0f;
            const Named& n = colors_[static_cast<std::size_t>(order_list_[slot])];
            // A checkerboard behind the swatch shows through Color.Transparent.
            rect(x, y, cell - 4.0f, 36.0f, Color(70, 70, 78, 255));
            rect(x, y, (cell - 4.0f) / 2.0f, 18.0f, Color(110, 110, 120, 255));
            rect(x + (cell - 4.0f) / 2.0f, y + 18.0f, (cell - 4.0f) / 2.0f, 18.0f, Color(110, 110, 120, 255));
            rect(x, y, cell - 4.0f, 36.0f, *n.color);
            if (static_cast<int>(slot) == selected_) {
                rect(x - 3.0f, y - 3.0f, cell + 2.0f, 3.0f, Kit::kGold);
                rect(x - 3.0f, y + 36.0f, cell + 2.0f, 3.0f, Kit::kGold);
                rect(x - 3.0f, y, 3.0f, 36.0f, Kit::kGold);
                rect(x + cell - 4.0f, y, 3.0f, 36.0f, Kit::kGold);
            }
        }
        const Named& cur = Current();
        rect(670.0f, 52.0f, 270.0f, 150.0f, Color(70, 70, 78, 255));
        rect(670.0f, 52.0f, 270.0f, 150.0f, *cur.color);
        text(cur.name, 670, 212, Kit::kText, 0.8f);
        text(Components(), 670, 252, Kit::kDim, 0.55f);
        text(Packed(), 670, 280, Kit::kDim, 0.55f);
        text("closest other: " + Closest(), 670, 308, Kit::kDim, 0.55f);
        text("Color::ToString:", 670, 346, Kit::kDim, 0.55f);
        text(cur.color->ToString(), 670, 372, Kit::kText, 0.5f);
        sb.End();
    }

private:
    void Reorder()
    {
        order_list_.resize(colors_.size());
        for (std::size_t i = 0; i < order_list_.size(); ++i) order_list_[i] = static_cast<int>(i);
        if (order_ == 1) std::stable_sort(order_list_.begin(), order_list_.end(), [this](int a, int b) { const Named& x = colors_[static_cast<std::size_t>(a)];
            const Named& y = colors_[static_cast<std::size_t>(b)];
            return std::floor(x.hue * 36.0f) != std::floor(y.hue * 36.0f) ? x.hue < y.hue : x.brightness < y.brightness;   // 36 hue bands, dark to light inside each
        });
        if (order_ == 2) std::stable_sort(order_list_.begin(), order_list_.end(), [this](int a, int b) { return colors_[static_cast<std::size_t>(a)].brightness < colors_[static_cast<std::size_t>(b)].brightness; });
    }
    [[nodiscard]] const Named& Current() const { return colors_[static_cast<std::size_t>(order_list_[static_cast<std::size_t>(std::clamp(selected_, 0, static_cast<int>(order_list_.size()) - 1))])]; }
    [[nodiscard]] std::string Components() const
    {
        const Color& c = *Current().color;
        char b[64];
        std::snprintf(b, sizeof b, "R %d  G %d  B %d  A %d", c.getRProperty(), c.getGProperty(), c.getBProperty(), c.getAProperty());
        return b;
    }
    [[nodiscard]] std::string Packed() const
    {
        char b[48];
        std::snprintf(b, sizeof b, "PackedValue 0x%08X", static_cast<unsigned>(Current().color->getPackedValueProperty()));
        return b;
    }
    [[nodiscard]] std::string Closest() const
    {
        const Color& c = *Current().color;
        const Named* best = nullptr;
        int bestDistance = 1 << 30;
        for (const Named& n : colors_) {
            if (n.color->getPackedValueProperty() == c.getPackedValueProperty()) continue;
            const int dr = n.color->getRProperty() - c.getRProperty(), dg = n.color->getGProperty() - c.getGProperty(), db = n.color->getBProperty() - c.getBProperty();
            const int d = dr * dr + dg * dg + db * db;
            if (d < bestDistance) { bestDistance = d; best = &n; }
        }
        return best != nullptr ? best->name : "-";
    }

    std::vector<Named> colors_;
    std::vector<int> order_list_;
    int order_ = 0, selected_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeColorGalleryLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ColorGalleryLab>(room, services);
}
