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
ATTENTION_NAMES='Disabled IdleWithoutKnowledge TrackFreshSighting UpdateObservedTarget HoldLastKnown HoldExpiry StalePerceived Reacquire InvalidTarget InvalidLocation InvalidTime InvalidSettings TurnLeft TurnRight ShortestPath OppositeHeading DeadZone SpeedAndOvershoot InvalidDeltaTime DegenerateGeometry'.split()
NATIVE_NAMES |= {PREFIX+'Attention.'+name for name in ATTENTION_NAMES}
assert len(NATIVE_NAMES)==101
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
    need(len(names)==101 and len(set(names))==101,'expected exactly 101 unique native tests')
    need(set(names)==NATIVE_NAMES,f'native inventory differs: missing={sorted(NATIVE_NAMES-set(names))} extra={sorted(set(names)-NATIVE_NAMES)}')
    for t in tests:
        need(t['state']=='Success' and t['warnings']==0 and t['errors']==0, f'native failure: {t["fullTestPath"]}')
    need(report['succeeded']==101,'incorrect success total')
    for field in ['succeededWithWarnings','failed','notRun','inProcess']:
        need(report[field]==0,f'{field} nonzero')
    need(exit_status==0,f'all 101 native tests passed, but native process exit {exit_status}')
    return {'status':'PASS','baseline':81,'new':20,'total':101,'tests':sorted(names)}

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
    'Tests/PedroObserverAIControllerTests.cpp', 'Tests/PedroAwarenessWorldTests.cpp',
    'PedroAttentionPolicy.cpp','Tests/PedroAttentionTests.cpp'))
UNITY_NAME = r'Module\.AI_Gavin_Project(?:\.\d+)?\.cpp'

def verify_unity(text, project, exit_status):
    need(exit_status == 0, 'Unity build process failed')
    need('Result: Succeeded' in text, 'missing UBT success marker')
    need(project_linked(text),'missing project linker action')
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

# Each completed case and every measured frame is independently checked. The
# AW marker is retained for common BEGIN/EVENT/RESET/SUMMARY records.
ATT_TAG='PEDRO_ATTENTION_TEST '
ATT_WORLD='''A01 DefaultDisabled Disabled Perceived 0 0
A02 EnableWithoutSight Idle Unaware 0 0
A03 InitiallyBehind Idle Unaware 0 0
A04 TurnRight TrackObserved Perceived 0 0
A05 TurnLeft TrackObserved Perceived 0 0
A06 StationaryConverges TrackObserved Perceived 0 0
A07 VisibleMovingPlayer TrackObserved Perceived 0 0
A08 BoundedAngularSpeed TrackObserved Perceived 0 0
A09 WrapShortestPath TrackObserved Perceived 0 0
A10 OffsetEye TrackObserved Perceived 0 0
A11 StationaryBody TrackObserved Perceived 0 0
- BeforeHoldWhileTurning TrackObserved Perceived 0 0
A12 HoldOnLoss HoldLastKnown LostSight 0 0
A13 HiddenLateralMovement HoldLastKnown LostSight 0 0
A14 AttentionExpires Idle LostSight 0 0
A15 IdleReturn Idle LostSight 0 0
- BeforeEarlyLoss TrackObserved Perceived 0 0
- EarlyLoss HoldLastKnown LostSight 0 0
A16 EarlyReacquire TrackObserved Perceived 0 0
- LateLoss Idle LostSight 0 0
A17 LateReacquire TrackObserved Perceived 0 0
- BeginMovingBeforeDisable TrackObserved Perceived 0 0
A18 DisableMidTurn Disabled Perceived 0 0
A19 ReenableFreshAnchor TrackObserved Perceived 0 0
- RepeatedEnableKeepsAnchor TrackObserved Perceived 0 0
A20 DestroyPlayer Idle Unaware 0 0
- ReplaceHidden Idle Unaware 0 0
- ReplaceVisible TrackObserved Perceived 0 0
A21 UnpossessClearsAttention Disabled Unaware 0 0
- RepossessFreshAnchor Idle Unaware 0 0
- RepossessNewSight TrackObserved Perceived 0 0
- DestroyAttentionController Disabled Unaware 0 0
A22 IndependentTurning TrackObserved Perceived 0 0
- DemoSetup TrackObserved Perceived 0 0
- DemoVisibleAction TrackObserved Perceived 1 1
- DemoTurnAwayFromEvent TrackObserved Perceived 1 1
A23 OutsideConeAction TrackObserved Perceived 1 1
- DemoHold HoldLastKnown LostSight 1 1
- DemoHiddenAction HoldLastKnown LostSight 1 1
- DemoIdle Idle LostSight 1 1
- DemoReacquire TrackObserved Perceived 1 1
A24 DemoNewAction TrackObserved Perceived 2 2'''
ATT_INTEGRATION='''J01 SavedConfiguration Disabled Unaware 0 0
J02 AutonomousSavedTurn TrackObserved Perceived 0 0
J03 SavedStatue TrackObserved Perceived 1 1
J04 SavedRadioOccluded * * 1 1
- SavedRestoreNoRetroactive TrackObserved Perceived 1 1
J05 SavedRadioNewAction TrackObserved Perceived 2 2
J06 SavedLanternSeeking * * 3 3'''
ATT_PLANS={s:[(c,n,m,a,int(e),int(p)) for c,n,m,a,e,p in (l.split() for l in plan.splitlines())]
           for s,plan in [('attention',ATT_WORLD),('attention-integration',ATT_INTEGRATION)]}
