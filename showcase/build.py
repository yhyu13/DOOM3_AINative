# /// script
# requires-python = ">=3.11"
# dependencies = ["edge-tts==7.3.1", "mutagen>=1.47"]
# ///
"""Build the self-contained showcase.

Reads the authored shell (head.html, base.css, posters.css, body.html,
runtime.canvas.js, runtime.core.js) and narration.json, synthesises the
Chinese voice-over with Edge TTS, captures word boundaries, and writes one
offline `showcase/index.html` with the MP3s embedded as base64.

Usage:
    uv run --python 3.12 showcase/build.py
    uv run --python 3.12 showcase/build.py --hero path/to/capture.mp4
"""
from __future__ import annotations

import argparse
import asyncio
import base64
import hashlib
import io
import json
import re
import sys
from pathlib import Path

import edge_tts
from mutagen.mp3 import MP3

ROOT = Path(__file__).resolve().parent
SRC = ROOT / "src"
OUT = ROOT / "index.html"
CACHE = ROOT / "evidence" / "tts-cache.json"
DEFAULT_HERO = ROOT / "assets" / "hero-redacted.png"
DEFAULT_HERO_CREDIT = "LICENSING PENDING · NOT A LICENSED RELEASE"
DEFAULT_HERO_LABEL = "REDACTED CAPTURE"
WORD_RE = re.compile(r"\w", re.UNICODE)


def read(name: str) -> str:
    return (SRC / name).read_text(encoding="utf-8").lstrip("\ufeff")


def clean_map(text: str) -> tuple[str, list[int]]:
    """Lowercased word characters and the original index of each one."""
    out: list[str] = []
    orig: list[int] = []
    for i, ch in enumerate(text):
        if WORD_RE.match(ch):
            out.append(ch.lower())
            orig.append(i)
    return "".join(out), orig


async def synthesise(text: str, voice: str, rate: str, volume: str, pitch: str) -> tuple[bytes, list[dict]]:
    last: Exception | None = None
    for attempt in range(3):
        try:
            audio = bytearray()
            words: list[dict] = []
            com = edge_tts.Communicate(
                text,
                voice,
                rate=rate,
                volume=volume,
                pitch=pitch,
                boundary="WordBoundary",
            )
            async for chunk in com.stream():
                if chunk["type"] == "audio":
                    audio.extend(chunk["data"])
                elif chunk["type"] == "WordBoundary":
                    words.append(
                        {
                            "t": round(chunk["offset"] / 10_000_000, 3),
                            "d": round(chunk["duration"] / 10_000_000, 3),
                            "w": chunk["text"],
                        }
                    )
            if audio:
                return bytes(audio), words
            last = RuntimeError("Edge TTS returned no audio")
        except Exception as exc:  # network / service errors are retried
            last = exc
        await asyncio.sleep(1.4 * (attempt + 1))
    raise RuntimeError(f"Edge TTS failed after 3 attempts: {last}")


def assign_words(chunks: list[dict], words: list[dict], duration: float) -> None:
    """Map TTS word events into authored chunks and build character marks."""
    full_parts: list[str] = []
    chunk_start: list[int] = []
    pos = 0
    for ch in chunks:
        chunk_start.append(pos)
        full_parts.append(ch["zh"])
        pos += len(ch["zh"]) + 1  # the space used when joining for TTS
    full_text = " ".join(full_parts)

    chunk_of_orig = [0] * len(full_text)
    pos = 0
    for ci, ch in enumerate(chunks):
        text = ch["zh"]
        for j in range(len(text)):
            chunk_of_orig[pos + j] = ci
        pos += len(text) + 1

    clean, orig = clean_map(full_text)
    per_words: list[list[dict]] = [[] for _ in chunks]
    per_marks: list[list[dict]] = [[] for _ in chunks]
    cursor = 0
    matched = 0

    for event in words:
        needle, _ = clean_map(event["w"])
        if not needle:
            continue
        found = clean.find(needle, cursor)
        if found < 0:
            found = clean.find(needle)
        if found < 0:
            continue
        start_orig = orig[found]
        end_orig = orig[found + len(needle) - 1] + 1
        ci = chunk_of_orig[start_orig] if start_orig < len(chunk_of_orig) else 0
        chunk = chunks[ci]
        local_s = max(0, start_orig - chunk_start[ci])
        local_e = min(len(chunk["zh"]), end_orig - chunk_start[ci])
        if local_e > local_s:
            per_marks[ci].append({"s": local_s, "e": local_e, "t": event["t"], "d": event["d"]})
        per_words[ci].append(event)
        cursor = found + len(needle)
        matched += 1

    if not matched:
        # Fallback: proportional timing with no word highlighting.
        total_chars = sum(max(1, len(c["zh"])) for c in chunks) or 1
        acc = 0.0
        for ch in chunks:
            weight = max(1, len(ch["zh"])) / total_chars
            ch["start"] = round(acc, 3)
            ch["end"] = round(acc + weight * duration, 3)
            ch["words"] = []
            ch["marks"] = []
            acc += weight * duration
    else:
        for ci, ch in enumerate(chunks):
            ws = per_words[ci]
            marks = per_marks[ci]
            ch["words"] = ws
            ch["marks"] = marks
            if ws:
                ch["start"] = round(min(w["t"] for w in ws), 3)
                ch["end"] = round(max(w["t"] + w["d"] for w in ws) + 0.24, 3)
            else:
                ch["start"] = round(chunks[ci - 1]["end"] if ci else 0.0, 3)
                ch["end"] = round(ch["start"] + 0.9, 3)

    # Keep chunks ordered and inside the audio.
    prev_end = 0.0
    for ci, ch in enumerate(chunks):
        ch["start"] = max(prev_end, min(float(ch.get("start", 0)), max(0.0, duration - 0.15)))
        ch["end"] = min(duration, max(float(ch.get("end", 0)), ch["start"] + 0.45))
        if ci + 1 < len(chunks):
            nxt = float(chunks[ci + 1].get("start", duration))
            if ch["end"] > nxt - 0.02:
                ch["end"] = max(ch["start"] + 0.35, nxt - 0.02)
        prev_end = ch["end"]
    if chunks:
        chunks[0]["start"] = 0.0
        chunks[-1]["end"] = duration


