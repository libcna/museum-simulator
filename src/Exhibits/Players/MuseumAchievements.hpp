#pragma once

#include <set>
#include <string>

struct MuseumPlan;

/**
 * The museum's own achievements (Content/GamerServices/Achievements.json), awarded to the first signed-in gamer through CNA's
 * GamerServices as the visitor explores. A no-op in a build without GamerServices.
 */
namespace MuseumAchievements {

/** Call when the set of discovered rooms grows: awards what the new total has earned. */
void RoomsDiscovered(const MuseumPlan& museum, const std::set<int>& discovered);

/** Call after a ride in the elevator. */
void RodeTheElevator();

}  // namespace MuseumAchievements