ATT_COUNTS={'attention':24,'attention-integration':6}
ATT_EVENTS={
    'attention':[('DemoVisibleAction','AwarenessPropA','Visible',1),('OutsideConeAction','AwarenessPropA','OutsideViewCone',2),
                 ('DemoHiddenAction','AwarenessPropB','Occluded',1),('DemoNewAction','AwarenessPropB','Visible',2)],
    'attention-integration':[('SavedStatue','Inspect_Statue','Visible',1),('SavedRadioOccluded','Inspect_Radio','Occluded',1),
                             ('SavedRadioNewAction','Inspect_Radio','Visible',2),('SavedLanternSeeking','Inspect_Lantern','Visible',1)]}
ATT_FIELDS=set('status case step mode awareness has look known actual time now target player running attention body root eye forward rel relRot idle error request dt delta total travel frames evidence sampled emitted acquired final progress other otherYaw otherDt otherDelta otherTotal otherAwareness otherLook otherEye otherForward timerActive'.split())
ATT_NUMBERS='time now idle error request dt delta total travel otherYaw otherDt otherDelta otherTotal'.split()
ATT_VECTORS='known actual body root eye forward rel relRot otherEye otherForward'.split()
ATT_MODES={'Disabled','Idle','TrackObserved','HoldLastKnown'}
ATT_STATES={'Unaware','Perceived','LostSight'}
# The only setup-time manual yaw/eye changes occur with attention disabled.
ATT_RESET_POSE={'DefaultDisabled','EnableWithoutSight','WrapShortestPath','OffsetEye','BeforeEarlyLoss','IndependentTurning','DemoSetup'}
ATT_FREEZE_START={'HoldOnLoss','EarlyLoss','LateLoss','DemoHold'}
ATT_FREEZE_CONT={'HiddenLateralMovement','AttentionExpires','IdleReturn','DemoHiddenAction','DemoIdle'}
ATT_POSITIVE={'TurnRight':35,'VisibleMovingPlayer':35,'WrapShortestPath':30,'OffsetEye':20,
              'IndependentTurning':20,'DemoTurnAwayFromEvent':45,'DemoReacquire':15}
ATT_NEGATIVE={'TurnLeft':45,'BoundedAngularSpeed':50,'IdleReturn':10,'LateReacquire':30}

def ang(a,b): return (b-a+180)%360-180

def bearing(v): return math.degrees(math.atan2(v[1],v[0]))

def aligned(eye,forward,point):
    offset=tuple(a-b for a,b in zip(point,eye))
    return math.hypot(*offset[:2])>=9.99 and math.hypot(*forward[:2])>=0.0009 and abs(ang(bearing(forward),bearing(offset)))<=2.8

