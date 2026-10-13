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

/** One lab room: same shape for every maths room, so the data stays a table. */
RoomSpec Lab(const char* key, const char* title, const char* subtitle, const char* exhibit, const char* mode, std::vector<std::string> apis)
{
    RoomSpec r;
    r.key = key;
    r.title = title;
    r.subtitle = subtitle;
    r.exhibit = exhibit;
    r.params = {{"mode", mode}};
    r.width = 13;
    r.depth = 13;
    r.theme = "white-cube";
    r.apis = std::move(apis);
    return r;
}

WingSpec RotationWing()
{
    WingSpec w;
    w.name = "Rotation and Interpolation";
    w.tagline = "Quaternions, Euler angles, easing functions and curves";
    w.theme = "corridor-warm";
    w.accent = {0.98f, 0.70f, 0.35f};
    w.spineSide = Wall::East;
    w.rooms = {
        Lab("quat-euler", "Euler Angles and Gimbal Lock", "Yaw, pitch and roll as a quaternion and as three matrices, and where they stop agreeing", "quaternions", "euler",
            {"Quaternion.CreateFromYawPitchRoll", "Matrix.CreateFromQuaternion", "Matrix.CreateRotationX"}),
        Lab("quat-slerp", "Slerp and Lerp", "Quaternion.Slerp turns at constant speed; Lerp then Normalize does not", "quaternions", "slerp",
            {"Quaternion.Slerp", "Quaternion.Lerp", "Quaternion.Normalize", "Quaternion.Dot"}),
        Lab("quat-axis", "Axis and Angle", "Quaternion.CreateFromAxisAngle, Concatenate and Inverse", "quaternions", "axisangle",
            {"Quaternion.CreateFromAxisAngle", "Quaternion.Concatenate", "Quaternion.Inverse"}),
        Lab("curve-easing", "Easing Functions", "MathHelper.Lerp, SmoothStep, Hermite and CatmullRom graphed side by side", "curves", "easing",
            {"MathHelper.Lerp", "MathHelper.SmoothStep", "MathHelper.Hermite", "MathHelper.CatmullRom"}),
        Lab("curve-spline", "Splines", "Vector3.CatmullRom through four control points with a rider", "curves", "spline", {"Vector3.CatmullRom"}),
        Lab("curve-class", "The Curve Class", "Curve, CurveKey, tangents and loop types", "curves", "curve", {"Curve", "CurveKey", "CurveLoopType", "CurveTangent"}),
        Lab("curve-barycentric", "Barycentric Coordinates", "Vector3.Barycentric weights inside a triangle", "curves", "barycentric", {"Vector3.Barycentric"}),
    };
    return w;
}

WingSpec CollisionWing()
{
    WingSpec w;
    w.name = "Collision and Geometry";
    w.tagline = "BoundingBox, BoundingSphere, BoundingFrustum, Plane and Ray";
    w.theme = "corridor-dark";
    w.accent = {0.95f, 0.45f, 0.45f};
    w.spineSide = Wall::West;
    w.rooms = {
        Lab("col-box-box", "Box vs Box", "BoundingBox.Intersects and Contains between two axis-aligned boxes", "collision", "box-box", {"BoundingBox.Intersects", "BoundingBox.Contains", "ContainmentType"}),
        Lab("col-sphere-box", "Sphere vs Box", "BoundingSphere.Intersects and Contains against a box", "collision", "sphere-box", {"BoundingSphere.Intersects", "BoundingSphere.Contains"}),
        Lab("col-ray-box", "Ray vs Box", "Ray.Intersects(BoundingBox): the distance to the entry point", "collision", "ray-box", {"Ray.Intersects", "BoundingBox"}),
        Lab("col-ray-sphere", "Ray vs Sphere", "Ray.Intersects(BoundingSphere) with a movable aim", "collision", "ray-sphere", {"Ray.Intersects", "BoundingSphere"}),
        Lab("col-ray-plane", "Ray vs Plane", "Ray.Intersects(Plane): crossing distance, parallel and backward rays", "collision", "ray-plane", {"Ray.Intersects", "Plane"}),
        Lab("col-plane-box", "Which Side of the Plane", "Plane.Intersects(BoundingBox) classifies five boxes as Front, Back or Intersecting", "collision", "plane-box", {"Plane.Intersects", "Plane.DotCoordinate", "PlaneIntersectionType"}),
        Lab("col-merge", "Fitting and Merging Bounds", "CreateFromPoints for boxes and spheres, and CreateMerged", "collision", "merge", {"BoundingBox.CreateFromPoints", "BoundingSphere.CreateFromPoints", "BoundingSphere.CreateMerged"}),
        Lab("col-frustum", "Frustum Culling", "BoundingFrustum.Contains: fully inside, partly inside or culled", "collision", "frustum", {"BoundingFrustum.Contains", "BoundingFrustum", "Matrix.CreatePerspectiveFieldOfView"}),
        Lab("col-pick", "Picking", "Your view ray tested with Ray.Intersects against boxes and spheres", "collision", "pick", {"Ray.Intersects", "BoundingBox", "BoundingSphere"}),
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
    f.wings.push_back(RotationWing());
    f.wings.push_back(CollisionWing());
    return f;
}
