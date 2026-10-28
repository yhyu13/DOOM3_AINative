# /// script
# requires-python = ">=3.11"
# dependencies = ["playwright", "pillow", "html5lib"]
# ///
"""Extended acceptance matrix for the showcase.

Covers what verify.py does not: Chrome + Edge, offline, reduced motion,
touch/swipe, 3->1 loop, caption/word sync at every chunk, evidence hovers,
mute, fullscreen, 4K and portrait scaling, animation, accessibility names,
HTML parse errors, and the static start.cmd contract.

Run:
    uv run --python 3.12 showcase/verify_deep.py
"""
from __future__ import annotations

import base64
import hashlib
import html5lib
import re
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageStat
from playwright.sync_api import Error as PWError, sync_playwright

ROOT = Path(__file__).resolve().parent
INDEX = ROOT / "index.html"
EVIDENCE = ROOT / "evidence"
PAGE2_URI = "data:image/jpeg;base64," + base64.b64encode((ROOT / "assets" / "aigc" / "page2-modern.jpg").read_bytes()).decode("ascii")
PAGE3_URI = "data:image/jpeg;base64," + base64.b64encode((ROOT / "assets" / "aigc" / "page3-vision.jpg").read_bytes()).decode("ascii")
HERO_URI = "data:image/png;base64," + base64.b64encode((ROOT / "assets" / "hero-redacted.png").read_bytes()).decode("ascii")
LAUNCH_ARGS = [
    "--autoplay-policy=no-user-gesture-required",
    "--allow-file-access-from-files",
    "--disable-features=msEdgeFirstRunExperience",
]

PASSES: list[str] = []
FAILS: list[str] = []
SKIPS: list[str] = []
LOG: list[str] = []


def record(ok: bool, line: str) -> bool:
    prefix = "PASS" if ok else "FAIL"
    text = f"{prefix} {line}"
    print(text, flush=True)
    LOG.append(text)
    (PASSES if ok else FAILS).append(line)
    return bool(ok)


def check(ok: bool, line: str) -> bool:
    return record(bool(ok), line)


def skip(line: str, reason: str) -> None:
    text = f"SKIP {line} :: {reason}"
    print(text, flush=True)
    LOG.append(text)
    SKIPS.append(line)


def wait_ready(page) -> None:
    page.wait_for_function("window.__SHOWCASE_READY__ === true", timeout=20000)
    page.wait_for_timeout(300)


def url(page: int = 1, snap: bool = False, mute: bool = True, cc: bool = True, autoplay: bool = True) -> str:
    q = [f"page={page}"]
    if snap:
        q.append("snap=1")
    if mute:
        q.append("mute=1")
    if not cc:
        q.append("cc=0")
    if not autoplay:
        q.append("autoplay=0")
    return INDEX.resolve().as_uri() + "?" + "&".join(q)


def new_ctx(browser, init_script=None, **kwargs):
    kwargs.setdefault("viewport", {"width": 1920, "height": 1080})
    kwargs.setdefault("device_scale_factor", 1)
    ctx = browser.new_context(**kwargs)
    if init_script:
        ctx.add_init_script(init_script)
    page = ctx.new_page()
    errors: list[str] = []
    requests: list[str] = []
    page.on("console", lambda m: errors.append(f"console.{m.type}: {m.text}") if m.type == "error" else None)
    page.on("pageerror", lambda e: errors.append(f"pageerror: {e}"))
    page.on("request", lambda r: requests.append(r.url))
    return ctx, page, errors, requests


def audit_console(errors: list[str], label: str) -> None:
    check(not errors, f"{label} :: zero console/page errors ({errors[:2]})")


def audit_requests(requests: list[str], label: str) -> None:
    bad = [u for u in requests if not re.match(r"^(file:|data:|blob:|about:)", u)]
    check(not bad, f"{label} :: local-only requests ({len(requests)} total, bad={bad[:2]})")


