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
NAVIGATION_NAMES='Disabled InvalidObserver NoKnowledge FreshObservedTarget InvalidTarget InvalidLocation InvalidTime InvalidSettings StartDistance StopDistance Hysteresis PreferredDestination PersonalSpace DegenerateGeometry CandidateGeneration CandidateRanking ProjectionLimits PartialAndExcessivePaths RetainExistingRequest ReplanThresholds ContinueLastKnown StaleAndExpiredKnowledge FailureCooldown RequestOwnership'.split()
NATIVE_NAMES |= {PREFIX+"Navigation."+name for name in NAVIGATION_NAMES}
assert len(NATIVE_NAMES)==125
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
    need(len(names)==125 and len(set(names))==125,'expected exactly 125 unique native tests')
    need(set(names)==NATIVE_NAMES,f'native inventory differs: missing={sorted(NATIVE_NAMES-set(names))} extra={sorted(set(names)-NATIVE_NAMES)}')
    for t in tests:
        need(t['state']=='Success' and t['warnings']==0 and t['errors']==0, f'native failure: {t["fullTestPath"]}')
    need(report['succeeded']==125,'incorrect success total')
    for field in ['succeededWithWarnings','failed','notRun','inProcess']:
        need(report[field]==0,f'{field} nonzero')
    need(exit_status==0,f'all 125 native tests passed, but native process exit {exit_status}')
    return {'status':'PASS','baseline':101,'new':24,'total':125,'tests':sorted(names)}

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
    'PedroAttentionPolicy.cpp','Tests/PedroAttentionTests.cpp',
    'PedroNavigationPolicy.cpp','Tests/PedroNavigationTests.cpp'))
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


# Navigation protocol is independent of the stationary awareness/attention protocols.
NAV_TAG='PEDRO_NAV_TEST '
NAV_WORLD='''N01 DisabledWithNavmesh
N02 InitiallyHidden
N03 WithinBand
N04 NativeFollow
N05 StandoffArrival
N06 HysteresisIdle
- ApproachWhileMoving
N07 PlayerApproaches
N08 SpeedAndBraking
- BeforeReplan
N09 ReplanVisible
N10 SmallMovementRetained
- RateFirst
- RateSecond
N11 BoundedReplanning
N12 NativeDetour
N13 NoNavigableStart
N14 OffmeshGoal
N15 DisconnectedRoute
N16 PhysicalStall
N17 RetryAfterBlockCleared
- BeforeStationaryLoss
N18 StationaryLoss
- BeforeFollowingLoss
N19 ContinueFixedRoute
N20 HiddenMoveNoRetarget
N21 ExpiredStops
N22 ReacquireNewRoute
- BeforeDisable
- DisabledStopsAndRestores
N23 ReenableFresh
N24 PlayerReplacement
- BeforeUnpossess
- UnpossessStops
- RepossessMoves
N25 DestroyControllerStops
N26 IndependentRoutes
N27 AttentionWhileWalking
- DemoBeforeMovement
- DemoRelocate
- DemoVisibleEvent
- DemoResumeMovement
- DemoLoseSight
- DemoHiddenEvent
- DemoStopNoEvidence
- DemoReacquireNoReplay
N28 DemoNewEvent'''
NAV_SAVED='''K01 SavedConfigurationAndNavmesh
K02 SavedAutonomousFollow
K03 SavedStatue
K04 SavedRadioOccluded
- SavedRestoreSightNoReplay
- SavedRelocationNoReplay
K05 SavedNewRadio
K06 SavedLanternSeeking'''
NAV_PLANS={'navigation': [tuple(x.split()) for x in NAV_WORLD.splitlines()],
           'navigation-integration':[tuple(x.split()) for x in NAV_SAVED.splitlines()],
           'navigation-probe':[('N01','DisabledWithNavmesh'),('N04','NativeFollow')]}
