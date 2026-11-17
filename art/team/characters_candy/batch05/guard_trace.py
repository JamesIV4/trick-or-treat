"""Reads the showcase logs written by guard_trace.ps1 (TOT_RIG_DEBUG on) and prints, per costume:
  - the roll (and the ground pound and hurt beats): the lowest point of the head's parts (TOP) and of the whole kid
    (BODY) above the road, before (raw) and after (now) the roll's ground guard, and the guard's raise (LIFT);
  - the stop (stop_idle beat): the hanging hem's bend as shown (x forward / back, z sideways, radians).
Clip frame n = the n-th RIGDBG line after "SHOWCASE beat turntable at frame 0" (one rig update per 60 fps frame); the
other beat markers are checked against that count.
  python guard_trace.py <dir> <id> [<id> ...] [--rows 1340-1400]
"""
import json
import re
import sys
from pathlib import Path

NUM = r"(-?[\d.]+)"
RX = {
    "top": re.compile(r"TOP raw=" + NUM + r" now=" + NUM),
    "body": re.compile(r"BODY raw=" + NUM + r" now=" + NUM + r" LIFT=" + NUM),
    "hem": re.compile(r"HEM raw=" + NUM + r" now=" + NUM),
    "sheet": re.compile(r"SHEET " + NUM + " " + NUM + " " + NUM + r" d " + NUM + " " + NUM),
    "state": re.compile(r"state=(\S+)"),
    "part": re.compile(r"LOWPART=(\S+)"),
    "of": re.compile(r" OF=(\d)"),
    "wand": re.compile(r" WAND=" + NUM),  # final fixes 2: the wand's lowest point above the floor (with the raise)
    # final fixes 2: the hanging hem's foremost / rearmost low point above the floor (how much leg shows)
    "fb": re.compile(r" FRONT=" + NUM + r" BACK=" + NUM),
}


def frames(log):
    rows, started, marks = [], False, []
    for line in Path(log).read_text(errors="replace").splitlines():
        m = re.search(r"SHOWCASE beat (\S+) at frame (\d+)", line)
        if m:
            if m.group(1) == "turntable":
                started = True
            marks.append((m.group(1), int(m.group(2)), len(rows)))
            continue
        if started and line.startswith("RIGDBG"):
            r = {}
            for k, rx in RX.items():
                mm = rx.search(line)
                if mm:
                    r[k] = mm.group(1) if k in ("state", "part", "of") else [float(x) for x in mm.groups()]
            rows.append(r)
    return rows, marks


def low(rows, lo, hi, key, idx, floor_only=False):
    """Lowest value in the beat; floor_only: frames on the floor only (in the air the trace's floor is the
    controller's own height, not the road)."""
    best = None
    for f in range(lo, min(hi + 1, len(rows))):
        if floor_only and rows[f].get("of", "1") != "1":
            continue
        v = rows[f].get(key)
        if v is None or v[idx] > 9.0:
            continue
        if best is None or v[idx] < best[0]:
            best = (v[idx], f)
    return best


def fmt(b):
    return "n/a" if b is None else f"{b[0] * 100:+.1f} cm @{b[1]}"


