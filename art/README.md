# Art pipeline

Adapted from the OpenXcom-Extended-3D pipeline (orchestrator plus workers, assets as headless
Blender code, cheap gates, four-angle sheets), with a low-poly cutesy contract instead of the XCOM
realism rules.

## Folder layout

| Path | Owner | What |
| --- | --- | --- |
| `art/palette/palette.json` | root | The shared palette (names and colours) |
| `art/tools/` | root | `make_palette.py`, `lowpoly_kit.py`, `gates.py`, `publish_catalog.py` |
| `art/asset_catalog.json` | root | Every asset: id, path, triangle budget, collision, sockets, brief |
| `art/team/<lane>/<batch>/` | worker | Build scripts, `results.json`, `CHECKPOINT.md`, notes |
| `art/review/sheets/` | both | Four-angle review sheets (`<asset_id>.png`) |
| `game/assets/models/` | export target | `.gltf` + `.bin` per asset |
| `game/assets/catalog.json` | root | Published catalog the game reads (via `publish_catalog.py`) |

## Conventions

- 1 Blender unit = 1 m. Z up in Blender. The origin sits at the ground contact point (floating
  assets such as candy, clouds and hanging lanterns use their centre or hang point instead).
- The front faces Blender **-Y**, which becomes glTF/Godot **+Z** (`Node3D.MODEL_FRONT`).
- Colour comes only from the palette: each face's UVs sit on the centre of one palette cell.
  `glow_*` cells are emissive in game; their flicker amount lives in `palette_emit.png` alpha.
- Export: glTF separate (`.gltf` + `.bin`), Y-up, no materials. The game applies the shared toon
  material (`game/shaders/toon.gdshader`) to every mesh.
- Sockets are empties named `socket_<anchor>`; character parts are named child objects with their
  origin at the joint.
- Scale reference: the dragon kid is 1.1 m tall. A door is 2.2 m, a porch step 0.2 m.

## Quality contract

1. Reads instantly as a flat silhouette from the gameplay camera (35 degrees down, about 15 m away).
2. Palette only. Rounded and bevelled: visible facets are deliberate, never noise.
3. Bottom-heavy, toy-like proportions. Nothing sharp, scary or realistic. Halloween is cute.
4. Within the triangle budget; every required socket and part present; origin at ground.
5. Technical success is not art completion: passing the gates earns a review, not an approval.

## Commands

```
python art/tools/make_palette.py                       # rebuild palette textures + C++ header
"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" --background --factory-startup --python <script>
python art/tools/gates.py <asset_id> ...               # cheap gates
python art/tools/publish_catalog.py                    # publish passing assets to the game
```
