// The circulation spaces: lobby and hubs, the concourse, wing galleries and the elevator.
// Everything on their signs comes from the catalogue, so adding a room needs no change here.
#include <algorithm>
#include <cmath>
#include <string>

#include "Audio/AudioSystem.hpp"
#include "Exhibits/Exhibit.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "UI/FloorMap.hpp"
#include "World/RoomBuilder.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Kit;

namespace {

const MuseumPlan& M(const Services& s) { return *s.museum; }

// A floor plan with a "you are here" marker, drawn once with SpriteBatch into a texture.
std::string FloorPlanTexture(Services& s, int floor, int hereCell, int w, int h)
{
    const FloorDef& fd = M(s).floors[static_cast<std::size_t>(floor)];
    return s.signs->CreateCustom(w, h, Color(16, 20, 32, 255), [&](SpriteBatch& sb, int pw, int ph) {
        Texture2D& white = *s.textures->White();
        sb.Draw(white, Rectangle(0, 0, pw, 6), Accent(fd.accent));
        sb.Draw(white, Rectangle(0, ph - 6, pw, 6), Accent(fd.accent));
        const std::string title = "FLOOR " + std::to_string(floor) + "  " + fd.name;
        float fs = 0.55f;
        while (fs > 0.3f && s.titleFont->MeasureString(title).X * fs > static_cast<float>(pw) - 24.0f) fs -= 0.03f;
        sb.DrawString(*s.titleFont, title, Vector2(12, 12), kGold, 0.0f, Vector2(0, 0), fs, SpriteEffects::None, 0.0f);
        MapView view;
        view.floor = floor;
        view.currentCell = hereCell;
        view.labels = true;
        const Rectangle area(10, 56, pw - 20, ph - 56 - 34);
        FloorMap::Draw(s, M(s), view, area);
        sb.DrawString(*s.font, "Gold outline = you are here   |   white marks = doors", Vector2(12, static_cast<float>(ph) - 30.0f),
                      kDim, 0.0f, Vector2(0, 0), 0.5f, SpriteEffects::None, 0.0f);
    });
}

void AddTexturedWallPatch(RoomBuilder& b, Wall wall, float along, float centreY, float width, float height, const std::string& tex,
                          float emissive = 0.55f)
{
    Vector3 n;
    const Vector3 p = b.WallPoint(wall, along, centreY, n);
    const Vector3 centre = p + n * 0.035f;
    const Vector3 right = Vector3::Normalize(Vector3::Cross(-n, Vector3::Up));
    b.Meshes().Opaque(tex).AddPatch(centre - right * (width * 0.5f) - Vector3(0, height * 0.5f, 0), right * width, Vector3(0, height, 0),
                                    0.0f, Vector3(1, 1, 1), 2.0f, emissive);
}

// --- hub and lobby ---------------------------------------------------------------------------------

class HubExhibit final : public Exhibit {
public:
    HubExhibit(const RoomDef& room, Services& services, bool lobby) : Exhibit(room, services), lobby_(lobby) {}

