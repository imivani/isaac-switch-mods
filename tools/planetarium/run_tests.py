#!/usr/bin/env python3
"""Build and run portable Planetarium lifecycle checks with bounded diagnostics."""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[2]
    helper_path = project / "tools" / "eid" / "run_cpp_tests.py"
    spec = importlib.util.spec_from_file_location("eid_bounded_test_runner", helper_path)
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    build = project / "features" / "planetarium" / "test-build"
    build.mkdir(parents=True, exist_ok=True)
    report = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "upstream_commit": "b724ac3641e89e0aaa8b43a301ccb168a07d1d6a",
        "compiler": subprocess.check_output([args.compiler, "--version"], text=True).splitlines()[0],
        "source_sha256": {name: hashlib.sha256((project / name).read_bytes()).hexdigest()
                          for name in ["features/planetarium/planetarium.hpp", "features/planetarium/identity.hpp", "features/planetarium/stat_layout.hpp", "features/planetarium/tests.cpp"]},
        "results": [], "hardware_test": False,
    }
    status = 0
    for mode in ("optimized", "ubsan"):
        output = build / ("planetarium-tests-" + mode)
        flags = ["-O2"] if mode == "optimized" else ["-O1", "-g", "-fsanitize=undefined", "-fno-omit-frame-pointer"]
        command = [args.compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", *flags,
                   "-I", "features/planetarium", "features/planetarium/tests.cpp", "-o", str(output)]
        compilation = helper.bounded_run(command, project, 120)
        result = {"mode": mode, "command": command, "compile": compilation}
        if compilation["exit_code"] == 0:
            execution = helper.bounded_run([str(output)], project, 15)
            result["run"] = execution
            status = execution["exit_code"]
            print(execution["output"][:2048], end="")
        else:
            status = compilation["exit_code"]
            print(compilation["output"][:2048], end="")
        report["results"].append(result)
        if status:
            break
    report["status"] = "PASS" if status == 0 else "FAIL"
    report_path = project / "features" / "planetarium" / "test-results.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{report['status']}: {report_path}")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
