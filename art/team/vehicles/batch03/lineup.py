"""Side-by-side lineup of the three parked cars for a gameplay-distance comparison in the engine.

    blender -b --factory-startup --python lineup.py -- <out.gltf>

Imports the exported glTFs, parks them side by side along X (like a street kerb) and writes one glTF.
run.ps1 / lineup.ps1 temporarily writes it over props/parked_car so tools/asset_preview.ps1 can show it,
then rebuilds parked_car. Never leave the lineup in place.
"""
import sys
from pathlib import Path

import bpy

ROOT = Path(__file__).resolve().parents[4]
MODELS = ROOT / "game" / "assets" / "models" / "props"
out = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
for i, (aid, x, y) in enumerate((("parked_wagon", -2.5, 0.2), ("parked_car", 0.0, -0.1), ("parked_pickup", 2.5, 0.1))):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.gltf(filepath=str(MODELS / f"{aid}.gltf"))
    for ob in set(bpy.context.scene.objects) - before:
        if ob.parent is None:
            ob.location.x += x  # the importer converts back to Blender Z-up, so Y is the car's length axis
            ob.location.y += y
bpy.ops.object.select_all(action="SELECT")
bpy.ops.export_scene.gltf(filepath=out, export_format="GLTF_SEPARATE", use_selection=True, export_yup=True,
                          export_apply=True, export_materials="NONE", export_texcoords=True, export_normals=True,
                          export_vertex_color="ACTIVE", export_active_vertex_color_when_no_material=True,
                          export_all_vertex_colors=False)
print("LINEUP", out)
