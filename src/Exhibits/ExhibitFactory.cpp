#include "Exhibits/ExhibitFactories.hpp"
#include "World/MuseumWorld.hpp"

std::unique_ptr<Exhibit> CreateExhibit(const RoomDef& room, Services& services)
{
    switch (room.id) {
        case RoomId::Lobby: return MakeLobby(room, services);
        case RoomId::Info: return MakeInfoHall(room, services);
        case RoomId::Materials: return MakeMaterialsRoom(room, services);
        case RoomId::Lighting: return MakeLightingRoom(room, services);
        case RoomId::RenderStates: return MakeRenderStateRoom(room, services);
        case RoomId::RenderTarget: return MakeRenderTargetRoom(room, services);
        case RoomId::Particles: return MakeParticleRoom(room, services);
        case RoomId::Models: return MakeModelGallery(room, services);
        case RoomId::Animation: return MakeAnimationRoom(room, services);
        case RoomId::Sky: return MakeSkyRoom(room, services);
        case RoomId::Audio: return MakeAudioRoom(room, services);
        default: return nullptr;
    }
}