    void BuildStatic(RoomBuilder& b) override
    {
        const MuseumPlan& m = M(S());
        const FloorDef& fd = m.floors[static_cast<std::size_t>(R().floor)];
        const float hw = R().halfW;
        const float hd = R().halfD;

        // A large floor numeral inlaid in the middle of the hall.
        SignSpec numeral;
        numeral.pixelWidth = 256;
        numeral.pixelHeight = 256;
        numeral.lines = {{std::to_string(R().floor), true, Accent(fd.accent)}};
        numeral.background = Color(18, 22, 34, 255);
        numeral.border = Accent(fd.accent, 0.8f);
        numeral.textScale = 3.2f;
        const std::string numeralTex = S().signs->Create(numeral);
        b.Meshes().Opaque(numeralTex).AddPatch(b.World(-1.6f, 0.014f, 1.6f), Vector3(3.2f, 0, 0), Vector3(0, 0, -3.2f), 0.0f,
                                               Vector3(1, 1, 1), 1.0f, 0.5f);
        // A rug of the floor's accent colour around it.
        b.Meshes().Opaque("carpet").AddPatch(b.World(-4.4f, 0.01f, 4.4f), Vector3(8.8f, 0, 0), Vector3(0, 0, -8.8f), 1.6f,
                                             fd.accent * 0.75f + Vector3(0.1f, 0.1f, 0.1f), 1.0f);
        b.Meshes().Opaque(numeralTex).AddPatch(b.World(-1.6f, 0.014f, 1.6f), Vector3(3.2f, 0, 0), Vector3(0, 0, -3.2f), 0.0f,
                                               Vector3(1, 1, 1), 1.0f, 0.5f);

        // Floor title and wing directory beside the concourse door (north wall).
        SignSpec title;
        title.pixelWidth = 900;
        title.lines = {{"Floor " + std::to_string(R().floor) + "  -  " + fd.name, true, Accent(fd.accent)},
                       {fd.tagline, false, kText}};
        if (!lobby_) b.AddWallSign(Wall::North, -hw * 0.5f - 0.5f, 3.6f, std::min(6.4f, hw * 0.9f), 1.3f, title);
        SignSpec wings;
        wings.leftAlign = true;
        wings.lines.push_back({"Wings on this floor", true, kGold});
        for (const int wi : fd.wings) {
            const WingDef& w = m.wings[static_cast<std::size_t>(wi)];
            wings.lines.push_back({std::to_string(w.number) + "  " + w.name + "  (" + std::to_string(w.rooms.size()) + " rooms)", false,
                                   Accent(w.accent, 1.2f)});
        }
        const float wingsH = 0.62f + 0.36f * static_cast<float>(wings.lines.size());
        b.AddWallSign(Wall::North, hw * 0.5f + 0.5f, 3.2f, std::min(6.4f, hw * 0.9f), std::max(1.6f, wingsH), wings);

        SignSpec toSpine;
        toSpine.lines = {{"Concourse  " + std::string("↑"), true, kGold}, {"to the wings", false, kText}};
        b.AddWallSign(Wall::North, 0.0f, 4.45f, 3.4f, 0.8f, toSpine);

        // Elevator (east wall): door sign and the list of floors.
        SignSpec toLift;
        toLift.lines = {{"Elevators  " + std::string("→"), true, kGold}, {"all floors", false, kText}};
        b.AddWallSign(Wall::East, 0.0f, 4.45f, 3.4f, 0.8f, toLift);
        SignSpec floors;
        floors.leftAlign = true;
        floors.lines.push_back({"Floors", true, kGold});
        for (const FloorDef& f : m.floors)
            floors.lines.push_back({std::to_string(f.index) + "  " + f.name, false, f.index == R().floor ? kGold : kText});
        b.AddWallSign(Wall::East, -hd * 0.5f - 0.2f, 3.0f, std::min(5.2f, hd * 0.85f), 0.5f + 0.34f * static_cast<float>(floors.lines.size()), floors);

        const auto stairSign = [&](int cell, const std::string& direction) {
            if (cell < 0) return;
            for (const DoorDef& d : R().doors) {
                if (d.to != cell) continue;
                SignSpec sign;
                sign.lines = {{"Stairs " + direction, true, kGold}};
                sign.border = Accent(fd.accent);
                b.AddWallSign(d.wall, d.along, 4.25f, 3.0f, 0.62f, sign);
                break;
            }
        };
        stairSign(fd.stairsUp, "up  ↑");
        stairSign(fd.stairsDown, "down  ↓");

        // The plan of this floor. The lobby has a glass wall to the south, so its plan hangs on the west wall.
        planTexture_ = FloorPlanTexture(S(), R().floor, R().index, 720, 900);
        if (lobby_) AddTexturedWallPatch(b, Wall::West, 0.0f, 2.35f, 3.0f, 3.75f, planTexture_);
        else AddTexturedWallPatch(b, Wall::South, 0.0f, 2.6f, 3.8f, 4.75f, planTexture_);

        if (lobby_) BuildLobbyFurniture(b);
        else BuildHubFurniture(b);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        // The directory kiosk: opens the full-screen map and directory.
        Interactable kiosk;
        kiosk.bounds = BoundingBox(R().ToWorld(kioskX_ - 0.7f, 0.0f, kioskZ_ - 0.7f), R().ToWorld(kioskX_ + 0.7f, 1.8f, kioskZ_ + 0.7f));
        kiosk.prompt = [] { return std::string("Open the museum directory and map"); };
        kiosk.activate = [this] {
            if (S().openDirectory) S().openDirectory();
        };
        interaction.Add(std::move(kiosk));
        if (lobby_) {
            Interactable desk;
            desk.bounds = BoundingBox(R().ToWorld(3.4f, 0.0f, -R().halfD + 0.9f), R().ToWorld(7.6f, 1.6f, -R().halfD + 2.8f));
            desk.prompt = [this] { return std::string(greeted_ ? "Ring the bell again" : "Ring the reception bell"); };
            desk.activate = [this] {
                greeted_ = true;
                if (S().audio != nullptr) S().audio->PlayAt("arpeggio", R().ToWorld(5.5f, 1.2f, -R().halfD + 1.8f), 0.5f, 18.0f);
            };
            interaction.Add(std::move(desk));
        }
    }

private:
    void BuildHubFurniture(RoomBuilder& b)
    {
        kioskX_ = -3.4f;
        kioskZ_ = -2.2f;
        BuildKiosk(b);
        for (const float x : {-R().halfW + 1.2f, R().halfW - 1.2f}) {
            for (const float z : {R().halfD - 1.2f}) PlantAt(b, x, z);
        }
        // Benches along the south wall.
        for (const float x : {-4.5f, 4.5f}) {
            b.AddBox(Vector3(x - 1.2f, 0.0f, R().halfD - 0.9f), Vector3(x + 1.2f, 0.45f, R().halfD - 0.3f), "wood", 1.0f, Vector3(0.8f, 0.65f, 0.55f));
        }
    }

