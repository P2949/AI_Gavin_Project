#!/usr/bin/env python3
"""Independent, fail-closed verifier for saved-world Pedro component interactions."""
import argparse
import json
import re
from pathlib import Path

CASES = (
    'Configuration', 'FailedDispatch', 'HiddenWorldTruthNotKnowledge',
    'NoReplay', 'VisibleDirectObservation', 'RepeatNoExtraCore',
)
BEGIN = re.compile(r'PEDRO_INTERACTION_TEST status=BEGIN map=(\S+) expected=(\d+) core=(\d+) required=(\d+) props=(\d+)')
PASS = re.compile(r'PEDRO_INTERACTION_TEST status=PASS case=(\w+)(?:\s|$)')
SUMMARY = re.compile(r'PEDRO_INTERACTION_TEST status=SUMMARY passed=(\d+) expected=(\d+) result=(PASS|FAIL)')
FATAL = re.compile(r'Fatal error:|Assertion failed:|Ensure condition failed:|Unhandled Exception:|Signal 11 caught|Segmentation fault', re.I)

class InvalidReport(ValueError):
    pass

def require(ok, why):
    if not ok:
        raise InvalidReport(why)


def verify(log: str, exit_status: int, expected_map: str):
    require(exit_status == 0, f'process exit {exit_status}')
    require(expected_map in (
        '/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype',
        '/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest',
    ), 'unapproved expected map')
    require(not FATAL.search(log), 'fatal, assert, or ensure in process log')
    require('PEDRO_INTERACTION_TEST status=FAIL' not in log, 'game-process failure marker')
    require(re.search(r'Engine exit requested', log), 'missing engine process exit')
    begins = list(BEGIN.finditer(log))
    passes = list(PASS.finditer(log))
    summaries = list(SUMMARY.finditer(log))
    require(len(begins) == len(summaries) == 1, 'missing or duplicate begin/summary')
    b, s = begins[0], summaries[0]
    require(b.group(1) == expected_map, 'wrong loaded map')
    require(tuple(map(int, b.groups()[1:4])) == (6, 4, 3), 'invalid 6/4/3 protocol')
    require(int(b.group(5)) >= 3, 'insufficient real core interactables')
    require((int(s.group(1)), int(s.group(2)), s.group(3)) == (6, 6, 'PASS'), 'wrong summary')
    require([p.group(1) for p in passes] == list(CASES), 'incorrect case inventory/order')
    require(b.start() < passes[0].start() < passes[-1].start() < s.start(), 'invalid event order')
    require(len(re.findall(r'PEDRO_INTERACTION_SETUP target=', log)) == 2,
            'expected exactly two grounded scenario setups')
    hidden = re.search(r'PEDRO_INTERACTION_TEST status=PASS case=HiddenWorldTruthNotKnowledge inspected=(\d+) evidence=(\d+)',log)
    visible = re.search(r'PEDRO_INTERACTION_TEST status=PASS case=VisibleDirectObservation evidence=(\d+) core=(\d+) event=([0-9A-Fa-f\-]+)',log)
    repeat = re.search(r'PEDRO_INTERACTION_TEST status=PASS case=RepeatNoExtraCore core=(\d+) evidence=(\d+)',log)
    require(hidden is not None and int(hidden.group(1)) >= 1 and int(hidden.group(2)) == 0, 'hidden truth/knowledge discrepancy')
    require(visible is not None and visible.group(1,2) == ('1','1') and len(visible.group(3)) >= 32, 'missing real direct evidence GUID')
    require(repeat is not None and repeat.group(1) == '1' and int(repeat.group(2)) in (1,2), 'repetition overcounts core or evidence')
    return {'status':'PASS','map':expected_map,'cases':list(CASES),
            'world_truth_vs_observer_knowledge':'verified in component-level game fixture',
            'core_progress_after_visible':1,'full_encounter_completion':'UNVERIFIED',
            'hub_travel':'UNVERIFIED','mapped_player_input':'UNVERIFIED'}


def main():
    p=argparse.ArgumentParser()
    p.add_argument('log',type=Path)
    p.add_argument('exit_status',type=int)
    p.add_argument('expected_map')
    p.add_argument('output',type=Path)
    a=p.parse_args()
    try:
        result=verify(a.log.read_text(encoding='utf-8',errors='replace'),a.exit_status,a.expected_map)
    except (InvalidReport, OSError) as exc:
        print(json.dumps({'status':'FAIL','reason':str(exc)}))
        raise SystemExit(1)
    a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))

if __name__=='__main__':
    main()
