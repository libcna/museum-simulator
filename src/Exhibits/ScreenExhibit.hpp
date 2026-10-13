#pragma once

// Base for exhibits whose centrepiece is a live 2D screen on the far wall: a render target drawn with
// SpriteBatch (virtual DrawScreen), shown as a quad, with the usual control panel and information sign.
#include <memory>
#include <string>
#include <vector>

#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Rendering/LiveScreen.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/RoomBuilder.hpp"

class ScreenExhibit : public Exhibit {
public:
    ScreenExhibit(const RoomDef& room, Services& services, int pixelWidth = 960, int pixelHeight = 540, float widthMetres = 8.2f)
        : Exhibit(room, services), pixelWidth_(pixelWidth), pixelHeight_(pixelHeight), widthMetres_(widthMetres)
    {
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        screen_ = std::make_unique<LiveScreen>(S(), pixelWidth_, pixelHeight_);
        OnScreenLoaded();
    }

    void RenderOffscreen(const DrawContext&) override
    {
        if (!screen_) return;
        screen_->RenderCustom(Background(), [this](Microsoft::Xna::Framework::Graphics::SpriteBatch& sb, int, int) { DrawScreen(sb); });
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!screen_ || !screen_->HasContent()) return;
        Microsoft::Xna::Framework::Vector3 centre;
        float yaw;
        FarScreenPose(2.6f, centre, yaw);
        using Microsoft::Xna::Framework::Vector3;
        const Vector3 n(std::sin(yaw), 0, std::cos(yaw));
        const Vector3 right = Vector3::Normalize(Vector3::Cross(-n, Vector3::Up)) * widthMetres_;
        const Vector3 up(0, widthMetres_ * static_cast<float>(pixelHeight_) / static_cast<float>(pixelWidth_), 0);
        RenderStates::SetOpaque3D(ctx.device);
        QuadDraw::Textured(ctx, screen_->Texture(), centre - right * 0.5f - up * 0.5f, right, up);
        DrawExtra(ctx);
    }

protected:
    /** Draws the screen's content; the SpriteBatch is not begun: call Begin/End as the demonstration needs. */
    virtual void DrawScreen(Microsoft::Xna::Framework::Graphics::SpriteBatch& sb) = 0;
    virtual void OnScreenLoaded() {}
    virtual void DrawExtra(const DrawContext&) {}
    [[nodiscard]] virtual Microsoft::Xna::Framework::Color Background() const { return Microsoft::Xna::Framework::Color(26, 32, 48, 255); }

    /** The standard information sign on the east wall. */
    void AddInfoSign(RoomBuilder& b, const std::string& title, const std::vector<std::string>& lines)
    {
        SignSpec what = Kit::Plate(title, lines);
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    std::unique_ptr<LiveScreen> screen_;
    int pixelWidth_, pixelHeight_;
    float widthMetres_;
};
