#include "UI/DirectoryScreen.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "UI/FloorMap.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using Microsoft::Xna::Framework::Input::Keys;

namespace {
constexpr int kRowHeight = 25;

Color Tint(const Vector3& v, float k = 1.0f)
{
    const auto b = [&](float x) { return static_cast<std::uint8_t>(std::min(1.0f, x * k) * 255.0f); };
    return Color(b(v.X), b(v.Y), b(v.Z), static_cast<std::uint8_t>(255));
}

std::string Lower(std::string s)
{
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
}  // namespace

void DirectoryScreen::Open(int currentFloor, int currentCell)
{
    open_ = true;
    floor_ = currentFloor;
    query_.clear();
    selected_ = currentCell >= 0 && s_.museum->Room(currentCell).kind == CellKind::Room ? currentCell : -1;
    scroll_ = 0;
    message_.clear();
    Rebuild();
}

bool DirectoryScreen::Unlocked(int cell, const std::set<int>& discovered) const
{
    const RoomDef& r = s_.museum->Room(cell);
    if (r.floor == 0) return true;
    return discovered.count(s_.museum->floors[static_cast<std::size_t>(r.floor)].hub) > 0 ||
           discovered.count(s_.museum->floors[static_cast<std::size_t>(r.floor)].elevator) > 0;
}

void DirectoryScreen::Rebuild()
{
    const MuseumPlan& m = *s_.museum;
    entries_.clear();
    const std::string q = Lower(query_);
    const auto matches = [&](const RoomDef& r) {
        if (q.empty()) return true;
        if (Lower(r.title).find(q) != std::string::npos || Lower(r.number).find(q) != std::string::npos ||
            Lower(r.key).find(q) != std::string::npos || Lower(r.subtitle).find(q) != std::string::npos)
            return true;
        for (const std::string& api : r.apis)
            if (Lower(api).find(q) != std::string::npos) return true;
        return false;
    };
    const auto addFloor = [&](int floor) {
        const FloorDef& fd = m.floors[static_cast<std::size_t>(floor)];
        if (q.empty()) entries_.push_back({fd.hub, -1});   // the hall first
        for (const int wi : fd.wings) {
            const WingDef& wing = m.wings[static_cast<std::size_t>(wi)];
            bool header = false;
            for (const int ri : wing.rooms) {
                if (!matches(m.Room(ri))) continue;
                if (!header) {
                    entries_.push_back({wing.gallery, wi});
                    header = true;
                }
                entries_.push_back({ri, -1});
            }
        }
    };
    if (q.empty()) addFloor(floor_);
    else for (const FloorDef& f : m.floors) addFloor(f.index);
    if (selected_ >= 0 && std::none_of(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.cell == selected_ && e.wingHeader < 0; }))
        selected_ = -1;
}

void DirectoryScreen::MoveSelection(int delta)
{
    std::vector<int> rooms;
    for (const Entry& e : entries_)
        if (e.wingHeader < 0) rooms.push_back(e.cell);
    if (rooms.empty()) return;
    auto it = std::find(rooms.begin(), rooms.end(), selected_);
    int index = it == rooms.end() ? (delta > 0 ? -1 : 0) : static_cast<int>(it - rooms.begin());
    index = std::clamp(index + delta, 0, static_cast<int>(rooms.size()) - 1);
    selected_ = rooms[static_cast<std::size_t>(index)];
    floor_ = s_.museum->Room(selected_).floor;
    // Keep the selection inside the visible rows.
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].cell == selected_ && entries_[i].wingHeader < 0) {
            scroll_ = std::clamp(scroll_, static_cast<int>(i) - 18, static_cast<int>(i));
            break;
        }
    }
}

