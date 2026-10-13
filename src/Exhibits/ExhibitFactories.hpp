#pragma once

#include <memory>

#include "Exhibits/Exhibit.hpp"

// One factory per room; ExhibitFactory.cpp maps RoomId to the right one.
std::unique_ptr<Exhibit> MakeLobby(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeInfoHall(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeMaterialsRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeLightingRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeRenderStateRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeRenderTargetRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeParticleRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeModelGallery(const RoomDef& room, Services& services);
