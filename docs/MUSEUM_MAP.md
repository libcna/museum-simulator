# Museum map

The museum is **data**: floors, wings and rooms are described in `src/Catalogue/FloorN.cpp`, laid out automatically by
`src/World/Layout.cpp` and built lazily around the visitor (`src/World/MuseumWorld.cpp`). This page is generated from that
catalogue by `tools/gen_docs.sh`; the plan below (hand-written) explains the shape every floor shares.

## Shape of a floor

```
                       wing galleries (one per wing, rooms on both sides)
   west wings  <--------- concourse (spine, runs north) --------->  east wings
                              |
                         floor hall (hub)  -- elevator car (10 buttons, one per floor)
```

- Every floor has a **hall** (the hub you arrive in from the elevator), a **concourse** that runs north from it,
  and **wings**: a gallery corridor off the concourse with exhibit rooms on both sides.
- Room numbers read `floor.wing.room`, for example `4.2.07`. They are printed on the door-side plaque of every room,
  on the lintel signs of the gallery, on the floor map and in the directory (`M` or `Tab`).
- Each floor has its own accent colour (floor numerals, signs, map highlight).
- Elevators connect all ten floors; the directory can also teleport to any room on a floor you have already visited
  (floor 0 is always open).

## What each floor is for

| Floor | Name | Subject |
|---|---|---|
| 0 | Lobby and Introductory Tour | the entrance hall and the first eleven showcase rooms |
| 1 | Fundamentals | application model, vectors, matrices, rotations and curves, collision, cameras |
| 2 | Two-Dimensional Rendering | SpriteBatch, blending and sorting, sprite techniques, SpriteFont and text |
| 3 | Three-Dimensional Fundamentals | vertex and index buffers, primitive types, dynamic geometry, instancing; textures |
| 4 | Effects and Lighting | BasicEffect in depth, the lighting lab, CNA ShaderEffect shaders |
| 5 | Render State and Render Targets | Rasterizer/DepthStencil/Blend states, render targets, mirrors, MRT |
| 6 | Models, Animation and Particles | glTF models, skeletal and rigid animation, particle systems |
| 7 | Audio and Input | SoundEffect, 3D audio, synthesis, music; keyboard, mouse, pad, text input |
| 8 | Content, Platform and CNA | content pipeline, CNA extensions (DebugDraw, CRT, ASCII...), platform queries |
| 9 | Performance and Validation | stress labs, renderer self-checks, reference scenes, the observation deck |

## The rooms

<!-- BEGIN MAP -->
<!-- END MAP -->
