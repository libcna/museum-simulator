// Floor 1: Fundamentals. The application model, the maths a game is built on, geometry tests and cameras.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec MathWing()
{
    WingSpec w;
    w.name = "Vector Mathematics";
    w.tagline = "Vector3: arithmetic, products, reflection and interpolation";
    w.theme = "corridor-cool";
    w.accent = {0.45f, 0.78f, 0.98f};
    w.spineSide = Wall::East;
    const auto vec = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "vectors";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "white-cube";
        r.apis = std::move(apis);
        return r;
    };
    w.rooms = {
        vec("vec-add", "Adding Vectors", "Vector3.Add and Subtract: the parallelogram rule, with sliders for both vectors", "add", {"Vector3.Add", "Vector3.Subtract"}),
        vec("vec-dot", "The Dot Product", "Vector3.Dot measures how much two vectors agree; its sign and size follow the angle", "dot", {"Vector3.Dot"}),
        vec("vec-cross", "The Cross Product", "Vector3.Cross builds a perpendicular vector whose length is the spanned area", "cross", {"Vector3.Cross", "Vector3.Length"}),
        vec("vec-project", "Projection and Rejection", "Splitting a vector into a part along another and a part across it", "project", {"Vector3.Dot", "Vector3.Multiply"}),
        vec("vec-reflect", "Reflection", "Vector3.Reflect bounces a vector off a surface with a given normal", "reflect", {"Vector3.Reflect", "Vector3.Normalize", "Plane"}),
        vec("vec-lerp", "Interpolation", "Vector3.Lerp, SmoothStep and CatmullRom between two points", "lerp", {"Vector3.Lerp", "Vector3.SmoothStep", "Vector3.CatmullRom"}),
        vec("vec-normalize", "Length and Normalize", "Vector3.Length, Normalize and Distance on the unit sphere", "normalize", {"Vector3.Length", "Vector3.Normalize", "Vector3.Distance"}),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor1()
{
    FloorSpec f;
    f.index = 1;
    f.name = "Fundamentals";
    f.tagline = "The application model, the maths of games, geometry tests and cameras";
    f.theme = "concourse";
    f.accent = {0.40f, 0.75f, 0.95f};
    f.wings.push_back(MathWing());
    return f;
}
