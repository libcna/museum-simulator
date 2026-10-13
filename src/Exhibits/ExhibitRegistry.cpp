#include "Exhibits/ExhibitRegistry.hpp"

#include "Exhibits/ExhibitFactories.hpp"

namespace {

struct Entry {
    const char* name;
    std::unique_ptr<Exhibit> (*make)(const RoomDef&, Services&);
};

constexpr Entry kEntries[] = {
    {"lobby", MakeLobby},
    {"hub", MakeHub},
    {"spine", MakeSpine},
    {"gallery", MakeGallery},
    {"elevator", MakeElevator},
    {"info", MakeInfoHall},
    {"materials", MakeMaterialsRoom},
    {"lighting", MakeLightingRoom},
    {"renderstates", MakeRenderStateRoom},
    {"rendertarget", MakeRenderTargetRoom},
    {"particles", MakeParticleRoom},
    {"models", MakeModelGallery},
    {"animation", MakeAnimationRoom},
    {"sky", MakeSkyRoom},
    {"audio", MakeAudioRoom},
    {"vectors", MakeVectorLab},
    {"transforms", MakeTransformLab},
    {"quaternions", MakeQuaternionLab},
    {"curves", MakeCurveLab},
    {"collision", MakeCollisionLab},
    {"cameras", MakeCameraLab},
    {"applab", MakeAppLab},
    {"sprites", MakeSpriteLab},
    {"text", MakeTextLab},
    {"geometry", MakeGeometryLab},
};

}  // namespace

std::unique_ptr<Exhibit> CreateExhibit(const RoomDef& room, Services& services)
{
    for (const Entry& e : kEntries)
        if (room.exhibit == e.name) return e.make(room, services);
    return nullptr;
}

bool IsKnownExhibit(const std::string& name)
{
    if (name.empty()) return true;   // an empty themed room
    for (const Entry& e : kEntries)
        if (name == e.name) return true;
    return false;
}

std::vector<std::string> KnownExhibits()
{
    std::vector<std::string> names;
    for (const Entry& e : kEntries) names.emplace_back(e.name);
    return names;
}
