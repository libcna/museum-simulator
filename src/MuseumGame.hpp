#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Audio/AudioSystem.hpp"
#include "Camera/FirstPersonCamera.hpp"
#include "Camera/InputState.hpp"
#include "Camera/PlayerController.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "MuseumOptions.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/ModelLibrary.hpp"
#include "Rendering/SignFactory.hpp"
#include "Rendering/SkyDome.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "UI/Hud.hpp"
#include "World/MuseumWorld.hpp"

/**
 * The application: a CNA Game that owns the camera, the world, audio and HUD. Update and
 * Draw only orchestrate; each exhibit does its own work.
 */
class MuseumGame : public Microsoft::Xna::Framework::Game {
public:
    explicit MuseumGame(const MuseumOptions& options);
    ~MuseumGame() override;
    GetTypeNameHPP()

protected:
    void LoadContent() override;
    void UnloadContent() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

private:
    void HandleSystemKeys();
    void SetPaused(bool paused);
    void TeleportToRoom(const RoomDef& room);
    void ResetToEntrance();
    void UpdateFrameStats(float dt);
    void RunScript();
    void SaveScreenshot(const std::string& path);
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 RoomSpawn(const RoomDef& room) const;

    MuseumOptions options_;
    Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;

    // Everything below holds GPU resources or services and is released in LoadContent's reverse order.
    Services services_;
    FrameStats stats_;
    std::unique_ptr<TextureLibrary> textures_;
    std::unique_ptr<ModelLibrary> models_;
    std::unique_ptr<EffectBank> effects_;
    std::unique_ptr<Shapes> shapes_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> sprites_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteFont> font_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteFont> titleFont_;
    std::unique_ptr<SignFactory> signs_;
    std::unique_ptr<AudioSystem> audio_;
    TimeOfDay timeOfDay_;
    std::unique_ptr<SkyDome> sky_;
    std::unique_ptr<MuseumWorld> world_;
    std::unique_ptr<Hud> hud_;

    FirstPersonCamera camera_;
    std::unique_ptr<PlayerController> player_;
    InputState input_;

    double time_ = 0.0;
    long long frame_ = 0;
    bool paused_ = false;
    bool wasActive_ = true;
    bool showHelp_ = false;
    bool showStats_ = false;
    bool showMap_ = false;
    float sensitivityScale_ = 1.0f;
    std::optional<RoomId> shownRoom_;
    float bannerAge_ = 99.0f;
    float fpsAccumulator_ = 0.0f;
    int fpsFrames_ = 0;
    std::string rendererName_;
    std::string focusPrompt_;
    bool screenshotDone_ = false;
    int scriptWait_ = 0;
    std::string pendingShot_;
    std::vector<std::string> scriptSteps_;
    std::size_t scriptIndex_ = 0;
};
