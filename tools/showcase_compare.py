"""Side-by-side before/after video from two showcase capture folders.

    python tools/showcase_compare.py <before_dir> <after_dir> <out_dir> [ids...] [--seconds N] [--crf N]

For every id present in both folders (or the ids given) this writes <out_dir>/<id>_compare.mp4
(1920x1080, 60 fps): BEFORE on the left, AFTER on the right, with the character's name and the
current beat's name along the bottom. It then joins them into <out_dir>/before_after_all.mp4.
Both clips must come from the same tools/showcase.ps1 recorder (same size, fps and beat list);
the script refuses a pair whose beat lists differ. --seconds trims each clip (for a quick test).
"""
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

ORDER = [
    "dragon_kid", "kid_ghost", "kid_cat", "kid_witch", "kid_skeleton", "kid_pumpkin",
    "monster_pumpkin", "monster_bat", "monster_skeleton", "monster_ghost", "monster_gremlin",
    "monster_mummy", "monster_slime", "monster_scarecrow", "monster_spider", "monster_toadstool",
]
NAMES = {
    "dragon_kid": "Dragon Kid", "kid_ghost": "Bedsheet Ghost costume", "kid_cat": "Black Cat costume",
    "kid_witch": "Little Witch costume", "kid_skeleton": "Skeleton costume", "kid_pumpkin": "Pumpkin costume",
    "monster_pumpkin": "Pumpkin Hopper", "monster_bat": "Bat", "monster_skeleton": "Rattlebones",
    "monster_ghost": "Ghost", "monster_gremlin": "Candy-Corn Gremlin", "monster_mummy": "Mummy",
    "monster_slime": "Goo Slime", "monster_scarecrow": "Scarecrow", "monster_spider": "Spider",
    "monster_toadstool": "Toadstool Imp",
}
BEATS = {
    "turntable": "turntable", "face": "face close-up", "walk": "walk", "run": "run",
    "stop_idle": "stop", "jump": "jump", "double_jump_glide": "double jump and glide",
    "spin": "spin attack", "roll": "roll", "ground_pound": "ground pound", "fire_cast": "wand cast",
    "hurt": "hurt", "candy_celebrate": "candy pickup", "idle": "idle", "wander": "wander",
    "chase": "chase", "attack_1": "attack (side)", "attack_2": "attack (front)", "hit": "hit",
    "die": "defeated", "rise_idle": "rise and idle",
}
FONT_SRC = Path(__file__).resolve().parent.parent / "game" / "assets" / "fonts" / "Fredoka-Variable.ttf"


def beats_of(folder: Path, cid: str):
    data = json.loads((folder / f"{cid}.beats.json").read_text(encoding="utf-8"))
    return data["fps"], data["total_frames"], [(b["name"], b["start_frame"], b["end_frame"]) for b in data["beats"]]


def text(label, x, y, size, extra=""):
    return (f"drawtext=fontfile=_font.ttf:text='{label}':x={x}:y={y}:fontsize={size}:fontcolor=white:"
            f"borderw=3:bordercolor=black@0.85{extra}")


def build(cid, before: Path, after: Path, out: Path, seconds, crf):
    fps, total, beats = beats_of(before, cid)
    fps2, total2, beats2 = beats_of(after, cid)
    if (fps, total, beats) != (fps2, total2, beats2):
        raise SystemExit(f"{cid}: before and after beat lists differ; re-record with the same recorder")
    chain = [
        "[0:v][1:v]hstack=inputs=2",
        "drawbox=x=958:y=0:w=4:h=ih:color=black@0.9:t=fill",
        "drawbox=x=0:y=ih-92:w=iw:h=92:color=black@0.45:t=fill",
        text("BEFORE", 36, 28, 54),
        text("AFTER", 960 + 36, 28, 54),
        text(NAMES.get(cid, cid), 36, "h-74", 46),
    ]
    for name, a, b in beats:
        label = BEATS.get(name, name.replace("_", " "))
        chain.append(text(label, "w-tw-36", "h-74", 46, f":enable='between(n,{a},{b})'"))
    script = out / f"_{cid}.filter"
    script.write_text(",".join(chain) + "[v]", encoding="utf-8")
    dst = out / f"{cid}_compare.mp4"
    cmd = ["ffmpeg", "-y", "-v", "error", "-i", str(before / f"{cid}.mp4"), "-i", str(after / f"{cid}.mp4"),
           "-filter_complex_script", script.name, "-map", "[v]", "-an", "-r", str(fps),
           "-c:v", "libx264", "-preset", "medium", "-crf", str(crf), "-pix_fmt", "yuv420p",
           "-movflags", "+faststart"]
    if seconds:
        cmd += ["-t", str(seconds)]
    cmd.append(dst.name)
    subprocess.run(cmd, cwd=out, check=True)
    script.unlink()
    return dst


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("before")
    ap.add_argument("after")
    ap.add_argument("out")
    ap.add_argument("ids", nargs="*")
    ap.add_argument("--seconds", type=float, default=0)
    ap.add_argument("--crf", type=int, default=20)
    a = ap.parse_args()
    seconds, crf = a.seconds, a.crf
    before, after, out = (Path(p).resolve() for p in (a.before, a.after, a.out))
    ids = a.ids or [c for c in ORDER if (before / f"{c}.mp4").exists() and (after / f"{c}.mp4").exists()]
    out.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(FONT_SRC, out / "_font.ttf")
    done = []
    for cid in ids:
        print(f"{cid} ...", flush=True)
        done.append(build(cid, before, after, out, seconds, crf))
    (out / "_font.ttf").unlink()
    if len(done) > 1:
        lst = out / "_concat.txt"
        lst.write_text("".join(f"file '{d.name}'\n" for d in done), encoding="utf-8")
        subprocess.run(["ffmpeg", "-y", "-v", "error", "-f", "concat", "-safe", "0", "-i", lst.name,
                        "-c", "copy", "-movflags", "+faststart", "before_after_all.mp4"], cwd=out, check=True)
        lst.unlink()
        print(f"joined {len(done)} clips -> {out / 'before_after_all.mp4'}")
    for d in done:
        print(d)


if __name__ == "__main__":
    main()
