# Trick or Treat

A cozy low-poly 3D collect-a-thon. A kid in a teal dragon costume trick-or-treats across an
endless, procedurally generated Halloween night: neighborhoods and small towns linked by country
roads through rolling, wooded hills. Every settlement is infested. Breathe fire to destroy the
monster nests and burn away the thorn vines, and the area's Lantern Gate wakes up, so you can
teleport between every place you have freed. Candy from doorbells, platforming runs and defeated
monsters buys upgrades, up to a magic ride-on car.

Godot 4.7 with the world streamer, generators, player, monsters, camera, candy and game loop in
GDExtension C++.

## Build and run

Requirements (as on the dev machine): Godot 4.7 standard build, MSYS2 MinGW-w64 g++ (`C:\msys64\mingw64\bin`),
Python 3 with SCons, Blender 5.1 for assets.

```powershell
native\BuildNative.ps1                 # first run clones the pinned godot-cpp and builds it (several minutes)
& "C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64.exe" --path game          # play
& "C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64.exe" --path game -e       # editor
```

### Build a Windows release to share

Run `BuildRelease.bat` (or `tools\build_release.ps1` from PowerShell). It builds the release
GDExtension, exports the game with the standard Godot 4.7 templates, and checks that the standalone
build can generate both a neighborhood and a town with all published models. Share the resulting
`builds\windows\Halloween - Trick or Treat!.exe` by itself. On launch it unpacks the game and native
library to a temporary folder, removes them when the game closes, and keeps saves in Godot's usual
user data folder.

Godot's Export button alone produces an EXE **and** a separate native DLL; both are required when
running that unbundled export. Use the release builder when you want one shareable EXE.

## Controls

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Move | WASD | Left stick |
| Jump / wing flap (again in the air) / glide (hold) | Space | A |
| Fireball (homes in on a nearby monster, else what is ahead) | F, R or left click | Y or RT |
| Tail spin (wider pickup, swats monsters, small hover in the air) | E or J | X |
| Roll (ground) / ground pound with a fire ring (air) | Shift or K | B |
| Hanging from a ledge: climb up / let go | Space / Ctrl | A / B |
| Candy shop | Tab | Start |
| Summon / leave the Pumpkin Cruiser (once bought) | V | LB |
| Switch to the next costume found | T | RB |
| Orbit camera | Q / C, or right-mouse drag | Right stick |
| New world | N | Back |
| HUD / debug panel / lightning toggle | H / F3 / L | |

