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
                succeeded=81,succeededWithWarnings=0,failed=0,notRun=0,inProcess=0)


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
    def test_valid(self): self.assertEqual(v.native(native_report())['total'],81)
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
        with self.assertRaisesRegex(ValueError,'all 81 native tests passed, but native process exit 1'):v.native(native_report(),1)
    def test_process_failure_diagnostics(self):
        text=v.native_diagnostics(native_report(),1)
        self.assertIn('Native process exit: 1',text)
        self.assertIn('"succeeded": 81',text)
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
    def test_unity_valid(self):self.assertEqual(len(self.verify()['source_membership']),4)
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
    def test_unity_process_failure(self):
        with self.assertRaises(ValueError):self.verify(rc=1)
    def test_unity_missing_generated_file(self):
        self.blob.unlink()
        with self.assertRaises(OSError):self.verify()
    def test_unity_wrong_path(self):
        self.write(self.blob,tuple('Unrelated/'+Path(n).name for n in v.UNITY_SOURCES))
        with self.assertRaises(ValueError):self.verify()

if __name__=='__main__':unittest.main()
