# /// script
# requires-python = ">=3.11"
# dependencies = ["playwright", "pillow"]
# ///
"""Acceptance checks for the single-file showcase.

Run:
    uv run --python 3.12 showcase/verify.py
    uv run --python 3.12 showcase/verify.py --headed

Uses the system Microsoft Edge (Playwright channel="msedge") so no browser
download is needed. Writes screenshots and a log under showcase/evidence/.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
import sys
import time
from pathlib import Path

from PIL import Image, ImageStat
from playwright.sync_api import sync_playwright, Error as PWError

ROOT = Path(__file__).resolve().parent
INDEX = ROOT / "index.html"
EVIDENCE = ROOT / "evidence"
LOG_PATH = EVIDENCE / "verify.log"

FAILS: list[str] = []
PASSES: list[str] = []
LOG: list[str] = []


def record(ok: bool, line: str) -> None:
    prefix = "PASS" if ok else "FAIL"
    text = f"{prefix} {line}"
    print(text, flush=True)
    LOG.append(text)
    (PASSES if ok else FAILS).append(line)


def check(ok: bool, line: str) -> bool:
    record(bool(ok), line)
    return bool(ok)


def page_url(page: int, snap: bool = True, mute: bool = True) -> str:
    q = [f"page={page}"]
    if snap:
        q.append("snap=1")
    if mute:
        q.append("mute=1")
    return INDEX.resolve().as_uri() + "?" + "&".join(q)


def new_page(browser, init_script: str | None = None):
    ctx = browser.new_context(
        viewport={"width": 1920, "height": 1080},
        device_scale_factor=1,
        reduced_motion="no-preference",
    )
    if init_script:
        ctx.add_init_script(init_script)
    page = ctx.new_page()
    errors: list[str] = []
    requests: list[str] = []
    page.on("console", lambda m: errors.append(f"console.{m.type}: {m.text}") if m.type == "error" else None)
    page.on("pageerror", lambda e: errors.append(f"pageerror: {e}"))
    page.on("request", lambda r: requests.append(r.url))
    return ctx, page, errors, requests


def wait_ready(page) -> None:
    page.wait_for_function("window.__SHOWCASE_READY__ === true", timeout=20000)
    page.wait_for_timeout(350)


def audit_requests(requests: list[str], label: str) -> None:
    bad = [u for u in requests if not re.match(r"^(file:|data:|blob:|about:)", u)]
    check(not bad, f"{label} :: offline request audit ({len(requests)} local requests, external={bad[:3]})")


def audit_console(errors: list[str], label: str) -> None:
    check(not errors, f"{label} :: zero console/page errors ({errors[:2]})")


def screenshot_check(page, n: int) -> tuple[Path, str]:
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    path = EVIDENCE / f"poster-{n}.png"
    page.screenshot(path=str(path), full_page=False, animations="disabled")
    img = Image.open(path).convert("RGB")
    stat = ImageStat.Stat(img)
    stddev = sum(stat.stddev) / 3.0
    colors = len(img.resize((160, 90)).getcolors(maxcolors=1_000_000) or [])
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    check(stddev > 8.0, f"poster {n} :: non-blank screenshot (stddev {stddev:.1f})")
    check(colors > 200, f"poster {n} :: visual richness ({colors} colors in 160x90)")
    return path, digest


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--headed", action="store_true")
    args = ap.parse_args()

    if not INDEX.exists():
        print(f"ERROR: {INDEX} does not exist. Run: uv run --python 3.12 showcase/build.py", file=sys.stderr)
        return 2

    html = INDEX.read_text(encoding="utf-8")
    size = INDEX.stat().st_size
    check(100_000 <= size <= 4 * 1024 * 1024, f"artifact :: single file size {size/1024:.1f} KiB within 100 KiB–4 MiB")
    check('src="http' not in html and "src='http" not in html, "artifact :: no external src")
    check('href="http' not in html and "href='http" not in html, "artifact :: no external href")
    check("@import url(http" not in html, "artifact :: no remote CSS import")
    check(not re.search(r"(?:src|href)\s*=\s*['\"]?https?://", html, re.I), "artifact :: no remote src/href (regex)")
    check(not re.search(r"url\(\s*['\"]?https?://", html, re.I), "artifact :: no remote CSS url()")
    check(not re.search(r"fetch\(\s*['\"]?https?://", html, re.I), "artifact :: no remote fetch()")
    check(not INDEX.read_bytes().startswith(b"\xef\xbb\xbf"), "artifact :: no UTF-8 BOM")
    check(not re.search(r"[A-Za-z]:\\", html), "artifact :: no absolute drive path")
    check(not re.search(r"(?<![A-Za-z])[A-Za-z]:[\\/]", html), "artifact :: no drive path (slash/backslash)")
    check(not re.search(r"\b(?:Users|home)[\\/]", html), "artifact :: no home directory path")
    check(not re.search(r"ghp_|gho_|github_pat_|sk-[A-Za-z0-9]{20,}|BEGIN [A-Z ]*PRIVATE KEY", html), "artifact :: no token/private-key pattern")
    check(not (ROOT / "assets" / "hero.png").exists() and not any(ROOT.rglob("source_steam*")), "asset :: original unredacted screenshot absent")
    check(html.count("data:audio/mpeg;base64,") == 3, "artifact :: three embedded MP3 narration tracks")
    check((ROOT / "assets" / "hero-redacted.png").exists(), "asset :: redacted capture file exists")
    check((ROOT / "assets" / "README.md").exists(), "asset :: provenance note exists")
    check("data:image/png;base64," in html, "artifact :: redacted hero image embedded")
    check("REDACTED CAPTURE" in html and "LICENSING PENDING" in html, "artifact :: redaction label and credit present")

    launch_args = [
        "--autoplay-policy=no-user-gesture-required",
        "--allow-file-access-from-files",
        "--disable-features=msEdgeFirstRunExperience",
    ]
    with sync_playwright() as p:
        try:
            browser = p.chromium.launch(channel="msedge", headless=not args.headed, args=launch_args)
            print("runtime: Microsoft Edge", flush=True)
        except PWError as exc:
            print(f"WARNING: Edge channel unavailable ({exc}); trying bundled Chromium.", file=sys.stderr)
            browser = p.chromium.launch(headless=True, args=launch_args)

        hashes: list[str] = []
        durations: list[float] = []
        chunk_counts: list[int] = []

        # ---- three settled posters
        for n in (1, 2, 3):
            ctx, page, errors, requests = new_page(browser)
            page.goto(page_url(n), wait_until="load")
            wait_ready(page)

            check(page.evaluate("document.body.dataset.page") == str(n), f"poster {n} :: body page state")
            check(page.evaluate("document.querySelectorAll('.poster.is-active').length") == 1, f"poster {n} :: exactly one active poster")
            check(page.evaluate("document.querySelector('.poster.is-active').dataset.poster") == str(n), f"poster {n} :: correct active poster")
            if n == 2:
                rooms = page.evaluate("window.__SHOWCASE_MAP__ ? window.__SHOWCASE_MAP__.rooms : null")
                check(bool(rooms) and len(rooms) == 7 and "HELL GATE" in rooms and "REACTOR" in rooms, f"poster 2 :: isometric map rooms ({rooms})")
            if n == 1:
                check(page.locator(".monitor").count() == 1, "poster 1 :: browser monitor present")
                check(page.locator("#browserView #fxFrame").count() == 1, "poster 1 :: game canvas sits inside the browser viewport")
                badge = page.locator("#mediaBadge").inner_text()
                check(page.evaluate("document.querySelector('#browserView').dataset.hero") == "1", "poster 1 :: redacted hero is active")
                check(page.evaluate("document.querySelector('#heroImage').classList.contains('is-on')"), "poster 1 :: redacted image is visible")
                check("REDACTED" in badge and "LICENSING PENDING" in badge, "poster 1 :: redacted / licensing label")
                check(page.evaluate("Array.from(document.querySelectorAll('#audioBank audio')).every(a => a.muted)"), "poster 1 :: ?mute=1 mutes every track")
                chunk_counts = page.evaluate("window.__SHOWCASE__.data.pages.map(p => p.chunks.length)")
                check(page.locator(".feature-row .feature").count() == 4, "poster 1 :: four capability cards")
                features = page.locator(".feature-row").inner_text()
                check(all(x in features for x in ("PLAY", "REMAP", "MOD", "AI ROGUE")), "poster 1 :: capability names present")
                check(page.locator(".feature .f-tag.is-concept").count() == 1, "poster 1 :: AI Rogue is marked CONCEPT")
            check(len(page.locator("#ccZh").inner_text().strip()) > 0, f"poster {n} :: caption text present")
            opacities = page.evaluate("""() => Array.from(document.querySelectorAll('.poster.is-active .reveal')).map(el => +getComputedStyle(el).opacity)""")
            check(bool(opacities) and min(opacities) >= 0.99, f"poster {n} :: all reveals settled (snap mode)")

            tracks = page.evaluate("""() => Array.from(document.querySelectorAll('#audioBank audio')).map(a => ({
                page: a.dataset.page,
                data: a.src.startsWith('data:audio/mpeg;base64,'),
                duration: (window.__SHOWCASE__.data.pages[+a.dataset.page - 1] || {}).duration || 0
            }))""")
            check(len(tracks) == 3, f"poster {n} :: three audio elements in the DOM")
            check(all(t["data"] for t in tracks), f"poster {n} :: all tracks are embedded data URIs")
            if n == 1:
                durations = [float(t["duration"]) for t in tracks]

            _, digest = screenshot_check(page, n)
            hashes.append(digest)
            audit_requests(requests, f"poster {n}")
            audit_console(errors, f"poster {n}")
            ctx.close()

        check(len(set(hashes)) == 3, "posters :: three pairwise-different screenshots")
        check(len(durations) == 3, "narration :: three tracks found")
        if len(durations) == 3:
            total = sum(durations)
            check(all(d >= 12 for d in durations), f"narration :: every poster at least 12s ({[round(d,1) for d in durations]})")
            check(50 <= total <= 85, f"narration :: total {total:.1f}s within 50–85s")
        check(len(chunk_counts) == 3 and all(n >= 5 for n in chunk_counts), f"narration :: at least five caption chunks per poster ({chunk_counts})")

        # ---- evidence lives in README + a one-line proof strip on poster 2
        ctx, page, errors, requests = new_page(browser)
        page.goto(page_url(2), wait_until="load")
        wait_ready(page)
        content = page.content()
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        for literal in ("1,387", "734,180", "100", "207", "pak archives", "Emscripten references"):
            check(literal in readme, f"claims :: README evidence present: {literal}")
        check("ROADMAP §1" in readme or "docs/ROADMAP.md §1" in readme, "claims :: README cites the roadmap")
        for literal in ("MODERN FPS.", "PERSONAL HELL.", "NEXT-GEN LOOK", "AI DIRECTOR", "PERSONAL RUN",
                        "1,387", "734,180", "0 GAME DATA", "PROCEDURAL LEVEL MAP · NOT A GAME CAPTURE"):
            check(literal in content, f"dream :: literal present: {literal}")
        for literal in ("CONCEPT ROADMAP / 2026", "M5 / PLANNED", "M7 / PLANNED", "CONCEPT DEMO · NOT A SHIPPED FEATURE"):
            check(literal in content, f"vision :: literal present: {literal}")
        check("M0 · SCAFFOLD" in content and "Port work starts at M1" in content and "The target, not a shipped build" in content, "claims :: scaffold status is explicit")
        ctx.close()

        # ---- AI Rogue concept demo (poster 3)
        ctx, page, errors, requests = new_page(browser)
        page.goto(page_url(3, snap=True), wait_until="load")
        wait_ready(page)
        before_prompt = page.locator("#promptText").inner_text()
        before_chips = page.locator("#promptOut span").all_inner_texts()
        page.locator("#generateBtn").click()
        page.wait_for_timeout(1000)
        after_prompt = page.locator("#promptText").inner_text()
        after_chips = page.locator("#promptOut span").all_inner_texts()
        check(after_prompt != before_prompt, "vision :: AI Rogue prompt regenerates")
        check(before_chips != after_chips and len(after_chips) == 3, "vision :: seed / map / modifier regenerate")
        check("SEED" in page.locator("#promptOut").inner_text(), "vision :: generated run shows a seed")
        check("CONCEPT DEMO" in page.locator(".prompt-note").inner_text(), "vision :: AI Rogue is labelled as a concept")
        audit_console(errors, "vision")
        ctx.close()

        # ---- interaction: navigation, pause, captions
        ctx, page, errors, requests = new_page(browser)
        page.goto(page_url(1, snap=False, mute=True), wait_until="load")
        wait_ready(page)
        t0 = time.perf_counter()
        page.keyboard.press("ArrowRight")
        page.wait_for_function("document.body.dataset.page === '2'", timeout=1000)
        elapsed = time.perf_counter() - t0
        check(elapsed < 0.3, f"controls :: ArrowRight moves to poster 2 in {elapsed*1000:.0f} ms")
        page.keyboard.press("ArrowLeft")
        page.wait_for_timeout(420)
        check(page.evaluate("document.body.dataset.page") == "1", "controls :: ArrowLeft returns to poster 1")

        page.evaluate("document.activeElement && document.activeElement.blur()")
        page.keyboard.press("Space")
        page.wait_for_timeout(220)
        check(page.evaluate("window.__SHOWCASE__.state().playing") is False, "controls :: Space pauses")
        page.keyboard.press("Space")
        page.wait_for_timeout(420)
        check(page.evaluate("window.__SHOWCASE__.state().playing") is True, "controls :: Space resumes")

        caption_a = page.locator("#ccZh").inner_text()
        try:
            page.wait_for_function("prev => document.querySelector('#ccZh').innerText !== prev", arg=caption_a, timeout=8000)
            caption_b = page.locator("#ccZh").inner_text()
            changed = bool(caption_b) and caption_a != caption_b
        except Exception:
            changed = False
            caption_b = page.locator("#ccZh").inner_text()
        check(changed, f"captions :: progression across chunks ({caption_a[:12]!r} -> {caption_b[:12]!r})")
        page.keyboard.press("KeyC")
        page.wait_for_timeout(400)
        check(page.evaluate("document.querySelector('#caption').classList.contains('is-hidden')"), "controls :: C toggles captions")

        if page.evaluate("window.__SHOWCASE__.state().mode") == "audio":
            page.evaluate("""() => {
                const a = document.querySelector('#audioBank audio');
                const d = window.__SHOWCASE__.data.pages[0].duration;
                try { a.currentTime = Math.max(0, d - 0.2); } catch (e) {}
                a.play();
            }""")
            page.wait_for_timeout(2300)
            check(page.evaluate("document.body.dataset.page") == "2", "auto-advance :: narration end moves to poster 2")
        else:
            record(False, "auto-advance :: test requires audio mode")

        audit_console(errors, "controls")
        ctx.close()

        # ---- paused start mode
        ctx, page, errors, requests = new_page(browser)
        page.goto(INDEX.resolve().as_uri() + "?page=1&autoplay=0&mute=1", wait_until="load")
        wait_ready(page)
        check(page.evaluate("window.__SHOWCASE__.state().playing") is False, "autoplay=0 :: starts paused")
        check(page.locator("#playBtn").inner_text().strip() == "PLAY", "autoplay=0 :: button reads PLAY")
        audit_console(errors, "autoplay=0")
        ctx.close()

        # ---- responsive stage scaling
        ctx = browser.new_context(viewport={"width": 1366, "height": 768}, device_scale_factor=1)
        page = ctx.new_page()
        page.goto(page_url(2), wait_until="load")
        wait_ready(page)
        transform = page.evaluate("getComputedStyle(document.querySelector('#stage')).transform")
        try:
            scale = float(transform.split("(")[1].split(",")[0])
        except Exception:
            scale = 0.0
        check(0.69 <= scale <= 0.73, f"responsive :: 1366x768 scales the 1920 stage ({transform})")
        check(page.evaluate("document.documentElement.scrollWidth <= window.innerWidth"), "responsive :: no horizontal overflow")
        ctx.close()

        # ---- autoplay blocked -> silent clock -> one click restores sound
        blocker = """
        (() => {
          const orig = HTMLMediaElement.prototype.play;
          let first = true;
          HTMLMediaElement.prototype.play = function () {
            if (first) { first = false; return Promise.reject(new DOMException('blocked', 'NotAllowedError')); }
            return orig.apply(this, arguments);
          };
        })();
        """
        ctx, page, errors, requests = new_page(browser, blocker)
        page.goto(page_url(1, snap=False, mute=False), wait_until="load")
        wait_ready(page)
        page.wait_for_timeout(900)
        check(page.evaluate("document.querySelector('#stage').dataset.mode") == "silent", "fallback :: blocked autoplay enters silent clock")
        check(page.evaluate("!document.querySelector('#soundGate').hidden"), "fallback :: click-for-sound affordance is visible")
        before = page.evaluate("window.__SHOWCASE__.state().time")
        page.wait_for_timeout(1200)
        after = page.evaluate("window.__SHOWCASE__.state().time")
        check(after > before + 0.4, "fallback :: silent clock advances captions")
        page.locator("#soundGate").click()
        page.wait_for_timeout(900)
        check(page.evaluate("document.querySelector('#stage').dataset.mode") == "audio", "fallback :: one click restores audio")
        check(page.evaluate("document.querySelector('#soundGate').hidden"), "fallback :: gate hides after audio starts")
        audit_console(errors, "fallback")
        ctx.close()

        browser.close()

    EVIDENCE.mkdir(parents=True, exist_ok=True)
    summary = f"\n{len(PASSES)} passed, {len(FAILS)} failed"
    print(summary, flush=True)
    LOG.append(summary)
    LOG_PATH.write_text("\n".join(LOG) + "\n", encoding="utf-8")
    print(f"log: {LOG_PATH}", flush=True)
    return 0 if not FAILS else 1


if __name__ == "__main__":
    raise SystemExit(main())
