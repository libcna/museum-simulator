#pragma once

#include <string>

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

class AudioSystem;
class EffectBank;
class SignFactory;
class TextureLibrary;
class ModelLibrary;
class Shapes;
class SkyDome;
class TimeOfDay;

/** Per-frame numbers shown on the Information Hall screens and the F3 overlay. */
struct FrameStats {
    float fps = 0.0f;
    float frameMilliseconds = 0.0f;
    long long frame = 0;
    int backBufferWidth = 0;
    int backBufferHeight = 0;
    bool fullscreen = false;
    int roomsDrawn = 0;
    int activeVoices = 0;
};

/** Everything an exhibit may use. Pointers are owned by MuseumGame and outlive every exhibit. */
struct Services {
    Microsoft::Xna::Framework::Game* game = nullptr;
    Microsoft::Xna::Framework::Graphics::GraphicsDevice* device = nullptr;
    Microsoft::Xna::Framework::Content::ContentManager* content = nullptr;
    TextureLibrary* textures = nullptr;
    ModelLibrary* models = nullptr;
    EffectBank* effects = nullptr;
    Shapes* shapes = nullptr;
    SkyDome* sky = nullptr;
    TimeOfDay* timeOfDay = nullptr;
    SignFactory* signs = nullptr;
    AudioSystem* audio = nullptr;
    Microsoft::Xna::Framework::Graphics::SpriteBatch* sprites = nullptr;
    Microsoft::Xna::Framework::Graphics::SpriteFont* font = nullptr;
    Microsoft::Xna::Framework::Graphics::SpriteFont* titleFont = nullptr;
    const FrameStats* stats = nullptr;
    std::string contentRoot;
    bool supportsThreeD = true;
    bool supportsWireframe = true;
};

/** A camera, its matrices and the frustum built from them. */
struct SceneView {
    Microsoft::Xna::Framework::Matrix view;
    Microsoft::Xna::Framework::Matrix projection;
    Microsoft::Xna::Framework::Vector3 eye;
    Microsoft::Xna::Framework::BoundingFrustum frustum;

    SceneView(const Microsoft::Xna::Framework::Matrix& v, const Microsoft::Xna::Framework::Matrix& p,
              const Microsoft::Xna::Framework::Vector3& e)
        : view(v), projection(p), eye(e), frustum(v * p)
    {
    }
};

/** Passed to every draw call. `offscreen` is true while rendering into a RenderTarget2D. */
struct DrawContext {
    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device;
    EffectBank& fx;
    const SceneView& view;
    double time;
    bool offscreen;
};
