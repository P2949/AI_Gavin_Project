"""Offline protocol tests; synthetic examples are not Unreal test evidence."""
import subprocess
import tempfile
from unittest.mock import patch
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import verify_awareness_report as v
import b2b6a_audit as audit


def native_report():
    return dict(tests=[dict(fullTestPath=n,state='Success',warnings=0,errors=0) for n in sorted(v.NATIVE_NAMES)],
                succeeded=165,succeededWithWarnings=0,failed=0,notRun=0,inProcess=0)


def example_log(suite):
    """Construct an illustrative complete stream with real-shaped independent IDs."""
    tag=v.TAG
    rows=[tag+f'status=BEGIN suite={suite} map={v.MAP} expected={v.COUNTS[suite]}']
    evs={s:(op,vis,count) for s,op,vis,count in v.EVENTS[suite]}
    emitted=[]; acquired=[]; known=(0,0,0); actual=(600,0,0); time=0; seq=0
    after_destroy=False
    positions={'InitiallyVisible':(600,0,0),'InitiallyBehind':(600,0,0),'InitiallyOutsideRange':(2200,0,0),'InitiallyBehindCover':(600,0,0),'ClearAfterInitialCover':(600,0,0),
               'VisiblePlayerMovement':(750,100,0),'LostBeforeEnterRange':(2200,0,0),'PlayerEntersRange':(1400,0,0),'RetainInHysteresisMargin':(1650,0,0),'LoseBeyondHysteresis':(2050,0,0),'NoReacquireInMargin':(1650,0,0),'ReacquireInsideRadius':(600,0,0),'WallBlocksSight':(650,-250,0),'OccludedPlayerMovement':(900,400,0),'DistractorIgnored':(-600,0,0),'VisibleBeforeDestroy':(600,0,0),'PlayerDestroyed':(0,0,0),'ReplacementInitiallyHidden':(-600,0,0),'ReplacementNeedsOwnSighting':(600,0,0),'RepossessWhileHidden':(-600,0,0),'RepossessedFreshSighting':(600,0,0),'PlayerHiddenEventClear':(-600,0,0),'DemoDetect':(600,0,0),'DemoHiddenAction':(900,400,0)}
    def vs(x):return ','.join(map(str,x))
    def es(x):return ','.join(x) or '-'
    for i,(case,step,aw,evidence,sampled) in enumerate(v.PLANS[suite]):
        if step in ('AwarenessOnlyNoEvidence','DemoDetect'):
            rows.append(tag+'status=RESET_EVIDENCE');emitted=[];acquired=[]
        if step in positions: actual=positions[step]
        if step=='PlayerDestroyed': after_destroy=True
        player='None' if step=='PlayerDestroyed' else ('Replacement' if after_destroy else 'Original')
        if step in evs:
            op,vis,count=evs[step];seq+=1;eid=f'{seq:032x}';emitted.append(eid)
            rows.append(f'PEDRO_DIRECT status={vis} kind=1 subject={op} opportunity={op} actualLOS=checked')
            if vis=='Visible':
                acquired.append(eid);rows.append(f'PEDRO_DIRECT acquired=yes kind=1 subject={op} opportunity={op} total={len(acquired)} provenance=DirectObservation')
            rows.append(tag+f'status=EVENT id={eid} subject={op} opportunity={op} expected={vis} inspection={count}')
        aw='Perceived' if aw=='*' else aw
        if aw=='Perceived': known=actual;time=i+1
        elif aw=='Unaware':known=(0,0,0);time=0
        target='None' if aw=='Unaware' else player
        native='yes' if aw=='Perceived' else 'no'
        has='no' if aw=='Unaware' else 'yes'
        running='no' if step in ('UnpossessClears','ControllerDestroyed','SavedConfiguration') else 'yes'
        final=int(step=='LanternSeekingCompletion')
        extra=f'other={"Unaware" if step in ("IndependentObservers","ControllerDestroyed") else "None"} otherNative=no distractorNative={"yes" if step=="DistractorIgnored" else "no"} destroyedTimerActive=no'
        fields=f'case={case} step={step} awareness={aw} native={native} has={has} known={vs(known)} actual={vs(actual)} time={time} running={running} target={target} player={player} evidence={evidence} sampled={sampled} emitted={es(emitted)} acquired={es(acquired)} final={"yes" if final else "no"} progress={final} nativeLoc={vs(actual if native=="yes" else (0,0,0))} nativeAge={0 if native=="yes" else -1} eye={"0,80,84" if step=="EyeOffsetAndFacingRestored" else "0,0,64"} forward={"-1,0,0" if step=="EyeOnlyTurnsAway" else "1,0,0"} {extra}'
        rows.append(tag+'status=STEP_BEGIN '+fields)
        if final: rows.append('PEDRO_OUTCOME policy=EvidenceDriven interpretation=Seeking sampledCore=3 requiredCore=3 resolved=yes actualLOS=checked')
        rows.append(tag+('status=CHECK ' if case=='-' else 'status=PASS ')+fields)
    rows.append(tag+f'status=SUMMARY suite={suite} passed={v.COUNTS[suite]} expected={v.COUNTS[suite]} result=PASS')
    rows.append('LogCore: Engine exit requested (reason: GenericPlatform RequestExit)')
    return '\n'.join(rows)+'\n'

class NativeTests(unittest.TestCase):
    def test_valid(self): self.assertEqual(v.native(native_report())['total'],165)
    def test_missing(self):
        r=native_report();r['tests'].pop()
        with self.assertRaises(ValueError): v.native(r)
    def test_duplicate(self):
        r=native_report();r['tests'][0]=r['tests'][1]
        with self.assertRaises(ValueError): v.native(r)
    def test_wrong_name(self):
        r=native_report();r['tests'][0]['fullTestPath']='AI_Gavin_Project.Pedro.Unexpected'
        with self.assertRaises(ValueError):v.native(r)
    def test_warning(self):
        r=native_report();r['tests'][0]['warnings']=1
        with self.assertRaises(ValueError):v.native(r)
    def test_skip(self):
        r=native_report();r['notRun']=1
        with self.assertRaises(ValueError):v.native(r)
    def test_process_failure(self):
        with self.assertRaisesRegex(ValueError,'all 165 native tests passed, but native process exit 1'):v.native(native_report(),1)
    def test_process_failure_diagnostics(self):
        text=v.native_diagnostics(native_report(),1)
        self.assertIn('Native process exit: 1',text)
        self.assertIn('"succeeded": 165',text)
        self.assertNotIn('Native test diagnostic:',text)
    def test_failed_assertion_diagnostics(self):
        report=native_report();report['tests'][0].update(state='Fail',errors=1,entries=[{'event':{'message':'actual assertion'}}])
        text=v.native_diagnostics(report,1)
        self.assertIn(report['tests'][0]['fullTestPath'],text)
        self.assertIn('actual assertion',text)
        with self.assertRaisesRegex(ValueError,'native failure:'):v.native(report,1)

class RuntimeTests(unittest.TestCase):
    def test_valid_world(self): self.assertEqual(len(v.verify_runtime(example_log('world'),'world')['cases']),25)
    def test_valid_integration(self): self.assertEqual(len(v.verify_runtime(example_log('integration'),'integration')['cases']),6)
    def test_missing_case(self):
        text='\n'.join(l for l in example_log('world').splitlines() if 'status=PASS case=W11 ' not in l)
        with self.assertRaises(ValueError): v.verify_runtime(text,'world')
    def test_duplicate_case(self):
        text=example_log('world');line=next(l for l in text.splitlines() if 'status=PASS case=W11 ' in l)
        with self.assertRaises(ValueError):v.verify_runtime(text.replace(line,line+'\n'+line),'world')
    def test_wrong_total(self):
        with self.assertRaises(ValueError):v.verify_runtime(example_log('world').replace('passed=25','passed=24'),'world')
    def test_failure_marker(self):
        with self.assertRaises(ValueError):v.verify_runtime(example_log('world').replace('status=PASS case=W11','status=FAIL case=W11'),'world')
    def test_hidden_position_leak(self):
        text='\n'.join(l.replace('known=600,0,0','known=900,400,0') if 'status=PASS case=W11 ' in l else l for l in example_log('world').splitlines())
        with self.assertRaises(ValueError):v.verify_runtime(text,'world')
    def test_frozen_time_leak(self):
        text='\n'.join(l.replace('time=14','time=999') if 'status=PASS case=W11 ' in l else l for l in example_log('world').splitlines())
        self.assertNotEqual(text,example_log('world').strip())
        with self.assertRaises(ValueError):v.verify_runtime(text,'world')
    def test_missing_hidden_action(self):
        text='\n'.join(l for l in example_log('world').splitlines() if not ('status=EVENT' in l and 'AwarenessPropB' in l and 'expected=Occluded' in l))
        with self.assertRaises(ValueError):v.verify_runtime(text,'world')
    def test_wrong_event_identity(self):
        text=example_log('integration');line=next(l for l in text.splitlines() if 'status=PASS case=I04 ' in l)
        bad=line.replace('acquired='+f'{1:032x}','acquired='+f'{2:032x}')
        self.assertNotEqual(line,bad)
        with self.assertRaises(ValueError):v.verify_runtime(text.replace(line,bad),'integration')
    def test_lifecycle_inconsistent(self):
        text=example_log('world');line=next(l for l in text.splitlines() if 'status=PASS case=W17 ' in l)
        with self.assertRaises(ValueError):v.verify_runtime(text.replace(line,line.replace('running=no','running=yes')),'world')
    def test_duplicate_resolution(self):
        text=example_log('integration');line=next(l for l in text.splitlines() if 'PEDRO_OUTCOME' in l)
        with self.assertRaises(ValueError):v.verify_runtime(text.replace(line,line+'\n'+line),'integration')
    def test_truncated(self):
        with self.assertRaises(ValueError):v.verify_runtime(example_log('world').split('status=SUMMARY')[0],'world')
    def test_missing_exit(self):
        with self.assertRaises(ValueError):v.verify_runtime(example_log('world').replace('Engine exit requested','Exit unknown'),'world')
    def test_fatal_after_pass(self):
        with self.assertRaises(ValueError):v.verify_runtime(example_log('world')+'Fatal error: crash\n','world')
    def test_nonzero_exit(self):
        with self.assertRaises(ValueError):v.verify_runtime(example_log('world'),'world',137)

