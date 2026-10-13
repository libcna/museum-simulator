#include "Catalogue/CatalogueBuilder.hpp"

std::vector<FloorSpec> AllFloorSpecs()
{
    std::vector<FloorSpec> floors;
    floors.push_back(MakeFloor0());
    return floors;
}
