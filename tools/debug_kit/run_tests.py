"""Compile and exercise the same chord state machine used by the Switch adapter."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
build = root/'features/debug_kit/test-build'; build.mkdir(parents=True,exist_ok=True)
binary = build/'tests'
command = ['g++','-std=c++23','-O1','-g','-Wall','-Wextra','-Werror','-fno-omit-frame-pointer','-no-pie','-fsanitize=address,undefined','features/debug_kit/tests.cpp','-o',str(binary)]
compiled = subprocess.run(command,cwd=root,text=True,capture_output=True,timeout=120)
ran = subprocess.run([str(binary)],cwd=root,text=True,capture_output=True,timeout=20) if compiled.returncode==0 else None
status='PASS' if ran and ran.returncode==0 else 'FAIL'
sources=['features/debug_kit/policy.hpp','features/debug_kit/tests.cpp','native/source/program/debug_kit.cpp','native/source/program/debug_kit.hpp','native/source/program/generated/debug_kit_guards.hpp']
report={'timestamp_utc':datetime.now(timezone.utc).isoformat(),'result':status,'hardware_test':False,'compile_exit':compiled.returncode,'compile_output':compiled.stdout+compiled.stderr,'test_exit':ran.returncode if ran else None,'test_output':ran.stdout+ran.stderr if ran else '', 'source_sha256':{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in sources}}
(root/'evidence/debug_kit/test-results.json').write_text(json.dumps(report,indent=2)+'\n')
print(compiled.stdout+compiled.stderr,end='')
if ran: print(ran.stdout+ran.stderr,end='')
raise SystemExit(0 if status=='PASS' else 1)
