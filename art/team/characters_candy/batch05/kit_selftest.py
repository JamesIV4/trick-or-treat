"""Kit v2 self-test and usage example: a plain hooded kid built only from kit calls, wearing the kit's
sneakers (the path the five unlockable costumes take) with one soft_ and one bob_ part. Nothing is
exported; it renders art/review/sheets/kit_selftest.png, _poses.png and _joints.png.

Run: blender --background --factory-startup --python art/team/characters_candy/batch05/kit_selftest.py
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kid_kit as KK  # noqa: E402
from kid_kit import D, K, V, PLUSH  # noqa: E402

K.reset()
COL = "candy_grape"
parts = {"body": D.part([KK.torso2(COL)], "body", (0, 0, KK.HIP_Z))}
static, children = KK.face2(fang=False)
parts["head"] = D.part(KK.hood2(COL, rim_color="lolly_white", stitch_color="ink") + static + KK.fringe2(clip=KK.hood_clip()),
                       "head", KK.NECK)
for s, hip in (("l", KK.HIP_L), ("r", KK.HIP_R)):
    # the shin is the whole sneaker; its closed top sits on the knee, the thigh's ball inside it
    parts["leg_" + s], parts["shin_" + s] = KK.leg2("leg_" + s, hip, "ink", lambda x, s=s: KK.sneaker("leg_" + s, x),
                                                    thigh_mat="fabric", stripes=("candy_orange", 0.03))
cuff = ("lolly_white", PLUSH)
parts["arm_l"], parts["forearm_l"] = KK.arm2("arm_l", KK.SHO_L, KK.ELBOW_L, KK.WRIST_L, COL, cuff=cuff)
parts["arm_r"], parts["forearm_r"] = KK.arm2("arm_r", KK.SHO_R, KK.ELBOW_R, KK.WRIST_R, COL, cuff=cuff)
parts["hand_l"] = KK.grip_hand("hand_l", KK.WRIST_L, KK.WAND_GRIP, KK.WAND_DIR, (0.55, 0.83, 0.0), 0.0155, fat=1.12)
parts["hand_r"] = KK.grip_hand("hand_r", KK.WRIST_R, KK.BUCKET_GRIP, (1, 0, 0), (0.0, -0.45, 0.9), 0.012, fat=1.12)
# a soft part (one mesh, origin at its pin) under the head and a bob part under the body
tip_pin = V((0.0, 0.03, 1.06))
tip = KK.soft_part("soft_tip", [KK.limb("t", [tip_pin - V((0, 0, 0.02)), tip_pin + V((0, 0.05, 0.1)), tip_pin + V((0, 0.14, 0.14))],
                                        [0.05, 0.035, 0.016], color=COL)], tip_pin)
bell_pin = V((0.0, -0.17, 0.6))
bell = KK.bob_part("bob_bell", [KK.blob("b", bell_pin - V((0, 0, 0.035)), (0.03, 0.03, 0.03), color="foil_gold", mat="metal")],
                   bell_pin)
children += [("soft_tip", tip, "head"), ("bob_bell", bell, "body")]
root, total, counts = KK.finish2("kit_selftest", parts, children, sheet=True, export=False)
legs, arms = (0, 0, 0.2), (0, -0.04, 0.47)


def bend(P):
    for s in "lr":
        KK.pose(P["forearm_" + s], -1.2)
        KK.pose(P["shin_" + s], 1.2)
        KK.pose(P["leg_" + s], -0.5)


def limits(P):
    KK.pose(P["shin_l"], 1.5)
    KK.pose(P["shin_r"], -1.5)
    KK.pose(P["forearm_l"], -1.5)
    KK.pose(P["forearm_r"], 1.5)


KK.sheet_grid(root, "kit_selftest_joints", [(bend, -90, 5, 3.0, legs), (bend, -30, 10, 3.0, legs), (limits, -90, 5, 3.0, legs),
                                            (limits, 90, 5, 3.0, legs), (bend, -90, 5, 3.0, arms), (limits, 90, 5, 3.0, arms)],
              cols=3, size=640)
KK.sheet_grid(root, "kit_selftest_poses", [(None, -35, 15, 1.15, (0, 0, 0.56)), (None, 145, 15, 1.15, (0, 0, 0.56)),
                                           (bend, -35, 12, 1.15, (0, 0, 0.56)), (bend, -90, 5, 1.15, (0, 0, 0.56))], cols=4)
print("SELFTEST", total, {k: v for k, v in counts.items() if k.startswith(("soft_", "bob_", "shin", "leg"))})
