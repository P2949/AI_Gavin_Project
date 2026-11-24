#!/usr/bin/env python3
"""Read-only triage of RETAINED Pedro UE Editor sightline reports.

This is not an Unreal probe, an editor authoring program or an acceptance
verifier. It never treats a Python script's JSON as proof of runtime gameplay.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

MODES = ('probe-test', 'align-test', 'probe-production', 'align-production')
RELEVANT = re.compile(
    r'PEDRO_VISIBILITY|LogPython:\s*(?:Error|Warning)|Traceback|RuntimeError|'
    r'Error:\s|Assertion failed:|Fatal error:|Segmentation fault', re.I)
MAX_JSON = 1024 * 1024
MAX_LOG = 64 * 1024 * 1024


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise ValueError('PEDRO_VISIBILITY_DIAG STOP: ' + reason)


def resolve_child(base: Path, candidate: Path) -> Path:
    require(candidate.is_absolute(), 'report path must be absolute')
    require(not candidate.is_symlink(), 'report directory must not be a symlink')
    path = candidate.resolve(strict=True)
    require(path.is_relative_to(base.resolve(strict=True)), 'report outside Saved/PedroValidation')
    require(path.is_dir(), 'report path is not a directory')
    return path


def read_file(folder: Path, name: str, limit: int = MAX_LOG) -> str | None:
    p = folder / name
    if not p.exists():
        return None
    require(p.is_file() and not p.is_symlink(), 'not a regular report file: ' + name)
    require(p.stat().st_size <= limit, 'report too large: ' + name)
    return p.read_text(encoding='utf-8', errors='replace')


def digest_paths(text: str | None) -> dict[str, str] | None:
    if text is None:
        return None
    rows = {}
    for line in text.splitlines():
        m = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
        require(m is not None, 'malformed report file manifest')
        path = m.group(2)
        require(path and path not in rows and not path.startswith('/') and '..' not in Path(path).parts,
                'unsafe or duplicate report manifest entry')
        rows[path] = m.group(1)
    return rows


def excerpt_log(folder: Path, name: str) -> list[str]:
    data = read_file(folder, name)
    if data is None:
        return []
    hits = [row.strip()[:650] for row in data.splitlines() if RELEVANT.search(row)]
    return hits[-14:]


def describe(folder: Path, base: Path, requested_mode: str | None = None) -> dict:
    folder = resolve_child(base, folder)
    raw = read_file(folder, 'results.json', MAX_JSON)
    if raw is None:
        result = {}
        report_state = 'MISSING_RESULTS'
    else:
        try:
            result = json.loads(raw)
        except (ValueError, TypeError) as exc:
            raise ValueError('invalid Unreal results.json: ' + str(exc)) from exc
        require(isinstance(result, dict), 'results.json must be a JSON object')
        report_state = result.get('status', 'MISSING_STATUS')
    actual_mode = result.get('mode') if result else None
    if requested_mode is not None:
        require(requested_mode in MODES, 'invalid requested mode')
        if actual_mode is not None:
            require(actual_mode == requested_mode, 'unexpected report mode')
    mode = requested_mode or actual_mode or 'UNKNOWN'
    require(mode in (*MODES, 'UNKNOWN'), 'unexpected report mode')
    first = digest_paths(read_file(folder, 'files-before.sha256'))
    last = digest_paths(read_file(folder, 'files-after.sha256'))
    changes = None
    if first is not None and last is not None:
        changes = sorted(k for k in first.keys() | last.keys()
                         if first.get(k) != last.get(k))
    pre_status = (folder / 'status-before.txt')
    post_status = (folder / 'status-after.txt')
    git_status_unchanged = None
    if pre_status.exists() and post_status.exists():
        require(pre_status.is_file() and post_status.is_file() and
                not pre_status.is_symlink() and not post_status.is_symlink(),
                'invalid Git status snapshot')
        git_status_unchanged = pre_status.read_bytes() == post_status.read_bytes()
    exit_text = read_file(folder, 'editor-exit.txt', 128)
    if exit_text is not None:
        require(re.fullmatch(r'\s*\d+\s*', exit_text) is not None, 'invalid Editor exit code')
    original = result.get('original_sight')
    candidate = result.get('candidate')
    source_details = original.get('sources') if isinstance(original, dict) else None
    if not isinstance(source_details, list):
        source_details = []
    problem = str(result.get('reason', ''))[:1200]
    if report_state == 'BLOCKED' and problem == 'NoRotationOnlyVisibleAndHiddenCore':
        next_gate = 'NO_MAP_EDIT__REVIEW_MEASURED_GEOMETRY'
    elif report_state == 'BLOCKED':
        next_gate = 'NO_MAP_EDIT__REVIEW_BLOCK_REASON'
    elif report_state == 'FAIL' or report_state == 'MISSING_RESULTS':
        next_gate = 'NO_MAP_EDIT__DEBUG_EDITOR_OR_SCRIPT'
    elif report_state == 'PASS' and mode.startswith('probe-'):
        next_gate = 'PROBE_REPORTED_PASS__RUN_INDEPENDENT_AUDIT_BEFORE_ALIGN'
    elif report_state == 'PASS':
        next_gate = 'ALIGN_REPORTED_PASS__RUN_INDEPENDENT_AUDIT_AND_GAMEPLAY'
    else:
        next_gate = 'NO_MAP_EDIT__UNKNOWN_REPORT_STATE'
    return {
        'mode': mode,
        'report': str(folder),
        'unreal_status': report_state,
        'unreal_reason': problem or None,
        'editor_exit_code': int(exit_text) if exit_text is not None else None,
        'map': result.get('map'),
        'map_sha256': result.get('map_sha256'),
        'best_possible_visible': result.get('best_visible_count'),
        'candidate_count': result.get('evaluated_yaws'),
        'rotation_needed': result.get('rotation_needed'),
        'proposed_yaw': candidate.get('yaw') if isinstance(candidate, dict) else None,
        'original_sight': source_details,
        'saved_file_changes': changes,
        'project_files_before': len(first) if first is not None else None,
        'git_status_unchanged': git_status_unchanged,
        'error_excerpt': str(result.get('traceback', ''))[-1800:] if result.get('traceback') else None,
        'editor_log_errors': excerpt_log(folder, 'editor.log'),
        'terminal_log_errors': excerpt_log(folder, 'terminal.log'),
        'next_gate': next_gate,
    }


def report_dirs(base: Path, modes: list[str]) -> list[tuple[Path, str]]:
    found = []
    for mode in modes:
        require(mode in MODES, 'invalid requested mode')
        pointer = base / ('b2b6e-visibility-' + mode + '-latest.txt')
        require(pointer.is_file() and not pointer.is_symlink(), 'missing latest pointer: ' + mode)
        value = pointer.read_text(encoding='utf-8').strip()
        require(len(value) < 4096, 'oversized latest pointer')
        found.append((resolve_child(base, Path(value)), mode))
    return found


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--run', type=Path, help='an exact report directory, instead of latest pointers')
    parser.add_argument('--mode', choices=MODES, action='append',
                        help='restrict latest-pointer diagnostics (repeatable)')
    args = parser.parse_args()
    project = args.project.resolve(strict=True)
    require((project / 'AI_Gavin_Project.uproject').is_file(), 'not the expected Unreal project')
    base = (project / 'Saved/PedroValidation').resolve(strict=True)
    if args.run is not None:
        selected = [(resolve_child(base, args.run), None)]
    else:
        selected = report_dirs(base, args.mode or
            ['probe-test', 'align-test', 'probe-production'])
    reports = [describe(folder, base, mode) for folder, mode in selected]
    print(json.dumps({'status': 'DIAGNOSTICS_ONLY__NOT_ACCEPTANCE',
                      'reports': reports}, indent=2, sort_keys=True))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError) as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(2)
