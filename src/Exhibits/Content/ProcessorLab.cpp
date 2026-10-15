// Processor laboratory: the XNA content pipeline's TextureImporter and TextureProcessor running in the game. A picture is
// imported (or built in memory), processed with the same options a content project exposes (colour key, premultiplied alpha,
// mipmaps, power-of-two resize, DXT compression), and the resulting mip chain is uploaded and drawn. Every number shown is
// measured from the processor's output.
// Without CNA's pipeline library (-DMUSEUM_PIPELINE=OFF) the room says so.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Exhibits/ScreenExhibit.hpp"

#ifdef MUSEUM_PIPELINE
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentBuildLogger.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentImporterContext.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentProcessorContext.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/PixelBitmapContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/TextureContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/TextureProcessor.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/TextureImporter.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "System/NotSupportedException.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

#ifdef MUSEUM_PIPELINE
namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace PipeGfx = Microsoft::Xna::Framework::Content::Pipeline::Graphics;
namespace Processors = Microsoft::Xna::Framework::Content::Pipeline::Processors;
using PipelineSupport::ImporterContext;
using PipelineSupport::ProcessorContext;

std::shared_ptr<PipeGfx::MipmapChain> FirstFace(PipeGfx::TextureContent& texture)
{
    const auto& faces = static_cast<const System::Collections::ObjectModel::Collection<std::shared_ptr<PipeGfx::MipmapChain>>&>(texture.getFacesProperty());
    return faces[0];
}

// A 48 x 40 picture that is not a power of two, on a magenta background, with a disc whose alpha falls off towards the edge.
std::shared_ptr<PipeGfx::TextureContent> BuildSource()
{
    auto bitmap = std::make_shared<PipeGfx::PixelBitmapContent<Color>>(48, 40);
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 48; ++x) {
            const float dx = (static_cast<float>(x) - 23.5f) / 17.0f, dy = (static_cast<float>(y) - 19.5f) / 17.0f;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > 1.0f) {
                bitmap->SetPixel(x, y, Color(255, 0, 255, 255));
            } else {
                const int a = static_cast<int>(255.0f - 190.0f * d * d);
                const bool stripe = ((x + y) / 6) % 2 == 0;
                bitmap->SetPixel(x, y, stripe ? Color(255, 200, 80, a) : Color(70, 170, 255, a));
            }
        }
    }
    auto texture = std::make_shared<PipeGfx::Texture2DContent>();
    texture->getMipmapsProperty().Add(bitmap);
    return texture;
}
#endif

class ProcessorLab final : public ScreenExhibit {
public:
    ProcessorLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("TextureProcessor", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_PIPELINE
        p.AddChoice("Source", {"built in memory (magenta key, soft alpha)", "coin.png (TextureImporter)", "brick.png (TextureImporter)"}, 0, [this](int v) { source_ = v; Process(); });
        p.AddToggle("ColorKeyEnabled (magenta)", true, [this](bool v) { colorKey_ = v; Process(); });
        p.AddToggle("PremultiplyAlpha", true, [this](bool v) { premultiply_ = v; Process(); });
        p.AddToggle("GenerateMipmaps", false, [this](bool v) { mipmaps_ = v; Process(); });
        p.AddToggle("ResizeToPowerOfTwo", false, [this](bool v) { powerOfTwo_ = v; Process(); });
        p.AddChoice("TextureFormat", {"NoChange", "Color", "DxtCompressed"}, 1, [this](int v) { format_ = v; Process(); });
        p.AddReadout("Imported", [this] { return imported_; });
        p.AddReadout("Processed", [this] { return processed_; });
        p.AddReadout("Mip levels", [this] { return std::to_string(levels_.size()); });
        p.AddReadout("Pixel data", [this] { return bytes_; });
#else
        p.AddReadout("Content pipeline", [] { return std::string("not built into this museum"); });
#endif
        AddInfoSign(b, "Processing a Texture",
                    {"A content project imports a picture (TextureImporter) and runs it through a processor (TextureProcessor) whose options you set: a colour key turns one colour transparent,",
                     "premultiplying alpha makes blending cheaper, mip levels are halved copies, and DXT compression trades quality for a quarter or an eighth of the memory."});
    }

