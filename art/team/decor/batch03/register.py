"""Adds/updates the decor batch03 entries in art/asset_catalog.json (read-modify-write in one go,
other workers edit the same file) and updates the lamp briefs.

    python art/team/decor/batch03/register.py
"""
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[4]
P = ROOT / "art" / "asset_catalog.json"

NEW = [
    {"id": "string_jack", "path": "props/string_jack", "lane": "decor", "tris": 150, "collision": "none",
     "floating": True, "sockets": ["light"],
     "brief": "Tiny glowing jack-o'-lantern bulb 0.14 m for porch string lights: ribbed glow_jack_shell globe with a "
              "glow_jack_hot carved face toward -Y, green stem and black lamp socket. Origin at the top of the socket "
              "(the wire hang point); socket_light at the globe centre. Instanced along sagging wires."},
    {"id": "bunting_flag_orange", "path": "props/bunting_flag_orange", "lane": "decor", "tris": 60,
     "collision": "none", "floating": True,
     "brief": "Felt bunting pennant 0.28 m wide x 0.34 m tall, banner_orange, slight cloth wave, double-sided "
              "geometry (no alpha). Origin at the top-centre hang point where the cord runs through the rolled hem; "
              "the cord runs along X."},
    {"id": "bunting_flag_purple", "path": "props/bunting_flag_purple", "lane": "decor", "tris": 60,
     "collision": "none", "floating": True, "brief": "As bunting_flag_orange in banner_purple."},
    {"id": "bunting_flag_cream", "path": "props/bunting_flag_cream", "lane": "decor", "tris": 60,
     "collision": "none", "floating": True,
     "brief": "As bunting_flag_orange in bunting_cream with a small black bat silhouette on both faces."},
    {"id": "door_wreath", "path": "props/door_wreath", "lane": "decor", "tris": 900, "collision": "none",
     "floating": True,
     "brief": "Autumn door wreath 0.6 m across: dark twig ring covered in orange/gold/rust leaves, red and purple "
              "berries and a puffy banner_purple bow at the bottom. Origin at the back centre (touches the door), "
              "front faces -Y; centre of the ring is at the origin height."},
]
BRIEFS = {
    "lamp_post": "Black Victorian street lamp 3.5 m tall (lamp_black): octagonal plinth, bell base, fluted post, "
                 "scroll-bracket collar, four-sided lantern head with two-tone glowing panes (glow_lantern rim, "
                 "glow_lamp core), domed roof with cupola, ball and spike finial. socket_light at the glass centre.",
    "porch_lantern": "Wall-mounted porch lantern 0.5 m tall: wall plate, scroll arm, four-sided hanging lantern with "
                     "two-tone glowing panes (glow_lantern rim, glow_lamp core) and wide flared roof. Origin at the "
                     "wall mount, it sticks out toward -Y; socket_light at the glass centre.",
}

data = json.loads(P.read_text(encoding="utf-8"))
ids = {a["id"]: i for i, a in enumerate(data["assets"])}
for e in NEW:
    if e["id"] in ids:
        data["assets"][ids[e["id"]]] = e
    else:
        data["assets"].append(e)
for a in data["assets"]:
    if a["id"] in BRIEFS:
        a["brief"] = BRIEFS[a["id"]]
P.write_text(json.dumps(data, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
print("registered", [e["id"] for e in NEW], "briefs", list(BRIEFS))