def att_fields(line):
    # Use the established duplicate/malformed key parser for both tags.
    m=parse_fields(line.replace(ATT_TAG,TAG,1));need(set(m)==ATT_FIELDS,'attention snapshot fields differ')
    for k in ATT_NUMBERS:
        m[k]=float(m[k]);need(math.isfinite(m[k]),f'non-finite attention {k}')
    for k in ATT_VECTORS: m[k]=vec(m[k])
    for k in ('look','otherLook'): m[k]=None if m[k]=='-' else vec(m[k])
    for k in ('frames','evidence','sampled','progress'):
        m[k]=int(m[k]);need(m[k]>=0,f'negative attention {k}')
    for k in ('has','running','attention','final','timerActive'): need(m[k] in ('yes','no'),f'invalid attention {k}')
    for k in ('emitted','acquired'): m[k]=ids(m[k])
    need(m['mode'] in ATT_MODES and m['awareness'] in ATT_STATES,'invalid attention state')
    need(m['other'] in ATT_MODES|{'None'} and m['otherAwareness'] in ATT_STATES|{'None'},'invalid second observer state')
    need((m['look'] is not None)==(m['mode'] in ('TrackObserved','HoldLastKnown')),'look point inconsistent with attention mode')
    need(m['time']>=0 and m['now']>=0 and m['time']<=m['now']+0.00001 and m['dt']>=0 and m['otherDt']>=0 and m['travel']>=0,'invalid attention clock')
    need(abs(m['error'])<=180.001 and abs(m['request'])<=9.001,'invalid requested rotation')
    if m['mode']=='Idle': need(abs(m['request'])<=4.501,'idle request exceeds cap')
    if m['mode']=='Disabled': need(abs(m['request'])<=0.001,'disabled rotation request')
    return m

def att_rate(mode):return 0 if mode in ('Disabled','None') else 45 if mode=='Idle' else 90

def att_frame(m,prev,start):
    need(m['frames']==prev['frames']+1,'skipped or duplicated physical sample')
    need(abs((m['now']-prev['now'])-m['dt'])<=0.000025,'frame time disagreement')
    need(abs(ang(prev['body'][1],m['body'][1])-m['delta'])<=0.00005,'measured yaw delta mismatch')
    need(abs(m['delta'])<=att_rate(m['mode'])*min(m['dt'],.1)+.121,'physical yaw exceeds rate')
    need(abs(m['total']-prev['total']-m['delta'])<=.00005 and abs(m['travel']-prev['travel']-abs(m['delta']))<=.00005,'rotation accumulation mismatch')
    need(max(abs(a-b) for a,b in zip(m['root'],start['root']))<=.102,'observer translated')
    need(all(abs(ang(start['body'][i],m['body'][i]))<=.101 for i in (0,2)),'observer pitch/roll drift')
    need(pos_equal(m['rel'],start['rel'],.004) and all(abs(ang(a,b))<=.15 for a,b in zip(m['relRot'],start['relRot'])),'eye relative transform changed')
    if m['other']!='None':
        need(abs(m['otherDt']-m['dt'])<=.000025 and abs(ang(prev['otherYaw'],m['otherYaw'])-m['otherDelta'])<=.00005,'second observer delta mismatch')
        need(abs(m['otherDelta'])<=att_rate(m['other'])*min(m['otherDt'],.1)+.121 and abs(m['otherTotal']-prev['otherTotal']-m['otherDelta'])<=.00005,'second observer rotation exceeds rate')