    void BuildLobbyFurniture(RoomBuilder& b)
    {
        const float hd = R().halfD;
        kioskX_ = -3.4f;
        kioskZ_ = -2.0f;
        BuildKiosk(b);
        // A burgundy rug under the visitor's first steps.
        b.Meshes().Opaque("carpet").AddPatch(b.World(-3.0f, 0.012f, hd - 0.6f), Vector3(6.0f, 0, 0), Vector3(0, 0, -5.4f), 1.6f,
                                             Vector3(1.0f, 0.9f, 0.9f), 1.0f);
        // Reception desk against the east side of the north wall.
        b.AddBox(Vector3(3.6f, 0.0f, -hd + 1.0f), Vector3(7.4f, 1.05f, -hd + 2.3f), "wood", 1.0f, Vector3(0.75f, 0.6f, 0.5f));
        b.AddBox(Vector3(3.5f, 1.05f, -hd + 0.9f), Vector3(7.5f, 1.12f, -hd + 2.4f), "marble", 1.5f, Vector3(0.9f, 0.9f, 0.95f), false);
        SignSpec desk;
        desk.lines = {{"Information", true, kGold}, {"Ask the museum anything", false, kText}};
        b.AddPedestalPlaque(5.5f, -hd + 1.65f, 1.3f, 0.62f, desk, 1.6f, 0.45f, 0.0f);
        // Benches by the glass.
        for (const float x : {-4.5f, 4.5f}) {
            b.AddBox(Vector3(x - 1.2f, 0.0f, hd - 1.6f), Vector3(x + 1.2f, 0.45f, hd - 1.0f), "wood", 1.0f, Vector3(0.8f, 0.65f, 0.55f));
            b.AddBox(Vector3(x - 1.2f, 0.45f, hd - 1.1f), Vector3(x + 1.2f, 0.95f, hd - 1.0f), "wood", 1.0f, Vector3(0.75f, 0.6f, 0.5f), false);
        }
        PlantAt(b, -R().halfW + 1.0f, hd - 1.2f);
        PlantAt(b, R().halfW - 1.0f, hd - 1.2f);

        SignSpec welcome;
        welcome.pixelWidth = 1100;
        welcome.lines = {{"Welcome to the CNA Museum", true, kGold},
                         {"Ten floors of interactive demonstrations of the CNA framework.", false, kText},
                         {"Walk with W A S D, look with the mouse, press E or click to interact.", false, kText},
                         {"The elevators are east; the concourse north leads to the first wings.", false, kText},
                         {"Press M for the museum map and directory, F1 for all controls.", false, kText}};
        b.AddWallSign(Wall::North, -4.8f, 2.6f, 5.0f, 1.7f, welcome);
        // The logo, framed, on the east wall north of the elevator door.
        b.AddBox(Vector3(R().halfW - 0.12f, 1.4f, -hd + 1.2f), Vector3(R().halfW - 0.03f, 3.6f, -hd + 3.6f), "wood", 1.0f,
                 Vector3(0.45f, 0.32f, 0.22f), false);
        b.AddQuadFacing(Vector3(R().halfW - 0.04f, 2.5f, -hd + 2.4f), -MathHelper::PiOver2, 2.0f, 2.0f, "logo", Vector3(1, 1, 1), 0.5f);
    }

