#include "World/Catalogue.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>
#include <set>

#include "Catalogue/CatalogueBuilder.hpp"

using namespace Microsoft::Xna::Framework;

std::string Params::Get(const std::string& key, const std::string& fallback) const
{
    for (const auto& [k, v] : items)
        if (k == key) return v;
    return fallback;
}

bool Params::Has(const std::string& key) const
{
    for (const auto& kv : items)
        if (kv.first == key) return true;
    return false;
}

float Params::GetFloat(const std::string& key, float fallback) const
{
    const std::string v = Get(key);
    return v.empty() ? fallback : static_cast<float>(std::atof(v.c_str()));
}

int Params::GetInt(const std::string& key, int fallback) const
{
    const std::string v = Get(key);
    return v.empty() ? fallback : std::atoi(v.c_str());
}

BoundingBox RoomDef::Interior() const
{
    return BoundingBox(Vector3(centre.X - halfW, centre.Y, centre.Z - halfD),
                       Vector3(centre.X + halfW, centre.Y + height, centre.Z + halfD));
}

bool RoomDef::Contains(const Vector3& p, float margin) const
{
    const float top = kind == CellKind::Stairs ? centre.Y + kFloorPitch + kDoorHeight : centre.Y + kFloorPitch;
    return std::abs(p.X - centre.X) <= halfW + margin && std::abs(p.Z - centre.Z) <= halfD + margin &&
           p.Y >= centre.Y - 0.5f && p.Y <= top;
}

BoundingBox RoomDef::DoorBounds(const DoorDef& door) const
{
    const float hw = door.width * 0.5f;
    Vector3 lo, hi;
    switch (door.wall) {
        case Wall::North:
            lo = Vector3(centre.X + door.along - hw, centre.Y + door.floorOffset, centre.Z - halfD - kWallThickness);
            hi = Vector3(centre.X + door.along + hw, centre.Y + door.floorOffset + kDoorHeight, centre.Z - halfD);
            break;
        case Wall::South:
            lo = Vector3(centre.X + door.along - hw, centre.Y + door.floorOffset, centre.Z + halfD);
            hi = Vector3(centre.X + door.along + hw, centre.Y + door.floorOffset + kDoorHeight, centre.Z + halfD + kWallThickness);
            break;
        case Wall::West:
            lo = Vector3(centre.X - halfW - kWallThickness, centre.Y + door.floorOffset, centre.Z + door.along - hw);
            hi = Vector3(centre.X - halfW, centre.Y + door.floorOffset + kDoorHeight, centre.Z + door.along + hw);
            break;
        default:
            lo = Vector3(centre.X + halfW, centre.Y + door.floorOffset, centre.Z + door.along - hw);
            hi = Vector3(centre.X + halfW + kWallThickness, centre.Y + door.floorOffset + kDoorHeight, centre.Z + door.along + hw);
            break;
    }
    return BoundingBox(lo, hi);
}

const RoomDef* MuseumPlan::FindByKey(const std::string& key) const
{
    const auto it = byKey_.find(key);
    return it == byKey_.end() ? nullptr : &rooms[static_cast<std::size_t>(it->second)];
}

void MuseumPlan::IndexCells()
{
    byFloor_.assign(floors.size(), {});
    byKey_.clear();
    for (const RoomDef& r : rooms) {
        byFloor_[static_cast<std::size_t>(r.floor)].push_back(r.index);
        if (r.kind == CellKind::Stairs && r.floor + 1 < static_cast<int>(byFloor_.size()))
            byFloor_[static_cast<std::size_t>(r.floor + 1)].push_back(r.index);
        byKey_[r.key] = r.index;
    }
}

int MuseumPlan::FloorAt(float worldY) const
{
    const int f = static_cast<int>(std::floor((worldY + 0.5f) / kFloorPitch));
    return std::clamp(f, 0, static_cast<int>(floors.size()) - 1);
}

