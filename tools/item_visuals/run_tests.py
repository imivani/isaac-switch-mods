#!/usr/bin/env python3
"""Run bounded host checks, including the actual shared texture-swap helper."""
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
    build = ROOT / "features/item_visuals/test-build"
    build.mkdir(parents=True, exist_ok=True)
    sources = ["features/item_visuals/state.hpp", "features/item_visuals/generated.hpp",
               "features/item_visuals/exchange_scope.hpp", "features/item_visuals/tests.cpp",
               "features/item_visuals/native_sprite_tests.cpp", "features/item_visuals/adapter_tests.cpp",
               "features/item_visuals/native_calls.hpp", "native/source/program/item_visuals.cpp",
               "native/source/program/item_visuals.hpp", "native/source/program/native_sprite.cpp",
               "native/source/program/native_sprite.hpp", "native/source/program/feature_api.hpp",
               "native/source/program/mod_settings.hpp", "native/source/program/clean_map.hpp",
               "native/source/program/generated/build_profile.hpp", "tools/item_visuals/stubs/lib.hpp",
               "tools/item_visuals/stubs/lib/nx/kernel/svc.h", "tools/item_visuals/run_tests.py"]
    report = {"timestamp_utc": datetime.now(timezone.utc).isoformat(), "hardware_test": False,
              "compiler": subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
              "source_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in sources},
              "results": []}
    status = 0
    for mode in ("optimized", "ubsan"):
        flags = ["-O2"] if mode == "optimized" else ["-O1", "-g", "-fsanitize=undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]
        for name, inputs in (("behavior", ["features/item_visuals/tests.cpp"]),
                             ("native-texture", ["features/item_visuals/native_sprite_tests.cpp", "native/source/program/native_sprite.cpp"]),
                             ("actual-item-adapter", ["features/item_visuals/adapter_tests.cpp"])):
            executable = build / (name + "-" + mode)
            command = ["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", *flags,
                       "-DISAAC_ITEM_VISUALS_ADAPTER_TEST",
                       "-I", "tools/item_visuals/stubs", *inputs, "-o", str(executable)]
            result = {"mode": mode, "suite": name, "command": command, "compile": helper.bounded_run(command, ROOT, 120)}
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
        if status:
            break
    report["status"] = "PASS" if not status else "FAIL"
    (ROOT / "features/item_visuals/test-results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
