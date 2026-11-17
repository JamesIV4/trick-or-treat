"""Player costumes on kit v2 (rig contract v2: CHECKPOINT.md in this folder).

Each costume's builder lives in costume_<name>.py and returns (parts, children) for kid_kit.finish2().
Ported so far: dragon_kid. The other five still build from batch04 (v1) until they are ported here:
copy batch04/costume_<name>.py, switch it to the v2 kit calls and add it to BUILDERS.

Run: blender --background --factory-startup --python art/team/characters_candy/batch05/build_costumes.py
     -- [ids] [--nosheet] [--pieces] [--close] [--face] [--poses] [--noexport]
  --close   <id>_close.png (1024 px close views) and <id>_feet.png
  --face    <id>_face.png: the face at zoom 4.2 from four angles (alt_ parts hidden, as in game)
  --head    <id>_head.png: head and hair at zoom 2.7 from twelve angles (sides, back, high and low cameras)
  --poses   <id>_poses.png: the rig posed like the game poses it (elbows -1.2, knees +1.2, run, hang,
            cast, the joint limits) and <id>_joints.png (knees and elbows up close) to catch joint gaps before
            the engine; <id>_faces.png: half blink,
            blink, each mouth, the iris slide
  --noexport  build and render the sheets only (leaves game/assets untouched)
Sheets go to art/review/sheets/. Triangle counts go to build_log.json.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kid_kit as KK  # noqa: E402
from kid_kit import K  # noqa: E402
import costume_cat  # noqa: E402
import costume_dragon  # noqa: E402
import costume_ghost  # noqa: E402
import costume_witch  # noqa: E402
import costume_skeleton  # noqa: E402
import costume_pumpkin  # noqa: E402

ALL = ["dragon_kid", "kid_ghost", "kid_cat", "kid_witch", "kid_skeleton", "kid_pumpkin"]
BUILDERS = dict(dragon_kid=costume_dragon.build, kid_ghost=costume_ghost.build, kid_cat=costume_cat.build,
                kid_witch=costume_witch.build, kid_skeleton=costume_skeleton.build,
                kid_pumpkin=costume_pumpkin.build)
FACE_FOCUS = (0.0, -0.12, 0.805)
# = KidTuning::arm_out, elbow_rest in kid_rig.cpp: --poses also renders <id>_out.png in that default arm pose
ARM_OUT = dict(kid_ghost=(costume_ghost.ARM_OUT, costume_ghost.ELBOW_REST), kid_witch=(0.22, 0.15),
              kid_pumpkin=(costume_pumpkin.ARM_OUT, costume_pumpkin.ELBOW_REST))


# ------------------------------------------------------------------ poses (Godot Euler angles, as kid_poses.cpp)
def p_bend(P):
    for s in "lr":
        KK.pose(P["forearm_" + s], -1.2)
        KK.pose(P["shin_" + s], 1.2)
        KK.pose(P["leg_" + s], -0.5)


def p_run(P):
    KK.pose(P["leg_l"], -1.0)
    KK.pose(P["shin_l"], 0.25)
    KK.pose(P["leg_r"], 0.55)
    KK.pose(P["shin_r"], 1.5)
    KK.pose(P["arm_l"], -0.9, 0, 0.1)
    KK.pose(P["forearm_l"], -1.3)
    KK.pose(P["arm_r"], 0.45, 0, -0.14)
    KK.pose(P["forearm_r"], -0.5)


def p_hang(P):
    KK.pose(P["arm_l"], 0.1, 0, 2.5)
    KK.pose(P["arm_r"], 0.1, 0, -2.5)
    for s in "lr":
        KK.pose(P["forearm_" + s], -0.35)
        KK.pose(P["leg_" + s], 0.15)
        KK.pose(P["shin_" + s], 0.55)


def p_cast(P):
    KK.pose(P["arm_l"], -1.45, 0, -0.2)
    KK.pose(P["hand_l"], -0.3)
    KK.pose(P["arm_r"], 0.2, 0, -0.3)
    KK.pose(P["forearm_r"], -0.6)


def p_limits(P):
    KK.pose(P["forearm_l"], -1.5)
    KK.pose(P["forearm_r"], 1.5)
    KK.pose(P["shin_l"], 1.5)
    KK.pose(P["shin_r"], -1.5)
    KK.pose(P["hand_l"], 0.5)
    KK.pose(P["hand_r"], -0.5)


def f_blink(k):
    def go(P):
        for s in "lr":
            KK.pose(P["eye_" + s], scale=(1.0, 1.0 - 0.9 * k, 1.0))
            P["alt_lash_" + s].hide_render = k <= 0.6
            P["eye_" + s].hide_render = P["iris_" + s].hide_render = k > 0.6
    return go


def f_mouth(name):
    def go(P):
        P["mouth_smile"].hide_render = True
        P[name].hide_render = False
    return go


def f_look(dx, dy):
    def go(P):
        for s in "lr":
            KK.pose(P["iris_" + s])
            P["iris_" + s].location.x += dx
            P["iris_" + s].location.z += dy
    return go


def f_brows(P):
    for s, sg in (("l", 1), ("r", -1)):
        KK.pose(P["brow_" + s], 0, 0, 0.3 * sg)
        P["brow_" + s].location.z += 0.006
    f_mouth("alt_mouth_open")(P)


POSES = [(None, -35, 15), (None, 145, 15), (None, -90, 5), (None, 0, 8),
         (p_bend, -35, 12), (p_bend, -90, 5), (p_bend, 145, 15), (p_limits, -90, 5),
         (p_run, -35, 12), (p_run, -90, 5), (p_run, 90, 5), (p_limits, 150, 12),
         (p_hang, -20, 8), (p_hang, 160, 12), (p_cast, -90, 5), (p_cast, -35, 15)]
FACES = [(None, 0, 4), (f_blink(0.5), 0, 4), (f_blink(1.0), 0, 4), (f_blink(1.0), -40, 8),
         (f_mouth("alt_mouth_open"), 0, 4), (f_mouth("alt_mouth_o"), 0, 4), (f_mouth("alt_mouth_grit"), 0, 4),
         (f_mouth("alt_mouth_open"), -40, 8),
         (f_look(0.005, 0.0), 0, 4), (f_look(-0.005, 0.004), 0, 4), (f_brows, 0, 4), (None, -65, 6)]


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ids = [a for a in argv if not a.startswith("--")] or ALL
    out = Path(__file__).resolve().parent / "build_log.json"
    log = json.loads(out.read_text()) if out.exists() else {}
    for aid in ids:
        K.reset()
        parts, children = BUILDERS[aid]()
        root, total, counts = KK.finish2(aid, parts, children, sheet="--nosheet" not in argv, export="--noexport" not in argv)
        log[aid] = {"tris": total, "parts": counts}
        if "--close" in argv:
            K.review_sheet([root], f"{aid}_close", scale_figure=False, size=1024,
                           views=[(-28, 10), (150, 14), (0, 4), (-15, 32)], zoom=1.75, focus=(-0.03, 0, 0.6))
            K.review_sheet([root], f"{aid}_feet", scale_figure=False, size=768,
                           views=[(-35, 18), (145, 18), (-90, 6), (0, 10)], zoom=4.0, focus=(0, -0.02, 0.09))
        if "--face" in argv:
            K.review_sheet([root], f"{aid}_face", scale_figure=False, size=1024,
                           views=[(0, 4), (-32, 8), (32, 8), (-20, 35)], zoom=4.2, focus=FACE_FOCUS)
            K.review_sheet([root], f"{aid}_hands", scale_figure=False, size=768,
                           views=[(-20, 10), (160, 14), (-90, 6), (90, 6)], zoom=3.4, focus=(0, -0.05, 0.39))
        if "--head" in argv:
            # the head and hair from every showcase camera (front, 3/4, both sides, back, the high gameplay camera)
            hd = (0, 0.0, 0.82)
            KK.sheet_grid(root, f"{aid}_head", [(None, yaw, pitch, 2.7, hd) for yaw, pitch in
                                                ((0, 4), (-35, 10), (35, 10), (-90, 5), (90, 5), (180, 8), (145, 15),
                                                 (-20, 40), (0, -12), (160, 45), (-60, 25), (60, -5))], cols=4, size=512)
        if "--poses" in argv:
            KK.sheet_grid(root, f"{aid}_poses", [(fn, yaw, pitch, 1.15, (0, 0, 0.56)) for fn, yaw, pitch in POSES], cols=4)
            legs, arms = (0, 0, 0.2), (0, -0.04, 0.47)
            KK.sheet_grid(root, f"{aid}_joints", [(p_bend, -90, 5, 3.0, legs), (p_bend, 90, 5, 3.0, legs), (p_bend, -30, 10, 3.0, legs),
                                                  (p_limits, -90, 5, 3.0, legs), (p_limits, 90, 5, 3.0, legs), (p_run, -90, 5, 2.6, legs),
                                                  (p_bend, -90, 5, 3.0, arms), (p_bend, 90, 5, 3.0, arms), (p_bend, -30, 10, 2.6, arms),
                                                  (p_limits, -90, 5, 3.0, arms), (p_limits, 90, 5, 3.0, arms), (None, 150, 10, 2.6, arms)],
                          cols=3, size=640)
            KK.sheet_grid(root, f"{aid}_faces", [(fn, yaw, pitch, 4.4, FACE_FOCUS) for fn, yaw, pitch in FACES], cols=4,
                          size=640)
            if aid in ARM_OUT:
                # the rest pose as the game shows it: the costume's arm_out (kid_rig.cpp tuning row) applied
                ao, er = ARM_OUT[aid]

                def p_out(P, out=ao, el=er, walk=0.0):
                    KK.pose(P["arm_l"], walk, 0, out)
                    KK.pose(P["arm_r"], 0, 0, -out)
                    KK.pose(P["forearm_l"], -el)
                    KK.pose(P["forearm_r"], -el)
                tiles = [(p_out, -35, 12, 1.0, (0, 0, 0.5)), (p_out, 145, 12, 1.0, (0, 0, 0.5)),
                         (p_out, -90, 6, 1.0, (0, 0, 0.5)), (p_out, 0, 6, 1.0, (0, 0, 0.5)),
                         (p_out, 180, 8, 1.0, (0, 0, 0.5)), (p_out, -60, 25, 1.0, (0, 0, 0.5))]
                KK.sheet_grid(root, f"{aid}_out", tiles, cols=3, size=640)
                arms = (0, -0.03, 0.45)
                KK.sheet_grid(root, f"{aid}_outzoom", [(p_out, 0, 6, 2.0, arms), (p_out, -90, 6, 2.0, arms),
                                                       (p_out, 90, 6, 2.0, arms), (p_out, -30, -12, 2.0, arms),
                                                       (p_out, 35, 20, 2.0, arms), (p_out, 0, -25, 2.0, arms)],
                              cols=3, size=640)
    out.write_text(json.dumps(log, indent=1))


main()
