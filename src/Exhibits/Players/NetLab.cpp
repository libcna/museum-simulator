// Networking laboratory: Microsoft.Xna.Framework.Net as CNA implements it. Two rooms: PacketWriter and PacketReader (what a
// packet is, byte by byte), and a Local NetworkSession between the two signed-in profiles, with simulated latency and loss.
// Everything stays in this process: a Local session never touches the network, which is also why it works under test.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <deque>
#include <exception>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"

#ifdef MUSEUM_NETWORKING
#include "Microsoft/Xna/Framework/GamerServices/Gamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamer.hpp"
#include "Microsoft/Xna/Framework/GamerServices/SignedInGamerCollection.hpp"
#include "Microsoft/Xna/Framework/Net/GameEndedEventArgs.hpp"
#include "Microsoft/Xna/Framework/Net/GameStartedEventArgs.hpp"
#include "Microsoft/Xna/Framework/Net/GamerJoinedEventArgs.hpp"
#include "Microsoft/Xna/Framework/Net/GamerLeftEventArgs.hpp"
#include "Microsoft/Xna/Framework/Net/LocalNetworkGamer.hpp"
#include "Microsoft/Xna/Framework/Net/NetworkSession.hpp"
#include "Microsoft/Xna/Framework/Net/NetworkSessionEndedEventArgs.hpp"
#include "Microsoft/Xna/Framework/Net/PacketReader.hpp"
#include "Microsoft/Xna/Framework/Net/PacketWriter.hpp"
#include "Microsoft/Xna/Framework/Net/SendDataOptions.hpp"
#include "System/IO/MemoryStream.hpp"
#include "System/TimeSpan.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Packets, Session };

Mode ParseMode(const std::string& s) { return s == "session" ? Mode::Session : Mode::Packets; }

#ifdef MUSEUM_NETWORKING
using namespace Microsoft::Xna::Framework::Net;

const char* StateName(NetworkSessionState s)
{
    switch (s) {
        case NetworkSessionState::Lobby: return "Lobby";
        case NetworkSessionState::Playing: return "Playing";
        default: return "Ended";
    }
}
#endif

class NetLab final : public ScreenExhibit {
public:
    NetLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))) {}

    ~NetLab() override { CloseSession(); }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_NETWORKING
        if (mode_ == Mode::Packets) {
            p.AddToggle("int32 sequence number", true, [this](bool v) { fields_[0] = v; dirty_ = true; });
            p.AddToggle("bool", true, [this](bool v) { fields_[1] = v; dirty_ = true; });
            p.AddToggle("float (Single)", true, [this](bool v) { fields_[2] = v; dirty_ = true; });
            p.AddToggle("double", false, [this](bool v) { fields_[3] = v; dirty_ = true; });
            p.AddToggle("string", true, [this](bool v) { fields_[4] = v; dirty_ = true; });
            p.AddToggle("Vector3", true, [this](bool v) { fields_[5] = v; dirty_ = true; });
            p.AddToggle("Quaternion", false, [this](bool v) { fields_[6] = v; dirty_ = true; });
            p.AddToggle("Matrix", false, [this](bool v) { fields_[7] = v; dirty_ = true; });
            p.AddSlider("String length", 0.0f, 40.0f, static_cast<float>(stringLength_), [this](float v) { stringLength_ = static_cast<int>(v); dirty_ = true; }, "%.0f", 1.0f);
            p.AddReadout("Packet size", [this] { return std::to_string(size_) + " bytes"; });
            p.AddReadout("Read back", [this] { return roundTrip_; });
        } else {
            p.AddButton("NetworkSession.StartGame", [this] { NetworkSession* n = Primary(); if (n != nullptr && n->getSessionStateProperty() == NetworkSessionState::Lobby) n->StartGame(); });
            p.AddButton("NetworkSession.EndGame", [this] { NetworkSession* n = Primary(); if (n != nullptr && n->getSessionStateProperty() == NetworkSessionState::Playing) n->EndGame(); });
            p.AddToggle("Both players ready (IsReady)", false, [this](bool v) { SetReady(v); });
            p.AddReadout("SessionState", [this] { NetworkSession* n = Primary(); return n != nullptr ? std::string(StateName(n->getSessionStateProperty())) + (n->getIsHostProperty() ? ", this machine hosts" : "") : std::string("no session"); });
            p.AddReadout("IsEveryoneReady", [this] { NetworkSession* n = Primary(); return n != nullptr ? std::string(n->getIsEveryoneReadyProperty() ? "true" : "false") : std::string("-"); });
            p.AddReadout("Gamers (local / all)", [this] { NetworkSession* n = Primary(); return n != nullptr ? std::to_string(n->getLocalGamersProperty().getCountProperty()) + " / " + std::to_string(n->getAllGamersProperty().getCountProperty()) : std::string("-"); });
            p.AddToggle("Deliver packets: a SystemLink session (visible on your network)", false, [this](bool v) { SetTransport(v); });
            p.AddSlider("Packets per second", 2.0f, 60.0f, rate_, [this](float v) { rate_ = v; }, "%.0f", 1.0f);
            p.AddReadout("Transport", [this] { return transportStatus_; });
        }
