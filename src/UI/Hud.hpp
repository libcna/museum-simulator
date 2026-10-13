#pragma once

#include <deque>
#include <string>

#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "World/RoomDef.hpp"

struct HudState {
    std::string prompt;          // interaction text for the crosshair target, empty when none
    bool paused = false;
    bool showHelp = false;
    bool showStats = false;
    bool showMap = false;
    bool audioMuted = false;
    float mouseSensitivityScale = 1.0f;
    float volume = 1.0f;
    std::string roomTitle;       // current room, empty in a corridor
    std::string roomSubtitle;
    float bannerAlpha = 0.0f;    // 0..1 fade for the room banner
    Microsoft::Xna::Framework::Vector3 playerPosition;
    float playerYaw = 0.0f;
    std::string rendererName;
};

/** 2D overlay drawn with SpriteBatch + SpriteFont after the 3D scene. */
class Hud {
public:
    explicit Hud(Services& services) : s_(services) {}

    void Toast(const std::string& message, float seconds = 2.5f);
    void Update(float dt);
    void Draw(const HudState& state);

private:
    struct ToastItem {
        std::string text;
        float remaining;
    };

    void Text(const Microsoft::Xna::Framework::Graphics::SpriteFont& font, const std::string& text,
              Microsoft::Xna::Framework::Vector2 position, Microsoft::Xna::Framework::Color color, float scale = 1.0f,
              bool shadow = true);
    void TextCentred(const Microsoft::Xna::Framework::Graphics::SpriteFont& font, const std::string& text, float centreX,
                     float y, Microsoft::Xna::Framework::Color color, float scale = 1.0f);
    void Rect(int x, int y, int w, int h, Microsoft::Xna::Framework::Color color);
    void Panel(int x, int y, int w, int h);
    void DrawCrosshair(bool focused, int w, int h);
    void DrawMinimap(const HudState& state, int screenW);
    void DrawPause(const HudState& state, int w, int h);
    void DrawHelp(int w, int h);

    Services& s_;
    std::deque<ToastItem> toasts_;
};
