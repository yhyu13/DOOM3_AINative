#!/usr/bin/env python3
"""Validate the Q7a field-to-WebGPU mapping against the portable RHI header.

The mapping document must contain exactly one row per public declaration of
`src/renderer/rhi/rhi.h` (enums, enumerators, structs, struct fields, class
methods and free functions), each naming a WebGPU-shaped operation or an
explicit gap, each with an allowed status and a conformance-check reference.
A non-`mapped` row must carry a justification.

This is a structural and completeness gate. It cannot judge whether a mapping
is technically correct, and it is not evidence that a WebGPU backend exists.

Usage:
    py -3 -I scripts/validate-webgpu-mapping.py
    py -3 -I scripts/validate-webgpu-mapping.py --self-test
    py -3 -I scripts/validate-webgpu-mapping.py --header <rhi.h> --doc <mapping.md> --dump

Exit codes: 0 valid, 1 invalid, 2 usage or unreadable input.
"""

import argparse
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_HEADER = HERE.parent / "src" / "renderer" / "rhi" / "rhi.h"
DEFAULT_DOC = HERE.parent / "specs" / "004-quality-renderer" / "webgpu-mapping.md"

ALLOWED_STATUS = {"mapped", "mapped-with-gap", "unsupported"}
CHECK_IDS = {"C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "N1", "N2", "N3", "E"}
REQUIRED_CHECK_IDS = {"C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "N1", "N2", "N3", "E"}
NOT_APPLICABLE = {"—", "-", "none"}
PLACEHOLDER_NOTES = {"todo", "tbd", "fixme", "n/a", "na", "none", "-", "—", "?"}


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def _blocks(text):
    """Return [(kind, name, body, start, end)] for enum class / struct / class."""
    pattern = re.compile(r"\b(enum\s+class|struct|class)\s+(\w+)\s*(?::\s*[\w:]+)?\s*\{")
    found = []
    for match in pattern.finditer(text):
        kind = {"enum class": "enum", "struct": "struct", "class": "class"}[match.group(1)]
        depth = 0
        index = match.end() - 1
        start = index + 1
        while index < len(text):
            char = text[index]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    found.append((kind, match.group(2), text[start:index], match.start(), index + 1))
                    break
            index += 1
    return found


def _take_statements(body):
    """Yield (access, statement) for a struct/class body, honouring access labels."""
    access = None
    for statement in body.split(";"):
        statement = statement.strip()
        while True:
            match = re.match(r"^(public|private|protected)\s*:\s*", statement)
            if not match:
                break
            access = match.group(1)
            statement = statement[match.end():].strip()
        if statement:
            yield access, statement


def parse_header(text):
    """Return [(kind, qualifier, name)] for the public surface of the header."""
    text = strip_comments(text)
    decls = []
    masked = list(text)
    for kind, name, body, start, end in _blocks(text):
        for index in range(start, end):
            if masked[index] not in "\r\n":
                masked[index] = " "
        decls.append((kind, "", name))
        if kind == "enum":
            for part in body.split(","):
                token = part.split("=")[0].strip()
                if re.fullmatch(r"[A-Za-z_]\w*", token):
                    decls.append(("enumerator", name, token))
            continue
        for access, statement in _take_statements(body):
            # A struct is public by default; a class is private until a label.
            effective = access or ("public" if kind == "struct" else "private")
            if effective != "public":
                continue
            statement = re.sub(r"\b(public|private|protected)\s*:", " ", statement).strip()
            if re.match(r"^(struct|class|enum|friend|using|typedef|template)\b", statement):
                continue
            if "delete" in statement or "default" in statement:
                continue
            if "(" in statement:
                match = re.search(r"(~?\w+)\s*\(", statement)
                if not match:
                    continue
                method = match.group(1)
                if method == name or method.startswith("~"):
                    continue
                decls.append(("method" if kind == "class" else "field", name, method))
                continue
            declaration = statement.split("=")[0].strip()
            match = re.search(r"(\w+)\s*$", declaration)
            if match:
                decls.append(("field", name, match.group(1)))
    outside = "".join(masked)
    for match in re.finditer(r"^[ \t]*[A-Za-z_][\w:<>,\*& \t]*?\b(\w+)\s*\([^;{}]*\)\s*;\s*$", outside, re.M):
        if match.group(1) not in ("if", "for", "while", "switch"):
            decls.append(("function", "", match.group(1)))
    return decls

def normalized_declarations(text):
    out = set()
    for kind, qualifier, name in parse_header(text):
        if kind == "enumerator":
            out.add(f"{qualifier}.{name}")
        elif kind in ("field", "method"):
            out.add(f"{qualifier}.{name}")
        elif kind == "function":
            out.add(name)
        else:
            out.add(name)
    return out


def split_row(line):
    cells = []
    current = []
    index = 0
    while index < len(line):
        char = line[index]
        if char == "\\" and index + 1 < len(line) and line[index + 1] == "|":
            current.append("|")
            index += 2
            continue
        if char == "|":
            cells.append("".join(current))
            current = []
        else:
            current.append(char)
        index += 1
    cells.append("".join(current))
    if cells and not cells[0].strip():
        cells = cells[1:]
    if cells and not cells[-1].strip():
        cells = cells[:-1]
    return [cell.strip() for cell in cells]


def find_mapping_table(lines, errors):
    for index, line in enumerate(lines):
        if not line.strip().startswith("|"):
            continue
        cells = split_row(line)
        if len(cells) >= 4 and cells[0].lower() == "declaration" and "webgpu" in cells[1].lower():
            header = cells
            if index + 1 >= len(lines) or not re.fullmatch(r"\s*\|?[\s:\-|]+\|?\s*", lines[index + 1]):
                errors.append("mapping table has no separator row")
                return header, []
            rows = []
            cursor = index + 2
            while cursor < len(lines) and lines[cursor].strip().startswith("|"):
                rows.append(split_row(lines[cursor]))
                cursor += 1
            return header, rows
    errors.append("no mapping table with columns 'Declaration' and 'WebGPU ...'")
    return None, []


def find_checks_table(lines, errors):
    for index, line in enumerate(lines):
        if not line.strip().startswith("|"):
            continue
        cells = split_row(line)
        if cells and cells[0].strip().strip("`").lower() == "check":
            ids = []
            cursor = index + 2
            while cursor < len(lines) and lines[cursor].strip().startswith("|"):
                row = split_row(lines[cursor])
                if row:
                    ids.append(row[0].strip().strip("`").split()[0])
                cursor += 1
            return set(ids)
    errors.append("no conformance-check table with a 'Check' first column")
    return set()


def validate(header_text, doc_text):
    errors = []
    declarations = normalized_declarations(header_text)
    if not declarations:
        errors.append("header declares no public surface, so a mapping cannot be checked")
        return errors, 0, 0

    lines = doc_text.splitlines()
    header, rows = find_mapping_table(lines, errors)
    if header is None:
        return errors, 0, 0
    if len(header) < 5:
        errors.append("mapping table must have five columns: Declaration, WebGPU operation, Status, Conformance check, Justification")
        return errors, 0, 0

    seen = {}
    for number, cells in enumerate(rows, start=2):
        if len(cells) != len(header):
            errors.append(f"row {number} has {len(cells)} cells, expected {len(header)}")
            continue
        name = cells[0].strip().strip("`").strip()
        operation = cells[1].strip()
        status = cells[2].strip().strip("`").strip().lower()
        check = cells[3].strip().strip("`").strip()
        justification = cells[4].strip()
        if not name:
            errors.append(f"row {number} has an empty declaration")
            continue
        if name in seen:
            errors.append(f"duplicate mapping row for {name}")
        seen[name] = number
        if not operation:
            errors.append(f"{name}: empty WebGPU operation")
        if status not in ALLOWED_STATUS:
            errors.append(f"{name}: status '{status}' is not one of {sorted(ALLOWED_STATUS)}")
        if check not in CHECK_IDS and check not in NOT_APPLICABLE:
            errors.append(f"{name}: conformance reference '{check}' is not a known check id")
        if status != "mapped" and not justification:
            errors.append(f"{name}: status '{status}' needs a justification in the last column")
        elif status != "mapped" and justification.strip().strip("`").lower() in PLACEHOLDER_NOTES:
            errors.append(f"{name}: justification '{justification.strip()}' is a placeholder, not a reason")

    missing = sorted(declarations - set(seen))
    stale = sorted(set(seen) - declarations)
    for name in missing:
        errors.append(f"declaration not mapped: {name}")
    for name in stale:
        errors.append(f"mapping row does not correspond to a header declaration: {name}")

    check_ids = find_checks_table(lines, errors)
    for check in sorted(REQUIRED_CHECK_IDS):
        if check not in check_ids:
            errors.append(f"the conformance-check table has no row for required check {check}")
    joined = "\n".join(lines)
    if "## Gaps" not in joined:
        errors.append("document has no '## Gaps' section")
    return errors, len(declarations), len(seen)


FIXTURE_HEADER = """
namespace demo {
enum class Colour { Red, Green };
enum class Mode : uint32_t { A, B };
struct Thing {
    uint32_t width = 0;
    std::vector<uint8_t> pixels;
};
class Device {
public:
    Status Initialize(const Options& options, std::string* error);
    bool IsComplete(const Submission& submission) const;
private:
    struct Impl;
};
const char* ToString(Colour value);
}
"""

FIXTURE_DOC = """
# Fixture

## Mapping table

| Declaration | WebGPU operation | Status | Conformance check | Justification |
|---|---|---|---|---|
| `Colour` | local enum | mapped | C1 | |
| `Colour.Red` | clearValue r | mapped | C1 | |
| `Colour.Green` | clearValue g | mapped | C1 | |
| `Mode` | local scoped enum | mapped | C1 | |
| `Mode.A` | local value | mapped | C1 | |
| `Mode.B` | local value | mapped | C1 | |
| `Thing` | local struct | mapped | C1 | |
| `Thing.width` | GPUExtent3D.width | mapped | C1 | |
| `Thing.pixels` | mapped range copy | mapped | C2 | |
| `Device` | GPUDevice | mapped | C1 | |
| `Device.Initialize` | requestAdapter/requestDevice | mapped | C1 | |
| `Device.IsComplete` | onSubmittedWorkDone | mapped-with-gap | C8 | no polling in WebGPU |
| `ToString` | local | mapped | — | |

## Conformance checks

| Check | Proves |
|---|---|
| C1 | device |
| C2 | buffer |
| C3 | bind group |
| C4 | mrt |
| C5 | compute |
| C6 | pitch |
| C7 | surface |
| C8 | device loss |
| N1 | comparator |
| N2 | packed offset |
| N3 | liveness |
| E | errors |

## Gaps and deltas

Fixture gap text.
"""


def self_test():
    failures = []
    baseline, declared, rows = validate(FIXTURE_HEADER, FIXTURE_DOC)
    if baseline:
        failures.append(f"fixture baseline should be valid: {baseline}")
    if declared != rows:
        failures.append(f"fixture baseline should map every declaration: {declared} vs {rows}")

    def expect(name, header, doc, needle):
        problems, _, _ = validate(header, doc)
        if not problems:
            failures.append(f"{name}: mutation was not detected")
        elif not any(needle in problem for problem in problems):
            failures.append(f"{name}: expected a problem containing '{needle}', got {problems}")

    expect("missing-row", FIXTURE_HEADER, FIXTURE_DOC.replace("| `Thing.width` | GPUExtent3D.width | mapped | C1 | |\n", ""), "declaration not mapped: Thing.width")
    expect("stale-row", FIXTURE_HEADER, FIXTURE_DOC.replace("| `ToString` | local | mapped | — | |",
                                                             "| `ToString` | local | mapped | — | |\n| `Ghost.field` | none | mapped | C1 | |"),
           "does not correspond to a header declaration")
    expect("empty-operation", FIXTURE_HEADER, FIXTURE_DOC.replace("GPUExtent3D.width", ""), "empty WebGPU operation")
    expect("bad-status", FIXTURE_HEADER, FIXTURE_DOC.replace("| mapped-with-gap |", "| todo |"), "is not one of")
    expect("gap-without-justification", FIXTURE_HEADER,
           FIXTURE_DOC.replace("| mapped-with-gap | C8 | no polling in WebGPU |", "| mapped-with-gap | C8 | |"),
           "needs a justification")
    expect("unknown-check", FIXTURE_HEADER, FIXTURE_DOC.replace("| C8 | no polling", "| X9 | no polling"), "is not a known check id")
    expect("missing-check-reference", FIXTURE_HEADER, FIXTURE_DOC.replace("| C8 | device loss |\n", ""), "no row for required check C8")
    expect("missing-negative-check-row", FIXTURE_HEADER, FIXTURE_DOC.replace("| N2 | packed offset |\n", ""), "no row for required check N2")
    expect("placeholder-justification", FIXTURE_HEADER,
           FIXTURE_DOC.replace("| mapped-with-gap | C8 | no polling in WebGPU |", "| mapped-with-gap | C8 | TODO |"),
           "is a placeholder")
    expect("broken-separator", FIXTURE_HEADER, FIXTURE_DOC.replace("|---|---|---|---|---|\n", ""), "no separator row")
    expect("short-row", FIXTURE_HEADER, FIXTURE_DOC.replace("| `ToString` | local | mapped | — | |", "| `ToString` | local |"), "cells, expected")
    expect("no-gaps-section", FIXTURE_HEADER, FIXTURE_DOC.replace("## Gaps and deltas", "## Something"), "no '## Gaps' section")
    expect("duplicate-row", FIXTURE_HEADER,
           FIXTURE_DOC.replace("| `Thing.width` | GPUExtent3D.width | mapped | C1 | |",
                               "| `Thing.width` | GPUExtent3D.width | mapped | C1 | |\n| `Thing.width` | GPUExtent3D.width | mapped | C1 | |"),
           "duplicate mapping row")
    expect("no-table", FIXTURE_HEADER,
           FIXTURE_DOC.replace("| Declaration | WebGPU operation | Status | Conformance check | Justification |", "| Name | Operation | State | Check | Notes |"),
           "no mapping table")

    if failures:
        print("SELF-TEST FAILED")
        for failure in failures:
            print(f"  - {failure}")
        return 1
    print(f"self-test: baseline valid, {declared} declarations, 14 negative controls detected")
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--header", type=Path, default=DEFAULT_HEADER)
    parser.add_argument("--doc", type=Path, default=DEFAULT_DOC)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--dump", action="store_true", help="print every parsed declaration")
    args = parser.parse_args(argv)

    if args.self_test:
        return self_test()

    if not args.header.is_file():
        print(f"ERROR: missing input {args.header}", file=sys.stderr)
        return 2
    header_text = args.header.read_text(encoding="utf-8")
    if args.dump:
        for name in sorted(normalized_declarations(header_text)):
            print(name)
        return 0
    if not args.doc.is_file():
        print(f"ERROR: missing input {args.doc}", file=sys.stderr)
        return 2
    doc_text = args.doc.read_text(encoding="utf-8")

    errors, declared, rows = validate(header_text, doc_text)
    if errors:
        print(f"INVALID: {len(errors)} problem(s); {rows}/{declared} declarations mapped")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(f"OK: {rows}/{declared} public declarations classified; statuses, check rows and justifications valid")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
