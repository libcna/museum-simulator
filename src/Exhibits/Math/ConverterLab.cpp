// Converter laboratory: Microsoft.Xna.Framework.Design, the type converters XNA's tools use to show and edit a Vector3 or a
// Color as text ("1.5, 2.25, -3.75"). The text depends on the culture (a Czech list separator is a semicolon, because the
// decimal mark is a comma), and what a converter refuses it refuses with a message.
#include <algorithm>
#include <any>
#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"

#ifdef MUSEUM_DESIGN
#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Plane.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"
#include "Microsoft/Xna/Framework/Design/BoundingBoxConverter.hpp"
#include "Microsoft/Xna/Framework/Design/BoundingSphereConverter.hpp"
#include "Microsoft/Xna/Framework/Design/ColorConverter.hpp"
#include "Microsoft/Xna/Framework/Design/MatrixConverter.hpp"
#include "Microsoft/Xna/Framework/Design/PlaneConverter.hpp"
#include "Microsoft/Xna/Framework/Design/PointConverter.hpp"
#include "Microsoft/Xna/Framework/Design/QuaternionConverter.hpp"
#include "Microsoft/Xna/Framework/Design/RayConverter.hpp"
#include "Microsoft/Xna/Framework/Design/RectangleConverter.hpp"
#include "Microsoft/Xna/Framework/Design/Vector2Converter.hpp"
#include "Microsoft/Xna/Framework/Design/Vector3Converter.hpp"
#include "Microsoft/Xna/Framework/Design/Vector4Converter.hpp"
#include "Microsoft/Xna/Framework/Point.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"
#include "System/Globalization/CultureInfo.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
namespace Design = Microsoft::Xna::Framework::Design;

