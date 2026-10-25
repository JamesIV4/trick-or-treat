"""Publishes gate-passing assets to the game's catalog (game/assets/catalog.json).

    python art/tools/publish_catalog.py

Only assets whose export exists and passes every cheap gate are published; the generator falls
back to placeholders for the rest. Prints a summary and exits 1 if anything failed.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gates  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "game" / "assets" / "catalog.json"
FIELDS = ("id", "path", "collision", "walkable", "radius", "height")


def main():
    published, failed, missing = [], [], []
    for asset in gates.CATALOG:
        result = gates.check(asset)
        if result["fails"] == ["missing export"]:
            missing.append(asset["id"])
            continue
        if not result["pass"]:
            failed.append((asset["id"], result["fails"]))
            continue
        published.append({k: asset[k] for k in FIELDS if k in asset})
    OUT.write_text(json.dumps({"assets": published}, indent=1))
    print(f"published {len(published)} assets -> {OUT}")
    for aid, fails in failed:
        print(f"  FAIL {aid}: {'; '.join(fails)}")
    if missing:
        print(f"  not exported yet: {', '.join(missing)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
