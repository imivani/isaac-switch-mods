#!/usr/bin/env python3
"""Run bounded portable Flip/Crown/salvage checks without touching the game."""
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    spec = importlib.util.spec_from_file_location("eid_bounded", ROOT / "tools/eid/run_cpp_tests.py")
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    build = ROOT / "eid_special/test-build"
    build.mkdir(parents=True, exist_ok=True)
    sources = ["eid_special/special.hpp", "eid_special/generated.hpp", "eid_special/tests.cpp",
               "eid/context.hpp", "eid/generated/context.hpp", "eid/generated/descriptions.cpp",
               "eid_ui/layout.hpp", "eid_ui/generated/assets.hpp", "native/source/program/eid_special.cpp"]
    report = {"timestamp_utc": datetime.now(timezone.utc).isoformat(), "hardware_test": False,
              "compiler": subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
              "source_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in sources},
              "results": []}
    status = 0
    for mode in ("optimized", "ubsan"):
        executable = build / ("eid-special-" + mode)
        flags = ["-O2"] if mode == "optimized" else ["-O1", "-g", "-fsanitize=undefined", "-fno-omit-frame-pointer"]
        command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", *flags, "-I", "eid",
                   "eid_special/tests.cpp", "eid/generated/descriptions.cpp", "-o", str(executable)]
        result = {"mode": mode, "command": command, "compile": helper.bounded_run(command, ROOT, 120)}
        status = result["compile"]["exit_code"]
        if not status:
            result["run"] = helper.bounded_run([str(executable)], ROOT, 30)
            status = result["run"]["exit_code"]
            print(result["run"]["output"], end="")
        else:
            print(result["compile"]["output"], end="")
        report["results"].append(result)
        if status:
            break
    report["status"] = "PASS" if not status else "FAIL"
    (ROOT / "eid_special/test-results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
