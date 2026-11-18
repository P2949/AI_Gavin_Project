#!/usr/bin/env python3
"""Fail-closed scope and engine inspection, no writes to project or engine."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

BASELINE = 'afb94d071d359ad01ff10967a13f65b3bdaa0c2d'
BRANCH = 'feature/pedro-observer-awareness'
SRC = 'Source/AI_Gavin_Project/Encounters/Pedro/'
TOOLS = 'Tools/PedroValidation/'
EXPECTED = frozenset([
    SRC+'PedroAwarenessState.h', SRC+'PedroObserverAIController.h',
    SRC+'PedroObserverAIController.cpp', SRC+'Tests/PedroAwarenessStateTests.cpp',
    SRC+'Tests/PedroObserverAIControllerTests.cpp', SRC+'Tests/PedroAwarenessWorldTests.cpp',
    TOOLS+'b2b6a_audit.py', TOOLS+'b2b6a_run.sh', TOOLS+'verify_awareness_report.py',
    TOOLS+'tests/test_awareness_report.py', 'docs/encounters/pedro-observer-awareness.md',
])

def require(condition, message):
    if not condition:
        raise ValueError(message)

def git(*args):
    return subprocess.check_output(['git', *args])

def records(output):
    return [os.fsdecode(item) for item in output.split(b'\0') if item]

def audit(clean=False):
    require(git('branch','--show-current').decode().strip() == BRANCH, 'wrong branch')
    require(git('rev-parse','HEAD').decode().strip() == BASELINE,
            'expected exact pre-implementation HEAD; do not automatically rebase')
    changes = records(git('status','--porcelain=v1','-z','--untracked-files=all'))
    if clean:
        require(not changes, 'installation requires clean baseline')
        require(all(not Path(p).exists() and not Path(p).is_symlink() for p in EXPECTED), 'destination already exists')
        print('PASS: clean exact baseline, all destinations absent')
        return
    actual = {}
    for entry in changes:
        status, path = entry[:2], entry[3:]
        require(status in ('??','A ','AM'), f'unexpected status: {entry}')
        require(path not in actual, f'duplicate path: {path}')
        actual[path] = status
    require(set(actual) == EXPECTED,
            f'changed paths mismatch: extra={sorted(set(actual)-EXPECTED)} missing={sorted(EXPECTED-set(actual))}')
    for path in sorted(EXPECTED):
        require(Path(path).is_file() and not Path(path).is_symlink(), f'not an ordinary file: {path}')
        print(f'PASS: {actual[path]} {path}')
    print('PASS: exactly 11 new milestone files; no baseline file changes')

def snapshot(directory, label):
    directory=Path(directory)
    status=git('status','--porcelain=v1','-z','--untracked-files=all')
    paths=sorted(set(records(git('ls-files','-z','--cached','--others','--exclude-standard'))))
    hashes=[]
    for path in paths:
        p=Path(path)
        require(p.is_file() or p.is_symlink(), f'cannot hash: {p}')
        h=hashlib.sha256()
        if p.is_symlink():
            h.update(b'symlink\0'+os.fsencode(os.readlink(p)))
        else:
            with p.open('rb') as f:
                for block in iter(lambda:f.read(1024*1024),b''):
                    h.update(block)
        hashes.append(f'{h.hexdigest()}  {path}\n')
    (directory/f'status-{label}.txt').write_bytes(status)
    (directory/f'files-{label}.sha256').write_text(''.join(hashes),encoding='utf-8')
    print(f'PASS: {label} snapshot, {len(hashes)} files hashed (real LFS working data)')

def locate(root, name):
    files=sorted(root.rglob(name))
    require(len(files)==1,f'expected one {name} in {root}; found {files}')
    return files[0]

def engine_inspection(engine):
    engine=Path(engine).expanduser().resolve()
    version=engine/'Engine/Build/Build.version'
    require(version.is_file(),f'missing {version}')
    v=json.loads(version.read_text())
    require(tuple(v[k] for k in ('MajorVersion','MinorVersion','PatchVersion'))==(5,8,2),f'expected UE 5.8.2, got {v}')
    print('PASS: local engine Build.version is 5.8.2')
    ubt=engine/'Engine/Source/Programs/UnrealBuildTool/Configuration/Rules/TargetRules.cs'
    data=ubt.read_text(encoding='utf-8-sig')
    require('[CommandLine("-DisableAdaptiveUnity", Value = "false")]' in data
            and 'bUseAdaptiveUnityBuild' in data, f'local adaptive Unity flag missing: {ubt}')
    print(f'PASS: -DisableAdaptiveUnity declared in {ubt.relative_to(engine)}')
    root=engine/'Engine/Source/Runtime/AIModule'
    require(root.is_dir(),f'missing AIModule headers: {root}')
    checks={
        'AIController.h': ['UpdateControlRotation','OnPossess','SetPerceptionComponent'],
        'AIPerceptionComponent.h': ['GetLocationAndDirection','GetActorsPerception','SetSenseEnabled','IsSenseEnabled','RequestStimuliListenerUpdate','ForgetAll','OnTargetPerceptionUpdated'],
        'AISenseConfig_Sight.h': ['SightRadius','LoseSightRadius','PeripheralVisionAngleDegrees','AutoSuccessRangeFromLastSeenLocation','PointOfViewBackwardOffset','NearClippingRadius'],
        'AISenseConfig.h': ['GetSenseID','GetMaxAge','SetMaxAge'],
        'AIPerceptionTypes.h': ['WasSuccessfullySensed','IsExpired','GetAge','StimulusLocation'],
        'AITypes.h': ['InvalidLocation','InvalidRange','IsValidLocation'],
    }
    for name, tokens in checks.items():
        path=locate(root,name)
        data=path.read_text(encoding='utf-8',errors='replace')
        require(all(t in data for t in tokens),f'API tokens missing in {path}: {[t for t in tokens if t not in data]}')
        print(f'PASS: {path.relative_to(engine)} ({", ".join(tokens)})')
    # The installed engine distribution may contain only public headers. A
    # token check is a capability preflight, not a proof of semantics.
    inspect={
        'AIPerceptionComponent.cpp': ['GetLocationAndDirection(', 'ProcessStimuli(', 'RefreshStimulus(', 'ForgetAll(', 'SetSenseEnabled('],
        'AISense_Sight.cpp': ['bAutoRegisterAllPawnsAsSources', 'NotifyType', 'StimulusLocation', 'RegisterTarget(', 'RegisterStimulus(', 'DefaultSightCollisionChannel'],
        'AIPerceptionSystem.cpp': ['GetLocationAndDirection(', 'RegisterAllPawnsAsSources', 'Tick('],
        'AIController.cpp': ['GetActorEyesViewPoint(', 'GetPlayerViewPoint(', 'UpdateControlRotation('],
    }
    for name, tokens in inspect.items():
        paths=sorted(root.rglob(name))
        if not paths:
            print(f'NOTE: {name} not distributed; runtime tests remain mandatory')
            continue
        for path in paths:
            lines=path.read_text(encoding='utf-8',errors='replace').splitlines()
            print(f'\n--- engine inspection: {path.relative_to(engine)} ---')
            indices=set()
            for token in tokens:
                matches=[i for i,line in enumerate(lines) if token in line]
                for i in matches[:3]:
                    indices.update(range(max(0,i-3),min(len(lines),i+19)))
                if not matches: print(f'NOTE: token absent: {token}')
            for i in sorted(indices): print(f'{i+1}: {lines[i]}')
    print('PASS: local API token inspection; compile and native sight checks establish actual compatibility')

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--clean',action='store_true')
    p.add_argument('--engine-only',type=Path)
    p.add_argument('--snapshot',nargs=2,metavar=('DIRECTORY','LABEL'))
    a=p.parse_args()
    try:
        if a.engine_only: engine_inspection(a.engine_only)
        elif a.snapshot: snapshot(*a.snapshot)
        else: audit(a.clean)
        return 0
    except (ValueError,OSError,subprocess.CalledProcessError,KeyError) as e:
        print(f'FAIL: {e}',file=sys.stderr)
        return 1

if __name__=='__main__': sys.exit(main())