    void OnScreenLoaded() override
    {
#ifdef MUSEUM_PIPELINE
        Process();
#endif
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        Texture2D& white = *S().whiteTexture;
        const auto text = [&](const std::string& t, float x, float y, Color c, float s) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::PointClamp, nullptr, nullptr);
        text("The processed texture", 24, 12, Kit::kGold, 1.0f);
#ifdef MUSEUM_PIPELINE
        // A checkerboard shows what is transparent.
        for (int cy = 0; cy < 14; ++cy)
            for (int cx = 0; cx < 29; ++cx)
                sb.Draw(white, Rectangle(24 + cx * 32, 56 + cy * 32, 32, 32), ((cx + cy) % 2 == 0) ? Color(58, 64, 82, 255) : Color(42, 48, 64, 255));
        sb.End();
        sb.Begin(SpriteSortMode::Deferred, premultiplied_ ? &BlendState::AlphaBlend : &BlendState::NonPremultiplied, &SamplerState::PointClamp, nullptr, nullptr);
        float x = 40.0f;
        for (std::size_t i = 0; i < levels_.size(); ++i) {
            const Level& l = levels_[i];
            const float scale = 3.0f;
            const float w = static_cast<float>(l.width) * scale, h = static_cast<float>(l.height) * scale;
            if (l.texture != nullptr) sb.Draw(*l.texture, Rectangle(static_cast<int>(x), 70, static_cast<int>(w), static_cast<int>(h)), Color::White);
            else sb.Draw(white, Rectangle(static_cast<int>(x), 70, static_cast<int>(w), static_cast<int>(h)), Color(90, 40, 40, 200));
            x += w + 24.0f;
            if (x > 900.0f) break;
        }
        sb.End();
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        x = 40.0f;
        for (std::size_t i = 0; i < levels_.size(); ++i) {
            const Level& l = levels_[i];
            const float w = static_cast<float>(l.width) * 3.0f;
            text("level " + std::to_string(i), x, 70.0f + static_cast<float>(l.height) * 3.0f + 8.0f, Kit::kText, 0.55f);
            text(std::to_string(l.width) + " x " + std::to_string(l.height) + " " + l.format, x, 70.0f + static_cast<float>(l.height) * 3.0f + 28.0f, Kit::kDim, 0.5f);
            if (!l.note.empty()) text(l.note, x, 70.0f + static_cast<float>(l.height) * 3.0f + 48.0f, Kit::Rgba(255, 190, 150), 0.45f);
            x += w + 24.0f;
            if (x > 900.0f) break;
        }
        if (!error_.empty()) text(error_, 24, 500, Kit::Rgba(255, 190, 150), 0.6f);
        text(std::string("drawn with ") + (premultiplied_ ? "BlendState.AlphaBlend (premultiplied)" : "BlendState.NonPremultiplied"), 24, 504, Kit::kDim, 0.55f);
#else
        text("This museum was built without CNA's content pipeline library (-DMUSEUM_PIPELINE=OFF).", 24, 90, Kit::Rgba(255, 190, 150), 0.7f);
        (void)white;
#endif
        sb.End();
    }

