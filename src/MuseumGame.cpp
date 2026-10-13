#include "MuseumGame.hpp"
#include "Catalogue/ReferenceViews.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <exception>
#include <sstream>

#include "CNA/GraphicsCapability.hpp"
#include "Diagnostics/Log.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/ClearOptions.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Rendering/AssetPaths.hpp"
#include "Rendering/ImageUtil.hpp"
#include "Rendering/RenderStates.hpp"
#include "UI/FontLoader.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::Input;

namespace {
constexpr float kFov = 70.0f * MathHelper::Pi / 180.0f;
constexpr float kMaxFrameSeconds = 0.1f;
// The near plane is as far out as the 0.32 m collision radius allows: depth precision is what keeps wall
// signs from z-fighting with the wall they hang on, and it improves with the square of the near distance.
constexpr float kNearPlane = 0.2f;
constexpr float kFarPlane = 260.0f;
}  // namespace

MuseumGame::MuseumGame(const MuseumOptions& options) : options_(options), graphics_(this)
{
    // HiDef: render-target readback (signs, screenshots), 32-bit indices and larger textures.
    graphics_.setGraphicsProfileProperty(GraphicsProfile::HiDef);
    graphics_.setPreferredBackBufferWidthProperty(options.width);
    graphics_.setPreferredBackBufferHeightProperty(options.height);
    graphics_.setPreferredDepthStencilFormatProperty(DepthFormat::Depth24Stencil8);
    graphics_.setIsFullScreenProperty(options.fullscreen);
    graphics_.setSynchronizeWithVerticalRetraceProperty(options.vsync);
    // The simulation uses real elapsed time, clamped, so frame pacing is left to vertical sync.
    setIsFixedTimeStepProperty(false);
    getWindowProperty().setTitleProperty("CNA 3D Museum");
    getWindowProperty().setAllowUserResizingProperty(true);
}

MuseumGame::~MuseumGame()
{
    // CNA does not call Game::UnloadContent when the loop ends, so progress is written here.
    try {
        if (progress_ && progressDirty_) progress_->Save(discovered_, *museum_);
    } catch (...) {
    }
}