#else
        p.AddReadout("Net", [] { return std::string("not built into this museum"); });
#endif
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
#ifdef MUSEUM_NETWORKING
        if (mode_ == Mode::Session) OpenLobby();
#endif
    }

    void OnLeave() override
    {
#ifdef MUSEUM_NETWORKING
        CloseSession();   // a session holds its gamers; the next visit opens a new one
#endif
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
#ifdef MUSEUM_NETWORKING
        time_ += ctx.dt;
        if (mode_ == Mode::Packets) {
            if (dirty_) BuildPacket();
        } else {
            if (lobby_ == nullptr && host_ == nullptr && !tried_ && ctx.playerInRoom) OpenLobby();
            if (lobby_ != nullptr) lobby_->Update();
            Pump(ctx.dt);
        }
#endif
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(Title(), 24, 12, Kit::kGold, 1.0f);
#ifdef MUSEUM_NETWORKING
        if (mode_ == Mode::Packets) DrawPacket(rect, text);
        else DrawSession(rect, text);
#else
        text("This museum was built without CNA's networking (-DMUSEUM_NETWORKING=OFF).", 24, 90, Kit::Rgba(255, 190, 150), 0.7f);
        (void)rect;
#endif
        sb.End();
    }

private:
    [[nodiscard]] std::string Title() const { return mode_ == Mode::Packets ? "PacketWriter and PacketReader" : "A network session on this machine"; }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        if (mode_ == Mode::Packets)
            return {"Everything a game sends is bytes. PacketWriter writes XNA's types (Vector3, Quaternion, Matrix, strings...) in a fixed order; PacketReader must read them in", "the same order. The packet below is rebuilt whenever you change a field; its size is what every send costs on the wire, before headers."};
        return {"A Local session holds both signed-in profiles as gamers and runs the lobby (StartGame, EndGame, IsReady, GamerJoined...) but delivers no packets, as in XNA. A SystemLink session delivers them,",
                "and announces itself on your network, so it stays off until you switch it on. Only one session can exist per process; SimulatedLatency and SimulatedPacketLoss act on packets that arrive from another machine."};
    }

