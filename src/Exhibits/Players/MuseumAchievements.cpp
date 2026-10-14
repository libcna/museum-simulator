#include "Exhibits/Players/MuseumAchievements.hpp"

#include <exception>

#include "Diagnostics/Log.hpp"
#include "World/Catalogue.hpp"

#ifdef MUSEUM_NETWORKING
#include "Microsoft/Xna/Framework/GamerServices/Gamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamerCollection.hpp"
#endif

namespace MuseumAchievements {

namespace {

#ifdef MUSEUM_NETWORKING
// Awards a key once per run (CNA keeps the first earned date anyway; this only saves the store a write per frame).
void Award(const char* key)
{
    static std::set<std::string> done;
    if (!done.insert(key).second) return;
    using namespace Microsoft::Xna::Framework::GamerServices;
    auto* gamers = Gamer::getSignedInGamersProperty();
    if (gamers == nullptr || gamers->getCountProperty() == 0) {
        done.erase(key);   // nobody to give it to yet: try again when someone signs in
        return;
    }
    try {
        (*gamers)[0]->AwardAchievement(key);
        Museum::Log("achievement", "awarded %s to %s", key, (*gamers)[0]->getGamertagProperty().c_str());
    } catch (const std::exception& e) {
        Museum::Log("achievement", "could not award %s: %s", key, e.what());
    }
}
#endif

}  // namespace

void RoomsDiscovered(const MuseumPlan& museum, const std::set<int>& discovered)
{
#ifdef MUSEUM_NETWORKING
    int rooms = 0, totalRooms = 0;
    std::set<int> floors;
    for (const RoomDef& r : museum.rooms)
        if (r.kind == CellKind::Room) ++totalRooms;
    for (const int cell : discovered) {
        const RoomDef& r = museum.Room(cell);
        if (r.kind != CellKind::Room) continue;
        ++rooms;
        floors.insert(r.floor);
    }
    if (rooms >= 1) Award("first-steps");
    if (rooms >= 25) Award("explorer");
    if (rooms >= 100) Award("cartographer");
    if (static_cast<int>(floors.size()) >= static_cast<int>(museum.floors.size())) Award("every-floor");
    if (totalRooms > 0 && rooms >= totalRooms) Award("completionist");
#else
    (void)museum;
    (void)discovered;
#endif
}

void RodeTheElevator()
{
#ifdef MUSEUM_NETWORKING
    Award("elevator");
#endif
}

}  // namespace MuseumAchievements
