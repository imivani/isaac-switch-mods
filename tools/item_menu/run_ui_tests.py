#!/usr/bin/env python3
"""Exercise unchanged eid_ui.cpp and export actual glyph/inline-icon paint."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools/eid"))
from run_cpp_tests import bounded_run


def main():
    evidence=ROOT/"evidence/iteration4/item-menu"
    build=evidence/"ui-test-build"
    build.mkdir(parents=True,exist_ok=True)
    capture_path=evidence/"menu-preview.json"
    source=json.loads(capture_path.read_text())
    lines=["#pragma once","namespace capture {",
        f"inline constexpr float Width={source['width']}.f, Height={source['height']}.f;",
        "struct Call { unsigned screen,index,kind,item,color; const char* text; const char* name; const char* markup; float x,y,width,height; };",
        "inline constexpr Call Calls[]={"]
    for screen,row in enumerate(source["screens"]):
        for index,paint in enumerate(row["paint"]):
            kind={"text":1,"description":2}.get(paint["kind"],0)
            if not kind: continue
            values=[str(screen),str(index),str(kind),str(paint["item"]),str(paint["color"])]
            values += [json.dumps(paint[key],ensure_ascii=False) for key in ("text","name","markup")]
            values += [f"{float(paint[key])}f" for key in ("x","y","width","height")]
            lines.append("{"+",".join(values)+"},")
    lines += ["};","}",""]
    generated=build/"menu_ui_calls.hpp"
    generated.write_text("\n".join(lines))
    inputs=["tools/item_menu/run_ui_tests.py","tools/item_menu/test_eid_menu_ui.cpp","tools/eid/run_cpp_tests.py",
        "native/source/program/eid_ui.cpp","native/source/program/eid_ui.hpp","native/source/program/native_sprite.hpp",
        "native/source/program/feature_api.hpp","native/source/program/generated/build_profile.hpp",
        "eid_ui/layout.hpp","eid_ui/generated/assets.hpp","eid/catalogue.hpp","eid/markup.hpp",
        "eid/context.hpp","eid/pickup_policy.hpp","eid/generated/context.hpp","eid/generated/descriptions.cpp",
        "eid/test_support/lib/nx/kernel/svc.h","evidence/iteration4/item-menu/menu-preview.json",
        "evidence/iteration4/item-menu/ui-test-build/menu_ui_calls.hpp"]
    report={"timestamp_utc":datetime.now(timezone.utc).isoformat(),"hardware_test":False,
        "scope":"Unchanged production eid_ui.cpp (including the iteration 7 paper-menu ink, width, body and icon helpers) linked with recording NativeSprite, config getter trampolines, memory-query and fallback-canvas boundaries. No production-source substitution.",
        "compiler":subprocess.check_output(["g++","--version"],text=True).splitlines()[0],
        "source_sha256":{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in inputs},"results":[]}
    status=0
    for mode in ("optimized","ubsan"):
        executable=build/("eid-menu-ui-"+mode)
        output=evidence/("eid-menu-paint"+("-ubsan" if mode=="ubsan" else "")+".json")
        flags=["-O2"] if mode=="optimized" else ["-O1","-g","-fsanitize=undefined","-fno-sanitize-recover=all"]
        command=["g++","-std=c++20","-Wall","-Wextra","-Werror",*flags,"-I","eid/test_support","-I","eid","-I",str(build),
            "native/source/program/eid_ui.cpp","eid/generated/descriptions.cpp","tools/item_menu/test_eid_menu_ui.cpp","-o",str(executable)]
        result={"mode":mode,"compile":bounded_run(command,ROOT,120)}
        status=result["compile"]["exit_code"]
        if not status:
            result["run"]=bounded_run([str(executable),str(output)],ROOT,60)
            status=result["run"]["exit_code"]
            print(result["run"]["output"],end="")
            if not status: result["paint_sha256"]=hashlib.sha256(output.read_bytes()).hexdigest()
        else: print(result["compile"]["output"],end="")
        report["results"].append(result)
        if status: break
    report["status"]="PASS" if not status else "FAIL"
    (evidence/"ui-tests.json").write_text(json.dumps(report,indent=2)+"\n")
    return status


if __name__=="__main__": raise SystemExit(main())
