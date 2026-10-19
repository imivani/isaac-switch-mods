"""Compile the production Time Machine inspector/dispatch and exact archived regression."""
from datetime import datetime, timezone
import hashlib
import json
import re
from pathlib import Path
import subprocess
import zipfile

root = Path(__file__).resolve().parents[2]
build = root / 'features/timemachine/test-build'
build.mkdir(parents=True, exist_ok=True)
archive = root / 'deployments/20261015T113822-iteration4-baseline/source-before-iteration4.zip'
with zipfile.ZipFile(archive) as saved:
    matches = [x for x in saved.namelist() if x.replace('\\','/').endswith('source/program/timemachine.cpp')]
    assert len(matches) == 1, matches
    original_raw = saved.read(matches[0])
    old = original_raw.decode('utf-8-sig').replace('\r\n','\n')
legacy = old[old.index('tm::Snapshot Inspect('):old.index('HOOK_DEFINE_TRAMPOLINE(TimeMachineGameUpdate)')]
legacy = legacy.replace('tm::Snapshot Inspect(std::uintptr_t expectedGame)',
    'template<class Engine> Snapshot Inspect(Engine& engine, std::uintptr_t s_Module, std::uintptr_t expectedGame)')
legacy = legacy.replace('!s_Ready || !s_Module ||','!s_Module ||')
paused = 'reinterpret_cast<bool (*)(void*)>(s_Module + api::IsPaused)(reinterpret_cast<void*>(game))'
movement = 'reinterpret_cast<Direction (*)(void*)>(s_Module + api::MovementInput)(\n            reinterpret_cast<void*>(p.address))'
assert legacy.count(paused)==1 and legacy.count(movement)==1
legacy = legacy.replace(paused,'engine.Paused(game)').replace(movement,'engine.Movement(p.address)')
legacy = legacy.replace('    tm::Snapshot out;', '    auto Readable=[&](std::uintptr_t p,std::size_t n){return engine.Readable(p,n);};\n    auto TryRead=[&](std::uintptr_t p,auto& value){return engine.TryRead(p,value);};\n    auto Alive=[&](std::uintptr_t p,std::size_t n){return p>0x10000 && !(p&7) && Readable(p,n) && Load<std::uint8_t>(p,profile::EntityExists) && !Load<std::uint8_t>(p,profile::EntityDead);};\n    auto Pressed=[&](std::uintptr_t m,unsigned a,unsigned c,std::uintptr_t p){return engine.Pressed(m,a,c,p);};\n    auto Triggered=[&](std::uintptr_t m,unsigned a,unsigned c,std::uintptr_t p){return engine.Triggered(m,a,c,p);};\n    tm::Snapshot out;')
assert 'reinterpret_cast' not in legacy and 's_Ready' not in legacy
(build/'legacy_adapter.hpp').write_text('#pragma once\nnamespace legacy {\nnamespace tm=isaac::timemachine; using namespace isaac; using namespace tm; using tm::detail::Load; using tm::detail::Player;\n'+legacy+'\n}\n')

# Compile the actual native .cpp with only unavailable platform boundaries
# replaced. The callback bodies, guards, settings gate and lifecycle stay intact.
native_path=root/'native/source/program/timemachine.cpp'
native_source=native_path.read_text()
source=native_source
for old_call,new_call in [
    ('reinterpret_cast<bool (*)(void*)>(s_Module+api::IsPaused)(reinterpret_cast<void*>(game))','host->world.Paused(game)'),
    ('reinterpret_cast<bool (*)(void*,unsigned,unsigned,void*)>(s_Module+api::IsActionPressed)(\n            reinterpret_cast<void*>(manager),action,controller,reinterpret_cast<void*>(player))','host->world.Pressed(manager,action,controller,player)'),
    ('reinterpret_cast<bool (*)(void*,unsigned,unsigned,void*)>(s_Module+api::IsActionTriggered)(\n            reinterpret_cast<void*>(manager),action,controller,reinterpret_cast<void*>(player))','host->world.Triggered(manager,action,controller,player)'),
    ('reinterpret_cast<tm::Direction (*)(void*)>(s_Module+api::MovementInput)(reinterpret_cast<void*>(player))','host->world.Movement(player)')]:
    assert source.count(old_call)==1,old_call
    source=source.replace(old_call,new_call)
def include(match):
    name=match.group(1)
    if name in ('feature_api.hpp','lib.hpp'): return '// Platform boundary provided by test_hook.cpp'
    return '#include "'+str((native_path.parent/name).resolve())+'"'