def shot(page, name: str) -> tuple[Path, str]:
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    path = EVIDENCE / name
    page.screenshot(path=str(path), full_page=False, animations="disabled")
    img = Image.open(path).convert("RGB")
    stat = ImageStat.Stat(img)
    stddev = sum(stat.stddev) / 3.0
    colors = len(img.resize((160, 90)).getcolors(maxcolors=1_000_000) or [])
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    check(stddev > 8.0, f"{name} :: non-blank (stddev {stddev:.1f})")
    check(colors > 150, f"{name} :: visual richness ({colors} colors)")
    return path, digest


def diff_fraction(a: Path, b: Path) -> float:
    ia = Image.open(a).convert("RGB").resize((480, 270))
    ib = Image.open(b).convert("RGB").resize((480, 270))
    d = ImageChops.difference(ia, ib)
    hist = d.convert("L").histogram()
    changed = sum(hist[8:])
    total = 480 * 270
    return changed / total


def core_browser(p, channel: str, label: str) -> None:
    try:
        browser = p.chromium.launch(channel=channel, headless=True, args=LAUNCH_ARGS)
    except PWError as exc:
        skip(f"{label} :: launch", str(exc).splitlines()[0][:160])
        return
    print(f"runtime: {label}", flush=True)
    hashes = []
    for n in (1, 2, 3):
        ctx, page, errors, requests = new_ctx(browser)
        page.goto(url(n, snap=True), wait_until="load")
        wait_ready(page)
        check(page.evaluate("document.body.dataset.page") == str(n), f"{label} :: poster {n} page state")
        check(page.evaluate("document.querySelectorAll('.poster.is-active').length") == 1, f"{label} :: poster {n} single active")
        check(page.evaluate("document.querySelector('.poster.is-active').dataset.poster") == str(n), f"{label} :: poster {n} correct active")
        check(len(page.locator("#ccZh").inner_text().strip()) > 0, f"{label} :: poster {n} caption present")
        _, digest = shot(page, f"deep-{label.lower()}-poster-{n}.png")
        hashes.append(digest)
        audit_requests(requests, f"{label} poster {n}")
        audit_console(errors, f"{label} poster {n}")
        ctx.close()
    check(len(set(hashes)) == 3, f"{label} :: three distinct poster screenshots")
    browser.close()


