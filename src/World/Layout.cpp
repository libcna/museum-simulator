// Turns the declarative FloorSpecs into positioned cells with matching doors.
//
// Every floor has the same shape, so a visitor learns it once:
//
//     elevator - HUB ---- SPINE (a long corridor running north)
//                            |  \_ wing galleries branch east and west off the spine;
//                            |     rooms open off both sides of every gallery
//
// Cells never overlap and are separated by exactly one double wall (kCellGap), so a door is
// simply a matching opening in both walls.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Catalogue/CatalogueBuilder.hpp"
#include "World/Catalogue.hpp"

using namespace Microsoft::Xna::Framework;

namespace {

constexpr float kSpineHalfWidth = 3.0f;
constexpr float kSpineHeight = 5.0f;
constexpr float kGalleryHeight = 4.6f;
constexpr float kMainDoorWidth = 4.0f;
constexpr float kElevatorHalf = 2.0f;
constexpr float kSpineEndMargin = 2.5f;
constexpr float kWingGap = 1.2f;   // extra clearance between stacked wings

std::string Format(const char* fmt, int a, int b = 0, int c = 0)
{
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, fmt, a, b, c);
    return buffer;
}

void Join(MuseumPlan& m, int a, Wall wallOfA, float alongA, int b, float alongB, float width)
{
    m.rooms[static_cast<std::size_t>(a)].doors.push_back({wallOfA, alongA, width, b});
    m.rooms[static_cast<std::size_t>(b)].doors.push_back({Opposite(wallOfA), alongB, width, a});
}

int AddCell(MuseumPlan& m, RoomDef cell)
{
    cell.index = static_cast<int>(m.rooms.size());
    m.rooms.push_back(std::move(cell));
    return m.rooms.back().index;
}

struct PlacedRoom {
    const RoomSpec* spec;
    bool north;
    float x0;   // interior west edge, relative to the gallery's interior start
};

}  // namespace

