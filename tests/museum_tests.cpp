// Structural tests for the museum's data: no graphics device, no window.
// Run with `ctest --test-dir build` or build/bin/museum_tests.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <queue>
#include <set>
#include <string>

#include "Catalogue/CatalogueBuilder.hpp"
#include "Catalogue/Inventory.hpp"
#include "Catalogue/ReferenceViews.hpp"
#include "Exhibits/ExhibitRegistry.hpp"
#include "Exhibits/Shaders/ShaderSources.hpp"
#include "World/Catalogue.hpp"
#include "World/CollisionWorld.hpp"

using Microsoft::Xna::Framework::BoundingBox;
using Microsoft::Xna::Framework::Vector3;

namespace {

int g_failures = 0;
int g_checks = 0;

void Check(bool ok, const std::string& what)
{
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    }
}

void TestCatalogueIsValid(const MuseumPlan& m)
{
    const std::vector<std::string> errors = ValidateMuseum(m);
    for (const std::string& e : errors) std::fprintf(stderr, "  catalogue: %s\n", e.c_str());
    Check(errors.empty(), "ValidateMuseum reports no structural problems");
}

void TestFloorStructure(const MuseumPlan& m)
{
    Check(!m.floors.empty(), "the museum has floors");
    for (const FloorDef& f : m.floors) {
        Check(f.hub >= 0 && m.Room(f.hub).kind == CellKind::Hub, "floor " + std::to_string(f.index) + " has a hub");
        Check(f.spine >= 0 && m.Room(f.spine).kind == CellKind::Spine, "floor " + std::to_string(f.index) + " has a concourse");
        Check(f.elevator >= 0 && m.Room(f.elevator).kind == CellKind::Elevator, "floor " + std::to_string(f.index) + " has an elevator");
        Check((f.index == 0 && f.stairsDown < 0) ||
                  (f.index > 0 && f.stairsDown >= 0 && m.Room(f.stairsDown).kind == CellKind::Stairs),
              "floor " + std::to_string(f.index) + " has the expected stairs down");
        Check((f.index + 1 == static_cast<int>(m.floors.size()) && f.stairsUp < 0) ||
                  (f.index + 1 < static_cast<int>(m.floors.size()) && f.stairsUp >= 0 && m.Room(f.stairsUp).kind == CellKind::Stairs),
              "floor " + std::to_string(f.index) + " has the expected stairs up");
        Check(std::fabs(f.baseY - static_cast<float>(f.index) * kFloorPitch) < 1e-3f, "floor " + std::to_string(f.index) + " height");
        // The cars line up vertically so an elevator ride is seamless.
        Check(std::fabs(m.Room(f.elevator).centre.X - m.Room(m.floors.front().elevator).centre.X) < 0.01f ||
                  m.Room(f.hub).halfW != m.Room(m.floors.front().hub).halfW,
              "elevator cars share a shaft when hubs are the same size");
    }
}

void TestKeysAndNumbers(const MuseumPlan& m)
{
    std::set<std::string> keys, numbers;
    for (const RoomDef& r : m.rooms) {
        Check(keys.insert(r.key).second, "unique key " + r.key);
        if (r.kind == CellKind::Room) Check(numbers.insert(r.number).second, "unique room number " + r.number);
        Check(!r.title.empty(), "room " + r.key + " has a title");
        if (r.kind == CellKind::Room) Check(!r.subtitle.empty(), "room " + r.key + " explains itself (subtitle)");
        if (r.kind == CellKind::Room) Check(!r.apis.empty(), "room " + r.key + " names the CNA APIs it demonstrates");
        Check(m.FindByKey(r.key) == &r, "FindByKey round trip for " + r.key);
    }
}

void TestPointLookup(const MuseumPlan& m)
{
    for (const RoomDef& r : m.rooms) {
        const Spawn s = m.SpawnPoint(r.index);
        Check(m.CellAt(s.eye) == r.index, "the spawn point of " + r.key + " is inside it");
        Check(m.CellAt(r.ToWorld(0, 1.6f, 0)) == r.index || r.kind == CellKind::Room, "the centre of " + r.key + " resolves to it");
    }
    Check(m.CellAt({1000.0f, 1.0f, 1000.0f}) == -1, "a point outside the building belongs to no cell");
}

void TestDoorsAreSymmetric(const MuseumPlan& m)
{
    for (const RoomDef& r : m.rooms) {
        for (const DoorDef& d : r.doors) {
            const RoomDef& other = m.Room(d.to);
            const BoundingBox a = r.DoorBounds(d);
            bool matched = false;
            for (const DoorDef& e : other.doors) {
                if (e.to != r.index) continue;
                const BoundingBox b = other.DoorBounds(e);
                // The two openings must line up: the same slot in the double wall.
                const float dx = std::fabs((a.Min.X + a.Max.X) - (b.Min.X + b.Max.X)) * 0.5f;
                const float dz = std::fabs((a.Min.Z + a.Max.Z) - (b.Min.Z + b.Max.Z)) * 0.5f;
                const float dy = std::fabs(a.Min.Y - b.Min.Y);
                matched = matched || (dx < kCellGap + 0.01f && dz < kCellGap + 0.01f && dy < 0.01f);
            }
            Check(matched, "door " + r.key + " -> " + other.key + " lines up with its twin");
        }
    }
}

