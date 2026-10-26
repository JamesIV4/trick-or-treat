# landmarks / batch03 checkpoint

This batch adds the travel gate (as two assets), the candy shop cart, and a fidelity pass on four street props. All seven assets pass `python art/tools/gates.py` and are published with `art/tools/publish_catalog.py`. Each asset has a Workbench sheet in `art/review/sheets/` and an in-engine preview in `art/review/engine/`. The palette is unchanged: everything uses existing cells, so I did not add row 15.

## Scripts

| Script | What it does |
| --- | --- |
| `lm_common.py` | Reuses `yard_street/batch02/yard_common.py` and adds `stroke()` (a flat tapered ribbon for runes and painted swirls), `rock()`, `sector_slab()`, `ribbed()` (pumpkins) and `export_many()` (one joint AO bake, then a separate export for each asset). |
| `build_gate.py` | Builds travel_gate_base and travel_gate_lantern. `-- --combined` also writes base and lantern merged into props/travel_gate_base, because the engine preview can only show one asset. That export has no AO and fails the budget gate, so always rebuild without the flag afterwards. `-- --stats` prints a triangle breakdown. |
| `build_cart.py` | Builds shop_cart. Accepts `-- --stats`. |
| `build_street.py` | Rebuilds mailbox, parked_car and flower_pot. The car reuses batch02 `build_car.py` with a new `wheel()` patched in. |
| `register.py` | Inserts or updates this lane's catalog entries. It re-reads the catalog just before writing. |
| `inspect_gltf.py` | Prints an export's bounds, the palette cells it uses (glow cells listed separately) and the bounds of chosen cells. |
| `run.ps1` | Build, gate and optionally preview: `run.ps1 build_gate.py travel_gate_base,travel_gate_lantern -Preview [-Extra --combined]` |

## Assets

| id | tris / budget | Notes |
| --- | --- | --- |
| travel_gate_base | 6310 / 7000 | Runed flagstone pad (r 1.3, top 0.22) with 8 bevelled outer slabs, 6 inner slabs and a medallion, a skirt of fieldstones, moss and fallen leaves. Carved rune grooves are stone_dark with no glow. A stacked stone pillar at x -1.08 carries a wrought-iron crook post (crown about 3.41 m, crescent-moon finial to 3.69 m) with scrolls and ivy. There are two runestones and two pumpkins. The asset has no glow_* cells (verified). |
| travel_gate_lantern | 2460 / 2500 | Hexagonal lantern with two tiers of glow_magic_teal panes, a flared roof with curled tips, a hanger ring and a teal drop crystal. Glass runs from y 2.27 to 2.83, so the centre is at (-0.15, 2.55, 0) in Godot. It also holds every switch-on glow: the channel ring (r 0.945–0.985), 8 runes, the centre star and ring, a crescent on the pillar and a glyph on each runestone. Its only colours are glow_magic_teal and lamp_black. `floating: true`. |
| shop_cart | 9178 / 9500 | Mint plank cart with cream trim, painted lollipop and candy panels, candy-corn side bands, wagon wheels and kickstands. Four candy-cane posts hold a pink/white awning with bulging stripes, a scalloped front and side valance with bobbles, and gold piping. A purple sign board with a gold frame has no text and is flanked by lollipop medallions. The counter holds 4 layered candy jars (two open, heaped with gumballs), a tray of wrapped candies, a cup of lollipops and a jack-o'-lantern (glow_jack face). A lantern hangs under the awning and there are 3 pumpkins. The round braided doormat is flat (top at 0.035) with a glow_lantern ring. |
| mailbox | 780 / 800 | The post is now painted trim_white to match the picket fences. The number plate is brass (caramel) instead of cream, which glared at night. The handle and pivot are gold. A hedge tuft, two daisies and a tiny pumpkin sit at the foot. Embossed ribs were tried but cost 180 tris that cannot be seen at gameplay distance, so I dropped them. |
| parked_car | 3952 / 4000 | Body unchanged. New 16-segment wheels with charcoal tyres and hubcaps that carry a whitewall ring, an iron band, a silver dish and a red button (the old 14/10-sided tyres and hubs read as white octagons). Headlight and mirror blobs are lighter to pay for this. |
| flower_pot | 800 / 900 | The three large "cauliflower" mum balls are replaced by five felt-textured pompom heads with petal bumps and dark undersides: orange, gold and mum_orange, plus one purple accent. They sit on a leafy cushion, with four pointed leaves spilling over the rim instead of the pill leaves. |
| bench | 1188 / 1200 | Reviewed in engine and left unchanged. The slats, scroll arms and iron frame already match the concept's level of polish, and the budget is full. |

## Integration notes (Godot coordinates, +Z = front)

- **Gate:** place both assets at `gate_pos_` with the same transform. The lantern glass centre is (-0.15, 2.55, 0) and the hang ring sits at y ≈ 3.16. The existing `gate_light_` at (-0.15, 2.5, 0) is fine. The pad has no physics body in C++. If the post should block the player, add a trunk collider at (-1.08, 0, 0), r ≈ 0.32, height 3.4. The runestones are at (1.23, 0, 0.86) and (0.70, 0, -1.32), about r 0.18.
- **Cart:** the mat is centred at (0, 0, 1.25) with r 0.75, flat, top at y 0.035, so `shop_pos_` is unchanged. The existing box collider (2.2 × 1.25 × 1.3 at y 0.62) still covers the body and wheels. The pumpkins at x ±1.1–1.3 are decorative and sit outside it. The hanging lantern's glass centre is (-0.62, 1.98, 0.70). The C++ OmniLight is at (-0.85, 1.9, 0.55), about 0.27 m away, so move it to the new position. The sign board's face is at z 0.66, spanning y 2.40–2.83 and x ±0.89. The current Label3D at (0, 2.62, 0.69) sits 3 cm in front of the face. A label at y 2.9 would sit above the board, but its lower edge may touch the arch peak at 2.83.

## Weak or unresolved

- From the 35° gameplay camera facing the front, the awning hides most of the counter goods. They read from the side, from the back and at low angles. The front panels carry the candy story from the front.
- Cart mat: its burlap rings look nearly emissive next to the glow ring at night. The colours are the same as the C++ version.
- The gate's iron post is thin relative to the 2.9 m pad when seen from far away. The runestones read as plain tapered posts. I only saw the lantern in the engine at full emission, never at the locked glow_boost of 0.12.
- Car: the body proportions (long hood, flat window panels) are unchanged, and the car is at 3952/4000.
- Flower-pot heads still show slight polygon silhouettes at close range. Mailbox flowers are tiny at gameplay distance.
