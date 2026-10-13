#include "Catalogue/CatalogueBuilder.hpp"

std::vector<FloorSpec> AllFloorSpecs()
{
    std::vector<FloorSpec> floors;
    floors.push_back(MakeFloor0());
    floors.push_back(MakeFloor1());
    floors.push_back(MakeFloor2());
    floors.push_back(MakeFloor3());
    floors.push_back(MakeFloor4());
    floors.push_back(MakeFloor5());
    floors.push_back(MakeFloor6());
    floors.push_back(MakeFloor7());
    floors.push_back(MakeFloor8());
    floors.push_back(MakeFloor9());
    return floors;
}
