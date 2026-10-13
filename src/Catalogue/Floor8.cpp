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
        ct("ct-lifetime", "Resource Lifetime", "Dispose, IsDisposed, the Disposing event and ContentManager.Unload", "lifetime", {"GraphicsResource.Dispose", "GraphicsResource.IsDisposed", "GraphicsResource.Disposing", "GraphicsDevice.ResourceCreated", "GraphicsDevice.ResourceDestroyed", "ResourceCreatedEventArgs", "ResourceDestroyedEventArgs", "ContentManager.Unload"}),
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
        cx("cx-package", "Shader Packages", "One shader in several languages: the package picks the variant this renderer can run", "package", {"CNA::Graphics::ShaderPackageEXT", "ShaderPackageEXT.selectFor", "ShaderCodeEXT", "ShaderEffect(device, package)", "ShaderLanguageEXT"}),
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
        pl("pl-adapter", "Adapter and Back Buffer", "GraphicsAdapter, display modes, GraphicsProfile and PresentationParameters", "adapter", {"GraphicsAdapter", "DisplayMode", "GraphicsDevice.GraphicsProfile", "PresentationParameters"}, ApiOrigin::Xna),
        pl("pl-renderer", "The Active Renderer", "Renderer identity, maturity, features, limits and surface-format support", "renderer", {"GraphicsDevice.GetRendererCapabilityProfileEXT", "RendererFeature", "RendererLimit", "RendererFormatUsage", "GraphicsDevice.GetGraphicsRendererName"}, ApiOrigin::CnaExtension),
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
    f.wings.push_back(HistoryWing());
    return f;
}
