#!/usr/bin/env python3
"""Fail-closed whitelist for newly authored Git-LFS binary assets."""
from __future__ import annotations
import json
from pathlib import Path
import sys

TEST_BASE = 'Content/AI_Gavin/Encounters/Pedro/Validation/B2b6e/'
TEST_NAMES = {
    TEST_BASE + 'L_Pedro_AutonomousTest.umap',
    TEST_BASE + 'L_Hub_Pedro_AutonomousTest.umap',
    TEST_BASE + 'DA_Encounter_Pedro_AutonomousTest.uasset',
}
TEST_OPTIONAL = {
    TEST_BASE + 'L_Pedro_AutonomousTest_BuiltData.uasset',
    TEST_BASE + 'L_Hub_Pedro_AutonomousTest_BuiltData.uasset',
}
PROD_NAMES = {
    'Content/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype.umap',
    'Content/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype_BuiltData.uasset',
}


def hashes(p: Path):
    rows = {}
    for row in p.read_text(encoding='utf-8').splitlines():
        digest, path = row.split('  ', 1)
        if path in rows:
            raise ValueError('Duplicate manifest path: ' + path)
        rows[path] = digest
    return rows


def check(mode, before, after, report=None):
    changed = sorted(p for p in set(before) | set(after) if before.get(p) != after.get(p))
    allow = ((TEST_NAMES | TEST_OPTIONAL) if mode in ('prepare-test', 'author-test', 'build-test-nav')
             else PROD_NAMES if mode in ('author-production', 'build-production-nav')
             else set())
    if mode == 'prepare-test':
        created = all(p not in before and p in after for p in TEST_NAMES)
        resumed = all(p in before and p in after for p in TEST_NAMES)
        if created:
            if report is not None and report.get('resumed_pristine_duplicates'):
                raise ValueError('New copies cannot claim a resumed run')
        elif resumed:
            if not (isinstance(report, dict) and report.get('mode') == 'prepare-test'
                    and report.get('status') == 'PASS'
                    and report.get('resumed_pristine_duplicates') is True):
                raise ValueError('Existing duplicate assets require proven pristine-resume report')
            if not TEST_NAMES.issubset(set(changed)):
                raise ValueError('Pristine resume did not update every test asset')
        else:
            raise ValueError('Expected either three new duplicates or verified resume of exactly three')
    unknown = [p for p in changed if p not in allow]
    if unknown:
        raise ValueError('Unexpected files changed outside Pedro allowlist: ' + repr(unknown))
    # A nav rebuild may leave a previously valid map byte-identical. The Editor
    # already required a real projected path. A fresh game-process Recast query
    # remains authoritative; do not reject a valid already-built nav map just
    # because Unreal had no dirty package to save.
    if not changed and mode not in ('build-test-nav', 'build-production-nav'):
        raise ValueError('Author operation produced no saved asset changes')
    return {'status': 'PASS', 'mode': mode, 'changed': changed,
            'navigation_unchanged': not changed and mode.startswith('build-')}


def main():
    if len(sys.argv) != 6:
        raise ValueError('usage: b2b6e_write_scope.py MODE BEFORE_SHA AFTER_SHA OUTPUT_JSON RESULT_JSON')
    mode, before, after, dest, report_path = sys.argv[1:]
    report = json.loads(Path(report_path).read_text(encoding='utf-8'))
    result = check(mode, hashes(Path(before)), hashes(Path(after)), report=report)
    Path(dest).write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('PASS: Pedro binary authoring allowlist: ' + ', '.join(result['changed']))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError) as e:
        print('STOP: ' + str(e), file=sys.stderr)
        sys.exit(1)
