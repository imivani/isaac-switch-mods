#!/usr/bin/env python3
"""Portable behavior/layout tests; never launches or modifies the game."""
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
    sources = ["eid/context.hpp", "eid/context_tests.cpp", "eid/generated/context.hpp", "eid/catalogue.hpp",
               "eid/pickup_policy.hpp", "eid/generated/descriptions.cpp", "eid_ui/layout.hpp", "eid_ui/generated/assets.hpp"]
    report = {"timestamp_utc":datetime.now(timezone.utc).isoformat(),"hardware_test":False,
              "compiler":subprocess.check_output(["g++","--version"],text=True).splitlines()[0],
              "source_sha256":{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in sources},"results":[]}
    status = 0
    for mode in ("optimized","ubsan"):
        executable = build / ("eid-context-"+mode)
        flags = ["-O2"] if mode=="optimized" else ["-O1","-g","-fsanitize=undefined","-fno-omit-frame-pointer"]
        command = ["g++","-std=c++17","-Wall","-Wextra","-Werror",*flags,"-I","eid","eid/context_tests.cpp","eid/generated/descriptions.cpp","-o",str(executable)]
        result = {"mode":mode,"command":command,"compile":bounded_run(command,ROOT,120)}
        status = result["compile"]["exit_code"]
        if not status:
            result["run"] = bounded_run([str(executable)],ROOT,40)
            status = result["run"]["exit_code"]
            print(result["run"]["output"],end="")
        else: print(result["compile"]["output"],end="")
        report["results"].append(result)
        if status: break
    report["status"] = "PASS" if not status else "FAIL"
    (ROOT/"eid/context-test-results.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    return status


if __name__ == "__main__": raise SystemExit(main())