void MuseumGame::LoadContent()
{
    GraphicsDevice& device = getGraphicsDeviceProperty();
    rendererName_ = std::string(device.GetGraphicsRendererName());
    getWindowProperty().setTitleProperty("CNA 3D Museum  -  " + rendererName_);

    services_.game = this;
    services_.device = &device;
    services_.supportsThreeD = device.SupportsCapability(CNA::GraphicsCapability::ThreeD);
    if (!services_.supportsThreeD) {
        // 2D-only renderers cannot draw the museum; say so rather than throwing from inside a draw call.
        Museum::Log("main", "renderer %s has no 3D pipeline; the museum needs a 3D renderer", rendererName_.c_str());
        return;
    }

    services_.contentRoot = AssetPaths::FindContentRoot();
    if (services_.contentRoot.empty()) {
        Museum::Log("main", "cannot find the Content directory (set MUSEUM_CONTENT); textures, models and sounds will be missing");
    } else {
        getContentProperty().setRootDirectoryProperty(services_.contentRoot);
    }
    services_.content = &getContentProperty();

    textures_ = std::make_unique<TextureLibrary>(device, getContentProperty());
    models_ = std::make_unique<ModelLibrary>(getContentProperty());
    effects_ = std::make_unique<EffectBank>(device, textures_->White());
    shapes_ = std::make_unique<Shapes>(device);
    sprites_ = std::make_unique<SpriteBatch>(device);
    font_ = FontLoader::Load(getContentProperty(), services_.contentRoot, "ui");
    titleFont_ = FontLoader::Load(getContentProperty(), services_.contentRoot, "title");
    signs_ = std::make_unique<SignFactory>(device, *sprites_, font_.get(), titleFont_.get(), *textures_);
    audio_ = std::make_unique<AudioSystem>(getContentProperty(), options_.audio, options_.volume);

    services_.textures = textures_.get();
    services_.models = models_.get();
    services_.effects = effects_.get();
    services_.shapes = shapes_.get();
    services_.sprites = sprites_.get();
    services_.font = font_.get();
    services_.titleFont = titleFont_.get();
    services_.signs = signs_.get();
    services_.audio = audio_.get();
    services_.stats = &stats_;
    services_.mutableStats = &stats_;
    services_.graphics = &graphics_;
    services_.whiteTexture = textures_->White();

    sky_ = std::make_unique<SkyDome>(services_, timeOfDay_);
    services_.sky = sky_.get();
    services_.timeOfDay = &timeOfDay_;
    museum_ = &TheMuseum();
    services_.museum = museum_;
    for (const std::string& problem : ValidateMuseum(*museum_)) Museum::Log("catalogue", "INVALID: %s", problem.c_str());
    world_ = std::make_unique<MuseumWorld>(services_, *museum_);
    hud_ = std::make_unique<Hud>(services_);
    services_.travelToFloor = [this](int floor) { StartRide(floor); };
    services_.openDirectory = [this] { OpenDirectory(); };
    directory_ = std::make_unique<DirectoryScreen>(services_);

    PlayerController::Settings settings;
    settings.mouseSensitivity = options_.mouseSensitivity;
    player_ = std::make_unique<PlayerController>(settings);
    player_->onFootstep = [this] {
        if (audio_) audio_->PlayUi("step", 0.35f, static_cast<float>((frame_ % 5) - 2) * 0.04f);
    };

    progress_ = std::make_unique<ProgressStore>(options_.save);
    if (!options_.fresh) discovered_ = progress_->Load(*museum_);

    // Start in the entrance lobby unless the command line asked for somewhere else.
    ResetToEntrance();
    if (options_.startRoom) {
        if (const RoomDef* r = museum_->FindByKey(*options_.startRoom)) TeleportToCell(r->index);
        else Museum::Log("main", "unknown room '%s'", options_.startRoom->c_str());
    }
    if (options_.view) GoToView(*options_.view);
    // Deferred: the request comes from a button inside the room we are about to leave, which must not be destroyed under its own callback.
    services_.showView = [this](const std::string& name) { pendingView_ = name; };
    services_.focusView = [this](const Vector3& eye, float yaw, float pitch) { StartGlide(eye, yaw, pitch); };
    if (options_.startPosition) {
        camera_.SetPosition(*options_.startPosition);
        world_->Arrive(std::max(0, museum_->CellAt(*options_.startPosition)));
    }
    if (options_.startYawDegrees || options_.startPitchDegrees) {
        camera_.SetPose(camera_.Position(), MathHelper::ToRadians(options_.startYawDegrees.value_or(MathHelper::ToDegrees(camera_.Yaw()))),
                        MathHelper::ToRadians(options_.startPitchDegrees.value_or(0.0f)));
    }

    // The ambience plays everywhere, quietly; footsteps and exhibits add their own sounds.
    if (Voice3D* ambience = audio_->CreateLoop("ambience", Vector3(0, 0, 0), 0.18f, 1e6f)) audio_->SetPlaying(ambience, true);

    if (options_.startHours) timeOfDay_.SetHours(*options_.startHours);
    timeRuns_ = options_.timeRuns;

    SetPaused(false);
    hud_->Toast("Welcome! Press F1 for controls.", 5.0f);

    if (!options_.script.empty()) {
        std::stringstream ss(options_.script);
        std::string step;
        while (std::getline(ss, step, ';')) scriptSteps_.push_back(step);
    }
}

void MuseumGame::UnloadContent()
{
    if (progress_ && progressDirty_) {
        progress_->Save(discovered_, *museum_);
        progressDirty_ = false;
    }
    if (player_ && player_->MouseCaptured()) player_->SetMouseCaptured(false, 0, 0);
    // Release in reverse dependency order; GPU resources must go before the device does.
    directory_.reset();
    hud_.reset();
    world_.reset();
    sky_.reset();
    audio_.reset();
    signs_.reset();
    titleFont_.reset();
    font_.reset();
    sprites_.reset();
    shapes_.reset();
    effects_.reset();
    models_.reset();
    textures_.reset();
}

void MuseumGame::TeleportToCell(int cell)
{
    const Spawn spawn = museum_->SpawnPoint(cell);
    camera_.SetPose(spawn.eye, spawn.yaw, 0.0f);
    world_->Arrive(cell);
}

