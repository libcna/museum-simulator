#pragma once

#include "Exhibits/Services.hpp"
#include "World/Catalogue.hpp"

class RoomBuilder;

/**
 * The standard information plaque every exhibit room carries beside its door: room number and
 * title, what it demonstrates, the CNA APIs involved and whether they are standard XNA 4.0 or a
 * CNA extension. Generated from the catalogue, so it never disagrees with the directory.
 */
void AddRoomCard(RoomBuilder& builder, Services& services, const RoomDef& room);
