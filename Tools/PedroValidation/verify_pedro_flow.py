#!/usr/bin/env python3
"""B2b-6f: strict independent log verification, including world-process exit."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re

PRODUCTION = ('/Game/AI_Gavin/World/Hub/Maps/L_Hub_Prototype',
              '/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype')
ISOLATED = ('/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Hub_Pedro_AutonomousTest',
            '/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest')
CASES = {
    'completion': ['HubEntry', 'PedroConfiguration', 'Evidence01',
                   'RepeatNoExtraCore', 'Evidence02', 'NoPrematureCompletion',
                   'Evidence03', 'ExactlyOneResolution', 'Satisfied',
                   'ReturnToHub', 'HubProgressPersisted', 'CompletedReentry'],
    'early': ['HubEntry', 'PedroConfiguration', 'EarlyReturn',
              'HubUnresolved', 'FreshReentry'],
}
GUID = re.compile(r'[0-9a-fA-F]{32}\Z')
TAG = re.compile(r'PEDRO_FLOW_TEST status=(BEGIN|PASS|SUMMARY|FAIL)\b([^\r\n]*)')
FATAL = re.compile(r'Fatal error:|Assertion failed:|Ensure condition failed:|Unhandled Exception:|Signal 11 caught|Segmentation fault',re.I)
OUTCOME = re.compile(r'PEDRO_OUTCOME policy=EvidenceDriven interpretation=(Gentle|Seeking|Disruptive) sampledCore=3 requiredCore=3 resolved=yes actualLOS=checked')

class FlowVerificationError(ValueError): pass

def require(ok, reason):
    if not ok: raise FlowVerificationError(reason)

def fields(tail):
    out={}
    for token in tail.strip().split():
        require('=' in token, 'invalid flow marker: ' + token)
        key, val = token.split('=',1)
        require(bool(key and val) and key not in out, 'duplicate/empty flow field: '+token)
        out[key]=val
    return out

def verify(log: str, rc: int, scenario: str, hub: str, pedro: str) -> dict:
    require(scenario in CASES, 'unapproved scenario')
    require((hub,pedro) in (PRODUCTION, ISOLATED), 'unapproved hub/Pedro pair')
    require(rc==0, f'failed Unreal executable exit={rc}')
    require(not FATAL.search(log), 'fatal/assertion/ensure in Unreal process')
    matches=[(m.start(),m.group(1),fields(m.group(2))) for m in TAG.finditer(log)]
    # Report the real fixture failure rather than masking it with the
    # secondary 'missing/extra flow markers' envelope error.
    failures=[row for _,kind,row in matches if kind=='FAIL']
    if failures:
        detail=re.findall(r'PEDRO_FLOW_DIAG\b[^\r\n]*',log)
        fail=failures[0]
        reason=fail.get('reason','Unknown')
        phase=fail.get('phase','Unknown')
        diagnostic=detail[-1] if detail else 'none'
        raise FlowVerificationError(
            f'game process FAIL scenario={fail.get("scenario")} reason={reason} '
            f'phase={phase}; last_diagnostic={diagnostic}')
    require(len(matches)==len(CASES[scenario])+2, 'missing/extra flow markers')
    require(matches[0][1]=='BEGIN' and matches[-1][1]=='SUMMARY', 'invalid envelope')
    require(all(kind=='PASS' for _,kind,_ in matches[1:-1]), 'failing or unexpected marker')
    require(matches[0][2]=={'scenario':scenario,'hub':hub,'pedro':pedro,
                            'expected':str(len(CASES[scenario]))}, 'BEGIN mismatch')
    require(matches[-1][2]=={'scenario':scenario,'passed':str(len(CASES[scenario])),
                             'expected':str(len(CASES[scenario])),'result':'PASS'}, 'SUMMARY mismatch')
    body=[row for _,_,row in matches[1:-1]]
    require([row.get('case') for row in body]==CASES[scenario], 'case order/inventory mismatch')
    require(all(row.get('scenario')==scenario for row in body), 'mixed scenarios')
    require(body[0].get('travel')=='RealPlayerInteraction', 'no real hub entry interaction')
    require(body[1].get('policy')=='EvidenceDriven' and
            body[1].get('core')=='4' and body[1].get('required')=='3', 'invalid real opportunity gate')
    require(re.search(r'Engine exit requested',log[matches[-1][0]:]) is not None,
            'no engine exit after terminal PASS')
    if scenario=='early':
        require(body[2].get('travel')=='RealOverlap','not an actual return-volume overlap')
        require(body[3].get('sessionCount')=='0' and body[3].get('callbacks')=='0',
                'early exit incorrectly resolved progress')
        require(all(body[4].get(k)==v for k,v in
                    {'localEvidence':'0','sessionCount':'0','callbacks':'0','active':'yes'}.items()),
                'early reentry not fresh/active')
        require('PEDRO_OUTCOME policy=EvidenceDriven' not in log,'unexpected early outcome')
        return {'status':'PASS','scenario':scenario,'map_pair':[hub,pedro],
                'cases':CASES[scenario], 'early_exit_and_fresh_reentry':True,
                'completion':'NOT_RUN_IN_THIS_SCENARIO'}
    events=[]
    for index, required_core, required_history in [(2,1,1),(3,1,2),(4,2,3),(6,3,4)]:
        row=body[index]
        require(set(row)=={'scenario','case','opportunity','event','core','evidence','provenance'},
                'missing/extraneous evidence fields')
        require(GUID.fullmatch(row['event']) is not None, 'bad evidence GUID')
        require(row['provenance']=='DirectObservation' and
                int(row['core'])==required_core and int(row['evidence'])==required_history,
                'evidence/core count mismatch')
        events.append((row['opportunity'],row['event'].lower()))
    require(len({event for _,event in events})==4, 'duplicate evidence occurrence GUID')
    require(events[0][0]==events[1][0], 'repeat was not on the first source')
    require(len({events[0][0],events[2][0],events[3][0]})==3,
            'third completion was not three distinct core responses')
    require(body[5].get('after_two_distinct')=='yes' and body[5].get('progress')=='0',
            'premature outcome not disproved')
    resolution=body[7]
    require(resolution.get('callbacks')=='1' and resolution.get('sessionCount')=='1',
            'not exactly one resolution broadcast and session ID')
    require(resolution.get('classification') in ('0','1','2'), 'unrecognized classification enum')
    require(body[8].get('ai_mode')=='Satisfied','observer not Satisfied')
    require(body[9].get('travel')=='RealOverlap','return did not come through saved overlap volume')
    require(body[10].get('sessionCount')=='1' and body[10].get('callbacks')=='1',
            'progress did not survive world travel')
    require(all(body[11].get(k)==v for k,v in
                {'localEvidence':'0','sessionCount':'1','callbacks':'1','active':'no'}.items()),
            'completed reentry not inactive/session-preserving')
    outcomes=list(OUTCOME.finditer(log))
    require(len(outcomes)==1, 'expected exactly one real Director outcome log')
    expected={'Gentle':'0','Seeking':'1','Disruptive':'2'}[outcomes[0].group(1)]
    require(resolution['classification']==expected,'classification mismatches Director outcome')
    require(log.count('PEDRO_DIRECT acquired=yes')>=4,
            'no proof of Director accepting four real direct observations (including repeat)')
    return {'status':'PASS','scenario':scenario,'map_pair':[hub,pedro],
            'cases':CASES[scenario], 'real_distinct_core_count':3,
            'direct_evidence_records':4, 'observed_outcome':outcomes[0].group(1),
            'progress_notifications':1,'hub_travel_and_completed_reentry':True,
            'physical_mapped_input':'NOT_VERIFIED','shipping':'NOT_VERIFIED'}

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('log',type=Path)
    a.add_argument('exit_status',type=int)
    a.add_argument('scenario', choices=sorted(CASES))
    a.add_argument('hub')
    a.add_argument('pedro')
    a.add_argument('result',type=Path)
    ns=a.parse_args()
    try:
        output=verify(ns.log.read_text(encoding='utf-8',errors='replace'),ns.exit_status,
                      ns.scenario,ns.hub,ns.pedro)
    except (FlowVerificationError,OSError,ValueError) as exc:
        print(json.dumps({'status':'FAIL','reason':str(exc)}))
        raise SystemExit(1)
    ns.result.write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(output,indent=2))

if __name__=='__main__':main()
