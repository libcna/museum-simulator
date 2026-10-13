#include "Catalogue/CatalogueBuilder.hpp"

std::vector<FloorSpec> AllFloorSpecs()
{
    std::vector<FloorSpec> floors;
    floors.push_back(MakeFloor0());
    floors.push_back(MakeFloor1());
    floors.push_back(MakeFloor2());
    return floors;
}