    void BuildKiosk(RoomBuilder& b)
    {
        const std::string tex = "metal";
        b.AddBox(Vector3(kioskX_ - 0.45f, 0.0f, kioskZ_ - 0.45f), Vector3(kioskX_ + 0.45f, 1.15f, kioskZ_ + 0.45f), tex, 1.0f,
                 Vector3(0.45f, 0.5f, 0.62f));
        b.AddBox(Vector3(kioskX_ - 0.5f, 1.15f, kioskZ_ - 0.5f), Vector3(kioskX_ + 0.5f, 1.2f, kioskZ_ + 0.5f), "marble", 1.0f,
                 Vector3(0.9f, 0.9f, 0.95f), false);
        SignSpec plate;
        plate.lines = {{"Directory", true, kGold}, {"Map, search, quick travel", false, kText}};
        b.AddPedestalPlaque(kioskX_, kioskZ_, 0.9f, 0.7f, plate, 0.8f, 0.3f);
    }

    void PlantAt(RoomBuilder& b, float x, float z)
    {
        const Vector3 base = b.World(x, 0.0f, z);
        b.Meshes().Opaque("white").AddCylinder(base, 0.34f, 0.55f, 18, 1.0f, Vector3(0.45f, 0.3f, 0.25f));
        b.Meshes().Opaque("white").AddSphere(base + Vector3(0, 1.15f, 0), 0.62f, 18, 10, Vector3(0.22f, 0.5f, 0.2f));
        b.Meshes().Opaque("white").AddSphere(base + Vector3(0.25f, 1.55f, 0.1f), 0.38f, 14, 8, Vector3(0.28f, 0.58f, 0.24f));
        b.AddCollider(BoundingBox(base + Vector3(-0.4f, 0, -0.4f), base + Vector3(0.4f, 2.0f, 0.4f)));
    }

