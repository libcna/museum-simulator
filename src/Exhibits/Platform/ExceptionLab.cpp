// Exception laboratory: what CNA throws when a program does the wrong thing, and with what message. Each button provokes one mistake
// on purpose, catches it, and shows the exception's own type and text. These are the same exception types XNA documents (CNA keeps
// the .NET names), so a port's catch blocks keep working; the museum itself relies on this when a renderer refuses something.
#include <algorithm>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>
#if defined(__GNUG__)
#include <cxxabi.h>
#endif

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Content/ContentLoadException.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Storage/StorageContainer.hpp"
#include "Microsoft/Xna/Framework/Storage/StorageDevice.hpp"
#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#ifdef MUSEUM_NETWORKING
#include "Microsoft/Xna/Framework/GamerServices/Guide.hpp"
#include "Microsoft/Xna/Framework/GamerServices/Gamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamerCollection.hpp"
#include "Microsoft/Xna/Framework/PlayerIndex.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

std::string TypeName(const std::exception& e)
{
    const char* raw = typeid(e).name();
#if defined(__GNUG__)
    int status = 0;
    std::unique_ptr<char, void (*)(void*)> name(abi::__cxa_demangle(raw, nullptr, nullptr, &status), std::free);
    if (status == 0 && name) return name.get();
#endif
    return raw;
}

struct Case {
    const char* doing;
    std::function<void()> action;
    std::string result = "(not run)";
    int status = 0;   // 0 not run, 1 threw, 2 did not throw
};

class ExceptionLab final : public ScreenExhibit {
public:
    ExceptionLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        BuildCases();
        ControlPanel& p = AddPanel("Provoke an exception", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        for (std::size_t i = 0; i < cases_.size(); ++i) p.AddButton(cases_[i].doing, [this, i] { Run(i); });
        p.AddButton("Run them all", [this] { for (std::size_t i = 0; i < cases_.size(); ++i) Run(i); });
        AddInfoSign(b, "Exceptions CNA Throws",
                    {"A library is judged by how it fails. Each button here does one wrong thing on purpose and shows the exception that comes back: the type is the .NET name XNA documents,",
                     "so a catch block written for XNA catches it here. The room never crashes: every call is caught, and an action that does NOT throw is flagged in orange."});
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto text = [&](const std::string& t, float x, float y, Color c, float s) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text("Mistakes and what they throw", 24, 12, Kit::kGold, 1.0f);
        float y = 56.0f;
        // As many rows as there are cases fit the 540-pixel screen: the row height and the text shrink together.
        const float pitch = std::min(50.0f, 474.0f / static_cast<float>(std::max<std::size_t>(1, cases_.size())));
        const float row = pitch - 4.0f, scale = std::min(1.0f, pitch / 50.0f);
        for (const Case& c : cases_) {
            sb.Draw(white, Rectangle(24, static_cast<int>(y), 912, static_cast<int>(row)), Color(22, 30, 46, 235));
            sb.Draw(white, Rectangle(24, static_cast<int>(y), 5, static_cast<int>(row)), c.status == 1 ? Color(110, 230, 150, 255) : c.status == 2 ? Color(255, 170, 90, 255) : Color(90, 100, 130, 255));
            text(c.doing, 40, y + 2, Kit::kText, 0.55f * scale);
            text(c.result, 40, y + 2 + 21.0f * scale, c.status == 2 ? Kit::Rgba(255, 190, 150) : Kit::kDim, 0.45f * scale);
            y += pitch;
        }
        sb.End();
    }

private:
    void Run(std::size_t i)
    {
        Case& c = cases_[i];
        try {
            c.action();
            c.result = "no exception was thrown";
            c.status = 2;
        } catch (const std::exception& e) {
            c.result = TypeName(e) + ": " + std::string(e.what()).substr(0, 110);
            c.status = 1;
        }
    }

    void BuildCases()
    {
        GraphicsDevice* device = S().device;
        cases_.push_back({"Use a texture after it was disposed (Texture2D.Dispose, then SetData)", [device] {
                              Texture2D t(*device, 4, 4);
                              t.Dispose();
                              std::vector<Color> px(16);
                              t.SetData(px.data(), 16);
                          }});
        cases_.push_back({"Create a texture with no pixels (Texture2D 0 x 0)", [device] { Texture2D t(*device, 0, 0); }});
        cases_.push_back({"End a SpriteBatch that was never begun", [device] {
                              SpriteBatch batch(*device);
                              batch.End();
                          }});
        cases_.push_back({"Begin a SpriteBatch twice", [device] {
                              SpriteBatch batch(*device);
                              batch.Begin();
                              try {
                                  batch.Begin();
                              } catch (...) {
                                  batch.End();
                                  throw;
                              }
                              batch.End();
                          }});
        cases_.push_back({"Load a content file that does not exist (ContentManager.Load)", [this] {
                              Content::ContentManager content(&S().game->getServicesProperty(), S().contentRoot);
                              (void)content.Load<Texture2D>("Textures/not-there");
                          }});
        cases_.push_back({"Open a title file that does not exist (TitleContainer.OpenStream)", [] { (void)TitleContainer::OpenStream("Content/nothing-here.txt"); }});
        cases_.push_back({"Reach outside the save folder (StorageContainer.OpenFile \"../escape\")", [] {
                              Storage::StorageDevice::SetAppNameEXT("cna-3d-museum");
                              auto selector = Storage::StorageDevice::BeginShowSelector(nullptr, nullptr);
                              auto dev = Storage::StorageDevice::EndShowSelector(selector.get());
                              auto open = dev->BeginOpenContainer("MuseumLab", nullptr, nullptr);
                              auto container = dev->EndOpenContainer(open.get());
                              (void)container->OpenFile("../escape", System::IO::FileMode::OpenOrCreate);
                          }});
#ifdef MUSEUM_NETWORKING
        using Microsoft::Xna::Framework::GamerServices::Guide;
        cases_.push_back({"Ask for an online page with no account service (Guide.ShowFriends)", [] { Guide::ShowFriends(PlayerIndex::One); }});
        cases_.push_back({"Open the marketplace for an offline profile (Guide.ShowMarketplace)", [] { Guide::ShowMarketplace(PlayerIndex::One); }});
        cases_.push_back({"Ask an offline profile who its friends are (SignedInGamer.IsFriend)", [] {
                              auto* gamers = Microsoft::Xna::Framework::GamerServices::Gamer::getSignedInGamersProperty();
                              if (gamers == nullptr || gamers->begin() == gamers->end()) throw std::runtime_error("no profile is signed in");
                              (void)(*gamers->begin())->IsFriend(*gamers->begin());
                          }});
        cases_.push_back({"Open the Guide twice (Guide.BeginShowMessageBox, again)", [] {
                              auto* first = Guide::BeginShowMessageBox("Exceptions", "one", {"OK"}, 0, GamerServices::MessageBoxIcon::None, {}, std::any{});
                              (void)first;
                              try {
                                  (void)Guide::BeginShowMessageBox("Exceptions", "two", {"OK"}, 0, GamerServices::MessageBoxIcon::None, {}, std::any{});
                              } catch (...) {
                                  Guide::ResetPendingMessageBoxForTestingEXT();
                                  delete first;
                                  throw;
                              }
                              Guide::ResetPendingMessageBoxForTestingEXT();
                              delete first;
                          }});
#endif
    }

    std::vector<Case> cases_;
};

}  // namespace

std::unique_ptr<Exhibit> MakeExceptionLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ExceptionLab>(room, services);
}
