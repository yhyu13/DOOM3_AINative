#!/usr/bin/env python3
"""Analytic motion fixtures for T041 (specs/004-quality-renderer/motion-fixtures.md).

This script is the reproducible source of the expected values in the document. It does not
render; it computes the camera/basis projection described by the pristine engine and emits the
Markdown rows used by the fixture contract. `--check` re-reads the document and fails on drift.
`--self-test` applies the mutations the document requires each fixture to detect.

Fixture convention: the engine's 16:9 pair from idGameLocal::CalcFov (base 90, 640x480 seed):
fov_x = 106.260205 deg, fov_y = 73.739795 deg, tan_x = 4/3, tan_y = 3/4. zNear is 3 but the
UV formula is independent of it. No game data, GPU, or engine build is used.
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

WIDTH = 1280
HEIGHT = 720
TAN_X = 4.0 / 3.0
TAN_Y = 3.0 / 4.0
Z_NEAR = 3.0  # r_znear default; not used by the UV formula, recorded for the fixture camera
TOL_FLOOR = 1.0e-7

AXIS = ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
STATE0 = ((0.0, 0.0, 0.0), AXIS)


def axis_yaw(deg: float):
    r = math.radians(deg)
    c, s = math.cos(r), math.sin(r)
    return ((c, s, 0.0), (-s, c, 0.0), (0.0, 0.0, 1.0))


def uv_of(point, eye, axis, jitter_px=(0.0, 0.0), origin="top-left"):
    """Project with the engine's basis and R_SetupProjection's infinite-far form.

    `origin="bottom-left"` is a deliberate mutation used by --self-test.
    """
    delta = [point[i] - eye[i] for i in range(3)]
    forward, left, up = axis
    depth = sum(forward[i] * delta[i] for i in range(3))
    if depth <= 0.0:
        return None
    x_view = -sum(left[i] * delta[i] for i in range(3))
    y_view = sum(up[i] * delta[i] for i in range(3))
    u = 0.5 + 0.5 * x_view / (depth * TAN_X)
    v = 0.5 - 0.5 * y_view / (depth * TAN_Y)
    if origin == "bottom-left":
        v = 1.0 - v
    u -= jitter_px[0] / WIDTH
    v += jitter_px[1] / HEIGHT
    return (u, v)


def motion(prev_point, curr_point, prev_state, curr_state, prev_jitter=(0, 0),
           curr_jitter=(0, 0), origin="top-left"):
    up = uv_of(prev_point, prev_state[0], prev_state[1], prev_jitter, origin)
    uc = uv_of(curr_point, curr_state[0], curr_state[1], curr_jitter, origin)
    if up is None or uc is None:
        return None, up, uc
    return (up[0] - uc[0], up[1] - uc[1]), up, uc


def half(x: float) -> float:
    return struct.unpack("e", struct.pack("e", x))[0]


def half_hex(x: float) -> str:
    """IEEE binary16 bit pattern (16-bit word), not the little-endian byte string."""
    return "0x%04x" % struct.unpack("<H", struct.pack("e", x))[0]


def ulp_half(x: float) -> float:
    h = half(abs(x))
    bits = struct.unpack("<H", struct.pack("e", h))[0]
    nxt = struct.unpack("e", struct.pack("<H", bits + 1))[0]
    return abs(nxt - h)


def tolerance(x: float) -> float:
    return max(ulp_half(x), TOL_FLOOR)


def fmt(x: float) -> str:
    return f"{x:.9f}"


# id, class, previous state, current state, previous point, current point, note
CASES = [
    ("CAM-01", "camera translation", ((0, 2, 0), AXIS), STATE0, (100, 0, 0), (100, 0, 0),
     "camera moves +Y (left) 2 units; the fixed point appears at u>0.5 in the previous frame"),
    ("CAM-02", "camera rotation", ((0, 0, 0), axis_yaw(2.0)), STATE0, (100, 0, 0), (100, 0, 0),
     "previous camera yaw +2 deg (left); positive du"),
    ("CAM-03", "camera / vertical", STATE0, STATE0, (100, 0, 1), (100, 0, 0),
     "point moves +Z (up) 1 unit; top-left origin makes dv negative"),
    ("CAM-04", "camera / zero", STATE0, STATE0, (100, 0, 0), (100, 0, 0),
     "stationary: (0,0) is valid, not invalid"),
    ("CAM-05", "camera / jitter exclusion", STATE0, STATE0, (100, 0, 0), (100, 0, 0),
     "r_jitter must be excluded from history; a history using the jittered projection is nonzero"),
    ("CAM-06", "invalid / behind camera", STATE0, STATE0, (-10, 0, 0), (-10, 0, 0),
     "current depth <= 0; invalid, not a zero vector"),
    ("CAM-07", "invalid / previous behind camera", STATE0, STATE0, (-10, 0, 0), (100, 0, 0),
     "previous depth <= 0; invalid even though the current point is in front"),
    ("RIG-01", "rigid translation", STATE0, STATE0, (100, 0.5, 0), (100, 0, 0),
     "object moves +Y (left) 0.5 units"),
    ("RIG-02", "rigid rotation", STATE0, STATE0,
     (100 * math.cos(math.radians(1.0)), 100 * math.sin(math.radians(1.0)), 0), (100, 0, 0),
     "object yaw +1 deg about Z"),
    ("RIG-03", "rigid depth / off-axis", STATE0, STATE0, (101, 1, 0), (100, 1, 0),
     "motion along the view ray but off-axis; a depth-ignoring renderer gives 0, so this detects scale"),
    ("RIG-04", "rigid depth / on-axis", STATE0, STATE0, (101, 0, 0), (100, 0, 0),
     "center ray; zero UV motion is valid"),
    ("SKN-01", "skinned blend", STATE0, STATE0, (100, 0, 1), (100, 0, 0.5),
     "two-bone 0.5/0.5 blend; previous bone 1 translates +Z 1, current is rest"),
    ("SKN-02", "skinned stationary", STATE0, STATE0, (100, 0, 0), (100, 0, 0),
     "bone 0 vertex; zero is valid"),
    ("SKN-03", "skinned rotation", STATE0, STATE0,
     (100 * math.cos(math.radians(2.0)), 100 * math.sin(math.radians(2.0)), 1), (100, 0, 1),
     "single-bone vertex (weight 1 on bone 1); previous bone yaw +2 deg; depth changes slightly"),
    ("DEF-01", "supported deformation", STATE0, STATE0, (100, 0, 0.3), (100, 0, -0.2),
     "same vertex identity, explicit deformation samples; topology unchanged"),
    ("VM-01", "viewmodel", STATE0, STATE0, (20, 0, 0.1), (20, 0, 0),
     "viewmodel at depth 20; weaponDepthHack changes z only, so x/y follows this formula"),
    ("VM-02", "viewmodel / zero", STATE0, STATE0, (20, 0, 0), (20, 0, 0),
     "stationary viewmodel; zero is valid"),
    ("SEQ-01", "rendered-frame history", STATE0, STATE0, (100, 0, 0.5), (100, 0, 1.0),
     "correct history: last rendered pose B(0.5) to current C(1.0)"),
    ("SEQ-02", "rendered-frame history / negative", STATE0, STATE0, (100, 0, 0.0), (100, 0, 1.0),
     "wrong history: last simulation tick A(0.0) to C(1.0); must fail against SEQ-01"),
    ("SEQ-03", "rendered-frame history / same pose", STATE0, STATE0, (100, 0, 1.0), (100, 0, 1.0),
     "same pose rendered twice; expected zero, even if a tick advanced"),
]

INVALID_CASES = [
    ("VAL-01", "stationary", "VALID", "-", "(0,0) with VALID"),
    ("VAL-02", "no previous rendered record", "INVALID", "NO_HISTORY", "no vector; never a valid (0,0)"),
    ("VAL-03", "previous UV outside [0,1]", "INVALID", "DISOCCLUSION", "no vector"),
    ("VAL-04", "previous or current depth <= 0", "INVALID", "BEHIND_CAMERA", "no vector"),
    ("VAL-05", "topology or vertex identity change", "INVALID", "TOPOLOGY", "no vector; identity included"),
    ("VAL-06", "renderer/provider switch", "INVALID", "PROVIDER_SWITCH", "no vector"),
    ("VAL-07", "history generation mismatch", "INVALID", "GENERATION", "no vector"),
    ("VAL-08", "cut / teleport / level change", "INVALID", "RESET", "T045; one reason for the reset family"),
    ("VAL-09", "resize / resolution-mode change", "INVALID", "RESIZE", "T045; one reason for the resize family"),
    ("VAL-10", "entity slot reuse", "INVALID", "ENTITY_REUSE", "T045"),
]


def compute(case):
    cid, cls, prev_state, curr_state, prev_p, curr_p, note = case
    vel, up, uc = motion(prev_p, curr_p, prev_state, curr_state)
    return cid, cls, prev_state, curr_state, prev_p, curr_p, note, vel, up, uc


def emit_markdown():
    print("| Case | Class | uv previous | uv current | du | dv | RG16F word (du, dv) | tolerance (du, dv) |")
    print("|---|---|---|---:|---:|---:|---|---|")
    for case in CASES:
        cid, cls, _, _, _, _, _, vel, up, uc = compute(case)
        if vel is None:
            print(f"| `{cid}` | {cls} | — | — | — | — | — | invalid: `BEHIND_CAMERA` |")
            continue
        du, dv = vel
        print(f"| `{cid}` | {cls} | ({fmt(up[0])}, {fmt(up[1])}) | ({fmt(uc[0])}, {fmt(uc[1])}) | "
              f"{fmt(du)} | {fmt(dv)} | ({half_hex(du)}, {half_hex(dv)}) | "
              f"({tolerance(du):.2e}, {tolerance(dv):.2e}) |")


def emit_validity():
    print("| Case | Condition | Expected | Reason | Notes |")
    print("|---|---|---|---|---|")
    for row in INVALID_CASES:
        print("| `%s` | %s | %s | `%s` | %s |" % row)


def _cells(line):
    text = line.strip()
    if text.startswith("|"):
        text = text[1:]
    if text.endswith("|"):
        text = text[:-1]
    return [c.strip() for c in text.split("|")]


def parse_table(text):
    rows = {}
    for line in text.splitlines():
        if not line.startswith("| `"):
            continue
        cells = _cells(line)
        if len(cells) < 8:
            continue
        cid = cells[0].strip("`")
        if cid in {c[0] for c in CASES}:
            rows[cid] = cells
    return rows


def check_doc(path: Path):
    text = path.read_text(encoding="utf-8")
    rows = parse_table(text)
    errors = []
    for case in CASES:
        cid = case[0]
        _, _, _, _, _, _, _, vel, up, uc = compute(case)
        if cid not in rows:
            errors.append(f"missing fixture row: {cid}")
            continue
        cells = rows[cid]
        if vel is None:
            if "BEHIND_CAMERA" not in " ".join(cells):
                errors.append(f"{cid}: expected BEHIND_CAMERA")
            continue
        du, dv = vel
        expected = [
            f"({fmt(up[0])}, {fmt(up[1])})",
            f"({fmt(uc[0])}, {fmt(uc[1])})",
            fmt(du),
            fmt(dv),
            f"({half_hex(du)}, {half_hex(dv)})",
            f"({tolerance(du):.2e}, {tolerance(dv):.2e})",
        ]
        labels = ("uv previous", "uv current", "du", "dv", "RG16F word", "tolerance")
        for label, value, actual in zip(labels, expected, cells[2:8]):
            if value not in actual:
                errors.append(f"{cid}: {label} expected {value!r}, document has {actual!r}")
    for row in INVALID_CASES:
        if f"`{row[0]}`" not in text or row[3] not in text:
            errors.append(f"missing validity row: {row[0]} / {row[3]}")
    if errors:
        for error in errors:
            print("FAIL:", error)
        return 1
    print(f"PASS: {len(CASES)} fixture rows and {len(INVALID_CASES)} validity rows match the generator")
    return 0


def self_test():
    """Apply the mutations the document says each fixture must detect."""
    def catches(case_id, transform):
        case = next(c for c in CASES if c[0] == case_id)
        _, _, prev_state, curr_state, prev_p, curr_p, _, vel, _, _ = compute(case)
        if vel is None:
            return True
        mutated = transform(prev_state, curr_state, prev_p, curr_p)
        return abs(mutated[0] - vel[0]) > tolerance(vel[0]) or \
               abs(mutated[1] - vel[1]) > tolerance(vel[1])

    failures = []

    # M1: real sign inversion, by swapping the previous and current states.
    def swapped(ps, cs, pp, cp):
        v, _, _ = motion(cp, pp, cs, ps)
        return v
    if not catches("CAM-01", swapped):
        failures.append("sign inversion")

    # M2: bottom-left origin in the history calculation.
    def bottom_left(ps, cs, pp, cp):
        v, _, _ = motion(pp, cp, ps, cs, origin="bottom-left")
        return v
    if not catches("CAM-03", bottom_left):
        failures.append("bottom-left origin mutation not detected")

    # M3: jitter included in the history projection.
    def jittered(ps, cs, pp, cp):
        v, _, _ = motion(pp, cp, ps, cs, (0.7, 0.3), (0.1, 0.9))
        return v
    if not catches("CAM-05", jittered):
        failures.append("jitter inclusion not detected")

    # M4: current pose used as the previous pose.
    def current_as_previous(ps, cs, pp, cp):
        v, _, _ = motion(cp, cp, cs, cs)
        return v
    if not catches("SKN-01", current_as_previous):
        failures.append("current-pose-as-previous mutation not detected")

    # M5: simulation-tick history instead of the last rendered frame.
    def tick_history(ps, cs, pp, cp):
        v, _, _ = motion((100, 0, 0), cp, ps, cs)
        return v
    if not catches("SEQ-01", tick_history):
        failures.append("simulation-tick-vs-rendered-history mutation not detected")

    # M6: zero treated as invalid is a contract assertion, not a numeric mutation.
    if not any(c[0] == "VAL-01" and c[2] == "VALID" for c in INVALID_CASES):
        failures.append("zero-valid contract missing")

    if failures:
        for f in failures:
            print("FAIL self-test:", f)
        return 1
    print("PASS: 5 numeric mutations and the zero-valid contract are detected; "
          "M7 (depth-based viewmodel history) and M8 (subview commit) are T043/T046 assertions")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emit", action="store_true", help="emit the Markdown fixture table")
    parser.add_argument("--emit-validity", action="store_true", help="emit the validity table")
    parser.add_argument("--check", type=Path, help="check the document against the generator")
    parser.add_argument("--self-test", action="store_true", help="run the mutation controls")
    args = parser.parse_args()
    if args.emit:
        emit_markdown()
    if args.emit_validity:
        emit_validity()
    if args.check:
        return check_doc(args.check)
    if args.self_test:
        return self_test()
    if not (args.emit or args.emit_validity):
        parser.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
