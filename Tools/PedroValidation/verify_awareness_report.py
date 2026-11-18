#!/usr/bin/env python3
"""Independently validate the native JSON and runtime log protocol."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys

PREFIX='AI_Gavin_Project.Pedro.'
# Exact 67-name merged B2b-5b2 baseline, not just a total.
OLD_GROUPS={
 'ActivePresentation':'Disruptive DuplicateActor GateNotReady Gentle MissingActor Seeking',
 'Classification':'Disqualifiers Disruptive Duplicate Empty Gentle MixedAndReset Provenance RepetitionCap Seeking Ties UnownedProperty',
 'CompletionGate':'AcquisitionLast AllFourCore BelowThreshold BonusDoesNotCount BonusOnly ClassificationForwarding Empty EvidenceOnly ExactThreshold ExposureLast ExposureOnly FractionRounding InconsistentSample NoEvidence NonFiniteFraction Provenance RepetitionAndDuplicate Reset',
 'DirectObservation.Geometry':'Angle Boundary FullCircle InvalidSettings NonFinite Range Visible ZeroForward',
 'Director':'AcquisitionLifecycle ConfirmedEvent',
 'DirectorActive':'IgnoresLegacyClicks InsufficientEvidence LegacyDefault MissingWorldFailsClosed',
 'DirectorShadow':'AcquisitionRequired Disabled DuplicatesAndPostOutcome EvidenceThenExposure ExposureThenEvidence LegacyRemainsAuthoritative',
 'Evidence':'Acceptance Meaning Repetition Reset',
 'ObserverCharacter':'EyeComponent EyeViewPoint',
 'Opportunity':'Definitions DirectorIntegration EvidenceGate Sampling',
 'Source':'Configuration Emission',
}
NEW='InitialState FirstSighting RefreshSighting LoseSight IgnoreFailedStimulusLocation Reacquire LossBeforeAcquisition InvalidLocation Reset InvalidTarget ControllerConfiguration NonPlayerFiltering MissingObserver DefaultCompatibility'.split()
NATIVE_NAMES={PREFIX+group+'.'+name for group,names in OLD_GROUPS.items() for name in names.split()} | {PREFIX+'Awareness.'+name for name in NEW}
assert len(NATIVE_NAMES)==81
MAP='/Game/AI_Gavin/Encounters/Pedro/Validation/L_Pedro_ObserverActorTest'
# Protocol sequence: ID (or - for substep), step, awareness, accepted, sampled.
# Asterisk is deliberately unconstrained only for I04: event LOS is independent
# of whether the player itself remains visible while the blocker is present.
WORLD='''W01 InitiallyVisible Perceived 0 0
W02 InitiallyBehind Unaware 0 0
W03 InitiallyOutsideRange Unaware 0 0
W04 InitiallyBehindCover Unaware 0 0
- ClearAfterInitialCover Perceived 0 0
W05 VisiblePlayerMovement Perceived 0 0
W06 ObserverTurnsAway LostSight 0 0
W07 ObserverTurnsBack Perceived 0 0
- LostBeforeEnterRange LostSight 0 0
W08 PlayerEntersRange Perceived 0 0
- RetainInHysteresisMargin Perceived 0 0
- LoseBeyondHysteresis LostSight 0 0
- NoReacquireInMargin LostSight 0 0
W09 ReacquireInsideRadius Perceived 0 0
W10 WallBlocksSight LostSight 0 0
W11 OccludedPlayerMovement LostSight 0 0
W12 WallRemoved Perceived 0 0
- EyeOnlyTurnsAway LostSight 0 0
W13 EyeOffsetAndFacingRestored Perceived 0 0
W14 DistractorIgnored Unaware 0 0
- VisibleBeforeDestroy Perceived 0 0
W15 PlayerDestroyed Unaware 0 0
- ReplacementInitiallyHidden Unaware 0 0
W16 ReplacementNeedsOwnSighting Perceived 0 0
W17 UnpossessClears Unaware 0 0
- RepossessWhileHidden Unaware 0 0
W18 RepossessedFreshSighting Perceived 0 0
W19 IndependentObservers Perceived 0 0
W20 ControllerDestroyed Unaware 0 0
W21 AwarenessOnlyNoEvidence Perceived 0 0
W22 VisiblePlayerOccludedEvent Perceived 0 0
- PlayerHiddenEventClear LostSight 0 0
W23 UnseenPlayerVisibleEvent LostSight 1 1
- DemoDetect Perceived 0 0
- DemoVisibleAction Perceived 1 1
- DemoLoseSight LostSight 1 1
- DemoHiddenAction LostSight 1 1
W24 DemoReacquireNoRetroactive Perceived 1 1
W25 DemoNewActionAcquired Perceived 2 2'''
INTEGRATION='''I01 SavedConfiguration Unaware 0 0
I02 RuntimeAwarenessOptIn Perceived 0 0
I03 RealStatueInspection Perceived 1 1
- PrepareRadio Perceived 1 1
I04 RealRadioOccluded * 1 1
I05 RestoreSightNoRetroactive Perceived 1 1
- RadioVisibleNewAction Perceived 2 2
- PrepareLantern Perceived 2 2
I06 LanternSeekingCompletion * 3 3'''
PLANS={s:[(c,n,a,int(e),int(p)) for c,n,a,e,p in (l.split() for l in plan.splitlines())] for s,plan in [('world',WORLD),('integration',INTEGRATION)]}
COUNTS={'world':25,'integration':6}
# Event must be logged immediately before the associated step begins. Explicit
# sequence and provenance prevent a missing hidden action from passing.
EVENTS={
 'world': [('VisiblePlayerOccludedEvent','AwarenessPropA','Occluded',1),('UnseenPlayerVisibleEvent','AwarenessPropA','Visible',2),('DemoVisibleAction','AwarenessPropA','Visible',1),('DemoHiddenAction','AwarenessPropB','Occluded',1),('DemoNewActionAcquired','AwarenessPropB','Visible',2)],
 'integration':[('RealStatueInspection','Inspect_Statue','Visible',1),('RealRadioOccluded','Inspect_Radio','Occluded',1),('RadioVisibleNewAction','Inspect_Radio','Visible',2),('LanternSeekingCompletion','Inspect_Lantern','Visible',1)],
}
TAG='PEDRO_AWARENESS_TEST '
FATAL=re.compile(r'Fatal error:|Assertion failed:|Ensure condition failed:|Unhandled Exception:|Signal 11 caught|Segmentation fault',re.I)

def need(condition,msg):
    if not condition: raise ValueError(msg)

def native(report,exit_status=0):
    tests=report['tests']; names=[t['fullTestPath'] for t in tests]
    need(len(names)==81 and len(set(names))==81,'expected exactly 81 unique native tests')
    need(set(names)==NATIVE_NAMES,f'native inventory differs: missing={sorted(NATIVE_NAMES-set(names))} extra={sorted(set(names)-NATIVE_NAMES)}')
    for t in tests:
        need(t['state']=='Success' and t['warnings']==0 and t['errors']==0, f'native failure: {t["fullTestPath"]}')
    need(report['succeeded']==81,'incorrect success total')
    for field in ['succeededWithWarnings','failed','notRun','inProcess']:
        need(report[field]==0,f'{field} nonzero')
    need(exit_status==0,f'all 81 native tests passed, but native process exit {exit_status}')
    return {'status':'PASS','baseline':67,'new':14,'total':81,'tests':sorted(names)}

def native_diagnostics(report, exit_status):
    """Report test failures separately from process failures; never grants acceptance."""
    totals={k:report.get(k) for k in ('succeeded','succeededWithWarnings','failed','notRun','inProcess')}
    lines=[f'Native process exit: {exit_status}', 'Native report totals: '+json.dumps(totals,sort_keys=True)]
    for test in report.get('tests',[]):
        if test.get('state')!='Success' or test.get('warnings')!=0 or test.get('errors')!=0:
            lines.append('Native test diagnostic: '+json.dumps(test,sort_keys=True))
    return '\n'.join(lines)

def parse_fields(line):
    d={}
    for token in line.split(TAG,1)[1].strip().split():
        need('=' in token,f'malformed marker token {token}')
        key,value=token.split('=',1)
        need(key and value and key not in d,f'invalid or duplicate marker field {key}')
        d[key]=value
    return d

def vec(value):
    v=tuple(map(float,value.split(',')))
    need(len(v)==3 and all(math.isfinite(x) for x in v),f'invalid vector {value}')
    return v

def ids(value):
    if value=='-': return []
    v=value.lower().split(',')
    need(all(re.fullmatch(r'[0-9a-f]{32}',x) and x!='0'*32 for x in v) and len(v)==len(set(v)),f'invalid IDs: {value}')
    return v

def pos_equal(a,b,tol=1.01): return math.dist(a,b)<=tol

def verify_runtime(text,suite,exit_status=0):
    need(exit_status==0,f'game process exit {exit_status}')
    need(suite in PLANS,'unknown suite')
    need(not FATAL.search(text),'fatal/assert/ensure detected anywhere in log')
    lines=text.splitlines()
    marks=[(i,parse_fields(l)) for i,l in enumerate(lines) if TAG in l]
    need(marks and marks[0][1].get('status')=='BEGIN','missing initial BEGIN')
    need(marks[-1][1].get('status')=='SUMMARY','missing terminal SUMMARY')
    count=COUNTS[suite]
    need(marks[0][1]=={'status':'BEGIN','suite':suite,'map':MAP,'expected':str(count)},'BEGIN mismatch')
    need(marks[-1][1]=={'status':'SUMMARY','suite':suite,'passed':str(count),'expected':str(count),'result':'PASS'},'SUMMARY mismatch')
    need(any('Engine exit requested' in l for l in lines[marks[-1][0]+1:]),'no clean engine exit request after summary')
    event_list=[]; emitted=[]; acquired=[]; seen_ids=set(); snapshots={}; completed=[]
    pending=None; start=None; idx=0; resets=[]
    for _,m in marks[1:-1]:
        status=m.get('status')
        need(idx<len(PLANS[suite]),'unexpected marker after final step')
        case,step,awareness,expected_evidence,expected_sampled=PLANS[suite][idx]
        if status=='RESET_EVIDENCE':
            need(suite=='world' and step in ('AwarenessOnlyNoEvidence','DemoDetect') and start is None and pending is None and step not in resets,'unexpected evidence reset')
            need(m=={'status':'RESET_EVIDENCE'},'invalid RESET marker')
            emitted=[]; acquired=[]; resets.append(step); continue
        if status=='EVENT':
            need(start is None and pending is None,'unexpected or duplicate event')
            need(len(event_list)<len(EVENTS[suite]),'extra event')
            step_expected,op,trace,inspection=EVENTS[suite][len(event_list)]
            need(step==step_expected and m['opportunity']==op and m['expected']==trace and int(m['inspection'])==inspection,'event sequence mismatch')
            need(set(m)=={'status','id','subject','opportunity','expected','inspection'},'unexpected EVENT fields')
            event_ids=ids(m['id']);need(len(event_ids)==1,'EVENT requires exactly one ID');event_id=event_ids[0]
            need(event_id not in seen_ids,'duplicate event ID across resets')
            seen_ids.add(event_id); emitted.append(event_id)
            if trace=='Visible': acquired.append(event_id)
            event_list.append({'step':step,'id':event_id,'opportunity':op,'trace':trace,'subject':m['subject'],'inspection':inspection})
            pending=step; continue
        need(status in ('STEP_BEGIN','PASS','CHECK'),f'unexpected status {status}')
        need(set(m)==set('status case step awareness native has known actual time running target player evidence sampled emitted acquired final progress nativeLoc nativeAge eye forward other otherNative distractorNative destroyedTimerActive'.split()),'missing or extra snapshot fields')
        need(m.get('case')==case and m.get('step')==step,'step or case order mismatch')
        if status=='STEP_BEGIN':
            need(start is None,'duplicate STEP_BEGIN')
            must_emit=any(e[0]==step for e in EVENTS[suite])
            need((pending==step)==must_emit,f'missing or unexpected event at {step}')
            pending=None; start=m; continue
        need(start is not None,'step finished without setup record')
        need(status==('CHECK' if case=='-' else 'PASS'),'wrong terminal step marker')
        need(m['awareness'] in ('Unaware','Perceived','LostSight') and ((awareness=='*' and m['awareness']!='Unaware') or m['awareness']==awareness),f'awareness mismatch: {step}')
        for field in ('native','has','running','final','otherNative','distractorNative','destroyedTimerActive'):
            need(m[field] in ('yes','no'),f'invalid bool {field}')
        known=vec(m['known']); actual=vec(m['actual']); native_loc=vec(m['nativeLoc']); eye=vec(m['eye']); forward=vec(m['forward'])
        timestamp=float(m['time']); age=float(m['nativeAge'])
        need(math.isfinite(timestamp) and timestamp>=0 and math.isfinite(age),'invalid time/age')
        if m['native']=='yes': need(0<=age<=5.01,'expired raw sight')
        else: need(age==-1 and native_loc==(0,0,0),'unexpected raw sight data')
        if m['awareness']=='Perceived':
            need(m['native']==m['has']=='yes' and m['target']==m['player'] and m['player']!='None' and pos_equal(known,actual),'inconsistent perception or spatial sample')
        if m['awareness']=='Unaware':
            need(m['has']=='no' and known==(0,0,0) and timestamp==0 and m['target']=='None','unaware contains spatial knowledge')
            if awareness!='*': need(m['native']=='no','expected no native sight')
        if m['awareness']=='LostSight':
            need(m['has']=='yes' and m['target']==m['player'] and timestamp>0,'loss erased memory')
            if awareness!='*': need(m['native']=='no','native perception has not lost sight')
        need(m['running']==('no' if step in ('UnpossessClears','ControllerDestroyed','SavedConfiguration') else 'yes'),'refresh lifecycle mismatch')
        need(ids(m['emitted'])==emitted and ids(m['acquired'])==acquired,'event identity/provenance mismatch')
        need(int(m['evidence'])==len(acquired)==expected_evidence and int(m['sampled'])==expected_sampled,'evidence or distinct sample mismatch')
        final=step=='LanternSeekingCompletion'
        need(m['final']==('yes' if final else 'no') and int(m['progress'])==int(final),'premature or missing progression')
        # Verify frozen successful position AND timestamp across losses, not only
        # the controller's label or its own PASS marker.
        frozen_from={'ObserverTurnsAway':'VisiblePlayerMovement','LostBeforeEnterRange':'ObserverTurnsBack','LoseBeyondHysteresis':'RetainInHysteresisMargin',
            'NoReacquireInMargin':'LoseBeyondHysteresis','WallBlocksSight':'ReacquireInsideRadius','OccludedPlayerMovement':'WallBlocksSight',
            'EyeOnlyTurnsAway':'WallRemoved','PlayerHiddenEventClear':'VisiblePlayerOccludedEvent','UnseenPlayerVisibleEvent':'PlayerHiddenEventClear',
            'DemoLoseSight':'DemoVisibleAction','DemoHiddenAction':'DemoLoseSight'}
        if step in frozen_from:
            prev=snapshots[frozen_from[step]]
            # On the first loss step, a valid sight refresh can occur between
            # the preceding PASS and this step's setup. Compare timestamp to
            # the actual setup sample, then enforce freezing across later loss.
            continuing=step in ('NoReacquireInMargin','OccludedPlayerMovement','UnseenPlayerVisibleEvent','DemoHiddenAction')
            expected_time=prev['time'] if continuing else float(start['time'])
            need(known==tuple(prev['known']) and timestamp==expected_time,f'last-known position or timestamp leaked: {step}')
        if step in ('WallBlocksSight','OccludedPlayerMovement','DemoHiddenAction'):
            need(not pos_equal(known,actual,20),'hidden target never actually moved')
        if step=='VisiblePlayerMovement': need(not pos_equal(known,tuple(snapshots['ClearAfterInitialCover']['known']),20),'visible target never moved')
        if step=='DistractorIgnored': need(m['distractorNative']=='yes','distractor never perceived by native sight')
        if step=='ReplacementNeedsOwnSighting': need(m['target']!=snapshots['VisibleBeforeDestroy']['target'],'replacement reused original identity')
        if step=='IndependentObservers': need(m['other']=='Unaware' and m['otherNative']=='no','second observer leaked knowledge')
        if step=='ControllerDestroyed': need(m['destroyedTimerActive']=='no' and m['other']=='Unaware','controller timer or second observer invalid')
        if step=='EyeOnlyTurnsAway': need(forward[0]<-0.9,'eye-only rotation not applied')
        if step=='EyeOffsetAndFacingRestored':
            prev=snapshots['EyeOnlyTurnsAway']; need(forward[0]>0.9 and not pos_equal(eye,tuple(prev['eye']),20),'eye offset or rotation did not change')
        snapshots[step]={'case':case,'awareness':m['awareness'],'native':m['native'],'known':known,'actual':actual,'time':timestamp,'native_location':native_loc,'native_age':age,'eye':eye,'forward':forward,'target':m['target'],'player':m['player'],'emitted':list(emitted),'acquired':list(acquired),'evidence':int(m['evidence']),'sampled':int(m['sampled']),'final':final,'progress':int(m['progress']),'running':m['running']}
        if case!='-': completed.append(case)
        start=None; idx+=1
    need(idx==len(PLANS[suite]) and start is None and pending is None,'incomplete steps')
    need(len(completed)==len(set(completed))==count,'wrong named case count')
    need(len(event_list)==len(EVENTS[suite]),'missing real events')
    need(resets==(['AwarenessOnlyNoEvidence','DemoDetect'] if suite=='world' else []),'evidence reset sequence mismatch')
    direct=[l for l in lines if 'PEDRO_DIRECT acquired=yes' in l]
    need(len(direct)==sum(e['trace']=='Visible' for e in event_list) and all('provenance=DirectObservation' in l for l in direct),'Director acquisition log mismatch')
    statuses=[l for l in lines if 'PEDRO_DIRECT status=' in l]
    need(len(statuses)==len(event_list),'unexpected direct observation status count')
    for line,event in zip(statuses,event_list):
        need(f'status={event["trace"]} ' in line and f'opportunity={event["opportunity"]} ' in line and 'actualLOS=checked' in line,'Director LOS event mismatch')
    outcomes=[l for l in lines if 'PEDRO_OUTCOME policy=' in l]
    if suite=='world': need(not outcomes,'world tests unexpectedly completed encounter')
    else:
        need(len(outcomes)==1 and 'policy=EvidenceDriven interpretation=Seeking sampledCore=3 requiredCore=3 resolved=yes actualLOS=checked' in outcomes[0],'missing or duplicate Seeking outcome')
    return {'status':'PASS','suite':suite,'cases':completed,'expected':count,'steps':snapshots,'events':event_list}


UNITY_SOURCES = tuple('Source/AI_Gavin_Project/Encounters/Pedro/'+name for name in (
    'PedroObserverAIController.cpp', 'Tests/PedroAwarenessStateTests.cpp',
    'Tests/PedroObserverAIControllerTests.cpp', 'Tests/PedroAwarenessWorldTests.cpp'))
UNITY_NAME = r'Module\.AI_Gavin_Project(?:\.\d+)?\.cpp'

def verify_unity(text, project, exit_status):
    need(exit_status == 0, 'Unity build process failed')
    need('Result: Succeeded' in text, 'missing UBT success marker')
    command = [line for line in text.splitlines() if 'Running command :' in line]
    need(any(all(flag in line.split() for flag in ('-ForceUnity','-DisableAdaptiveUnity','-gather')) for line in command), 'missing forced Unity build arguments')
    need(not re.search(r'\[Adaptive Build\].*Excluded from AI_Gavin_Project unity file',text), 'project adaptive Unity exclusion detected')
    # Require completed UBT compile actions, not an arbitrary mention of the module.
    actions = re.findall(r'^\s*\[\d+/\d+\]\s+Compile\s+(?:\[[^\]\r\n]+\]\s+)?([^\r\n]+)',text,re.M)
    names=[Path(a.strip().split()[0].replace('\\','/')).name for a in actions]
    blobs=[n for n in names if re.fullmatch(UNITY_NAME,n)]
    need(blobs and len(blobs)==len(set(blobs)), 'missing or duplicate project Unity compile actions')
    need(not set(names)&{Path(p).name for p in UNITY_SOURCES}, 'new source compiled separately')
    project=Path(project).resolve()
    directory=project/'Intermediate/Build/Linux/x64/UnrealEditor/Development/AI_Gavin_Project'
    sources={str((project/name).resolve()):name for name in UNITY_SOURCES}
    owners={name:[] for name in UNITY_SOURCES}
    artifacts={}
    for blob in blobs:
        path=directory/blob
        data=path.read_bytes()
        includes=re.findall(r'^\s*#\s*include\s*"([^"\r\n]+)"',data.decode('utf-8-sig'),re.M)
        for inc in includes:
            candidate=Path(inc.replace('\\','/'))
            matches={sources[str(p.resolve())] for p in (path.parent/candidate, project/candidate) if str(p.resolve()) in sources}
            for name in matches: owners[name].append(blob)
        artifacts[blob]={'path':str(path.relative_to(project)), 'sha256':hashlib.sha256(data).hexdigest()}
    need(all(len(v)==1 for v in owners.values()),f'expected each new TU exactly once in compiled Unity blobs: {owners}')
    return {'status':'PASS','suite':'unity','expected':len(UNITY_SOURCES),'compiled_blobs':artifacts,'source_membership':{k:v[0] for k,v in owners.items()}}

def main():
    p=argparse.ArgumentParser(); p.add_argument('suite',choices=['native','world','integration','unity']);p.add_argument('input',type=Path);p.add_argument('exit_status',type=int);p.add_argument('output',type=Path)
    p.add_argument('--project',type=Path)
    a=p.parse_args()
    try:
        text=a.input.read_text(encoding='utf-8-sig',errors='strict')
        if a.suite=='native':
            report=json.loads(text)
            print(native_diagnostics(report,a.exit_status),flush=True)
            result=native(report,a.exit_status)
        elif a.suite=='unity':
            need(a.project is not None,'Unity verification requires --project')
            result=verify_unity(text,a.project,a.exit_status)
        else:
            result=verify_runtime(text,a.suite,a.exit_status)
        a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print(f'PASS: {a.suite}: {result.get("total",result.get("expected"))} verified')
        return 0
    except (ValueError,KeyError,TypeError,IndexError,OSError) as exc:
        print(f'FAIL: {a.suite}: {exc}',file=sys.stderr);return 1
if __name__=='__main__': sys.exit(main())
