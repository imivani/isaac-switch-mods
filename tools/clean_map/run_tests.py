#!/usr/bin/env python3
"""Compile and test the actual CleanMap native predicate with bounded memory."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools/eid"))
from run_cpp_tests import bounded_run


def main():
    build = ROOT/"features/clean_map/test-build"
    build.mkdir(parents=True,exist_ok=True)
    inputs = ["tools/clean_map/run_tests.py","tools/eid/run_cpp_tests.py","native/source/program/clean_map.cpp",
        "native/source/program/clean_map.hpp","native/source/program/feature_api.hpp","native/source/program/mod_settings.hpp",
        "native/source/program/generated/build_profile.hpp","features/clean_map/generated/native.hpp",
        "features/clean_map/adapter_tests.cpp","eid/test_support/lib/nx/kernel/svc.h"]
    report = {"timestamp_utc":datetime.now(timezone.utc).isoformat(),"hardware_test":False,
        "scope":"Actual clean_map.cpp predicate and initialization; only memory-query and mod-setting boundaries mocked.",
        "compiler":subprocess.check_output(["g++","--version"],text=True).splitlines()[0],
        "source_sha256":{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in inputs},"results":[]}
    status = 0
    for mode in ("optimized","ubsan"):
        executable = build/("clean-map-"+mode)
        flags = ["-O2"] if mode == "optimized" else ["-O1","-g","-fsanitize=undefined","-fno-sanitize-recover=all"]
        command = ["g++","-std=c++17","-Wall","-Wextra","-Werror",*flags,"-I","eid/test_support",
            "native/source/program/clean_map.cpp","features/clean_map/adapter_tests.cpp","-o",str(executable)]
        result = {"mode":mode,"compile":bounded_run(command,ROOT,120)}
        status = result["compile"]["exit_code"]
        if not status:
            result["run"] = bounded_run([str(executable)],ROOT,40)
            status = result["run"]["exit_code"]
            print(result["run"]["output"],end="")
        else: print(result["compile"]["output"],end="")
        report["results"].append(result)
        if status: break
    report["status"] = "PASS" if not status else "FAIL"
    (ROOT/"features/clean_map/test-results.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    return status


if __name__ == "__main__": raise SystemExit(main())
