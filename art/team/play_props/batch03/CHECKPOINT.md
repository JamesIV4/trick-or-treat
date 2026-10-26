# play_props / batch03 checkpoint: trampolines

Both assets are built, exported, gated (PASS), registered in `art/asset_catalog.json` and published to
`game/assets/catalog.json`. Each one went through three build, preview and revise passes.

## Files

| File | What |
| --- | --- |
| `build_trampolines.py` | Builds both. It reuses `yard_street/batch02/yard_common.py` (sweep, lathe, rod, box, clean). |
| `register.py` | Adds or updates the two catalog entries. It re-reads the catalog right before writing. |
| `run.ps1` | Build, gate and preview: `powershell -NoProfile -File art\team\play_props\batch03\run.ps1 [ids] -Preview` |

## Assets

| id | export | tris | mat top (verified in the glTF) | catalog |
| --- | --- | --- | --- | --- |
| trampoline_large | `game/assets/models/props/trampoline_large.gltf` | 5710/6000 | 0.850 | box, walkable, collision_top 0.85, bounce 17.0 |
| trampoline_mini | `game/assets/models/props/trampoline_mini.gltf` | 1388/1500 | 0.280 | box, walkable, collision_top 0.28, bounce 14.0 |

- **trampoline_large**: pad outer radius 1.868 m, top 0.91 m. The mat is cat_black denim weave with a
  bone-stitched witch_purple border. The prints are a jack-o'-lantern (pumpkin, stem_green, ink face)
  inside an orange target ring, plus six faint bat_fur bats. There are 40 helical steel springs (3-sided
  wire) and an orange vinyl pad with alternating pumpkin and pumpkin_light panels and a purple skirt. It
  has six curved U/W steel legs with iron T-sleeves, and a two-step ladder at -62 deg (front-right). The
  ladder pushes the glTF AABB to +Z 2.056 (Godot front).
- **trampoline_mini**: pad outer radius 0.588 m, top 0.314 m, feet to r 0.614. It has an ink mat with a
  candy_lemon star and ring, a painted spring band (64 silver and charcoal columns), and a candy_violet pad
  with a witch_purple skirt. Six bowed iron legs end in candy_green rubber feet. There is no separate frame
  ring because the skirt hides it.

## Technique notes

- The prints are flat decal polygons above the mat: large +4 mm (pumpkin, bats), +8 mm (stem), +10 mm (face);
  mini +2 mm. They are a child node `mat_print`. AO is baked on the body *without* the prints, so the mat
  doesn't darken under them. The prints then get their own AO pass (no occlusion).
- The face/stem offset was 3 mm in pass 2 and was raised to 6 mm over the pumpkin to keep it clear of
  z-fighting at grazing angles. The pumpkin face looks "broken" at yaw -30/-62 only because it is a
  printed face seen rotated and foreshortened (checked top-down and front).

## Passes

1. First build (6478 / 1628 tris, over budget). Springs went from 2.5 to 2 coils, the T-sleeves lost
   their chamfer, the mini's frame ring was dropped and the mini spring band was halved.
2. The large mat went from spider_plum to cat_black, because in-engine it read mid-purple and the orange
   target lacked contrast. The pumpkin print was enlarged (R 0.5 to 0.54) and the ring moved out. The
   mini legs went from straight white rods (they read as table legs) to bowed iron legs with lime feet.
3. The large face prints were raised. The mini spring band went to 64 finer silver/charcoal columns
   (before, it read as a piano-key strip), the mini ring was thickened (0.29 to 0.335) and the legs were
   thickened.

## Weak or unresolved

- The large springs are 1 cm wire. From the gameplay camera they read as a grey band, not as coils.
- The mini spring band is painted, not modelled. Close up it still reads a bit like a film-strip border.
- The pumpkin face is directional (upright from -Y). From the sides it reads as a rotated face, though the
  ring and disc still read as a target.
- The ladder rails tuck under the pad skirt rather than hooking visibly over the frame. The ladder also
  widens the AABB if the box collision is taken from the mesh bounds.
- The pad rises about 6 cm (large) and 3 cm (mini) above `collision_top`, so feet sink slightly into the
  pad if the player stands on the rim.

---

# fire_wand (dragon kid's left-hand fire wand)

Built, exported, gated (PASS 566/600), registered (lane `characters_candy`, floating, collision none,
sockets `["tip"]`) and published.

| File | What |
| --- | --- |
| `build_wand.py` | `blender --background --factory-startup --python art\team\play_props\batch03\build_wand.py` |
| `register.py fire_wand` | Adds or updates only the fire_wand catalog entry (with no ids, it does every entry). |

- **Contract (verified in the glTF):** the wand points along Godot +Z (Blender -Y). The origin is the centre of
  the leather grip. `socket_tip` is a child of the `fire_wand` node at translation **(0, 0, 0.30)**, the
  centre of the flame crystal. The mesh spans Godot z -0.066 (pommel) to +0.362 (flame tip), so it is 0.43 m
  long, a bit longer than the 0.36 m brief, so the grip and the glowing tip both stay chunky. Its widest
  part is r 0.039 at the flame.
- **Build:** one merged mesh. It has a gold pommel ball, a leather grip with tilted wrap rings (leather,
  with cinnamon strap turns and bark_dark bindings), a gold collar, and a twisted three-lobed shaft (6
  sides, 210 deg twist, bark/wood_dark strands, bark material at 5x UV density). A darker
  (foil_gold_dark) cup with three scallops holds a faceted flame crystal: 9 segments, three swirling
  lobes, 70 deg twist, and a tip curling toward Godot +Y. AO is baked without the ground (radius 5 cm).

## Passes

1. First build. The silhouette and twist read well, but the flame read as a lemon-yellow blob and the
   grip wraps were invisible.
2. Shaft radius x1.12, cinnamon strap turns on the grip, bigger cup scallops, a stronger tip curl, and
   the flame recoloured mostly to glow_jack. It still read yellow: emission is cell x 2.6, then tonemapped.
3. Tried glow_lantern_red for the tongues. It reads peach-white in engine, so it was rejected.
4. Switched the tongues and tip to **glow_jack_shell** (the jack-o'-lantern shell cell). The flame now reads
   as fire: a yellow core (glow_jack_hot / glow_jack), with orange licks running down the lobe crests and an
   orange tip.
5. The cup went to foil_gold_dark so it separates from the yellow flame core.

## Weak or unresolved

- The glTF uses glow_jack_shell (dim cell, flicker 0.08) for the orange parts, not glow_candle. The game's
  flicker therefore comes mostly from the glow_jack and glow_jack_hot bands (flicker 0.6).
- The grip wraps are subtle, but the hand covers most of them in game anyway.
- The 0.43 m length is set by the contract (grip centre at 0, tip socket at 0.30). If the game wants a
  0.36 m wand, scale the node by about 0.85 and the socket moves with it.