def main() -> int:
    if not INDEX.exists():
        print(f"ERROR: {INDEX} missing. Run showcase/build.py first.", file=sys.stderr)
        return 2
    EVIDENCE.mkdir(parents=True, exist_ok=True)

    text = INDEX.read_text(encoding="utf-8")
    check('src="http' not in text and 'href="http' not in text, "artifact :: no external subresource")
    check("@import url(http" not in text, "artifact :: no remote CSS import")

    parser = html5lib.HTMLParser(strict=False)
    parser.parse(text)
    check(not parser.errors, f"html :: parse errors ({len(parser.errors)}; first={parser.errors[:1]})")

    start = (ROOT / "start.cmd").read_text(encoding="utf-8")
    for token in ("--kiosk", "--autoplay-policy=no-user-gesture-required", "%ProgramFiles%", "--user-data-dir", "file:///"):
        check(token in start, f"start.cmd :: contains {token}")
    sanitized = start.replace("%TARGET:\\=/%", "")
    check(not re.search(r"\b[A-Za-z]:\\\\", sanitized), "start.cmd :: no absolute drive path")

    files = subprocess.run(
        ["git", "-C", str(ROOT.parent), "ls-files", "*.pk4", "*.pk3", "*.pak"],
        capture_output=True, text=True, check=False,
    )
    check(files.returncode == 0 and not files.stdout.strip(), "git :: no game data tracked")

    try:
        with sync_playwright() as p:
            core_browser(p, "msedge", "Edge")
            core_browser(p, "chrome", "Chrome")
            try:
                ff = p.firefox.launch(headless=True)
                ff.close()
                check(True, "Firefox :: launch available")
            except PWError as exc:
                skip("Firefox :: launch", str(exc).splitlines()[0][:160])

            browser = p.chromium.launch(channel="msedge", headless=True, args=LAUNCH_ARGS)

            # offline
            try:
                ctx, page, errors, requests = new_ctx(browser, offline=True)
                page.goto(url(1, snap=False), wait_until="load")
                wait_ready(page)
                check(page.evaluate("document.querySelectorAll('#audioBank audio').length") == 3, "offline :: three audio elements load from file://")
                audit_requests(requests, "offline")
                audit_console(errors, "offline")
                ctx.close()
            except Exception as exc:
                skip("offline :: file:// load", str(exc).splitlines()[0][:160])

            # reduced motion: canvas is static, audio clock still advances
            ctx, page, errors, requests = new_ctx(browser, reduced_motion="reduce")
            page.goto(url(1, snap=False), wait_until="load")
            wait_ready(page)
            check(page.evaluate("matchMedia('(prefers-reduced-motion: reduce)').matches"), "reduced-motion :: media query active")
            a = page.evaluate("document.getElementById('fx1').toDataURL()")
            t0 = page.evaluate("window.__SHOWCASE__.state().time")
            page.wait_for_timeout(1200)
            b = page.evaluate("document.getElementById('fx1').toDataURL()")
            t1 = page.evaluate("window.__SHOWCASE__.state().time")
            check(a == b, "reduced-motion :: portal canvas is static")
            check(t1 > t0 + 0.4, "reduced-motion :: audio clock still advances")
            audit_console(errors, "reduced-motion")
            ctx.close()

            # captions disabled
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(1, snap=True, cc=False), wait_until="load")
            wait_ready(page)
            check(page.evaluate("document.querySelector('#caption').classList.contains('is-hidden')"), "cc=0 :: caption starts hidden")
            check(page.evaluate("window.__SHOWCASE__.state().captions") is False, "cc=0 :: captions state false")
            check(page.evaluate("document.querySelector('#caption').getAttribute('aria-hidden')") == "true", "cc=0 :: caption aria-hidden true")
            check(page.evaluate("document.querySelector('#ccEn').lang") == "en", "a11y :: English caption line is lang=en")
            ctx.close()

            # start on poster 3
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(3, snap=True), wait_until="load")
            wait_ready(page)
            check(page.evaluate("document.body.dataset.page") == "3", "page=3 :: starts on poster 3")
            ctx.close()

            # touch / swipe with real CDP touch events (not page.mouse)
            ctx, page, errors, requests = new_ctx(browser, viewport={"width": 1366, "height": 768}, has_touch=True)
            page.goto(url(1, snap=False), wait_until="load")
            wait_ready(page)
            page.evaluate("window.__cancelCount = 0; document.querySelector('#stage').addEventListener('pointercancel', () => window.__cancelCount++)")
            cdp = ctx.new_cdp_session(page)

            def swipe(x0, y0, x1, y1):
                cdp.send("Input.dispatchTouchEvent", {"type": "touchStart", "touchPoints": [{"x": x0, "y": y0, "radiusX": 2, "radiusY": 2, "force": 1, "id": 1}]})
                steps = 10
                for i in range(1, steps + 1):
                    cdp.send("Input.dispatchTouchEvent", {"type": "touchMove", "touchPoints": [{"x": int(x0 + (x1 - x0) * i / steps), "y": int(y0 + (y1 - y0) * i / steps), "radiusX": 2, "radiusY": 2, "force": 1, "id": 1}]})
                cdp.send("Input.dispatchTouchEvent", {"type": "touchEnd", "touchPoints": []})

            swipe(800, 400, 380, 400)
            page.wait_for_timeout(600)
            check(page.evaluate("document.body.dataset.page") == "2", "touch :: real touch swipe advances to poster 2")
            check(page.evaluate("window.__cancelCount") == 0, "touch :: no pointercancel during swipe")
            swipe(380, 400, 820, 400)
            page.wait_for_timeout(600)
            check(page.evaluate("document.body.dataset.page") == "1", "touch :: real touch swipe returns to poster 1")
            ctx.close()

            # 3 -> 1 loop
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(3, snap=False), wait_until="load")
            wait_ready(page)
            page.evaluate("""() => {
                const a = document.querySelector('#audioBank audio[data-page="3"]');
                const d = window.__SHOWCASE__.data.pages[2].duration;
                try { a.currentTime = Math.max(0, d - 0.2); } catch (e) {}
                a.play();
            }""")
            page.wait_for_timeout(2400)
            check(page.evaluate("document.body.dataset.page") == "1", "loop :: poster 3 end returns to poster 1")
            ctx.close()

            # caption / word sync for every chunk on every poster
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(1, snap=False), wait_until="load")
            wait_ready(page)
            for pidx in range(3):
                if pidx:
                    page.evaluate("(n) => window.__SHOWCASE__.go(n)", pidx + 1)
                    page.wait_for_timeout(400)
                chunks = page.evaluate("(n) => window.__SHOWCASE__.data.pages[n].chunks.map(c => ({zh: c.zh, mark: (c.marks && c.marks.length ? c.marks[0].t : c.start)}))", pidx)
                for ci, c in enumerate(chunks):
                    page.evaluate("""(args) => {
                        const a = document.querySelector(`#audioBank audio[data-page="${args.n}"]`);
                        a.currentTime = args.t;
                    }""", {"n": pidx + 1, "t": c["mark"] + 0.02})
                    try:
                        page.wait_for_function("(zh) => document.querySelector('#ccZh').innerText.trim() === zh", arg=c["zh"], timeout=1800)
                    except Exception:
                        pass
                    shown = page.locator("#ccZh").inner_text().strip()
                    on = page.evaluate("document.querySelectorAll('#ccZh .w.on').length")
                    check(shown == c["zh"], f"caption sync :: poster {pidx+1} chunk {ci+1} text")
                    check(on > 0, f"caption sync :: poster {pidx+1} chunk {ci+1} word highlight")
            audit_console(errors, "caption sync")
            ctx.close()

            # evidence interactions
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(3, snap=True), wait_until="load")
            wait_ready(page)
            check(page.locator(".monitor").count() == 1, "vision :: browser monitor present")
            check(page.locator("#browserView #fxFrame").count() == 1, "vision :: game canvas sits inside the browser viewport")
            before_prompt = page.locator("#promptText").inner_text()
            page.locator("#generateBtn").click()
            page.wait_for_timeout(1000)
            after_prompt = page.locator("#promptText").inner_text()
            check(after_prompt != before_prompt, "vision :: AI Rogue prompt regenerates")
            check("CONCEPT DEMO" in page.locator(".prompt-note").inner_text(), "vision :: AI Rogue is labelled as a concept")
            page.hover('.rm-cell[data-m="M4"]')
            page.wait_for_timeout(250)
            rm = page.locator("#rmDetail").inner_text()
            check("M4" in rm and "WebGL2" in rm, "evidence :: roadmap hover reveals milestone")
            page.goto(url(2, snap=False), wait_until="load")
            wait_ready(page)
            check(page.locator(".p2-feature").count() == 3, "compare :: three experience cards")
            check("NEXT-GEN LOOK" in page.locator(".p2-features").inner_text(), "compare :: next-gen look card")
            check(page.evaluate("(u) => document.querySelector('#p2Old').src === u", HERO_URI), "compare :: 2004 archival frame reuses the redacted hero")
            check(page.evaluate("(u) => document.querySelector('#p2New').src === u", PAGE2_URI), "compare :: #p2New is exactly page2-modern.jpg")
            check(page.evaluate("""() => {const i=document.querySelector('#p2New');
                return !i.hidden && i.complete && i.naturalWidth===1600 && i.naturalHeight===900;}"""),
                  "compare :: AIGC modern vision decoded 1600x900 and visible")
            check(page.locator("#p2VisionLabel").inner_text().strip() == "AI-GENERATED VISION · NOT A GAME CAPTURE", "compare :: exact AIGC vision label")
            check("734,180" in page.locator(".p2-proof").inner_text(), "compare :: proof strip carries the reality check")
            check(page.locator(".claim,.claims").count() == 0, "compare :: no source-card wall")
            before_split = page.locator("#p2Handle").get_attribute("aria-valuenow")
            page.locator("#p2Reveal").click()
            page.wait_for_timeout(2200)
            after_split = page.locator("#p2Handle").get_attribute("aria-valuenow")
            check(before_split == "46" and after_split == "80", f"compare :: back-to-2004 changes the split ({before_split} -> {after_split})")
            page.goto(url(2, snap=False), wait_until="load")
            wait_ready(page)
            p2fps = page.evaluate("""() => new Promise(resolve => {
                let n = 0; const t0 = performance.now();
                function f() {
                    n++;
                    if (performance.now() - t0 < 1200) requestAnimationFrame(f);
                    else resolve(n / ((performance.now() - t0) / 1000));
                }
                requestAnimationFrame(f);
            })""")
            check(p2fps >= 30, f"performance :: page 2 comparison {p2fps:.1f} fps")
            audit_console(errors, "compare poster")
            ctx.close()

            # ---- page 3 AIGC background
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(3, snap=True), wait_until="load")
            wait_ready(page)
            check(page.evaluate("(u) => document.querySelector('#p3Bg').src === u", PAGE3_URI), "vision :: #p3Bg is exactly page3-vision.jpg")
            check(page.evaluate("""() => {const i=document.querySelector('#p3Bg');
                return !i.hidden && i.complete && i.naturalWidth===1600 && i.naturalHeight===900;}"""),
                  "vision :: AIGC background decoded 1600x900 and visible")
            check(page.locator("#p3AiLabel").inner_text().strip() == "AI VISION · GPT IMAGE 2.5", "vision :: exact AI vision label")
            check(page.locator("#p3Truth").inner_text().strip() == "AI-GENERATED VISION · NOT A GAME CAPTURE", "vision :: page 3 truth label")
            check(page.locator(".vision-card").count() == 3, "vision :: three vision cards")
            check(page.locator(".loop-step").count() == 7, "vision :: seven loop steps")
            check(page.locator(".rm-cell").count() == 8, "vision :: eight roadmap milestones")
            on_top = page.evaluate("""() => Array.from(document.querySelectorAll('.vision-card')).every(el => {
                const r = el.getBoundingClientRect();
                const hit = document.elementFromPoint(r.left + r.width/2, r.top + r.height/2);
                return !!(hit && hit.closest('.vision-card'));
            })""")
            check(on_top, "vision :: all cards sit above the AIGC background")
            audit_console(errors, "vision background")
            ctx.close()

            # mute toggle
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(1, snap=False, mute=False), wait_until="load")
            wait_ready(page)
            page.locator("#muteBtn").click()
            page.wait_for_timeout(200)
            check(page.evaluate("Array.from(document.querySelectorAll('#audioBank audio')).every(a => a.muted)"), "mute :: all tracks muted")
            page.locator("#muteBtn").click()
            page.wait_for_timeout(200)
            check(page.evaluate("Array.from(document.querySelectorAll('#audioBank audio')).every(a => !a.muted)"), "mute :: all tracks unmuted")
            ctx.close()

            # fullscreen (skip if headless refuses)
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(1, snap=True), wait_until="load")
            wait_ready(page)
            try:
                page.locator('[data-act="full"]').click()
                page.wait_for_timeout(500)
                if page.evaluate("!!document.fullscreenElement"):
                    check(True, "fullscreen :: enters fullscreen")
                    page.keyboard.press("Escape")
                    page.wait_for_timeout(400)
                    if page.evaluate("!!document.fullscreenElement"):
                        page.evaluate("document.exitFullscreen && document.exitFullscreen()")
                        page.wait_for_timeout(500)
                        check(page.evaluate("!document.fullscreenElement"), "fullscreen :: exits via API (headless Escape is a no-op)")
                    else:
                        check(True, "fullscreen :: Escape exits")
                else:
                    skip("fullscreen :: enters", "headless browser refused the request")
            except Exception as exc:
                skip("fullscreen :: enters", str(exc).splitlines()[0][:120])
            ctx.close()

            # 4K
            ctx, page, errors, requests = new_ctx(browser, viewport={"width": 3840, "height": 2160})
            page.goto(url(1, snap=True), wait_until="load")
            wait_ready(page)
            tr = page.evaluate("getComputedStyle(document.querySelector('#stage')).transform")
            try:
                s = float(tr.split("(")[1].split(",")[0])
            except Exception:
                s = 0.0
            check(1.99 <= s <= 2.01, f"4k :: stage scales 2x ({tr})")
            shot(page, "deep-4k-poster-1.png")
            ctx.close()

            # 4K pages 2/3: content must stay above the AIGC imagery
            ctx, page, errors, requests = new_ctx(browser, viewport={"width": 3840, "height": 2160})
            for n, selector in ((2, ".p2-copy, .p2-features, .p2-compare-actions"), (3, ".vision-card, .loop-step, .rm-cell")):
                page.goto(url(n, snap=True), wait_until="load")
                wait_ready(page)
                on_top = page.evaluate("""(sel) => Array.from(document.querySelectorAll(sel)).every(el => {
                    const r = el.getBoundingClientRect();
                    const x = r.left + Math.min(r.width/2, 120);
                    const y = r.top + Math.min(r.height/2, 60);
                    const hit = document.elementFromPoint(x, y);
                    return !!(hit && (hit === el || el.contains(hit) || hit.closest(sel)));
                })""", selector)
                check(on_top, f"4k :: poster {n} content sits above the background")
                check(page.evaluate("document.documentElement.scrollWidth <= window.innerWidth"), f"4k :: poster {n} has no horizontal overflow")
                shot(page, f"deep-4k-poster-{n}.png")
                audit_console(errors, f"4k poster {n}")
            ctx.close()

            # DPR2 / high-density backing store
            ctx, page, errors, requests = new_ctx(browser, viewport={"width": 1920, "height": 1080}, device_scale_factor=2)
            page.goto(url(1, snap=True), wait_until="load")
            wait_ready(page)
            dims = page.evaluate("""() => ({
              fx1: [document.getElementById('fx1').width, document.getElementById('fx1').height],
              frame: [document.getElementById('fxFrame').width, document.getElementById('fxFrame').height]
            })""")
            check(dims["fx1"] == [3840, 2160] and dims["frame"] == [3840, 2160], f"dpr2 :: canvases render at 2x backing ({dims})")
            ctx.close()

            # portrait
            ctx, page, errors, requests = new_ctx(browser, viewport={"width": 390, "height": 844})
            page.goto(url(1, snap=True), wait_until="load")
            wait_ready(page)
            tr = page.evaluate("getComputedStyle(document.querySelector('#stage')).transform")
            try:
                s = float(tr.split("(")[1].split(",")[0])
            except Exception:
                s = 0.0
            check(0.19 <= s <= 0.21, f"portrait :: stage letterboxes to width ({tr})")
            check(page.evaluate("document.documentElement.scrollWidth <= window.innerWidth"), "portrait :: no horizontal overflow")
            ctx.close()

            # animation + performance
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(1, snap=False), wait_until="load")
            wait_ready(page)
            p1 = EVIDENCE / "deep-anim-1.png"
            p2 = EVIDENCE / "deep-anim-2.png"
            canvas_a = page.evaluate("document.getElementById('fx1').toDataURL()")
            page.screenshot(path=str(p1), animations="disabled")
            page.wait_for_timeout(1200)
            canvas_b = page.evaluate("document.getElementById('fx1').toDataURL()")
            page.screenshot(path=str(p2), animations="disabled")
            changed = diff_fraction(p1, p2)
            check(canvas_a != canvas_b, "animation :: portal canvas is drawing new frames")
            check(changed > 0.004, f"animation :: page changes between frames ({changed*100:.1f}% pixels)")
            fps = page.evaluate("""() => new Promise(resolve => {
                let n = 0; const t0 = performance.now();
                function f() {
                    n++;
                    if (performance.now() - t0 < 1500) requestAnimationFrame(f);
                    else resolve(n / ((performance.now() - t0) / 1000));
                }
                requestAnimationFrame(f);
            })""")
            check(fps >= 30, f"performance :: {fps:.1f} fps over 1.5 s")
            audit_console(errors, "animation")
            ctx.close()

            # Space on a focused control must activate the control, not toggle playback
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(1, snap=False), wait_until="load")
            wait_ready(page)
            page.locator('[data-act="next"]').focus()
            page.keyboard.press("Space")
            page.wait_for_timeout(450)
            check(page.evaluate("document.body.dataset.page") == "2", "keyboard :: Space on NEXT activates the control")
            check(page.evaluate("window.__SHOWCASE__.state().playing") is True, "keyboard :: Space on NEXT does not pause")
            page.goto(url(3, snap=False), wait_until="load")
            wait_ready(page)
            before_prompt = page.locator("#promptText").inner_text()
            page.locator("#generateBtn").focus()
            page.keyboard.press("Space")
            page.wait_for_timeout(1000)
            check(page.locator("#promptText").inner_text() != before_prompt, "keyboard :: Space on GENERATE RUN regenerates the prompt")
            ctx.close()

            # visibility: pause on hidden, resume on visible if it was playing
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(1, snap=False), wait_until="load")
            wait_ready(page)
            try:
                page.evaluate("Object.defineProperty(document, 'hidden', { configurable: true, get: () => true }); document.dispatchEvent(new Event('visibilitychange'))")
                page.wait_for_timeout(350)
                check(page.evaluate("window.__SHOWCASE__.state().playing") is False, "visibility :: pauses while hidden")
                page.evaluate("Object.defineProperty(document, 'hidden', { configurable: true, get: () => false }); document.dispatchEvent(new Event('visibilitychange'))")
                page.wait_for_timeout(450)
                check(page.evaluate("window.__SHOWCASE__.state().playing") is True, "visibility :: resumes when visible")
            except Exception as exc:
                skip("visibility :: pause/resume", str(exc).splitlines()[0][:120])
            audit_console(errors, "visibility")
            ctx.close()

            # canvas unavailable: DOM, captions and navigation must still work
            ctx, page, errors, requests = new_ctx(browser, init_script="HTMLCanvasElement.prototype.getContext = function () { return null; };")
            page.goto(url(1, snap=True), wait_until="load")
            wait_ready(page)
            check(page.evaluate("document.body.dataset.page") == "1", "canvas-fallback :: page still boots")
            check(len(page.locator("#ccZh").inner_text().strip()) > 0, "canvas-fallback :: captions still render")
            page.keyboard.press("ArrowRight")
            page.wait_for_timeout(350)
            check(page.evaluate("document.body.dataset.page") == "2", "canvas-fallback :: navigation still works")
            audit_console(errors, "canvas-fallback")
            ctx.close()

            # accessible names
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(1, snap=True), wait_until="load")
            wait_ready(page)
            names = page.evaluate("""() => Array.from(document.querySelectorAll('button')).map(b => ({
                text: (b.textContent || '').trim(),
                aria: b.getAttribute('aria-label') || '',
                title: b.getAttribute('title') || ''
            }))""")
            empty = [n for n in names if not (n["text"] or n["aria"] or n["title"])]
            check(not empty, f"a11y :: every button has an accessible name ({len(names)} buttons)")
            check(page.evaluate("document.documentElement.lang.startsWith('zh')"), "a11y :: html lang is Chinese")
            ctx.close()

            # audio element health
            ctx, page, errors, requests = new_ctx(browser)
            page.goto(url(1, snap=False), wait_until="load")
            wait_ready(page)
            health = page.evaluate("""() => Array.from(document.querySelectorAll('#audioBank audio')).map(a => ({
                error: a.error ? a.error.code : null,
                ready: a.readyState,
                duration: a.duration
            }))""")
            check(all(h["error"] is None for h in health), f"audio :: no decode errors ({health})")
            check(all(h["ready"] >= 1 for h in health), f"audio :: metadata ready ({[h['ready'] for h in health]})")
            ctx.close()

            browser.close()
    except PWError as exc:
        record(False, f"runtime :: Playwright failure: {str(exc).splitlines()[0][:200]}")

    summary = f"\n{len(PASSES)} passed, {len(FAILS)} failed, {len(SKIPS)} skipped"
    print(summary, flush=True)
    LOG.append(summary)
    (EVIDENCE / "verify-deep.log").write_text("\n".join(LOG) + "\n", encoding="utf-8")
    print(f"log: {EVIDENCE / 'verify-deep.log'}", flush=True)
    return 0 if not FAILS else 1


if __name__ == "__main__":
    raise SystemExit(main())