void MuseumGame::StartGlide(const Vector3& eye, float yaw, float pitch)
{
    if (ride_.active || glide_.active) return;
    glide_ = Glide{true, camera_.Position(), eye, camera_.Yaw(), yaw, camera_.Pitch(), pitch, 0.0f};
    // Turn the short way round.
    while (glide_.yawTo - glide_.yawFrom > MathHelper::Pi) glide_.yawTo -= MathHelper::TwoPi;
    while (glide_.yawTo - glide_.yawFrom < -MathHelper::Pi) glide_.yawTo += MathHelper::TwoPi;
}

void MuseumGame::GoToView(const std::string& name)
{
    const ReferenceView* view = FindReferenceView(name);
    const RoomDef* room = view != nullptr ? museum_->FindByKey(view->room) : nullptr;
    if (room == nullptr) {
        Museum::Log("main", "unknown reference view '%s'", name.c_str());
        return;
    }
    TeleportToCell(room->index);
    camera_.SetPose(camera_.Position() + camera_.GroundForward() * view->advance, camera_.Yaw(), 0.0f);
    world_->Arrive(room->index);
}

void MuseumGame::ResetToEntrance()
{
    // The lobby is the ground-floor hub; the visitor arrives from the street, facing into the building.
    const RoomDef& lobby = museum_->Room(museum_->floors.front().hub);
    camera_.SetPose(lobby.ToWorld(0.0f, 1.65f, lobby.halfD * 0.55f), 0.0f, 0.0f);
    world_->Arrive(lobby.index);
}

// Riding an elevator: fade out, swap the visitor into the target floor's elevator car while the screen is
// black, fade back in. The cars sit at the same x/z on every floor, so the swap is invisible.
namespace {
constexpr float kRideSwapAt = 1.1f;
constexpr float kRideLength = 2.3f;
}

void MuseumGame::StartRide(int floor)
{
    if (ride_.active || floor < 0 || floor >= static_cast<int>(museum_->floors.size())) return;
    ride_ = {true, false, floor, 0.0f};
    audio_->PlayUi("chirp", 0.5f, -0.4f);
}

void MuseumGame::UpdateRide(float dt)
{
    ride_.time += dt;
    if (!ride_.swapped && ride_.time >= kRideSwapAt) {
        ride_.swapped = true;
        TeleportToCell(museum_->floors[static_cast<std::size_t>(ride_.floor)].elevator);
        audio_->PlayUi("arpeggio", 0.35f, 0.3f);
        hud_->Toast("Floor " + std::to_string(ride_.floor) + ": " + museum_->floors[static_cast<std::size_t>(ride_.floor)].name, 3.5f);
    }
    if (ride_.time >= kRideLength) ride_.active = false;
}

void MuseumGame::SetPaused(bool paused)
{
    paused_ = paused;
    ApplyMouseMode();
}

// The pointer is captured for looking around, and free whenever a menu (pause, directory) is up.
void MuseumGame::ApplyMouseMode()
{
    const bool menu = paused_ || (directory_ && directory_->IsOpen());
    setIsMouseVisibleProperty(menu);
    if (player_) {
        const auto& vp = getGraphicsDeviceProperty().getViewportProperty();
        player_->SetMouseCaptured(!menu, vp.getWidthProperty(), vp.getHeightProperty());
    }
}

void MuseumGame::OpenDirectory()
{
    if (!directory_ || paused_ || ride_.active) return;
    directory_->Open(museum_->FloorAt(camera_.Position().Y), shownCell_);
    ApplyMouseMode();
}

