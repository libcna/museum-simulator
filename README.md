# CNA 3D Museum

A first-person 3D museum that showcases the [CNA](../cna) framework (a C++ port of the
XNA/MonoGame API). Each room demonstrates one area of the API and is interactive.

## Rooms

| Room | Demonstrates |
|---|---|
| Entrance Lobby | start of the tour |
| Information Hall | what CNA is, the renderer in use, controls |
| Model Gallery | `ContentManager.Load<Model>`, world/view/projection, matrix transforms |
| Materials & Textures | `Texture2D`, samplers, addressing, mipmaps, UVs |
| Lighting Room | `BasicEffect` lights, per-pixel lighting, specular, emissive, CNA point/spot lights |
| Render State Room | `DepthStencilState`, `RasterizerState`, `BlendState` |
| Render Target Room | `RenderTarget2D` (a live security-camera screen) |
| Particle Room | dynamic geometry: fire, smoke, sparks, fountain |
| Animation Room | matrix hierarchies: robot arm and orrery |
| Sky Courtyard | sky dome, day-night cycle, sunlight |
| Audio Room | `SoundEffect`, 3D positional audio |

Rooms sit on a 3x4 grid, 20 m apart; the lobby is at the origin looking -Z.

## Build

Requires CNA as a sibling checkout (`../cna`), CMake, Ninja and ccache.

```bash
tools/build.sh                # incremental build in build/, prints only diagnostics
build/bin/cna-3d-museum       # run (WASD + mouse, E interacts, F1 help)
```

Run `build/bin/cna-3d-museum --help` for options (`--room`, `--pos`, `--no-audio`,
`--screenshot`, `--script`, ...).

`tools/shot.sh` and `tools/run.sh` render on a private virtual X display for screenshots
and scripted tests. `tools/gen_assets.py` and `tools/gen_font.py` regenerate the
procedural textures, models, sounds and fonts (Python + Pillow).

## Layout

- `src/World` — room definitions, museum shell, collision
- `src/Exhibits` — one file per room
- `src/Rendering`, `src/Audio`, `src/UI`, `src/Interaction`, `src/Camera` — engine-side helpers
- `Content/` — generated textures, glTF models, WAV sounds, fonts
- `HANDOFF.md` — status and CNA findings for whoever continues the work

## License

MIT, see [LICENSE](LICENSE). The bundled DejaVu font keeps its own license
(`Content/Fonts/LICENSE-DejaVu.txt`).