class ScopeAuditTests(unittest.TestCase):
    def setUp(self):
        self.status = b''
        self.ancestor = True
        def fake_git(*args):
            if args == ('merge-base', '--is-ancestor', audit.BASELINE, 'HEAD'):
                if not self.ancestor:
                    raise subprocess.CalledProcessError(1, ['git', *args])
                return b''
            if args == ('status', '--porcelain=v1', '-z', '--untracked-files=all', '--no-renames'):
                return self.status
            if args == ('diff', '--cached', '--check'):
                return b''
            raise AssertionError(args)
        patcher = patch.object(audit, 'git', side_effect=fake_git)
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_clean(self):
        audit.audit()

    def test_fully_staged(self):
        self.status = b'A  new file\0M  edited\0D  old file\0'
        audit.audit()

    def test_wrong_baseline(self):
        self.ancestor = False
        with self.assertRaises(subprocess.CalledProcessError):
            audit.audit()

    def test_untracked(self):
        self.status = b'?? unexpected\0'
        with self.assertRaises(ValueError):
            audit.audit()

    def test_unstaged(self):
        self.status = b' M Config/DefaultEngine.ini\0'
        with self.assertRaises(ValueError):
            audit.audit()

    def test_partly_staged(self):
        self.status = b'AM edited-after-stage\0'
        with self.assertRaises(ValueError):
            audit.audit()

    def test_conflict(self):
        self.status = b'UU unresolved\0'
        with self.assertRaises(ValueError):
            audit.audit()

class UnityTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.project=Path(self.tmp.name)
        self.dir=self.project/'Intermediate/Build/Linux/x64/UnrealEditor/Development/AI_Gavin_Project'
        self.dir.mkdir(parents=True)
        self.blob=self.dir/'Module.AI_Gavin_Project.cpp'
        self.write(self.blob,v.UNITY_SOURCES)
        self.log='Running command : UBT -ForceUnity -DisableAdaptiveUnity -gather\n[1/2] Compile Module.AI_Gavin_Project.cpp\n[2/2] Link libUnrealEditor-AI_Gavin_Project.so\nResult: Succeeded\n'
    def write(self,blob,sources):
        blob.write_text(''.join(f'#include "{self.project/name}"\n' for name in sources))
    def verify(self,log=None,rc=0):return v.verify_unity(self.log if log is None else log,self.project,rc)
    def test_unity_valid(self):self.assertEqual(len(self.verify()['source_membership']),12)
    def test_unity_split(self):
        self.write(self.blob,v.UNITY_SOURCES[:2]);self.write(self.dir/'Module.AI_Gavin_Project.2.cpp',v.UNITY_SOURCES[2:])
        self.assertEqual(len(self.verify(self.log.replace('[2/2] Link','[2/3] Compile [x64] Module.AI_Gavin_Project.2.cpp\n[3/3] Link'))['compiled_blobs']),2)
    def test_unity_no_compile(self):
        with self.assertRaises(ValueError):self.verify(self.log.replace('[1/2] Compile','Mention'))
    def test_unity_missing_flag(self):
        with self.assertRaises(ValueError):self.verify(self.log.replace(' -DisableAdaptiveUnity',''))
    def test_unity_missing_member(self):
        self.write(self.blob,v.UNITY_SOURCES[:-1])
        with self.assertRaises(ValueError):self.verify()
    def test_unity_stale_uncompiled_blob(self):
        self.write(self.blob,v.UNITY_SOURCES[:-1]);self.write(self.dir/'Module.AI_Gavin_Project.2.cpp',v.UNITY_SOURCES[-1:])
        with self.assertRaises(ValueError):self.verify()
    def test_unity_duplicate_member(self):
        self.write(self.blob,v.UNITY_SOURCES+v.UNITY_SOURCES[:1])
        with self.assertRaises(ValueError):self.verify()
    def test_unity_separate_compile(self):
        with self.assertRaises(ValueError):self.verify(self.log+'[3/3] Compile PedroAwarenessWorldTests.cpp\n')
    def test_unity_excluded(self):
        with self.assertRaises(ValueError):self.verify(self.log+'[Adaptive Build] Excluded from AI_Gavin_Project unity file: PedroAwarenessWorldTests.cpp\n')
    def test_unity_missing_link(self):
        with self.assertRaisesRegex(ValueError,'linker action'):self.verify(self.log.replace('Link libUnrealEditor','Mention libUnrealEditor'))
    def test_unity_process_failure(self):
        with self.assertRaises(ValueError):self.verify(rc=1)
    def test_unity_missing_generated_file(self):
        self.blob.unlink()
        with self.assertRaises(OSError):self.verify()
    def test_unity_wrong_path(self):
        self.write(self.blob,tuple('Unrelated/'+Path(n).name for n in v.UNITY_SOURCES))
        with self.assertRaises(ValueError):self.verify()