#ifdef MUSEUM_NETWORKING
    // ---- packets ------------------------------------------------------------------------------------------------------
    void BuildPacket()
    {
        dirty_ = false;
        try {
            PacketWriter w;
            const int seq = 1234567;
            const float f = 3.14159f;
            const Vector3 v(1.5f, -2.25f, 8.0f);
            const Quaternion q = Quaternion::CreateFromYawPitchRoll(0.4f, 0.2f, -0.1f);
            const Matrix m = Matrix::CreateRotationY(0.5f) * Matrix::CreateTranslation(1, 2, 3);
            const std::string str(static_cast<std::size_t>(stringLength_), 'x');
            if (fields_[0]) w.Write(seq);
            if (fields_[1]) w.Write(true);
            if (fields_[2]) w.Write(f);
            if (fields_[3]) w.Write(2.718281828459045);
            if (fields_[4]) w.Write(str);
            if (fields_[5]) w.Write(v);
            if (fields_[6]) w.Write(q);
            if (fields_[7]) w.Write(m);
            auto* memory = dynamic_cast<System::IO::MemoryStream*>(w.getBaseStreamProperty());
            bytes_ = memory != nullptr ? memory->ToArray() : std::vector<SharpRuntime::bytecs>{};
            size_ = static_cast<int>(bytes_.size());
            // The reader gets the same bytes (what LocalNetworkGamer.ReceiveData hands it) and reads the fields back in order.
            PacketReader r;
            r.setPositionProperty(0);
            r.getBaseStreamProperty()->Write(bytes_.data(), 0, size_);
            r.setPositionProperty(0);
            bool ok = true;
            if (fields_[0]) ok = ok && r.ReadInt32() == seq;
            if (fields_[1]) ok = ok && r.ReadBoolean();
            if (fields_[2]) ok = ok && r.ReadSingle() == f;
            if (fields_[3]) ok = ok && r.ReadDouble() == 2.718281828459045;
            if (fields_[4]) ok = ok && r.ReadString() == str;
            if (fields_[5]) { const Vector3 b = r.ReadVector3(); ok = ok && b.X == v.X && b.Y == v.Y && b.Z == v.Z; }
            if (fields_[6]) { const Quaternion b = r.ReadQuaternion(); ok = ok && b.X == q.X && b.W == q.W; }
            if (fields_[7]) { const Matrix b = r.ReadMatrix(); ok = ok && b.M11 == m.M11 && b.M43 == m.M43; }
            roundTrip_ = ok ? "every field equal, all " + std::to_string(r.getPositionProperty()) + " bytes consumed" : "MISMATCH";
        } catch (const std::exception& e) {
            roundTrip_ = std::string("error: ") + std::string(e.what()).substr(0, 40);
        }
    }

    template <typename Rect, typename Text>
    void DrawPacket(Rect& rect, Text& text)
    {
        static const char* names[8] = {"int32", "bool", "Single", "double", "string", "Vector3", "Quaternion", "Matrix"};
        static const int sizes[8] = {4, 1, 4, 8, 0, 12, 16, 64};
        text("packet: " + std::to_string(size_) + " bytes      " + roundTrip_, 24, 62, Kit::kText, 0.62f);
        float y = 100.0f;
        for (int i = 0; i < 8; ++i) {
            rect(24, y + 4, 12, 12, fields_[static_cast<std::size_t>(i)] ? Kit::Rgba(110, 230, 150) : Kit::Rgba(90, 90, 100));
            const int n = i == 4 ? stringLength_ + 1 : sizes[i];
            text(std::string(names[i]) + (fields_[static_cast<std::size_t>(i)] ? "   " + std::to_string(n) + " bytes" : "   (not written)"), 44, y - 2, fields_[static_cast<std::size_t>(i)] ? Kit::kText : Kit::kDim, 0.55f);
            y += 27.0f;
        }
        // The bytes themselves, sixteen to a row.
        text("the bytes, in hex:", 420, 100, Kit::kDim, 0.55f);
        for (std::size_t row = 0; row < 11 && row * 16 < bytes_.size(); ++row) {
            std::string line;
            char cell[8];
            for (std::size_t i = row * 16; i < std::min(bytes_.size(), row * 16 + 16); ++i) {
                std::snprintf(cell, sizeof cell, "%02X ", static_cast<unsigned>(static_cast<unsigned char>(bytes_[i])));
                line += cell;
            }
            text(line, 420, 126.0f + static_cast<float>(row) * 24.0f, Kit::Rgba(190, 235, 200), 0.5f);
        }
        if (bytes_.size() > 176) text("... " + std::to_string(bytes_.size() - 176) + " more", 420, 392, Kit::kDim, 0.5f);
        text("a Matrix costs 64 bytes: sending a whole transform every frame is expensive, a position and a yaw are not.", 24, 340, Kit::kDim, 0.5f);
    }

    // ---- session ------------------------------------------------------------------------------------------------------
    // The lobby is a Local session holding both profiles: its state machine, readiness and events are real, but a Local session
    // sends packets nowhere (as in XNA). Packets move only when the real transport is switched on: a SystemLink host for the
    // Visitor and a client for the Guest, both in this process, talking over this machine's loopback.
    void OpenLobby()
    {
        if (lobby_ != nullptr) return;
        tried_ = true;
        try {
            auto* signedIn = GamerServices::Gamer::getSignedInGamersProperty();
            if (signedIn == nullptr || signedIn->getCountProperty() == 0) {
                status_ = "nobody is signed in yet";
                tried_ = false;
                return;
            }
            lobby_ = NetworkSession::Create(NetworkSessionType::Local, 2, 8);
            started_ = lobby_->GameStarted.Add([this](System::Object*, const GameStartedEventArgs&) { Log("GameStarted"); });
            ended_ = lobby_->GameEnded.Add([this](System::Object*, const GameEndedEventArgs&) { Log("GameEnded"); });
            joined_ = lobby_->GamerJoined.Add([this](System::Object*, const GamerJoinedEventArgs&) { Log("GamerJoined"); });
            left_ = lobby_->GamerLeft.Add([this](System::Object*, const GamerLeftEventArgs&) { Log("GamerLeft"); });
            sessionEnded_ = lobby_->SessionEnded.Add([this](System::Object*, const NetworkSessionEndedEventArgs&) { Log("SessionEnded"); });
            subscribed_ = true;
            Log("Create(Local, 2 local, 8 max): " + std::to_string(lobby_->getLocalGamersProperty().getCountProperty()) + " local gamer(s)");
            status_ = "lobby open";
        } catch (const std::exception& e) {
            status_ = std::string("cannot create a session: ") + std::string(e.what()).substr(0, 60);
            Museum::Log("net", "%s", status_.c_str());
        }
    }

    static void Dispose(NetworkSession*& s)
    {
        if (s == nullptr) return;
        try { s->Dispose(); } catch (const std::exception&) {}
        delete s;
        s = nullptr;
    }

    void CloseTransport()
    {
        Dispose(host_);
        phase_ = Phase::Off;
        transportStatus_ = "off: a Local session delivers no packets";
    }

    void DisposeLobby()
    {
        if (lobby_ != nullptr) {
            try {
                if (subscribed_) {
                    lobby_->GameStarted.Remove(started_);
                    lobby_->GameEnded.Remove(ended_);
                    lobby_->GamerJoined.Remove(joined_);
                    lobby_->GamerLeft.Remove(left_);
                    lobby_->SessionEnded.Remove(sessionEnded_);
                }
            } catch (const std::exception&) {}
            Dispose(lobby_);
        }
        subscribed_ = false;
    }

    void CloseSession()
    {
        CloseTransport();
        DisposeLobby();
        tried_ = false;
        ResetStats();
    }

    void ResetStats()
    {
        sent_ = received_ = 0;
        trail_.clear();
        received_trail_.clear();
    }

    void Log(const std::string& line)
    {
        log_.push_back(line);
        while (log_.size() > 9) log_.pop_front();
    }

    void SetTransport(bool on)
    {
        if (!on) {
            CloseTransport();
            tried_ = false;
            OpenLobby();
            return;
        }
        try {
            ResetStats();
            DisposeLobby();   // the same profiles cannot be in a Local lobby and a SystemLink session at once (one session per process, as in XNA)
            host_ = NetworkSession::Create(NetworkSessionType::SystemLink, 2, 8);
            phase_ = Phase::Connected;
            transportStatus_ = "SystemLink session, both profiles local: packets flow in memory";
            Log("Create(SystemLink, 2 local, 8 max): " + std::to_string(host_->getLocalGamersProperty().getCountProperty()) + " local gamer(s)");
            if (wantReady_) SetReady(true);
        } catch (const std::exception& e) {
            CloseTransport();
            tried_ = false;
            OpenLobby();
            transportStatus_ = std::string("unavailable here: ") + std::string(e.what()).substr(0, 56);
            Museum::Log("net", "%s", transportStatus_.c_str());
        }
    }

    // The session the lobby controls act on: the Local lobby, or the SystemLink host while the real transport is on. A signed-in gamer can
    // belong to one session at a time, so the two are never open together.
    [[nodiscard]] NetworkSession* Primary() const { return host_ != nullptr ? host_ : lobby_; }
    void SetReady(bool v)
    {
        wantReady_ = v;
        NetworkSession* n = Primary();
        if (n == nullptr || n->getSessionStateProperty() != NetworkSessionState::Lobby) return;   // IsReady can only change in the lobby
        for (int i = 0; i < n->getLocalGamersProperty().getCountProperty(); ++i) n->getLocalGamersProperty()[i]->setIsReadyProperty(v);
    }

    [[nodiscard]] Vector2 Truth(float t) const { return Vector2(std::sin(t * 1.3f) * 0.85f, std::sin(t * 2.1f + 0.7f) * 0.8f); }

    // One frame: the Visitor sends at `rate_` per second, the Guest (the second local gamer) drains what arrived.
    void Pump(float dt)
    {
        if (host_ == nullptr || phase_ != Phase::Connected) return;
        try {
            host_->Update();
            if (host_->getLocalGamersProperty().getCountProperty() < 2) return;
            LocalNetworkGamer* sender = host_->getLocalGamersProperty()[0];
            LocalNetworkGamer* guest = host_->getLocalGamersProperty()[1];
            sendClock_ += dt * rate_;
            while (sendClock_ >= 1.0f) {
                sendClock_ -= 1.0f;
                PacketWriter w;
                const Vector2 p = Truth(time_);
                w.Write(sequence_++);
                w.Write(time_);
                w.Write(p.X);
                w.Write(p.Y);
                sender->SendData(w, SendDataOptions::InOrder);
                ++sent_;
                trail_.push_back(p);
                if (trail_.size() > 60) trail_.pop_front();
            }
            while (sender->getIsDataAvailableProperty()) {   // the sender's own copy: drained so the queue does not grow
                PacketReader r;
                NetworkGamer* from = nullptr;
                sender->ReceiveData(r, from);
            }
            while (guest->getIsDataAvailableProperty()) {
                PacketReader r;
                NetworkGamer* from = nullptr;
                guest->ReceiveData(r, from);
                const int seq = r.ReadInt32();
                const float sentAt = r.ReadSingle();
                const Vector2 p(r.ReadSingle(), r.ReadSingle());
                ++received_;
                lastSeq_ = seq;
                ageMs_ = (time_ - sentAt) * 1000.0f;
                received_trail_.push_back(p);
                if (received_trail_.size() > 60) received_trail_.pop_front();
            }
        } catch (const std::exception& e) {
            status_ = std::string("session error: ") + std::string(e.what()).substr(0, 50);
        }
    }

    template <typename Rect, typename Text>
    void DrawSession(Rect& rect, Text& text)
    {
        if (lobby_ == nullptr && host_ == nullptr) {
            text(status_, 24, 90, Kit::Rgba(255, 190, 150), 0.65f);
            return;
        }
        const float px = 24.0f, py = 100.0f, size = 300.0f;
        for (int pane = 0; pane < 2; ++pane) {
            const float x = px + static_cast<float>(pane) * 330.0f;
            rect(x, py, size, size, Color(18, 24, 38, 240));
            rect(x, py, size, 3, pane == 0 ? Kit::kGold : Kit::Rgba(110, 170, 255));
            text(pane == 0 ? "the Visitor's dot (what is sent)" : "what the Guest receives", x, py - 28, pane == 0 ? Kit::kGold : Kit::Rgba(150, 190, 255), 0.55f);
            const std::deque<Vector2>& points = pane == 0 ? trail_ : received_trail_;
            for (std::size_t i = 0; i < points.size(); ++i) {
                const float a = 0.25f + 0.75f * static_cast<float>(i) / static_cast<float>(std::max<std::size_t>(1, points.size()));
                const float cx = x + size * 0.5f + points[i].X * size * 0.5f, cy = py + size * 0.5f + points[i].Y * size * 0.5f;
                const bool newest = i + 1 == points.size();
                const float r = newest ? 8.0f : 4.0f;
                rect(cx - r, cy - r, r * 2, r * 2, newest ? (pane == 0 ? Color(250, 210, 80, 255) : Color(120, 190, 255, 255)) : Kit::Rgba(static_cast<int>(200 * a), static_cast<int>(200 * a), static_cast<int>(230 * a)));
            }
            if (points.empty()) text(phase_ == Phase::Off ? "A Local session delivers no packets: switch packet delivery on." : "waiting...", x + 14, py + size * 0.5f - 10, Kit::kDim, 0.5f);
        }
        const float lost = sent_ > 0 ? 100.0f * static_cast<float>(sent_ - received_) / static_cast<float>(sent_) : 0.0f;
        char line[200];
        std::snprintf(line, sizeof line, "sent %d   received %d   missing %.0f %%   last arrived #%d, %.0f ms old   %d B/s out   %d B/s in", sent_, received_, static_cast<double>(std::max(0.0f, lost)), lastSeq_,
                      static_cast<double>(ageMs_), host_ != nullptr ? host_->getBytesPerSecondSentProperty() : 0, host_ != nullptr ? host_->getBytesPerSecondReceivedProperty() : 0);
        text(line, 24, 420, Kit::kText, 0.5f);
        text("events", 670, 70, Kit::kDim, 0.55f);
        float y = 96.0f;
        for (const std::string& l : log_) {
            text(l, 670, y, Kit::Rgba(190, 235, 200), 0.45f);
            y += 24.0f;
        }
        text("transport: " + transportStatus_, 24, 450, phase_ == Phase::Failed ? Kit::Rgba(255, 190, 150) : Kit::kDim, 0.5f);
    }

    enum class Phase { Off, Connected, Failed };
    std::array<bool, 8> fields_ = {true, true, true, false, true, true, false, false};
    int stringLength_ = 12, size_ = 0;
    bool dirty_ = true;
    std::vector<SharpRuntime::bytecs> bytes_;
    std::string roundTrip_ = "-";

    bool wantReady_ = false;
    NetworkSession* lobby_ = nullptr;
    NetworkSession* host_ = nullptr;
    Phase phase_ = Phase::Off;
    std::string transportStatus_ = "off: no packets leave this process";
    std::size_t started_ = 0, ended_ = 0, joined_ = 0, left_ = 0, sessionEnded_ = 0;
    bool subscribed_ = false, tried_ = false;
    float time_ = 0.0f, rate_ = 20.0f, sendClock_ = 0.0f, ageMs_ = 0.0f;
    int sent_ = 0, received_ = 0, lastSeq_ = 0, sequence_ = 0;
    std::deque<Vector2> trail_, received_trail_;
    std::deque<std::string> log_;
    std::string status_ = "-";
#else
    void CloseSession() {}
#endif
    Mode mode_;
};

}  // namespace

std::unique_ptr<Exhibit> MakeNetLab(const RoomDef& room, Services& services)
{
    return std::make_unique<NetLab>(room, services);
}
