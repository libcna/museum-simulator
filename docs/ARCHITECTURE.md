# Architecture

The museum is an application on top of CNA. It adds structure, not an engine: there is no scene graph, entity system,
renderer, scripting language or editor. Everything that draws calls CNA's `GraphicsDevice`, `SpriteBatch`,
`BasicEffect` and friends directly.

## Layers

```
 src/Main.cpp                     data-only commands (--validate, --list-rooms, --map-markdown ...) or MuseumGame
 src/MuseumGame                   the CNA Game subclass: update/draw loop, elevator rides, directory, scripting, clock
   src/World                      MuseumWorld: which rooms exist right now, what is drawn, collision
   src/Catalogue                  the museum as data (floors, wings, rooms, themes, reference views, inventory)
   src/Exhibits                   Exhibit base class, registry, one subject per directory
   src/Rendering, Interaction, UI, Audio, Camera   application helpers
```

## The museum is data

`src/Catalogue/FloorN.cpp` describes each floor with the small builder in `CatalogueBuilder.hpp`:

```cpp
RoomSpec r;  r.key = "cam-orbit";  r.title = "Orbit Camera";  r.subtitle = "...";
r.exhibit = "camera-lab";  r.params = {{"mode", "orbit"}};  r.apis = {"Matrix.CreateLookAt", ...};
```

`World/Layout.cpp` turns the specs into a `MuseumPlan` (`World/Catalogue.hpp`): for every floor a hall, an elevator car, a
concourse and one gallery per wing, with rooms on both sides, door pairs generated symmetrically and numbers
`floor.wing.room`. `ValidateMuseum()` checks duplicate keys and numbers, broken or one-way doors, unreachable cells,
unknown exhibit kinds and missing assets; `tests/museum_tests.cpp` and every start-up run it.

An **exhibit kind** is a C++ class registered by name in `Exhibits/ExhibitRegistry.cpp`. One class serves many rooms
through `RoomDef::params` (for example `camera-lab` with `mode=orbit|chase|...`), so data-driven rooms still each demonstrate
something different. Each room gets its door-side plaque (`RoomCard.cpp`) automatically from the catalogue.

## Scale: only what is near exists

Hundreds of rooms are never instantiated together. `MuseumWorld` works on the door graph:

- cells within a few doors of the visitor are **built** (static meshes, colliders, exhibit objects) and cells a little closer
  are **loaded** (render targets, audio voices, textures); far cells are destroyed (`keepDepth`, a hysteresis factor)
- visibility uses the portals: a room is drawn only if a chain of doors leads to it through the camera frustum
  (`maxVisibleDepth`), and only the current room's overlays (labels) are drawn
- exhibits update only while `ctx.nearby`; the lifetime counters (`built`, `loaded`, `destroyed`) are in the `F3`
  overlay and in `tools/tour.sh` output, which checks that rooms are released as the tour moves on

## Exhibit framework

- `Exhibit` (base): `BuildStatic` (architecture through `RoomBuilder`), `Load` (GPU objects, interactables), `Update`,
  `RenderOffscreen` (render-target passes, before the back buffer), `DrawOpaque`, `DrawTransparent`, `DrawOverlay`.
- `ScreenExhibit`: a room whose centrepiece is a live SpriteBatch screen on the far wall (`DrawScreen`), a control
  panel and an information sign. Most labs derive from it.
- `ControlPanel` (`Interaction/`): sliders, toggles, choices, buttons and read-outs drawn on a render target in the
  world; hover, click, drag, wheel. Scripts operate it with `press:LABEL=V`.
- `Gizmos`, `MiniView`, `LitMesh`/`Shapes`, `ParticleSystem`/`ParticleRenderer`: small helpers over CNA primitives.
- Exhibits own their GPU objects and release them in destructors; shared effect state is never relied on (every
  exhibit sets everything it uses before drawing).

### The room's local frame

Every exhibit is written for "the door is on the south wall, screens and the main subject face the visitor on the
north wall". Rooms alternate between the two sides of a gallery, so half of them are entered through their *north* wall
(`RoomDef::Turned()`, `--list-turned-rooms`). Those rooms have their local frame turned half a circle about the room
centre (`RoomDef::frameYaw`): `ToWorld(x, y, z)`, `Rotate(offset)`, `LocalWall(wall)` and `Exhibit::Off(x, y, z)` do the
mapping, `RoomBuilder` (boxes, signs, wall points, facing quads) and `ControlPanel` apply it, and
`InteractionSystem::Add` puts interactable bounds in order. Rules for exhibit code:

- Positions come from `R().ToWorld(x, y, z)`; a horizontal offset from such a point is `base + Off(dx, dy, dz)`, not
  `base + Vector3(dx, dy, dz)`.
- A yaw that faces something placed in local space gets `+ R().frameYaw`; `FarPoint`/`FarYaw`/`FarQuad` already do.
- Things tied to the real world (the compass and sun on the observation deck, `MuseumPlan::SpawnPoint`) stay in world
  axes; `RoomDef::WorldAt` is the plain centre-plus-offset.
- `museum_tests` (`TestRoomFrames`) checks that every room's door is on the local south wall and its arrival view faces
  the local north wall.

## Navigation

`UI/FloorMap` (plan and minimap), `UI/DirectoryScreen` (search, tabs, quick travel; floors unlock when their hall or
elevator has been visited), elevator rides (fade, teleport between identical cars, fade), room numbers and signs
generated from the catalogue.

## Time and determinism

`TimeOfDay` is the museum clock (sun, moon, daylight, ambient). `--time H` fixes it, `T`/`Y` move it in the game.
`--fixed-step` makes every run reproducible; `--view` and `--no-hud` make screenshots comparable
(`tools/reference.sh`).

## Build

`CMakeLists.txt` builds everything except `main()` as the static library `museum_core`, which the executable and
`museum_tests` both link. CNA is added with `add_subdirectory(../cna)`; the museum sets three CNA options
(`CNA_CNAEXT`, `CNA_DEVICES`, `CNA_DIAGNOSTICS=STATS`) and compiles `Content/Pipeline` with `cna-content`.

## Conventions

- No renderer-specific code, no per-frame heap churn in hot paths (buffers are reused), RAII for every resource.
- Warnings are on for the museum's own sources only (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`).
- Never block on optional hardware: a missing pad, joystick, microphone, touch screen or audio device is reported in the
  room and everything else keeps working.