def attention_example(suite='attention-integration',resolution_frame=1):
    """Synthetic verifier fixture, not engine evidence. All samples are coherent."""
    import math
    rows=[v.TAG+f'status=BEGIN suite={suite} map={v.MAP} expected={v.ATT_COUNTS[suite]}']
    events={step:(op,trace,inspection) for step,op,trace,inspection in v.ATT_EVENTS[suite]}
    now=1.; yaw=0.; idle=0.; known=(0.,0.,0.); timestamp=0.; frozen=None
    player='PlayerA'; emitted=[]; acquired=[]; seq=0; rel=(0.,0.,64.); relrot=(0.,0.,0.); other_yaw=0.
    # Nominal endpoints (not intended to mimic engine timing exactly).
    targets={'TurnRight':45,'TurnLeft':-10,'StationaryConverges':-10,'VisibleMovingPlayer':35,'BoundedAngularSpeed':-25,
             'WrapShortestPath':-150,'OffsetEye':35,'StationaryBody':35,'BeforeHoldWhileTurning':45,'HoldOnLoss':60,'IdleReturn':0,'BeforeEarlyLoss':40,
             'EarlyReacquire':15,'LateLoss':0,'LateReacquire':-40,'BeginMovingBeforeDisable':-25,
             'ReenableFreshAnchor':20,'RepeatedEnableKeepsAnchor':20,'DestroyPlayer':-25,'ReplaceHidden':-25,
             'ReplaceVisible':5,'RepossessFreshAnchor':5,'RepossessNewSight':-30,'IndependentTurning':30,
             'DemoSetup':0,'DemoTurnAwayFromEvent':60,'DemoIdle':0,'DemoReacquire':40,'AutonomousSavedTurn':30}
    reset_yaw={'DefaultDisabled':0,'EnableWithoutSight':0,'WrapShortestPath':170,'OffsetEye':0,'BeforeEarlyLoss':0,'IndependentTurning':0,'DemoSetup':0}
    def vector(x):return ','.join(f'{a:.6f}' for a in x)
    def ray(a):return (math.cos(math.radians(a)),math.sin(math.radians(a)),0.)
    def pos(a):return tuple(x*700 for x in ray(a))
    def fmt(m):return ' '.join(f'{k}={z}' for k,z in m.items())
    for case,step,mode,awareness,evidence,sampled in v.ATT_PLANS[suite]:
        if step in reset_yaw:yaw=idle=reset_yaw[step]
        if step=='OffsetEye':rel=(20,80,84);relrot=(0,10,0)
        if step=='BeforeEarlyLoss':rel=(0,0,64);relrot=(0,0,0)
        if step in ('ReenableFreshAnchor','RepossessFreshAnchor'):idle=yaw
        if step=='DestroyPlayer':player='None'
        if step=='ReplaceHidden':player='PlayerB'
        if step=='DemoSetup':rows.append(v.TAG+'status=RESET_EVIDENCE')
        if step in events:
            op,trace,inspection=events[step];seq+=1;gid=f'{seq:032x}';emitted.append(gid)
            if trace=='Visible':acquired.append(gid)
            rows.append(f'PEDRO_DIRECT status={trace} kind=1 subject={op} opportunity={op} actualLOS=checked')
            if trace=='Visible':rows.append('PEDRO_DIRECT acquired=yes provenance=DirectObservation')
            rows.append(v.TAG+f'status=EVENT id={gid} subject={op} opportunity={op} expected={trace} inspection={inspection}')
        mode='TrackObserved' if mode=='*' else mode;awareness='Perceived' if awareness=='*' else awareness
        end=targets.get(step,yaw);delta=v.ang(yaw,end);travel=0.;total=0.;other_total=0.;actual=pos(end+relrot[1]);other_yaw=0.
        if step=='BeginMovingBeforeDisable':actual=pos(20)
        if step=='BeforeHoldWhileTurning':actual=pos(70)
        if step=='DefaultDisabled':actual=pos(40)
        if awareness=='Unaware':known=(0.,0.,0.);timestamp=0.
        elif awareness=='Perceived':known=actual;timestamp=now
        elif step in v.ATT_FREEZE_START:frozen=(known,timestamp)
        if awareness=='LostSight':known,timestamp=frozen;actual=pos(-65)
        if step in ('AttentionExpires','LateLoss','DemoIdle'):
            # Advance through stationary frames to the expiration boundary.
            pass
        def sample(status,frame=0,dt=0.,change=0.,other_change=0.):
            final=step=='SavedLanternSeeking' and resolution_frame is not None and frame>=resolution_frame
            point=known if mode in ('TrackObserved','HoldLastKnown') else None
            eye=(0.,0.,64.) # use horizontal direction for synthetic fixture
            d=dict(status=status,case=case,step=step,mode=mode,awareness=awareness,has='no' if awareness=='Unaware' else 'yes',
                look='-' if point is None else vector(point),known=vector(known),actual=vector(actual),time=f'{timestamp:.6f}',now=f'{now:.6f}',
                target='None' if awareness=='Unaware' else player,player=player,
                running='no' if step in ('SavedConfiguration','UnpossessClearsAttention','DestroyAttentionController') else 'yes',
                attention='no' if step in ('SavedConfiguration','DefaultDisabled','DisableMidTurn','UnpossessClearsAttention','DestroyAttentionController') else 'yes',
                body=vector((0,yaw,0)),root='0,0,0',eye=vector(eye),forward=vector(ray(yaw+relrot[1])),rel=vector(rel),relRot=vector(relrot),idle=idle,error=0,request=0,
                dt=f'{dt:.6f}',delta=f'{change:.6f}',total=f'{total:.6f}',travel=f'{travel:.6f}',frames=frame,evidence=evidence,sampled=sampled,
                emitted=','.join(emitted) or '-',acquired=','.join(acquired) or '-',final='yes' if final else 'no',progress=int(final),
                other='TrackObserved' if step=='IndependentTurning' else 'None',otherYaw=other_yaw,otherDt=dt if step=='IndependentTurning' else 0,
                otherDelta=other_change,otherTotal=other_total,otherAwareness='Perceived' if step=='IndependentTurning' else 'None',
                otherLook=vector(actual) if step=='IndependentTurning' else '-',otherEye=vector((0,700,0)) if step=='IndependentTurning' else '0,0,0',otherForward=vector(ray(other_yaw)) if step=='IndependentTurning' else '0,0,0',timerActive='no')
            rows.append(v.ATT_TAG+fmt(d))
        sample('STEP_BEGIN')
        count=0
        while True:
            count+=1;dt=.025 if step in ('HoldOnLoss','HiddenLateralMovement','EarlyLoss','DemoHold','DemoHiddenAction') else .1
            now+=dt
            if awareness=='Perceived':timestamp=now
            speed=v.att_rate(mode);change=max(-speed*dt,min(speed*dt,v.ang(yaw,end)));yaw=(yaw+change+180)%360-180;total+=change;travel+=abs(change)
            if step=='IndependentTurning':other_end=v.bearing((actual[0],actual[1]-700));other_change=max(-90*dt,min(90*dt,v.ang(other_yaw,other_end)));other_yaw+=other_change;other_total+=other_change
            else:other_change=0.
            if step=='SavedLanternSeeking' and count==resolution_frame:
                rows.append('PEDRO_OUTCOME policy=EvidenceDriven interpretation=Seeking sampledCore=3 requiredCore=3 resolved=yes actualLOS=checked')
            sample('FRAME',count,dt,change,other_change)
            enough=count>=4 and abs(v.ang(yaw,end))<.0001
            if step in ('AttentionExpires','LateLoss','DemoIdle'):enough=enough and now-timestamp>=2.01
            if enough:break
            if count>100:raise AssertionError(step)
        sample('CHECK' if case=='-' else 'PASS',count)
    rows.append(v.TAG+f'status=SUMMARY suite={suite} passed={v.ATT_COUNTS[suite]} expected={v.ATT_COUNTS[suite]} result=PASS')
    rows.append('LogCore: Engine exit requested (reason: GenericPlatform RequestExit)')
    return '\n'.join(rows)+'\n'

class AttentionReportTests(unittest.TestCase):
    def setUp(self): self.log=attention_example()
    def verify(self,log=None,rc=0,suite='attention-integration'):return v.verify_attention(self.log if log is None else log,suite,rc)
    def corrupt(self,old,new):
        self.assertIn(old,self.log);return self.log.replace(old,new,1)
    def test_valid_saved(self):
        result=self.verify();self.assertEqual(result['expected'],6);self.assertEqual(result['resolution_frame'],1)
    def test_deferred_third_frame(self):
        self.assertEqual(self.verify(attention_example(resolution_frame=3))['resolution_frame'],3)
    def change_completion(self,case,status,old,new,frame=None):
        rows=self.log.splitlines()
        matches=[i for i,x in enumerate(rows) if f'status={status} case={case} ' in x and (frame is None or f' frames={frame} ' in x)]
        self.assertTrue(matches);i=matches[0];self.assertIn(old,rows[i]);rows[i]=rows[i].replace(old,new,1)
        return '\n'.join(rows)+'\n'
    def test_premature_j06_begin(self):
        with self.assertRaisesRegex(ValueError,'premature attention completion'):
            self.verify(self.change_completion('J06','STEP_BEGIN','final=no progress=0','final=yes progress=1'))
    def test_premature_j05_frame(self):
        with self.assertRaisesRegex(ValueError,'premature attention completion'):
            self.verify(self.change_completion('J05','FRAME','final=no progress=0','final=yes progress=1'))
    def test_incoherent_resolution_flag(self):
        with self.assertRaisesRegex(ValueError,'incoherent attention progression'):
            self.verify(self.change_completion('J06','FRAME','final=yes progress=1','final=yes progress=0',1))
    def test_incoherent_progress_first(self):
        with self.assertRaisesRegex(ValueError,'incoherent attention progression'):
            self.verify(self.change_completion('J06','FRAME','final=yes progress=1','final=no progress=1',1))
    def test_double_progress(self):
        with self.assertRaisesRegex(ValueError,'incoherent attention progression'):
            self.verify(self.change_completion('J06','FRAME','final=yes progress=1','final=yes progress=2',2))
    def test_reversed_completion(self):
        with self.assertRaisesRegex(ValueError,'attention completion reversed'):
            self.verify(self.change_completion('J06','FRAME','final=yes progress=1','final=no progress=0',2))
    def test_missing_completion(self):
        # Retain the claimed outcome but never observe progression in frames or terminal.
        with self.assertRaisesRegex(ValueError,'missing deferred attention completion'):
            self.verify(self.log.replace('final=yes progress=1','final=no progress=0'))
    def test_outcome_before_j06(self):
        rows=self.log.splitlines();i=next(i for i,x in enumerate(rows) if 'PEDRO_OUTCOME' in x);outcome=rows.pop(i)
        i=next(i for i,x in enumerate(rows) if 'status=STEP_BEGIN case=J06' in x);rows.insert(i,outcome)
        with self.assertRaisesRegex(ValueError,'outcome outside observed J06 transition'):
            self.verify('\n'.join(rows))
    def test_outcome_after_transition(self):
        rows=self.log.splitlines();i=next(i for i,x in enumerate(rows) if 'PEDRO_OUTCOME' in x);outcome=rows.pop(i)
        i=next(i for i,x in enumerate(rows) if 'status=FRAME case=J06' in x);rows.insert(i+1,outcome)
        with self.assertRaisesRegex(ValueError,'outcome outside observed J06 transition'):
            self.verify('\n'.join(rows))
    def test_j06_evidence_mutation(self):
        with self.assertRaisesRegex(ValueError,'attention event history mismatch'):
            self.verify(self.change_completion('J06','FRAME','evidence=3 sampled=3','evidence=2 sampled=3',2))
    def test_j06_pose_mutation(self):
        with self.assertRaisesRegex(ValueError,'observer translated'):
            self.verify(self.change_completion('J06','FRAME','root=0,0,0','root=10,0,0',2))
    def test_valid_world(self):self.assertEqual(self.verify(attention_example('attention'),suite='attention')['expected'],24)
    def test_missing_frame(self):
        row=next(x for x in self.log.splitlines() if 'status=FRAME case=J02' in x)
        with self.assertRaises(ValueError):self.verify(self.log.replace(row+'\n','',1))
    def test_invalid_mode(self):
        with self.assertRaises(ValueError):self.verify(self.corrupt('mode=TrackObserved','mode=Fabricated'))
    def test_nonfinite(self):
        with self.assertRaises(ValueError):self.verify(self.corrupt('error=0','error=nan'))
    def test_excess_yaw_delta(self):
        row=next(x for x in self.log.splitlines() if 'status=FRAME case=J02' in x)
        with self.assertRaises(ValueError):self.verify(self.log.replace(row,row.replace('delta=9.000000','delta=50.000000'),1))
    def test_coherent_overrate(self):
        rows=[v.att_fields(x) for x in self.log.splitlines() if 'case=J02 ' in x and ('status=STEP_BEGIN' in x or 'status=FRAME' in x)]
        prev=rows[0]; m=dict(rows[1]);m.update(body=(0.,50.,0.),delta=50.,total=50.,travel=50.)
        with self.assertRaisesRegex(ValueError,'physical yaw exceeds rate'):v.att_frame(m,prev,prev)
    def test_duplicate_resolution(self):
        row=next(x for x in self.log.splitlines() if 'PEDRO_OUTCOME' in x)
        with self.assertRaises(ValueError):self.verify(self.log.replace(row,row+'\n'+row,1))
    def test_root_drift(self):
        row=next(x for x in self.log.splitlines() if 'status=FRAME case=J02' in x)
        with self.assertRaises(ValueError):self.verify(self.log.replace(row,row.replace('root=0,0,0','root=10,0,0'),1))
    def test_hidden_location_leak(self):
        text=attention_example('attention');row=next(x for x in text.splitlines() if 'status=FRAME case=A13' in x)
        import re
        corrupted=re.sub(r'known=[^ ]+','known=999,999,999',row)
        with self.assertRaisesRegex(ValueError,'hidden location'):self.verify(text.replace(row,corrupted,1),suite='attention')
    def test_hidden_timestamp_leak(self):
        text=attention_example('attention');row=next(x for x in text.splitlines() if 'status=FRAME case=A13' in x)
        import re
        corrupted=re.sub(r' time=[^ ]+',' time=0',row)
        with self.assertRaisesRegex(ValueError,'hidden location'):self.verify(text.replace(row,corrupted,1),suite='attention')
    def test_wrong_event_id(self):
        row=next(x for x in self.log.splitlines() if 'status=PASS case=J04' in x)
        with self.assertRaises(ValueError):self.verify(self.log.replace(row,row.replace('acquired='+f'{1:032x}','acquired='+f'{2:032x}'),1))
    def test_missing_case(self):
        with self.assertRaises(ValueError):self.verify('\n'.join(x for x in self.log.splitlines() if 'status=PASS case=J05' not in x))
    def test_fatal_after_pass(self):
        with self.assertRaises(ValueError):self.verify(self.log+'Fatal error: crash\n')
    def test_nonzero_exit(self):
        with self.assertRaises(ValueError):self.verify(rc=137)

