#include "UI/FloorMap.hpp"

#include <algorithm>
#include <cmath>

#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

struct Frame {
    float cx, cz, scale;   // plan centre (metres) and pixels per metre
    Rectangle area;
};

Frame MakeFrame(const MuseumPlan& museum, const MapView& view, const Rectangle& area)
{
    Frame f{0, 0, 1, area};
    if (view.follow) {
        f.cx = view.player.X;
        f.cz = view.player.Z;
        f.scale = static_cast<float>(std::min(area.Width, area.Height)) / (2.0f * view.followRadius);
        return f;
    }
    float x0, z0, x1, z1;
    FloorMap::FloorBounds(museum, view.floor, x0, z0, x1, z1);
    const float margin = 2.0f;
    f.cx = (x0 + x1) * 0.5f;
    f.cz = (z0 + z1) * 0.5f;
    f.scale = std::min(static_cast<float>(area.Width) / (x1 - x0 + margin * 2.0f), static_cast<float>(area.Height) / (z1 - z0 + margin * 2.0f));
    return f;
}

Vector2 ToPixel(const Frame& f, float x, float z)
{
    return Vector2(static_cast<float>(f.area.X) + static_cast<float>(f.area.Width) * 0.5f + (x - f.cx) * f.scale,
                   static_cast<float>(f.area.Y) + static_cast<float>(f.area.Height) * 0.5f + (z - f.cz) * f.scale);
}

Color Shade(const Vector3& c, float k, int a = 255)
{
    const auto b = [&](float v) { return static_cast<std::uint8_t>(std::clamp(v * k, 0.0f, 1.0f) * 255.0f); };
    return Color(b(c.X), b(c.Y), b(c.Z), static_cast<std::uint8_t>(a));
}

// Fills a rectangle given in pixels, clipped to the plan area.
void Fill(Services& s, const Rectangle& area, float x0, float y0, float x1, float y1, Color c)
{
    const float ax0 = std::max(x0, static_cast<float>(area.X));
    const float ay0 = std::max(y0, static_cast<float>(area.Y));
    const float ax1 = std::min(x1, static_cast<float>(area.X + area.Width));
    const float ay1 = std::min(y1, static_cast<float>(area.Y + area.Height));
    if (ax1 - ax0 < 0.5f || ay1 - ay0 < 0.5f) return;
    s.sprites->Draw(*s.textures->White(), Rectangle(static_cast<int>(ax0), static_cast<int>(ay0),
                                                    std::max(1, static_cast<int>(ax1 - ax0 + 0.5f)), std::max(1, static_cast<int>(ay1 - ay0 + 0.5f))), c);
}

}  // namespace

