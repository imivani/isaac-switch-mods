#!/usr/bin/env python3
"""Run the actual Specialist native adapter with bounded mock game services."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/eid"))
from run_cpp_tests import bounded_run


def main():
    build = ROOT / "features/specialist/test-build"
    build.mkdir(exist_ok=True)
    sources = ["tools/specialist/run_tests.py", "tools/eid/run_cpp_tests.py", "native/source/program/specialist.cpp", "native/source/program/specialist.hpp",
               "native/source/program/feature_api.hpp", "native/source/program/native_sprite.hpp", "native/source/program/generated/build_profile.hpp",
               "features/specialist/policy.hpp", "features/specialist/native_calls.hpp", "features/specialist/generated/assets.hpp",
               "features/specialist/generated/native.hpp", "features/specialist/adapter_tests.cpp", "eid/test_support/lib/nx/kernel/svc.h"]
    report = {"timestamp_utc": datetime.now(timezone.utc).isoformat(), "hardware_test": False,
              "scope": "Actual specialist.cpp registration, selection, costumes, music ownership and lifecycle; native functions and memory are mocked.",
              "compiler": subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
              "source_sha256": {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in sources}, "results": []}
    status = 0
    for mode in ("optimized", "ubsan"):
        executable = build / ("specialist-adapter-" + mode)
        flags = ["-O2"] if mode == "optimized" else ["-O1", "-g", "-fsanitize=undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]
        command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", *flags, "-DISAAC_SPECIALIST_ADAPTER_TEST", "-I", "eid/test_support",
                   "native/source/program/specialist.cpp", "features/specialist/adapter_tests.cpp", "-o", str(executable)]
        result = {"mode": mode, "command": command, "compile": bounded_run(command, ROOT, 120)}
        status = result["compile"]["exit_code"]
        if not status:
            result["run"] = bounded_run([str(executable)], ROOT, 40); status = result["run"]["exit_code"]
            print(result["run"]["output"], end="")
        else: print(result["compile"]["output"], end="")
        report["results"].append(result)
        if status: break
    report["status"] = "PASS" if not status else "FAIL"
    (ROOT/"features/specialist/test-results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return status


if __name__ == "__main__": raise SystemExit(main())