private:
#ifdef MUSEUM_PIPELINE
    struct Level {
        int width = 0, height = 0;
        std::string format, note;
        std::unique_ptr<Texture2D> texture;
    };

    // Imports (or builds) the picture, runs the processor with the chosen options and uploads every level it produced.
    void Process()
    {
        try {
            levels_.clear();
            error_.clear();
            std::shared_ptr<PipeGfx::TextureContent> input;
            if (source_ == 0) {
                input = BuildSource();
            } else {
                ImporterContext context;
                Xna::TextureImporter importer;
                input = importer.Import(S().contentRoot + (source_ == 1 ? "/Textures/coin.png" : "/Textures/brick.png"), context);
            }
            const std::shared_ptr<PipeGfx::BitmapContent> firstLevel = (*FirstFace(*input))[0];
            const PipeGfx::BitmapContent& first = *firstLevel;
            imported_ = std::to_string(first.getWidthProperty()) + " x " + std::to_string(first.getHeightProperty()) + ", Color";

            Processors::TextureProcessor processor;
            processor.setColorKeyEnabledProperty(colorKey_);
            processor.setColorKeyColorProperty(Color(255, 0, 255, 255));
            processor.setPremultiplyAlphaProperty(premultiply_);
            processor.setGenerateMipmapsProperty(mipmaps_);
            processor.setResizeToPowerOfTwoProperty(powerOfTwo_);
            static constexpr Processors::TextureProcessorOutputFormat formats[] = {Processors::TextureProcessorOutputFormat::NoChange, Processors::TextureProcessorOutputFormat::Color,
                                                                                    Processors::TextureProcessorOutputFormat::DxtCompressed};
            processor.setTextureFormatProperty(formats[format_]);
            ProcessorContext context;
            const std::shared_ptr<PipeGfx::TextureContent> output = processor.Process(input, context);

            premultiplied_ = premultiply_;
            std::size_t total = 0;
            const std::shared_ptr<PipeGfx::MipmapChain> chain = FirstFace(*output);
            for (SharpRuntime::intcs i = 0; i < chain->getCountProperty(); ++i) {
                const std::shared_ptr<PipeGfx::BitmapContent> bitmapPtr = (*chain)[i];
                const PipeGfx::BitmapContent& bitmap = *bitmapPtr;
                Level level;
                level.width = bitmap.getWidthProperty();
                level.height = bitmap.getHeightProperty();
                SurfaceFormat surface{};
                const bool known = bitmap.TryGetFormat(surface);
                level.format = !known ? "?" : surface == SurfaceFormat::Color ? "Color" : surface == SurfaceFormat::Dxt1 ? "Dxt1" : surface == SurfaceFormat::Dxt3 ? "Dxt3" : surface == SurfaceFormat::Dxt5 ? "Dxt5" : "other";
                const std::vector<SharpRuntime::bytecs> data = bitmap.GetPixelData();
                total += data.size();
                try {
                    if (known) {
                        auto texture = std::make_unique<Texture2D>(*S().device, level.width, level.height, false, surface);
                        texture->SetData(reinterpret_cast<const std::uint8_t*>(data.data()), static_cast<int>(data.size()));
                        level.texture = std::move(texture);
                    }
                } catch (const std::exception& e) {
                    level.note = (level.format[0] == 'D' && (level.width < 4 || level.height < 4)) ? "under one 4x4 block" : std::string(e.what()).substr(0, 60);
                }
                levels_.push_back(std::move(level));
            }
            processed_ = std::to_string(levels_.empty() ? 0 : levels_[0].width) + " x " + std::to_string(levels_.empty() ? 0 : levels_[0].height) + ", " + (levels_.empty() ? "?" : levels_[0].format);
            const std::size_t base = static_cast<std::size_t>(first.getWidthProperty()) * static_cast<std::size_t>(first.getHeightProperty()) * 4;
            char buf[96];
            std::snprintf(buf, sizeof buf, "%zu bytes (%.0f%% of the imported level)", total, base > 0 ? 100.0 * static_cast<double>(total) / static_cast<double>(base) : 0.0);
            bytes_ = buf;
        } catch (const std::exception& e) {
            error_ = std::string("processor: ") + e.what();
            processed_ = "failed";
        }
    }

    std::vector<Level> levels_;
    int source_ = 0, format_ = 1;
    bool colorKey_ = true, premultiply_ = true, mipmaps_ = false, powerOfTwo_ = false, premultiplied_ = true;
    std::string imported_ = "-", processed_ = "-", bytes_ = "-", error_;
#endif
};

}  // namespace

std::unique_ptr<Exhibit> MakeProcessorLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ProcessorLab>(room, services);
}