NAV_COUNTS={s:sum(c!='-' for c,_ in p) for s,p in NAV_PLANS.items()}
assert NAV_COUNTS=={'navigation':28,'navigation-integration':6,'navigation-probe':2}
NAV_EVENTS={
    'navigation': [('DemoVisibleEvent','AwarenessPropA','Visible',1),('DemoHiddenEvent','AwarenessPropB','Occluded',1),('DemoNewEvent','AwarenessPropB','Visible',2)],
    'navigation-integration':[('SavedStatue','Inspect_Statue','Visible',1),('SavedRadioOccluded','Inspect_Radio','Occluded',1),('SavedNewRadio','Inspect_Radio','Visible',2),('SavedLanternSeeking','Inspect_Lantern','Visible',1)],
    'navigation-probe':[]}
NAV_VECTORS='root start velocity body eye forward rel relRot goal sighting known actual otherRoot'.split()
NAV_FLOATS='now dt travel yawTravel maxSide sightTime planTime length time speedLimit accel brake'.split()
NAV_INTS='reason attention id submissions evidence sampled progress otherId'.split()
NAV_BOOLS='running active valid has grounded final accelPaths'.split()
NAV_STRINGS='status case step mode controller awareness target player emitted acquired'.split()
NAV_MODES={'Disabled','Idle','FollowObserved','ContinueLastKnown','Blocked'}

def nav_fields(line):
    result={}
    for token in line.split(NAV_TAG,1)[1].split():
        need('=' in token,'malformed navigation marker')
        k,v=token.split('=',1);need(k and v and k not in result,'duplicate/empty navigation field');result[k]=v
    return result

def nav_sample(d, *, replacement_prior=None, replacement_start=None, replacement_prev=None):
    need(set(d)==set(NAV_VECTORS+NAV_FLOATS+NAV_INTS+NAV_BOOLS+NAV_STRINGS),'navigation sample fields differ')
    r=dict(d)
    for k in NAV_VECTORS:r[k]=vec(d[k])
    for k in NAV_FLOATS:
        r[k]=float(d[k]);need(math.isfinite(r[k]),f'nonfinite {k}')
    for k in NAV_INTS:
        r[k]=int(d[k]);need(r[k]>=0,f'negative {k}')
    for k in NAV_BOOLS:
        need(d[k] in ('yes','no'),f'invalid {k}');r[k]=d[k]=='yes'
    for k in ('emitted','acquired'):r[k]=ids(d[k])
    need(r['mode'] in NAV_MODES and r['awareness'] in ('Unaware','Perceived','LostSight'),'invalid navigation/awareness mode')
    need(r['attention'] in range(4) and r['reason'] in range(18),'invalid reason or attention')
    need(r['now']>=0 and r['dt']>=0 and r['travel']>=0 and r['yawTravel']>=0 and r['maxSide']>=0 and r['time']>=0 and r['time']<=r['now']+1e-4,'invalid clock or rate metadata')
    need(r['evidence']==len(r['acquired']) and r['sampled']<=r['evidence'] and set(r['acquired'])<=set(r['emitted']),'incoherent evidence counters')
    need(r['final']==(r['progress']==1) and r['progress'] in (0,1),'incoherent progression')
    need(abs(r['body'][0])<=.51 and abs(r['body'][2])<=.51 and math.hypot(*r['velocity'][:2])<=185.1,'invalid physical pose or speed')
    if r['running']:
        need(abs(r['speedLimit']-180)<.01 and abs(r['accel']-500)<.01 and abs(r['brake']-600)<.01 and r['accelPaths'],'navigation movement configuration incorrect')
    if r['active']:
        need(r['running'] and r['valid'] and r['id']>0 and r['grounded'],'invalid owned route')
        need(r['mode'] in ('FollowObserved','ContinueLastKnown'),'active route in non-follow mode')
        need(0<r['length']<=2200.1 and r['submissions']>0 and 0<=r['sightTime']<=r['planTime']+1e-4<=r['now']+1e-3,'invalid route length/time')
        need(349.9<=math.dist(r['goal'][:2],r['sighting'][:2])<=550.1,'destination not at valid stand-off')
    else:need(r['id']==0 and not r['valid'] and r['length']==0 and pos_equal(r['goal'],(0,0,0),.01),'orphaned route metadata')
    if r['awareness']=='Unaware':need(not r['has'] and r['target']=='None' and pos_equal(r['known'],(0,0,0),.01) and r['time']==0,'unaware has spatial memory')
    else:
        need(r['has'] and (r['target']!='None' or replacement_prior is not None),'missing spatial memory')
    if replacement_prior is not None:
        verify_replacement_sample(r,replacement_prior,replacement_start,replacement_prev)
    return r


