#pragma once

#include <set>
#include <string>

#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "World/Catalogue.hpp"

/** What a floor plan shows and where the viewer is. */
struct MapView {
    int floor = 0;
    int currentCell = -1;                 // highlighted outline; -1 for none
    bool showPlayer = false;
    Microsoft::Xna::Framework::Vector3 player;
    float yaw = 0.0f;
    const std::set<int>* discovered = nullptr;   // cells to draw lit; null draws everything lit
    bool labels = true;                   // room numbers (and names when there is room)
    bool follow = false;                  // centre on the player and show `followRadius` metres around them
    float followRadius = 55.0f;
    int selectedCell = -1;                // cursor in the directory
    int highlightWing = -1;
};

/**
 * Draws one floor of the museum as a plan view with SpriteBatch. Used by the corner
 * minimap, the full-screen map, the directory and the wall maps.
 * The caller owns the SpriteBatch Begin/End pair.
 */
namespace FloorMap {

/** Draws the plan fitted into `area` (or around the player when view.follow). Returns metres-to-pixels scale. */
float Draw(Services& services, const MuseumPlan& museum, const MapView& view, const Microsoft::Xna::Framework::Rectangle& area);

/** The plan's bounding box on the floor plane (min x, min z, max x, max z). */
void FloorBounds(const MuseumPlan& museum, int floor, float& minX, float& minZ, float& maxX, float& maxZ);

/** The cell under a pixel of a plan drawn with the same arguments, or -1. */
int CellAtPixel(const MuseumPlan& museum, const MapView& view, const Microsoft::Xna::Framework::Rectangle& area, int px, int py);

}  // namespace FloorMap
