#!/usr/bin/env python3
"""Measure the Q4 harness's process VRAM peak with DXGI QueryVideoMemoryInfo.

The harness samples `IDXGIAdapter3::QueryVideoMemoryInfo` for the calling process
after initialization, after the material upload, at scene start and after every
rendered frame, then prints one `VRAM_JSON` line. This tool runs each Q4 scene a
few times, keeps the peak `CurrentUsage` of the LOCAL segment, and records it in
`evidence/t056/vram.json` and `measurements.json`.

The adapter's reported dedicated memory is recorded as identity only. It is a
capacity figure, never a use figure, and is never substituted for the peak.

Usage:
    py -3 specs/004-quality-renderer/evidence/t056/tools/vram_measure.py \
        --exe build-q4-harness/bin/Release/q4_harness.exe --runs 3 --frames 30
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import subprocess
import sys
from pathlib import Path

SCENES = [
    "static_opaque",
    "alpha_emissive",
    "moving",
    "transmissive_environment",
]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[5]


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def detect_driver(adapter_name: str) -> str:
    """Return the OS driver version for `adapter_name`, or 'unreported'."""
    try:
        completed = subprocess.run(
            [
                "powershell",
                "-NoProfile",
                "-Command",
                "Get-CimInstance Win32_VideoController | "
                "Select-Object Name,DriverVersion | ConvertTo-Json -Compress",
            ],
            capture_output=True,
            text=True,
            timeout=60,
        )
    except (OSError, subprocess.SubprocessError):
        return "unreported"
    if completed.returncode != 0 or not completed.stdout.strip():
        return "unreported"
    try:
        parsed = json.loads(completed.stdout)
    except json.JSONDecodeError:
        return "unreported"
    rows = parsed if isinstance(parsed, list) else [parsed]
    for row in rows:
        if not isinstance(row, dict):
            continue
        name = str(row.get("Name") or "")
        if adapter_name and adapter_name in name:
            return str(row.get("DriverVersion") or "unreported")
    return "unreported"


def run_once(exe: Path, repo: Path, scene: str, frames: int, run_index: int) -> dict:
    capture = repo / "out" / "vram" / f"{scene}-run{run_index}.png"
    capture.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(exe),
        "-Scene",
        scene,
        "-Frames",
        str(frames),
        "-AdapterIndex",
        "0",
        "-VramProbe",
        "-CaptureExact",
        "-Capture",
        str(capture),
        "-ContentDir",
        str(repo),
    ]
    recorded_exe = (
        str(exe.relative_to(repo)).replace("\\", "/")
        if exe.is_relative_to(repo)
        else exe.name
    )
    recorded_command = [
        recorded_exe,
        "-Scene",
        scene,
        "-Frames",
        str(frames),
        "-AdapterIndex",
        "0",
        "-VramProbe",
        "-CaptureExact",
        "-Capture",
        f"out/vram/{scene}-run{run_index}.png",
        "-ContentDir",
        ".",
    ]
    completed = subprocess.run(
        command, cwd=str(repo), capture_output=True, text=True, timeout=1800
    )
    vram_lines = [
        line[len("VRAM_JSON ") :]
        for line in completed.stdout.splitlines()
        if line.startswith("VRAM_JSON ")
    ]
    identity_lines = [
        line for line in completed.stdout.splitlines() if line.startswith("RHI backend:")
    ]
    result = {
        "scene": scene,
        "run": run_index,
        "command": " ".join(recorded_command),
        "exit": completed.returncode,
        "identity": identity_lines[-1] if identity_lines else "",
        "frames": frames,
    }
    if completed.returncode != 0:
        result["error"] = "harness exited non-zero"
        result["stderr_tail"] = completed.stderr.splitlines()[-10:]
        return result
    if len(vram_lines) != 1:
        result["error"] = f"expected one VRAM_JSON line, got {len(vram_lines)}"
        result["stdout_tail"] = completed.stdout.splitlines()[-10:]
        return result
    try:
        sample = json.loads(vram_lines[0])
    except json.JSONDecodeError as exc:
        result["error"] = f"VRAM_JSON is not valid JSON: {exc}"
        return result
    if not sample.get("available"):
        result["error"] = "VRAM probe reported unavailable"
        result["probe"] = sample
        return result
    samples = sample.get("samples") or []
    peak = max(samples, key=lambda item: item["localUsage"]) if samples else None
    if peak is None:
        result["error"] = "VRAM probe produced no samples"
        return result
    result.update(
        {
            "adapter": sample.get("adapter", ""),
            "luid": sample.get("luid", ""),
            "adapterDedicatedBytes": sample.get("dedicatedVideoMemory", 0),
            "adapterSharedSystemBytes": sample.get("sharedSystemMemory", 0),
            "sampleCount": len(samples),
            "peakLocalUsageBytes": peak["localUsage"],
            "peakLocalBudgetBytes": peak["localBudget"],
            "peakLocalAvailableForReservationBytes": peak[
                "localAvailableForReservation"
            ],
            "peakLocalReservationBytes": peak["localReservation"],
            "peakNonLocalUsageBytes": max(
                (item.get("nonLocalUsage", 0) for item in samples), default=0
            ),
            "peakPhase": peak["phase"],
            "peakFrame": peak["frame"],
            "samples": samples,
        }
    )
    return result


def summarise(scene: str, runs: list[dict]) -> dict:
    successful = [run for run in runs if "error" not in run]
    if len(successful) != len(runs):
        return {
            "scene": scene,
            "complete": False,
            "runs": runs,
            "error": "one or more runs failed",
        }
    peaks = [run["peakLocalUsageBytes"] for run in successful]
    budgets = [run["peakLocalBudgetBytes"] for run in successful]
    peak_run = max(successful, key=lambda run: run["peakLocalUsageBytes"])
    return {
        "scene": scene,
        "complete": True,
        "runs": len(successful),
        "frames_per_run": successful[0]["frames"],
        "runPeaksBytes": peaks,
        "minPeakBytes": min(peaks),
        "maxPeakBytes": max(peaks),
        "medianPeakBytes": sorted(peaks)[len(peaks) // 2],
        "peakLocalUsageBytes": max(peaks),
        "peakLocalBudgetBytes": max(budgets),
        "peakPhase": peak_run["peakPhase"],
        "peakFrame": peak_run["peakFrame"],
        "peakNonLocalUsageBytes": max(
            run["peakNonLocalUsageBytes"] for run in successful
        ),
        "adapter": successful[0]["adapter"],
        "identity": successful[0]["identity"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", default="build-q4-harness/bin/Release/q4_harness.exe")
    parser.add_argument("--repo", default="")
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--frames", type=int, default=30)
    parser.add_argument("--scene", action="append", default=[])
    parser.add_argument(
        "--out",
        default="specs/004-quality-renderer/evidence/t056/vram.json",
    )
    parser.add_argument(
        "--measurements",
        default="specs/004-quality-renderer/evidence/t056/measurements.json",
    )
    parser.add_argument("--no-merge", action="store_true")
    args = parser.parse_args()

    repo = Path(args.repo).resolve() if args.repo else repo_root()
    exe = (repo / args.exe).resolve() if not Path(args.exe).is_absolute() else Path(args.exe)
    if not exe.is_file():
        print(f"ERROR: harness not found: {exe}", file=sys.stderr)
        return 2
    scenes = args.scene or SCENES

    all_runs: dict[str, list[dict]] = {}
    for scene in scenes:
        runs = []
        for run_index in range(args.runs):
            print(f"RUN {scene} {run_index + 1}/{args.runs}", flush=True)
            result = run_once(exe, repo, scene, args.frames, run_index)
            runs.append(result)
            if "error" in result:
                print(f"  FAILED: {result['error']}", file=sys.stderr)
            else:
                print(
                    f"  peak localUsage={result['peakLocalUsageBytes']} "
                    f"budget={result['peakLocalBudgetBytes']} "
                    f"phase={result['peakPhase']} frame={result['peakFrame']}",
                    flush=True,
                )
        all_runs[scene] = runs

    summaries = {scene: summarise(scene, runs) for scene, runs in all_runs.items()}
    failures = [
        scene for scene, summary in summaries.items() if not summary.get("complete")
    ]
    if failures:
        print(f"ERROR: measurement incomplete for {failures}", file=sys.stderr)

    first_success = next(
        (
            run
            for runs in all_runs.values()
            for run in runs
            if "error" not in run
        ),
        None,
    )
    adapter = first_success["adapter"] if first_success else ""
    exe_display = (
        str(exe.relative_to(repo)).replace("\\", "/")
        if exe.is_relative_to(repo)
        else str(exe).replace("\\", "/")
    )
    artifact = {
        "schema": "doom3-q4-vram-measurement/1",
        "task": "T056",
        "measuredAt": _dt.datetime.now()
        .astimezone()
        .replace(microsecond=0)
        .isoformat(),
        "method": (
            "The harness calls IDXGIAdapter3::QueryVideoMemoryInfo(node 0, "
            "DXGI_MEMORY_SEGMENT_GROUP_LOCAL) for the calling process after "
            "harness initialization, after the material upload, at scene start "
            "and after every rendered frame. The recorded peak is the maximum "
            "CurrentUsage over those samples; it is process usage, not adapter "
            "capacity and not ResidencyPolicy bookkeeping."
        ),
        "counter": "IDXGIAdapter3::QueryVideoMemoryInfo",
        "segmentGroup": "LOCAL",
        "exe": exe_display,
        "exe_sha256": file_sha256(exe),
        "adapter": adapter,
        "driver": detect_driver(adapter),
        "adapterDedicatedBytes": (
            first_success["adapterDedicatedBytes"] if first_success else 0
        ),
        "adapterDedicatedMeaning": (
            "adapter capacity reported by DXGI; not a use figure and not "
            "substituted for the measured peak"
        ),
        "runsPerScene": args.runs,
        "framesPerRun": args.frames,
        "scenes": summaries,
        "complete": not failures,
        "runs": all_runs,
    }
    if first_success:
        complete = [
            summary for summary in summaries.values() if summary.get("complete")
        ]
        top = max(complete, key=lambda summary: summary["peakLocalUsageBytes"])
        tied = all(
            summary["peakLocalUsageBytes"] == top["peakLocalUsageBytes"]
            for summary in complete
        )
        artifact["peak"] = {
            "scene": "all (tied)" if tied else top["scene"],
            "localUsageBytes": top["peakLocalUsageBytes"],
            "localBudgetBytes": top["peakLocalBudgetBytes"],
            "phase": top["peakPhase"],
            "frame": top["peakFrame"],
            "nonLocalUsageBytes": top["peakNonLocalUsageBytes"],
        }

    out_path = repo / args.out
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(artifact, indent=2) + "\n", encoding="utf-8")
    print(f"WROTE {out_path}")

    if not args.no_merge and not failures:
        measurements_path = repo / args.measurements
        measurements = json.loads(measurements_path.read_text(encoding="utf-8"))
        measurements["vram"] = {
            "measuredAt": artifact["measuredAt"],
            "method": artifact["method"],
            "counter": artifact["counter"],
            "segmentGroup": artifact["segmentGroup"],
            "adapter": artifact["adapter"],
            "driver": artifact["driver"],
            "exe": artifact["exe"],
            "exe_sha256": artifact["exe_sha256"],
            "adapterDedicatedBytes": artifact["adapterDedicatedBytes"],
            "adapterDedicatedMeaning": artifact["adapterDedicatedMeaning"],
            "runsPerScene": artifact["runsPerScene"],
            "framesPerRun": artifact["framesPerRun"],
            "scenes": {
                scene: {
                    "peakLocalUsageBytes": summary["peakLocalUsageBytes"],
                    "peakLocalBudgetBytes": summary["peakLocalBudgetBytes"],
                    "minPeakBytes": summary["minPeakBytes"],
                    "maxPeakBytes": summary["maxPeakBytes"],
                    "medianPeakBytes": summary["medianPeakBytes"],
                    "runPeaksBytes": summary["runPeaksBytes"],
                    "peakPhase": summary["peakPhase"],
                    "peakFrame": summary["peakFrame"],
                    "peakNonLocalUsageBytes": summary["peakNonLocalUsageBytes"],
                }
                for scene, summary in summaries.items()
            },
            "peak": artifact["peak"],
            "artifact": str(out_path.relative_to(repo)).replace("\\", "/"),
        }
        measurements_path.write_text(
            json.dumps(measurements, indent=2) + "\n", encoding="utf-8"
        )
        print(f"MERGED vram into {measurements_path}")

    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
