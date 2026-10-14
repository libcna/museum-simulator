// Floor 8: content, platform and the CNA extensions.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec ContentWing()
{
    WingSpec w;
    w.name = "Content Pipeline";
    w.tagline = "ContentManager, loose files, compiled .cnb and .xnb, load failures, storage and resource lifetime";
    w.theme = "workshop";
    w.accent = {0.35f, 0.80f, 0.55f};
    w.spineSide = Wall::West;
    const auto ct = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin = ApiOrigin::Xna) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "content-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "workshop";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        ct("ct-load", "Loading Assets", "ContentManager.Load for textures, sounds and models, timed", "load", {"ContentManager.Load<Texture2D>", "ContentManager.Load<SoundEffect>", "ContentManager.Load<Model>", "ContentManager.RootDirectory"}),
        ct("ct-cache", "The ContentManager Cache", "Cached loads, Unload and separate managers", "cache", {"ContentManager.Load", "ContentManager.Unload", "GraphicsResource.IsDisposed"}),
        ct("ct-formats", "Loose Image Formats", "PNG, JPEG, BMP, TGA, GIF and QOI decoded without a build step, a DXT1 DDS read from a stream (and TIFF, which is not)", "formats", {"ContentManager.Load<Texture2D>", "Texture2D.GetData", "Texture2D.DDSFromStreamEXT", "SurfaceFormat.Dxt1"}, ApiOrigin::CnaExtension),
        ct("ct-failure", "When Loading Fails", "ContentLoadException for missing, mistyped and corrupt assets", "failure", {"ContentLoadException", "ContentManager.Load"}),
        ct("ct-pipeline", "The Content Compiler", "cna-content builds .cnb and .xnb; loading and sizes compared with loose files", "pipeline", {"cna-content", "ContentManager.Load", "ContentManager.GetContentManifest"}, ApiOrigin::CnaExtension),
        []{
            RoomSpec r;
            r.key = "ct-intermediate";
            r.title = "The Pipeline in the Game";
            r.subtitle = "A content type written as XNA intermediate XML and read back, and what the pipeline says about a damaged document";
            r.exhibit = "pipeline-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "workshop";
            r.apis = {"IntermediateSerializer", "IntermediateSerializer.Serialize", "IntermediateSerializer.Deserialize", "ContentItem", "ContentTypeDescriptor", "InvalidContentException", "ContentSerializerAttribute", "ContentSerializerIgnoreAttribute", "ContentSerializerCollectionItemNameAttribute"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
        []{
            RoomSpec r;
            r.key = "ct-processor";
            r.title = "Processing a Texture";
            r.subtitle = "TextureImporter and TextureProcessor: colour key, premultiplied alpha, mipmaps, power-of-two resize and DXT compression, measured";
            r.exhibit = "processor-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "workshop";
            r.apis = {"TextureImporter", "TextureProcessor", "TextureProcessor.ColorKeyEnabled", "TextureProcessor.PremultiplyAlpha", "TextureProcessor.GenerateMipmaps", "TextureProcessor.ResizeToPowerOfTwo",
                      "TextureProcessor.TextureFormat", "TextureProcessorOutputFormat", "Texture2DContent", "MipmapChain", "BitmapContent", "PixelBitmapContent"};
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
        ct("ct-storage", "Title Files and Saved Data", "TitleContainer.OpenStream, StorageDevice and StorageContainer", "storage", {"TitleContainer.OpenStream", "StorageDevice", "StorageContainer", "StorageContainer.CreateDirectory", "StorageContainer.DeleteDirectory", "StorageContainer.DirectoryExists", "StorageContainer.GetFileNames", "StorageContainer.GetDirectoryNames", "StorageDevice.GetStorageRootEXT", "StorageDevice.DeleteContainer", "StorageDevice.TotalSpace", "StorageDevice.FreeSpace"}),
        ct("ct-custom", "Teaching ContentManager a New Type", "Your own types loaded through ContentManager: a loose-file reader and a named .cnj loader", "custom", {"ContentManager.RegisterTypeReader", "LooseFileContentTypeReader", "ContentManager.RegisterCnjLoader", "ContentLoadException"}, ApiOrigin::CnaExtension),
        ct("ct-reader", "Content Readers", "An .xnb file with your own type: a ContentTypeReader registered under the name the file records", "reader",
           {"ContentTypeReader", "ContentTypeReaderManager.AddTypeCreator", "ContentReader", "ContentReader.ReadColor", "ContentReader.ReadVector2", "ContentReader.ReadVector4", "ContentReader.AssetName", "ContentManager.Load"}),
        ct("ct-lifetime", "Resource Lifetime", "Dispose, IsDisposed, the Disposing event and ContentManager.Unload", "lifetime", {"GraphicsResource.Dispose", "GraphicsResource.IsDisposed", "GraphicsResource.Disposing", "GraphicsDevice.ResourceCreated", "GraphicsDevice.ResourceDestroyed", "ResourceCreatedEventArgs", "ResourceDestroyedEventArgs", "ContentManager.Unload"}),
        ct("ct-resource", "Content from Memory", "ResourceContentManager serving compiled .xnb assets from a System.Resources.ResourceManager instead of the content folder", "resource",
           {"ResourceContentManager", "System.Resources.ResourceManager", "ContentManager.Load", "ContentLoadException"}),
    };
    return w;
}



RoomSpec ShadowRoom()
{
    RoomSpec r;
    r.key = "cx-shadow";
    r.title = "Shadow Mapping";
    r.subtitle = "A caster pass into a float render target (optionally cascaded), then BasicEffect receives the shadows";
    r.exhibit = "shadow-lab";
    r.width = 15;
    r.depth = 15;
    r.height = 5.5f;
    r.theme = "futuristic";
    r.apis = {"IShadowReceiverEXT", "BasicEffect.ShadowMapEXT", "BasicEffect.LightViewProjectionEXT", "BasicEffect.ShadowDepthBiasEXT", "BasicEffect.ShadowFilterRadiusEXT", "ShadowCascadeStateEXT", "BasicEffect.ShadowCascadesEXT", "ShaderEffect", "Matrix.CreateOrthographic"};
    r.origin = ApiOrigin::CnaExtension;
    return r;
}

WingSpec CnaWing()
{
    WingSpec w;
    w.name = "CNA Extensions";
    w.tagline = "What CNA adds to XNA 4.0: DebugDraw, post-process effects, engine statistics, point lists and device queries";
    w.theme = "futuristic";
    w.accent = {0.35f, 0.80f, 0.55f};
    w.spineSide = Wall::East;
    const auto cx = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "cnaext-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "futuristic";
        r.apis = std::move(apis);
        r.origin = ApiOrigin::CnaExtension;
        return r;
    };
    w.rooms = {
        cx("cx-debugdraw", "DebugDraw", "Wire boxes, spheres, frusta and lines batched into one call", "debugdraw", {"CNA::Graphics::DebugDraw", "DebugDraw.addBox", "DebugDraw.addSphere", "DebugDraw.addFrustum", "DebugDraw.setDepthTested"}),
        cx("cx-crt", "CRT Effect", "Scanlines, curvature, vignette and a phosphor mask as a SpriteBatch effect", "crt", {"CNA::Graphics::CRTEffect", "CRTMaskType", "SpriteBatch.Begin(effect)"}),
        cx("cx-depth", "Colour Depth Effect", "16-bit, 8-bit, grayscale and palette looks with ordered dithering", "depth", {"CNA::Graphics::DepthEffect", "DepthEffectMode", "DitherMode"}),
        cx("cx-ascii", "ASCII Post-Process", "A texture redrawn as text, in black and white or colour", "ascii", {"CNA::Graphics::AsciiPostProcessEffect", "AsciiQuantizeMode"}),
        cx("cx-package", "Shader Packages", "One shader in several languages: the package picks the variant this renderer can run", "package", {"CNA::Graphics::ShaderPackageEXT", "ShaderPackageEXT.selectFor", "ShaderCodeEXT", "ShaderEffect(device, package)", "ShaderLanguageEXT", "ShaderEffect.DeclareUniformBlockEXT"}),
        ShadowRoom(),
        cx("cx-stats", "Engine Statistics", "Draw calls, primitives, state changes and every live graphics resource", "stats", {"CNA::Diagnostics::GetProvider", "Snapshot.metrics", "Snapshot.resources", "SetRuntimeMode"}),
        cx("cx-points", "Point Lists", "PrimitiveType::PointListEXT and debug string markers", "points", {"PrimitiveType.PointListEXT", "GraphicsDevice.DrawUserPrimitives", "GraphicsDevice.SetStringMarkerEXT"}),
        cx("cx-devices", "Device Extensions", "Power, locale, CPU and memory, display scale and safe area", "devices", {"CNA::Devices::SystemInfo", "CNA::Devices::Locale", "CNA::Devices::PowerInfo", "CNA::Devices::DisplayInfo"}),
    };
    return w;
}