namespace {

struct Kind {
    const char* name;
    int parts;
    std::function<std::any(const float*)> make;
    std::function<std::unique_ptr<Design::MathTypeConverter>()> converter;
};

std::vector<Kind> Kinds()
{
    const auto i = [](float v) { return static_cast<int>(std::lround(v)); };
    const auto byte = [&](float v) { return static_cast<std::uint8_t>(std::clamp(i(v * 25.5f), 0, 255)); };
    return {
        {"Vector2", 2, [](const float* v) { return std::any(Vector2(v[0], v[1])); }, [] { return std::make_unique<Design::Vector2Converter>(); }},
        {"Vector3", 3, [](const float* v) { return std::any(Vector3(v[0], v[1], v[2])); }, [] { return std::make_unique<Design::Vector3Converter>(); }},
        {"Vector4", 4, [](const float* v) { return std::any(Vector4(v[0], v[1], v[2], v[3])); }, [] { return std::make_unique<Design::Vector4Converter>(); }},
        {"Quaternion", 4, [](const float* v) { return std::any(Quaternion(v[0], v[1], v[2], v[3])); }, [] { return std::make_unique<Design::QuaternionConverter>(); }},
        {"Color", 4, [=](const float* v) { return std::any(Color(byte(std::abs(v[0])), byte(std::abs(v[1])), byte(std::abs(v[2])), byte(std::abs(v[3])))); }, [] { return std::make_unique<Design::ColorConverter>(); }},
        {"Rectangle", 4, [=](const float* v) { return std::any(Rectangle(i(v[0]), i(v[1]), i(std::abs(v[2])), i(std::abs(v[3])))); }, [] { return std::make_unique<Design::RectangleConverter>(); }},
        {"Point", 2, [=](const float* v) { return std::any(Point(i(v[0]), i(v[1]))); }, [] { return std::make_unique<Design::PointConverter>(); }},
        // Six components: a corner and an extent for the box, a position and a direction for the ray.
        {"BoundingBox", 6, [](const float* v) { return std::any(BoundingBox(Vector3(v[0], v[1], v[2]), Vector3(v[0] + std::abs(v[3]), v[1] + std::abs(v[4]), v[2] + std::abs(v[5])))); }, [] { return std::make_unique<Design::BoundingBoxConverter>(); }},
        {"BoundingSphere", 4, [](const float* v) { return std::any(BoundingSphere(Vector3(v[0], v[1], v[2]), std::abs(v[3]))); }, [] { return std::make_unique<Design::BoundingSphereConverter>(); }},
        {"Plane", 4, [](const float* v) { return std::any(Plane(Vector3(v[0], v[1], v[2]), v[3])); }, [] { return std::make_unique<Design::PlaneConverter>(); }},
        {"Ray", 6, [](const float* v) { return std::any(Ray(Vector3(v[0], v[1], v[2]), Vector3(v[3], v[4], v[5]))); }, [] { return std::make_unique<Design::RayConverter>(); }},
        // A turn about Y (component 4, radians) followed by a translation.
        {"Matrix", 4, [](const float* v) { return std::any(Matrix::CreateRotationY(v[3] * 0.25f) * Matrix::CreateTranslation(v[0], v[1], v[2])); }, [] { return std::make_unique<Design::MatrixConverter>(); }},
    };
}

class ConverterLab final : public ScreenExhibit {
public:
    ConverterLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), kinds_(Kinds()) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Type converters", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddChoice("Type", {"Vector2", "Vector3", "Vector4", "Quaternion", "Color", "Rectangle", "Point", "BoundingBox", "BoundingSphere", "Plane", "Ray", "Matrix"}, 1, [this](int v) { kind_ = v; Recompute(); });
        p.AddChoice("CultureInfo", {"invariant", "en-US", "cs-CZ", "de-DE"}, 0, [this](int v) { culture_ = v; Recompute(); });
        static const char* names[6] = {"Component 1", "Component 2", "Component 3", "Component 4", "Component 5 (box, ray only)", "Component 6 (box, ray only)"};
        for (int c = 0; c < 6; ++c) p.AddSlider(names[c], -10.0f, 10.0f, kDefaults[c], [this, c](float v) { comp_[c] = v; Recompute(); }, "%.2f", 0.25f);
        p.AddChoice("Text to read back", {"what ConvertToString wrote", "with the other culture's separator", "one part too few", "one part too many", "words", "nothing"}, 0, [this](int v) { text_ = v; Recompute(); });
        p.AddButton("Set the first property to 9 (PropertyDescriptor)", [this] { SetFirst(); });
        p.AddReadout("ConvertToString", [this] { return written_; });
        p.AddReadout("ConvertFromString", [this] { return read_; });
        p.AddReadout("Properties", [this] { return properties_; });
        AddInfoSign(b, "Type Converters",
                    {"Microsoft.Xna.Framework.Design holds the converters a designer uses to edit a Vector3 or a Color as text. ConvertToString writes the culture's own list separator,",
                     "ConvertFromString accepts only what the same culture would write, and each component is a PropertyDescriptor the converter can read and set."});
    }

    void OnScreenLoaded() override { Recompute(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(std::string(kinds_[kind_].name) + "Converter", 24, 12, Kit::kGold, 1.0f);
        const auto block = [&](float y, const std::string& head, const std::string& body, Color c) {
            rect(24, y, 912, 120, Color(22, 30, 46, 235));
            rect(24, y, 6, 120, c);
            text(head, 44, y + 8, Kit::kDim, 0.55f);
            float ty = y + 36.0f;
            for (const std::string& line : Kit::Wrap(*S().font, body, 860.0f, 0.8f)) {
                text(line, 44, ty, Kit::kText, 0.8f);
                ty += 30.0f;
            }
        };
        block(62, std::string("ConvertToString(culture \"") + cultureName() + "\", value)", "\"" + written_ + "\"", Kit::Rgba(110, 170, 255));
        block(196, "ConvertFromString(culture, \"" + (shownText_.size() > 60 ? shownText_.substr(0, 57) + "..." : shownText_) + "\")", read_, ok_ ? Kit::Rgba(110, 230, 150) : Kit::Rgba(255, 150, 120));
        block(330, "GetProperties(value)", properties_, Kit::kGold);
        text(std::string("CanConvertFrom(string): ") + (canFrom_ ? "true" : "false") + "     GetCreateInstanceSupported: true", 24, 470, Kit::kDim, 0.6f);
        sb.End();
    }

private:
    static constexpr float kDefaults[6] = {1.5f, 2.25f, -3.75f, 4.0f, 2.0f, 1.0f};

    [[nodiscard]] const char* cultureName() const
    {
        static const char* n[] = {"", "en-US", "cs-CZ", "de-DE"};
        return n[culture_];
    }
    [[nodiscard]] System::Globalization::CultureInfo Culture(int which) const
    {
        static const char* n[] = {"", "en-US", "cs-CZ", "de-DE"};
        return System::Globalization::CultureInfo(n[which]);
    }

    void Recompute()
    {
        try {
            const Kind& k = kinds_[kind_];
            const auto conv = k.converter();
            const System::Globalization::CultureInfo culture = Culture(culture_);
            value_ = k.make(comp_);
            canFrom_ = conv->CanConvertFrom(System::Type::From<std::string>());
            written_ = conv->ConvertToString(nullptr, &culture, value_);
            // The strings a reader is offered.
            const std::string sep = culture_ == 2 ? "; " : ", ";
            const std::string other = culture_ == 2 ? ", " : "; ";
            std::string shown;
            switch (text_) {
                case 0: shown = written_; break;
                case 1: shown = Swap(written_, sep, other); break;
                case 2: shown = DropLast(written_, sep); break;
                case 3: shown = written_ + sep + "7"; break;
                case 4: shown = "not a valid component list"; break;
                default: shown = ""; break;
            }
            shownText_ = shown;
            try {
                const std::any back = conv->ConvertFromString(nullptr, &culture, shown);
                ok_ = true;
                read_ = "returned " + conv->ConvertToString(nullptr, &culture, back) + (text_ == 0 ? "   (the value you set)" : "");
            } catch (const std::exception& e) {
                ok_ = false;
                read_ = std::string("refused: ") + e.what();
            }
            properties_.clear();
            std::any boxed = value_;
            const auto props = conv->GetProperties(boxed);
            for (const auto& d : props) {
                if (!properties_.empty()) properties_ += "    ";
                properties_ += d->getNameProperty() + " = " + Format(d->GetValue(boxed));
            }
        } catch (const std::exception& e) {
            ok_ = false;
            read_ = std::string("failed: ") + e.what();
        }
    }

    void SetFirst()
    {
        try {
            const Kind& k = kinds_[kind_];
            const auto conv = k.converter();
            std::any boxed = k.make(comp_);
            const auto props = conv->GetProperties(boxed);
            for (const auto& d : props) {
                const System::Type t = d->getPropertyTypeProperty();
                if (t == System::Type::From<float>()) d->SetValue(boxed, std::any(9.0f));
                else if (t == System::Type::From<int>() || t == System::Type::From<std::int32_t>()) d->SetValue(boxed, std::any(static_cast<std::int32_t>(9)));
                else if (t == System::Type::From<std::uint8_t>()) d->SetValue(boxed, std::any(static_cast<std::uint8_t>(9)));
                else continue;
                break;
            }
            const System::Globalization::CultureInfo culture = Culture(culture_);
            read_ = "after PropertyDescriptor.SetValue: " + conv->ConvertToString(nullptr, &culture, boxed);
            ok_ = true;
        } catch (const std::exception& e) {
            ok_ = false;
            read_ = std::string("SetValue: ") + e.what();
        }
    }

    [[nodiscard]] static std::string Format(const std::any& v)
    {
        char buf[96];
        if (const float* f = std::any_cast<float>(&v)) std::snprintf(buf, sizeof buf, "%g", static_cast<double>(*f));
        else if (const std::int32_t* n = std::any_cast<std::int32_t>(&v)) std::snprintf(buf, sizeof buf, "%d", *n);
        else if (const std::uint8_t* u = std::any_cast<std::uint8_t>(&v)) std::snprintf(buf, sizeof buf, "%d", *u);
        else if (const Vector3* v3 = std::any_cast<Vector3>(&v)) std::snprintf(buf, sizeof buf, "(%g, %g, %g)", static_cast<double>(v3->X), static_cast<double>(v3->Y), static_cast<double>(v3->Z));
        else if (const Vector4* v4 = std::any_cast<Vector4>(&v)) std::snprintf(buf, sizeof buf, "(%g, %g, %g, %g)", static_cast<double>(v4->X), static_cast<double>(v4->Y), static_cast<double>(v4->Z), static_cast<double>(v4->W));
        else return "?";
        return buf;
    }
    [[nodiscard]] static std::string Swap(std::string s, const std::string& a, const std::string& b)
    {
        for (std::size_t at = 0; (at = s.find(a, at)) != std::string::npos; at += b.size()) s.replace(at, a.size(), b);
        return s;
    }
    [[nodiscard]] static std::string DropLast(const std::string& s, const std::string& sep)
    {
        const std::size_t at = s.rfind(sep);
        return at == std::string::npos ? s : s.substr(0, at);
    }

    std::vector<Kind> kinds_;
    int kind_ = 1, culture_ = 0, text_ = 0;
    float comp_[6] = {kDefaults[0], kDefaults[1], kDefaults[2], kDefaults[3], kDefaults[4], kDefaults[5]};
    std::any value_;
    std::string written_ = "-", read_ = "-", properties_ = "-", shownText_;
    bool ok_ = true, canFrom_ = false;
};

}  // namespace

#else   // CNA's Design library is not part of this build

namespace {
class ConverterLab final : public ScreenExhibit {
public:
    ConverterLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}
    void BuildStatic(RoomBuilder& b) override { AddInfoSign(b, "Type Converters", {"This museum was built without CNA's Design library (cna_design), which holds Vector3Converter and its relatives."}); }
protected:
    void DrawScreen(Microsoft::Xna::Framework::Graphics::SpriteBatch&) override {}
};
}  // namespace
#endif

std::unique_ptr<Exhibit> MakeConverterLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ConverterLab>(room, services);
}