def verify_replacement_sample(m,prior,start,prev):
    # N24 destroys the old pawn synchronously in fixture Setup(), then captures
    # STEP_BEGIN before the next world tick. The weak target becomes null before
    # the awareness timer (0.1 s) can clear its historical snapshot. Navigation
    # must cancel on the next sampled tick, independently of that timer.
    need(m['case']=='N24' and m['step']=='PlayerReplacement' and prior['step']=='ReenableFresh',
         'replacement exception outside N24')
    need(prior['active'] and prior['awareness']=='Perceived' and prior['has'] and prior['target']==prior['player']!='None',
         'replacement requires a previously owned observed route')
    need(m['controller']==prior['controller']!='None' and m['player'] not in ('None',prior['player']) and m['running'] and m['submissions']==prior['submissions'],
         'replacement identity or submission count changed')
    def stale_snapshot(reference):
        need(m['awareness']=='Perceived' and m['has'] and m['target']=='None' and pos_equal(m['known'],reference['known'],.01),
             'replacement retained foreign or changed memory')
    if m['status']=='STEP_BEGIN':
        need(start is None and prev is None,'duplicate replacement begin')
        if m['awareness']!='Unaware':
            stale_snapshot(prior)
            need(m['time']>=prior['time'],'replacement sighting time regressed')
        if m['active']:
            need(m['mode']==prior['mode'] and m['id']==prior['id'] and m['goal']==prior['goal'] and m['sighting']==prior['sighting'] and m['sightTime']==prior['sightTime'] and m['planTime']==prior['planTime'] and m['length']==prior['length'],
                 'replacement begin changed old owned route')
        return
    need(start is not None and prev is not None and m['status'] in ('FRAME','PASS') and m['player']==start['player'],
         'replacement frame without matching begin')
    need(not m['active'] and m['mode']=='Idle' and m['id']==0,'replacement retained or issued a route after setup')
    if m['awareness']!='Unaware':
        # Only a frozen, invalid weak target from setup can persist briefly;
        # no active movement, re-sighting, or resurrection after clearing.
        need(m['status']=='FRAME' and start['awareness']=='Perceived' and prev['awareness']=='Perceived' and m['now']-start['now']<=.2001,
             'replacement memory not cleared within bounded refresh window')
        stale_snapshot(start)
        need(m['time']==start['time'],'replacement stale timestamp changed')
    else:
        need(m['target']=='None' and not m['has'],'replacement memory did not clear')

def saved_heading_error(m):
    dx=m['actual'][0]-m['eye'][0];dy=m['actual'][1]-m['eye'][1]
    fx,fy=m['forward'][:2]
    need(math.hypot(dx,dy)>=10 and math.hypot(fx,fy)>=.001,'degenerate saved heading')
    bearing=math.degrees(math.atan2(dy,dx));facing=math.degrees(math.atan2(fy,fx))
    return abs((bearing-facing+180)%360-180)


def verify_saved_follow(start,end):
    # The initial orientation is set only during fixture setup. Require a real
    # off-axis target and net convergence, not merely accumulated yaw travel.
    need(11.9<=saved_heading_error(start)<=30.1,'saved follow starts without a valid off-axis target')
    need(not start['active'] and start['submissions']==0,'saved follow already had a route at setup')
    need(pos_equal(start['actual'],end['actual'],.01),'saved follow target moved during test')
    need(end['attention']==2 and saved_heading_error(end)<=2.7,'saved follow did not converge on observed player')
    net=abs((end['body'][1]-start['body'][1]+180)%360-180)
    need(net>5 and end['yawTravel']>5 and end['travel']>150 and end['submissions']>0,'saved follow lacks actual movement/turn')


