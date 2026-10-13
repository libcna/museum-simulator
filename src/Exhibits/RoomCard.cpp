#include "Exhibits/RoomCard.hpp"

#include <algorithm>

#include "Exhibits/ExhibitKit.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;

void AddRoomCard(RoomBuilder& b, Services& s, const RoomDef& room)
{
    if (room.doors.empty() || s.font == nullptr) return;
    const DoorDef& door = room.doors.front();
    constexpr float kWidth = 3.3f, kHeight = 1.9f;
    constexpr int kPixels = 220;
    const float textWidth = kWidth * static_cast<float>(kPixels) - 40.0f;

    SignSpec spec;
    spec.pixelWidth = static_cast<int>(kWidth * kPixels);
    spec.pixelHeight = static_cast<int>(kHeight * kPixels);
    spec.leftAlign = true;
    spec.textScale = 1.5f;
    spec.background = Color(16, 20, 32, 255);
    spec.border = Kit::Accent(room.accent);
    spec.lines.push_back({room.number + "  " + room.title, true, Kit::Accent(room.accent, 1.25f)});
    for (const std::string& l : Kit::Wrap(*s.font, room.subtitle, textWidth / 1.5f)) spec.lines.push_back({l, false, Kit::kText});
    if (!room.apis.empty()) {
        std::string apis = "APIs: ";
        for (std::size_t i = 0; i < room.apis.size(); ++i) apis += (i ? ", " : "") + room.apis[i];
        for (const std::string& l : Kit::Wrap(*s.font, apis, textWidth / 1.5f)) spec.lines.push_back({l, false, Kit::kDim});
    }
    const char* badge = room.origin == ApiOrigin::Xna ? "XNA 4.0 compatible feature"
                        : room.origin == ApiOrigin::CnaExtension ? "CNA Extension (not in XNA 4.0)" : "Application-level demonstration";
    spec.lines.push_back({badge, false, room.origin == ApiOrigin::Xna ? Kit::kXna : room.origin == ApiOrigin::CnaExtension ? Kit::kCnaExt : Kit::kApp});

    // Beside the door, on the door's wall: to the left of it as seen from inside the room.
    const float wallSpan = (door.wall == Wall::North || door.wall == Wall::South) ? room.halfW : room.halfD;
    float along = door.along - (door.width * 0.5f + 0.35f + kWidth * 0.5f);
    if (along - kWidth * 0.5f < -wallSpan + 0.3f) along = door.along + (door.width * 0.5f + 0.35f + kWidth * 0.5f);
    if (std::abs(along) + kWidth * 0.5f > wallSpan - 0.2f) return;   // a very small room: no space for a card
    b.AddWallSign(door.wall, along, 2.35f, kWidth, kHeight, spec);
}
