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
        ct("ct-formats", "Loose Image Formats", "PNG, JPEG, BMP, TGA, GIF and QOI decoded without a build step (and TIFF, which is not)", "formats", {"ContentManager.Load<Texture2D>", "Texture2D.GetData"}, ApiOrigin::CnaExtension),
        ct("ct-failure", "When Loading Fails", "ContentLoadException for missing, mistyped and corrupt assets", "failure", {"ContentLoadException", "ContentManager.Load"}),
        ct("ct-pipeline", "The Content Compiler", "cna-content builds .cnb and .xnb; loading and sizes compared with loose files", "pipeline", {"cna-content", "ContentManager.Load", "ContentManager.GetContentManifest"}, ApiOrigin::CnaExtension),
        ct("ct-storage", "Title Files and Saved Data", "TitleContainer.OpenStream, StorageDevice and StorageContainer", "storage", {"TitleContainer.OpenStream", "StorageDevice", "StorageContainer", "StorageDevice.GetStorageRootEXT"}),
        ct("ct-custom", "Teaching ContentManager a New Type", "Your own types loaded through ContentManager: a loose-file reader and a named .cnj loader", "custom", {"ContentManager.RegisterTypeReader", "LooseFileContentTypeReader", "ContentManager.RegisterCnjLoader", "ContentLoadException"}, ApiOrigin::CnaExtension),
        ct("ct-reader", "Content Readers", "An .xnb file with your own type: a ContentTypeReader registered under the name the file records", "reader",
           {"ContentTypeReader", "ContentTypeReaderManager.AddTypeCreator", "ContentReader", "ContentManager.Load"}),
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
        pl("pl-window", "The Game Window", "GameWindow title, client bounds, resizing, activation and file drops", "window", {"GameWindow.Title", "GameWindow.ClientBounds", "GameWindow.AllowUserResizing", "GameWindow.ClientSizeChanged", "Game.Activated", "GameWindow.FileDropEXT"}, ApiOrigin::Xna),
        pl("pl-adapter", "Adapter and Back Buffer", "GraphicsAdapter, display modes, GraphicsProfile and PresentationParameters", "adapter", {"GraphicsAdapter", "DisplayMode", "GraphicsDevice.GraphicsProfile", "PresentationParameters", "GraphicsAdapter.IsProfileSupported", "GraphicsAdapter.QueryRenderTargetFormat", "GraphicsAdapter.QueryBackBufferFormat", "GraphicsAdapter.Adapters"}, ApiOrigin::Xna),
        pl("pl-renderer", "The Active Renderer", "Renderer identity, maturity, features, limits, surface formats and the device's own colour-space and support queries", "renderer", {"GraphicsDevice.GetRendererCapabilityProfileEXT", "RendererFeature", "RendererLimit", "RendererFormatUsage", "GraphicsDevice.GetGraphicsRendererName", "GraphicsDevice.GetDisplayColorSpaceEXT", "GraphicsDevice.SupportsDisplayColorSpaceEXT", "GraphicsDevice.SetDisplayColorSpaceEXT", "GraphicsDevice.SupportsImageBasedLightingEXT", "GraphicsDevice.GetTrackedResourceCount", "GraphicsDevice.GetLogicalSizeEXT"}, ApiOrigin::CnaExtension),
        pl("pl-platform", "Platform Capabilities", "What the host platform can do: clipboard, IME, gamepads, haptics, camera, dialogs", "platform", {"Game.GetPlatformCapabilitiesEXT", "PlatformCapabilities"}, ApiOrigin::CnaExtension),
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
        hi("hist-cna", "This Build of CNA", "The revision, options, renderers and maturity of the CNA running right now", "cna", {"CNA::GraphicsRendererSelection.GetAvailable", "CNA::getGraphicsBackendMaturity", "CNA::getGraphicsRendererName"}, ApiOrigin::CnaExtension),
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
             {"AvatarDescription", "AvatarDescription.CreateRandom", "AvatarDescription.BeginGetFromGamer", "AvatarRenderer", "AvatarAnimation", "AvatarAnimationPreset", "AvatarExpression", "AvatarBodyType"}),
        room("net-packets", "Packets", "PacketWriter and PacketReader: XNA's types as bytes, in order, with their cost", "net-lab", "packets",
             {"PacketWriter", "PacketReader", "PacketWriter.Write", "PacketReader.ReadVector3", "PacketReader.ReadQuaternion", "PacketReader.ReadMatrix"}),
        room("net-session", "A Network Session", "A Local session between two players: send, receive, simulated latency and loss, start and end", "net-lab", "session",
             {"NetworkSession.Create", "NetworkSession.Update", "NetworkSession.StartGame", "NetworkSession.EndGame", "NetworkSession.SimulatedLatency", "NetworkSession.SimulatedPacketLoss", "NetworkSession.SessionState",
              "LocalNetworkGamer.SendData", "LocalNetworkGamer.ReceiveData", "SendDataOptions", "NetworkGamer.IsReady", "NetworkSession.GamerJoined", "NetworkSession.GameStarted"}),
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
    f.wings.push_back(HistoryWing());
    return f;
}
