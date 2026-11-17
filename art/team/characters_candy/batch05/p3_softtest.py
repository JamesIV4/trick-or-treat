"""Engine test of the soft_* / bob_* path: dragon_kid with four throwaway spring parts, exported OVER
the game's dragon_kid so the showcase recorder can film it. No shipped costume has such parts yet,
so this is the only way to see KidRig's discovery, presets, shader bend and hem ripple in the engine.

  soft_tip_test    a floppy cone on top of the hood          (child of head, preset "tip")
  soft_cape_test   a scarf end hanging down the chest side   (child of body, preset "cape": a hem)
  bob_bell_test    a bell under the chin                     (child of body, preset "bell")
  bob_hat_test     a little cap on the back of the hood      (child of head, preset "hat": lifts in a fall)

ALWAYS put the real model back afterwards (p3_softtest.ps1 does both steps):
  blender --background --factory-startup --python art/team/characters_candy/batch05/build_costumes.py -- dragon_kid --nosheet
  python art/tools/publish_catalog.py

Run: blender --background --factory-startup --python art/team/characters_candy/batch05/p3_softtest.py
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kid_kit as KK  # noqa: E402
from kid_kit import K, V  # noqa: E402
import costume_dragon  # noqa: E402

K.reset()
parts, children = costume_dragon.build()
children = list(children)

tip_pin = V((0.0, 0.0, 1.17))
tip = KK.soft_part("soft_tip_test", [KK.limb("t", [tip_pin - V((0, 0, 0.03)), tip_pin + V((0, 0.03, 0.12)), tip_pin + V((0, 0.1, 0.22))],
                                             [0.045, 0.03, 0.014], color="candy_grape")], tip_pin)
cape_pin = V((0.1, -0.15, 0.62))
cape = KK.soft_part("soft_cape_test", [KK.limb("c", [cape_pin, cape_pin + V((0.02, -0.03, -0.12)), cape_pin + V((0.03, -0.03, -0.24))],
                                               [0.03, 0.04, 0.05], color="candy_orange")], cape_pin)
bell_pin = V((0.0, -0.17, 0.6))
bell = KK.bob_part("bob_bell_test", [KK.blob("b", bell_pin - V((0, 0, 0.04)), (0.032, 0.032, 0.032), color="foil_gold", mat="metal")],
                   bell_pin)
hat_pin = V((0.0, 0.2, 1.06))
hat = KK.bob_part("bob_hat_test", [KK.blob("h", hat_pin + V((0, 0.02, 0.05)), (0.07, 0.07, 0.05), color="candy_grape")], hat_pin)
children += [("soft_tip_test", tip, "head"), ("soft_cape_test", cape, "body"), ("bob_bell_test", bell, "body"),
             ("bob_hat_test", hat, "head")]
root, total, counts = KK.finish2("dragon_kid", parts, children, sheet=False, export=True)
print("SOFTTEST exported dragon_kid with test parts", total, {k: v for k, v in counts.items() if "_test" in k})