def verify_saved_continuity(prior,start):
    need(prior is not None and prior['controller']==start['controller'] and prior['player']==start['player'] and start['running'], 'saved observer/controller identity changed between legs')
    need(pos_equal(prior['root'],start['root'],.1) and pos_equal(prior['body'],start['body'],.1) and pos_equal(prior['rel'],start['rel'],.01) and pos_equal(prior['relRot'],start['relRot'],.01),'saved observer pose changed between steps')
    need(not prior['active'] and not start['active'] and start['submissions']==prior['submissions'],'saved setup changed movement request')
    if start['step']=='SavedRelocationNoReplay':
        need(math.dist(start['actual'][:2],prior['actual'][:2])>=124.99 and math.dist(start['root'][:2],start['actual'][:2])>=649.99,'second leg lacks a new distant player position')
    else:
        need(pos_equal(prior['actual'],start['actual'],.01),'saved player relocated outside intended setup')


def verify_saved_restore_sample(start,m):
    need(m['controller']==start['controller'] and m['player']==start['player'] and pos_equal(m['actual'],start['actual'],.01) and m['running'], 'saved sight restoration changed identity or player position')
    need(not m['active'] and m['submissions']==start['submissions'] and m['travel']<2 and math.dist(m['root'][:2],start['root'][:2])<2 and math.hypot(*m['velocity'][:2])<1.01,'saved sight restoration moved or submitted a route')
    need(m['emitted']==start['emitted'] and m['acquired']==start['acquired'] and m['evidence']==m['sampled']==1 and not m['final'] and m['progress']==0,'saved sight restoration altered evidence')


def saved_restore_ready(start,m):
    return (m['awareness']=='Perceived' and m['has'] and m['target']==m['player'] and pos_equal(m['known'],m['actual'],1.01)
            and m['time']>start['time'] and m['now']-m['time']<=.3501 and m['attention']==2 and m['mode']=='Idle'
            and 250<=math.dist(m['root'][:2],m['known'][:2])<=505 and saved_heading_error(m)<=2.7)


def verify_saved_relocation(start,end):
    need(start['controller']==end['controller'] and start['player']==end['player'] and pos_equal(start['actual'],end['actual'],.01),'second leg target or controller changed during movement')
    need(end['submissions']>start['submissions'] and not end['active'],'second leg lacks a completed new movement request')
    need(end['travel']>150 and math.dist(start['root'][:2],end['root'][:2])>150,'second leg lacks actual net displacement')
    need(end['attention']==2 and saved_heading_error(end)<=2.7,'second leg did not converge on observed player')


