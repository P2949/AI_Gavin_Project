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
                succeeded=101,succeededWithWarnings=0,failed=0,notRun=0,inProcess=0)


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
    def test_valid(self): self.assertEqual(v.native(native_report())['total'],101)
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
        with self.assertRaisesRegex(ValueError,'all 101 native tests passed, but native process exit 1'):v.native(native_report(),1)
    def test_process_failure_diagnostics(self):
        text=v.native_diagnostics(native_report(),1)
        self.assertIn('Native process exit: 1',text)
        self.assertIn('"succeeded": 101',text)
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
    def test_unity_valid(self):self.assertEqual(len(self.verify()['source_membership']),6)
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
        self.log='Running command : UBT -DisableUnity -DisableAdaptiveUnity -gather\n'+''.join(f'[{i+1}/7] Compile {Path(s).name}\n' for i,s in enumerate(v.UNITY_SOURCES))+'[7/7] Link libUnrealEditor-AI_Gavin_Project.so\nResult: Succeeded\n'
    def test_valid(self):self.assertEqual(v.verify_non_unity(self.log,0)['expected'],6)
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


if __name__=='__main__':unittest.main()