float MuseumPlan::GroundHeightAt(const Vector3& feet) const
{
    for (const RoomDef& r : rooms) {
        if (r.kind != CellKind::Stairs) continue;
        if (std::abs(feet.X - r.centre.X) > r.halfW + 0.05f ||
            std::abs(feet.Z - r.centre.Z) > r.halfD + 0.05f ||
            feet.Y < r.centre.Y - 0.6f || feet.Y > r.centre.Y + kFloorPitch + 0.6f)
            continue;

        const float x = feet.X - r.centre.X;
        const float z = feet.Z - r.centre.Z;
        const float west = -r.halfW + kStairLandingDepth;
        const float east = r.halfW - kStairLandingDepth;
        const float run = east - west;
        const float halfRise = kFloorPitch * 0.5f;
        const float laneHalf = kStairLaneWidth * 0.5f;

        if (x <= west && std::abs(z) <= kStairLaneOffset + laneHalf)
            return r.centre.Y + halfRise;
        if (std::abs(z - kStairLaneOffset) <= laneHalf) {
            if (x >= east) return r.centre.Y;
            const float t = std::clamp((east - x) / run, 0.0f, 1.0f);
            return r.centre.Y + halfRise * t;
        }
        if (std::abs(z + kStairLaneOffset) <= laneHalf) {
            if (x >= east) return r.centre.Y + kFloorPitch;
            const float t = std::clamp((x - west) / run, 0.0f, 1.0f);
            return r.centre.Y + halfRise + halfRise * t;
        }
    }
    return floors[static_cast<std::size_t>(FloorAt(feet.Y))].baseY;
}

int MuseumPlan::CellAt(const Vector3& p, int hint) const
{
    if (hint >= 0 && Room(hint).Contains(p)) return hint;
    const int floor = FloorAt(p.Y);
    for (int i : CellsOnFloor(floor))
        if (Room(i).Contains(p)) return i;
    // Inside a doorway (the wall thickness between two cells): stay in the cell we came from.
    if (hint >= 0 && Room(hint).Contains(p, kWallThickness + 0.05f)) return hint;
    return -1;
}

Spawn MuseumPlan::SpawnPoint(int cell) const
{
    const RoomDef& r = Room(cell);
    Spawn s;
    s.eye = r.ToWorld(0.0f, 1.65f, 0.0f);
    if (r.doors.empty()) return s;
    const DoorDef& d = r.doors.front();
    // Quick travel lands exhibit visitors far enough inside that a far-wall screen is legible;
    // circulation cells keep the shorter inset so arrivals remain next to their exit door.
    const float inset = r.kind == CellKind::Room ? kRoomArrivalInset : kCirculationArrivalInset;
    float x = 0.0f, z = 0.0f, dx = 0.0f, dz = 0.0f;
    switch (d.wall) {
        case Wall::North: x = d.along; z = -r.halfD + inset; dz = 1.0f; break;
        case Wall::South: x = d.along; z = r.halfD - inset; dz = -1.0f; break;
        case Wall::West: x = -r.halfW + inset; z = d.along; dx = 1.0f; break;
        default: x = r.halfW - inset; z = d.along; dx = -1.0f; break;
    }
    // Keep the arrival point inside small rooms.
    x = std::clamp(x, -r.halfW + 0.8f, r.halfW - 0.8f);
    z = std::clamp(z, -r.halfD + 0.8f, r.halfD - 0.8f);
    s.eye = r.WorldAt(x, 1.65f, z);   // the door is described in world walls, not in the room's local frame
    s.yaw = std::atan2(-dx, -dz);
    if (r.kind == CellKind::Elevator) s.yaw += 3.14159265f;   // arrive facing the doors, ready to step out
    return s;
}

const MuseumPlan& TheMuseum()
{
    static const MuseumPlan museum = LayoutMuseum(AllFloorSpecs());
    return museum;
}