// Exhibits are written for "the door is on the south wall, screens hang on the north wall". A room entered through
// its north wall has its local frame turned half a circle so that this holds on both sides of a gallery.
void TestRoomFrames(const MuseumPlan& m)
{
    int turned = 0, straight = 0;
    for (const RoomDef& r : m.rooms) {
        if (r.kind != CellKind::Room) {
            Check(!r.Turned(), r.key + " is not an exhibit room: its frame is not turned");
            continue;
        }
        Check(!r.doors.empty(), r.key + " has a door");
        const Wall door = r.doors.front().wall;
        Check(door == Wall::North || door == Wall::South, r.key + " is entered through a north or south wall");
        Check(r.LocalWall(door) == Wall::South, r.key + ": its door is on the local south wall");
        Check(r.Turned() == (door == Wall::North), r.key + ": turned exactly when entered from the north");
        (r.Turned() ? turned : straight)++;
        // ToWorld, Rotate and WorldAt agree, and the arrival point looks at the local north wall.
        const Vector3 a = r.ToWorld(2.0f, 1.0f, -3.0f);
        const Vector3 b = r.WorldAt(0, 0, 0) + r.Rotate(Vector3(2.0f, 1.0f, -3.0f));
        Check((a - b).Length() < 1e-4f, r.key + ": ToWorld equals centre + Rotate");
        const Spawn s = m.SpawnPoint(r.index);
        const Vector3 ahead(-std::sin(s.yaw), 0.0f, -std::cos(s.yaw));
        const Vector3 far = r.ToWorld(0.0f, 0.0f, -r.halfD) - r.ToWorld(0.0f, 0.0f, 0.0f);
        Check(Vector3::Dot(ahead, far) > 0.0f, r.key + ": the arrival view faces the local north (far) wall");
    }
    Check(turned > 0 && straight > 0, "rooms exist on both sides of a gallery");
}

void TestExhibitsResolve(const MuseumPlan& m)
{
    for (const RoomDef& r : m.rooms) Check(IsKnownExhibit(r.exhibit), "exhibit '" + r.exhibit + "' of " + r.key + " is registered");
}

// Every fragment shader of the shader lab has a WGSL body (WEBGPU runs those), and every body names the entry point the renderer looks for.
void TestShaderLabHasEveryDialect(const MuseumPlan& m)
{
    int shaders = 0;
    for (const RoomDef& r : m.rooms) {
        if (r.exhibit != "shaders") continue;
        ++shaders;
        const char* body = ShaderSources::WgslBody(r.params.Get("shader"));
        Check(body != nullptr, "the shader room " + r.key + " has a WGSL body");
        if (body != nullptr) Check(std::string(body).find("fn fs_main") != std::string::npos, "the WGSL body of " + r.key + " defines fs_main");
    }
    Check(shaders >= 12, "the shader lab has its twelve rooms");
}

void TestAssetsExist(const MuseumPlan& m)
{
#ifdef MUSEUM_SOURCE_CONTENT_DIR
    const std::vector<std::string> missing = FindMissingAssets(m, MUSEUM_SOURCE_CONTENT_DIR);
    for (const std::string& a : missing) std::fprintf(stderr, "  missing asset: %s\n", a.c_str());
    Check(missing.empty(), "every asset the catalogue names exists");
#endif
}

void TestInventory(const MuseumPlan& m)
{
    const MuseumInventory inv = ComputeInventory(m);
    Check(inv.floors == static_cast<int>(m.floors.size()), "inventory floors");
    Check(inv.rooms > 0, "inventory counts rooms");
    int sum = 0;
    for (const auto& [floor, n] : inv.roomsPerFloor) sum += n;
    Check(sum == inv.rooms, "rooms per floor add up");
    Check(InventoryAsJson(m).find("\"rooms\"") != std::string::npos, "inventory JSON has rooms");
}