    bool lobby_;
    bool greeted_ = false;
    float kioskX_ = 0.0f, kioskZ_ = 0.0f;
    std::string planTexture_;
};

// --- concourse -------------------------------------------------------------------------------------

class SpineExhibit final : public Exhibit {
public:
    SpineExhibit(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        const MuseumPlan& m = M(S());
        const FloorDef& fd = m.floors[static_cast<std::size_t>(R().floor)];
        // An accent runner down the middle of the floor.
        b.Meshes().Opaque("carpet").AddPatch(b.World(-0.9f, 0.01f, R().halfD), Vector3(1.8f, 0, 0), Vector3(0, 0, -2.0f * R().halfD), 1.6f,
                                             fd.accent * 0.7f + Vector3(0.1f, 0.1f, 0.1f), 2.0f);
        for (const DoorDef& d : R().doors) {
            const RoomDef& to = m.Room(d.to);
            if (to.kind == CellKind::Gallery) {
                const WingDef& wing = m.wings[static_cast<std::size_t>(to.wing)];
                const bool east = d.wall == Wall::East;
                SignSpec sign;
                sign.pixelWidth = 640;
                sign.lines = {{(east ? std::string("Wing ") : std::string("← Wing ")) + std::to_string(wing.number) + (east ? " →" : ""), true,
                               Accent(wing.accent, 1.2f)},
                              {wing.name, false, kText},
                              {std::to_string(wing.rooms.size()) + " rooms", false, kDim}};
                sign.border = Accent(wing.accent);
                b.AddWallSign(d.wall, d.along, 4.2f, 3.2f, 1.1f, sign);
            } else if (to.kind == CellKind::Hub) {
                SignSpec sign;
                sign.lines = {{"↓  Hall and elevators", true, kGold}, {"Floor " + std::to_string(R().floor) + "  " + fd.name, false, kText}};
                b.AddWallSign(Wall::South, 0.0f, 4.0f, 3.6f, 1.0f, sign);
            }
        }
        SignSpec end;
        end.pixelWidth = 640;
        end.lines = {{"End of the concourse", true, kGold}, {"Turn back for the hall and the elevators", false, kText}};
        b.AddWallSign(Wall::North, 0.0f, 2.6f, 4.6f, 1.0f, end);
        b.AddPedestal(0.0f, -R().halfD + 1.4f, 0.0f + 0.9f, 0.1f, "stone");   // a low plinth marks the end of the run
    }
};

// --- wing gallery ----------------------------------------------------------------------------------

class GalleryExhibit final : public Exhibit {
public:
    GalleryExhibit(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        const MuseumPlan& m = M(S());
        const WingDef& wing = m.wings[static_cast<std::size_t>(R().wing)];
        // A runner in the wing colour makes the gallery recognisable at a glance.
        b.Meshes().Opaque("carpet").AddPatch(b.World(-R().halfW, 0.01f, 0.7f), Vector3(2.0f * R().halfW, 0, 0), Vector3(0, 0, -1.4f), 1.6f,
                                             wing.accent * 0.7f + Vector3(0.1f, 0.1f, 0.1f), 2.0f);
        for (const DoorDef& d : R().doors) {
            const RoomDef& to = m.Room(d.to);
            if (to.kind == CellKind::Room) {
                SignSpec sign;
                sign.pixelWidth = 560;
                sign.lines = {{to.number, false, Accent(wing.accent, 1.2f)}, {to.title, true, kText}};
                sign.border = Accent(wing.accent);
                b.AddWallSign(d.wall, d.along, 3.95f, 2.8f, 0.9f, sign);
            } else if (to.kind == CellKind::Spine) {
                SignSpec sign;
                sign.lines = {{"Concourse", true, kGold}, {d.wall == Wall::West ? "← back to the hall and elevators" : "back to the hall and elevators →", false, kText}};
                b.AddWallSign(d.wall, d.along, 3.8f, 3.0f, 0.9f, sign);
            }
        }
        // The far end of the gallery carries the wing's title.
        const Wall far = wing.spineSide == Wall::East ? Wall::East : Wall::West;
        SignSpec title;
        title.pixelWidth = 900;
        title.lines = {{"Wing " + std::to_string(wing.number) + "  -  " + wing.name, true, Accent(wing.accent, 1.2f)},
                       {wing.tagline, false, kText},
                       {std::to_string(wing.rooms.size()) + " rooms along this gallery", false, kDim}};
        title.border = Accent(wing.accent);
        b.AddWallSign(far, 0.0f, 2.3f, 4.8f, 1.5f, title);
    }
};

// --- elevator ------------------------------------------------------------------------------------

class ElevatorExhibit final : public Exhibit {
public:
    ElevatorExhibit(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        const MuseumPlan& m = M(S());
        const int count = static_cast<int>(m.floors.size());
        // Button panel on the north wall: two columns, floors counted upwards from the bottom row.
        const int cols = 2;
        const int rows = (count + cols - 1) / cols;
        b.AddBox(Vector3(-0.62f, 0.9f, -R().halfD), Vector3(0.62f, 0.9f + 0.34f * static_cast<float>(rows) + 0.2f, -R().halfD + 0.06f), "metal", 1.0f,
                 Vector3(0.3f, 0.3f, 0.34f), false);
        for (int f = 0; f < count; ++f) {
            const int col = f % cols;
            const int row = f / cols;
            const float x = (static_cast<float>(col) - 0.5f) * 0.55f;
            const float y = 1.05f + 0.34f * static_cast<float>(row);
            SignSpec button;
            button.pixelWidth = 64;
            button.pixelHeight = 64;
            const bool here = f == R().floor;
            button.background = here ? Color(190, 150, 50, 255) : Color(36, 40, 54, 255);
            button.border = here ? Color(255, 230, 150, 255) : Color(120, 130, 150, 255);
            button.lines = {{std::to_string(f), true, here ? Color(30, 20, 5, 255) : kText}};
            const std::string tex = S().signs->Create(button);
            b.AddQuadFacing(Vector3(x, y, -R().halfD + 0.075f), 0.0f, 0.26f, 0.26f, tex, Vector3(1, 1, 1), 0.8f);
        }
        SignSpec header;
        header.lines = {{"Floor " + std::to_string(R().floor), true, kGold}, {m.floors[static_cast<std::size_t>(R().floor)].name, false, kText}};
        b.AddWallSign(Wall::North, 0.0f, 2.7f, 2.2f, 0.7f, header);
        // Handrails.
        b.AddBox(Vector3(-R().halfW + 0.05f, 0.9f, -R().halfD + 0.8f), Vector3(-R().halfW + 0.12f, 0.96f, R().halfD - 0.8f), "metal", 1.0f,
                 Vector3(0.8f, 0.8f, 0.85f), false);
        b.AddBox(Vector3(R().halfW - 0.12f, 0.9f, R().halfD - 0.8f), Vector3(R().halfW - 0.05f, 0.96f, -R().halfD + 0.8f), "metal", 1.0f,
                 Vector3(0.8f, 0.8f, 0.85f), false);
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        const MuseumPlan& m = M(S());
        const int count = static_cast<int>(m.floors.size());
        const int cols = 2;
        for (int f = 0; f < count; ++f) {
            const int col = f % cols;
            const int row = f / cols;
            const float x = (static_cast<float>(col) - 0.5f) * 0.55f;
            const float y = 1.05f + 0.34f * static_cast<float>(row);
            Interactable ia;
            ia.bounds = BoundingBox(R().ToWorld(x - 0.15f, y - 0.15f, -R().halfD), R().ToWorld(x + 0.15f, y + 0.15f, -R().halfD + 0.3f));
            ia.maxDistance = 3.5f;
            ia.prompt = [this, f, &m] {
                return f == R().floor ? std::string("You are on floor ") + std::to_string(f)
                                      : std::string("Ride to floor ") + std::to_string(f) + ": " + m.floors[static_cast<std::size_t>(f)].name;
            };
            ia.activate = [this, f] {
                if (f != R().floor && S().travelToFloor) S().travelToFloor(f);
            };
            interaction.Add(std::move(ia));
        }
    }
};

class StairsExhibit final : public Exhibit {
public:
    StairsExhibit(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        const int upper = R().params.GetInt("upper-floor", R().floor + 1);
        const float landingEyeLevel = kFloorPitch * 0.5f + 1.7f;

        // Both flights reach the west wall at the half-storey landing.  Signs at
        // either floor's eye level are below/above the visitor here and cannot
        // guide the switchback, so put one directly in front of each approach.
        SignSpec up;
        up.lines = {{"STAIRS UP  →", true, kGold},
                    {"Turn right on the landing", false, kText},
                    {"Continue to floor " + std::to_string(upper), false, kDim}};
        up.border = Accent(M(S()).floors[static_cast<std::size_t>(upper)].accent);
        b.AddWallSign(Wall::West, kStairLaneOffset, landingEyeLevel, 3.0f, 1.15f, up);

        SignSpec down;
        down.lines = {{"←  STAIRS DOWN", true, kGold},
                      {"Turn left on the landing", false, kText},
                      {"Continue to floor " + std::to_string(R().floor), false, kDim}};
        down.border = Accent(R().accent);
        b.AddWallSign(Wall::West, -kStairLaneOffset, landingEyeLevel, 3.0f, 1.15f, down);
    }
};

}  // namespace

std::unique_ptr<Exhibit> MakeLobby(const RoomDef& room, Services& services)
{
    return std::make_unique<HubExhibit>(room, services, true);
}
std::unique_ptr<Exhibit> MakeHub(const RoomDef& room, Services& services)
{
    return std::make_unique<HubExhibit>(room, services, false);
}
std::unique_ptr<Exhibit> MakeSpine(const RoomDef& room, Services& services)
{
    return std::make_unique<SpineExhibit>(room, services);
}
std::unique_ptr<Exhibit> MakeGallery(const RoomDef& room, Services& services)
{
    return std::make_unique<GalleryExhibit>(room, services);
}
std::unique_ptr<Exhibit> MakeElevator(const RoomDef& room, Services& services)
{
    return std::make_unique<ElevatorExhibit>(room, services);
}
std::unique_ptr<Exhibit> MakeStairs(const RoomDef& room, Services& services)
{
    return std::make_unique<StairsExhibit>(room, services);
}
