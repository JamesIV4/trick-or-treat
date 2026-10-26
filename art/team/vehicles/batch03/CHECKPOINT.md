# vehicles / batch03 checkpoint

This batch rebuilds the decorative parked cars from scratch. The old `parked_car` was a single lofted blob with window panels painted on. All three cars pass `python art/tools/gates.py` and are published to `game/assets/catalog.json` with collision "box", walkable true, lane yard_street and a budget of 4500. The palette is unchanged: every colour is an existing cell, so no new row was added.

| id | size (bounds, m) | tris / budget | colours | preview |
| --- | --- | --- | --- | --- |
| parked_car | 1.91 (body 1.70) x 3.63 x 1.47 | 4420 / 4500 (4180 before the hood) | house_teal body, cream roof | `art/review/engine/parked_car.png` |
| parked_wagon | 1.97 (body 1.76) x 4.21 x 1.64 (roof 1.42) | 4670 / 4700 (4434 / 4500 before the hood) | metal_red body, wood_light panels in cream frames, cream roof | `art/review/engine/parked_wagon.png` |
| parked_pickup | 1.97 (body 1.76) x 4.07 x 1.60 | 4792 / 4800 (4492 / 4500 before the hood) | quilt_yellow body, trim_white roof, plank bed | `art/review/engine/parked_pickup.png` |

All three face -Y (Godot +Z) and have their origin at the ground centre, with the tyres touching z = 0. The width in the bounds includes the wing mirrors. The engine lineup (wagon, car, pickup side by side at gameplay distance) is `art/team/vehicles/batch03/lineup_engine.png`.

## How the cars are built (`car_kit.py`)

- **Lower body.** A side profile (filleted polygon with semicircular wheel-arch cut-outs) is lofted across X. Each vertex has its own rounding radius, set from the outline normal: about 0.22 m on the nose, 0.09 m on the top edges and 0.035 m on the arches. Because of this the flat side panels stay planar and flat-shaded, while the nose and tail are bubbly in plan. Arch ceilings and the underside are charcoal, so the wheels sit in dark wells. The bonnet has a slight crown and the nose and tail bulge in plan.
- **Pickup bed.** The same loft with extra rings: rail rings, then "lowered" rings for the bed floor and inner walls. A separate 12-quad plank floor gives the AO bake interior vertices. `finish_car(ao_lift=...)` lifts the AO inside the bed, which otherwise baked near-black.
- **Cabin.** Rounded-rectangle rings are lofted up Z with tumblehome, rake and an eased windshield curve. The glass bands are recessed 2.5 cm (shadow_navy, flat-shaded) with chrome reveal faces. The pillars are simply the un-recessed parts, in body colour. The roof is a contrasting colour with a rounded edge and a flat top cap. Each window gets one or two diagonal highlight quads in foil_silver, set 4 mm proud of the glass.
- **Details:**
  - chunky tyres (inner sidewall skipped) with a whitewall ring, body-coloured rim and chrome cap
  - lathe lamps (body-coloured pod, chrome bezel, domed lens)
  - an oval chrome grille with bars
  - raycast wrap-around chrome bumpers
  - door seams and a chrome rub strip as flat ribbons raycast onto the body
  - handles and wing mirrors
  - plates (BOO letters built from tori)
  - jack-o'-lanterns with glow_jack_face faces
- **Paint.** Body and roof use the "flat" material: glossy toy paint. The "metal" layer read as hairline brush streaks on large panels in the engine, even with UVs scaled down (pass 2).

## Scripts

| Script | What it does |
| --- | --- |
| `car_kit.py` | Shared builders, described above. |
| `build_cars.py` | Builds the three cars: `blender -b --factory-startup --python build_cars.py -- parked_car parked_wagon parked_pickup [--stats]`. `--stats` prints a per-part triangle breakdown (before cleanup). |
| `register.py` | Inserts or updates the catalog entries, re-reading the catalog right before it writes: `python register.py [ids]`. |
| `run.ps1` | Build, gate and optionally preview: `run.ps1 build_cars.py parked_car,parked_wagon -Preview [-Extra --stats]`. |
| `lineup.py` / `lineup.ps1` | Engine side-by-side view. It temporarily writes the trio over props/parked_car, previews it to `lineup_engine.png`, then rebuilds parked_car. |
| Close-up renders | Use `art/team/decor/batch03/view.py <gltf> <png> yaw:pitch:zoom:fx:fy:fz ...` (Workbench, tint x AO). |

## Iterations (engine preview each pass)

- **parked_car**
  - Pass 1: reads as a car, but the mint washed out to white, the cream greenhouse read as a tan box, the metal streaks looked hairy, and it was 5812 tris.
  - Pass 2: house_teal with body-coloured pillars and a cream roof only, lighter wheels and plates. Still 4550 tris and still streaky.
  - Pass 3: flat paint, a rounder and more raked cabin, chrome rub strip, 4438 tris.
  - Pass 4: a fifth rounding ring for a smoother nose, 4180 tris.
- **parked_wagon**
  - Pass 1: cream rims made the wheels read as white discs, and the roof pumpkins were tiny.
  - Pass 2: body-coloured rims, bigger cargo, budget cuts (two-step roof rounding, unbevelled feet and straps).
  - Pass 3: final.