async def build(hero: Path | None, out: Path, hero_credit: str | None, hero_label: str | None, no_hero: bool) -> int:
    import time as _time

    spec = json.loads((SRC / "narration.json").read_text(encoding="utf-8"))
    cache: dict = {}
    if CACHE.exists():
        try:
            cache = json.loads(CACHE.read_text(encoding="utf-8"))
        except Exception:
            cache = {}
    voice = spec.get("voice", "zh-CN-YunxiNeural")
    rate = spec.get("rate", "+0%")
    volume = spec.get("volume", "+0%")
    pitch = spec.get("pitch", "+0Hz")

    pages: list[dict] = []
    total = 0.0
    for page in spec["pages"]:
        text = " ".join(c["zh"] for c in page["chunks"])
        key = hashlib.sha256(
            json.dumps({"voice": voice, "rate": rate, "volume": volume, "pitch": pitch, "text": text},
                       ensure_ascii=False, sort_keys=True).encode("utf-8")
        ).hexdigest()
        cached = cache.get(key)
        if cached and cached.get("audio") and cached.get("words") is not None:
            audio = base64.b64decode(cached["audio"])
            words = cached["words"]
            duration = float(cached.get("duration") or 0.0)
            print(f"[cache] {page['id']}: {len(page['chunks'])} chunks, {len(text)} chars, {duration:5.2f}s", flush=True)
        else:
            print(f"[tts] {page['id']}: {len(page['chunks'])} chunks, {len(text)} chars...", flush=True)
            audio, words = await synthesise(text, voice, rate, volume, pitch)
            try:
                duration = float(MP3(io.BytesIO(audio)).info.length)
            except Exception:
                duration = (max((w["t"] + w["d"] for w in words), default=0.0) + 0.8) or 12.0
            cache[key] = {
                "duration": round(duration, 3),
                "audio": base64.b64encode(audio).decode("ascii"),
                "words": words,
            }
        chunks = [dict(c) for c in page["chunks"]]
        assign_words(chunks, words, duration)
        total += duration
        pages.append(
            {
                "id": page["id"],
                "name": page.get("name", page["id"].upper()),
                "label": page.get("label", page["id"].upper()),
                "duration": round(duration, 3),
                "audio": "data:audio/mpeg;base64," + base64.b64encode(audio).decode("ascii"),
                "chunks": chunks,
            }
        )
        print(
            f"      duration {duration:6.2f}s | audio {len(audio)/1024:7.1f} KiB | words {len(words):3d}",
            flush=True,
        )

    hero = hero.resolve() if hero else None
    default_hero = DEFAULT_HERO.resolve()
    if hero is None and not no_hero and DEFAULT_HERO.exists():
        if hero_label or hero_credit:
            print("WARNING: --hero-label/--hero-credit are ignored without --hero; using redacted defaults.", file=sys.stderr)
        hero = default_hero
        hero_credit = DEFAULT_HERO_CREDIT
        hero_label = DEFAULT_HERO_LABEL
        print("[hero] using the default redacted capture", flush=True)

    hero_data = None
    if hero:
        if not hero.exists():
            print(f"ERROR: hero media not found: {hero}", file=sys.stderr)
            return 2
        raw = hero.read_bytes()
        suffix = hero.suffix.lower()
        mime = {
            ".mp4": "video/mp4", ".webm": "video/webm", ".mov": "video/quicktime",
            ".png": "image/png", ".jpg": "image/jpeg", ".jpeg": "image/jpeg", ".webp": "image/webp",
        }.get(suffix)
        if not mime:
            print(f"ERROR: unsupported hero media type: {suffix} (use mp4, webm, mov, png, jpg, jpeg or webp)", file=sys.stderr)
            return 2
        if len(raw) > 8 * 1024 * 1024:
            print(f"WARNING: hero media is {len(raw)/1024/1024:.1f} MiB; the deck will exceed the 4 MiB budget.", file=sys.stderr)
        elif len(raw) > 2.5 * 1024 * 1024:
            print(f"WARNING: hero media is {len(raw)/1024/1024:.1f} MiB; aim for <= 2.5 MiB to stay near the 4 MiB deck budget.", file=sys.stderr)
        hero_type = "image" if mime.startswith("image/") else "video"
        is_default = hero == default_hero
        credit = (hero_credit or (DEFAULT_HERO_CREDIT if is_default else "LOCAL / NOT IN GIT")).strip()
        label = hero_label or (DEFAULT_HERO_LABEL if is_default else f"LICENSED {hero_type.upper()}")
        hero_data = {
            "type": hero_type,
            "src": f"data:{mime};base64," + base64.b64encode(raw).decode("ascii"),
            "credit": credit,
            "label": label,
        }
        print(f"[hero] inlined {hero.name} ({len(raw)/1024/1024:.2f} MiB, {mime}, {hero_type})", flush=True)
        print(f"[hero] label : {label}", flush=True)
        print(f"[hero] credit: {credit}", flush=True)

    css = read("base.css") + "\n" + read("posters.css")
    head = read("head.html").replace("<!--__CSS__-->", "<style>\n" + css + "\n</style>")
    body = read("body.html")
    frame = read("runtime.frame.js")
    canvas = read("runtime.canvas.js")
    core = read("runtime.core.js")
    data = {"pages": pages, "hero": hero_data}
    data_json = json.dumps(data, ensure_ascii=False).replace("<", "\\u003c")

    html = (
        head
        + "\n"
        + body
        + "\n<script>window.SHOWCASE_DATA = "
        + data_json
        + ";</script>\n<script>\n"
        + frame
        + "\n</script>\n<script>\n"
        + canvas
        + "\n</script>\n<script>\n"
        + core
        + "\n</script>\n</body>\n</html>\n"
    )
    CACHE.parent.mkdir(parents=True, exist_ok=True)
    CACHE.write_text(json.dumps(cache, ensure_ascii=False), encoding="utf-8")
    html = html.replace("\ufeff", "")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(html, encoding="utf-8", newline="\n")
    size = out.stat().st_size
    try:
        shown = out.relative_to(ROOT.parent)
    except ValueError:
        shown = out
    print("")
    print(f"generated : {shown}")
    print(f"size      : {size/1024:.1f} KiB ({size/1024/1024:.2f} MiB)")
    print(f"narration : {total:.1f}s across {len(pages)} posters")
    print(f"built     : {_time.strftime('%Y-%m-%d %H:%M:%S')}")
    if not (50 <= total <= 85):
        print(f"WARNING: total narration {total:.1f}s is outside the 50–85s target.", file=sys.stderr)
    if size > 4 * 1024 * 1024:
        print("WARNING: index.html exceeds the 4 MiB budget.", file=sys.stderr)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Build the single-file DOOM3_AINative showcase.")
    ap.add_argument("--hero", type=Path, help="Optional licensed image or video (png/jpg/webp/mp4/webm/mov) to inline on poster 1.")
    ap.add_argument("--hero-credit", type=str, default=None, help="Attribution shown on the media badge.")
    ap.add_argument("--hero-label", type=str, default=None, help="Badge label (default: REDACTED CAPTURE or LICENSED IMAGE/VIDEO).")
    ap.add_argument("--no-hero", action="store_true", help="Ignore the default redacted asset and use procedural visuals.")
    ap.add_argument("--out", type=Path, default=OUT, help="Output HTML path (default: showcase/index.html).")
    args = ap.parse_args()
    try:
        return asyncio.run(build(args.hero, args.out, args.hero_credit, args.hero_label, args.no_hero))
    except KeyboardInterrupt:
        return 130
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
