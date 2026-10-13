// Floor 6: models, animation and particles.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec ParticleWing()
{
    WingSpec w;
    w.name = "Particle Complex";
    w.tagline = "Eleven effects from one emitter: every control changes the effect live";
    w.theme = "corridor-warm";
    w.accent = {1.0f, 0.55f, 0.25f};
    w.spineSide = Wall::West;
    const auto pa = [](const char* key, const char* title, const char* subtitle, const char* preset, bool outdoor = false) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "particle-lab";
        r.params = {{"preset", preset}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.5f;
        r.theme = "cave";
        r.apis = {"DynamicVertexBuffer", "SetDataOptions.Discard", "IndexBuffer", "BlendState.Additive", "SpriteBatch"};
        (void)outdoor;
        return r;
    };
    w.rooms = {
        pa("pt-fire", "Fire", "Additive flames rising from a cone emitter, with a crackling loop", "fire"),
        pa("pt-smoke", "Smoke", "Large alpha-blended puffs growing and fading as they climb", "smoke"),
        pa("pt-sparks", "Sparks", "A fast fountain of sparks that bounce on the floor", "sparks"),
        pa("pt-snow", "Snow", "Thousands of flakes drifting down with a gentle sway", "snow"),
        pa("pt-rain", "Rain", "Fast thin streaks with gravity and a floor kill plane", "rain"),
        pa("pt-fountain", "Fountain", "An arc of water drops that splash back to the pool, with a water loop", "fountain"),
        pa("pt-dust", "Dust Motes", "A few slow specks drifting in a beam: sub-pixel size and alpha", "dust"),
        pa("pt-fireworks", "Fireworks", "Rockets that explode into sparks when their life ends", "fireworks"),
        pa("pt-bubbles", "Bubbles", "Rising translucent spheres that grow as they go", "bubbles"),
        pa("pt-leaves", "Falling Leaves", "Wide, slow, spinning leaves swaying on the way down", "leaves"),
        pa("pt-magic", "Magic Spiral", "Additive sparkles pulled into a rising spiral by a swirl force", "magic"),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor6()
{
    FloorSpec f;
    f.index = 6;
    f.name = "Models, Animation and Particles";
    f.tagline = "glTF models and their effects, procedural and skeletal animation, particle systems";
    f.theme = "concourse";
    f.accent = {1.0f, 0.55f, 0.25f};
    f.wings.push_back(ParticleWing());
    return f;
}
