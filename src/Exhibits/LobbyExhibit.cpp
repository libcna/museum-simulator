#include <algorithm>
#include <cmath>
#include <string>

#include "Audio/AudioSystem.hpp"
#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Rendering/SignFactory.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

const Color kGold(255, 226, 150, 255);
const Color kText(222, 228, 240, 255);

// A floor-plan poster drawn with SpriteBatch into a render target at build time.
std::string MakeMapPoster(Services& s)
{
    return s.signs->CreateCustom(440, 580, Color(20, 24, 36, 255), [&s](SpriteBatch& sb, int w, int h) {
        Texture2D& white = *s.textures->White();
        constexpr float kMinX = -28.5f, kMaxX = 28.5f, kMinZ = -68.5f, kMaxZ = 8.5f;
        const float margin = 36.0f;
        const float scale = std::min((static_cast<float>(w) - margin * 2.0f) / (kMaxX - kMinX),
                                     (static_cast<float>(h) - margin * 2.0f - 40.0f) / (kMaxZ - kMinZ));
        const auto px = [&](float x) { return margin + (x - kMinX) * scale; };
        const auto pz = [&](float z) { return margin + 36.0f + (z - kMinZ) * scale; };
        sb.Draw(white, Rectangle(0, 0, w, 6), Color(200, 170, 90, 255));
        sb.Draw(white, Rectangle(0, h - 6, w, 6), Color(200, 170, 90, 255));
        if (s.titleFont != nullptr)
            sb.DrawString(*s.titleFont, "MUSEUM MAP", Vector2(static_cast<float>(w) / 2.0f - s.titleFont->MeasureString("MUSEUM MAP").X * 0.3f, 12.0f),
                          kGold, 0.0f, Vector2(0, 0), 0.6f, SpriteEffects::None, 0.0f);
        // Corridors first, rooms on top.
        for (const RoomDef& a : AllRooms()) {
            for (const RoomDef& b : AllRooms()) {
                const bool east = (b.col == a.col + 1 && b.row == a.row);
                const bool south = (b.row == a.row + 1 && b.col == a.col);
                if (!east && !south) continue;
                const Vector3 ca = a.Center(), cb = b.Center();
                const float x0 = east ? ca.X + a.halfW : ca.X - 1.6f;
                const float x1 = east ? cb.X - b.halfW : ca.X + 1.6f;
                const float z0 = east ? ca.Z - 1.6f : ca.Z + a.halfD;
                const float z1 = east ? ca.Z + 1.6f : cb.Z - b.halfD;
                sb.Draw(white, Rectangle(static_cast<int>(px(x0)), static_cast<int>(pz(z0)), std::max(3, static_cast<int>((x1 - x0) * scale)),
                                         std::max(3, static_cast<int>((z1 - z0) * scale))), Color(90, 100, 124, 255));
            }
        }
        for (const RoomDef& r : AllRooms()) {
            const Vector3 c = r.Center();
            const Rectangle rect(static_cast<int>(px(c.X - r.halfW)), static_cast<int>(pz(c.Z - r.halfD)),
                                 static_cast<int>(2 * r.halfW * scale), static_cast<int>(2 * r.halfD * scale));
            const bool lobby = r.id == RoomId::Lobby;
            sb.Draw(white, rect, lobby ? Color(190, 140, 60, 255) : Color(62, 78, 112, 255));
            sb.Draw(white, Rectangle(rect.X, rect.Y, rect.Width, 2), Color(150, 170, 210, 255));
            std::string label = r.title;
            // Two short lines per room: the first word on top, the rest below.
            std::string top = label, bottom;
            const std::size_t sp = label.find(' ');
            if (sp != std::string::npos) { top = label.substr(0, sp); bottom = label.substr(sp + 1); }
            const float fs = 0.52f;
            const Vector2 ts = s.font->MeasureString(top) * fs;
            sb.DrawString(*s.font, top, Vector2(static_cast<float>(rect.X) + (static_cast<float>(rect.Width) - ts.X) / 2.0f,
                                                static_cast<float>(rect.Y) + static_cast<float>(rect.Height) / 2.0f - ts.Y),
                          Color::White, 0.0f, Vector2(0, 0), fs, SpriteEffects::None, 0.0f);
            const Vector2 bs = s.font->MeasureString(bottom) * fs;
            sb.DrawString(*s.font, bottom, Vector2(static_cast<float>(rect.X) + (static_cast<float>(rect.Width) - bs.X) / 2.0f,
                                                   static_cast<float>(rect.Y) + static_cast<float>(rect.Height) / 2.0f),
                          Color(210, 220, 240, 255), 0.0f, Vector2(0, 0), fs, SpriteEffects::None, 0.0f);
        }
        const RoomDef& lobby = GetRoom(RoomId::Lobby);
        const Vector3 lc = lobby.Center();
        sb.DrawString(*s.font, "YOU START HERE", Vector2(px(lc.X) - 52.0f, pz(lc.Z + lobby.halfD) + 8.0f), kGold, 0.0f,
                      Vector2(0, 0), 0.62f, SpriteEffects::None, 0.0f);
    });
}

