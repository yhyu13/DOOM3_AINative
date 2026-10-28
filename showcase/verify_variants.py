# /// script
# requires-python = ">=3.11"
# dependencies = ["playwright", "pillow", "edge-tts==7.3.1", "mutagen>=1.47"]
# ///
"""Variant gate for the AIGC showcase.

Builds the default deck twice, plus `--no-aigc`, `--no-hero` and a licensed `--hero` deck, then
checks the assets, labels, controls and fallbacks with Playwright. This is the reproducible form of
the manual variant matrix recorded in `specs/007-aigc-vision/review.md`.

Run:
    uv run --python 3.12 showcase/verify_variants.py
"""
from __future__ import annotations

import base64
import hashlib
import io
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageStat
from playwright.sync_api import Error as PWError, sync_playwright

ROOT = Path(__file__).resolve().parent
INDEX = ROOT / "index.html"
EVIDENCE = ROOT / "evidence"
LOG_PATH = EVIDENCE / "variants.log"
LAUNCH_ARGS = [
    "--autoplay-policy=no-user-gesture-required",
    "--allow-file-access-from-files",
    "--disable-features=msEdgeFirstRunExperience",
]
PASSES: list[str] = []
FAILS: list[str] = []


def check(ok: bool, line: str) -> None:
    prefix = "PASS" if ok else "FAIL"
    print(f"{prefix} {line}", flush=True)
    (PASSES if ok else FAILS).append(line)


def data_uri(path: Path, mime: str) -> str:
    return f"data:{mime};base64," + base64.b64encode(path.read_bytes()).decode("ascii")


P2_URI = data_uri(ROOT / "assets" / "aigc" / "page2-modern.jpg", "image/jpeg")
P3_URI = data_uri(ROOT / "assets" / "aigc" / "page3-vision.jpg", "image/jpeg")
HERO_URI = data_uri(ROOT / "assets" / "hero-redacted.png", "image/png")


def build(out: Path, *args: str) -> None:
    result = subprocess.run(
        [sys.executable, str(ROOT / "build.py"), "--out", str(out), *args],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr, file=sys.stderr)
        raise SystemExit(f"build failed: {' '.join(args) or '(default)'}")


def open_page(browser, path: Path, n: int):
    ctx = browser.new_context(viewport={"width": 1920, "height": 1080}, device_scale_factor=1)
    page = ctx.new_page()
    errors: list[str] = []
    requests: list[str] = []
    page.on("console", lambda m: errors.append(f"console.{m.type}: {m.text}") if m.type == "error" else None)
    page.on("pageerror", lambda e: errors.append(f"pageerror: {e}"))
    page.on("request", lambda r: requests.append(r.url))
    page.goto(path.resolve().as_uri() + f"?page={n}&snap=1&mute=1", wait_until="load")
    page.wait_for_function("window.__SHOWCASE_READY__ === true", timeout=20000)
    page.wait_for_timeout(500)
    return ctx, page, errors, requests


VISIBLE_JS = """(sel) => {
    const el = document.querySelector(sel);
    if (!el || el.hidden) return false;
    const r = el.getBoundingClientRect();
    if (r.width < 10 || r.height < 10) return false;
    if (r.right <= 0 || r.bottom <= 0 || r.left >= innerWidth || r.top >= innerHeight) return false;
    for (let n = el; n; n = n.parentElement) {
        const s = getComputedStyle(n);
        if (s.display === 'none' || s.visibility === 'hidden' || parseFloat(s.opacity) < 0.1) return false;
    }
    return true;
}"""


def split(page):
    return page.evaluate("""() => {
        const p = document.querySelector('.p2');
        const h = document.querySelector('#p2Handle');
        return {
            css: parseFloat(getComputedStyle(p).getPropertyValue('--split')),
            aria: parseFloat(h.getAttribute('aria-valuenow')),
            clip: getComputedStyle(document.querySelector('#p2Old')).clipPath
        };
    }""")


