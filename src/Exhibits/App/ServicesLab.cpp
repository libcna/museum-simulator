// GameServiceContainer: a type-keyed registry that lets unrelated parts of a game find each other without
// globals. The room owns its own container (never the game's, so nothing leaks into other rooms), registers two
// services on request and shows what three consumers get when they ask for them.
#include <exception>
#include <string>
#include <typeinfo>
#include <vector>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/GameServiceContainer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IGraphicsDeviceService.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

struct IScoreService {
    virtual ~IScoreService() = default;
    virtual void Add(int points) = 0;
    [[nodiscard]] virtual int Total() const = 0;
};
struct IMessageService {
    virtual ~IMessageService() = default;
    virtual void Post(const std::string& text) = 0;
    [[nodiscard]] virtual const std::vector<std::string>& Messages() const = 0;
};
class ScoreService final : public IScoreService {
public:
    void Add(int points) override { total_ += points; }
    [[nodiscard]] int Total() const override { return total_; }
private:
    int total_ = 0;
};
class MessageService final : public IMessageService {
public:
    void Post(const std::string& text) override
    {
        messages_.insert(messages_.begin(), text);
        if (messages_.size() > 6) messages_.pop_back();
    }
    [[nodiscard]] const std::vector<std::string>& Messages() const override { return messages_; }
private:
    std::vector<std::string> messages_;
};

class ServicesLab final : public ScreenExhibit {
public:
    ServicesLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("GameServiceContainer", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddToggle("AddService<IScoreService>", false, [this](bool v) { Toggle(v, true); });
        p.AddToggle("AddService<IMessageService>", false, [this](bool v) { Toggle(v, false); });
        p.AddButton("Door consumer scores 10", [this] { Consume(0); });
        p.AddButton("Sign consumer posts a message", [this] { Consume(1); });
        p.AddButton("Hud consumer asks for both", [this] { Consume(2); });
        p.AddButton("AddService twice (same type)", [this] { AddTwice(); });
        p.AddButton("Ask the GAME's container for IGraphicsDeviceService", [this] { AskGame(); });
        p.AddReadout("Last result", [this] { return last_; });
        AddInfoSign(b, "GameServiceContainer", {"AddService<T>(provider) files a pointer under the type T; GetService<T>() returns it, or null when nobody", "registered that type. Game.Services is one of these: components use it to find the GraphicsDevice or your own services."});
    }

    ~ServicesLab() override = default;

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        Texture2D& white = *S().whiteTexture;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text("GameServiceContainer", 24, 10, Kit::kGold, 1.0f);
        rect(300, 70, 360, 200, Kit::Rgba(24, 30, 46));
        rect(300, 70, 360, 4, Kit::Rgba(255, 210, 120));
        text("container", 312, 82, Kit::kDim, 0.6f);
        const Color on = Kit::Rgba(110, 230, 150), off = Kit::Rgba(90, 96, 120);
        rect(320, 120, 320, 56, score_ ? on : off);
        text(score_ ? "IScoreService  ->  ScoreService, total " + std::to_string(scoreImpl_.Total()) : "IScoreService  (not registered)", 332, 138, score_ ? Kit::Rgba(20, 20, 20) : Kit::kDim, 0.65f);
        rect(320, 190, 320, 56, msg_ ? on : off);
        text(msg_ ? "IMessageService  ->  MessageService, " + std::to_string(msgImpl_.Messages().size()) + " messages" : "IMessageService  (not registered)", 332, 208, msg_ ? Kit::Rgba(20, 20, 20) : Kit::kDim, 0.65f);
        static const char* names[3] = {"Door", "Sign", "Hud"};
        for (int i = 0; i < 3; ++i) {
            const float x = 60.0f + static_cast<float>(i) * 300.0f;
            rect(x, 330, 240, 80, Kit::Rgba(30, 38, 60));
            text(names[i], x + 12, 338, Kit::kText, 0.75f);
            text(asked_[i], x + 12, 372, asked_[i].find("null") != std::string::npos ? Kit::Rgba(235, 150, 120) : Kit::kDim, 0.55f);
        }
        float y = 430.0f;
        for (const std::string& m : msgImpl_.Messages()) {
            text("message: " + m, 60, y, Kit::kDim, 0.55f);
            y += 16.0f;
        }
        sb.End();
    }

private:
    void Toggle(bool on, bool score)
    {
        try {
            if (score) {
                if (on) container_.AddService<IScoreService>(&scoreImpl_);
                else container_.RemoveService<IScoreService>();
                score_ = on;
            } else {
                if (on) container_.AddService<IMessageService>(&msgImpl_);
                else container_.RemoveService<IMessageService>();
                msg_ = on;
            }
            last_ = std::string(on ? "added " : "removed ") + (score ? "IScoreService" : "IMessageService");
        } catch (const std::exception& e) {
            last_ = std::string("exception: ") + e.what();
        }
    }
    void Consume(int who)
    {
        auto* score = container_.GetService<IScoreService>();
        auto* msg = container_.GetService<IMessageService>();
        std::string result;
        if (who == 0) {
            if (score != nullptr) score->Add(10);
            result = score != nullptr ? "GetService<IScoreService>: found, scored" : "GetService<IScoreService>: null";
        } else if (who == 1) {
            if (msg != nullptr) msg->Post("hello from the sign");
            result = msg != nullptr ? "GetService<IMessageService>: found, posted" : "GetService<IMessageService>: null";
        } else {
            result = std::string("score ") + (score != nullptr ? "found" : "null") + ", messages " + (msg != nullptr ? "found" : "null");
        }
        asked_[who] = result;
        last_ = result;
    }
    void AskGame()
    {
        auto* svc = S().game->getServicesProperty().GetService<IGraphicsDeviceService>();
        last_ = svc != nullptr ? "Game.Services holds IGraphicsDeviceService: its device is " + std::string(svc->getGraphicsDeviceProperty() != nullptr ? "available" : "null") : "Game.Services has no IGraphicsDeviceService";
    }
    void AddTwice()
    {
        try {
            container_.AddService<IScoreService>(&scoreImpl_);
            container_.AddService<IScoreService>(&scoreImpl_);
            score_ = true;
            last_ = "the second AddService of the same type was accepted";
        } catch (const std::exception& e) {
            score_ = container_.GetService<IScoreService>() != nullptr;
            last_ = std::string("the second AddService threw: ") + e.what();
        }
    }

    GameServiceContainer container_;
    ScoreService scoreImpl_;
    MessageService msgImpl_;
    bool score_ = false, msg_ = false;
    std::string last_ = "nothing registered yet";
    std::string asked_[3] = {"has not asked yet", "has not asked yet", "has not asked yet"};
};

}  // namespace

std::unique_ptr<Exhibit> MakeServicesLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ServicesLab>(room, services);
}