void MuseumGame::HandleSystemKeys()
{
    // A text-entry exhibit owns the keyboard until Escape: no hotkeys, no walking.
    player_->SetKeyboardBlocked(stats_.typingCapture);
    if (stats_.typingCapture) {
        if (input_.Pressed(Keys::Escape)) stats_.typingCapture = false;
        return;
    }
    if (input_.Pressed(Keys::Escape) || input_.PadPressed(&GamePadButtons::getStartProperty)) SetPaused(!paused_);
    if (input_.Pressed(Keys::F1) || input_.Pressed(Keys::H)) showHelp_ = !showHelp_;
    if (input_.Pressed(Keys::F3)) showStats_ = !showStats_;
    if ((input_.Pressed(Keys::M) || input_.Pressed(Keys::Tab)) && !paused_ && !directory_->IsOpen()) OpenDirectory();
    if (input_.Pressed(Keys::N) && !paused_ && !directory_->IsOpen()) showMap_ = !showMap_;
    // The museum clock: T moves it an hour on (Shift: back), Y lets the day run or stops it.
    if (input_.Pressed(Keys::T) && !paused_) {
        const bool back = input_.Down(Keys::LeftShift) || input_.Down(Keys::RightShift);
        timeOfDay_.Advance(back ? -1.0f : 1.0f);
        char buf[48];
        std::snprintf(buf, sizeof buf, "Museum time %02d:%02d", static_cast<int>(timeOfDay_.Hours()) % 24, static_cast<int>(timeOfDay_.Hours() * 60.0f) % 60);
        hud_->Toast(buf, 1.5f);
    }
    if (input_.Pressed(Keys::Y) && !paused_) {
        timeRuns_ = !timeRuns_;
        hud_->Toast(timeRuns_ ? "The museum day is running" : "The museum clock is stopped", 1.5f);
    }
    if (input_.Pressed(Keys::F2)) audio_->SetMuted(!audio_->Muted());
    if (input_.Pressed(Keys::F11)) {
        graphics_.ToggleFullScreen();
        const auto& vp = getGraphicsDeviceProperty().getViewportProperty();
        if (player_->MouseCaptured()) {
            player_->SetMouseCaptured(false, 0, 0);
            player_->SetMouseCaptured(true, vp.getWidthProperty(), vp.getHeightProperty());
        }
    }
    if (input_.Pressed(Keys::R)) {
        ResetToEntrance();
        hud_->Toast("Back at the entrance");
        if (paused_) SetPaused(false);
    }
    if (input_.Pressed(Keys::OemCloseBrackets)) sensitivityScale_ = std::min(4.0f, sensitivityScale_ * 1.15f);
    if (input_.Pressed(Keys::OemOpenBrackets)) sensitivityScale_ = std::max(0.2f, sensitivityScale_ / 1.15f);
    if (input_.Pressed(Keys::OemPlus) || input_.Pressed(Keys::Add)) audio_->SetMasterVolume(audio_->MasterVolume() + 0.1f);
    if (input_.Pressed(Keys::OemMinus) || input_.Pressed(Keys::Subtract)) audio_->SetMasterVolume(audio_->MasterVolume() - 0.1f);
    player_->GetSettings().mouseSensitivity = options_.mouseSensitivity * sensitivityScale_;

    if (paused_) {
        if (input_.Pressed(Keys::Q)) Exit();
        if (input_.LeftClicked()) SetPaused(false);
    }
}

void MuseumGame::RecordLoopStats(const GameTime& gameTime)
{
    ++stats_.updates;
    ++updatesThisSecond_;
    stats_.totalSeconds = gameTime.getTotalGameTimeProperty().getTotalSecondsProperty();
    stats_.elapsedMilliseconds = static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalMillisecondsProperty());
    stats_.runningSlowly = gameTime.getIsRunningSlowlyProperty();
    stats_.fixedStep = getIsFixedTimeStepProperty();
    stats_.targetMilliseconds = static_cast<float>(getTargetElapsedTimeProperty().getTotalMillisecondsProperty());
    std::memmove(stats_.frameHistory, stats_.frameHistory + 1, sizeof(stats_.frameHistory) - sizeof(float));
    stats_.frameHistory[119] = stats_.elapsedMilliseconds;
    if (stats_.hitchFrames > 0) --stats_.hitchFrames;
    const float delay = stats_.hitchFrames > 0 ? std::max(stats_.injectedDelayMs, 40.0f) : stats_.injectedDelayMs;
    if (delay > 0.0f) {   // a deliberate slow Update, for the timing labs
        const auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(static_cast<long long>(delay * 1000.0f));
        while (std::chrono::steady_clock::now() < until) {}
    }
    if (world_) {
        stats_.cellsBuilt = world_->Stats().cellsBuilt;
        stats_.cellsLoaded = world_->Stats().cellsLoaded;
        stats_.cellsBuiltTotal = world_->Stats().cellsBuiltTotal;
        stats_.cellsDestroyedTotal = world_->Stats().cellsDestroyedTotal;
    }
}