WingSpec PlatformWing()
{
    WingSpec w;
    w.name = "Platform and Renderer";
    w.tagline = "The window, the display adapter, the active renderer and the platform, as CNA reports them";
    w.theme = "control-room";
    w.accent = {0.35f, 0.80f, 0.55f};
    w.spineSide = Wall::West;
    const auto pl = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "platform-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "control-room";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        [&] {
            RoomSpec r = pl("pl-exceptions", "Exceptions CNA Throws", "Ten deliberate mistakes and the XNA exception each one raises, with CNA's own message", "", {"System.ObjectDisposedException", "System.ArgumentOutOfRangeException", "System.InvalidOperationException", "ContentLoadException", "System.IO.FileNotFoundException",
                                                                                                            "GamerServicesNotAvailableException", "GuideAlreadyVisibleException", "StorageContainer", "TitleContainer.OpenStream", "SpriteBatch.Begin", "SpriteBatch.End"}, ApiOrigin::Xna);
            r.exhibit = "exception-lab";
            r.params = {};
            return r;
        }(),
        [&] {
            RoomSpec r = pl("pl-log", "CNA's Log", "CNA::Logger: seven levels, nine categories, a minimum level, conditional lines and a sink that receives what the engine itself writes", "", {"CNA::Logger", "Logger.Log", "Logger.InfoIf", "Logger.WarnIf", "Logger.SetMinimumLevel", "Logger.GetMinimumLevel", "Logger.SetSink", "Logger.ResetSink", "CNA::LogLevel", "CNA::LogCategory"}, ApiOrigin::CnaExtension);
            r.exhibit = "log-lab";
            r.params = {};
            return r;
        }(),
        pl("pl-window", "The Game Window", "GameWindow title, client bounds, resizing, activation and file drops", "window", {"GameWindow.GetNativeWindowHandleEXT", "CNA::Platform::NativeWindowHandle", "GameWindow.Title", "GameWindow.ClientBounds", "GameWindow.AllowUserResizing", "GameWindow.ClientSizeChanged", "Game.Activated", "GameWindow.FileDropEXT"}, ApiOrigin::Xna),
        pl("pl-adapter", "Adapter and Back Buffer", "GraphicsAdapter, display modes, GraphicsProfile and PresentationParameters", "adapter", {"GraphicsAdapter", "GraphicsAdapter.UseNullDevice", "GraphicsAdapter.UseReferenceDevice", "GraphicsAdapter.GetCurrentDeviceTypeEXT", "GraphicsAdapter.GetRequiredRendererEXT", "DisplayMode", "GraphicsDevice.GraphicsProfile", "PresentationParameters", "GraphicsAdapter.IsProfileSupported", "GraphicsAdapter.QueryRenderTargetFormat", "GraphicsAdapter.QueryBackBufferFormat", "GraphicsAdapter.Adapters", "GraphicsAdapter.IsWideScreenAspectRatioEXT"}, ApiOrigin::Xna),
        pl("pl-renderer", "The Active Renderer", "Renderer identity, maturity, features, limits, surface formats and the device's own colour-space and support queries", "renderer", {"GraphicsDevice.GetRendererCapabilityProfileEXT", "GraphicsDevice.GetRendererCapabilityReportEXT", "RendererFeature", "RendererLimit", "RendererFormatUsage", "GraphicsDevice.GetGraphicsRendererName", "GraphicsDevice.GetDisplayColorSpaceEXT", "GraphicsDevice.SupportsDisplayColorSpaceEXT", "GraphicsDevice.SetDisplayColorSpaceEXT", "GraphicsDevice.SupportsImageBasedLightingEXT", "GraphicsDevice.GetTrackedResourceCount", "GraphicsDevice.GetLogicalSizeEXT"}, ApiOrigin::CnaExtension),
        pl("pl-platform", "Platform Capabilities", "What the host platform can do: clipboard, IME, gamepads, haptics, camera, dialogs", "platform", {"Game.GetPlatformCapabilitiesEXT", "PlatformCapabilities", "Game.GetPlatformEXT", "CNA::Platform::IPlatform", "IPlatform.GetName", "IPlatform.IsSubsystemInitialized", "IPlatform.GetPerformanceFrequency", "IPlatform.GetTicksMilliseconds", "IPlatform.GetKeyboard"}, ApiOrigin::CnaExtension),
    };
    return w;
}