void TestEveryFloorIsConnected(const MuseumPlan& m)
{
    // The entrance now reaches the complete museum through ordinary doors and the stair flights.
    std::set<int> seen = {m.floors.front().hub};
    std::queue<int> open;
    open.push(m.floors.front().hub);
    while (!open.empty()) {
        const int c = open.front();
        open.pop();
        for (const DoorDef& d : m.Room(c).doors)
            if (d.to >= 0 && seen.insert(d.to).second) open.push(d.to);
    }
    Check(seen.size() == m.rooms.size(), "every cell is reachable on foot from the entrance");

    for (std::size_t floor = 0; floor + 1 < m.floors.size(); ++floor) {
        const RoomDef& stairs = m.Room(m.floors[floor].stairsUp);
        Check(stairs.floor == static_cast<int>(floor), stairs.key + " begins on its lower floor");
        Check(m.floors[floor + 1].stairsDown == stairs.index, stairs.key + " ends on the next floor");
        const float east = stairs.centre.X + stairs.halfW - kStairLandingDepth * 0.5f;
        const Vector3 lower(east, stairs.centre.Y, stairs.centre.Z + kStairLaneOffset);
        const Vector3 upper(east, stairs.centre.Y + kFloorPitch, stairs.centre.Z - kStairLaneOffset);
        const Vector3 middle(stairs.centre.X - stairs.halfW + kStairLandingDepth * 0.5f,
                             stairs.centre.Y + kFloorPitch * 0.5f, stairs.centre.Z);
        Check(std::fabs(m.GroundHeightAt(lower) - stairs.centre.Y) < 0.01f, stairs.key + " lower landing height");
        Check(std::fabs(m.GroundHeightAt(middle) - (stairs.centre.Y + kFloorPitch * 0.5f)) < 0.01f,
              stairs.key + " half landing height");
        Check(std::fabs(m.GroundHeightAt(upper) - (stairs.centre.Y + kFloorPitch)) < 0.01f,
              stairs.key + " upper landing height");
    }
}

void TestCollisionUsesFloorHeight()
{
    CollisionWorld collision;
    collision.AddBox(BoundingBox(Vector3(-1.0f, 9.0f, -1.0f), Vector3(1.0f, 11.0f, 1.0f)));
    Check(!collision.OverlapsCircle(0.0f, 0.0f, 0.3f, 0.0f), "an upper-floor wall does not block the ground floor");
    Check(collision.OverlapsCircle(0.0f, 0.0f, 0.3f, 9.0f), "an upper-floor wall blocks a visitor on that floor");
    const Vector3 stopped = collision.Move(Vector3(-1.5f, 9.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), 0.3f);
    Check(stopped.X < -1.4f, "upper-floor movement cannot pass through its wall");
}

void TestReferenceViews(const MuseumPlan& m)
{
    std::set<std::string> names;
    for (const ReferenceView& v : ReferenceViews()) {
        Check(names.insert(v.name).second, std::string("reference view name is unique: ") + v.name);
        const RoomDef* r = m.FindByKey(v.room);
        Check(r != nullptr, std::string("reference view ") + v.name + " names a real room (" + v.room + ")");
        if (r != nullptr) Check(v.advance >= 0.0f && v.advance < r->halfD * 2.0f, std::string("reference view ") + v.name + " stays inside its room");
        Check(FindReferenceView(v.name) == &v, std::string("FindReferenceView round trip for ") + v.name);
    }
}

void TestGeneratedDocumentsMentionEveryRoom(const MuseumPlan& m)
{
    const std::string map = MapAsMarkdown(m), coverage = CoverageAsMarkdown(m);
    for (const RoomDef& r : m.rooms) {
        if (r.kind != CellKind::Room) continue;
        Check(map.find("`" + r.key + "`") != std::string::npos, "the generated map lists " + r.key);
        for (const std::string& api : r.apis) Check(coverage.find("| " + api + " |") != std::string::npos, "the coverage table lists " + api);
    }
}

void TestToursPointAtRealRooms(const MuseumPlan& m)
{
    // docs/TOURS.md names rooms as `room:KEY`; every one must exist.
    const std::string path = std::string(MUSEUM_SOURCE_CONTENT_DIR) + "/../docs/TOURS.md";
    std::ifstream in(path);
    if (!in) {
        std::printf("note: %s not found, tour links not checked\n", path.c_str());
        return;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();
    int found = 0;
    for (std::size_t at = text.find("`room:"); at != std::string::npos; at = text.find("`room:", at + 1)) {
        const std::size_t end = text.find('`', at + 1);
        const std::string key = text.substr(at + 6, end - at - 6);
        if (key.find('*') != std::string::npos) continue;   // a pattern such as rv-*
        ++found;
        Check(m.FindByKey(key) != nullptr, "docs/TOURS.md: room " + key + " exists");
    }
    Check(found > 30, "docs/TOURS.md names a good number of rooms");
}

}  // namespace

int main()
{
    const MuseumPlan& m = TheMuseum();
    TestCatalogueIsValid(m);
    TestFloorStructure(m);
    TestKeysAndNumbers(m);
    TestPointLookup(m);
    TestRoomFrames(m);
    TestDoorsAreSymmetric(m);
    TestExhibitsResolve(m);
    TestShaderLabHasEveryDialect(m);
    TestAssetsExist(m);
    TestInventory(m);
    TestEveryFloorIsConnected(m);
    TestCollisionUsesFloorHeight();
    TestReferenceViews(m);
    TestGeneratedDocumentsMentionEveryRoom(m);
    TestToursPointAtRealRooms(m);
    std::printf("%d checks, %d failed\n%s", g_checks, g_failures, InventoryAsText(m).c_str());
    return g_failures == 0 ? 0 : 1;
}