class NonUnityTests(unittest.TestCase):
    def setUp(self):
        self.log='Running command : UBT -DisableUnity -DisableAdaptiveUnity -gather\n'+''.join(f'[{i+1}/13] Compile {Path(s).name}\n' for i,s in enumerate(v.UNITY_SOURCES))+'[13/13] Link libUnrealEditor-AI_Gavin_Project.so\nResult: Succeeded\n'
    def test_valid(self):self.assertEqual(v.verify_non_unity(self.log,0)['expected'],12)
    def test_missing_source(self):
        with self.assertRaises(ValueError):v.verify_non_unity(self.log.replace('Compile PedroAttentionPolicy.cpp','Mention PedroAttentionPolicy.cpp'),0)
    def test_unity_contamination(self):
        with self.assertRaises(ValueError):v.verify_non_unity(self.log+'[8/8] Compile Module.AI_Gavin_Project.cpp\n',0)
    def test_missing_flags(self):
        with self.assertRaises(ValueError):v.verify_non_unity(self.log.replace('-DisableUnity','-ForceUnity'),0)
    def test_missing_link(self):
        with self.assertRaisesRegex(ValueError,'linker action'):v.verify_non_unity(self.log.replace('Link libUnrealEditor','Mention libUnrealEditor'),0)
    def test_nonzero_exit(self):
        with self.assertRaises(ValueError):v.verify_non_unity(self.log,1)


