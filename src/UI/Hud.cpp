#include "UI/Hud.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "UI/FloorMap.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {
Color Fade(Color c, float a)
{
    return Color(c.getRProperty(), c.getGProperty(), c.getBProperty(),
                 static_cast<std::uint8_t>(std::clamp(a, 0.0f, 1.0f) * static_cast<float>(c.getAProperty())));
}
}  // namespace

void Hud::Toast(const std::string& message, float seconds)
{
    toasts_.push_back({message, seconds});
    if (toasts_.size() > 4) toasts_.pop_front();
}

void Hud::Update(float dt)
{
    for (ToastItem& t : toasts_) t.remaining -= dt;
    while (!toasts_.empty() && toasts_.front().remaining <= 0.0f) toasts_.pop_front();
}

void Hud::Rect(int x, int y, int w, int h, Color color)
{
    s_.sprites->Draw(*s_.textures->White(), Rectangle(x, y, w, h), color);
}

void Hud::Panel(int x, int y, int w, int h)
{
    Rect(x, y, w, h, Color(10, 14, 24, 190));
    Rect(x, y, w, 2, Color(200, 170, 90, 220));
    Rect(x, y + h - 2, w, 2, Color(200, 170, 90, 220));
}

void Hud::Text(const SpriteFont& font, const std::string& text, Vector2 position, Color color, float scale, bool shadow)
{
    if (shadow)
        s_.sprites->DrawString(font, text, position + Vector2(1.5f * scale, 1.5f * scale), Color(0, 0, 0, 200), 0.0f,
                               Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
    s_.sprites->DrawString(font, text, position, color, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
}

void Hud::TextCentred(const SpriteFont& font, const std::string& text, float centreX, float y, Color color, float scale)
{
    const Vector2 size = font.MeasureString(text);
    Text(font, text, Vector2(centreX - size.X * scale * 0.5f, y), color, scale);
}

void Hud::DrawCrosshair(bool focused, int w, int h)
{
    const int cx = w / 2;
    const int cy = h / 2;
    const Color c = focused ? Color(255, 214, 90, 240) : Color(255, 255, 255, 170);
    const int arm = focused ? 7 : 5;
    const int gap = focused ? 3 : 2;
    Rect(cx - arm - gap, cy - 1, arm, 2, c);
    Rect(cx + gap, cy - 1, arm, 2, c);
    Rect(cx - 1, cy - arm - gap, 2, arm, c);
    Rect(cx - 1, cy + gap, 2, arm, c);
}

void Hud::DrawMinimap(const HudState& state, int screenW)
{
    if (s_.museum == nullptr) return;
    const int size = std::clamp(screenW / 5, 190, 300);
    const int px = screenW - size - 14;
    const int py = 14;
    Panel(px, py, size, size);
    MapView view;
    view.floor = state.floor;
    view.currentCell = state.cell;
    view.showPlayer = true;
    view.player = state.playerPosition;
    view.yaw = state.playerYaw;
    view.discovered = state.discovered;
    view.follow = true;
    view.followRadius = 48.0f;
    FloorMap::Draw(s_, *s_.museum, view, Rectangle(px + 4, py + 4, size - 8, size - 8));
    char label[48];
    std::snprintf(label, sizeof label, "FLOOR %d", state.floor);
    Text(*s_.font, label, Vector2(static_cast<float>(px) + 10.0f, static_cast<float>(py + size) + 6.0f), Color(255, 214, 90, 255), 0.6f);
}

void Hud::DrawHelp(int w, int h)
{
    const char* lines[] = {
        "CONTROLS",
        "W A S D / Up Down   walk            Mouse   look around",
        "Left Right   turn                   Page Up / Down   look up / down",
        "Shift   walk faster                 E / Left click   interact",
        "Esc   pause menu                    F1 / H   this help",
        "M / Tab   directory and map            F3   statistics",
        "F11   fullscreen                    R   return to the entrance        N   minimap on/off",
        "F2   mute audio                     [ ]   mouse sensitivity       T / Y   time of day",
        "Gamepad: left stick walk, right stick look, A interact, Start pause",
    };
    const int panelW = 760;
    const int panelH = 28 + static_cast<int>(sizeof(lines) / sizeof(lines[0])) * 30;
    const int x = (w - panelW) / 2;
    const int y = (h - panelH) / 2;
    Panel(x, y, panelW, panelH);
    float ty = static_cast<float>(y) + 14.0f;
    for (std::size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i) {
        Text(*s_.font, lines[i], Vector2(static_cast<float>(x) + 24.0f, ty), i == 0 ? Color(255, 214, 90, 255) : Color(235, 238, 245, 255));
        ty += 30.0f;
    }
}

void Hud::DrawPause(const HudState& state, int w, int h)
{
    Rect(0, 0, w, h, Color(0, 0, 0, 140));
    const int panelW = 560;
    const int panelH = 360;
    const int x = (w - panelW) / 2;
    const int y = (h - panelH) / 2;
    Panel(x, y, panelW, panelH);
    TextCentred(*s_.titleFont, "PAUSED", static_cast<float>(w) / 2.0f, static_cast<float>(y) + 18.0f, Color(255, 214, 90, 255));
    char buf[160];
    float ty = static_cast<float>(y) + 100.0f;
    const auto line = [&](const std::string& text) {
        TextCentred(*s_.font, text, static_cast<float>(w) / 2.0f, ty, Color(235, 238, 245, 255));
        ty += 34.0f;
    };
    line("Esc / click   resume");
    line("R   return to the entrance");
    std::snprintf(buf, sizeof(buf), "[ ]   mouse sensitivity: %.2fx", static_cast<double>(state.mouseSensitivityScale));
    line(buf);
    std::snprintf(buf, sizeof(buf), "- =   volume: %d%%   (F2 mute: %s)", static_cast<int>(state.volume * 100.0f + 0.5f),
                  state.audioMuted ? "on" : "off");
    line(buf);
    line("F11   fullscreen");
    line("F1   controls");
    line("Q   quit");
}

void Hud::Draw(const HudState& state)
{
    if (s_.sprites == nullptr || s_.font == nullptr) return;
    GraphicsDevice& device = *s_.device;
    const int w = device.getViewportProperty().getWidthProperty();
    const int h = device.getViewportProperty().getHeightProperty();

    const SamplerState sampler = SamplerState::LinearClamp;
    s_.sprites->Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &sampler, nullptr, nullptr);

    if (!state.paused) {
        DrawCrosshair(!state.prompt.empty(), w, h);
        if (!state.prompt.empty()) {
            const std::string text = "[E]  " + state.prompt;
            const Vector2 size = s_.font->MeasureString(text);
            const int pw = static_cast<int>(size.X) + 36;
            Rect(w / 2 - pw / 2, h / 2 + 46, pw, 40, Color(10, 14, 24, 200));
            Rect(w / 2 - pw / 2, h / 2 + 46, pw, 2, Color(255, 214, 90, 230));
            Text(*s_.font, text, Vector2(static_cast<float>(w) / 2.0f - size.X / 2.0f, static_cast<float>(h) / 2.0f + 51.0f),
                 Color(255, 244, 214, 255));
        }
        if (state.bannerAlpha > 0.01f && !state.roomTitle.empty()) {
            const float a = state.bannerAlpha;
            const Vector2 ts = s_.titleFont->MeasureString(state.roomTitle);
            const float bw = std::max(ts.X, s_.font->MeasureString(state.roomSubtitle).X) + 80.0f;
            Rect(static_cast<int>(static_cast<float>(w) / 2.0f - bw / 2.0f), 56, static_cast<int>(bw), 112,
                 Fade(Color(10, 14, 24, 190), a));
            TextCentred(*s_.titleFont, state.roomTitle, static_cast<float>(w) / 2.0f, 62.0f, Fade(Color(255, 214, 90, 255), a));
            TextCentred(*s_.font, state.roomSubtitle, static_cast<float>(w) / 2.0f, 126.0f, Fade(Color(230, 235, 245, 255), a));
        }
        float ty = static_cast<float>(h) - 40.0f;
        for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
            const float a = std::min(1.0f, it->remaining * 2.0f);
            TextCentred(*s_.font, it->text, static_cast<float>(w) / 2.0f, ty, Fade(Color(255, 255, 255, 255), a));
            ty -= 32.0f;
        }
        if (state.showMap) DrawMinimap(state, w);
    }

    if (state.showStats && s_.stats != nullptr) {
        char buf[200];
        std::snprintf(buf, sizeof(buf), "%.0f fps   %.2f ms   %dx%d   %s", static_cast<double>(s_.stats->fps),
                      static_cast<double>(s_.stats->frameMilliseconds), s_.stats->backBufferWidth, s_.stats->backBufferHeight,
                      state.rendererName.c_str());
        Text(*s_.font, buf, Vector2(14, 12), Color(150, 255, 160, 255));
        std::snprintf(buf, sizeof(buf), "pos %.1f %.1f %.1f   zones drawn %d   voices %d",
                      static_cast<double>(state.playerPosition.X), static_cast<double>(state.playerPosition.Y),
                      static_cast<double>(state.playerPosition.Z), s_.stats->roomsDrawn, s_.stats->activeVoices);
        Text(*s_.font, buf, Vector2(14, 40), Color(150, 255, 160, 255));
    }
    if (state.showHelp && !state.paused) DrawHelp(w, h);
    if (state.paused) DrawPause(state, w, h);
    if (state.fade > 0.001f) {
        Rect(0, 0, w, h, Fade(Color(0, 0, 0, 255), state.fade));
        if (!state.fadeText.empty())
            TextCentred(*s_.titleFont, state.fadeText, static_cast<float>(w) / 2.0f, static_cast<float>(h) / 2.0f - 30.0f,
                        Fade(Color(255, 214, 90, 255), state.fade), 0.8f);
    }
    s_.sprites->End();
}
