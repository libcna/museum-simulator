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

WingSpec TransformWing()
{
    WingSpec w;
    w.name = "Matrices and Transforms";
    w.tagline = "World, view and projection: how a point reaches the screen";
    w.theme = "corridor-green";
    w.accent = {0.55f, 0.90f, 0.55f};
    w.spineSide = Wall::West;
    const auto tr = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "transforms";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.theme = "white-cube";
        r.apis = std::move(apis);
        return r;
    };
    w.rooms = {
        tr("mat-order", "Transform Order", "Matrix.CreateScale, CreateRotationY and CreateTranslation multiplied in two orders", "order", {"Matrix.CreateScale", "Matrix.CreateRotationY", "Matrix.CreateTranslation", "Matrix.Multiply"}),
        tr("mat-hierarchy", "Parent and Child", "Local and world matrices, Matrix.Decompose and transform hierarchies", "hierarchy", {"Matrix.Multiply", "Matrix.Decompose", "Matrix.Translation"}),
        tr("mat-view", "The View Matrix", "Matrix.CreateLookAt: where the camera is, what it looks at, and what it sees", "lookat", {"Matrix.CreateLookAt", "Matrix.Invert", "BoundingFrustum"}),
        tr("mat-perspective", "Perspective Projection", "CreatePerspectiveFieldOfView with live field of view, aspect and clip planes", "perspective", {"Matrix.CreatePerspectiveFieldOfView", "BoundingFrustum.Contains"}),
        tr("mat-ortho", "Orthographic Projection", "CreateOrthographic beside perspective: no vanishing point, constant size", "ortho", {"Matrix.CreateOrthographic", "Matrix.CreatePerspectiveFieldOfView"}),
        tr("mat-screen", "World to Screen", "World, view, clip and screen coordinates of a point, with Viewport.Project", "project", {"Viewport.Project", "Vector4.Transform", "Matrix.Multiply"}),
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
    f.wings.push_back(TransformWing());
    return f;
}
