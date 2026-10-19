#!/usr/bin/env python3
"""Compile the real EID feature adapter against bounded fake game services."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
from run_cpp_tests import bounded_run

ROOT = Path(__file__).resolve().parents[2]


def main():
    build = ROOT / "eid/test-build"
    build.mkdir(exist_ok=True)
    sources = [
        "tools/eid/run_reminder_adapter_tests.py", "tools/eid/run_cpp_tests.py",
        "native/source/program/eid_features.cpp", "native/source/program/eid_features.hpp",
        "native/source/program/feature_api.hpp", "native/source/program/eid_ui.hpp",
        "native/source/program/eid_special.hpp", "native/source/program/native_sprite.hpp",
        "native/source/program/generated/build_profile.hpp", "eid/native_adapter_calls.hpp",
        "eid/reminder_adapter_tests.cpp", "eid/test_support/lib/nx/kernel/svc.h",
        "eid/context.hpp", "eid/generated/context.hpp", "eid/catalogue.hpp",
        "eid/pickup_policy.hpp", "eid/generated/descriptions.cpp",
    ]
    report = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(), "hardware_test": False,
        "scope": "Actual eid_features.cpp; native calls, memory query and final rendering replaced by bounded host fixtures.",
        "compiler": subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
        "source_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in sources},
        "results": [],
    }
    status = 0
    for mode in ("optimized", "ubsan"):
        executable = build / ("eid-reminder-adapter-" + mode)
        flags = ["-O2"] if mode == "optimized" else ["-O1", "-g", "-fsanitize=undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]
        command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", *flags,
                   "-DISAAC_EID_ADAPTER_TEST", "-I", "eid/test_support", "-I", "eid",
                   "native/source/program/eid_features.cpp", "eid/reminder_adapter_tests.cpp",
                   "eid/generated/descriptions.cpp", "-o", str(executable)]
        result = {"mode": mode, "command": command, "compile": bounded_run(command, ROOT, 120)}
        status = result["compile"]["exit_code"]
        if not status:
            result["run"] = bounded_run([str(executable)], ROOT, 40)
            status = result["run"]["exit_code"]
            print(result["run"]["output"], end="")
        else:
            print(result["compile"]["output"], end="")
        report["results"].append(result)
        if status:
            break
    report["status"] = "PASS" if not status else "FAIL"
    (ROOT / "eid/reminder-adapter-test-results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
