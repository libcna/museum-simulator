#pragma once

#include <memory>

#include "Exhibits/Exhibit.hpp"

// One factory per room; ExhibitFactory.cpp maps RoomId to the right one.
std::unique_ptr<Exhibit> MakeLobby(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeHub(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeSpine(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeGallery(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeElevator(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeInfoHall(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeMaterialsRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeLightingRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeRenderStateRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeRenderTargetRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeParticleRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeModelGallery(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeAnimationRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeSkyRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeAudioRoom(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeVectorLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeTransformLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeQuaternionLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeCurveLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeCollisionLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeCameraLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeAppLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeSpriteLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeTextLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeGeometryLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeTextureLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeShaderLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeEffectLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeLightingLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeRenderStateLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeTargetLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeParticleLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeModelLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeSkinFxLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeReflectLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeTerrainLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeAnimLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeAudioLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeInputLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeContentLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeCnaExtLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakePlatformLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakePerfLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeValidationLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeRoofLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeViewsLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeQueryLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeServicesLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeShadowLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeHistoryLab(const RoomDef& room, Services& services);
std::unique_ptr<Exhibit> MakeStructLab(const RoomDef& room, Services& services);
