# /// script
# requires-python = ">=3.11"
# dependencies = ["playwright"]
# ///
"""Run axe-core WCAG 2.1 A/AA against all three settled posters.

axe-core is not vendored. Install it locally or point `AXE_PATH` at `axe.min.js`:

    npm install axe-core@4.10.3 --prefix .axe
    $env:AXE_PATH = ".axe/node_modules/axe-core/axe.min.js"
    uv run --python 3.12 showcase/verify_axe.py

Exit 0 only when there are zero violations. Incomplete color-contrast nodes over images are
recorded (not claimed as verified); see `showcase/README.md`.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parent
INDEX = ROOT / "index.html"
OUT = ROOT / "evidence" / "axe.json"
LAUNCH_ARGS = [
    "--autoplay-policy=no-user-gesture-required",
    "--allow-file-access-from-files",
    "--disable-features=msEdgeFirstRunExperience",
]


def find_axe() -> Path | None:
    candidates = [
        os.environ.get("AXE_PATH"),
        str(ROOT / ".axe" / "node_modules" / "axe-core" / "axe.min.js"),
        str(ROOT.parent / "node_modules" / "axe-core" / "axe.min.js"),
        str(ROOT.parent / ".axe" / "node_modules" / "axe-core" / "axe.min.js"),
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return Path(candidate)
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--axe", type=Path, default=None)
    args = ap.parse_args()
    axe = args.axe or find_axe()
    if not axe or not axe.is_file():
        print("SKIP: axe-core not found. Set AXE_PATH or install it with:", file=sys.stderr)
        print("      npm install axe-core@4.10.3 --prefix .axe", file=sys.stderr)
        return 2

    results = {}
    bad = 0
    with sync_playwright() as p:
        try:
            browser = p.chromium.launch(channel="msedge", headless=True, args=LAUNCH_ARGS)
        except Exception:
            browser = p.chromium.launch(headless=True, args=LAUNCH_ARGS)
        for n in (1, 2, 3):
            ctx = browser.new_context(viewport={"width": 1920, "height": 1080}, device_scale_factor=1)
            page = ctx.new_page()
            page.goto(INDEX.resolve().as_uri() + f"?page={n}&snap=1&mute=1", wait_until="load")
            page.wait_for_function("window.__SHOWCASE_READY__ === true", timeout=20000)
            page.wait_for_timeout(900)
            page.add_script_tag(path=str(axe))
            result = page.evaluate("""async () => {
                const r = await axe.run(document, {
                    runOnly: {type: 'tag', values: ['wcag2a','wcag2aa','wcag21a','wcag21aa']},
                    resultTypes: ['violations','incomplete']
                });
                return {
                    violations: r.violations.map(v => ({id: v.id, impact: v.impact, nodes: v.nodes.length,
                        targets: v.nodes.map(n => n.target)})),
                    incomplete: r.incomplete.map(v => ({id: v.id, impact: v.impact, nodes: v.nodes.length}))
                };
            }""")
            results[n] = result
            violations = result["violations"]
            incomplete = result["incomplete"]
            print(f"poster {n}: violations={len(violations)} incomplete={[(x['id'], x['nodes']) for x in incomplete]}")
            for v in violations:
                print(f"  VIOLATION {v['id']} ({v['impact']}) nodes={v['nodes']}: {v['targets'][:4]}")
            bad += len(violations)
            ctx.close()
        browser.close()

    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"axe: {bad} violations; report: {OUT}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
