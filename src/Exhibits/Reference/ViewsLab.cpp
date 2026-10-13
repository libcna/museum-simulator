// Reference scenes: the named viewpoints the visual regression tool compares. Pick one and the museum takes
// you there; tools/reference.sh captures the very same view headlessly and compares it with its stored image.
#include <string>

#include "Catalogue/ReferenceViews.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

class ViewsLab final : public ScreenExhibit {
public:
    ViewsLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Reference scenes", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        std::vector<std::string> names;
        for (const ReferenceView& v : ReferenceViews()) names.emplace_back(v.name);
        p.AddChoice("View", names, 0, [this](int v) { selected_ = v; });
        p.AddButton("Go there", [this] {
            if (S().showView) S().showView(ReferenceViews()[static_cast<std::size_t>(selected_)].name);
        });
        p.AddReadout("Room", [this] { return std::string(ReferenceViews()[static_cast<std::size_t>(selected_)].room); });
        p.AddReadout("Command line", [this] { return std::string("--view ") + ReferenceViews()[static_cast<std::size_t>(selected_)].name; });
        AddInfoSign(b, "Reference scenes", {"Each named view is a fixed pose in a fixed room at a fixed hour. tools/reference.sh renders all of", "them headlessly (--view NAME --time 11 --no-hud) and compares them with images stored per renderer."});
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text("Reference scenes", 24, 10, Kit::kGold, 1.0f);
        int i = 0;
        for (const ReferenceView& v : ReferenceViews()) {
            const float x = 24.0f + static_cast<float>(i / 12) * 470.0f, y = 62.0f + static_cast<float>(i % 12) * 38.0f;
            text(std::string(v.name) + "  -  " + v.shows, x, y, i == selected_ ? Kit::Rgba(255, 220, 120) : Kit::kText, 0.5f);
            ++i;
        }
        sb.End();
    }

private:
    int selected_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeViewsLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ViewsLab>(room, services);
}
