#!/usr/bin/env python3
"""Strict verifier of six real-game Pedro playable smoke cases.

Requires the exact map package passed to the game. Does not create success
markers or infer game success from process exit alone.
"""
from __future__ import annotations
import json
from pathlib import Path
import re
import sys

from b2b6e_contract import PRODUCTION, TEST

TAG = 'PEDRO_PLAYABLE_TEST '
STEPS = ('Configuration', 'Activation', 'SavedNavmesh', 'AutonomousFollow',
         'RealMeleeMiss', 'RealMeleeContact')
FATAL = re.compile(
    r'Fatal error:|Assertion failed:|Ensure condition failed:|Unhandled Exception:|'
    r'Signal 11 caught|Segmentation fault', re.I)


def tokens(line):
    pairs = [word.split('=', 1) for word in line.split() if '=' in word]
    if len(set(key for key, _ in pairs)) != len(pairs):
        raise ValueError('Duplicate marker field')
    return dict(pairs)


def verify(text: str, exit_status: int, expected_map: str = PRODUCTION) -> dict:
    if expected_map not in (PRODUCTION, TEST):
        raise ValueError('Unapproved expected map: ' + str(expected_map))
    if exit_status != 0:
        raise ValueError(f'Unreal process exit status {exit_status}')
    if FATAL.search(text):
        raise ValueError('Fatal/assert/ensure in Unreal process')
    marks = [tokens(line.split(TAG, 1)[1]) for line in text.splitlines() if TAG in line]
    if len(marks) != 8:
        raise ValueError(f'Expected BEGIN, 6 cases, SUMMARY; got {len(marks)} markers')
    begin = marks[0]
    if begin != {'status': 'BEGIN', 'map': expected_map, 'expected': '6'}:
        raise ValueError(f'BEGIN or loaded package mismatch: expected {expected_map}, got {begin}')
    for n, name in enumerate(STEPS, 1):
        entry = marks[n]
        if entry.get('status') != 'PASS' or entry.get('case') != name:
            raise ValueError(f'{name} missing/out of order/failed: {entry}')
        if name == 'AutonomousFollow':
            displacement = float(entry.get('displacement', 'nan'))
            submissions = int(entry.get('submissions', -1))
            if not (120.0 < displacement < 1e6 and submissions > 0):
                raise ValueError('Unproven real path-followed movement')
        if name == 'RealMeleeContact' and (
            entry.get('attackCount') != '1' or entry.get('provenance') != 'DirectContact'
        ):
            raise ValueError('Invalid physical melee-contact evidence')
    if marks[-1] != {'status': 'SUMMARY', 'passed': '6', 'expected': '6', 'result': 'PASS'}:
        raise ValueError('Invalid playable SUMMARY')
    if 'Engine exit requested' not in text:
        raise ValueError('No orderly Unreal exit marker')
    return {
        'status': 'PASS', 'suite': 'playable', 'map': expected_map,
        'expected': 6, 'cases': list(STEPS),
        'displacement_cm': float(marks[4]['displacement']),
        'move_submissions': int(marks[4]['submissions']),
    }


def main():
    if len(sys.argv) != 5:
        raise ValueError('usage: verify_playable_smoke.py GAME_LOG EXIT_STATUS RESULTS_JSON EXACT_MAP')
    result = verify(Path(sys.argv[1]).read_text(encoding='utf-8', errors='replace'),
                    int(sys.argv[2]), sys.argv[4])
    Path(sys.argv[3]).write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    print(f"PASS: playable map={result['map']} six cases, path displacement={result['displacement_cm']:.1f}cm")


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError) as exc:
        print('FAIL: ' + str(exc), file=sys.stderr)
        sys.exit(1)