DirectoryScreen::Result DirectoryScreen::Update(const InputState& in, const std::set<int>& discovered, int w, int h)
{
    Result result;
    const MuseumPlan& m = *s_.museum;
    if (in.Pressed(Keys::Escape) || (in.Pressed(Keys::M) && query_.empty()) || in.Pressed(Keys::Tab)) {
        result.close = true;
        return result;
    }
    // Search box: letters, digits, space and backspace.
    bool changed = false;
    for (int k = static_cast<int>(Keys::A); k <= static_cast<int>(Keys::Z); ++k) {
        if (in.Pressed(static_cast<Keys>(k)) && query_.size() < 24) {   // a leading M closed the screen above
            query_.push_back(static_cast<char>('a' + (k - static_cast<int>(Keys::A))));
            changed = true;
        }
    }
    for (int k = static_cast<int>(Keys::D0); k <= static_cast<int>(Keys::D9); ++k) {
        if (in.Pressed(static_cast<Keys>(k)) && query_.size() < 24) {
            query_.push_back(static_cast<char>('0' + (k - static_cast<int>(Keys::D0))));
            changed = true;
        }
    }
    if (in.Pressed(Keys::OemPeriod) && query_.size() < 24) { query_.push_back('.'); changed = true; }
    if (in.Pressed(Keys::Space) && !query_.empty() && query_.size() < 24) { query_.push_back(' '); changed = true; }
    if (in.Pressed(Keys::Back) && !query_.empty()) { query_.pop_back(); changed = true; }
    if (changed) {
        scroll_ = 0;
        Rebuild();
    }

    if (in.Pressed(Keys::Down)) MoveSelection(+1);
    if (in.Pressed(Keys::Up)) MoveSelection(-1);
    if (in.Pressed(Keys::Right) && floor_ + 1 < static_cast<int>(m.floors.size()) && query_.empty()) { ++floor_; selected_ = -1; scroll_ = 0; Rebuild(); }
    if (in.Pressed(Keys::Left) && floor_ > 0 && query_.empty()) { --floor_; selected_ = -1; scroll_ = 0; Rebuild(); }
    scroll_ = std::max(0, scroll_ - static_cast<int>(in.WheelDelta()));

    bool go = in.Pressed(Keys::Enter);
    // Mouse: tabs, list rows, map cells.
    const int mx = in.Mouse().getXProperty();
    const int my = in.Mouse().getYProperty();
    const int px = 40, py = 30, pw = w - 80, ph = h - 60;
    if (in.LeftClicked()) {
        // Floor tabs.
        const int tabW = (pw - 20) / static_cast<int>(m.floors.size());
        if (my >= py + 52 && my < py + 86 && mx >= px + 10) {
            const int f = (mx - px - 10) / tabW;
            if (f >= 0 && f < static_cast<int>(m.floors.size())) { floor_ = f; query_.clear(); selected_ = -1; scroll_ = 0; Rebuild(); }
        }
        // List rows.
        const int listX = px + 14, listW = pw * 2 / 5 - 20, listY = py + 130;
        if (mx >= listX && mx < listX + listW && my >= listY) {
            const std::size_t row = static_cast<std::size_t>((my - listY) / kRowHeight + scroll_);
            if (row < entries_.size() && entries_[row].wingHeader < 0) {
                go = go || (entries_[row].cell == selected_);
                selected_ = entries_[row].cell;
            }
        }
        // The map.
        const Rectangle mapArea(px + pw * 2 / 5 + 10, py + 130, pw * 3 / 5 - 24, ph - 130 - 150);
        if (mapArea.Contains(mx, my)) {
            MapView view;
            view.floor = floor_;
            const int cell = FloorMap::CellAtPixel(m, view, mapArea, mx, my);
            if (cell >= 0) {
                go = go || (cell == selected_);
                selected_ = cell;
            }
        }
    }
    if (go && selected_ >= 0) {
        if (Unlocked(selected_, discovered)) {
            result.travelTo = selected_;
            result.close = true;
        } else {
            message_ = "Visit floor " + std::to_string(m.Room(selected_).floor) + " by elevator first to unlock quick travel there.";
        }
    }
    return result;
}