namespace FloorMap {

void FloorBounds(const MuseumPlan& museum, int floor, float& minX, float& minZ, float& maxX, float& maxZ)
{
    minX = minZ = 1e9f;
    maxX = maxZ = -1e9f;
    for (const int i : museum.CellsOnFloor(floor)) {
        const RoomDef& r = museum.Room(i);
        minX = std::min(minX, r.centre.X - r.halfW);
        maxX = std::max(maxX, r.centre.X + r.halfW);
        minZ = std::min(minZ, r.centre.Z - r.halfD);
        maxZ = std::max(maxZ, r.centre.Z + r.halfD);
    }
}

float Draw(Services& s, const MuseumPlan& museum, const MapView& view, const Rectangle& area)
{
    const Frame f = MakeFrame(museum, view, area);
    const FloorDef& floor = museum.floors[static_cast<std::size_t>(view.floor)];
    const std::vector<int>& cells = museum.CellsOnFloor(view.floor);
    const auto lit = [&](int i) { return view.discovered == nullptr || view.discovered->count(i) > 0; };

    // Cells: wing colour for rooms, lighter for circulation. Undiscovered cells are drawn dim.
    for (const int i : cells) {
        const RoomDef& r = museum.Room(i);
        const Vector2 a = ToPixel(f, r.centre.X - r.halfW - kWallThickness * 0.5f, r.centre.Z - r.halfD - kWallThickness * 0.5f);
        const Vector2 b = ToPixel(f, r.centre.X + r.halfW + kWallThickness * 0.5f, r.centre.Z + r.halfD + kWallThickness * 0.5f);
        const bool circulation = r.kind != CellKind::Room;
        const float k = lit(i) ? (circulation ? 0.55f : 0.78f) : 0.22f;
        Vector3 colour = r.kind == CellKind::Room || r.kind == CellKind::Gallery ? r.accent : floor.accent;
        if (r.kind == CellKind::Elevator) colour = Vector3(0.7f, 0.75f, 0.8f);
        Fill(s, area, a.X, a.Y, b.X, b.Y, Color(14, 18, 28, 255));
        Fill(s, area, a.X + 1, a.Y + 1, b.X - 1, b.Y - 1, Shade(colour, k));
        if (view.highlightWing >= 0 && r.wing == view.highlightWing)
            Fill(s, area, a.X + 1, a.Y + 1, b.X - 1, a.Y + 3, Color(255, 255, 255, 230));
        if (i == view.selectedCell) {
            const Color sel(255, 255, 255, 255);
            Fill(s, area, a.X - 2, a.Y - 2, b.X + 2, a.Y, sel);
            Fill(s, area, a.X - 2, b.Y, b.X + 2, b.Y + 2, sel);
            Fill(s, area, a.X - 2, a.Y, a.X, b.Y, sel);
            Fill(s, area, b.X, a.Y, b.X + 2, b.Y, sel);
        }
        if (i == view.currentCell) {
            const Color cur(255, 214, 90, 255);
            Fill(s, area, a.X, a.Y, b.X, a.Y + 2, cur);
            Fill(s, area, a.X, b.Y - 2, b.X, b.Y, cur);
            Fill(s, area, a.X, a.Y, a.X + 2, b.Y, cur);
            Fill(s, area, b.X - 2, a.Y, b.X, b.Y, cur);
        }
    }

    // Doors: a light mark across each opening, drawn once per pair.
    for (const int i : cells) {
        const RoomDef& r = museum.Room(i);
        for (const DoorDef& d : r.doors) {
            if (d.to < i) continue;
            const BoundingBox bb = r.DoorBounds(d);
            const Vector2 a = ToPixel(f, bb.Min.X, bb.Min.Z);
            const Vector2 b = ToPixel(f, bb.Max.X, bb.Max.Z);
            const bool acrossX = (d.wall == Wall::North || d.wall == Wall::South);
            const float grow = std::max(1.0f, f.scale * 0.15f);
            if (acrossX) Fill(s, area, a.X, a.Y - grow, b.X, b.Y + grow, Color(235, 225, 190, 255));
            else Fill(s, area, a.X - grow, a.Y, b.X + grow, b.Y, Color(235, 225, 190, 255));
        }
    }

    // Labels: room numbers where they fit, names when the cell is large on screen.
    if (view.labels && s.font != nullptr) {
        for (const int i : cells) {
            const RoomDef& r = museum.Room(i);
            if (!lit(i) || r.kind == CellKind::Elevator) continue;
            const Vector2 c = ToPixel(f, r.centre.X, r.centre.Z);
            const float wpx = r.halfW * 2.0f * f.scale;
            const float hpx = r.halfD * 2.0f * f.scale;
            std::string text = r.number;
            float fs = 0.5f;
            if (r.kind == CellKind::Room && wpx > 90.0f && hpx > 44.0f) {
                text = r.title;
                fs = 0.5f;
                while (fs > 0.3f && s.font->MeasureString(text).X * fs > wpx - 6.0f) fs -= 0.04f;
            } else if (r.kind != CellKind::Room && wpx < 40.0f && hpx < 40.0f) {
                continue;
            }
            const Vector2 size = s.font->MeasureString(text) * fs;
            if (size.X > wpx && size.X > hpx) continue;
            const Vector2 p(c.X - size.X * 0.5f, c.Y - size.Y * 0.5f);
            const float ax = static_cast<float>(area.X), ay = static_cast<float>(area.Y);
            if (p.X < ax || p.Y < ay || p.X + size.X > ax + static_cast<float>(area.Width) || p.Y + size.Y > ay + static_cast<float>(area.Height)) continue;
            s.sprites->DrawString(*s.font, text, p + Vector2(1, 1), Color(0, 0, 0, 220), 0.0f, Vector2(0, 0), fs, SpriteEffects::None, 0.0f);
            s.sprites->DrawString(*s.font, text, p, Color(240, 244, 250, 255), 0.0f, Vector2(0, 0), fs, SpriteEffects::None, 0.0f);
        }
    }

    // The visitor: a dot with a heading line.
    if (view.showPlayer) {
        const Vector2 me = ToPixel(f, view.player.X, view.player.Z);
        const float fx = -std::sin(view.yaw);
        const float fz = -std::cos(view.yaw);
        for (int i = 1; i <= 6; ++i)
            Fill(s, area, me.X + fx * static_cast<float>(i) * 2.0f - 1, me.Y + fz * static_cast<float>(i) * 2.0f - 1,
                 me.X + fx * static_cast<float>(i) * 2.0f + 2, me.Y + fz * static_cast<float>(i) * 2.0f + 2, Color(255, 220, 100, 255));
        Fill(s, area, me.X - 3, me.Y - 3, me.X + 4, me.Y + 4, Color(255, 70, 50, 255));
    }
    return f.scale;
}

int CellAtPixel(const MuseumPlan& museum, const MapView& view, const Rectangle& area, int px, int py)
{
    const Frame f = MakeFrame(museum, view, area);
    const float x = f.cx + (static_cast<float>(px) - static_cast<float>(area.X) - static_cast<float>(area.Width) * 0.5f) / f.scale;
    const float z = f.cz + (static_cast<float>(py) - static_cast<float>(area.Y) - static_cast<float>(area.Height) * 0.5f) / f.scale;
    for (const int i : museum.CellsOnFloor(view.floor)) {
        const RoomDef& r = museum.Room(i);
        if (std::abs(x - r.centre.X) <= r.halfW && std::abs(z - r.centre.Z) <= r.halfD) return i;
    }
    return -1;
}

}  // namespace FloorMap
