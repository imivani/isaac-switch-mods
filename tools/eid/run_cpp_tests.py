#!/usr/bin/env python3
"""Compile and run the portable EID checks on the host, optionally with sanitizers."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import threading


def bounded_run(command, cwd, timeout):
    """Bound subprocess runtime and diagnostic bytes, even if a sanitizer loops."""
    process = subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    chunks, total, output_limited = [], 0, False

    def drain():
        nonlocal total, output_limited
        while True:
            chunk = process.stdout.read(4096)
            if not chunk:
                return
            previous = total
            total += len(chunk)
            if previous < 8192:
                chunks.append(chunk[:8192 - previous])
            if total > 65536:
                output_limited = True
                process.kill()
                return

    reader = threading.Thread(target=drain, daemon=True)
    reader.start()
    timed_out = False
    try:
        process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        process.kill()
        process.wait()
    reader.join(timeout=2)
    return {
        "exit_code": 124 if timed_out else process.returncode,
        "output": b"".join(chunks).decode("utf-8", errors="replace"),
        "output_bytes": total, "output_limited": output_limited, "timed_out": timed_out,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--ubsan", action="store_true")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[2]
    build = project / "eid" / "test-build"
    build.mkdir(parents=True, exist_ok=True)
    source_names = ["eid/tests/portable_test.cpp", "eid/generated/descriptions.cpp"]
    report = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "compiler": subprocess.check_output([args.compiler, "--version"], text=True).splitlines()[0],
        "source_sha256": {name: hashlib.sha256((project / name).read_bytes()).hexdigest()
                          for name in source_names + ["eid/catalogue.hpp", "eid/markup.hpp", "eid/pickup_policy.hpp"]},
        "results": [],
        "hardware_test": False,
    }
    modes = ["optimized"] + (["undefined-sanitizer"] if args.ubsan else []) + (["address-sanitizer"] if args.sanitize else [])
    status = 0
    for mode in modes:
        output = build / ("eid-portable-test-" + mode + (".exe" if os.name == "nt" else ""))
        flags = ["-O2"] if mode == "optimized" else ["-O1", "-g", "-fsanitize=" + ("undefined" if mode == "undefined-sanitizer" else "address,undefined"), "-fno-omit-frame-pointer"]
        command = [args.compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", *flags,
                   "-I", "eid", *source_names, "-o", str(output)]
        compilation = bounded_run(command, project, 120)
        result = {"mode": mode, "command": command, "compile": compilation}
        if compilation["exit_code"] == 0:
            execution = bounded_run([str(output)], project, 15)
            result["run"] = execution
            status = status or execution["exit_code"]
            print(execution["output"][:2048], end="")
        else:
            status = compilation["exit_code"]
            print(compilation["output"][:2048], file=sys.stderr, end="")
        report["results"].append(result)
        if status:
            break
    report["status"] = "PASS" if status == 0 else "FAIL"
    report_path = project / "eid" / "generated" / ("cpp-asan-results.json" if args.sanitize else "cpp-test-results.json")
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{report['status']}: {report_path}")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