def main():
    args = sys.argv[1:]
    rows_rng = None
    if "--rows" in args:
        i = args.index("--rows")
        a, b = args[i + 1].split("-")
        rows_rng = (int(a), int(b))
        del args[i:i + 2]
    d = Path(args[0])
    for cid in args[1:]:
        log = d / f"{cid}.log"
        if not log.exists():
            print(f"{cid}: no log")
            continue
        rows, marks = frames(log)
        beats = {b["name"]: (b["start_frame"], b["end_frame"]) for b in json.loads((d / f"{cid}.beats.json").read_text())["beats"]}
        off = [f"{n} {f - c:+d}" for n, f, c in marks if f != c]
        print(f"== {cid}: {len(rows)} traced frames; beat markers off by: {', '.join(off) if off else 'none'}")
        for beat in ("roll", "ground_pound", "hurt", "jump", "stop_idle"):
            lo, hi = beats[beat]
            lift = [(rows[f]["body"][2], f) for f in range(lo, min(hi + 1, len(rows))) if "body" in rows[f]]
            lmax = max(lift) if lift else (0.0, -1)
            on = [f for v, f in lift if v > 1e-4]
            fo = beat != "roll"
            braw = low(rows, lo, hi, 'body', 0, fo)
            bnow = low(rows, lo, hi, 'body', 1, fo)
            part = rows[braw[1]].get("part", "?") if braw else "?"
            pnow = rows[bnow[1]].get("part", "?") if bnow else "?"
            print(f"  {beat:13s} TOP raw {fmt(low(rows, lo, hi, 'top', 0, fo))}, now {fmt(low(rows, lo, hi, 'top', 1, fo))}; "
                  f"BODY{' (on floor)' if fo else ''} raw {fmt(braw)} ({part}), now {fmt(bnow)} ({pnow}); "
                  f"LIFT max {lmax[0] * 100:.1f} cm @{lmax[1]}" + (f" (frames {on[0]}-{on[-1]})" if on else "")
                  + (f"; HEM now {fmt(low(rows, lo, hi, 'hem', 1))}" if low(rows, lo, hi, 'hem', 1) else "")
                  + (f"; WAND now {fmt(low(rows, lo, hi, 'wand', 0, fo))}" if low(rows, lo, hi, 'wand', 0, fo) else ""))
        lo, hi = beats["stop_idle"]
        sh = [(f, rows[f]["sheet"]) for f in range(lo - 10, min(hi + 1, len(rows))) if "sheet" in rows[f]]
        if sh:
            mx = max(sh, key=lambda t: (t[1][0] ** 2 + t[1][2] ** 2))
            wide = [f for f, s in sh if (s[0] ** 2 + s[2] ** 2) ** 0.5 > 0.2]
            print(f"  stop sheet: max |bend| {((mx[1][0] ** 2 + mx[1][2] ** 2) ** 0.5):.3f} rad @{mx[0]} (x {mx[1][0]:+.3f}, z {mx[1][2]:+.3f}); "
                  f"frames over 0.2 rad: {len(wide)}" + (f" ({wide[0]}-{wide[-1]})" if wide else ""))
            print("  stop sheet per frame (frame: x z | spring x z):")
            line = []
            for f, s in sh:
                if f % 2 == 0:
                    line.append(f"{f}: {s[0]:+.2f} {s[2]:+.2f} | {s[3]:+.2f} {s[4]:+.2f}")
            for i in range(0, len(line), 5):
                print("    " + "   ".join(line[i:i + 5]))
        fbs = [(f, r["fb"]) for f, r in enumerate(rows) if "fb" in r]
        if fbs:
            # Hem height above the floor, front and back edge (cm): rest = the last idle frames; per beat min / max.
            rest = rows[min(beats["stop_idle"][1], len(rows) - 1)].get("fb", [0, 0])
            print(f"  hem edge height (cm, front / back; rest {rest[0] * 100:.0f} / {rest[1] * 100:.0f}):")
            for beat in ("walk", "run", "stop_idle", "jump", "double_jump_glide"):
                if beat not in beats:
                    continue
                lo, hi = beats[beat]
                v = [r for f, r in fbs if lo <= f <= hi and rows[f].get("of", "1") == "1"]
                if v:
                    print(f"    {beat:10s} front {min(x[0] for x in v) * 100:5.1f} .. {max(x[0] for x in v) * 100:5.1f}   "
                          f"back {min(x[1] for x in v) * 100:5.1f} .. {max(x[1] for x in v) * 100:5.1f}  (on-floor frames)")
            lo, hi = beats["stop_idle"]
            st = [(f, r) for f, r in fbs if lo - 10 <= f <= lo + 40]
            print("    stop per 2 frames (frame front/back): " + "  ".join(f"{f}:{r[0] * 100:.0f}/{r[1] * 100:.0f}" for f, r in st if f % 2 == 0))
        if rows_rng:
            print("  per frame (frame state TOPraw BODYraw LIFT, cm):")
            out = []
            for f in range(rows_rng[0], min(rows_rng[1] + 1, len(rows))):
                r = rows[f]
                if "body" in r:
                    out.append(f"{f} {r.get('state', '?')[:6]} {r['top'][0] * 100:+.1f} {r['body'][0] * 100:+.1f} {r['body'][2] * 100:.1f} {r.get('part', '')[:10]}")
            for i in range(0, len(out), 4):
                print("    " + " | ".join(out[i:i + 4]))


if __name__ == "__main__":
    main()