- **parked_pickup**
  - Pass 1: white rims, and the cab looked low.
  - Pass 2: body-coloured rims, taller cab (belt 1.0, roof 1.61), plank floor grid.
  - Pass 3: the jack had been hidden below the tailgate, so it now sits on the bale. Its face shows over the rails, and the AO lift fixes the dark bed.

## Weak or unresolved

- The studio-light preview row washes the teal and yellow out to pastels. Under night lighting they read saturated.
- The paint is flat with no painted brush texture, so the cars are smoother and cleaner than the concept's painterly surfaces. This was deliberate (see Paint above).
- The pickup reads more like a car-based ute than a 1950s farm truck: it has no separate flared fenders or running boards, and the cab is still compact.
- Wing mirrors are small and read as dark slivers in side view. The car's antenna (a 4-sided rod, 8 mm) and its tiny jack topper are almost invisible at gameplay distance.
- BOO letters are about 8 cm tall and only legible up close. The other plates show dashes.
- The wagon's roof cargo lifts the collision box top to 1.64 m, about 0.22 m above the roof, so the player will stand slightly above the roof there. The mirrors widen the box about 0.1 m per side.
- Lamps are unlit, as in the original brief. Only the jacks glow.
- The budgets are nearly full for the wagon and pickup (66 and 8 tris spare), so further detail needs cuts elsewhere.

## Opening bonnet (hood) and engine bay

The game flaps the hood up when the player bounces on it, so each car's hood is now its own glTF node, named `hood`, a direct child of the root node. Its origin sits on the hinge line: the hood's rear edge, x = 0, at the hood's surface height. The game rotates the node about its local X axis, and a negative angle lifts the front. The closed car looks the same as before: the hood's top faces are the very same lofted body faces, and the sheets differ from the old ones only by a few hundred faint pixels (AO and seam shading).

| id | hinge y (Blender) | hood_hinge (Godot) | hood_aabb (Godot) | hood length (z) | tris / budget |
| --- | --- | --- | --- | --- | --- |
| parked_car | -0.90 | [0, 0.919, 0.90] | [-0.752, 0.749, 0.895, 0.752, 0.919, 1.574] | 0.68 m | 4420 / 4500 |
| parked_wagon | -1.10 | [0, 0.956, 1.10] | [-0.785, 0.814, 1.097, 0.785, 0.956, 1.861] | 0.76 m | 4670 / 4700 (was 4500) |
| parked_pickup | -0.75 | [0, 1.013, 0.75] | [-0.796, 0.868, 0.748, 0.796, 1.013, 1.794] | 1.05 m | 4792 / 4800 (was 4500) |

`register.py` writes `hood_aabb`, `hood_hinge` and `parts: ["hood"]` into the catalog from `hood.json`, which the build writes. `parts` makes the gate check that the node exists. `publish_catalog.py` now publishes `hood_aabb` and `hood_hinge`. The wagon and pickup budgets were raised because they were already full (66 and 8 tris spare). The hood, bay and engine add about 240 to 300 tris, and nothing visible could be cut without changing the closed look.

How it works (`car_kit.py`):

- **Hinge line.** `insert_outline_y()` puts one collinear vertex into the side profile at the hinge line, so the outline shape does not change. The hinge is a few cm in front of where the windshield base meets the body, so the buried cabin base stays under the cowl strip.
- **Hood split.** `split_hood()` takes the flat top of the loft, from the edge ring through the mid ring to the other edge ring, between the hinge vertex and the nose-fillet vertex just above the headlights. It makes those faces the hood: 3 cm thick, with a darker underside (roof_teal, roof_red or straw_dark) and body-colour rims. The fenders, grille, lamps and bumpers stay in the body.
- **Engine bay.** The bay is a tub: charcoal walls go from the hole edge down to a shadow_navy floor plane. The floor runs parallel to the bonnet and clears the front wheel-arch tunnels by 3 cm, so you never see the arch backfaces or a hole. `engine_bay()` adds:
  - a bevelled block toward the hinge, where there is most head room: pumpkin on the car, metal_blue on the wagon, witch_purple on the pickup
  - a chrome pie-pan air cleaner and an oil cap on the block
  - a chrome radiator tank with a cap at the front
  - two fat coloured hoses

  Every engine part is sized against the closed hood's underside with a 1.5 cm gap. A raycast check found no body geometry above the closed hood.
- **AO.** AO is baked twice. The first bake is closed, for the body and the hood top. The second has the hood open 50 degrees, for the bay and the hood underside, so the bay is not black when the hood lifts.
- **Open-hood check.** I checked the hood open at 55 degrees in the engine by temporarily adding the rotation to the .gltf, then restoring it: `<id>_hood_open_engine.png` in this folder.

Weak or unresolved:

- The car's hood is only 2 quads across and 2 rows long: it is the original loft, with the crown as a ridge. When lifted it reads as a fairly flat panel.
- The car's front bay is shallow (about 10 cm under the front edge) because the front arch tunnel sits right under it.
- The pickup's bay is long (1 m), so the hoses run a long way to the radiator.
