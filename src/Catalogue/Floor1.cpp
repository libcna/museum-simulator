// Floor 1: Fundamentals (placeholder content, replaced as the wings are written).
#include "Catalogue/CatalogueBuilder.hpp"

FloorSpec MakeFloor1()
{
    FloorSpec f;
    f.index = 1;
    f.name = "Fundamentals";
    f.tagline = "The application model, math and geometry";
    f.theme = "concourse";
    f.accent = {0.40f, 0.75f, 0.95f};
    WingSpec w;
    w.name = "Application Model";
    w.tagline = "Game, GameTime and the device";
    w.theme = "corridor-cool";
    w.accent = {0.40f, 0.75f, 0.95f};
    w.rooms = {
        {.key = "placeholder-a", .title = "Placeholder A", .subtitle = "to be replaced", .width = 12, .depth = 12},
        {.key = "placeholder-b", .title = "Placeholder B", .subtitle = "to be replaced", .width = 12, .depth = 12},
    };
    f.wings.push_back(w);
    return f;
}