class LobbyExhibit final : public Exhibit {
public:
    LobbyExhibit(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        // A burgundy rug under the visitor's first steps.
        b.Meshes().Opaque("carpet").AddPatch(b.World(-3.4f, 0.012f, 3.8f), Vector3(6.8f, 0, 0), Vector3(0, 0, -7.6f), 1.6f,
                                             Vector3(1.0f, 0.9f, 0.9f), 1.0f);

        // Reception desk against the east side of the north wall.
        b.AddBox(Vector3(3.6f, 0.0f, -4.6f), Vector3(7.4f, 1.05f, -3.3f), "wood", 1.0f, Vector3(0.75f, 0.6f, 0.5f));
        b.AddBox(Vector3(3.5f, 1.05f, -4.7f), Vector3(7.5f, 1.12f, -3.2f), "marble", 1.5f, Vector3(0.9f, 0.9f, 0.95f), false);
        SignSpec desk;
        desk.lines = {{"Information", true, kGold}, {"Ask the museum anything", false, kText}};
        b.AddPedestalPlaque(5.5f, -3.95f, 1.3f, 0.62f, desk, 1.6f, 0.45f, 0.0f);

        // Benches by the glass.
        for (const float x : {-4.5f, 4.5f}) {
            b.AddBox(Vector3(x - 1.2f, 0.0f, 4.4f), Vector3(x + 1.2f, 0.45f, 5.0f), "wood", 1.0f, Vector3(0.8f, 0.65f, 0.55f));
            b.AddBox(Vector3(x - 1.2f, 0.45f, 4.9f), Vector3(x + 1.2f, 0.95f, 5.0f), "wood", 1.0f, Vector3(0.75f, 0.6f, 0.5f), false);
        }
        // Potted plants in the corners.
        for (const float x : {-7.2f, 7.2f}) {
            for (const float z : {-5.2f, 5.0f}) {
                if (x < 0.0f && z < 0.0f) continue;   // keep the doorway to the Render Target Room clear
                const Vector3 base = b.World(x, 0.0f, z);
                b.Meshes().Opaque("white").AddCylinder(base, 0.34f, 0.55f, 18, 1.0f, Vector3(0.45f, 0.3f, 0.25f));
                b.Meshes().Opaque("white").AddSphere(base + Vector3(0, 1.15f, 0), 0.62f, 18, 10, Vector3(0.22f, 0.5f, 0.2f));
                b.Meshes().Opaque("white").AddSphere(base + Vector3(0.25f, 1.55f, 0.1f), 0.38f, 14, 8, Vector3(0.28f, 0.58f, 0.24f));
                b.AddCollider(BoundingBox(base + Vector3(-0.4f, 0, -0.4f), base + Vector3(0.4f, 2.0f, 0.4f)));
            }
        }

        // Signs.
        const std::string map = MakeMapPoster(S());
        {
            Vector3 n;
            const Vector3 wallPoint = b.WallPoint(Wall::North, -4.8f, 2.25f, n);
            const Vector3 c = wallPoint + n * 0.035f;
            b.Meshes().Opaque(map).AddPatch(c + Vector3(-0.95f, -1.25f, 0), Vector3(1.9f, 0, 0), Vector3(0, 2.5f, 0), 0.0f, Vector3(1, 1, 1), 2.0f, 0.55f);
        }
        SignSpec welcome;
        welcome.pixelWidth = 1100;
        welcome.lines = {{"Welcome to the CNA 3D Museum", true, kGold},
                         {"Ten rooms, each one demonstrating a part of the CNA framework.", false, kText},
                         {"Walk with W A S D, look with the mouse, press E to interact.", false, kText},
                         {"Start with the map, then follow the corridors.", false, kText}};
        b.AddWallSign(Wall::North, 4.8f, 3.1f, 5.0f, 1.5f, welcome);

        SignSpec toMaterials;
        toMaterials.lines = {{"North: Materials & Textures", false, kGold}, {"then the Information Hall", false, kText}};
        b.AddWallSign(Wall::North, 0.0f, 4.15f, 3.0f, 0.62f, toMaterials);
        SignSpec toRt;
        toRt.lines = {{"West: Render Target Room", false, kGold}, {"and the Model Gallery", false, kText}};
        b.AddWallSign(Wall::West, 0.0f, 4.15f, 3.0f, 0.62f, toRt);

        // The logo, framed, on the east wall.
        b.AddBox(Vector3(7.88f, 1.15f, -1.45f), Vector3(7.97f, 3.65f, 1.45f), "wood", 1.0f, Vector3(0.45f, 0.32f, 0.22f), false);
        b.AddQuadFacing(Vector3(7.96f, 2.4f, 0.0f), -MathHelper::PiOver2, 2.4f, 2.4f, "logo", Vector3(1, 1, 1), 0.5f);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        Interactable desk;
        desk.room = R().id;
        desk.bounds = BoundingBox(R().ToWorld(3.4f, 0.0f, -4.8f), R().ToWorld(7.6f, 1.6f, -3.1f));
        desk.prompt = [this] { return std::string(greeted_ ? "Ring the bell again" : "Ring the reception bell"); };
        desk.activate = [this] {
            greeted_ = true;
            if (S().audio != nullptr) S().audio->PlayAt("arpeggio", R().ToWorld(5.5f, 1.2f, -4.0f), 0.5f, 18.0f);
        };
        interaction.Add(std::move(desk));
    }

private:
    bool greeted_ = false;
};

}  // namespace

std::unique_ptr<Exhibit> MakeLobby(const RoomDef& room, Services& services)
{
    return std::make_unique<LobbyExhibit>(room, services);
}