class EngineUnityFlagTests(unittest.TestCase):
    # Minimal source excerpts based on the installed UE 5.8.2 diagnostics.
    def setUp(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        self.engine=Path(temp.name)
        configuration=self.engine/'Engine/Source/Programs/UnrealBuildTool/Configuration'
        self.descriptor=configuration/'Descriptors/TargetDescriptor.cs'
        self.rules=configuration/'Rules/TargetRules.cs'
        self.descriptor.parent.mkdir(parents=True);self.rules.parent.mkdir(parents=True)
        self.descriptor.write_text('[CommandLine("-DisableUnity", Value = "false")]\n'
                                   'public bool bUseUnityBuild { get; set; } = true;\n')
        self.rules.write_text('public bool bUseUnityBuild\n{\nget => bUseUnityBuildOverride ?? IntermediateEnvironment.IsUnity();\n}\n'
                              '[CommandLine("-ForceUnity")]\n[XmlConfigFile(Category = "BuildConfiguration")]\n'
                              'public bool bForceUnityBuild { get; set; }\n'
                              '[CommandLine("-DisableAdaptiveUnity", Value = "false")]\n'
                              '[XmlConfigFile(Category = "BuildConfiguration")]\n'
                              'public bool bUseAdaptiveUnityBuild { get; set; } = true;\n')
    def test_descriptor_layout(self):
        audit.inspect_unity_flags(self.engine)
    def test_missing_descriptor(self):
        self.descriptor.unlink()
        with self.assertRaises(OSError):audit.inspect_unity_flags(self.engine)
    def test_wrong_nonunity_value(self):
        self.descriptor.write_text(self.descriptor.read_text().replace('"false"','"true"'))
        with self.assertRaisesRegex(ValueError,'-DisableUnity'):audit.inspect_unity_flags(self.engine)
    def test_unrelated_nonunity_property(self):
        self.descriptor.write_text(self.descriptor.read_text().replace('bUseUnityBuild','bUnrelated'))
        with self.assertRaisesRegex(ValueError,'-DisableUnity'):audit.inspect_unity_flags(self.engine)
    def test_missing_adaptive(self):
        self.rules.write_text(self.rules.read_text().replace('-DisableAdaptiveUnity','-Unrelated'))
        with self.assertRaisesRegex(ValueError,'-DisableAdaptiveUnity'):audit.inspect_unity_flags(self.engine)
    def test_missing_force(self):
        self.rules.write_text(self.rules.read_text().replace('-ForceUnity','-Unrelated'))
        with self.assertRaisesRegex(ValueError,'-ForceUnity'):audit.inspect_unity_flags(self.engine)



def navigation_example(suite='navigation-probe'):
    """Synthetic log to validate the checker, not proof of Unreal execution."""
    from math import hypot,cos,sin,radians
    saved=suite=='navigation-integration'
    rows=[v.NAV_TAG+f'status=BEGIN suite={suite} map={v.MAP} expected={v.NAV_COUNTS[suite]}']
    emitted=[];acquired=[];now=1.;x=-900. if saved else 100000.;y=0. if saved else 100000.;z=90. if saved else 5000.
    yaw=0.;controller='None' if saved else 'Controller1';submissions=0;rid=0;player=x+900;plan=-1.;sight=0.;route_point=0.;goal=0.;travel=0.;yawtravel=0.;final=False
    events={n:(op,trace,ins) for n,op,trace,ins in v.NAV_EVENTS[suite]}
    def serial(k,val):
        if isinstance(val,bool):return 'yes' if val else 'no'
        if isinstance(val,(tuple,list)):
            if k in ('emitted','acquired'):return ','.join(val) or '-'
            return ','.join(f'{n:.6f}' for n in val)
        if isinstance(val,float):return f'{val:.6f}'
        return str(val)
    def line(status,case,step,dt,active,seen,root_start,velocity=0.,lost=False):
        running=controller not in ('None','Controller1')
        mode='FollowObserved' if active else ('Idle' if running else 'Disabled')
        fields=dict(status=status,case=case,step=step,now=now,dt=dt,root=(x,y,z),start=(root_start,y,z),velocity=(velocity,0,0),body=(0,yaw,0),eye=(x,y,z+64),forward=(cos(radians(yaw)),sin(radians(yaw)),0),rel=(0,0,64),relRot=(0,0,0),travel=travel,yawTravel=yawtravel,maxSide=0.,mode=mode,reason=5 if active else 2,running=running,attention=2 if saved and running else 0,active=active,id=rid if active else 0,valid=active,goal=(goal,y,z-88) if active else (0,0,0),sighting=(route_point,y,z) if sight else (0,0,0),sightTime=sight,planTime=plan,length=350. if active else 0.,submissions=submissions,controller=controller,speedLimit=180. if running else 600.,accel=500. if running else 2048.,brake=600. if running else 2048.,accelPaths=running,awareness='Perceived' if seen else 'Unaware',has=seen,known=(player,y,z) if seen else (0,0,0),actual=(player,y,z),time=now if seen else 0.,target='Player' if seen else 'None',player='Player',grounded=True,evidence=len(acquired),sampled=len(acquired),emitted=emitted,acquired=acquired,final=final,progress=int(final),otherId=0,otherRoot=(0,0,0))
        if lost:
            fields.update(awareness='LostSight',has=True,known=(player,y,z),target='Player',attention=3,time=last_clear)
        return v.NAV_TAG+' '.join(f'{k}={serial(k,w)}' for k,w in fields.items())
    last_clear=0.
    for case,step in v.NAV_PLANS[suite]:
        if step in events:
            op,trace,ins=events[step];eid=f'{len(emitted)+1:032x}';emitted.append(eid)
            rows.append(f'PEDRO_DIRECT status={trace} opportunity={op} actualLOS=checked')
            if trace=='Visible':acquired.append(eid);rows.append(f'PEDRO_DIRECT acquired=yes opportunity={op} provenance=DirectObservation')
            rows.append(v.TAG+f'status=EVENT id={eid} subject={op} opportunity={op} expected={trace} inspection={ins}')
        moving=step in ('NativeFollow','SavedAutonomousFollow','SavedRelocationNoReplay')
        if moving:
            controller='Controller2';player=x+(1200 if not saved else 800.)
        if step=='SavedAutonomousFollow':yaw=-25.
        seen=step!='SavedConfigurationAndNavmesh'
        if step=='NativeFollow':seen=False
        start=x;travel=0.;yawtravel=0.;active=False
        now+=.1;rows.append(line('STEP_BEGIN',case,step,0.,False,seen,start,lost=step in ('SavedRadioOccluded','SavedRestoreSightNoReplay')))
        if step=='DisabledWithNavmesh':rows.append(v.NAV_TAG+'status=CAPABILITY dynamic=yes connected=yes disconnected=yes start=100000,100000,4912 end=100700,100000,4912 island=102100,100000,4912 length=700')
        if step=='SavedConfigurationAndNavmesh':rows.append(v.NAV_TAG+'status=SAVED_READY original=yes dynamic=yes projected=-900,0,0')
        n=7 if step in ('NativeFollow','SavedRestoreSightNoReplay') else 22 if moving else 5
        for i in range(n):
            now+=.1
            if moving and i==0:
                submissions+=1;rid=100+submissions;plan=now;sight=now;route_point=player;goal=player-450
            if moving and (not saved or i<18):
                active=True;x+=18.;travel+=18.;velocity=180.
                if step=='SavedAutonomousFollow' and i<5:yaw+=5.;yawtravel+=5.
            else:active=False;velocity=0.
            if step=='SavedLanternSeeking' and i==1:
                rows.append('PEDRO_OUTCOME policy=EvidenceDriven interpretation=Seeking sampledCore=3 requiredCore=3 resolved=yes actualLOS=checked');final=True
            lost=step=='SavedRadioOccluded' or (step=='SavedRestoreSightNoReplay' and i<2)
            rows.append(line('FRAME',case,step,.1,active,step!='SavedConfigurationAndNavmesh',start,velocity,lost=lost))
            if not lost:last_clear=now
        rows.append(line('CHECK' if case=='-' else 'PASS',case,step,0.,active,step!='SavedConfigurationAndNavmesh',start,velocity,lost=step=='SavedRadioOccluded'))
    rows.append(v.NAV_TAG+f'status=SUMMARY suite={suite} passed={v.NAV_COUNTS[suite]} expected={v.NAV_COUNTS[suite]} result=PASS')
    rows.append('LogCore: Engine exit requested (reason: GenericPlatform RequestExit)')
    return '\n'.join(rows)+'\n'

class NavigationReportTests(unittest.TestCase):
    def verify(self,text=None,suite='navigation-probe',rc=0):return v.verify_navigation(navigation_example(suite) if text is None else text,suite,rc)
    def alter(self,key,value,step='NativeFollow',index=0,suite='navigation-probe',status='FRAME'):
        import re
        rows=navigation_example(suite).splitlines();matches=[i for i,l in enumerate(rows) if v.NAV_TAG+f'status={status} ' in l and f' step={step} ' in l];p=matches[index]
        rows[p]=re.sub(r'(?<!\S)'+re.escape(key)+'=[^ ]+',key+'='+str(value),rows[p]);return '\n'.join(rows)+'\n'
    def reject(self,text,suite='navigation-probe'):
        with self.assertRaises((ValueError,KeyError)):self.verify(text,suite)
    def test_valid_probe(self):self.assertEqual(self.verify()['cases'],['N01','N04'])
    def test_valid_saved(self):self.assertEqual(self.verify(suite='navigation-integration')['resolution'],1)
    def test_missing_case(self):self.reject(navigation_example().replace('case=N04','case=N05'))
    def test_duplicate_frame(self):
        t=navigation_example();row=next(l for l in t.splitlines() if 'status=FRAME case=N04' in l);self.reject(t.replace(row,row+'\n'+row,1))
    def test_missing_frame(self):
        t=navigation_example();row=next(l for l in t.splitlines() if 'status=FRAME case=N04' in l);self.reject(t.replace(row+'\n','',1))
    def test_nonfinite(self):self.reject(self.alter('now','nan'))
    def test_speed(self):self.reject(self.alter('velocity','181,181,0'))
    def test_displacement(self):self.reject(self.alter('root','101000,100000,5000'))
    def test_unowned_yaw(self):self.reject(self.alter('body','0,40,0'))
    def test_eye_relative_change(self):self.reject(self.alter('rel','0,50,64'))
    def test_invalid_path(self):self.reject(self.alter('valid','no'))
    def test_excessive_path(self):self.reject(self.alter('length','2201'))
    def test_standoff(self):self.reject(self.alter('goal','101200,100000,4912'))
    def test_stale_provenance(self):self.reject(self.alter('sightTime','0.1'))
    def test_hidden_new_route(self):self.reject(self.alter('awareness','LostSight'))
    def test_fixed_goal_mutation(self):self.reject(self.alter('goal','100760,100000,4912',index=1))
    def test_hidden_position_goal(self):
        text=self.alter('sighting','101100,100100,5000')
        rows=text.splitlines();i=next(i for i,l in enumerate(rows) if 'status=FRAME case=N04' in l)
        import re
        rows[i]=re.sub(r'(?<!\S)goal=[^ ]+','goal=100650,100100,4912',rows[i])
        self.reject('\n'.join(rows)+'\n')
    def test_orphan_request(self):self.reject(self.alter('active','no'))
    def test_invalid_rate(self):
        import re
        rows=navigation_example().splitlines();matches=[i for i,l in enumerate(rows) if 'status=FRAME case=N04' in l];i=matches[1]
        for k,val in [('id','102'),('submissions','2')]:rows[i]=re.sub(r'(?<!\S)'+k+'=[^ ]+',k+'='+val,rows[i])
        self.reject('\n'.join(rows)+'\n')
    def test_wrong_event(self):self.reject(navigation_example('navigation-integration').replace('opportunity=Inspect_Radio','opportunity=Inspect_Wrong',1),'navigation-integration')
    def test_missing_event(self):
        t=navigation_example('navigation-integration');row=next(l for l in t.splitlines() if 'status=EVENT' in l);self.reject(t.replace(row+'\n','',1),'navigation-integration')
    def test_premature_completion(self):self.reject(self.alter('final','yes',step='SavedLanternSeeking',suite='navigation-integration',status='STEP_BEGIN'),'navigation-integration')
    def test_missing_outcome(self):
        t=navigation_example('navigation-integration');row=next(l for l in t.splitlines() if 'PEDRO_OUTCOME ' in l);self.reject(t.replace(row+'\n','',1),'navigation-integration')
    def test_process_and_fatal(self):
        with self.assertRaises(ValueError):self.verify(rc=1)
        self.reject(navigation_example()+'Fatal error: simulated\n')



class NavigationReplacementTests(unittest.TestCase):
    # Actual compact endpoint records from the successful UE navigation run.
    # Intermediate frames below are synthetic, not an Unreal replay.
    def setUp(self):
        self.prior=v.nav_sample(v.nav_fields(v.NAV_TAG+'status=PASS case=N23 step=ReenableFresh now=30.237866 dt=0.000000 root=100116.840,100000.000,5002.150 start=100071.992,100000.000,5002.150 velocity=180.000,0.000,0.000 body=0.000000,0.000000,-0.000000 eye=100116.840,100000.000,5066.150 forward=1.000,0.000,0.000 rel=0.000,0.000,64.000 relRot=0.000000,0.000000,0.000000 travel=44.849 yawTravel=0.000 maxSide=0.000 mode=FollowObserved reason=5 running=yes attention=0 active=yes id=16 valid=yes goal=100850.000,100000.000,4920.000 sighting=101300.000,100000.000,5000.000 sightTime=29.787857 planTime=29.812858 length=778.008 submissions=1 controller=PedroObserverAIController_14 speedLimit=180.000 accel=500.000 brake=600.000 accelPaths=yes awareness=Perceived has=yes known=101300.000,100000.000,5000.000 actual=101300.000,100000.000,5000.000 time=30.187865 target=BP_MainPlayerCharacter_C_0 player=BP_MainPlayerCharacter_C_0 grounded=yes evidence=0 sampled=0 emitted=- acquired=- final=no progress=0 otherId=0 otherRoot=0.000,0.000,0.000'))
        self.begin_raw=v.nav_fields(v.NAV_TAG+'status=STEP_BEGIN case=N24 step=PlayerReplacement now=30.246200 dt=0.000000 root=100118.341,100000.000,5002.150 start=100118.341,100000.000,5002.150 velocity=180.000,0.000,0.000 body=0.000000,0.000000,-0.000000 eye=100118.341,100000.000,5066.150 forward=1.000,0.000,0.000 rel=0.000,0.000,64.000 relRot=0.000000,0.000000,0.000000 travel=0.000 yawTravel=0.000 maxSide=0.000 mode=FollowObserved reason=5 running=yes attention=0 active=yes id=16 valid=yes goal=100850.000,100000.000,4920.000 sighting=101300.000,100000.000,5000.000 sightTime=29.787857 planTime=29.812858 length=778.008 submissions=1 controller=PedroObserverAIController_14 speedLimit=180.000 accel=500.000 brake=600.000 accelPaths=yes awareness=Perceived has=yes known=101300.000,100000.000,5000.000 actual=99418.341,100000.000,5002.150 time=30.187865 target=None player=PedroObserverCharacter_1 grounded=yes evidence=0 sampled=0 emitted=- acquired=- final=no progress=0 otherId=0 otherRoot=0.000,0.000,0.000')
        self.end_raw=v.nav_fields(v.NAV_TAG+'status=PASS case=N24 step=PlayerReplacement now=30.687876 dt=0.000000 root=100118.375,100000.000,5002.150 start=100118.341,100000.000,5002.150 velocity=0.000,0.000,0.000 body=0.000000,0.000000,-0.000000 eye=100118.375,100000.000,5066.150 forward=1.000,0.000,0.000 rel=0.000,0.000,64.000 relRot=0.000000,0.000000,0.000000 travel=0.035 yawTravel=0.000 maxSide=0.000 mode=Idle reason=2 running=yes attention=0 active=no id=0 valid=no goal=0.000,0.000,0.000 sighting=0.000,0.000,0.000 sightTime=0.000000 planTime=-1.000000 length=0.000 submissions=1 controller=PedroObserverAIController_14 speedLimit=180.000 accel=500.000 brake=600.000 accelPaths=yes awareness=Unaware has=no known=0.000,0.000,0.000 actual=99418.341,100000.000,5002.150 time=0.000000 target=None player=PedroObserverCharacter_1 grounded=yes evidence=0 sampled=0 emitted=- acquired=- final=no progress=0 otherId=0 otherRoot=0.000,0.000,0.000')
        self.begin=v.nav_sample(self.begin_raw,replacement_prior=self.prior)
    def frame(self,**changes):
        d=dict(self.end_raw,status='FRAME',now=f"{self.begin['now']+.05:.6f}",dt='0.05')
        d.update(changes);return d
    def sample(self,d,prev=None):
        return v.nav_sample(d,replacement_prior=self.prior,replacement_start=self.begin,replacement_prev=self.begin if prev is None else prev)
    def test_recorded_replacement_begin_and_pass(self):
        clean=self.sample(self.frame())
        self.assertEqual(self.sample(self.end_raw,clean)['awareness'],'Unaware')
    def test_stale_snapshot_while_route_cancelled_then_clear(self):
        stale=self.sample(self.frame(awareness='Perceived',has='yes',known=self.begin_raw['known'],time=self.begin_raw['time']))
        self.assertFalse(stale['active']);self.assertEqual(self.sample(self.frame(),stale)['awareness'],'Unaware')
    def test_ordinary_sample_stays_strict(self):
        with self.assertRaisesRegex(ValueError,'missing spatial memory'):v.nav_sample(self.begin_raw)
    def test_begin_cannot_change_route(self):
        with self.assertRaisesRegex(ValueError,'changed old owned route'):v.nav_sample(dict(self.begin_raw,id='999'),replacement_prior=self.prior)
    def test_begin_requires_new_identity(self):
        with self.assertRaisesRegex(ValueError,'identity'):v.nav_sample(dict(self.begin_raw,player=self.prior['player']),replacement_prior=self.prior)
    def test_retained_route_after_setup(self):
        with self.assertRaisesRegex(ValueError,'retained or issued'):self.sample(dict(self.begin_raw,status='FRAME',now=str(self.begin['now']+.01),dt='0.01'))
    def test_new_submission_after_setup(self):
        with self.assertRaisesRegex(ValueError,'submission count'):self.sample(self.frame(submissions='2'))
    def test_mutated_stale_memory(self):
        with self.assertRaisesRegex(ValueError,'changed memory'):self.sample(self.frame(awareness='Perceived',has='yes',known='100000,0,0',time=self.begin_raw['time']))
    def test_mutated_stale_time(self):
        with self.assertRaisesRegex(ValueError,'timestamp changed'):self.sample(self.frame(awareness='Perceived',has='yes',known=self.begin_raw['known'],time=str(self.begin['time']+.01)))
    def test_expired_refresh_window(self):
        with self.assertRaisesRegex(ValueError,'bounded refresh window'):self.sample(self.frame(awareness='Perceived',has='yes',known=self.begin_raw['known'],time=self.begin_raw['time'],now=str(self.begin['now']+.25)))
    def test_no_resurrection_after_clear(self):
        clean=self.sample(self.frame())
        with self.assertRaisesRegex(ValueError,'bounded refresh window'):self.sample(self.frame(awareness='Perceived',has='yes',known=self.begin_raw['known'],time=self.begin_raw['time']),clean)
    def test_no_stale_pass(self):
        with self.assertRaisesRegex(ValueError,'bounded refresh window'):self.sample(dict(self.end_raw,awareness='Perceived',has='yes',known=self.begin_raw['known'],time=self.begin_raw['time']))



class SavedNavigationHeadingTests(unittest.TestCase):
    def setUp(self):
        rows=navigation_example('navigation-integration').splitlines()
        self.start=v.nav_sample(v.nav_fields(next(l for l in rows if 'status=STEP_BEGIN case=K02' in l)))
        self.end=v.nav_sample(v.nav_fields(next(l for l in rows if 'status=PASS case=K02' in l)))
    def test_real_failed_straight_route(self):
        start=v.nav_sample(v.nav_fields(v.NAV_TAG+'status=STEP_BEGIN case=K02 step=SavedAutonomousFollow now=0.288879 dt=0.000000 root=-935.211,0.000,89.650 start=-935.211,0.000,89.650 velocity=0.000,0.000,0.000 body=0.000000,0.000000,-0.000000 eye=-935.211,0.000,153.650 forward=0.919,0.000,-0.394 rel=0.000,0.000,64.000 relRot=-23.198591,0.000000,-0.000000 travel=0.000 yawTravel=0.000 maxSide=0.000 mode=Idle reason=2 running=yes attention=1 active=no id=0 valid=no goal=0.000,0.000,0.000 sighting=0.000,0.000,0.000 sightTime=0.000000 planTime=-1.000000 length=0.000 submissions=0 controller=PedroObserverAIController_0 speedLimit=180.000 accel=500.000 brake=600.000 accelPaths=yes awareness=Unaware has=no known=0.000,0.000,0.000 actual=-135.211,0.000,90.000 time=0.000000 target=None player=BP_MainPlayerCharacter_C_0 grounded=yes evidence=0 sampled=0 emitted=- acquired=- final=no progress=0 otherId=0 otherRoot=0.000,0.000,0.000'))
        end=v.nav_sample(v.nav_fields(v.NAV_TAG+'status=DIAGNOSTIC case=K02 step=SavedAutonomousFollow now=35.290206 dt=0.000000 root=-633.811,0.000,89.650 start=-935.211,0.000,89.650 velocity=0.000,0.000,0.000 body=0.000000,0.000000,-0.000000 eye=-633.811,0.000,153.650 forward=0.919,0.000,-0.394 rel=0.000,0.000,64.000 relRot=-23.198591,0.000000,-0.000000 travel=301.400 yawTravel=0.000 maxSide=0.000 mode=Idle reason=4 running=yes attention=2 active=no id=0 valid=no goal=0.000,0.000,0.000 sighting=-135.211,0.000,90.000 sightTime=0.297213 planTime=0.305546 length=0.000 submissions=1 controller=PedroObserverAIController_0 speedLimit=180.000 accel=500.000 brake=600.000 accelPaths=yes awareness=Perceived has=yes known=-135.211,0.000,90.000 actual=-135.211,0.000,90.000 time=35.290206 target=BP_MainPlayerCharacter_C_0 player=BP_MainPlayerCharacter_C_0 grounded=yes evidence=0 sampled=0 emitted=- acquired=- final=no progress=0 otherId=0 otherRoot=0.000,0.000,0.000'))
        self.assertAlmostEqual(end['travel'],301.4)
        with self.assertRaisesRegex(ValueError,'off-axis'):v.verify_saved_follow(start,end)
    def test_valid_seed_and_convergence(self):
        v.verify_saved_follow(self.start,self.end)
    def test_wrong_final_heading(self):
        with self.assertRaisesRegex(ValueError,'converge'):v.verify_saved_follow(self.start,dict(self.end,forward=(0,1,0)))
    def test_no_net_yaw(self):
        with self.assertRaisesRegex(ValueError,'actual movement/turn'):v.verify_saved_follow(self.start,dict(self.end,body=self.start['body']))
    def test_target_changed(self):
        with self.assertRaisesRegex(ValueError,'target moved'):v.verify_saved_follow(self.start,dict(self.end,actual=(1,1,1)))
    def test_preexisting_request(self):
        with self.assertRaisesRegex(ValueError,'already had a route'):v.verify_saved_follow(dict(self.start,submissions=1),self.end)

class SavedNavigationPairTests(unittest.TestCase):
    def setUp(self):
        self.rows=navigation_example('navigation-integration').splitlines()
        def sample(status,step):
            return v.nav_sample(v.nav_fields(next(l for l in self.rows if f'status={status} ' in l and f' step={step} ' in l)))
        self.prior=sample('PASS','SavedRadioOccluded')
        self.start=sample('STEP_BEGIN','SavedRelocationNoReplay')
        self.end=sample('CHECK','SavedRelocationNoReplay')
    def test_valid_second_leg(self):
        v.verify_saved_continuity(self.prior,self.start);v.verify_saved_relocation(self.start,self.end)
    def test_observer_teleport_between_steps(self):
        with self.assertRaisesRegex(ValueError,'pose changed'):v.verify_saved_continuity(self.prior,dict(self.start,root=(0,0,90)))
    def test_controller_reset(self):
        with self.assertRaisesRegex(ValueError,'identity changed'):v.verify_saved_continuity(self.prior,dict(self.start,controller='Other'))
    def test_preissued_second_route(self):
        with self.assertRaisesRegex(ValueError,'setup changed'):v.verify_saved_continuity(self.prior,dict(self.start,submissions=self.start['submissions']+1))
    def test_same_player_position(self):
        with self.assertRaisesRegex(ValueError,'new distant'):v.verify_saved_continuity(self.prior,dict(self.start,actual=self.prior['actual']))
    def test_no_new_request(self):
        with self.assertRaisesRegex(ValueError,'new movement request'):v.verify_saved_relocation(self.start,dict(self.end,submissions=self.start['submissions']))
    def test_travel_without_net_displacement(self):
        with self.assertRaisesRegex(ValueError,'net displacement'):v.verify_saved_relocation(self.start,dict(self.end,root=self.start['root']))
    def test_second_target_moved(self):
        with self.assertRaisesRegex(ValueError,'changed during movement'):v.verify_saved_relocation(self.start,dict(self.end,actual=(0,0,0)))
    def test_second_heading_not_converged(self):
        with self.assertRaisesRegex(ValueError,'converge'):v.verify_saved_relocation(self.start,dict(self.end,forward=(0,1,0)))
    def test_full_report_rejects_teleport(self):
        t=NavigationReportTests().alter('controller','Other',step='SavedRelocationNoReplay',suite='navigation-integration',status='STEP_BEGIN')
        with self.assertRaisesRegex(ValueError,'identity changed'):v.verify_navigation(t,'navigation-integration')

class SavedNavigationRestoreTests(unittest.TestCase):
    suite='navigation-integration'
    step='SavedRestoreSightNoReplay'
    def setUp(self):
        self.text=navigation_example(self.suite)
        self.result=v.verify_navigation(self.text,self.suite)
        self.end=self.result['steps'][self.step]
        self.start=v.nav_sample(v.nav_fields(next(l for l in self.text.splitlines() if 'status=STEP_BEGIN ' in l and f' step={self.step} ' in l)))
    def test_lost_then_reacquired(self):
        self.assertEqual(self.start['awareness'],'LostSight');self.assertTrue(v.saved_restore_ready(self.start,self.end))
    def test_stale_sighting_not_ready(self):
        self.assertFalse(v.saved_restore_ready(self.start,dict(self.end,time=self.start['time'])))
    def test_not_tracking_not_ready(self):
        self.assertFalse(v.saved_restore_ready(self.start,dict(self.end,attention=3)))
    def test_new_request_rejected(self):
        with self.assertRaisesRegex(ValueError,'submitted a route'):v.verify_saved_restore_sample(self.start,dict(self.end,submissions=self.start['submissions']+1))
    def test_player_motion_rejected(self):
        with self.assertRaisesRegex(ValueError,'player position'):v.verify_saved_restore_sample(self.start,dict(self.end,actual=(0,0,0)))
    def test_evidence_replay_rejected(self):
        with self.assertRaisesRegex(ValueError,'altered evidence'):v.verify_saved_restore_sample(self.start,dict(self.end,evidence=2))
    def test_missing_restore_step(self):
        with self.assertRaisesRegex(ValueError,'case order'):v.verify_navigation('\n'.join(l for l in self.text.splitlines() if f' step={self.step} ' not in l),self.suite)
    def test_unstable_restore_rejected(self):
        t=NavigationReportTests().alter('attention','3',step=self.step,index=-2,suite=self.suite)
        with self.assertRaisesRegex(ValueError,'stable fresh perception'):v.verify_navigation(t,self.suite)

if __name__=='__main__':unittest.main()

# Synthetic context reports: these test the verifier, not Unreal behaviour.
def context_sample(**overrides):
    d={k:'0,0,0' for k in v.CTX_VECTORS}
    d.update({k:'0' for k in v.CTX_FLOATS+v.CTX_INTS})
    d.update({k:'no' for k in v.CTX_BOOLS})
    d.update({k:'None' for k in v.CTX_STRINGS})
    d.update(status='FRAME',case='P01',step='ProbeFollow',mode='ObserveIdle',purpose='None',awareness='Perceived',player='Player',target='Player',controller='C',director='D',event=v.CTX_ZERO,lastEvent=v.CTX_ZERO,emitted='-',acquired='-',has='yes',grounded='yes',running='yes',now='10',time='9.95',health='100',known='1000,0,0',actual='1000,0,0',forward='1,0,0',holdStart='-1',lastLoss='-1',closeSince='-1',planTime='-1',otherSeen='-1')
    d.update({k:str(x) for k,x in overrides.items()});return d


def context_candidate(id_='A',current=False,**kw):
    d=dict(controller='C',serial='1',id=id_,current='yes' if current else 'no',goal='450,0,0',v='1',f='0' if current else '1',i='0',p='0' if current else '1',c='0' if current else '.25',r='0')
    d.update({k:str(x) for k,x in kw.items()});d['score']=str(40*float(d['v'])+20*float(d['f'])+15*float(d['i'])+10*float(d['p'])-15*float(d['c'])-10*float(d['r']));return d


def context_selection():
    return [context_candidate('None',True),context_candidate()],dict(controller='C',serial='1',attempts='1',selectedScore='66.25',currentScore='40',selected='A')


def context_line(tag,d):return tag+' '.join(f'{k}={x}' for k,x in d.items())


def context_probe_example():
    import math
    suite='context-probe';rows=[v.CTX_TAG+f'status=BEGIN suite={suite} map={v.MAP} expected=4','PEDRO_NAV_TEST status=CAPABILITY dynamic=yes connected=yes disconnected=yes length=700']
    t=1.;eid='1'*32
    def step(case,name,c,d,player,points,*,mode,active=False,purpose='None',request=0,goal=0,selection=0,watch=None,activity=False,withdraw=False):
        nonlocal t
        t+=.1;begin=t; travel=yawtravel=0.;lastx=points[0][0];lastyaw=points[0][1];startx=lastx;plan=begin+.1; sight=plan-.05
        def make(kind,x,yaw,frame,run_active,final_mode):
            theta=math.radians(yaw);focus=activity;at=begin if focus else 0
            e=eid if focus else v.CTX_ZERO
            values=context_sample(status=kind,case=case,step=name,controller=c,director=d,now=f'{t:.6f}',dt='.1' if kind=='FRAME' else '0',root=f'{x},0,0',start=f'{startx},0,0',velocity='100,0,0' if run_active and not withdraw else '-150,0,0' if run_active else '0,0,0',yaw=yaw,eye=f'{x},0,64',forward=f'{math.cos(theta):.6f},{math.sin(theta):.6f},0',travel=f'{travel:.3f}',yawTravel=f'{yawtravel:.3f}',mode=final_mode,active='yes' if run_active else 'no',purpose=purpose if run_active else 'None',id=request if run_active else 0,goal=f'{goal},0,0' if run_active else '0,0,0',valid='yes' if run_active else 'no',length=abs(goal-startx) if run_active else 0,submissions=1 if frame>0 and (active or withdraw) else 1 if focus else 0,planTime=plan if run_active else -1,sighting=f'{player},0,0' if run_active else '0,0,0',sightTime=sight if run_active else 0,known=f'{player},0,0',actual=f'{player},0,0',time=f'{t-.02:.6f}',attention=4 if focus else 2,lookSet='yes' if focus else 'no',look='1000,250,64' if focus else '0,0,0',event=e,lastEvent=e,activity='1000,250,64' if focus else '0,0,0',lastActivity='1000,250,64' if focus else '0,0,0',activityAt=f'{at:.6f}',lastActivityAt=f'{at:.6f}',expiry=f'{at+2:.6f}' if focus else 0,holdStart=at if focus else -1,seen=1 if focus else 0,evidence=1 if focus else 0,sampled=1 if focus else 0,emitted=eid if focus else '-',acquired=eid if focus else '-',selection=selection,routePoint=watch if run_active and watch else 'None',point=watch if watch else 'None',currentPoint=watch if final_mode=='WatchFromPosition' else 'None',selectedScore=66.25 if watch else 0,currentScore=40 if watch else 0,attempts=1 if watch else 0,closeSince=begin-.5 if withdraw else -1,disengageUntil=t+3 if final_mode=='Disengaged' else 0)
            return values
        # The event occurs synchronously in Setup before STEP_BEGIN.
        if activity:rows.extend(['PEDRO_DIRECT status=Visible kind=1 subject=Prop opportunity=Prop actualLOS=checked','PEDRO_DIRECT acquired=yes kind=1 subject=Prop opportunity=Prop total=1 provenance=DirectObservation',f'PEDRO_CONTEXT_EVENT id={eid} subject=Prop opportunity=Prop trace=Visible location=1000,250,64 time={begin:.6f} director={d}'])
        rows.append(context_line(v.CTX_TAG,make('STEP_BEGIN',lastx,lastyaw,0,False,mode)))
        final=None
        for i,(x,yaw) in enumerate(points[1:],1):
            t+=.1;travel+=abs(x-lastx);yawtravel+=abs(yaw-lastyaw);lastx=x;lastyaw=yaw
            moving=(active or withdraw) and (i<len(points)-3 or mode=='FollowPlayer')
            endmode=mode if moving or not(active or withdraw) else ('Disengaged' if withdraw else 'WatchFromPosition')
            final=make('FRAME',x,yaw,i,moving,endmode)
            if moving and withdraw:final['planTime']=f'{plan:.6f}';final['sightTime']=f'{sight:.6f}'
            rows.append(context_line(v.CTX_TAG,final))
            if i==1 and watch:
                cand,summary=context_selection();cand[1]['id']=watch;cand[1]['goal']=f'{goal},0,0';summary['selected']=watch
                for item in cand:item['controller']=c;rows.append(context_line('PEDRO_CONTEXT_CANDIDATE ',item))
                summary['controller']=c;rows.append(context_line('PEDRO_CONTEXT_SELECTION ',summary))
        final=dict(final,status='CHECK' if case=='-' else 'PASS',dt='0');rows.append(context_line(v.CTX_TAG,final))
    step('-','ProbeNavCapability','C0','None',900,[(0,0)]*6,mode='Disabled')
    rows.append('PEDRO_AWARENESS_TEST status=RESET_EVIDENCE')
    step('P01','ProbeFollow','C1','D1',1200,[(i*10,0) for i in range(11)],mode='FollowPlayer',active=True,purpose='Follow',request=1,goal=750)
    step('P02','ProbeActivity','C1','D1',1200,[(100,0),(100,6),(100,12),(100,15.524),(100,15.524),(100,15.524)],mode='ObserveActivity',activity=True)
    rows.append('PEDRO_AWARENESS_TEST status=RESET_EVIDENCE')
    step('P03','ProbeWatchPoint','C2','D2',1000,[(i*15,0) for i in range(35)]+[(510,0)]*3,mode='TravelToWatchPoint',active=True,purpose='WatchPoint',request=2,goal=550,selection=1,watch='Primary')
    rows.append('PEDRO_AWARENESS_TEST status=RESET_EVIDENCE')
    step('P04','ProbeWithdrawal','C3','D3',180,[(i*-15,0) for i in range(17)]+[(-240,0)]*3,mode='Withdraw',purpose='Withdraw',request=3,goal=-270,withdraw=True)
    rows.extend([v.CTX_TAG+'status=SUMMARY suite=context-probe passed=4 expected=4 result=PASS','LogCore: Engine exit requested']);return '\n'.join(rows)+'\n'


class ContextVerifierTests(unittest.TestCase):
    def sample(self,**kw):return v.ctx_sample(context_sample(**kw))
    def reject(self,**kw):
        with self.assertRaises(ValueError):self.sample(**kw)
    def frames(self,**kw):
        a=self.sample(now='10',dt='0');b=self.sample(now='10.1',dt='.1',**kw);return v.ctx_frame(a,b)
    def test_valid_sample(self):self.assertEqual(self.sample()['mode'],'ObserveIdle')
    def test_missing_field(self):
        d=context_sample();del d['goal']
        with self.assertRaises(ValueError):v.ctx_sample(d)
    def test_duplicate_field(self):
        with self.assertRaises(ValueError):v.ctx_fields(v.CTX_TAG+'status=FRAME status=PASS')
    def test_nonfinite(self):self.reject(yaw='nan')
    def test_malformed_boolean(self):self.reject(active='true')
    def test_speed(self):self.reject(velocity='200,0,0')
    def test_displacement(self):
        with self.assertRaises(ValueError):self.frames(root='100,0,0')
    def test_yaw_rate(self):
        with self.assertRaises(ValueError):self.frames(yaw='90',attention=2)
    def test_player_forced_move(self):
        with self.assertRaises(ValueError):self.frames(actual='1100,0,0')
    def test_player_damage(self):
        with self.assertRaises(ValueError):self.frames(health='90')
    def test_orphan_request(self):self.reject(id=7)
    def test_missing_purpose(self):self.reject(active='yes',valid='yes',id=3,length=100,submissions=1)
    def test_expired_plan(self):self.reject(active='yes',valid='yes',id=3,length=550,purpose='Follow',submissions=1,planTime=9,sightTime=8,goal='550,0,0',sighting='1000,0,0')
    def test_personal_space(self):self.reject(active='yes',valid='yes',id=3,length=800,purpose='WatchPoint',routePoint='A',submissions=1,planTime=10,sightTime=9.9,goal='800,0,0',sighting='1000,0,0')
    def test_hidden_activity(self):self.reject(event='1'*32,emitted='1'*32)
    def test_hidden_interest(self):self.reject(lastEvent='1'*32,emitted='1'*32)
    def test_activity_cap(self):self.reject(event='1'*32,emitted='1'*32,acquired='1'*32,evidence=1,activityAt=9,holdStart=9,expiry=13)
    def test_adaptation_bound(self):self.reject(losses=33)
    def test_query_bound(self):self.reject(attempts=6)
    def test_score_valid(self):self.assertAlmostEqual(v.ctx_score(context_candidate()),66.25)
    def test_score_forged(self):
        a=context_candidate();a['score']='100';self.assertRaises(ValueError,v.ctx_score,a)
    def test_score_nonfinite(self):
        a=context_candidate();a['v']='nan';self.assertRaises(ValueError,v.ctx_score,a)
    def test_selection_valid(self):self.assertEqual(v.ctx_selection(*context_selection())[0],'A')
    def test_selection_duplicate(self):
        a,b=context_selection();a.append(a[-1]);self.assertRaises(ValueError,v.ctx_selection,a,b)
    def test_selection_no_improvement(self):
        a,b=context_selection();a[1]=context_candidate(v=.2);self.assertRaises(ValueError,v.ctx_selection,a,b)
    def test_selection_tie(self):
        a,b=context_selection();a.append(context_candidate('B'));b.update(attempts='2',selected='B');self.assertRaises(ValueError,v.ctx_selection,a,b)
    def test_false_satisfaction(self):
        with self.assertRaises(ValueError):self.frames(mode='Satisfied')
    def test_probe_valid(self):self.assertEqual(v.verify_context(context_probe_example(),'context-probe')['expected'],4)
    def test_probe_process_exit(self):self.assertRaises(ValueError,v.verify_context,context_probe_example(),'context-probe',1)
    def test_probe_missing_case(self):self.assertRaises(ValueError,v.verify_context,context_probe_example().replace('status=PASS case=P03','status=CHECK case=P03'),'context-probe')
    def test_probe_fake_score(self):self.assertRaises(ValueError,v.verify_context,context_probe_example().replace('selectedScore=66.25 currentScore=40 selected=Primary','selectedScore=99 currentScore=40 selected=Primary'),'context-probe')
    def test_probe_bad_event(self):self.assertRaises(ValueError,v.verify_context,context_probe_example().replace('trace=Visible location=1000,250,64','trace=Occluded location=1000,250,64'),'context-probe')