def verify_attention(text,suite,exit_status=0):
    need(exit_status==0,f'game process exit {exit_status}');need(not FATAL.search(text),'fatal/assert/ensure detected')
    lines=text.splitlines(); markers=[(i,l) for i,l in enumerate(lines) if TAG in l or ATT_TAG in l]
    count=ATT_COUNTS[suite]; plans=ATT_PLANS[suite]
    need(bool(markers) and TAG in markers[0][1] and TAG in markers[-1][1],'missing attention envelope')
    need(parse_fields(markers[0][1])=={'status':'BEGIN','suite':suite,'map':MAP,'expected':str(count)},'attention BEGIN mismatch')
    need(parse_fields(markers[-1][1])=={'status':'SUMMARY','suite':suite,'passed':str(count),'expected':str(count),'result':'PASS'},'attention SUMMARY mismatch')
    need(any('Engine exit requested' in l for l in lines[markers[-1][0]+1:]),'no clean engine exit after attention summary')
    idx=0;start=None;prev=None;last=None;frozen=None;early=False;events=[];emitted=[];acquired=[];completed=[];snapshots={};reset=False;pending=None;frame_count=0
    outcome_rows=[(i,l) for i,l in enumerate(lines) if 'PEDRO_OUTCOME policy=' in l]
    need(not outcome_rows if suite=='attention' else len(outcome_rows)==1 and
         'policy=EvidenceDriven interpretation=Seeking sampledCore=3 requiredCore=3 resolved=yes actualLOS=checked' in outcome_rows[0][1],
         'unexpected attention outcome')
    resolved=False;transition_frame=None;previous_sample_line=None
    for line_index,line in markers[1:-1]:
        need(idx<len(plans),'extra marker after attention steps');case,step,mode,awareness,evidence,sampled=plans[idx]
        final=step=='SavedLanternSeeking'
        if ATT_TAG not in line:
            m=parse_fields(line);status=m.get('status')
            if status=='RESET_EVIDENCE':
                need(suite=='attention' and step=='DemoSetup' and not reset and start is None and m=={'status':'RESET_EVIDENCE'},'unexpected evidence reset')
                reset=True;continue
            need(status=='EVENT' and start is None and pending is None and len(events)<len(ATT_EVENTS[suite]),'unexpected attention event or failure marker')
            name,op,trace,inspection=ATT_EVENTS[suite][len(events)]
            need(set(m)=={'status','id','subject','opportunity','expected','inspection'} and step==name and m['opportunity']==op and m['expected']==trace and int(m['inspection'])==inspection,'attention event sequence mismatch')
            event_id=ids(m['id']);need(len(event_id)==1 and event_id[0] not in emitted,'duplicate or invalid attention event ID')
            emitted+=event_id
            if trace=='Visible':acquired+=event_id
            events.append({'step':step,'id':event_id[0],'opportunity':op,'trace':trace,'inspection':inspection});pending=step;continue
        m=att_fields(line);status=m['status'];need(m['step']==step and m['case']==case,'attention step order mismatch')
        need(m['emitted']==emitted and m['acquired']==acquired and m['evidence']==evidence==len(acquired) and m['sampled']==sampled,'attention event history mismatch')
        # Only J06 may resolve, and only after its unresolved STEP_BEGIN.
        pair=(m['final'],m['progress'])
        if not final or status=='STEP_BEGIN':
            need(pair==('no',0),'premature attention completion')
        elif status=='FRAME':
            need(pair in (('no',0),('yes',1)),'incoherent attention progression')
            if resolved:
                need(pair==('yes',1),'attention completion reversed')
            elif pair==('yes',1):
                need(start is not None and previous_sample_line is not None and
                     previous_sample_line<outcome_rows[0][0]<line_index,'outcome outside observed J06 transition')
                resolved=True;transition_frame=m['frames']
        else:
            need(resolved and pair==('yes',1),'missing deferred attention completion')
        if status=='STEP_BEGIN':
            need(start is None and (pending==step)==any(e[0]==step for e in ATT_EVENTS[suite]),'duplicate setup or missing event')
            need(m['frames']==0 and m['total']==m['travel']==m['dt']==m['delta']==0,'invalid step baseline')
            if last is not None:
                need(m['now']>=last['now'] and pos_equal(m['root'],last['root'],.18),'step clock/root discontinuity')
                if step not in ATT_RESET_POSE:
                    need(abs(ang(last['body'][1],m['body'][1]))<=90*min(m['now']-last['now'],.1)+.15,'unexplained between-step yaw change')
                    need(pos_equal(m['rel'],last['rel'],.004),'unexpected eye reset')
            if step in ATT_FREEZE_START:
                need(last is not None and pos_equal(m['known'],last['known'],.004) and m['time']>=last['time'],'loss setup changed the last observed position')
                frozen=(m['known'],m['time'])
            if step=='EarlyReacquire':early=False
            pending=None;start=prev=m;previous_sample_line=line_index;continue
        need(start is not None,'attention sample without setup')
        if step in ATT_FREEZE_START|ATT_FREEZE_CONT:
            need(frozen is not None and (m['known'],m['time'])==frozen,'hidden location or timestamp leak')
            if m['look'] is not None:need(pos_equal(m['look'],frozen[0],.004),'hidden position used as look point')
        if status=='FRAME':
            att_frame(m,prev,start);frame_count+=1
            if step=='EarlyReacquire' and frozen and m['mode']=='TrackObserved' and m['time']>frozen[1] and m['now']-frozen[1]<2:early=True
            prev=m;previous_sample_line=line_index;continue
        need(status==('CHECK' if case=='-' else 'PASS'),'unexpected attention terminal marker')
        need(prev['frames']>=4 and all(m[k]==prev[k] for k in ATT_FIELDS-{'status','dt','delta','otherDt','otherDelta'}),'terminal snapshot not backed by measured frames')
        need((mode=='*' or m['mode']==mode) and (awareness=='*' or m['awareness']==awareness),'attention terminal state mismatch')
        if awareness=='*':need(m['awareness']!='Unaware','saved map unexpectedly lost all knowledge')
        if m['awareness']=='Unaware':need(m['known']==(0,0,0) and m['has']=='no' and m['time']==0 and m['target']=='None','fabricated unaware memory')
        else:need(m['has']=='yes' and m['target']==m['player'] and m['player']!='None','inconsistent target identity')
        if m['mode']=='TrackObserved':
            need(m['awareness']=='Perceived' and pos_equal(m['known'],m['actual']) and pos_equal(m['known'],m['look'],.004),'tracking not grounded in legitimate knowledge')
            need(m['now']-m['time']<=.36,'tracking stale knowledge')
            if step not in ('BeginMovingBeforeDisable','BeforeHoldWhileTurning'):need(aligned(m['eye'],m['forward'],m['known']),'physical eye not aligned')
        if m['mode']=='HoldLastKnown':need(m['awareness']=='LostSight' and m['now']-m['time']<2.01,'hold not bounded by last sighting')
        need(m['running']==('no' if step in ('SavedConfiguration','UnpossessClearsAttention','DestroyAttentionController') else 'yes'),'awareness lifecycle mismatch')
        need(m['attention']==('no' if step in ('SavedConfiguration','DefaultDisabled','DisableMidTurn','UnpossessClearsAttention','DestroyAttentionController') else 'yes'),'attention lifecycle mismatch')
        if step in ATT_POSITIVE:need(m['total']>ATT_POSITIVE[step],'missing positive autonomous turn')
        if step in ATT_NEGATIVE:need(m['total']< -ATT_NEGATIVE[step],'missing negative autonomous turn')
        if step in ('DefaultDisabled','InitiallyBehind','StationaryConverges','StationaryBody','DisableMidTurn','HiddenLateralMovement','DestroyAttentionController'):
            need(m['travel']<.21,'unexpected rotation at rest')
        if step=='WrapShortestPath':need(m['total']<45 and m['body'][1]< -145,'wrong wrap direction')
        if step=='OffsetEye':need(abs(m['rel'][1])>50 and abs(m['relRot'][1])>5,'offset eye fixture missing')
        if step in ('HiddenLateralMovement','DemoHiddenAction'):need(not pos_equal(m['known'],m['actual'],20),'hidden player never moved')
        if step in ('AttentionExpires','LateLoss','DemoIdle','IdleReturn'):need(m['now']-m['time']>=1.99999,'premature attention expiry')
        if step in ('EnableWithoutSight','InitiallyBehind','IdleReturn','DestroyPlayer','ReplaceHidden','RepossessFreshAnchor','LateLoss','DemoIdle'):
            need(abs(ang(m['body'][1],m['idle']))<=2.11,'did not return to idle anchor')
        if step=='EarlyReacquire':need(early,'did not reacquire before hold expiration')
        if step=='BeginMovingBeforeDisable':need(5<m['total']<45,'not actually turning before disable')
        if step=='ReenableFreshAnchor':need(abs(ang(start['body'][1],m['idle']))<=.11 and abs(ang(m['body'][1],m['idle']))>10,'fresh idle anchor incorrect')
        if step=='RepeatedEnableKeepsAnchor':need(abs(ang(snapshots['ReenableFreshAnchor']['idle'],m['idle']))<=.11,'repeated enable recaptured anchor')
        if step=='ReplaceHidden':need(m['player']!=snapshots['RepeatedEnableKeepsAnchor']['player'] and m['player']!='None','replacement not distinct')
        if step=='RepossessFreshAnchor':need(abs(ang(start['body'][1],m['idle']))<=.11,'repossessed anchor stale')
        if step=='DestroyAttentionController':need(m['timerActive']=='no','destroyed controller timer still active')
        if step=='IndependentTurning':need(m['other']=='TrackObserved' and m['otherAwareness']=='Perceived' and m['otherTotal']< -20 and m['otherLook'] is not None and pos_equal(m['otherLook'],m['actual']) and aligned(m['otherEye'],m['otherForward'],m['otherLook']),'second observer did not independently turn')
        if step=='AutonomousSavedTurn':need(m['travel']>15,'saved actor did not autonomously turn')
        if step=='BeforeHoldWhileTurning':need(not aligned(m['eye'],m['forward'],m['known']),'not turning when preparing occlusion')
        if step=='HoldOnLoss':need(m['total']>1 and aligned(m['eye'],m['forward'],m['known']),'did not finish turning towards remembered point under cover')
        snapshots[step]=m;last=m;start=prev=None;idx+=1
        if case!='-':completed.append(case)
    need(idx==len(plans) and start is None and pending is None and len(completed)==len(set(completed))==count,'incomplete attention cases')
    need(len(events)==len(ATT_EVENTS[suite]) and reset==(suite=='attention'),'incomplete attention evidence sequence')
    statuses=[l for l in lines if 'PEDRO_DIRECT status=' in l];direct=[l for l in lines if 'PEDRO_DIRECT acquired=yes' in l]
    need(len(statuses)==len(events) and len(direct)==len(acquired) and all('provenance=DirectObservation' in l for l in direct),'attention Director acquisition mismatch')
    for l,e in zip(statuses,events):need(f'status={e["trace"]} ' in l and f'opportunity={e["opportunity"]} ' in l and 'actualLOS=checked' in l,'attention Director visibility mismatch')
    need(resolved==(suite=='attention-integration'),'missing or unexpected attention completion transition')
    return {'status':'PASS','suite':suite,'cases':completed,'expected':count,'frames':frame_count,'steps':snapshots,'events':events,'resolution_frame':transition_frame}