def non_blank(page, selector: str) -> float:
    img = Image.open(io.BytesIO(page.locator(selector).screenshot())).convert("RGB")
    return sum(ImageStat.Stat(img).stddev) / 3.0


def audit(errors: list[str], requests: list[str], label: str) -> None:
    check(not errors, f"{label} :: zero console/page errors ({errors[:2]})")
    bad = [u for u in requests if not u.startswith(("file:", "data:", "blob:", "about:"))]
    check(not bad, f"{label} :: local-only requests (bad={bad[:3]})")


def main() -> int:
    EVIDENCE.mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="doom3-showcase-variants-"))
    try:
        default_a = work / "default-a.html"
        default_b = work / "default-b.html"
        no_aigc = work / "no-aigc.html"
        no_hero = work / "no-hero.html"
        licensed = work / "licensed.html"
        licensed_input = work / "licensed.jpg"
        shutil.copyfile(ROOT / "assets" / "aigc" / "page3-vision.jpg", licensed_input)

        print("building variants…", flush=True)
        build(default_a)
        build(default_b)
        build(no_aigc, "--no-aigc")
        build(no_hero, "--no-hero")
        build(licensed, "--hero", str(licensed_input), "--hero-label", "LICENSED CAPTURE",
              "--hero-credit", "variant gate")

        h_a = hashlib.sha256(default_a.read_bytes()).hexdigest()
        h_b = hashlib.sha256(default_b.read_bytes()).hexdigest()
        h_index = hashlib.sha256(INDEX.read_bytes()).hexdigest()
        check(h_a == h_b == h_index, f"reproducibility :: two builds match the committed artifact ({h_a[:12]}…)")
        check(default_a.stat().st_size < 4 * 1024 * 1024, "size :: under 4 MiB")
        raw = default_a.read_text(encoding="utf-8")
        check(raw.count("data:image/jpeg;base64,") == 2, "default :: exactly two AIGC JPEG data URIs")
        check(raw.count("data:image/png;base64,") == 1, "default :: redacted hero embedded exactly once")
        check("AI-GENERATED VISION · NOT A GAME CAPTURE" in raw and "AI VISION · GPT IMAGE 2.5" in raw,
              "default :: AIGC labels present in the artifact")

        with sync_playwright() as p:
            try:
                browser = p.chromium.launch(channel="msedge", headless=True, args=LAUNCH_ARGS)
            except PWError:
                browser = p.chromium.launch(headless=True, args=LAUNCH_ARGS)

            # ---------- default
            ctx, page, errors, requests = open_page(browser, default_a, 2)
            check(page.evaluate("(u) => document.querySelector('#p2New').src === u", P2_URI), "default :: page 2 uses page2-modern.jpg")
            check(page.evaluate("(u) => document.querySelector('#p2Old').src === u", HERO_URI), "default :: page 2 archival reuses the redacted hero")
            check(page.evaluate("() => {const i=document.querySelector('#p2New'); return !i.hidden && i.complete && i.naturalWidth===1600 && i.naturalHeight===900;}"), "default :: page 2 image decodes 1600x900")
            check(page.evaluate(VISIBLE_JS, "#p2New"), "default :: page 2 image is actually visible")
            check(page.locator("#p2VisionLabel").inner_text().strip() == "AI-GENERATED VISION · NOT A GAME CAPTURE", "default :: exact page 2 label")
            s0 = split(page)
            page.locator("#p2Reveal").click()
            page.wait_for_timeout(1200)
            s1 = split(page)
            check(s0["aria"] == 46 and s1["aria"] == 80 and abs(s1["css"] - 80) <= 0.6 and s1["clip"] != s0["clip"], "default :: BACK TO 2004 moves the visual clip")
            page.locator("#p2Reveal").click()
            page.wait_for_timeout(1200)
            s2 = split(page)
            check(s2["aria"] == 32 and abs(s2["css"] - 32) <= 0.6 and s2["clip"] != s1["clip"], "default :: REVEAL 2026 moves the visual clip")
            box = page.locator("#p2Handle").bounding_box()
            page.mouse.move(box["x"] + box["width"] / 2, box["y"] + box["height"] / 2)
            page.mouse.down()
            page.mouse.move(box["x"] + box["width"] / 2 + 400, box["y"] + box["height"] / 2, steps=8)
            page.mouse.up()
            page.wait_for_timeout(300)
            s3 = split(page)
            check(s3["aria"] > s2["aria"] and abs(s3["css"] - s3["aria"]) <= 0.6, "default :: pointer drag stays in sync")
            audit(errors, requests, "default page 2")
            ctx.close()

            ctx, page, errors, requests = open_page(browser, default_a, 3)
            check(page.evaluate("(u) => document.querySelector('#p3Bg').src === u", P3_URI), "default :: page 3 uses page3-vision.jpg")
            check(page.evaluate("() => {const i=document.querySelector('#p3Bg'); return !i.hidden && i.complete && i.naturalWidth===1600 && i.naturalHeight===900;}"), "default :: page 3 image decodes 1600x900")
            check(page.evaluate(VISIBLE_JS, "#p3Bg"), "default :: page 3 image is actually visible")
            check(page.locator("#p3AiLabel").inner_text().strip() == "AI VISION · GPT IMAGE 2.5", "default :: exact page 3 AI label")
            check(page.locator("#p3Truth").inner_text().strip() == "AI-GENERATED VISION · NOT A GAME CAPTURE", "default :: page 3 truth label")
            check(page.evaluate("""() => Array.from(document.querySelectorAll('.vision-card')).every(el => {
                const r = el.getBoundingClientRect();
                const hit = document.elementFromPoint(r.left + r.width/2, r.top + r.height/2);
                return !!(hit && hit.closest('.vision-card'));
            })"""), "default :: page 3 cards above the image")
            audit(errors, requests, "default page 3")
            ctx.close()

            # ---------- --no-aigc
            raw = no_aigc.read_text(encoding="utf-8")
            check("data:image/jpeg;base64," not in raw, "no-aigc :: no AIGC JPEG data")
            check('id="p2VisionLabel">AI-GENERATED VISION' not in raw
                  and 'id="p3Truth">AI-GENERATED VISION' not in raw
                  and 'id="p3AiLabel">AI VISION · GPT IMAGE 2.5' not in raw,
                  "no-aigc :: no AIGC claim in the static DOM labels")
            ctx, page, errors, requests = open_page(browser, no_aigc, 2)
            check(page.evaluate("document.querySelector('.p2').dataset.aigc === '0'"), "no-aigc :: page 2 data-aigc=0")
            check(page.evaluate("document.querySelector('#p2New').hidden === true"), "no-aigc :: page 2 image hidden")
            check(page.evaluate("(u) => document.querySelector('#p2Old').src === u", HERO_URI), "no-aigc :: page 2 archival stays redacted")
            check(page.evaluate("""() => {const i=document.querySelector('#p2Old');
                return i.complete && i.naturalWidth > 0 && i.naturalHeight > 0;}"""), "no-aigc :: page 2 archival decoded")
            check(page.evaluate(VISIBLE_JS, "#p2Old"), "no-aigc :: page 2 archival is actually visible")
            check(page.evaluate("getComputedStyle(document.querySelector('#fx2')).display !== 'none'"), "no-aigc :: page 2 canvas visible")
            check(page.locator("#p2VisionLabel").inner_text().strip() == "PROCEDURAL MODERN RENDER · NOT A GAME CAPTURE", "no-aigc :: page 2 procedural label")
            s0 = split(page)
            page.locator("#p2Reveal").click()
            page.wait_for_timeout(1200)
            check(int(page.locator("#p2Handle").get_attribute("aria-valuenow")) == 80 and split(page)["clip"] != s0["clip"], "no-aigc :: split still works")
            check(non_blank(page, ".poster.p2") > 8, "no-aigc :: page 2 fallback is non-blank")
            audit(errors, requests, "no-aigc page 2")
            ctx.close()

            ctx, page, errors, requests = open_page(browser, no_aigc, 3)
            check(page.evaluate("document.querySelector('.p3').dataset.aigc === '0'"), "no-aigc :: page 3 data-aigc=0")
            check(page.evaluate("document.querySelector('#p3Bg').hidden === true"), "no-aigc :: page 3 image hidden")
            check(page.evaluate("getComputedStyle(document.querySelector('#fx3')).display !== 'none'"), "no-aigc :: page 3 canvas visible")
            check(page.locator("#p3Truth").inner_text().strip() == "PROCEDURAL VISION · NOT A GAME CAPTURE", "no-aigc :: page 3 procedural truth label")
            check(page.locator("#p3AiLabel").inner_text().strip() == "PROCEDURAL VISION", "no-aigc :: page 3 procedural AI label")
            check(page.locator(".vision-card").count() == 3, "no-aigc :: page 3 keeps three cards")
            check(non_blank(page, ".poster.p3") > 8, "no-aigc :: page 3 fallback is non-blank")
            audit(errors, requests, "no-aigc page 3")
            ctx.close()

            # ---------- --no-hero (poster 1 procedural; page 2 archival stays)
            ctx, page, errors, requests = open_page(browser, no_hero, 1)
            check(page.evaluate("!document.querySelector('#browserView').dataset.hero"), "no-hero :: poster 1 is procedural")
            check("PROCEDURAL" in page.locator("#mediaBadge").inner_text(), "no-hero :: procedural badge visible")
            audit(errors, requests, "no-hero page 1")
            ctx.close()
            ctx, page, errors, requests = open_page(browser, no_hero, 2)
            check(page.evaluate("(u) => document.querySelector('#p2Old').src === u", HERO_URI), "no-hero :: page 2 archival still redacted")
            check(page.evaluate("(u) => document.querySelector('#p2New').src === u", P2_URI), "no-hero :: page 2 AIGC image still present")
            audit(errors, requests, "no-hero page 2")
            ctx.close()

            # ---------- licensed --hero
            raw = licensed.read_text(encoding="utf-8")
            check(raw.count("data:image/png;base64,") == 1, "licensed :: redacted archival embedded once")
            check(raw.count("data:image/jpeg;base64,") == 3, "licensed :: licensed hero + two AIGC JPEGs")
            ctx, page, errors, requests = open_page(browser, licensed, 1)
            check(page.evaluate("(u) => document.querySelector('#heroImage').src === u", data_uri(licensed_input, "image/jpeg")), "licensed :: poster 1 uses the licensed image")
            check("LICENSED CAPTURE" in page.locator("#mediaBadge").inner_text(), "licensed :: licensed label visible")
            audit(errors, requests, "licensed page 1")
            ctx.close()
            ctx, page, errors, requests = open_page(browser, licensed, 2)
            check(page.evaluate("(u) => document.querySelector('#p2Old').src === u", HERO_URI), "licensed :: page 2 archival is the redacted asset")
            check(page.evaluate("(u) => document.querySelector('#p2Old').src !== u", data_uri(licensed_input, "image/jpeg")), "licensed :: licensed hero is not the 2004 archival")
            audit(errors, requests, "licensed page 2")
            ctx.close()
            browser.close()

        LOG_PATH.write_text("\n".join(f"{'PASS' if x in PASSES else 'FAIL'} {x}" for x in PASSES + FAILS) + "\n", encoding="utf-8")
        print(f"\n{len(PASSES)} passed, {len(FAILS)} failed")
        print(f"log: {LOG_PATH}")
        return 1 if FAILS else 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