Stepping onto a doormat rings the bell: the door swings open and treats spray into the yard. When
every candy of a house is collected its porch lanterns light up. Getting hit spills some of the
candy you carry; grab it before it blinks away (it never lowers an area's "candy found" count).
Step onto a freed area's Lantern Gate pad (beside the side street, inside the lantern arch) to
travel to any other freed place.

Jump at a ledge that's just out of reach (a porch or garage roof, a deck) and the kid catches the
lip and hangs there. Trampolines are scattered through the neighborhoods: big backyard ones throw
you five metres up (nearly eight if you hold jump, or press it just as you land), and little ones
sit beside roofs and other things worth climbing.

## The world

- **Regions.** Space is a grid of 200 m regions, each a neighborhood (40%), a small town (25%) or
  woods (35%); the start is always a neighborhood. Everything is deterministic from the world seed.
- **Roads.** Country roads join neighbouring regions street end to street end (or through a woods
  junction), curving and following the big swells, with gravel shoulders and street lamps. They
  leave a settlement exactly as wide as its street and narrow to country roads over about 10 m.
- **Terrain.** Rolling hills from layered gradient noise, flat-shaded in 2 m cells, levelled into a
  plateau under each settlement and eased flat along the roads. A coarse horizon ring carries the
  hills out into the fog.
- **Grass.** Clumps cover every grassy surface: lawns, street verges, pocket parks, the open ground
  around settlements and the meadows (more sparsely the forest floor), tinted to the ground below.
- **Woods.** Layered pines, maples, bare spooky trees, bushes, mossy rocks, stumps, fallen logs and
  glowing mushrooms, scattered by a density field, never on roads or in settlements.
- **Towns.** A Main Street of continuous storefronts (bakery, candy shop, toy store, cinema...) with
  striped walkable awnings, lettered sign boards, flat roofs and water towers to climb, wide paved
  sidewalks and a town square with a gazebo.
- **Streaming.** Terrain chunks (48 m, with collision) stream within 170 m; settlements generate
  when you come within 95 m of them (about 30 ms) and free themselves when you leave. Progress in
  each area (candy taken, doors rung, nests and vines destroyed, freed) survives unloading.

## Monsters

Each settlement has 4 to 6 nests (from a mix of three or four kinds). Nests keep replacing the
monsters you defeat until they are destroyed; a soft beacon of the monster's colour rises from
every live nest so it can be found. Monsters wander, then wobble their way toward you and wind up
(flashing) before simple attacks.

Each settlement also has two or three thorn nests: huge bramble masses around a beating purple
heart, marked by a purple beacon. Thorn vines creep out from them over lawns, sidewalks and
streets, bristling with big thorns, and one reaches the nearest front door and knots into a thorn
cluster in front of the doorbell. Vines hurt to touch: jump over them or burn them. When a thorn
nest burns, its vines wither and burn back outward from it. The area is freed once every monster
nest and thorn nest is gone.

| Monster | Nest | Attack |
| --- | --- | --- |
| Pumpkin Hopper | Cracked Pumpkin | hops in and body-slams |
| Bat | Bat Belfry | circles overhead, then dives |
| Rattlebones | Crooked Graves | keeps its distance and throws bones |
| Ghost | Haunted Lantern | floats through walls, lunges "boo" |
| Candy-Corn Gremlin | Gremlin Burrow | fast skitter, bite lunge |
| Mummy | Sarcophagus | slow, long grab |
| Goo Slime | Bubbling Cauldron | bounces about, lobs goo |
| Scarecrow | Crow Post | charges in a straight line, then gets dizzy |
| Spider | Web Nest | leaps in from a distance |
| Toadstool Imp | Fairy Ring | swells up and puffs spores around itself |



## Upgrades (candy shop)

Open the shop with Tab / Start anywhere, or step onto the doormat of the candy cart that stands
across the side street from every settlement's travel gate.

Hotter Flames (3 levels), Quick Breath (2), Triple Flame, Bigger Blast (ground-pound ring, 2),
Bucket Lid (spill less, 2), Candy Magnet, and the Pumpkin Cruiser: a ride-on toy car with
point-and-go steering, a hop, headlights and a ram that bowls small monsters over.

## Command-line review modes

Arguments after `--` are read by `FestivalGame`:

```powershell
Godot --path game -- --seed=101                            # fixed world seed
Godot --headless --path game -- --sweep=60 --seed=500      # validate 60 settlements (alternating neighborhood / town)
Godot --path game -- --capture-splash=splash.png           # capture the startup logo
Godot --path game -- --capture=menu.png --capture-view=menu # capture the main menu
Godot --path game -- --seed=101 --capture=shot.png --capture-view=play|overview|lot:3|nest:0|gate|shop|"prop:ID#K"|"at:x,z"|"atover:x,z" [--overview-dist=20]
Godot --path game -- --seed=101 --ledge-test=2 --capture=hang.png --capture-frames=9999   # jump at a roof edge, hang (screenshot), climb
Godot --path game -- --seed=103 --capture-view=prop:trampoline_large --drop-test --capture=x.png   # bounce height
Godot --path game -- --seed=101 --autopilot=120            # review bot: chases candy, fights, reports rate, hits, kills, fps
Godot --path game -- --seed=101 --fly-test=40              # streaming stress test: glide east at car speed, log worst frames
Godot --path game -- --fx-test | --open-shop --wallet=400 | --give-car | --free-home | --travel-test   # review aids
tools\review.ps1 -Seeds 101,102 -Views play,overview       # build + capture into art/review/captures
```

## Layout

| Path | What |
| --- | --- |
| `native/src/world/` | `WorldPlan` (regions, roads, terrain height), `WorldStreamer` (chunks, woods, horizon, settlement loading), procedural woods kit |
| `native/src/gen/` | Settlement generator: street, lots, households, houses, shops and town square (`town.cpp`), yards and props, landmarks, candy runs, traversal validation |
| `native/src/play/` | `FestivalGame`, `DragonController`, `FestivalCamera`, `CandyField`, `LooseCandy`, `Combat`, monsters/nests/vines (`monsters.cpp`, `monster_art.cpp`), `RideOnCar`, `StormSky` |
| `native/src/mesh/` | Flat-shaded palette mesh builder (shares the palette with the Blender kit) |
| `game/` | Godot project: scene, shaders (toon, candy, halo, sky), HUD, input setup, synth audio |
| `art/` | Asset pipeline: palette, Blender low-poly kit, catalog, gates, worker batches, review sheets ([art/README.md](art/README.md)) |

## How the generator works

Ten stages, all deterministic per seed (PCG32, seed tree seed -> stage -> lot):

1. Seed and theme, moon placement.
2. Street spline with a T-junction side lane; lots 14 to 19 m wide on both sides.
3. Households: 2 or 3 landmarks from a pool of 9 go to the most visible lots; every other lot gets one of 12 personas.
4. Parametric houses: storeys, steep gable roofs, porches with rails and roofs, garages, glowing windows, swinging doors, fences that match or contrast with the neighbors.
5. Lot routes planned first (a loop with two entrances, a porch-roof branch, a landmark summit) so props keep clear of them.
6. Contextual props: candidate, score, weighted pick of the top three, using persona, zone, anchor, facing, clustering and co-occurrence rules.
7. Street rhythm: street trees and lamp posts with driveway gaps, parked cars.
8. Candy: plenty of ground candy in loose clusters around every yard and along the sidewalks, plus
   jump candy in rhythm groups laid along the real jump trajectory (simulated with
   the controller's own movement constants, `core/movement.h`): runs onto bales, rails, crates, car
   and porch roofs, chains between platforms, bonus pieces above them, rooftop climbs and hopscotch
   hops. Guidance is a swirling magic trail down the street that branches to every doorstep and
   fades once that bell is rung.
9. Traversal validation: jump-arc reachability over every walkable surface; unreachable candy is lifted or dropped and the openness checks run.
10. Dressing: grass, clover and leaf litter scatter.

Generation takes about 10 to 30 ms per neighborhood.

## Platforming design

Grounded in published platformer design work:

- **Jump from designed metrics** (Pittman, "Building a Better Jump", GDC 2016): gravity and launch
  speed are derived from a 1.9 m jump height and 0.37 s to apex; the fall is 1.6x heavier; releasing
  jump early cuts the rise for short hops.
- **Forgiveness** (Thorson, "Celeste & Forgiveness"): coyote time, jump buffering, half gravity at
  the apex while jump is held, ceiling-edge correction, and a ledge-lip pop when a jump clips the
  top of a ledge. The candy magnet widens horizontal reach but height still has to be earned.
- **Ledge grab** (Super Mario 64, Spyro): a jump that comes up just short of a ledge catches it;
  jump climbs up, drop lets go. Only real ledges count (room to stand on top, a lip within arm's
  reach), so fence rails and thin trim never snag the kid, and a jump still rising fast enough to
  clear the ledge lands on it instead.
- **Trampolines** give the neighborhoods a vertical toy: backyard trampolines launch at 17 m/s and
  mini ones (14 m/s) are parked a step away from porch and garage roofs. Holding jump, or pressing it
  within a quarter second before landing (or a moment after), springs 1.45x higher; releasing jump
  never cuts a bounce short.
- **Monsters path around obstacles**: each settlement has a 0.6 m walk grid (houses, fences, hedges,
  porches and big props blocked); monsters follow A* routes, and nothing walks through houses.
- **Fireball aim**: heavy lock-on only within about four body lengths (monsters, then spawners and
  thorn nests, then vines); out to about ten, only targets already roughly ahead get a nudge.
- **Readable landings** (Iwata Asks: Super Mario 3D Land): a drop shadow straight below the kid,
  and a small glow pool under every floating candy, so the spot to jump from is visible.
- **Unambiguous jumps** ("Lessons from Suzy Cube: Measuring Up"): landings within 0.3 m of the
  single jump's peak are planned as wing-flap jumps instead of "maybe" jumps.
- **Rhythm groups with rests** (Smith et al., Launchpad): each candy run is one jump's beats, evenly
  spaced in time and at least 4 m from the next run.
- **Introduce, develop, twist, conclude** (Hayashida, Super Mario 3D Land): each lot starts with its
  lowest platform, chains higher, hangs a bonus above a platform top and ends on the porch roof.
- **No jumping for stairs**: walking into a step or kerb up to 0.45 m lifts the kid onto it (the
  body eases up so it never pops).
- **Feedback proportional to success** (Kao 2020; Kao et al. 2024): no per-candy pitch climb or
  wobble; a small combo meter (x13) beside the candy counter counts candy collected in quick
  succession, and sweeping up a whole run cleanly earns a chime and a burst of sparkles.

## Effects

Particle art is designed and rendered in Blender (`art/tools/make_vfx.py`): a four-point twinkle
star, a soft glow, a star-dotted ring, and 4x4 flipbooks of a cartoon smoke poof, a flickering
flame and a magic swirl, plus little star, heart, candy-corn and confetti meshes.
`art/tools/assemble_vfx.py` packs them into `game/assets/vfx/`, and `play/vfx.cpp` turns them into
pooled effects. Candy leaves a sparkle trail in its own colour as it flies to the bucket, and the
pickup grows with the candy: a glow pop and twinkles for plain candy, a magic swirl for chocolate,
a star ring and confetti for lollipops, and a sparkle fountain for the golden bar. Fireballs trail
flipbook flames, the ground-pound ring bursts into flames, defeated monsters go up in tinted
smoke, and celebrations throw star, heart and candy-corn confetti.

```powershell
& "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" --background --factory-startup --python art\tools\make_vfx.py
python art\tools\assemble_vfx.py
```

## Look

Textured low poly: 28 hand-painted, seamless material layers (`art/materials/`, AI-painted with
provenance, processed by `art/tools/make_materials.py` into texture arrays) tinted per face by the
shared palette, with normal maps and baked vertex AO. Every mesh (Blender kit v2 and the C++ mesh
builder) encodes tiling UVs in UV and `layer + palette cell` in UV2. Rendering adds SSAO, SSIL,
volumetric fog halos around lights, AgX/ACES grading, a toy-lens depth of field, dense wind-swept
grass, fireflies and painterly sky clouds. `tools\asset_preview.ps1 -Id <id>` renders any asset
in-engine (night and studio light).

## Notes and deviations from the concept document

- Houses are built parametrically in C++ (`gen/houses.cpp`) rather than snapped from Blender kit
  pieces; props, candy, the dragon kid, clouds and bats come from the Blender pipeline.
- Authored assets face Blender -Y, which is glTF/Godot +Z (`MODEL_FRONT`).
- Audio is FM-rendered offline (`art/tools/make_sfx.py`, `make_music.py`) and played by `game/scripts/audio.gd`.
- Vertical slice scope: one theme (Pumpkin Patch Lane), 9 of the 12 landmarks, no hub or co-op yet.
- Settlements sit on level plateaus that blend into the rolling hills (houses, streets and the
  platforming metrics stay exact); everything between them rolls.
- The night no longer ends with a festival bell: the open world is one endless night.
- Monsters, nests, vines, the car and the woods are built procedurally in C++ rather than in the
  Blender kit, in the same palette and painted materials.
