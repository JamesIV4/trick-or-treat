"""parked_car: rounded toy hatchback, front faces -Y."""
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import K, finish, paint_where_world, want  # noqa: E402

BODY = "house_sky"
GLASS = "shadow_navy"


def parked_car():
    K.reset()
    parts = []
    body = K.box("body", (1.64, 3.5, 0.66), color=BODY, loc=(0, 0, 0.57), bevel=0.26, segments=3)
    parts.append(body)
    cabin = K.box("cabin", (1.48, 2.1, 0.66), color=BODY, bevel=0.12, segments=2)

    def shape(co):
        if co.z > 0:
            co.x *= 0.84
            co.y += 0.38 if co.y < 0 else -0.08
        return co
    K.deform(cabin, shape)
    cabin.location = (0, 0.3, 1.16)
    parts.append(cabin)
    paint_where_world(cabin, GLASS, lambda c, n: 0.95 < c.z < 1.4 and (abs(n.x) > 0.9 or (n.y < -0.6 and n.z < 0.75) or n.y > 0.9))
    # B-pillars splitting the side glass
    for sx in (-1, 1):
        parts.append(K.box(f"pillar{sx}", (0.07, 0.12, 0.46), color=BODY, loc=(sx * 0.665, 0.28, 1.17), rot=(0, sx * -6, 0)))
    # wheels: chunky tyres with silver hubs
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * 0.7, sy * 1.15
            parts.append(K.cylinder(f"tyre{sx}{sy}", 0.4, 0.34, color="charcoal", verts=10, loc=(x, y, 0.4),
                                    rot=(0, 90, 0), bevel=0.04))
            parts.append(K.cylinder(f"hub{sx}{sy}", 0.19, 0.06, color="silver", verts=8, loc=(x + sx * 0.17, y, 0.4),
                                    rot=(0, 90, 0)))
    # bumpers, lights, grille, plate, mirrors
    parts.append(K.box("bumper_f", (1.66, 0.22, 0.2), color="stone_light", loc=(0, -1.73, 0.4), bevel=0.08, segments=2))
    parts.append(K.box("bumper_r", (1.66, 0.22, 0.2), color="stone_light", loc=(0, 1.73, 0.4), bevel=0.08, segments=2))
    for sx in (-1, 1):
        parts.append(K.sphere(f"headlight{sx}", 0.15, color="silver", loc=(sx * 0.52, -1.72, 0.68),
                              scale=(1.0, 0.45, 0.85), segments=8, rings=4))
        parts.append(K.box(f"taillight{sx}", (0.26, 0.06, 0.18), color="metal_red", loc=(sx * 0.6, 1.765, 0.7),
                           bevel=0.03))
        parts.append(K.box(f"mirror{sx}", (0.16, 0.08, 0.1), color=BODY, loc=(sx * 0.82, -0.62, 0.98), bevel=0.03))
    parts.append(K.box("grille", (0.6, 0.06, 0.14), color="charcoal", loc=(0, -1.765, 0.62), bevel=0.03))
    parts.append(K.box("plate", (0.4, 0.04, 0.14), color="cream", loc=(0, 1.86, 0.42)))
    car = K.merge(parts, "parked_car")
    lo = min(v.co.z for v in car.data.vertices)
    for v in car.data.vertices:
        v.co.z -= lo
    return finish([car], "props/parked_car", "parked_car")


if want("parked_car"):
    parked_car()
