# decor / batch03 checkpoint: hanging decor and lights

Target: `art/review/concept/target.png`. I checked the porch string lights (top-left porch), the bunting and wall lantern (right porch), the wreath (right door) and the street lamps (mid-street and foreground).

## Assets (all pass `python art/tools/gates.py`, all published)

| id | path | tris / budget | origin | sockets |
| --- | --- | --- | --- | --- |
| string_jack | props/string_jack | 146 / 150 | top of the black lamp socket (wire hang point) | light (globe centre, 0.077 m below the origin) |
| bunting_flag_orange | props/bunting_flag_orange | 48 / 60 | top-centre of the hem (the cord runs along X) | none |
| bunting_flag_purple | props/bunting_flag_purple | 48 / 60 | same | none |
| bunting_flag_cream | props/bunting_flag_cream | 60 / 60 | same | none |
| door_wreath | props/door_wreath | 892 / 900 | back centre, touching the door (ring centre at origin height), front -Y | none |
| lamp_post | props/lamp_post | 1172 / 1200 | ground | light (0, 2.92, 0 Godot) |
| porch_lantern | props/porch_lantern | 590 / 600 | wall mount, sticks out to -Y | light (glass centre, 0.21 m out, 0.17 m below) |

## Scripts

- `decor_common.py`: imports `yard_street/batch02/yard_common.py`. Adds `raw()` (explicit verts and faces with per-face palette/material), `lantern4()` (shared four-sided Victorian lantern head with framed two-tone panes, used by lamp_post and porch_lantern) and `report()` (triangles per part).
- `build_lamps.py`: lamp_post, porch_lantern. `build_decor.py`: string_jack, the bunting flags, door_wreath.
- `set_palette_row.py`: rewrites palette row 10 only, then runs make_palette. `register.py`: adds/updates the catalog entries.
- `view.py`: copy of view_gltf.py with a per-view focus point. Pass absolute paths, because relative output paths resolve against Blender's cwd.
- `porch_test.py`: Workbench mock-up of a porch (catenary jack string, bunting, wreath on a door, wall lantern, street lamp). Renders are in `porch_mockup_near.png` and `porch_mockup_15m.png`.

Run: `blender --background --factory-startup --python art/team/decor/batch03/build_decor.py -- [ids]`.

## Palette (row 10)

`bunting_cream:#f6ead0`, `glow_jack_shell:#a0400c:0.08`, `glow_jack_face:#ffd864:0.05`. The shell glow is deliberately dark. Brighter shells (#ea6a1c, then #c24c10) bloomed to yellow-white in the engine and swallowed the face. At #a0400c the globe stays orange and the pale face reads, as in the concept.

## Shared tooling change

`art/tools/gates.py`: a catalog entry with `"floating": true` now skips the ground-origin gate. The old rule was a hard-coded ID list. All five new hanging assets set it.

## Iterations

- **lamp_post**: the old one was a shepherd's crook. I rebuilt it as the concept's post-top lantern: octagonal plinth, bell base, fluted shaft (8 soft ribs), mid ring, collar with four scroll brackets, four-sided tapered lantern (wider at the top), roof with flared eaves and a small cupola, ball and spike finial. Total height 3.49 m.
  - Pass 1 was 1506 tris. I cut the bell bands and segments and dropped the scroll-end balls.
  - Pass 2: closed the gap between the head and the collar, and changed the roof from a flat pyramid to a flared roof with a cupola.
  - Pass 3: thickened the shaft and bell for toy proportions.
  - Engine: the panes glow strongly (amber rim, pale core) and the mullions stay visible. A 20 m downscale matches the concept's mid-street lamp in proportion and read.
- **porch_lantern**: the old one was a squat box. It is now a wall plate with a scroll arm and hook, and a hanging four-sided lantern (same head as the lamp) with wide flared eaves and a drop finial, like the concept's right-porch lantern.
  - The scroll arc was too angular at 6 points, so I raised it to 8.
  - I dropped the second boss for the budget.
- **string_jack**: ribbed pumpkin globe (grooves calmed on the front so the face decal sits flush), triangle eyes and a grin, green stem, black socket.
  - The shell colour took three passes (see Palette).
  - The globe went from 10x5 to 12x5 to 10x6. 10x6 has the roundest silhouette within budget.
- **bunting**: grid pennant with a sine wave that grows toward the tip, separate front and back skins 2.4 mm apart, and a rolled hem.
  - Pass 1 had back-skin faces with flipped winding. Orientation now goes by skin index, not position.
  - The bat outline was redrawn twice: the first read as a moustache or crown. It now has ears, a head and scalloped wings, on both skins.
  - The cream flag uses 2 rows instead of 3 to fit the bat in 60 tris.
- **door_wreath**: squashed twig torus (back faces removed), dark filler leaves, maple and teardrop leaves swirling round the ring, berry octahedra, puffy purple bow.
  - Pass 1 leaves read as Christmas stars (deep notches plus raised centres). Shallow notches and less cup fixed it.
  - The bare ring showed through, so I added mid-tone fillers.
  - The thin outline bow loops became puffy filled loops.

## Weak or unresolved

- string_jack: at grazing side angles the face decal edges stand off the shell by a few mm. The globe silhouette is still slightly polygonal up close. The concept's bulbs look bigger than 0.14 m (about 0.25 m against the 2.2 m door).
- door_wreath: brighter and more orange than the concept's darker, chocolate-toned wreath. The leaves are single-sided, so it is nearly invisible from behind (fine on a door). In the engine the leaves show some specular glints.
- bunting: the hem is very thin and invisible at distance. The flags are paper-thin edge-on. The bat on the cream flag is small (about 0.16 m span) and reads as a dark mark beyond about 10 m.
- lamp_post: the plinth chamfers catch a bright white rim highlight in the engine (toon spec and rim). The catalog collision (trunk r 0.12, h 3.2) is unchanged, while the plinth is 0.26 m in radius.
- The engine preview drops floating assets onto the grass, so I judged the hanging assets in Workbench (`view.py`, `porch_test.py`) for placement and in the engine for glow and colour only.
