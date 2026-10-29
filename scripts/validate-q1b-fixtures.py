#!/usr/bin/env python3
"""Validate the worked Q1b fixture tables against independent literals.

The expected values below are independent of the renderer and of any readback.
Most are typed out; the 8x8 pattern is expanded here from the published literal
coordinate rule. Every Fixture P/C/D table cell in
tests/renderer/q1b-fixtures.md must match. Fixture G and S behaviour is asserted
by their executable tests; this validator checks that their required rows and
controls are present. `--self-test` alters a value in memory and requires
validation to fail.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

DOC = pathlib.Path(__file__).resolve().parent.parent / "tests" / "renderer" / "q1b-fixtures.md"

PATTERN = [
    [f"{x * 32},{y * 32},128,255" for x in range(8)] for y in range(8)
]

TINTS = {
    "1.0": [("0,0,128,255"), ("96,160,128,255"), ("224,224,128,255")],
    "0.5": [("0,0,64,255"), ("48,80,64,255"), ("112,112,64,255")],
    "0.25": [("0,0,32,255"), ("24,40,32,255"), ("56,56,32,255")],
    "0.75": [("0,0,96,255"), ("72,120,96,255"), ("168,168,96,255")],
}

COMPUTE = [
    ["162", "178", "130", "146"],
    ["175", "191", "143", "159"],
    ["168", "184", "136", "152"],
    ["181", "133", "149", "229"],
]

MRT = [
    ("0, x > 0", "10,20,30,255"),
    ("1, x > 0", "40,50,60,255"),
    ("2, x > 0", "70,80,90,255"),
    ("0\u20132, x = 0", "0,0,0,255"),
    ("3, x = 0", "255,0,0,255"),
    ("3, 1 \u2264 x < 4", "64,0,0,255"),
    ("3, x \u2265 4", "191,0,0,255"),
]

DEPTH = [("1.0 (discarded by `LESS`)", "255"), ("0.25", "64"), ("0.75", "191")]

REQUIRED = [
    "Wrong pipeline kind",
    "in-flight readback",
    "Depth 1.0",
    "Adjacent pixel",
    "Wrong tint",
    "Transposed expectation",
    "Cross-attachment",
    "In-flight reuse",
    "Read a transient before any write",
    "Minimize and submit",
    "valid = true (distinct from invalid)",
]


def table_after(lines: list[str], heading: str, errors: list[str]) -> list[list[str]]:
    start = None
    for index, line in enumerate(lines):
        if heading in line:
            start = index
            break
    if start is None:
        errors.append(f"missing heading: {heading}")
        return []
    rows: list[list[str]] = []
    seen = False
    for line in lines[start + 1 :]:
        stripped = line.strip()
        if not stripped:
            if seen:
                break
            continue
        if not stripped.startswith("|"):
            if seen:
                break
            continue
        seen = True
        cells = [cell.strip() for cell in stripped.strip("|").split("|")]
        if all(re.fullmatch(r":?-{3,}:?", cell) for cell in cells if cell):
            continue
        rows.append(cells)
    if not rows:
        errors.append(f"no table after heading: {heading}")
    return rows[0], rows[1:] if rows else ([], [])


def check_table(name, actual_header, actual_rows, expected, header_ok, errors):
    if not header_ok:
        errors.append(f"{name}: unexpected header {actual_header}")
    if len(actual_rows) != len(expected):
        errors.append(f"{name}: expected {len(expected)} rows, found {len(actual_rows)}")
        return
    for index, (row, wanted) in enumerate(zip(actual_rows, expected)):
        if row != wanted:
            errors.append(f"{name}: row {index} is {row}, expected {wanted}")


def validate_text(text: str) -> list[str]:
    errors: list[str] = []
    lines = text.splitlines()

    header, rows = table_after(lines, "Expected bytes for tint 1.0:", errors)
    if rows:
        wanted_rows = [[str(y)] + PATTERN[y] for y in range(8)]
        check_table(
            "pattern",
            header,
            rows,
            wanted_rows,
            header[:1] == ["y \\ x"] and header[1:] == [str(i) for i in range(8)],
            errors,
        )

    header, rows = table_after(lines, "Per-pass uniform variants", errors)
    if rows:
        wanted_rows = [[tint] + values for tint, values in TINTS.items()]
        check_table(
            "tints",
            header,
            rows,
            wanted_rows,
            header == ["tint", "(0,0)", "(3,5)", "(7,7)"],
            errors,
        )

    header, rows = table_after(lines, "Expected 32-bit values", errors)
    if rows:
        wanted_rows = [[str(y)] + COMPUTE[y] for y in range(4)]
        check_table(
            "compute",
            header,
            rows,
            wanted_rows,
            header == ["y \\ x", "0", "1", "2", "3"],
            errors,
        )

    header, rows = table_after(lines, "Expected colours:", errors)
    if rows:
        if header != ["Attachment", "Expected RGBA", "Note"]:
            errors.append(f"mrt: unexpected header {header}")
        if len(rows) != len(MRT):
            errors.append(f"mrt: expected {len(MRT)} rows, found {len(rows)}")
        else:
            for index, (row, (label, value)) in enumerate(zip(rows, MRT)):
                if len(row) != 3 or row[0] != label or row[1] != value:
                    errors.append(
                        f"mrt: row {index} is {row}, expected [{label}, {value}, note]"
                    )

    header, rows = table_after(lines, "Depth conversion:", errors)
    if rows:
        wanted_rows = [[depth, value] for depth, value in DEPTH]
        check_table(
            "depth",
            header,
            rows,
            wanted_rows,
            header == ["Written depth", "Expected byte"],
            errors,
        )

    for required in REQUIRED:
        if required not in text:
            errors.append(f"missing required control or value: {required}")

    if "0x887A002D" not in text:
        errors.append("debug-layer unavailability is not recorded as 0x887A002D")

    return errors


def self_test(path: pathlib.Path) -> int:
    text = path.read_text(encoding="utf-8")
    if validate_text(text):
        print("SELF-TEST precondition failed: the unchanged document is not valid")
        return 1
    altered = text.replace("0,0,128,255", "0,0,127,255", 1)
    if altered == text:
        print("SELF-TEST could not alter a pattern cell")
        return 1
    if not validate_text(altered):
        print("SELF-TEST altered pattern value was accepted")
        return 1
    altered = text.replace("162", "163", 1)
    if altered == text:
        print("SELF-TEST could not alter a compute value")
        return 1
    if not validate_text(altered):
        print("SELF-TEST altered compute value was accepted")
        return 1
    print("PASS self-test: altered pattern and compute values were rejected")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--doc", type=pathlib.Path, default=DOC)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if not args.doc.is_file():
        print(f"missing document: {args.doc}")
        return 1
    if args.self_test:
        return self_test(args.doc)
    errors = validate_text(args.doc.read_text(encoding="utf-8"))
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1
    print(f"PASS {args.doc.name}: pattern, tints, compute, MRT, depth and controls")
    return 0


if __name__ == "__main__":
    sys.exit(main())
