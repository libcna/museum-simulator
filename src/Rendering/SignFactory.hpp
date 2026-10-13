#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Rendering/TextureLibrary.hpp"

struct SignLine {
    std::string text;
    bool large = false;
    Microsoft::Xna::Framework::Color color = Microsoft::Xna::Framework::Color::White;
};

struct SignSpec {
    std::vector<SignLine> lines;
    int pixelWidth = 512;
    int pixelHeight = 128;
    Microsoft::Xna::Framework::Color background{24, 28, 40, 255};
    Microsoft::Xna::Framework::Color border{200, 170, 90, 255};
    bool leftAlign = false;
    float textScale = 1.0f;   // upper bound; text still shrinks to fit the sign
};

/**
 * Renders text into small textures once, at build time: SpriteBatch + SpriteFont draw
 * into a RenderTarget2D, which is read back into an ordinary mipmapped Texture2D. The
 * result is registered with the TextureLibrary, so signs are drawn like any other
 * textured surface and survive a lost render-target.
 */
class SignFactory {
public:
    SignFactory(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                Microsoft::Xna::Framework::Graphics::SpriteBatch& sprites,
                Microsoft::Xna::Framework::Graphics::SpriteFont* font,
                Microsoft::Xna::Framework::Graphics::SpriteFont* titleFont, TextureLibrary& textures);

    /** Renders the sign and returns the name it is registered under. */
    std::string Create(const SignSpec& spec);

    /**
     * Renders arbitrary 2D content: `draw` runs between SpriteBatch.Begin/End (non-premultiplied
     * alpha, linear clamp) on a w x h render target cleared to `background`.
     */
    std::string CreateCustom(int width, int height, const Microsoft::Xna::Framework::Color& background,
                             const std::function<void(Microsoft::Xna::Framework::Graphics::SpriteBatch&, int, int)>& draw);

private:
    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    Microsoft::Xna::Framework::Graphics::SpriteBatch& sprites_;
    Microsoft::Xna::Framework::Graphics::SpriteFont* font_;
    Microsoft::Xna::Framework::Graphics::SpriteFont* titleFont_;
    TextureLibrary& textures_;
    int counter_ = 0;
};
