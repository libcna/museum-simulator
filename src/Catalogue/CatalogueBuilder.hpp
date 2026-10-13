#pragma once

#include <string>
#include <vector>

#include "World/Catalogue.hpp"

/**
 * Authoring types for the catalogue. Content files return a FloorSpec; Layout.cpp turns the
 * specs into a Museum. Specs are plain data: no pointers, no behaviour.
 */
enum class PlaceSide { Auto, North, South };

struct RoomSpec {
    std::string key;
    std::string title;
    std::string subtitle;
    std::string exhibit;           // exhibit factory name; empty = an empty themed room
    Params params;
    float width = 14.0f;           // interior metres, east-west
    float depth = 14.0f;           // interior metres, north-south
    float height = 5.0f;
    std::string theme;             // empty: the wing's theme
    std::vector<std::string> apis;
    ApiOrigin origin = ApiOrigin::Xna;
    PlaceSide side = PlaceSide::Auto;
    bool openRoof = false;
};

struct WingSpec {
    std::string name;
    std::string tagline;
    std::string theme;
    Microsoft::Xna::Framework::Vector3 accent{0.8f, 0.7f, 0.3f};
    Wall spineSide = Wall::East;   // East or West of the spine
    std::vector<RoomSpec> rooms;
    float galleryHalfWidth = 3.0f;
};

struct FloorSpec {
    int index = 0;
    std::string name;
    std::string tagline;
    std::string theme;
    Microsoft::Xna::Framework::Vector3 accent{0.8f, 0.7f, 0.3f};
    std::string hubExhibit = "hub";
    Params hubParams;              // e.g. {"glass-wall", "south"} for the street-facing lobby
    float hubHalfW = 11.0f, hubHalfD = 9.0f, hubHeight = 6.0f;
    std::vector<WingSpec> wings;
};

/** Each content file defines one floor. */
FloorSpec MakeFloor0();
FloorSpec MakeFloor1();
FloorSpec MakeFloor2();
FloorSpec MakeFloor3();
FloorSpec MakeFloor4();
FloorSpec MakeFloor5();

/** All floors in order. */
std::vector<FloorSpec> AllFloorSpecs();

/** Places the specs and generates cells and doors. Defined in World/Layout.cpp. */
MuseumPlan LayoutMuseum(const std::vector<FloorSpec>& floors);