void MuseumGame::UpdateFrameStats(float dt)
{
    fpsAccumulator_ += dt;
    ++fpsFrames_;
    if (fpsAccumulator_ >= 0.5f) {
        stats_.updatesPerSecond = static_cast<float>(updatesThisSecond_) / fpsAccumulator_;
        stats_.drawsPerSecond = static_cast<float>(drawsThisSecond_) / fpsAccumulator_;
        updatesThisSecond_ = drawsThisSecond_ = 0;
        stats_.fps = static_cast<float>(fpsFrames_) / fpsAccumulator_;
        stats_.frameMilliseconds = fpsAccumulator_ / static_cast<float>(fpsFrames_) * 1000.0f;
        fpsAccumulator_ = 0.0f;
        fpsFrames_ = 0;
    }
    const auto& vp = getGraphicsDeviceProperty().getViewportProperty();
    stats_.backBufferWidth = vp.getWidthProperty();
    stats_.backBufferHeight = vp.getHeightProperty();
    stats_.fullscreen = graphics_.getIsFullScreenProperty();
    stats_.frame = frame_;
}

void MuseumGame::Update(GameTime& gameTime)
{
    if (!world_) {
        if (++frame_ > 2) Exit();
        return;
    }
    Game::Update(gameTime);   // GameComponents (the application-model rooms register some)
    float dt = static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalMillisecondsProperty()) / 1000.0f;
    dt = std::clamp(dt, 0.0f, kMaxFrameSeconds);
    if (options_.fixedTimeStep > 0.0f) dt = options_.fixedTimeStep;
    ++frame_;
    RecordLoopStats(gameTime);

    if (!pendingView_.empty()) {
        const std::string view = std::move(pendingView_);
        pendingView_.clear();
        GoToView(view);
    }
    input_.Update();
    UpdateFrameStats(dt);
    HandleSystemKeys();

    // Losing the window focus releases the mouse and pauses, so alt-tabbing never traps the pointer.
    const bool active = getIsActiveProperty();
    if (wasActive_ && !active && !paused_ && options_.maxFrames == 0) SetPaused(true);
    wasActive_ = active;

    RunScript();

    if (directory_->IsOpen()) {
        const auto& vp = getGraphicsDeviceProperty().getViewportProperty();
        const DirectoryScreen::Result r = directory_->Update(input_, discovered_, vp.getWidthProperty(), vp.getHeightProperty());
        if (r.close) {
            directory_->Close();
            ApplyMouseMode();
        }
        if (r.travelTo) TeleportToCell(*r.travelTo);
        audio_->Update();
        hud_->Update(dt);
        if (options_.maxFrames > 0 && frame_ >= options_.maxFrames) Exit();
        return;
    }

    if (ride_.active) UpdateRide(dt);
    sinceSave_ += dt;
    if (progressDirty_ && sinceSave_ > 5.0f && progress_) {
        progress_->Save(discovered_, *museum_);
        progressDirty_ = false;
        sinceSave_ = 0.0f;
    }
    if (timeRuns_ && !paused_) timeOfDay_.Advance(dt * 0.2f);   // 12 museum minutes per second
    if (!paused_ && glide_.active) {
        time_ += dt;
        glide_.time += dt;
        const float t = std::min(1.0f, glide_.time / 0.45f);
        const float e = t * t * (3.0f - 2.0f * t);
        camera_.SetPose(Vector3::Lerp(glide_.from, glide_.to, e), MathHelper::Lerp(glide_.yawFrom, glide_.yawTo, e), MathHelper::Lerp(glide_.pitchFrom, glide_.pitchTo, e));
        if (t >= 1.0f) glide_.active = false;
        world_->Update(dt, time_, camera_.Position(), camera_.Forward());
    } else if (!paused_ && ride_.active) {
        time_ += dt;
        world_->Update(dt, time_, camera_.Position(), camera_.Forward());
    } else if (!paused_) {
        time_ += dt;
        player_->SetBaseY(museum_->floors[static_cast<std::size_t>(museum_->FloorAt(camera_.Position().Y))].baseY);
        player_->Update(dt, camera_, world_->Collision(), input_, true);
        const Vector3 forward = camera_.Forward();
        world_->Update(dt, time_, camera_.Position(), forward);

        const int cell = world_->CurrentCell();
        if (cell != shownCell_) {
            shownCell_ = cell;
            if (cell >= 0) {
                bannerAge_ = 0.0f;
                if (discovered_.insert(cell).second) progressDirty_ = true;
            }
        }
        bannerAge_ += dt;

        InteractionSystem* interaction = world_->CurrentInteraction();
        if (interaction != nullptr) {
            const bool held = input_.LeftDown();
            interaction->Focus(camera_.ViewRay(), world_->Collision(), held);
            focusPrompt_ = interaction->FocusedPrompt();
            InteractEvent event;
            event.hit = interaction->LastHit();
            event.pressed = (!stats_.typingCapture && input_.Pressed(Keys::E)) || input_.LeftClicked() || input_.PadPressed(&GamePadButtons::getAProperty);
            event.held = held || (!stats_.typingCapture && input_.Down(Keys::E));
            event.wheel = input_.WheelDelta();
            event.alternate = (!stats_.typingCapture && input_.Pressed(Keys::Q)) || input_.RightClicked();
            if (event.pressed || event.held || event.wheel != 0.0f || event.alternate) {
                if (interaction->Dispatch(event) && event.pressed) audio_->PlayUi("click", 0.6f);
            }
        } else {
            focusPrompt_.clear();
        }
        audio_->SetListener(camera_.Position(), forward);
        stats_.activeVoices = audio_->ActiveVoiceCount();
    }
    audio_->Update();
    hud_->Update(dt);
    stats_.roomsDrawn = world_->Stats().cellsDrawn;

    if (options_.maxFrames > 0 && frame_ >= options_.maxFrames) Exit();
}