def verify_navigation(text,suite,exit_status=0):
    need(exit_status==0,'navigation process exit nonzero');need(not FATAL.search(text),'fatal/assert/ensure detected')
    need(suite in NAV_PLANS,'unknown navigation suite')
    lines=text.splitlines();marks=[(i,nav_fields(l)) for i,l in enumerate(lines) if NAV_TAG in l]
    count=NAV_COUNTS[suite];plans=NAV_PLANS[suite]
    need(marks and marks[0][1]=={'status':'BEGIN','suite':suite,'map':MAP,'expected':str(count)},'navigation BEGIN mismatch')
    need(marks[-1][1]=={'status':'SUMMARY','suite':suite,'passed':str(count),'expected':str(count),'result':'PASS'},'navigation SUMMARY mismatch')
    need(any('Engine exit requested' in l for l in lines[marks[-1][0]+1:]),'missing clean exit')
    need(not any('PEDRO_AWARENESS_TEST status=FAIL' in l for l in lines),'fixture failure marker')
    expected_events=NAV_EVENTS[suite];seen_events=[];acquired=[];emitted=[];pending=None
    event_by_step={s:(op,trace,n) for s,op,trace,n in expected_events}
    idx=0;start=prev=None;frames=total_frames=0;travel=yawtravel=side=0.;cap=False;saved=False;reset=False;completed=[];snapshots={};transition=0;outcomes=0
    history={};last_by_controller={};last_step=None;restore_stable=None;restore_frames=0
    def sample_provenance(m):
        c=m['controller'];h=history.setdefault(c,[])
        if m['awareness']=='Perceived' and m['has'] and m['target']==m['player']:
            h.append((m['time'],m['known']))
            if len(h)>512:del h[:256]
        old=last_by_controller.get(c)
        if m['active'] and (old is None or not old['active'] or old['id']!=m['id']):
            need(m['mode']=='FollowObserved' and m['awareness']=='Perceived','new route without fresh sight')
            need(m['target']==m['player'] and m['sightTime']<=m['time']+1e-4 and m['planTime']-m['sightTime']<=.3501,'stale/foreign route provenance')
            need(any(pos_equal(p,m['sighting'],1.01) and t+1e-4>=m['sightTime'] for t,p in h),'destination lacks legitimate sighting')
            if old and old['submissions'] and m['submissions']>old['submissions'] and old['player']==m['player']:
                need(m['submissions']==old['submissions']+1 and m['planTime']-old['planTime']>=.3498,'route submission rate or sequence invalid')
        if old and old['active'] and m['active'] and old['id']==m['id']:
            need(pos_equal(old['goal'],m['goal'],.01) and pos_equal(old['sighting'],m['sighting'],.01) and old['sightTime']==m['sightTime'] and old['submissions']==m['submissions'],'active fixed goal mutated')
        if m['active'] and m['mode']=='ContinueLastKnown':need(m['now']-m['time']<2.0001,'expired active route')
        last_by_controller[c]=m
    # Consume event and outcome markers in their original order alongside samples.
    for line in lines[marks[0][0]+1:marks[-1][0]]:
        if 'PEDRO_AWARENESS_TEST status=RESET_EVIDENCE' in line:
            need(suite=='navigation' and not reset and not emitted and start is None,'unexpected evidence reset');reset=True;continue
        if 'PEDRO_AWARENESS_TEST status=EVENT' in line:
            d=parse_fields(line);need(start is None and pending is None and len(seen_events)<len(expected_events),'unexpected evidence event')
            step,op,trace,n=expected_events[len(seen_events)]
            need(idx<len(plans) and plans[idx][1]==step and d['opportunity']==op and d['expected']==trace and int(d['inspection'])==n,'event timing/identity mismatch')
            eid=ids(d['id'])[0];need(eid not in emitted,'duplicate event identity');emitted.append(eid)
            if trace=='Visible':acquired.append(eid)
            pending=step;seen_events.append({'step':step,'opportunity':op,'trace':trace,'id':eid});continue
        if 'PEDRO_OUTCOME ' in line:
            need(suite=='navigation-integration' and start and start['step']=='SavedLanternSeeking' and not outcomes and not transition and 'policy=EvidenceDriven interpretation=Seeking sampledCore=3 requiredCore=3 resolved=yes actualLOS=checked' in line,'unexpected outcome');outcomes+=1;continue
        if NAV_TAG not in line:continue
        d=nav_fields(line);kind=d.get('status')
        if kind=='CAPABILITY':
            need(not cap and suite!='navigation-integration' and start and start['step']=='DisabledWithNavmesh' and set(d)=={'status','dynamic','connected','disconnected','start','end','island','length'},'unexpected capability marker')
            need(all(d[x]=='yes' for x in ('dynamic','connected','disconnected')) and math.isfinite(float(d['length'])) and float(d['length'])>=600 and math.dist(vec(d['start']),vec(d['end']))>=600 and math.dist(vec(d['end']),vec(d['island']))>=1000,'navigation capability failed');cap=True;continue
        if kind=='SAVED_READY':
            need(not saved and suite=='navigation-integration' and start and start['step']=='SavedConfigurationAndNavmesh' and set(d)=={'status','original','dynamic','projected'} and d['original']==d['dynamic']=='yes','saved configuration not proved');vec(d['projected']);saved=True;continue
        need(kind in ('STEP_BEGIN','FRAME','PASS','CHECK'),'unexpected navigation marker')
        need(idx<len(plans) and (d.get('case'),d.get('step'))==plans[idx],'navigation case order mismatch')
        case,step=plans[idx]
        try:
            if suite=='navigation' and step=='PlayerReplacement':
                m=nav_sample(d,replacement_prior=last_step,replacement_start=start,replacement_prev=prev)
            else:
                m=nav_sample(d)
        except (ValueError,KeyError,TypeError) as exc:
            raise ValueError(f'{case} {step} {kind}: {exc}') from exc
        need(m['emitted']==emitted and m['acquired']==acquired and m['evidence']==len(acquired) and m['sampled']==len(acquired),'event history differs')
        if step!='SavedLanternSeeking':need(not m['final'] and m['progress']==0,'premature completion')
        if kind=='STEP_BEGIN':
            need(start is None and m['dt']==0 and m['travel']==0 and m['yawTravel']==0 and m['maxSide']==0 and pos_equal(m['root'],m['start'],.01),'invalid step begin')
            need((pending==step)==(step in event_by_step),'missing event before step');pending=None
            if step=='SavedLanternSeeking':need(not m['final'] and m['progress']==0,'premature K06 completion')
            if suite=='navigation-integration' and step not in ('SavedConfigurationAndNavmesh','SavedAutonomousFollow'):verify_saved_continuity(last_step,m)
            start=prev=m;frames=0;travel=yawtravel=side=0.
            if step=='SavedRestoreSightNoReplay':
                restore_stable=None;restore_frames=0;verify_saved_restore_sample(start,m)
            # Fixture may reposition player/controller between steps; a cached prior
            # observation at STEP_BEGIN is intentionally not treated as new sight.
            if m['controller'] not in last_by_controller or not m['active']:
                sample_provenance(m)
            continue
        need(start is not None and prev is not None,'frame without begin')
        need(pos_equal(start['start'],m['start'],.01) and pos_equal(start['rel'],m['rel'],.01) and pos_equal(start['relRot'],m['relRot'],.01),'eye relative transform changed')
        need(abs(m['root'][2]-start['root'][2])<=12.01,'unexpected vertical motion')
        if step=='SavedRestoreSightNoReplay':
            verify_saved_restore_sample(start,m)
            if kind=='FRAME':
                if saved_restore_ready(start,m):
                    if restore_stable is None:restore_stable=m['now']
                    restore_frames+=1
                else:restore_stable=None;restore_frames=0
            else:
                need(saved_restore_ready(start,m) and restore_stable is not None and restore_frames>=4 and m['now']-restore_stable>=.24998,'saved sight restoration lacks stable fresh perception')
        if step=='SavedLanternSeeking':
            if m['final'] and not prev['final']:
                need(outcomes==1 and not transition and m['progress']==1 and kind=='FRAME','invalid deferred completion');transition=1
            if prev['final']:need(m['final'] and m['progress']==1,'reversed completion')
        if kind=='FRAME':
            if step in ('SavedAutonomousFollow','SavedRelocationNoReplay'):need(pos_equal(start['actual'],m['actual'],.01) and start['controller']==m['controller'] and start['player']==m['player'],'saved follow target or controller changed during test')
            if step in ('ContinueFixedRoute','HiddenMoveNoRetarget','ExpiredStops','DemoLoseSight','DemoHiddenEvent','DemoStopNoEvidence','StationaryLoss'):
                need(pos_equal(m['known'],start['known'],.01) and m['time']==start['time'] and m['submissions']==start['submissions'],'hidden memory or request count changed within step')
                if step in ('HiddenMoveNoRetarget','ExpiredStops','DemoHiddenEvent','DemoStopNoEvidence'):
                    need(last_step and m['time']==last_step['time'] and pos_equal(m['known'],last_step['known'],.01),'hidden memory changed across steps')
            dt=m['now']-prev['now'];need(dt>0 and abs(dt-m['dt'])<.00002,'invalid frame clock')
            dist=math.dist(m['root'][:2],prev['root'][:2]);yaw=abs((m['body'][1]-prev['body'][1]+180)%360-180)
            need(dist<=190*dt+.505,'excess measured displacement');need(yaw<=90*min(dt,.1)+.805,'excess measured yaw')
            if m['attention']==0:need(yaw<=.105,'movement changed yaw without attention')
            if step=='IndependentRoutes' and m['otherId'] and prev['otherId']:
                need(m['otherId']!=m['id'] and math.dist(m['otherRoot'][:2],prev['otherRoot'][:2])<=190*dt+.505,'second observer invalid movement or shared ID')
            if step=='NativeDetour':
                x,y=m['root'][0]-100000,m['root'][1]-100000
                need(math.hypot(x-min(max(x,210),290),y-min(max(y,680),1120))>=30,'observer crosses physical detour blocker')
            # Acceleration limit applies when increasing speed on one owned route;
            # explicit cancellation is allowed to brake instantly.
            if step=='SpeedAndBraking' and m['active'] and prev['active'] and m['id']==prev['id']:
                a=math.hypot(*m['velocity'][:2])-math.hypot(*prev['velocity'][:2]);need(a<=500*dt+4,'excess acceleration')
            travel+=dist;yawtravel+=yaw;side=max(side,abs(m['root'][1]-start['root'][1]));frames+=1;total_frames+=1
            need(abs(travel-m['travel'])<=.06+frames*.002 and abs(yawtravel-m['yawTravel'])<=.06+frames*.00001 and abs(side-m['maxSide'])<.01,'reported movement totals inconsistent')
            sample_provenance(m);prev=m;continue
        need(kind==('CHECK' if case=='-' else 'PASS') and frames>=4 and m['dt']==0 and abs(m['now']-prev['now'])<.00002 and pos_equal(m['root'],prev['root'],.01),'incomplete navigation frame chain')
        need(abs(m['travel']-prev['travel'])<.002 and abs(m['yawTravel']-prev['yawTravel'])<.002 and m['active']==prev['active'] and m['id']==prev['id'],'completion differs from last frame')
        def stopped():return not m['active'] and math.hypot(*m['velocity'][:2])<1.01
        def following():return m['active'] and m['mode']=='FollowObserved' and m['awareness']=='Perceived'
        def seen():return m['awareness']=='Perceived' and m['has'] and m['target']==m['player'] and pos_equal(m['known'],m['actual'],1.01)
        if step=='DisabledWithNavmesh':need(cap and m['mode']=='Disabled' and stopped() and seen() and m['travel']<2,'disabled control or capability failed')
        if step in ('InitiallyHidden','PlayerReplacement','UnpossessStops','DestroyControllerStops'):need(stopped() and m['awareness']=='Unaware','lifecycle/hidden observer did not stop')
        if step in ('WithinBand','HysteresisIdle','BeforeStationaryLoss','DisabledStopsAndRestores'):need(stopped() and seen() and m['travel']<2,'stationary condition failed')
        if step in ('WithinBand','InitiallyHidden','StationaryLoss'):need(m['submissions']==0,'unexpected request')
        if step in ('NativeFollow','ApproachWhileMoving','BeforeReplan','BeforeFollowingLoss','BeforeDisable','ReenableFresh','BeforeUnpossess','RepossessMoves','IndependentRoutes','AttentionWhileWalking','DemoResumeMovement'):need(following() and seen() and m['travel']>30,'native locomotion not demonstrated')
        if step=='NativeFollow':need(m['travel']>80,'probe lacks real movement')
        if step in ('StandoffArrival','SpeedAndBraking','NativeDetour','DemoRelocate','SavedAutonomousFollow','SavedRelocationNoReplay','DemoReacquireNoReplay'):
            need(stopped() and seen() and 250<=math.dist(m['root'][:2],m['known'][:2])<=505,'invalid stand-off arrival')
        if step=='SpeedAndBraking':need(m['travel']>600,'insufficient speed/braking exercise')
        if step=='NativeDetour':need(m['travel']>750 and m['maxSide']>230,'detour not demonstrated')
        if step=='PlayerApproaches':need(stopped() and seen() and m['travel']<50,'failed to cancel near player')
        if step in ('NoNavigableStart','DisconnectedRoute'):need(stopped() and m['mode']=='Blocked' and m['submissions']==0 and m['travel']<2,'unreachable path accepted')
        if step=='PhysicalStall':need(stopped() and m['mode']=='Blocked' and m['submissions']>0 and m['root'][0]<100110,'physical stall not aborted')
        if step=='OffmeshGoal':need((stopped() and m['mode']=='Blocked') or (following() and m['goal'][0]<101500),'unsafe offmesh goal')
        if step=='RetryAfterBlockCleared':need(following() and m['submissions']>1 and m['root'][0]>100200,'no successful retry')
        if step in ('ReplanVisible','RateFirst','RateSecond','BoundedReplanning','ReacquireNewRoute'):need(following() and last_step is not None and m['submissions']==last_step['submissions']+1,'replanning missing or duplicated')
        if step=='SmallMovementRetained':need(following() and last_step and m['id']==last_step['id'] and m['submissions']==last_step['submissions'],'unnecessary replan')
        if step in ('ContinueFixedRoute','HiddenMoveNoRetarget','ExpiredStops','DemoLoseSight','DemoHiddenEvent','DemoStopNoEvidence'):
            reference=snapshots['BeforeFollowingLoss'] if step in ('ContinueFixedRoute','HiddenMoveNoRetarget','ExpiredStops') else snapshots['DemoResumeMovement']
            need(m['awareness']=='LostSight' and pos_equal(m['known'],reference['known'],1.01) and m['time']+1e-4>=reference['time'] and m['submissions']==reference['submissions'],'hidden movement changed memory or request count')
            for q in (start,m):
                if q['active']:need(q['id']==reference['id'] and pos_equal(q['goal'],reference['goal'],.01),'hidden route redirected')
            if step in ('ContinueFixedRoute','HiddenMoveNoRetarget'):need(m['active'] and m['mode']=='ContinueLastKnown' and m['travel']>10,'did not continue fixed route')
            if step in ('ExpiredStops','DemoStopNoEvidence'):need(stopped() and m['now']-m['time']>=1.9999,'expired route not stopped')
        if step=='StationaryLoss':need(stopped() and m['awareness']=='LostSight','stationary sight loss moved')
        if step=='HiddenMoveNoRetarget':need(not pos_equal(m['actual'],m['known'],50),'hidden movement not demonstrated')
        if step=='IndependentRoutes':need(m['otherId'] and m['otherId']!=m['id'] and m['otherRoot'][0]>100070,'second route missing')
        if step=='AttentionWhileWalking':need(m['yawTravel']>20 and m['attention']==2 and m['travel']>120,'simultaneous movement and attention not proved')
        if step=='DemoRelocate':need(m['travel']>550,'physical relocation not demonstrated')
        if step=='SavedConfigurationAndNavmesh':need(saved and stopped() and m['evidence']==0,'saved-map preconditions not proved')
        if step=='SavedAutonomousFollow':verify_saved_follow(start,m)
        if step=='SavedRelocationNoReplay':verify_saved_relocation(start,m)
        if step=='SavedLanternSeeking':need(m['final'] and m['progress']==1 and transition==1 and outcomes==1,'missing deferred resolution')
        snapshots[step]=m;last_step=m;idx+=1;start=prev=None
        if case!='-':completed.append(case)
    need(idx==len(plans) and start is None and len(completed)==count and len(set(completed))==count,'incomplete navigation case sequence')
    need(len(seen_events)==len(expected_events) and pending is None and reset==(suite=='navigation'),'incomplete navigation events')
    need((cap and not saved) if suite!='navigation-integration' else (saved and not cap),'missing navigation capability')
    need(outcomes==transition==int(suite=='navigation-integration'),'resolution count differs')
    statuses=[l for l in lines if 'PEDRO_DIRECT status=' in l];direct=[l for l in lines if 'PEDRO_DIRECT acquired=yes' in l]
    need(len(statuses)==len(seen_events) and len(direct)==len(acquired) and all('provenance=DirectObservation' in l for l in direct),'director log does not match acquired events')
    for l,e in zip(statuses,seen_events):need(f'status={e["trace"]} ' in l and f'opportunity={e["opportunity"]} ' in l and 'actualLOS=checked' in l,'director trace mismatch')
    return {'status':'PASS','suite':suite,'expected':count,'cases':completed,'frames':total_frames,'events':seen_events,'steps':snapshots,'resolution':transition}


def main():
    p=argparse.ArgumentParser(); p.add_argument('suite',choices=['native','world','integration','unity','non-unity','attention','attention-integration','navigation','navigation-integration','navigation-probe']);p.add_argument('input',type=Path);p.add_argument('exit_status',type=int);p.add_argument('output',type=Path)
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
        elif a.suite in NAV_PLANS:
            result=verify_navigation(text,a.suite,a.exit_status)
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