MuseumPlan LayoutMuseum(const std::vector<FloorSpec>& specs)
{
    MuseumPlan m;
    for (const FloorSpec& fs : specs) {
        FloorDef floor;
        floor.index = fs.index;
        floor.name = fs.name;
        floor.tagline = fs.tagline;
        floor.theme = fs.theme;
        floor.accent = fs.accent;
        floor.baseY = static_cast<float>(fs.index) * kFloorPitch;
        const float base = floor.baseY;
        const Theme& floorTheme = FindTheme(fs.theme);
        const int floorNo = fs.index;

        // --- hub ------------------------------------------------------------------------
        RoomDef hub;
        hub.key = Format("hub-%d", floorNo);
        hub.number = Format("%d.0", floorNo);
        hub.title = "Floor " + std::to_string(floorNo) + " Hall";
        hub.subtitle = fs.name;
        hub.kind = CellKind::Hub;
        hub.floor = floorNo;
        hub.exhibit = fs.hubExhibit;
        hub.params = fs.hubParams;
        hub.centre = Vector3(0, base, 0);
        hub.halfW = fs.hubHalfW;
        hub.halfD = fs.hubHalfD;
        hub.height = fs.hubHeight;
        hub.theme = floorTheme;
        hub.accent = fs.accent;
        floor.hub = AddCell(m, hub);

        // --- elevator car, east of the hub --------------------------------------------------
        RoomDef lift;
        lift.key = Format("elevator-%d", floorNo);
        lift.number = Format("%d.E", floorNo);
        lift.title = "Elevator";
        lift.subtitle = "Choose a floor";
        lift.kind = CellKind::Elevator;
        lift.floor = floorNo;
        lift.exhibit = "elevator";
        lift.centre = Vector3(fs.hubHalfW + kCellGap + kElevatorHalf, base, 0);
        lift.halfW = kElevatorHalf;
        lift.halfD = kElevatorHalf;
        lift.height = 3.8f;
        lift.theme = FindTheme("elevator");
        lift.accent = fs.accent;
        floor.elevator = AddCell(m, lift);
        Join(m, floor.hub, Wall::East, 0.0f, floor.elevator, 0.0f, kDoorWidth);

        // --- wings: galleries and rooms ------------------------------------------------------
        // East and west wings stack independently along the spine, walking north from the hub.
        const float spineSouth = -fs.hubHalfD - kCellGap;
        float cursor[2] = {spineSouth, spineSouth};   // [0] east, [1] west: z of the southernmost free position
        struct WingPlacement { int wing; float zc; float galleryHalfLen; };
        std::vector<WingPlacement> placements;

        int wingNumber = 0;
        for (const WingSpec& ws : fs.wings) {
            ++wingNumber;
            const bool east = ws.spineSide != Wall::West;
            float& cur = cursor[east ? 0 : 1];
            const float gz = ws.galleryHalfWidth;

            // Rooms alternate between the north and south side of the gallery unless a spec says otherwise.
            std::vector<PlacedRoom> placed;
            float rowLen[2] = {0.0f, 0.0f};   // [0] north row, [1] south row
            float maxDepth[2] = {0.0f, 0.0f};
            int autoCounter = 0;
            for (const RoomSpec& rs : ws.rooms) {
                bool north = true;
                if (rs.side == PlaceSide::North) north = true;
                else if (rs.side == PlaceSide::South) north = false;
                else north = (autoCounter++ % 2) == 0;
                const int row = north ? 0 : 1;
                placed.push_back({&rs, north, rowLen[row]});
                rowLen[row] += rs.width + (rowLen[row] > 0.0f ? 0.0f : 0.0f) + kCellGap;
                maxDepth[row] = std::max(maxDepth[row], rs.depth);
            }
            // Row lengths counted one gap too many; the gallery spans the longer row.
            const float galleryLen = std::max({std::max(rowLen[0], rowLen[1]) - kCellGap, 12.0f});
            const float zc = cur - (gz + kCellGap + maxDepth[1]) ;
            cur = zc - (gz + kCellGap + maxDepth[0]) - kWingGap;
            placements.push_back({static_cast<int>(m.wings.size()), zc, galleryLen * 0.5f});

            WingDef wing;
            wing.index = static_cast<int>(m.wings.size());
            wing.floor = floorNo;
            wing.number = wingNumber;
            wing.name = ws.name;
            wing.tagline = ws.tagline;
            wing.theme = ws.theme;
            wing.accent = ws.accent;
            wing.spineSide = east ? Wall::East : Wall::West;

            const Theme& wingTheme = FindTheme(ws.theme);
            // Gallery interior spans [xa, xa + galleryLen] east of the spine, mirrored for west wings.
            const float xa = kSpineHalfWidth + kCellGap;
            const float sign = east ? 1.0f : -1.0f;
            const float galleryCentreX = sign * (xa + galleryLen * 0.5f);

            RoomDef gallery;
            gallery.key = Format("gallery-%d-%d", floorNo, wingNumber);
            gallery.number = Format("%d.%d", floorNo, wingNumber);
            gallery.title = ws.name;
            gallery.subtitle = ws.tagline;
            gallery.kind = CellKind::Gallery;
            gallery.floor = floorNo;
            gallery.wing = wing.index;
            gallery.exhibit = "gallery";
            gallery.centre = Vector3(galleryCentreX, base, zc);
            gallery.halfW = galleryLen * 0.5f;
            gallery.halfD = gz;
            gallery.height = kGalleryHeight;
            gallery.theme = wingTheme;
            gallery.accent = ws.accent;
            wing.gallery = AddCell(m, gallery);

            int ordinal = 0;
            for (const PlacedRoom& pr : placed) {
                const RoomSpec& rs = *pr.spec;
                ++ordinal;
                RoomDef room;
                room.key = rs.key;
                room.number = Format("%d.%d.%02d", floorNo, wingNumber, ordinal);
                room.title = rs.title;
                room.subtitle = rs.subtitle;
                room.kind = CellKind::Room;
                room.floor = floorNo;
                room.wing = wing.index;
                room.exhibit = rs.exhibit;
                room.params = rs.params;
                room.apis = rs.apis;
                room.origin = rs.origin;
                room.halfW = rs.width * 0.5f;
                room.halfD = rs.depth * 0.5f;
                room.height = rs.height;
                room.openRoof = rs.openRoof;
                room.theme = rs.theme.empty() ? wingTheme : FindTheme(rs.theme);
                room.accent = ws.accent;
                // Row position: interior west edge for east wings; mirrored for west wings.
                const float edge = east ? (xa + pr.x0) : -(xa + pr.x0 + rs.width) ;
                const float cx = edge + rs.width * 0.5f;
                const float cz = pr.north ? zc - gz - kCellGap - rs.depth * 0.5f : zc + gz + kCellGap + rs.depth * 0.5f;
                room.centre = Vector3(cx, base, cz);
                const int idx = AddCell(m, room);
                wing.rooms.push_back(idx);
                // Gallery wall facing the room, and the room's own wall facing the gallery.
                Join(m, wing.gallery, pr.north ? Wall::North : Wall::South, cx - galleryCentreX, idx, 0.0f, kDoorWidth);
            }
            floor.wings.push_back(wing.index);
            m.wings.push_back(wing);
        }

        // --- spine ----------------------------------------------------------------------------
        const float northEnd = std::min(cursor[0], cursor[1]) + kWingGap - kSpineEndMargin;
        const float spineHalfLen = (spineSouth - northEnd) * 0.5f;
        RoomDef spine;
        spine.key = Format("spine-%d", floorNo);
        spine.number = Format("%d.S", floorNo);
        spine.title = "Floor " + std::to_string(floorNo) + " Concourse";
        spine.subtitle = fs.name;
        spine.kind = CellKind::Spine;
        spine.floor = floorNo;
        spine.exhibit = "spine";
        spine.centre = Vector3(0, base, (spineSouth + northEnd) * 0.5f);
        spine.halfW = kSpineHalfWidth;
        spine.halfD = std::max(spineHalfLen, 4.0f);
        spine.height = kSpineHeight;
        spine.theme = floorTheme;
        spine.accent = fs.accent;
        floor.spine = AddCell(m, spine);
        Join(m, floor.hub, Wall::North, 0.0f, floor.spine, 0.0f, kMainDoorWidth);
        for (const WingPlacement& wp : placements) {
            const WingDef& wing = m.wings[static_cast<std::size_t>(wp.wing)];
            const bool east = wing.spineSide == Wall::East;
            Join(m, floor.spine, east ? Wall::East : Wall::West, wp.zc - spine.centre.Z, wing.gallery, 0.0f, kMainDoorWidth);
        }
        // A gallery's short end wall faces the spine: the door there is on the gallery's west (east wings) or east wall.
        // Join() has already given the gallery the opposite wall of the spine's, which is exactly that.

        m.floors.push_back(floor);
    }
    m.IndexCells();
    return m;
}
