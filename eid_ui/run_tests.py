#!/usr/bin/env python3
"""Check every imported description with the renderer's real font metrics."""
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    spec = importlib.util.spec_from_file_location("bounded", ROOT / "tools/eid/run_cpp_tests.py")
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    build = ROOT / "eid_ui/test-build"
    build.mkdir(exist_ok=True)
    report = {"timestamp_utc": datetime.now(timezone.utc).isoformat(), "hardware_test": False,
              "compiler": subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
              "source_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in
                                ("eid_ui/layout.hpp", "eid_ui/tests.cpp", "eid_ui/generated/assets.hpp")}, "results": []}
    status = 0
    for mode in ("optimized", "ubsan"):
        executable = build / ("eid-ui-" + mode)
        flags = ["-O2"] if mode == "optimized" else ["-O1", "-g", "-fsanitize=undefined", "-fno-omit-frame-pointer"]
        command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", *flags, "-I", "eid",
                   "eid_ui/tests.cpp", "eid/generated/descriptions.cpp", "-o", str(executable)]
        compilation = helper.bounded_run(command, ROOT, 120)
        result = {"mode": mode, "compile": compilation, "command": command}
        status = compilation["exit_code"]
        if status == 0:
            result["run"] = helper.bounded_run([str(executable)], ROOT, 30)
            status = result["run"]["exit_code"]
            print(result["run"]["output"], end="")
        else:
            print(compilation["output"], end="")
        report["results"].append(result)
        if status:
            break
    report["status"] = "PASS" if not status else "FAIL"
    (ROOT / "eid_ui/test-results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
