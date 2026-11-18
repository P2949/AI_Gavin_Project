#!/usr/bin/env python3
"""Fail-closed scope and engine inspection, no writes to project or engine."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

BASELINE = 'b740d47ceda2564a7cd0fb817079bfa8d6e4d5f8'  # merged awareness regression baseline

def require(condition, message):
    if not condition:
        raise ValueError(message)

def git(*args):
    return subprocess.check_output(['git', *args])

def records(output):
    return [os.fsdecode(item) for item in output.split(b'\0') if item]

def audit():
    # Regression is reusable on a clean or fully staged descendant. The
    # caller reviews the staged scope; this guard protects the tested bytes.
    git('merge-base', '--is-ancestor', BASELINE, 'HEAD')
    changes = records(git('status', '--porcelain=v1', '-z', '--untracked-files=all', '--no-renames'))
    for entry in changes:
        require(entry[:2] in ('A ', 'M ', 'D ', 'T '),
                f'Unstaged, untracked or unresolved change: {entry}')
    git('diff', '--cached', '--check')
    print(f'PASS: merged awareness baseline; {len(changes)} fully staged paths, no unstaged/untracked changes')


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
    p.add_argument('--engine-only',type=Path)
    p.add_argument('--snapshot',nargs=2,metavar=('DIRECTORY','LABEL'))
    a=p.parse_args()
    try:
        if a.engine_only: engine_inspection(a.engine_only)
        elif a.snapshot: snapshot(*a.snapshot)
        else: audit()
        return 0
    except (ValueError,OSError,subprocess.CalledProcessError,KeyError) as e:
        print(f'FAIL: {e}',file=sys.stderr)
        return 1

if __name__=='__main__': sys.exit(main())
