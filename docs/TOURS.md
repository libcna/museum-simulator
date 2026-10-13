# Suggested tours

Each stop is a room key: type it with `--room KEY`, find it in the directory (`M`), or use the room number on its plaque.
Every stop below is checked by `museum_tests`, so this page cannot point at a room that does not exist.

## 1. First hour: what CNA is

1. `room:info` the Information Hall: renderer in use, what the museum demonstrates
2. `room:models` a glTF model with world, view and projection matrices
3. `room:lighting` BasicEffect lights, including CNA's point and spot lights
4. `room:rendertarget` a live camera feeding a render target
5. `room:particles` a dynamic vertex buffer drawing fire and smoke
6. `room:audio` 3D positional audio
7. `room:app-lifecycle` the Game loop with live counters
8. `room:cx-debugdraw` something CNA adds to XNA

## 2. Maths for graphics programmers (floor 1)

`room:vec-dot` then `room:vec-cross`, `room:vec-reflect`, `room:mat-order` (why matrix multiplication order matters),
`room:mat-hierarchy`, `room:mat-view`, `room:mat-perspective`, `room:quat-slerp`, `room:curve-spline`, `room:col-ray-box`,
`room:col-frustum`, `room:vt-packed` (what a vertex format really stores).

## 3. Rendering from the first triangle (floors 3, 4, 5)

`room:geo-triangle` (winding) → `room:geo-indexed` → `room:geo-primitives` → `room:tex-mip` → `room:tex-address` →
`room:be-combos` → `room:be-perpixel` → `room:be-fog` → `room:light-types` → `room:rs-depthfunc` → `room:rs-blend` →
`room:rs-portal` (stencil) → `room:rt-mirror` → `room:rt-mrt` → `room:cx-shadow` (shadow mapping).

## 4. 2D and text (floor 2)

`room:sprite-basics`, `room:sprite-origin`, `room:sprite-sheet`, `room:sprite-blend`, `room:sprite-sort`,
`room:sprite-sampler`, `room:sprite-scissor`, `room:sprite-camera`, `room:text-align`, `room:text-measure`.

## 5. Shaders, models and animation

`room:fx-grade`, `room:fx-ripple`, `room:fx-crt`, `room:cx-package` (one shader, several languages), `room:model-pbr`,
`room:model-ibl` (image-based lighting), `room:model-material`, `room:model-skinned`, `room:model-morph`,
`room:an-gears`, `room:an-walker`.

## 6. Input, audio and content (floors 7 and 8)

`room:in-keyboard`, `room:in-mouse`, `room:in-pad` (turn on the keyboard-emulated pad), `room:in-text`, `room:in-touch`,
`room:au-basics`, `room:au-spatial`, `room:au-synth`, `room:ct-load`, `room:ct-formats`, `room:ct-pipeline`,
`room:ct-failure`, `room:ct-lifetime`.

## 7. Does my renderer work? (floor 9)

`room:pl-renderer` (what this renderer says it can do), then every `room:rv-*` room (self-checking pixel tests, one per area),
`room:pf-dashboard` and the stress rooms `room:pf-objects`, `room:pf-sprites`, `room:pf-state` (raise the stress level slowly),
`room:vr-index` (named reference views), and `room:rf-deck` for a rest in the open air.
