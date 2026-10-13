// Named surface/lighting presets for cells. Tints are linear multipliers on the texture colours.
#include <map>

#include "World/Catalogue.hpp"

using Microsoft::Xna::Framework::Vector3;

namespace {

Theme T(const char* floorTex, const char* wallTex, const char* ceilTex, Vector3 wallTint, Vector3 floorTint, Vector3 ambient,
        float light, Vector3 lightColor = Vector3(1.0f, 0.93f, 0.80f))
{
    Theme t;
    t.floorTex = floorTex;
    t.wallTex = wallTex;
    t.ceilTex = ceilTex;
    t.wallTint = wallTint;
    t.floorTint = floorTint;
    t.ambient = ambient;
    t.lightIntensity = light;
    t.lightColor = lightColor;
    return t;
}

const std::map<std::string, Theme>& Themes()
{
    static const std::map<std::string, Theme> themes = {
        // The original eleven rooms.
        {"classic", T("marble", "plaster", "plaster", {0.95f, 0.92f, 0.86f}, {1, 1, 1}, {0.30f, 0.29f, 0.28f}, 1.1f)},
        {"info", T("marble", "plaster", "plaster", {0.82f, 0.88f, 1.0f}, {0.9f, 0.95f, 1.0f}, {0.28f, 0.30f, 0.34f}, 1.2f)},
        {"wood-gallery", T("wood", "plaster", "plaster", {0.96f, 0.88f, 0.76f}, {1, 1, 1}, {0.28f, 0.26f, 0.24f}, 1.1f)},
        {"tiles", T("tiles", "plaster", "plaster", {0.90f, 0.95f, 0.90f}, {1, 1, 1}, {0.30f, 0.30f, 0.30f}, 1.15f)},
        {"dark-room", T("carpet", "plaster", "plaster", {0.40f, 0.42f, 0.55f}, {0.55f, 0.55f, 0.65f}, {0.07f, 0.07f, 0.09f}, 0.35f)},
        {"stone-hall", T("stone", "plaster", "plaster", {0.95f, 0.90f, 0.82f}, {1, 1, 1}, {0.30f, 0.29f, 0.28f}, 1.1f)},
        {"control-room", T("tiles", "metal", "plaster", {0.60f, 0.72f, 0.95f}, {0.75f, 0.85f, 1.0f}, {0.20f, 0.23f, 0.30f}, 1.0f)},
        {"cave", T("cobble", "stone", "stone", {0.62f, 0.58f, 0.58f}, {0.85f, 0.85f, 0.85f}, {0.14f, 0.12f, 0.12f}, 0.55f)},
        {"machine", T("metal", "plaster", "plaster", {0.88f, 0.92f, 0.98f}, {0.9f, 0.95f, 1.0f}, {0.30f, 0.31f, 0.34f}, 1.1f)},
        {"courtyard", T("grass", "brick", "plaster", {1.0f, 0.95f, 0.9f}, {1, 1, 1}, {1.0f, 1.0f, 1.0f}, 0.0f)},
        {"audio-hall", T("carpet", "wood", "plaster", {0.80f, 0.72f, 0.65f}, {1.0f, 0.9f, 0.9f}, {0.22f, 0.20f, 0.20f}, 0.95f)},

        // Floor and wing identities.
        {"elevator", T("metal", "metal", "metal", {0.75f, 0.78f, 0.82f}, {0.8f, 0.8f, 0.85f}, {0.35f, 0.35f, 0.38f}, 1.0f, {0.95f, 0.95f, 1.0f})},
        {"concourse", T("marble", "plaster", "plaster", {0.92f, 0.90f, 0.86f}, {0.95f, 0.95f, 0.98f}, {0.30f, 0.30f, 0.30f}, 1.0f)},
        {"corridor-warm", T("carpet", "plaster", "plaster", {0.95f, 0.88f, 0.78f}, {0.95f, 0.85f, 0.85f}, {0.30f, 0.27f, 0.25f}, 1.0f)},
        {"corridor-cool", T("tiles", "plaster", "plaster", {0.80f, 0.88f, 0.98f}, {0.85f, 0.92f, 1.0f}, {0.26f, 0.29f, 0.34f}, 1.05f)},
        {"corridor-green", T("tiles", "plaster", "plaster", {0.82f, 0.95f, 0.84f}, {0.9f, 1.0f, 0.9f}, {0.26f, 0.32f, 0.27f}, 1.05f)},
        {"corridor-dark", T("tiles", "metal", "metal", {0.45f, 0.5f, 0.62f}, {0.5f, 0.55f, 0.65f}, {0.12f, 0.13f, 0.17f}, 0.7f, {0.7f, 0.85f, 1.0f})},
        {"industrial", T("metal", "metal", "metal", {0.70f, 0.72f, 0.74f}, {0.7f, 0.72f, 0.75f}, {0.25f, 0.25f, 0.26f}, 1.0f, {1.0f, 0.95f, 0.85f})},
        {"brick-loft", T("wood", "brick", "plaster", {0.95f, 0.85f, 0.78f}, {0.9f, 0.85f, 0.8f}, {0.27f, 0.24f, 0.22f}, 1.05f)},
        {"retro", T("carpet", "wood", "plaster", {0.72f, 0.80f, 0.68f}, {0.7f, 0.9f, 0.7f}, {0.22f, 0.24f, 0.20f}, 1.0f)},
        {"futuristic", T("tiles", "metal", "metal", {0.55f, 0.75f, 1.0f}, {0.6f, 0.75f, 0.95f}, {0.14f, 0.18f, 0.28f}, 0.9f, {0.75f, 0.9f, 1.0f})},
        {"neon-lab", T("darkgrid", "metal", "metal", {0.30f, 0.32f, 0.42f}, {0.35f, 0.38f, 0.45f}, {0.07f, 0.08f, 0.12f}, 0.5f, {0.8f, 0.85f, 1.0f})},
        {"workshop", T("wood", "stone", "plaster", {0.80f, 0.74f, 0.66f}, {0.85f, 0.8f, 0.75f}, {0.24f, 0.22f, 0.20f}, 1.0f)},
        {"basement", T("cobble", "stone", "stone", {0.55f, 0.55f, 0.58f}, {0.6f, 0.6f, 0.62f}, {0.16f, 0.16f, 0.18f}, 0.8f, {1.0f, 0.9f, 0.7f})},
        {"white-cube", T("concrete", "panel", "panel", {1.0f, 1.0f, 1.0f}, {0.95f, 0.95f, 0.95f}, {0.45f, 0.45f, 0.45f}, 1.0f, {1.0f, 1.0f, 1.0f})},
        {"garden", T("grass", "brick", "plaster", {0.9f, 0.95f, 0.85f}, {0.95f, 1.0f, 0.95f}, {1.0f, 1.0f, 1.0f}, 0.0f)},
        {"plaza", T("cobble", "stone", "plaster", {0.95f, 0.93f, 0.9f}, {0.95f, 0.95f, 0.95f}, {1.0f, 1.0f, 1.0f}, 0.0f)},
    };
    return themes;
}

}  // namespace

const Theme& FindTheme(const std::string& name)
{
    const auto& themes = Themes();
    const auto it = themes.find(name);
    return it != themes.end() ? it->second : themes.at("classic");
}
