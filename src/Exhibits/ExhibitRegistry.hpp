#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Exhibits/Exhibit.hpp"

/**
 * Exhibit factories by name. A RoomDef names its exhibit; a data-driven exhibit reads its
 * RoomDef::params to decide what to show, so one factory serves many rooms.
 * The table is explicit (ExhibitRegistry.cpp) rather than filled by static initialisers,
 * which a static library would silently drop.
 */
std::unique_ptr<Exhibit> CreateExhibit(const RoomDef& room, Services& services);
[[nodiscard]] bool IsKnownExhibit(const std::string& name);
[[nodiscard]] std::vector<std::string> KnownExhibits();