void MuseumGame::Draw(const GameTime& gameTime)
{
    ++stats_.draws;
    ++drawsThisSecond_;
    GraphicsDevice& device = getGraphicsDeviceProperty();
    if (!world_) {
        device.Clear(Color::DarkSlateGray);
        return;
    }
    const auto& vp = device.getViewportProperty();
    const float aspect = static_cast<float>(vp.getWidthProperty()) / static_cast<float>(std::max(1, vp.getHeightProperty()));
    camera_.SetLens(kFov, aspect, kNearPlane, kFarPlane);

    const SceneView view(camera_.View(), camera_.Projection(), camera_.Position());
    services_.mainView = &view;
    DrawContext ctx{device, *effects_, view, time_, false};
    effects_->SetCamera(view.view, view.projection);

    // Render-to-texture passes first: they change the render target and must finish before the back buffer is cleared.
    world_->RenderOffscreen(ctx);
    device.SetRenderTarget(nullptr);

    device.Clear(Color::Black, 1.0f);
    RenderStates::SetOpaque3D(device);
    sky_->Draw(ctx);
    world_->Draw(ctx);
    world_->DrawOverlay();

    HudState hud;
    hud.prompt = paused_ ? std::string() : focusPrompt_;
    hud.paused = paused_;
    hud.showHelp = showHelp_;
    hud.showStats = showStats_;
    hud.showMap = showMap_;
    hud.audioMuted = audio_->Muted();
    hud.volume = audio_->MasterVolume();
    hud.mouseSensitivityScale = sensitivityScale_;
    hud.playerPosition = camera_.Position();
    hud.playerYaw = camera_.Yaw();
    hud.rendererName = rendererName_;
    if (ride_.active) {
        const float t = ride_.time;
        hud.fade = std::clamp(std::min(t / 0.5f, (kRideLength - t) / 0.5f), 0.0f, 1.0f);
        if (t > 0.5f && t < kRideLength - 0.5f)
            hud.fadeText = "Floor " + std::to_string(ride_.floor) + "  " + museum_->floors[static_cast<std::size_t>(ride_.floor)].name;
    }
    hud.cell = shownCell_;
    hud.floor = museum_->FloorAt(camera_.Position().Y);
    hud.discovered = &discovered_;
    if (shownCell_ >= 0) {
        const RoomDef& r = museum_->Room(shownCell_);
        hud.roomTitle = r.title;
        hud.roomSubtitle = r.subtitle;
        hud.bannerAlpha = bannerAge_ < 4.0f ? std::clamp(std::min(bannerAge_ * 3.0f, (4.0f - bannerAge_) * 1.5f), 0.0f, 1.0f) : 0.0f;
    }
    Game::Draw(gameTime);     // DrawableGameComponents draw over the scene
    if (options_.hud) hud_->Draw(hud);
    services_.mainView = nullptr;
    if (directory_->IsOpen()) {
        directory_->Draw(discovered_, hud.floor, shownCell_, camera_.Position(), camera_.Yaw(), vp.getWidthProperty(), vp.getHeightProperty());
    }

    if (!pendingShot_.empty()) {
        SaveScreenshot(pendingShot_);
        pendingShot_.clear();
    }
    if (!options_.screenshotPath.empty() && !screenshotDone_ && frame_ >= options_.screenshotFrame) {
        SaveScreenshot(options_.screenshotPath);
        screenshotDone_ = true;
        Exit();
    }
}