std::vector<std::string> ValidateMuseum(const MuseumPlan& m)
{
    std::vector<std::string> errors;
    const auto err = [&](const std::string& message) { errors.push_back(message); };

    std::set<std::string> keys;
    for (const RoomDef& r : m.rooms) {
        if (r.key.empty()) err("cell " + std::to_string(r.index) + " has no key");
        else if (!keys.insert(r.key).second) err("duplicate room key '" + r.key + "'");
        if (r.halfW < 1.5f || r.halfD < 1.5f) err(r.key + ": room is too small");
        if (r.height < 3.0f || (r.kind != CellKind::Stairs && r.height > kFloorPitch - 0.8f) ||
            (r.kind == CellKind::Stairs && r.height < kFloorPitch + kDoorHeight))
            err(r.key + ": height " + std::to_string(r.height) + " does not fit its cell");
        if (r.floor < 0 || r.floor >= static_cast<int>(m.floors.size())) err(r.key + ": bad floor");
        if (r.doors.empty() && r.kind != CellKind::Elevator) err(r.key + ": no doors, unreachable");
        for (const DoorDef& d : r.doors) {
            if (d.to < 0 || d.to >= static_cast<int>(m.rooms.size())) {
                err(r.key + ": door leads to missing cell " + std::to_string(d.to));
                continue;
            }
            const RoomDef& other = m.Room(d.to);
            const float span = (d.wall == Wall::North || d.wall == Wall::South) ? r.halfW : r.halfD;
            if (std::abs(d.along) + d.width * 0.5f > span - 0.2f) err(r.key + ": door does not fit in its wall");
            bool back = false;
            for (const DoorDef& e : other.doors) {
                if (e.to != r.index || e.wall != Opposite(d.wall)) continue;
                const BoundingBox a = r.DoorBounds(d), b = other.DoorBounds(e);
                back = std::abs(a.Min.Y - b.Min.Y) < 0.01f && std::abs(a.Max.Y - b.Max.Y) < 0.01f;
            }
            if (!back) err(r.key + " -> " + other.key + ": door has no matching door on the other side");
            if (other.floor != r.floor && r.kind != CellKind::Stairs && other.kind != CellKind::Stairs)
                err(r.key + " -> " + other.key + ": only stairs may join floors");
        }
    }

    // Cells on one floor must not overlap (walls included: they need at least the double-wall gap).
    for (std::size_t f = 0; f < m.floors.size(); ++f) {
        const std::vector<int>& cells = m.CellsOnFloor(static_cast<int>(f));
        for (std::size_t i = 0; i < cells.size(); ++i) {
            for (std::size_t j = i + 1; j < cells.size(); ++j) {
                const RoomDef& a = m.Room(cells[i]);
                const RoomDef& b = m.Room(cells[j]);
                const float dx = std::abs(a.centre.X - b.centre.X) - (a.halfW + b.halfW);
                const float dz = std::abs(a.centre.Z - b.centre.Z) - (a.halfD + b.halfD);
                if (std::max(dx, dz) < kCellGap - 0.01f)
                    err(a.key + " overlaps " + b.key + " (clearance " + std::to_string(std::max(dx, dz)) + " m)");
            }
        }
        if (m.floors[f].hub < 0) err("floor " + std::to_string(f) + " has no hub");
        if (m.floors[f].elevator < 0 && m.floors.size() > 1) err("floor " + std::to_string(f) + " has no elevator");
    }

    // Everything must be reachable on foot from the entrance. Elevators remain a convenience,
    // but the stair graph is a second, genuinely walkable route through all storeys.
    if (!m.floors.empty() && m.floors.front().hub >= 0) {
        std::vector<char> seen(m.rooms.size(), 0);
        std::queue<int> open;
        seen[static_cast<std::size_t>(m.floors.front().hub)] = 1;
        open.push(m.floors.front().hub);
        while (!open.empty()) {
            const int c = open.front();
            open.pop();
            for (const DoorDef& d : m.Room(c).doors) {
                if (d.to >= 0 && !seen[static_cast<std::size_t>(d.to)]) {
                    seen[static_cast<std::size_t>(d.to)] = 1;
                    open.push(d.to);
                }
            }
        }
        for (const RoomDef& r : m.rooms)
            if (!seen[static_cast<std::size_t>(r.index)]) err(r.key + ": not reachable on foot from the entrance");
    }
    return errors;
}