void DirectoryScreen::Draw(const std::set<int>& discovered, int currentFloor, int currentCell, const Vector3& player, float yaw, int w, int h)
{
    if (!open_ || s_.sprites == nullptr) return;
    const MuseumPlan& m = *s_.museum;
    SpriteBatch& sb = *s_.sprites;
    Texture2D& white = *s_.textures->White();
    const SamplerState sampler = SamplerState::LinearClamp;
    sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &sampler, nullptr, nullptr);
    const auto rect = [&](int x, int y, int rw, int rh, Color c) { sb.Draw(white, Rectangle(x, y, rw, rh), c); };
    const auto text = [&](const std::string& t, float x, float y, Color c, float scale = 0.7f) {
        sb.DrawString(*s_.font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
    };
    rect(0, 0, w, h, Color(0, 0, 0, 170));
    const int px = 40, py = 30, pw = w - 80, ph = h - 60;
    rect(px, py, pw, ph, Color(10, 14, 24, 235));
    rect(px, py, pw, 3, Color(200, 170, 90, 255));
    sb.DrawString(*s_.titleFont, "MUSEUM DIRECTORY", Vector2(static_cast<float>(px) + 16.0f, static_cast<float>(py) + 6.0f), Color(255, 226, 150, 255), 0.0f,
                  Vector2(0, 0), 0.6f, SpriteEffects::None, 0.0f);

    // Floor tabs.
    const int tabW = (pw - 20) / static_cast<int>(m.floors.size());
    for (const FloorDef& f : m.floors) {
        const int x = px + 10 + f.index * tabW;
        const bool cur = f.index == floor_ && query_.empty();
        rect(x + 2, py + 52, tabW - 4, 34, cur ? Tint(f.accent, 0.85f) : Color(34, 42, 62, 255));
        if (f.index == currentFloor) rect(x + 2, py + 82, tabW - 4, 4, Color(255, 214, 90, 255));
        text(std::to_string(f.index), static_cast<float>(x + tabW / 2 - 5), static_cast<float>(py + 58), cur ? Color(20, 20, 30, 255) : Color(220, 226, 240, 255));
    }

    // Search line and floor heading.
    const std::string heading = query_.empty() ? "Floor " + std::to_string(floor_) + "  " + m.floors[static_cast<std::size_t>(floor_)].name
                                               : "Search results for \"" + query_ + "\"";
    text(heading, static_cast<float>(px + 16), static_cast<float>(py + 92), Color(255, 214, 90, 255), 0.72f);
    text("Search: " + query_ + "_", static_cast<float>(px + 16), static_cast<float>(py + 112),
         Color(170, 190, 220, 255), 0.6f);

    // The list.
    const int listX = px + 14, listW = pw * 2 / 5 - 20, listY = py + 130;
    const int rowsVisible = (ph - 130 - 40) / kRowHeight;
    for (int i = 0; i < rowsVisible; ++i) {
        const std::size_t idx = static_cast<std::size_t>(i + scroll_);
        if (idx >= entries_.size()) break;
        const Entry& e = entries_[idx];
        const RoomDef& r = m.Room(e.cell);
        const int y = listY + i * kRowHeight;
        if (e.wingHeader >= 0) {
            const WingDef& wing = m.wings[static_cast<std::size_t>(e.wingHeader)];
            rect(listX, y + 2, listW, kRowHeight - 4, Tint(wing.accent, 0.35f));
            text("Wing " + std::to_string(wing.number) + "  " + wing.name, static_cast<float>(listX + 6), static_cast<float>(y + 3), Tint(wing.accent, 1.3f), 0.62f);
            continue;
        }
        if (e.cell == selected_) rect(listX, y, listW, kRowHeight - 2, Color(60, 84, 140, 255));
        const bool seen = discovered.count(e.cell) > 0;
        std::string label = (seen ? "* " : "  ") + r.number + "  " + r.title;
        while (label.size() > 8 && s_.font->MeasureString(label).X * 0.62f > static_cast<float>(listW - 12)) label.pop_back();
        text(label, static_cast<float>(listX + 6), static_cast<float>(y + 3), Unlocked(e.cell, discovered) ? Color(235, 238, 248, 255) : Color(130, 136, 150, 255), 0.62f);
    }
    if (entries_.empty()) text("No rooms match.", static_cast<float>(listX + 6), static_cast<float>(listY + 4), Color(200, 160, 160, 255), 0.7f);

    // The map and the selected room's details.
    const Rectangle mapArea(px + pw * 2 / 5 + 10, py + 130, pw * 3 / 5 - 24, ph - 130 - 150);
    rect(mapArea.X - 2, mapArea.Y - 2, mapArea.Width + 4, mapArea.Height + 4, Color(30, 36, 54, 255));
    rect(mapArea.X, mapArea.Y, mapArea.Width, mapArea.Height, Color(8, 10, 18, 255));
    MapView view;
    view.floor = floor_;
    view.currentCell = currentFloor == floor_ ? currentCell : -1;
    view.showPlayer = currentFloor == floor_;
    view.player = player;
    view.yaw = yaw;
    view.discovered = &discovered;
    view.selectedCell = selected_;
    FloorMap::Draw(s_, m, view, mapArea);

    const int dy = mapArea.Y + mapArea.Height + 10;
    if (selected_ >= 0) {
        const RoomDef& r = m.Room(selected_);
        text(r.number + "  " + r.title, static_cast<float>(mapArea.X), static_cast<float>(dy), Color(255, 226, 150, 255), 0.74f);
        std::string sub = r.subtitle;
        while (sub.size() > 8 && s_.font->MeasureString(sub).X * 0.6f > static_cast<float>(mapArea.Width)) sub.pop_back();
        text(sub, static_cast<float>(mapArea.X), static_cast<float>(dy + 26), Color(215, 222, 238, 255), 0.6f);
        std::string apis;
        for (const std::string& a : r.apis) apis += (apis.empty() ? "" : ", ") + a;
        while (apis.size() > 8 && s_.font->MeasureString(apis).X * 0.55f > static_cast<float>(mapArea.Width)) apis.pop_back();
        const char* origin = r.origin == ApiOrigin::Xna ? "XNA 4.0 compatible" : (r.origin == ApiOrigin::CnaExtension ? "CNA extension" : "application helper");
        text(std::string(origin) + (apis.empty() ? "" : "  |  ") + apis, static_cast<float>(mapArea.X), static_cast<float>(dy + 48),
             r.origin == ApiOrigin::CnaExtension ? Color(255, 170, 100, 255) : Color(130, 205, 255, 255), 0.55f);
        text(Unlocked(selected_, discovered) ? "Enter or a second click: go there" : "Locked: reach this floor by elevator first", static_cast<float>(mapArea.X),
             static_cast<float>(dy + 72), Color(150, 255, 170, 255), 0.6f);
    }
    if (!message_.empty()) text(message_, static_cast<float>(px + 16), static_cast<float>(py + ph - 56), Color(255, 150, 120, 255), 0.6f);
    int seenCount = 0, roomCount = 0;
    for (const RoomDef& r : m.rooms)
        if (r.kind == CellKind::Room) {
            ++roomCount;
            if (discovered.count(r.index)) ++seenCount;
        }
    char foot[200];
    std::snprintf(foot, sizeof foot, "Up/Down select   Left/Right floor   type to search   Enter go   Esc/M close      visited %d of %d rooms (* = visited)",
                  seenCount, roomCount);
    text(foot, static_cast<float>(px + 16), static_cast<float>(py + ph - 26), Color(150, 160, 185, 255), 0.55f);
    sb.End();
}
