#include "Exhibits/Exhibit.hpp"

#include "Audio/AudioSystem.hpp"

Exhibit::~Exhibit()
{
    if (services_.audio != nullptr)
        for (Voice3D* v : voices_) services_.audio->ReleaseLoop(v);
}

Voice3D* Exhibit::CreateLoop(const std::string& sound, const Microsoft::Xna::Framework::Vector3& position, float volume, float radius)
{
    if (services_.audio == nullptr) return nullptr;
    Voice3D* voice = services_.audio->CreateLoop(sound, position, volume, radius);
    if (voice != nullptr) voices_.push_back(voice);
    return voice;
}