void MuseumGame::SaveScreenshot(const std::string& path)
{
    GraphicsDevice& device = getGraphicsDeviceProperty();
    const PresentationParameters& pp = device.getPresentationParametersProperty();
    const int w = pp.getBackBufferWidthProperty();
    const int h = pp.getBackBufferHeightProperty();
    std::vector<Color> pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    try {
        device.GetBackBufferData(pixels.data(), static_cast<int>(pixels.size()));
        if (ImageUtil::WritePng(path.c_str(), pixels.data(), w, h)) Museum::Log("screenshot", "wrote %s (%dx%d)", path.c_str(), w, h);
        else Museum::Log("screenshot", "cannot write %s", path.c_str());
    } catch (const std::exception& e) {
        Museum::Log("screenshot", "back buffer read failed: %s", e.what());
    }
}

// Test scripting (--script "a;b;c"). Steps run one per frame unless they wait:
//   wait:N          skip N frames            goto:ROOM       teleport to a room's spawn point
//   pos:x,y,z       set the eye position     look:yaw,pitch  set the view (degrees)
//   at:x,z,yaw,pitch  stand at a cell-local spot    floor:N   teleport to a floor's hall    ride:N   take the elevator
//   directory:open|close   open the directory screen    wait:N frames
//   walk:F,S,N      walk with forward F / strafe S input for N frames (collision applies)
//   e               press E on whatever is under the crosshair
//   use:I           activate interactable I directly (index in registration order)
//   pressall        operate every control of the current room through its whole range (smoke test)
//   time:H          set the museum clock to hour H (0..24)
//   press:LABEL[=V] operate the control-panel row whose label contains LABEL (slider value V)
//   shot:FILE       save a screenshot        quit            exit
void MuseumGame::RunScript()
{
    if (scriptWait_ > 0) {
        --scriptWait_;
        return;
    }
    while (scriptIndex_ < scriptSteps_.size()) {
        const std::string step = scriptSteps_[scriptIndex_++];
        const std::size_t colon = step.find(':');
        const std::string op = step.substr(0, colon);
        const std::string arg = colon == std::string::npos ? std::string() : step.substr(colon + 1);
        if (op == "wait") {
            scriptWait_ = std::atoi(arg.c_str());
            return;
        } else if (op == "goto") {
            if (const RoomDef* r = museum_->FindByKey(arg)) TeleportToCell(r->index);
            else Museum::Log("script", "goto: unknown room '%s'", arg.c_str());
        } else if (op == "floor") {
            const int f = std::atoi(arg.c_str());
            if (f >= 0 && f < static_cast<int>(museum_->floors.size())) TeleportToCell(museum_->floors[static_cast<std::size_t>(f)].hub);
        } else if (op == "stats") {
            const WorldStats& w = world_->Stats();
            Museum::Log("stats", "built %d loaded %d (lifetime built %d destroyed %d) triangles %d  textures %zu  frame %lld  fps %.0f  interactables %d + %d panel controls in %d loaded cells  visited %zu  floor %d",
                        w.cellsBuilt, w.cellsLoaded, w.cellsBuiltTotal, w.cellsDestroyedTotal, w.triangles, textures_->Count(), frame_,
                        static_cast<double>(stats_.fps), w.interactablesSeen, w.controlsSeen, w.roomsLoadedEver, discovered_.size(), museum_->FloorAt(camera_.Position().Y));
        } else if (op == "directory") {
            if (arg == "close") directory_->Close();
            else OpenDirectory();
            ApplyMouseMode();
        } else if (op == "ride") {
            StartRide(std::atoi(arg.c_str()));
        } else if (op == "pos") {
            float x = 0, y = 1.65f, z = 0;
            if (std::sscanf(arg.c_str(), "%f,%f,%f", &x, &y, &z) == 3) {
                camera_.SetPosition(Vector3(x, y, z));
                const int cell = museum_->CellAt(Vector3(x, y, z));
                if (cell >= 0) world_->Arrive(cell);
            }
        } else if (op == "at") {
            // at:x,z,yaw,pitch : stand at local (x, z) metres in the current cell, looking as given (degrees).
            float x = 0, z = 0, yaw = 0, pitch = 0;
            if (std::sscanf(arg.c_str(), "%f,%f,%f,%f", &x, &z, &yaw, &pitch) >= 3 && shownCell_ >= 0) {
                const RoomDef& r = museum_->Room(shownCell_);
                camera_.SetPose(r.ToWorld(x, 1.65f, z), MathHelper::ToRadians(yaw), MathHelper::ToRadians(pitch));
            }
        } else if (op == "time") {
            timeOfDay_.SetHours(static_cast<float>(std::atof(arg.c_str())));
        } else if (op == "look") {
            float yaw = 0, pitch = 0;
            if (std::sscanf(arg.c_str(), "%f,%f", &yaw, &pitch) == 2)
                camera_.SetPose(camera_.Position(), MathHelper::ToRadians(yaw), MathHelper::ToRadians(pitch));
        } else if (op == "walk") {
            float f = 0, s = 0;
            int n = 0;
            if (std::sscanf(arg.c_str(), "%f,%f,%d", &f, &s, &n) == 3) player_->ScriptWalk(f, s, n);
            scriptWait_ = n;
            return;
        } else if (op == "e") {
            if (InteractionSystem* interaction = world_->CurrentInteraction()) {
                interaction->Focus(camera_.ViewRay(), world_->Collision());
                const std::string prompt = interaction->FocusedPrompt();
                const bool used = interaction->Activate();
                Museum::Log("script", "e: %s (%s)", used ? prompt.c_str() : "nothing under the crosshair", used ? "activated" : "-");
            }
        } else if (op == "use") {
            const std::size_t index = static_cast<std::size_t>(std::atoi(arg.c_str()));
            if (InteractionSystem* interaction = world_->CurrentInteraction()) {
                const auto& items = interaction->Items();
                if (index < items.size() && items[index].activate) items[index].activate();
            }
        } else if (op == "press") {
            // press:<label>[=value]: operate a control-panel row of the room the visitor is in (tests)
            std::string label = arg;
            float number = 0.0f;
            const float* value = nullptr;
            if (const std::size_t eq = arg.rfind('='); eq != std::string::npos) {
                label = arg.substr(0, eq);
                number = static_cast<float>(std::atof(arg.c_str() + eq + 1));
                value = &number;
            }
            bool done = false;
            if (InteractionSystem* interaction = world_->CurrentInteraction())
                for (const auto& item : interaction->Items())
                    if (!done && item.command) done = item.command(label, value);
            Museum::Log("script", "press '%s': %s", label.c_str(), done ? "done" : "no such control");
        } else if (op == "pressall") {
            // pressall: operate every control of the current room's panels through its whole range (smoke test)
            int panels = 0;
            if (InteractionSystem* interaction = world_->CurrentInteraction())
                for (const auto& item : interaction->Items())
                    if (item.command) {
                        try {
                            if (item.command("*", nullptr)) ++panels;
                        } catch (const std::exception& e) {
                            Museum::Log("script", "pressall: exception in room %s: %s", shownCell_ >= 0 ? museum_->Room(shownCell_).key.c_str() : "?", e.what());
                        }
                    }
            Museum::Log("script", "pressall: %d panel(s) exercised", panels);
        } else if (op == "shot") {
            pendingShot_ = arg;
            scriptWait_ = 2;
            return;
        } else if (op == "quit") {
            Exit();
            return;
        }
    }
}

GetTypeNameCPP(MuseumGame, "MuseumGame")