WingSpec HistoryWing()
{
    WingSpec w;
    w.name = "History and Versions";
    w.tagline = "Where XNA came from, and what this very build of CNA is made of";
    w.theme = "wood-gallery";
    w.accent = {0.35f, 0.80f, 0.55f};
    w.spineSide = Wall::East;
    const auto hi = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, ApiOrigin origin) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "history-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "wood-gallery";
        r.apis = std::move(apis);
        r.origin = origin;
        return r;
    };
    w.rooms = {
        hi("hist-xna", "XNA, 2004 to Today", "The releases of XNA Game Studio and what happened to it", "xna", {"Game", "GraphicsProfile", "XNA 4.0"}, ApiOrigin::Xna),
        hi("hist-cna", "This Build of CNA", "The revision, options, renderers and maturity of the CNA running right now", "cna", {"CNA::GraphicsRendererSelection.GetAvailable", "CNA::GraphicsRendererSelection.GetSelected", "CNA::GraphicsRendererSelection.GetActive", "CNA::GraphicsRendererSelection.IsFallbackEnabled", "CNA::GraphicsRendererSelection.GetFallbackHistory", "CNA::GraphicsRendererFallbackReason", "CNA::getGraphicsBackendMaturity", "CNA::getGraphicsRendererName"}, ApiOrigin::CnaExtension),
        hi("hist-compare", "XNA 4.0 and What CNA Adds", "How many rooms use XNA features and which extensions the others demonstrate, counted live", "compare", {"ApiOrigin", "XNA 4.0", "CNAEXT"}, ApiOrigin::CnaExtension),
    };
    return w;
}

}  // namespace

