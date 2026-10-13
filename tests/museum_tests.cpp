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
#include "World/Catalogue.hpp"

using Microsoft::Xna::Framework::BoundingBox;

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
                matched = matched || (dx < kCellGap + 0.01f && dz < kCellGap + 0.01f);
            }
            Check(matched, "door " + r.key + " -> " + other.key + " lines up with its twin");
        }
    }
}

void TestExhibitsResolve(const MuseumPlan& m)
{
    for (const RoomDef& r : m.rooms) Check(IsKnownExhibit(r.exhibit), "exhibit '" + r.exhibit + "' of " + r.key + " is registered");
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
    // From each floor's hall, walking through doors must reach every cell of that floor (and nothing of another floor).
    for (const FloorDef& f : m.floors) {
        int onFloor = 0;
        for (const RoomDef& r : m.rooms) onFloor += r.floor == f.index ? 1 : 0;
        std::set<int> seen = {f.hub};
        std::queue<int> open;
        open.push(f.hub);
        while (!open.empty()) {
            const int c = open.front();
            open.pop();
            for (const DoorDef& d : m.Room(c).doors)
                if (d.to >= 0 && seen.insert(d.to).second) open.push(d.to);
        }
        bool crossed = false;
        for (const int c : seen) crossed = crossed || m.Room(c).floor != f.index;
        Check(static_cast<int>(seen.size()) == onFloor, "floor " + std::to_string(f.index) + ": every cell is reachable from the hall (" + std::to_string(seen.size()) + " of " + std::to_string(onFloor) + ")");
        Check(!crossed, "floor " + std::to_string(f.index) + ": doors never lead to another floor");
    }
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
    TestDoorsAreSymmetric(m);
    TestExhibitsResolve(m);
    TestAssetsExist(m);
    TestInventory(m);
    TestEveryFloorIsConnected(m);
    TestReferenceViews(m);
    TestGeneratedDocumentsMentionEveryRoom(m);
    TestToursPointAtRealRooms(m);
    std::printf("%d checks, %d failed\n%s", g_checks, g_failures, InventoryAsText(m).c_str());
    return g_failures == 0 ? 0 : 1;
}
