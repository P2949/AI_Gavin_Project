#!/usr/bin/env python3
"""Pedro editor snapshot diagnostics and deliberately narrow pycache recovery.

Never deletes files, edits binary assets, alters Git's index, or weakens the
read-only invariant. Recovery requires exact bytes from the failed probe's
recorded after-snapshot and ONLY two known project-local Python caches.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

CACHE_RE = re.compile(
    r'Tools/PedroValidation/__pycache__/b2b6e_(?:contract|editor_common)'
    r'\.cpython-\d{2,3}\.pyc\Z'
)


def require(predicate: bool, reason: str) -> None:
    if not predicate:
        raise ValueError(reason)


def read_hashes(path: Path) -> dict[str, str]:
    values = {}
    for row in path.read_text(encoding='utf-8').splitlines():
        digest, sep, name = row.partition('  ')
        require(bool(sep) and re.fullmatch('[a-f0-9]{64}', digest) is not None,
                'invalid snapshot row in ' + str(path) + ': ' + repr(row[:100]))
        require(name not in values, 'duplicate path in snapshot: ' + name)
        require(name and not Path(name).is_absolute() and '..' not in Path(name).parts,
                'unsafe path in snapshot: ' + name)
        values[name] = digest
    return values


def compare_manifests(before: dict[str, str], after: dict[str, str]) -> dict:
    b, a = set(before), set(after)
    return {
        'added': sorted(a - b),
        'removed': sorted(b - a),
        'modified': sorted(x for x in (b & a) if before[x] != after[x]),
        'before_file_count': len(b),
        'after_file_count': len(a),
    }


def run_snapshot_diff(run: Path) -> tuple[dict, dict]:
    for name in ('files-before.sha256', 'files-after.sha256',
                 'status-before.txt', 'status-after.txt'):
        require((run/name).is_file(), 'missing probe snapshot ' + str(run/name))
    b = read_hashes(run/'files-before.sha256')
    a = read_hashes(run/'files-after.sha256')
    diff = compare_manifests(b, a)
    diff['status_changed'] = ((run/'status-before.txt').read_bytes() !=
                              (run/'status-after.txt').read_bytes())
    diff['project_bytes_changed'] = bool(diff['added'] or diff['removed'] or diff['modified'])
    diff['candidate_import_bytecode'] = (not diff['modified'] and not diff['removed'] and
        len(diff['added']) == 2 and
        all(CACHE_RE.fullmatch(x) for x in diff['added']))
    (run/'snapshot-diff.json').write_text(json.dumps(diff, indent=2, sort_keys=True) + '\n')
    return diff, a


def check_run(project: Path, run: Path) -> None:
    require((project/'.git').exists() and (project/'AI_Gavin_Project.uproject').is_file(),
            'wrong Git project root')
    allowed = (project/'Saved/PedroValidation').resolve()
    resolved = run.resolve()
    require(run.is_dir() and resolved.parent == allowed and
            run.name.startswith('b2b6e-probe-'),
            'expected an existing b2b6e-probe-* report under project Saved/PedroValidation')


def git_bytes(project: Path, *arguments: str) -> bytes:
    result = subprocess.run(['git', '-C', str(project), *arguments],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise ValueError('git ' + ' '.join(arguments) + ': ' + result.stderr.decode(errors='replace'))
    return result.stdout


def hash_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as inp:
        for block in iter(lambda: inp.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def recovery_preflight(project: Path, run: Path, report: dict, recorded_after: dict) -> list[Path]:
    added = report['added']
    require(report['candidate_import_bytecode'],
            'NOT confirmed generated Python bytecode; inspect snapshot-diff.json. No files moved.')
    require(git_bytes(project, 'diff', '--name-only') == b'',
            'unstaged tracked modifications exist; no recovery permitted')
    current = git_bytes(project, 'status', '--porcelain=v1', '-z', '--untracked-files=all')
    require(current == (run/'status-after.txt').read_bytes(),
            'current Git status differs from failed probe after-snapshot; no recovery permitted')
    untracked = {os.fsdecode(x) for x in git_bytes(project, 'ls-files', '-z',
                   '--others', '--exclude-standard').split(b'\0') if x}
    require(untracked == set(added),
            'current untracked files do not exactly equal recorded generated caches: '
            + repr(sorted(untracked)))
    checked = []
    for relative in added:
        source = project/relative
        require(source.is_file() and not source.is_symlink(),
                'cache file replaced by nonregular file: ' + relative)
        require(hash_file(source) == recorded_after[relative],
                'generated file changed since probe: ' + relative)
        require((run/'quarantined-bytecode'/relative).exists() is False,
                'quarantine destination already exists: ' + relative)
        checked.append(source)
    return checked


def reconcile(project: Path, run: Path, apply: bool) -> dict:
    project = project.expanduser().resolve()
    run = run.expanduser().resolve()
    check_run(project, run)
    diff, after = run_snapshot_diff(run)
    out = {'run': str(run), 'snapshot_diff': diff, 'recovery_permitted': False}
    if not diff['candidate_import_bytecode']:
        print(json.dumps(out, indent=2))
        require(not apply,
                'recovery refused; changes are not exactly two known Python cache files')
        return out
    paths = recovery_preflight(project, run, diff, after)
    out['recovery_permitted'] = True
    out['files'] = [str(p.relative_to(project)) for p in paths]
    if not apply:
        print(json.dumps(out, indent=2))
        return out
    quarantined = []
    for path in paths:
        relative = path.relative_to(project)
        destination = run/'quarantined-bytecode'/relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        # Same-filesystem atomic rename. Deliberately no copy+unlink fallback.
        os.rename(path, destination)
        quarantined.append({'old': str(relative), 'saved': str(destination),
                            'sha256': after[str(relative)]})
    require(git_bytes(project, 'status', '--porcelain=v1', '-z', '--untracked-files=all') ==
            (run/'status-before.txt').read_bytes(),
            'Git status after quarantine differs from the original before-snapshot')
    out['quarantined'] = quarantined
    out['reconciled_to_before_snapshot'] = True
    (run/'quarantine-result.json').write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps(out, indent=2))
    return out


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='command', required=True)
    s = sub.add_parser('snapshots', help='Describe before/after from a retained run')
    s.add_argument('run', type=Path)
    r = sub.add_parser('recover', help='Identify or quarantine ONLY proven generated bytecode')
    r.add_argument('--project', type=Path, default=Path.cwd())
    r.add_argument('--run', type=Path)
    r.add_argument('--apply', action='store_true', help='Move verified caches into Saved report')
    a = p.parse_args()
    if a.command == 'snapshots':
        diff, _ = run_snapshot_diff(a.run)
        print(json.dumps(diff, indent=2))
    else:
        project = a.project.expanduser().resolve()
        if a.run is None:
            pointer = project/'Saved/PedroValidation/b2b6e-probe-latest.txt'
            require(pointer.is_file(), 'missing latest probe report pointer')
            run = Path(pointer.read_text().strip())
        else:
            run = a.run
        reconcile(project, run, a.apply)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError) as error:
        print('STOP: ' + str(error), file=sys.stderr)
        sys.exit(1)
