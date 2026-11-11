#!/usr/bin/env python3
"""Verify exactly eight isolated PIE cases for physical Pedro observer.

Run: python3 verify_physical_observer_world_log.py /path/to/editor.log
Read only; does not substitute for running a real UE PIE world.
"""
import argparse
import re
from pathlib import Path

PREFIX = 'PEDRO_PHYSICAL_WORLD_TEST'
EXPECTED = {
    'Visible': ('Visible', 1),
    'TurnAway': ('OutsideViewCone', 0),
    'TurnBack': ('Visible', 1),
    'MoveOutOfRange': ('TooFar', 0),
    'MoveIntoRange': ('Visible', 1),
    'CoverBlocks': ('Occluded', 0),
    # First event was unseen; after turning, ONLY a new event is acquired.
    'NoRetroactive': ('OutsideViewCone', 1),
    'EyeFollowsPose': ('Visible', 1),
}


def check(condition: bool, description: str) -> None:
    if not condition:
        raise AssertionError(f'FAIL: {description}')
    print(f'PASS: {description}')


def verify(log: str) -> None:
    begin = f'{PREFIX} status=BEGIN expected=8 mode=PIE'
    beginning = log.rfind(begin)
    check(beginning != -1, 'last physical-observer PIE fixture began')
    section = log[beginning:]
    check(section.count(begin) == 1, 'one fixture run in selected log section')
    check('status=FAIL' not in section, 'no fixture FAIL marker')
    check('Fatal error:' not in section and 'Assertion failed:' not in section,
          'no fatal assertion after fixture began')

    passes = re.findall(
        re.escape(PREFIX) + r' status=PASS case=(\w+)', section)
    check(len(passes) == 8 and set(passes) == set(EXPECTED),
          'eight named cases passed exactly once')
    details = re.findall(
        re.escape(PREFIX) + r' case=(\w+) expectedTrace=(\w+) '
        r'actualTrace=(\w+) acquired=(\d+) expectedAcquired=(\d+) '
        r'eyeMoved=(\w+) valid=(\w+)', section)
    check(len(details) == 8 and len({d[0] for d in details}) == 8,
          'eight unique detailed world-check records')
    for case, expected_status, actual, acquired, expected_acquired, eye_moved, valid in details:
        check(case in EXPECTED and (expected_status, int(expected_acquired)) == EXPECTED[case]
              and actual == expected_status and int(acquired) == int(expected_acquired)
              and eye_moved == 'yes' and valid == 'yes',
              f'{case}: correct trace, acquisition, eye transform and validity')
    check(section.count(f'{PREFIX} status=SUMMARY passed=8 failed=0 total=8') == 1,
          'exactly one successful 8/8 world-backed summary')
    summary = section.find(f'{PREFIX} status=SUMMARY passed=8 failed=0 total=8')
    check('Shutting down PIE online subsystems' in section[summary:],
          'PIE was stopped cleanly after harness completed')
    check('RequiresUnsavedUntitledPIE' not in section,
          'unsaved Untitled-world safety guard not triggered')
    print('\nPASS: B2b-5b1 physical observer WORLD PIE 8/8 verified')
    print('NOTE: This fixture is native world-backed validation, not player-driven gameplay.')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('log', type=Path)
    args = ap.parse_args()
    check(args.log.is_file() and args.log.stat().st_size > 0,
          'Editor log exists and is nonempty')
    verify(args.log.read_text(encoding='utf-8-sig', errors='replace'))


if __name__ == '__main__':
    main()
