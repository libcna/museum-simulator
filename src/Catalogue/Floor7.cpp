// Floor 7: audio and input.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec AudioWing()
{
    WingSpec w;
    w.name = "Audio Complex";
    w.tagline = "SoundEffect, instances, 3D audio, synthesis, generated sounds, music and the microphone";
    w.theme = "audio-hall";
    w.accent = {0.95f, 0.50f, 0.65f};
    w.spineSide = Wall::East;
    const auto au = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "audio-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "audio-hall";
        r.apis = std::move(apis);
        return r;
    };
    w.rooms = {
        au("au-basics", "SoundEffect.Play", "Volume, pitch and pan of fire-and-forget sounds, with overlapping playback", "basics", {"SoundEffect.Play", "SoundEffect", "ContentManager.Load<SoundEffect>"}),
        au("au-layers", "Instances and Layers", "Four looping SoundEffectInstances mixed live: volume, pitch, state", "layers", {"SoundEffectInstance", "SoundEffectInstance.IsLooped", "SoundEffectInstance.Volume", "SoundEffectInstance.Pitch", "SoundState"}),
        au("au-spatial", "3D Audio", "AudioEmitter and AudioListener with distance and Doppler settings", "spatial", {"SoundEffectInstance.Apply3D", "AudioEmitter", "AudioListener", "SoundEffect.DopplerScale", "SoundEffect.DistanceScale", "SoundEffect.SpeedOfSound"}),
        au("au-synth", "A Synthesiser", "DynamicSoundEffectInstance playing notes computed from waveform formulas", "synth", {"DynamicSoundEffectInstance", "DynamicSoundEffectInstance.SubmitBuffer", "AudioChannels"}),
        au("au-generate", "Generated Sounds", "SoundEffect built from a PCM buffer: laser, explosion, bell, coin and wind", "generate", {"SoundEffect(buffer, sampleRate, channels)", "AudioChannels"}),
        au("au-master", "Master Volume and Settings", "SoundEffect.MasterVolume and the static 3D audio properties", "master", {"SoundEffect.MasterVolume", "SoundEffect.DistanceScale", "SoundEffect.DopplerScale", "SoundEffect.SpeedOfSound"}),
        au("au-music", "Song and MediaPlayer", "Streaming music with a queue, pause, repeat and volume", "music", {"Song", "MediaPlayer.Play", "MediaPlayer.Volume", "MediaState"}),
        au("au-mic", "The Microphone", "Capturing 16-bit PCM and a level meter; reports cleanly when there is no microphone", "microphone", {"Microphone", "Microphone.GetData", "NoMicrophoneConnectedException"}),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor7()
{
    FloorSpec f;
    f.index = 7;
    f.name = "Audio and Input";
    f.tagline = "Sound, music, keyboards, mice and game pads";
    f.theme = "concourse";
    f.accent = {0.95f, 0.50f, 0.65f};
    f.wings.push_back(AudioWing());
    return f;
}