WingSpec PlayersWing()
{
    WingSpec w;
    w.name = "Players and Networking";
    w.tagline = "GamerServices and Net: signed-in players, achievements, the Guide, packets and a session between two local players";
    w.theme = "control-room";
    w.accent = {0.35f, 0.80f, 0.55f};
    w.spineSide = Wall::East;
    const auto room = [](const char* key, const char* title, const char* subtitle, const char* exhibit, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = exhibit;
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "control-room";
        r.apis = std::move(apis);
        r.origin = ApiOrigin::Xna;
        return r;
    };
    w.rooms = {
        room("gs-players", "Signed-in Players", "Gamer.SignedInGamers, presence, privileges, game defaults and the profile: two local offline players", "players-lab", "players",
             {"Gamer.SignedInGamers", "SignedInGamer.Presence", "SignedInGamer.Privileges", "SignedInGamer.GameDefaults", "Gamer.GetProfile", "GamerProfile", "SignedInGamer.SignedIn", "GamerPresenceMode"}),
        room("gs-achievements", "Achievements", "A catalog the title ships, awards kept by the framework: the museum awards them as you explore", "players-lab", "achievements",
             {"SignedInGamer.AwardAchievement", "SignedInGamer.GetAchievements", "Achievement", "AchievementCollection"}),
        room("gs-leaderboards", "Leaderboards", "LeaderboardWriter sets a gamer's rating, LeaderboardReader pages through the ranking or reads around one gamer", "players-lab", "leaderboards",
             {"LeaderboardWriter", "LeaderboardEntry", "LeaderboardReader", "LeaderboardIdentity", "LeaderboardKey", "Gamer.LeaderboardWriter"}),
        room("gs-guide", "The Guide", "Message boxes and text input drawn by the platform overlay, trial mode and notification position", "players-lab", "guide",
             {"Guide.BeginShowMessageBox", "Guide.EndShowMessageBox", "Guide.BeginShowKeyboardInput", "Guide.IsTrialMode", "Guide.SimulateTrialMode", "Guide.IsVisible", "Guide.NotificationPosition", "MessageBoxIcon"}),
        room("gs-avatar", "Avatars", "AvatarDescription, AvatarAnimation and AvatarRenderer: CNA's original avatars standing in the museum's own scene", "avatar-lab", "avatar",
             {"AvatarDescription", "AvatarDescription.CreateRandom", "AvatarDescription.BeginGetFromGamer", "AvatarRenderer", "AvatarAnimation", "AvatarAnimationPreset", "AvatarExpression", "AvatarBodyType", "AvatarBone", "AvatarEye", "AvatarMouth", "AvatarEyebrow", "AvatarRendererState", "IAvatarAnimation"}),
        room("net-packets", "Packets", "PacketWriter and PacketReader: XNA's types as bytes, in order, with their cost", "net-lab", "packets",
             {"PacketWriter", "PacketReader", "PacketWriter.Write", "PacketReader.ReadVector3", "PacketReader.ReadQuaternion", "PacketReader.ReadMatrix"}),
        room("net-session", "A Network Session", "A Local session between two players: send, receive, simulated latency and loss, start and end", "net-lab", "session",
             {"NetworkSession.AllowHostMigration", "NetworkSession.SessionType", "NetworkSession.MaxGamers", "NetworkSession.PrivateGamerSlots", "NetworkSession.Host", "NetworkSession.RemoteGamers", "NetworkSession.PreviousGamers", "NetworkSession.Create", "NetworkSession.Update", "NetworkSession.StartGame", "NetworkSession.EndGame", "NetworkSession.SimulatedLatency", "NetworkSession.SimulatedPacketLoss", "NetworkSession.SessionState",
              "LocalNetworkGamer.SendData", "LocalNetworkGamer.ReceiveData", "SendDataOptions", "NetworkGamer.IsReady", "NetworkSession.GamerJoined", "NetworkSession.GameStarted", "NetworkSession.BeginFind", "NetworkSession.EndFind", "AvailableNetworkSession", "AvailableNetworkSessionCollection", "QualityOfService", "NetworkSessionProperties", "NetworkSession.SessionProperties", "NetworkSession.AllowJoinInProgress", "NetworkSession.BeginJoin", "NetworkSession.EndJoin", "NetworkGamer.IsLocal", "NetworkGamer.IsHost", "NetworkGamer.RoundtripTime", "NetworkSession.AllGamers", "NetworkSessionJoinException", "NetworkSessionJoinError", "NetworkSessionEndReason", "NetworkSessionEndedEventArgs", "HostChangedEventArgs", "NetworkSession.HostChanged", "NetworkSession.SessionEnded", "NetworkSession.InviteAccepted", "NetworkSession.JoinInvited", "InviteAcceptedEventArgs", "NetworkSessionType.PlayerMatch"}),
    };
    return w;
}

