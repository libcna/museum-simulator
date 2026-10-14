// Video laboratory: Microsoft.Xna.Framework.Media.Video and VideoPlayer. CNA decodes the file with FFmpeg, hands each frame
// to the game as a Texture2D (VideoPlayer.GetTexture) and plays the soundtrack through its own mixer. The game draws the
// texture however it likes: here it is one SpriteBatch sprite on the room's screen.
// Without FFmpeg (-DMUSEUM_VIDEO=OFF, or no FFmpeg libraries installed) the room says so.
#include <algorithm>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"

#ifdef CNA_VIDEO_AVAILABLE
#include "Microsoft/Xna/Framework/Media/MediaState.hpp"
#include "Microsoft/Xna/Framework/Media/Video/Video.hpp"
#include "Microsoft/Xna/Framework/Media/Video/VideoPlayer.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

class VideoLab final : public ScreenExhibit {
public:
    VideoLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Video", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef CNA_VIDEO_AVAILABLE
        p.AddButton("VideoPlayer.Play(video)", [this] { Start(); });
        p.AddButton("VideoPlayer.Pause", [this] { if (player_) player_->Pause(); });
        p.AddButton("VideoPlayer.Resume", [this] { if (player_) player_->Resume(); });
        p.AddButton("VideoPlayer.Stop", [this] { if (player_) player_->Stop(); });
        p.AddToggle("VideoPlayer.IsLooped", true, [this](bool v) { looped_ = v; if (player_) player_->setIsLoopedProperty(v); });
        p.AddToggle("VideoPlayer.IsMuted", false, [this](bool v) { if (player_) player_->setIsMutedProperty(v); });
        p.AddSlider("VideoPlayer.Volume", 0.0f, 1.0f, 0.6f, [this](float v) { volume_ = v; if (player_) player_->setVolumeProperty(v); });
        p.AddReadout("State", [this] { return state_; });
        p.AddReadout("PlayPosition / Duration", [this] { return position_; });
        p.AddReadout("Video", [this] { return info_; });
        p.AddReadout("Frames delivered", [this] { return std::to_string(frames_); });
#else
        p.AddReadout("Video playback", [] { return std::string("needs FFmpeg: not built in"); });
#endif
        AddInfoSign(b, "Video",
                    {"VideoPlayer.Play starts decoding; every frame arrives as a Texture2D from GetTexture, and the soundtrack plays through CNA's mixer. The game decides how the frame is drawn:",
                     "on a quad, in a menu, as a texture on a model. Here it is a plain sprite. The clip is a zoom into the Mandelbrot set written by FFmpeg (tools/gen_video.sh)."});
    }

    void OnScreenLoaded() override
    {
#ifdef CNA_VIDEO_AVAILABLE
        try {
            player_ = std::make_unique<Media::VideoPlayer>();
            video_ = std::make_unique<Media::Video>(S().contentRoot + "/Video/zoom.webm", S().device);
            player_->setIsLoopedProperty(true);
            player_->setVolumeProperty(volume_);
        } catch (const std::exception& e) {
            error_ = e.what();
            Museum::Log("video", "%s", e.what());
        }
#endif
    }

    void Update(const UpdateContext& ctx) override
    {
#ifdef CNA_VIDEO_AVAILABLE
        if (!player_) return;
        if (ctx.nearby && !started_ && error_.empty()) {
            Start();
            started_ = true;
        }
        if (!ctx.nearby && player_->getStateProperty() == Media::MediaState::Playing) {
            player_->Pause();
            pausedByLeaving_ = true;
        } else if (ctx.nearby && pausedByLeaving_) {
            player_->Resume();
            pausedByLeaving_ = false;
        }
        try {
            switch (player_->getStateProperty()) {
                case Media::MediaState::Playing: state_ = "Playing"; break;
                case Media::MediaState::Paused: state_ = "Paused"; break;
                default: state_ = "Stopped"; break;
            }
            if (video_) {
                char buf[96];
                std::snprintf(buf, sizeof buf, "%.1f / %.1f s", player_->getPlayPositionProperty().getTotalSecondsProperty(), video_->getDurationProperty().getTotalSecondsProperty());
                position_ = buf;
                std::snprintf(buf, sizeof buf, "%d x %d, %.0f fps", video_->getWidthProperty(), video_->getHeightProperty(), static_cast<double>(video_->getFramesPerSecondProperty()));
                info_ = buf;
            }
            frames_ = static_cast<long long>(player_->GetFrameGenerationEXT());
            frame_ = player_->getStateProperty() == Media::MediaState::Stopped ? nullptr : player_->GetTexture();
        } catch (const std::exception& e) {
            error_ = e.what();
        }
#else
        (void)ctx;
#endif
    }

    void OnLeave() override
    {
#ifdef CNA_VIDEO_AVAILABLE
        if (player_) player_->Stop();
        started_ = false;
        pausedByLeaving_ = false;
        frame_ = nullptr;
#endif
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        sb.Draw(white, Rectangle(0, 0, 960, 540), Color(10, 14, 22, 255));
        sb.End();
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        sb.DrawString(*S().font, "VideoPlayer", Vector2(24, 12), Kit::kGold, 0.0f, Vector2(0, 0), 1.0f, SpriteEffects::None, 0.0f);
#ifdef CNA_VIDEO_AVAILABLE
        if (frame_ != nullptr && !frame_->getIsDisposedProperty()) {
            sb.Draw(*frame_, Rectangle(160, 64, 640, 360), Color::White);
        } else {
            sb.DrawString(*S().font, error_.empty() ? "No frame yet (press Play)" : error_, Vector2(180, 220), Kit::Rgba(255, 190, 150), 0.0f, Vector2(0, 0), 0.7f, SpriteEffects::None, 0.0f);
        }
        sb.DrawString(*S().font, state_ + "     " + position_ + "     " + info_ + "     frames " + std::to_string(frames_), Vector2(160, 440), Kit::kText, 0.0f, Vector2(0, 0), 0.65f, SpriteEffects::None, 0.0f);
#else
        sb.DrawString(*S().font, "This museum was built without FFmpeg video playback (-DMUSEUM_VIDEO=OFF or no FFmpeg libraries).", Vector2(24, 90), Kit::Rgba(255, 190, 150), 0.0f, Vector2(0, 0), 0.65f, SpriteEffects::None, 0.0f);
#endif
        sb.End();
    }

private:
#ifdef CNA_VIDEO_AVAILABLE
    void Start()
    {
        try {
            if (player_ && video_) player_->Play(video_.get());
        } catch (const std::exception& e) {
            error_ = e.what();
        }
    }

    // The player goes first: it points at the video it is playing.
    std::unique_ptr<Media::Video> video_;
    std::unique_ptr<Media::VideoPlayer> player_;
    Texture2D* frame_ = nullptr;
    std::string state_ = "-", position_ = "-", info_ = "-", error_;
    long long frames_ = 0;
    float volume_ = 0.6f;
    bool looped_ = true, started_ = false, pausedByLeaving_ = false;
#endif
};

}  // namespace

std::unique_ptr<Exhibit> MakeVideoLab(const RoomDef& room, Services& services)
{
    return std::make_unique<VideoLab>(room, services);
}