def project_linked(text):
    return bool(re.search(r'^\s*\[\d+/\d+\]\s+Link\s+(?:\[[^\]\r\n]+\]\s+)?[^\r\n]*libUnrealEditor-AI_Gavin_Project\.so(?:\s|$)',text,re.M))

def verify_non_unity(text,exit_status):
    need(exit_status==0 and 'Result: Succeeded' in text,'non-Unity build failed')
    need(project_linked(text),'missing project linker action')
    commands=[l for l in text.splitlines() if 'Running command :' in l]
    need(any(all(flag in l.split() for flag in ('-DisableUnity','-DisableAdaptiveUnity','-gather')) and '-ForceUnity' not in l.split() for l in commands),'missing non-Unity flags')
    actions=re.findall(r'^\s*\[\d+/\d+\]\s+Compile\s+(?:\[[^\]\r\n]+\]\s+)?([^\r\n]+)',text,re.M)
    names=[Path(a.strip().split()[0].replace('\\','/')).name for a in actions]
    need(not any(re.fullmatch(UNITY_NAME,n) for n in names),'project Unity compile in non-Unity build')
    need(all(names.count(Path(p).name)==1 for p in UNITY_SOURCES),'missing or duplicate individual source compile')
    return {'status':'PASS','suite':'non-unity','expected':len(UNITY_SOURCES),'compiled_sources':list(UNITY_SOURCES)}


def main():
    p=argparse.ArgumentParser(); p.add_argument('suite',choices=['native','world','integration','unity','non-unity','attention','attention-integration']);p.add_argument('input',type=Path);p.add_argument('exit_status',type=int);p.add_argument('output',type=Path)
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
        elif a.suite=='non-unity':
            result=verify_non_unity(text,a.exit_status)
        elif a.suite in ATT_PLANS:
            result=verify_attention(text,a.suite,a.exit_status)
        else:
            result=verify_runtime(text,a.suite,a.exit_status)
        a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print(f'PASS: {a.suite}: {result.get("total",result.get("expected"))} verified')
        return 0
    except (ValueError,KeyError,TypeError,IndexError,OSError) as exc:
        print(f'FAIL: {a.suite}: {exc}',file=sys.stderr);return 1
if __name__=='__main__': sys.exit(main())