WingSpec OnlineWing()
{
    WingSpec w;
    w.name = "Online Service";
    w.tagline = "CNA's account service: accounts, friends, messages and parties through the Guide (offline unless tools/online_stack.sh runs one)";
    w.theme = "control-room";
    w.accent = {0.45f, 0.75f, 0.95f};
    w.spineSide = Wall::East;
    const auto room = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "online-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "control-room";
        r.apis = std::move(apis);
        r.origin = ApiOrigin::Xna;
        return r;
    };
    w.rooms = {
        room("on-account", "Accounts and the Guide", "Guide.ShowSignIn for real accounts, the profile of a signed-in gamer, achievements and the content page", "account",
             {"Guide.ShowSignIn", "Guide.ShowAchievementsEXT", "Guide.ShowMarketplace", "SignedInGamer.IsSignedInToLive", "GamerProfile.GamerScore", "GamerProfile.Reputation", "GamerProfile.TotalAchievements", "CNA::GamerServices::resolveConfiguration"}),
        room("on-friends", "Friends and Presence", "SignedInGamer.GetFriends, FriendGamer presence and flags, the Guide's friends, players and gamer-card pages", "friends",
             {"SignedInGamer.GetFriends", "FriendCollection", "FriendGamer", "FriendGamer.IsOnline", "FriendGamer.IsAway", "FriendGamer.IsBusy", "FriendGamer.IsPlaying", "FriendGamer.IsJoinable", "FriendGamer.Presence", "Guide.ShowFriends",
              "Guide.ShowPlayers", "Guide.ShowGamerCard", "Guide.ShowPlayerReview", "Guide.ShowFriendRequest"}),
        room("on-messages", "Messages", "Guide.ShowMessages and ShowComposeMessage: the player's inbox and a message to friends", "messages", {"Guide.ShowMessages", "Guide.ShowComposeMessage"}),
        room("on-party", "Parties", "Guide.ShowParty and ShowPartySessions, and SignedInGamer.PartySize", "party", {"Guide.ShowParty", "Guide.ShowPartySessions", "SignedInGamer.PartySize"}),
    };
    return w;
}

FloorSpec MakeFloor8()
{
    FloorSpec f;
    f.index = 8;
    f.name = "Content, Platform and CNA";
    f.tagline = "How assets arrive, what the platform reports, and what CNA adds to XNA";
    f.theme = "concourse";
    f.accent = {0.35f, 0.80f, 0.55f};
    f.wings.push_back(ContentWing());
    f.wings.push_back(CnaWing());
    f.wings.push_back(PlatformWing());
    f.wings.push_back(PlayersWing());
    f.wings.push_back(OnlineWing());
    f.wings.push_back(HistoryWing());
    return f;
}
