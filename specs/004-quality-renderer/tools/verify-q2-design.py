#!/usr/bin/env python3
"""Structural and anchor checks for the S4 Q2 design documents.

This is a checker, not a technical review. It verifies:
- the three documents exist and contain the required sections/IDs;
- Markdown tables are contiguous, have one header, and every row has the header's cell count;
- relative Markdown links resolve;
- no drive-letter absolute paths appear;
- every `file:line` reference resolves to an existing file and an in-range line;
- the motion fixture table matches the generator (delegated to motion_fixtures.py --check).

Exit 0 only when every check passes.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
DOCS = ROOT / "specs" / "004-quality-renderer"
SUB = ROOT / "doom3_orig" / "doom3"

FILES = {
    "classic-parity.md": (
        "## 1. What \"parity\" means here",
        "## 2. Q0 input and its real status",
        "## 5. Coverage matrix",
        "## 6. Thresholds",
        "## 8. Per-case result record (T039)",
        "## 9. What Q2 would have to prove",
    ),
    "engine-integration.md": (
        "## 2. Current build topology (facts)",
        "## 3. Delivery model",
        "## 4. Both-executable linkage",
        "## 5. Selectable path and fallback",
        "## 6. Command-stream contract",
        "## 8. Verification plan",
    ),
    "motion-fixtures.md": (
        "## 2. Normative conventions",
        "## 4. Expected vectors",
        "## 5. Tolerances and storage",
        "## 6. Validity matrix",
        "## 8. Mutation controls",
    ),
    "evidence-q2-design.md": (
        "## 1. Scope and inputs",
        "## 7. Synthetic CMake linkage probe",
        "## 10. Verdict",
    ),
    "review-q2-design.md": (
        "## 1. Round 1",
        "## 4. Residual findings / not approved",
    ),
}

ABS_PATH = re.compile(r"(?<![A-Za-z0-9])[A-Za-z]:[\\/]")
LINK = re.compile(r"\[[^\]]+\]\(([^)]+)\)")
ANCHOR = re.compile(
    r"(?<![A-Za-z0-9_./-])"
    r"((?:[A-Za-z0-9_.-]+/)*[A-Za-z0-9_.-]+\.(?:cpp|h|hpp|ps1|txt|json|cmake|md))"
    r":(\d+(?:,\d+)*(?:-\d+)?)"
)

BARE_RENDERER = {
    "draw_common.cpp", "draw_arb2.cpp", "tr_render.cpp", "tr_main.cpp", "tr_local.h",
    "tr_backend.cpp", "tr_guisurf.cpp", "tr_subview.cpp", "RenderSystem.cpp",
    "RenderSystem_init.cpp", "GuiModel.cpp", "Material.cpp", "Material.h",
    "Interaction.cpp", "tr_light.cpp", "Model_md5.cpp", "Model_prt.cpp", "Model_local.h",
    "ModelDecal.h", "ModelOverlay.h", "Cinematic.cpp", "RenderWorld.h",
    "RenderWorld_load.cpp", "RenderWorld.cpp", "VertexCache.h", "Common.cpp",
    "Image_load.cpp", "Image_init.cpp", "Game_local.cpp", "Random.h",
}


def visible(text):
    """Drop fenced code and HTML comments for structural checks."""
    out = []
    fence = None
    in_comment = False
    for line in text.splitlines(keepends=True):
        if fence is not None:
            if re.match(r"^ {0,3}" + re.escape(fence[0]) + "{" + str(fence[1]) + r",}[ \t]*$",
                        line.rstrip("\r\n")):
                fence = None
            out.append("\n" if line.endswith("\n") else "")
            continue
        pieces = []
        pos = 0
        while pos < len(line):
            if in_comment:
                end = line.find("-->", pos)
                if end < 0:
                    break
                pos = end + 3
                in_comment = False
            else:
                start = line.find("<!--", pos)
                if start < 0:
                    pieces.append(line[pos:])
                    break
                pieces.append(line[pos:start])
                pos = start + 4
                in_comment = True
        content = "".join(pieces)
        opening = re.match(r"^ {0,3}(`{3,}|~{3,})([^\r\n]*)", content)
        if opening and not (opening.group(1)[0] == "`" and "`" in opening.group(2)):
            fence = (opening.group(1)[0], len(opening.group(1)))
            out.append("\n" if line.endswith("\n") else "")
        else:
            out.append(content)
    return "".join(out)


def cells(line):
    text = line.strip()
    if text.startswith("|"):
        text = text[1:]
    if text.endswith("|"):
        text = text[:-1]
    return [c.strip() for c in text.split("|")]


def check_tables(name, text):
    errors = []
    lines = text.splitlines()
    i = 0
    tables = 0
    while i < len(lines):
        if not re.match(r"^ {0,3}\|", lines[i]):
            i += 1
            continue
        header = cells(lines[i])
        if i + 1 >= len(lines) or not re.fullmatch(
                r" {0,3}\|(?:[ \t]*:?-+:?[ \t]*\|)+[ \t]*", lines[i + 1]):
            errors.append(f"{name}:{i + 1}: table header has no adjacent separator")
            i += 1
            continue
        sep = cells(lines[i + 1])
        if len(sep) != len(header):
            errors.append(f"{name}:{i + 2}: separator has {len(sep)} cells, header has {len(header)}")
        tables += 1
        i += 2
        while i < len(lines) and re.match(r"^ {0,3}\|", lines[i]):
            row = cells(lines[i])
            if len(row) != len(header):
                errors.append(f"{name}:{i + 1}: row has {len(row)} cells, header has {len(header)}")
            i += 1
    if tables == 0:
        errors.append(f"{name}: no tables found")
    return errors, tables


def check_links(name, text):
    errors = []
    for target in LINK.findall(text):
        target = target.strip()
        if target.startswith(("http://", "https://", "#", "mailto:")):
            continue
        path = (DOCS / target).resolve()
        if not path.exists():
            errors.append(f"{name}: missing relative link target: {target}")
    return errors


def resolve_anchor(path_text):
    candidates = []
    p = Path(path_text)
    if path_text.startswith("neo/"):
        candidates.append(SUB / path_text)
    if path_text.startswith(("targets/", "scripts/", "docs/", "specs/", "tests/")):
        candidates.append(ROOT / path_text)
    if path_text in {"CMakeLists.txt", "CMakePresets.json", "AGENTS.md"}:
        candidates.append(ROOT / path_text)
    if "/" not in path_text and path_text in BARE_RENDERER:
        candidates.append(SUB / "neo" / "renderer" / path_text)
    if "/" not in path_text and path_text in {"Common.cpp"}:
        candidates.append(SUB / "neo" / "framework" / path_text)
    if "/" not in path_text and path_text in {"Image_load.cpp", "Image_init.cpp"}:
        candidates.append(SUB / "neo" / "renderer" / path_text)
    if "/" not in path_text and path_text in {"Game_local.cpp"}:
        candidates.append(SUB / "neo" / "game" / path_text)
    if "/" not in path_text and path_text in {"Random.h"}:
        candidates.append(SUB / "neo" / "idlib" / "math" / path_text)
    if "/" not in path_text and path_text in {"win_wndproc.cpp"}:
        candidates.append(SUB / "neo" / "sys" / "win32" / path_text)
    if p.is_absolute():
        candidates.append(p)
    candidates.append(ROOT / path_text)
    for c in candidates:
        if c.is_file():
            return c
    return None


def check_anchors(name, text):
    errors = []
    seen = 0
    for match in ANCHOR.finditer(text):
        path_text, spec = match.group(1), match.group(2)
        resolved = resolve_anchor(path_text)
        if resolved is None:
            errors.append(f"{name}: anchor file not found: {path_text}")
            continue
        seen += 1
        line_count = sum(1 for _ in resolved.open(encoding="utf-8", errors="replace"))
        for part in spec.split(","):
            if "-" in part:
                start, end = part.split("-", 1)
            else:
                start = end = part
            try:
                start_i, end_i = int(start), int(end)
            except ValueError:
                errors.append(f"{name}: bad line spec {spec} for {path_text}")
                continue
            if start_i < 1 or end_i < start_i or end_i > line_count:
                errors.append(
                    f"{name}: anchor out of range {path_text}:{spec} (file has {line_count} lines)")
    return errors, seen


def main():
    errors = []
    texts = {}
    for name, sections in FILES.items():
        path = DOCS / name
        if not path.is_file():
            errors.append(f"missing document: {name}")
            continue
        raw = path.read_text(encoding="utf-8")
        texts[name] = raw
        if len(raw.strip()) < 2000:
            errors.append(f"{name}: suspiciously short ({len(raw)} bytes)")
        for section in sections:
            if section not in raw:
                errors.append(f"{name}: missing section {section!r}")
        if ABS_PATH.search(raw):
            errors.append(f"{name}: drive-letter absolute path found")
        if "classid" in raw:
            errors.append(f"{name}: typo 'classid'")
        table_errors, count = check_tables(name, visible(raw))
        errors.extend(table_errors)
        errors.extend(check_links(name, raw))
        anchor_errors, count_anchors = check_anchors(name, visible(raw))
        errors.extend(anchor_errors)
        print(f"{name}: {count} tables, {count_anchors} anchors")

    classic = texts.get("classic-parity.md", "")
    for i in range(1, 31):
        cid = f"CP-{i:02d}"
        if cid not in classic:
            errors.append(f"classic-parity.md: missing coverage row {cid}")
    for i in range(1, 11):
        tid = f"T-{i:02d}"
        if tid not in classic:
            errors.append(f"classic-parity.md: missing threshold {tid}")
    for token in ("FR-006", "FR-007", "SC-003", "T032", "T039", "T040"):
        if token not in classic:
            errors.append(f"classic-parity.md: missing trace token {token}")

    engine = texts.get("engine-integration.md", "")
    for token in ("doom3_rhi", "DOOM3XP", "/MT", "r_renderApi", "SC-006b", "T033",
                  "T035", "T036", "T080"):
        if token not in engine:
            errors.append(f"engine-integration.md: missing token {token}")

    motion = texts.get("motion-fixtures.md", "")
    for cid in ("CAM-01", "CAM-02", "CAM-03", "CAM-04", "CAM-05", "CAM-06",
                "RIG-01", "RIG-02", "RIG-03", "SKN-01", "SKN-02", "SKN-03",
                "DEF-01", "VM-01", "VM-02", "SEQ-01", "SEQ-02"):
        if cid not in motion:
            errors.append(f"motion-fixtures.md: missing fixture {cid}")
    for cid in (f"VAL-{i:02d}" for i in range(1, 11)):
        if cid not in motion:
            errors.append(f"motion-fixtures.md: missing validity case {cid}")
    for token in ("top-left", "ULP_half", "previous rendered", "provider"):
        if token not in motion:
            errors.append(f"motion-fixtures.md: missing convention token {token}")

    # T041's delivery path: the tests-tree copy must be byte-identical to the design mirror.
    test_motion = ROOT / "tests" / "renderer" / "motion-fixtures.md"
    if not test_motion.is_file():
        errors.append("missing T041 delivery path: tests/renderer/motion-fixtures.md")
    elif test_motion.read_bytes() != (DOCS / "motion-fixtures.md").read_bytes():
        errors.append("tests/renderer/motion-fixtures.md is not byte-identical to the design mirror")

    if errors:
        for error in errors:
            print("FAIL:", error)
        return 1

    # Delegate the numeric table to the generator, for both the design mirror and the T041 path.
    for motion_doc in (DOCS / "motion-fixtures.md", ROOT / "tests" / "renderer" / "motion-fixtures.md"):
        result = subprocess.run(
            [sys.executable, "-I", str(DOCS / "tools" / "motion_fixtures.py"),
             "--check", str(motion_doc)],
            cwd=str(ROOT), text=True, capture_output=True)
        print(f"{motion_doc.relative_to(ROOT)}: {result.stdout.strip()}")
        if result.returncode != 0:
            print(result.stderr.strip())
            return 1
    print("PASS: Q2 design documents are structurally consistent, anchors resolve, the T041 mirror matches, and motion vectors match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
