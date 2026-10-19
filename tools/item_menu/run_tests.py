from pathlib import Path
import hashlib
import json
import subprocess
from datetime import datetime, timezone
root=Path(__file__).resolve().parents[2]
out=root/'features/item_menu/test-build';out.mkdir(parents=True,exist_ok=True)
runs=[]
for mode,flags in [('optimized',['-O2']),('sanitized',['-O1','-g','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined'])]:
    binary=out/mode
    command=['g++','-std=c++23','-Wall','-Wextra','-Werror',*flags,'features/item_menu/tests.cpp','-o',str(binary)]
    built=subprocess.run(command,cwd=root,text=True,capture_output=True)
    ran=subprocess.run([str(binary)],cwd=root,text=True,capture_output=True) if built.returncode==0 else None
    output=built.stdout+built.stderr+(ran.stdout+ran.stderr if ran else '')
    print(output,end='')
    runs.append({'mode':mode,'compile_exit':built.returncode,'exit':ran.returncode if ran else None,'output':output})
sources=['features/item_menu/state.hpp','features/item_menu/adapter.hpp','features/item_menu/tests.cpp','native/source/program/generated/item_menu_catalogue.hpp','tools/item_menu/run_tests.py']
report={'result':'PASS' if all(r['exit']==0 for r in runs) else 'FAIL','timestamp_utc':datetime.now(timezone.utc).isoformat(),'hardware_test':False,'runs':runs,
        'source_sha256':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sources},'limits':'Host tests execute shared control and memory adapter code; native game function calls are simulated.'}
dest=root/'evidence/iteration4/item-menu/test-results.json';dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(0 if report['result']=='PASS' else 1)