source=re.sub(r'#include "([^"\n]+)"',include,source)
(build/'native_hook_under_test.cpp').write_text(source)

checks=[]
for name, source in [('policy','features/timemachine/tests.cpp'),('native-adapter','tools/timemachine/test_adapter.cpp'),('native-hooks','tools/timemachine/test_hook.cpp')]:
    binary=build/name
    command=['g++','-std=c++23','-O1','-g','-Wall','-Wextra','-Werror',
             '-fno-omit-frame-pointer','-no-pie','-fsanitize=address,undefined',
             '-I'+str(build),'-I'+str(root/'tools/timemachine'),source,'-o',str(binary)]
    compiled=subprocess.run(command,cwd=root,text=True,capture_output=True,timeout=120)
    ran=subprocess.run([str(binary)],cwd=root,text=True,capture_output=True,timeout=40) if compiled.returncode==0 else None
    check={'name':name,'compile_exit':compiled.returncode,'compile_output':compiled.stdout+compiled.stderr,
           'test_exit':ran.returncode if ran else None,'test_output':ran.stdout+ran.stderr if ran else ''}
    checks.append(check)
    print(check['compile_output']+check['test_output'],end='')

syntax_command=['/opt/devkitpro/devkitA64/bin/aarch64-none-elf-g++','-std=gnu++23','-fsyntax-only',
    '-march=armv8-a+crc+crypto','-mtune=cortex-a57','-mtp=soft','-fPIC','-fvisibility=hidden',
    '-Wall','-Werror','-fno-rtti','-fno-exceptions','-D__SWITCH__','-D__RTLD_6XX__',
    '-DEXL_LOAD_KIND=Module','-DEXL_LOAD_KIND_ENUM=2','-DEXL_PROGRAM_ID=0x010021C000B6A000',
    '-I/opt/devkitpro/libnx/include','-Inative/source','-Inative/include','-Inative/source/lib',
    'native/source/program/timemachine.cpp']
syntax=subprocess.run(syntax_command,cwd=root,text=True,capture_output=True,timeout=120)
print(syntax.stdout+syntax.stderr,end='')
if syntax.returncode==0: print('PASS: isolated Time Machine Switch compiler syntax check')
status='PASS' if all(c['compile_exit']==0 and c['test_exit']==0 for c in checks) and syntax.returncode==0 else 'FAIL'
sources=['features/timemachine/policy.hpp','features/timemachine/adapter.hpp','features/timemachine/dispatch.hpp',
    'features/timemachine/tests.cpp','tools/timemachine/test_fixture.hpp','tools/timemachine/test_hook.cpp','tools/timemachine/test_adapter.cpp','tools/timemachine/run_tests.py',
    'tools/timemachine/generate_guards.py','native/source/program/timemachine.cpp','native/source/program/timemachine.hpp',
    'native/source/program/generated/timemachine_guards.hpp']
report={'timestamp_utc':datetime.now(timezone.utc).isoformat(),'result':status,'hardware_test':False,
    'scope':'actual production memory inspector and dispatch, archived failing inspector, policy, isolated Switch syntax',
    'compile_exit':max(c['compile_exit'] for c in checks),'compile_output':''.join(c['compile_output'] for c in checks),
    'test_exit':0 if status=='PASS' else 1,'test_output':''.join(c['test_output'] for c in checks),
    'checks':checks,'switch_syntax_exit':syntax.returncode,'switch_syntax_output':syntax.stdout+syntax.stderr,
    'regression':{'archive':str(archive.relative_to(root)),
        'original_source_sha256':hashlib.sha256(original_raw).hexdigest(),
        'fixture_sha256':hashlib.sha256((build/'legacy_adapter.hpp').read_bytes()).hexdigest(),
        'production_hook_fixture_sha256':hashlib.sha256((build/'native_hook_under_test.cpp').read_bytes()).hexdigest(),
        'seams':'Only readable-memory validation, module globals, paused/input/movement native calls are injected. The archived Inspect body is otherwise unchanged.',
        'case':'Live type33 variant0, native State3, HP1, collision classes0: baseline rejects; fixed production adapter accelerates.'},
    'source_sha256':{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in sources}}
(root/'evidence/timemachine/test-results.json').write_text(json.dumps(report,indent=2)+'\n')
(root/'evidence/timemachine/iteration4/adapter-tests.json').write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(0 if status=='PASS' else 1)
