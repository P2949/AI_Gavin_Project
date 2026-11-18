#!/usr/bin/env python3
"""Fail-closed scope and engine inspection, no writes to project or engine."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import sys

BASELINE = '3505be00d2816442ed3c1fbbe82f7b31a0d19a24'  # merged navigation regression baseline

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
    print(f'PASS: merged navigation baseline; {len(changes)} fully staged paths, no unstaged/untracked changes')


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

def inspect_unity_flags(engine):
    # UE 5.8.2 moved DisableUnity onto TargetDescriptor; adaptive Unity
    # remains a TargetRules setting. Bind the attribute to its actual property.
    configuration=engine/'Engine/Source/Programs/UnrealBuildTool/Configuration'
    descriptor=configuration/'Descriptors/TargetDescriptor.cs'
    rules=configuration/'Rules/TargetRules.cs'
    for path, flag, prop, value in (
        (descriptor, '-DisableUnity', 'bUseUnityBuild', 'false'),
        (rules, '-DisableAdaptiveUnity', 'bUseAdaptiveUnityBuild', 'false'),
        (rules, '-ForceUnity', 'bForceUnityBuild', None),
    ):
        data=path.read_text(encoding='utf-8-sig')
        value_pattern=r'\s*,\s*Value\s*=\s*"'+value+'"' if value is not None else ''
        pattern=(r'\[CommandLine\(\s*"'+re.escape(flag)+'"'+value_pattern+r'\s*\)\]'
                 r'(?:\s*\[[^\]\r\n]+\])*\s*public\s+bool\s+'+prop+r'\s*\{')
        require(re.search(pattern,data) is not None,
                f'local {flag} declaration for {prop} missing: {path}')
        print(f'PASS: {flag} declared for {prop} in {path.relative_to(engine)}')

def engine_inspection(engine):
    engine=Path(engine).expanduser().resolve()
    version=engine/'Engine/Build/Build.version'
    require(version.is_file(),f'missing {version}')
    v=json.loads(version.read_text())
    require(tuple(v[k] for k in ('MajorVersion','MinorVersion','PatchVersion'))==(5,8,2),f'expected UE 5.8.2, got {v}')
    print('PASS: local engine Build.version is 5.8.2')
    inspect_unity_flags(engine)
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
    # Verify the installed source actually invokes the rotation hook in Tick.
    controller=locate(root,'AIController.cpp')
    controller_data=controller.read_text(encoding='utf-8',errors='replace')
    tick=re.search(r'void\s+AAIController::Tick\s*\([^)]*\)\s*\{',controller_data)
    require(tick is not None,'AAIController::Tick not found in local source')
    depth=1;end=tick.end()
    for i in range(end,len(controller_data)):
        if controller_data[i]=='{':depth+=1
        elif controller_data[i]=='}':depth-=1
        if depth==0:end=i;break
    require(depth==0 and re.search(r'UpdateControlRotation\s*\(\s*DeltaTime\s*(?:,\s*true\s*)?\)',controller_data[tick.end():end]) is not None,
            'local Tick does not call the expected rotation hook; inspect before building')
    print(f'PASS: local AAIController::Tick calls UpdateControlRotation(DeltaTime)')
    print(controller_data[tick.start():end+1])
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
    navigation_checks = {
        'NavigationSystem/Public/NavigationSystem.h': ['GetNavDataForProps','ProjectPointToNavigation','FindPathSync','OnNavigationBoundsUpdated'],
        'NavigationSystem/Public/NavigationData.h': ['EnableRecalculationOnInvalidation','IsUpToDate','IsPartial','ContainsAnyCustomLink','GetPathPoints'],
        'NavigationSystem/Public/NavMesh/NavMeshPath.h': ['FNavMeshNodeFlags','IsNavLink'],
        'NavigationSystem/Public/NavMesh/RecastNavMesh.h': ['ARecastNavMesh'],
    }
    runtime=engine/'Engine/Source/Runtime'
    for rel,tokens in navigation_checks.items():
        f=runtime/rel
        if not f.is_file(): f=locate(runtime/rel.split('/')[0],Path(rel).name)
        data=f.read_text(encoding='utf-8',errors='replace')
        require(all(t in data for t in tokens),f'API tokens missing in {f}: {[t for t in tokens if t not in data]}')
        print(f'PASS: {f.relative_to(engine)} ({", ".join(tokens)})')
    for module,name,tokens in (
        ('AIModule','PathFollowingComponent.h',['RequestMove','AbortMove','GetCurrentRequestId','HasReached']),
        ('Engine','NavMovementComponent.h',['GetNavMovementProperties','GetNavAgentPropertiesRef']),
        ('Engine','NavigationTypes.h',['FNavMovementProperties','bUseAccelerationForPaths','CustomNavLinkId']),
    ):
        matches=[f for f in (runtime/module).rglob(name) if all(t in f.read_text(encoding='utf-8',errors='replace') for t in tokens)]
        require(len(matches)==1,f'local navigation API ambiguous or missing: {name}: {matches}')
        print(f'PASS: {matches[0].relative_to(engine)} ({", ".join(tokens)})')
    # Token inspection is not runtime NavMesh proof. Run context-probe first for the new behaviours.
    print('PASS: local API token inspection; compile and native sight/navigation checks establish actual compatibility')

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
