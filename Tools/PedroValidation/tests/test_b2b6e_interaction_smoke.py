"""Offline refusal cases for the real saved-world player interaction gate."""
import importlib.util
import pathlib
import re
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location('verify_interaction_smoke',ROOT/'verify_interaction_smoke.py')
VERIFY=importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)
MAP='/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype'
CASES=VERIFY.CASES

def valid_log():
    lines=[f'LogTemp: Display: PEDRO_INTERACTION_TEST status=BEGIN map={MAP} expected=6 core=4 required=3 props=4',
           'LogTemp: Display: PEDRO_INTERACTION_TEST status=PASS case=Configuration',
           'LogTemp: Display: PEDRO_INTERACTION_TEST status=PASS case=FailedDispatch',
           'PEDRO_INTERACTION_SETUP target=SomeRealProp opportunity=CoreOne visible=no tested=16 projected=12',
           'LogTemp: Display: PEDRO_INTERACTION_TEST status=PASS case=HiddenWorldTruthNotKnowledge inspected=1 evidence=0',
           'LogTemp: Display: PEDRO_INTERACTION_TEST status=PASS case=NoReplay',
           'PEDRO_INTERACTION_SETUP target=OtherRealProp opportunity=CoreTwo visible=yes tested=16 projected=14',
           'LogTemp: Display: PEDRO_INTERACTION_TEST status=PASS case=VisibleDirectObservation evidence=1 core=1 event=12345678-1234-1234-1234-123456789012',
           'LogTemp: Display: PEDRO_INTERACTION_TEST status=PASS case=RepeatNoExtraCore core=1 evidence=2',
           'LogTemp: Display: PEDRO_INTERACTION_TEST status=SUMMARY passed=6 expected=6 result=PASS',
           'LogCore: Engine exit requested (reason: GenericPlatform RequestExit)']
    return '\n'.join(lines)

class Validator(unittest.TestCase):
    def assert_bad(self, text, exit_status=0, expected=MAP):
        with self.assertRaises(VERIFY.InvalidReport):VERIFY.verify(text,exit_status,expected)
    def test_accept_exact(self):
        data=VERIFY.verify(valid_log(),0,MAP)
        self.assertEqual(data['cases'],list(CASES))
        self.assertEqual(data['hub_travel'],'UNVERIFIED')
    def test_wrong_map(self):self.assert_bad(valid_log(),expected='/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest')
    def test_wrong_exit(self):self.assert_bad(valid_log(),exit_status=1)
    def test_extra_marker(self):self.assert_bad(valid_log().replace('case=FailedDispatch','case=FailedDispatch\nPEDRO_INTERACTION_TEST status=PASS case=FailedDispatch'))
    def test_missing_marker(self):self.assert_bad(valid_log().replace('case=NoReplay','case=UnknownCase'))
    def test_no_world_exit(self):self.assert_bad(valid_log().replace('Engine exit requested','Engine continued'))
    def test_false_hidden_evidence(self):self.assert_bad(valid_log().replace('inspected=1 evidence=0','inspected=1 evidence=1'))
    def test_false_visible_evidence(self):self.assert_bad(valid_log().replace('evidence=1 core=1 event=','evidence=0 core=1 event='))
    def test_repeat_inflated_core(self):self.assert_bad(valid_log().replace('RepeatNoExtraCore core=1','RepeatNoExtraCore core=2'))
    def test_injected_failure(self):self.assert_bad(valid_log()+'\nPEDRO_INTERACTION_TEST status=FAIL reason=RealProblem')
    def test_fatal(self):self.assert_bad(valid_log()+'\nEnsure condition failed: Bad')
    def test_duplicate_begin(self):self.assert_bad(valid_log().replace('LogTemp: Display: PEDRO_INTERACTION_TEST status=BEGIN','PEDRO_INTERACTION_TEST status=BEGIN map='+MAP+' expected=6 core=4 required=3 props=4\nLogTemp: Display: PEDRO_INTERACTION_TEST status=BEGIN',1))
    def test_no_setup_proof(self):self.assert_bad(valid_log().replace('PEDRO_INTERACTION_SETUP target=OtherRealProp','NOT_A_REAL_SETUP target=OtherRealProp'))

class SourceScope(unittest.TestCase):
    def test_no_synthetic_actions(self):
        source=(ROOT.parents[1]/'Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroPlayableSmoke.cpp').read_text()
        new=source[source.index('namespace PedroInteractionSmoke'):]
        self.assertIn('S.Interaction->TryInteract()',new)
        self.assertIn('GetSuccessfulInspectionCount()',new)
        self.assertIn('EPedroKnowledgeSource::DirectObservation',new)
        self.assertIn('GetSampledCoreCount()',new)
        self.assertIn('ObserverCanSee',new)
        for forbidden in ('ReportConfirmedAction(', 'RecordEvidence(', 'ResolveEncounter(',
                          'OnWorldEventProduced.Broadcast(', 'SetActorLocation(Observer',
                          'S.Observer->SetActorLocation(', 'RequestMove('):
            self.assertNotIn(forbidden,new)
    def test_exit_logging_has_braced_if_else(self):
        source=(ROOT.parents[1]/'Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroPlayableSmoke.cpp').read_text()
        section=source.split('namespace PedroInteractionSmoke',1)[1]
        done=section.split('bool Done(const TCHAR* Reason=nullptr)',1)[1].split('bool ObserverCanSee',1)[0]
        self.assertRegex(done, r'(?s)if\s*\(Reason\)\s*\{\s*UE_LOG\(LogTemp,\s*Error,.*?\);\s*\}\s*else\s*\{\s*UE_LOG\(LogTemp,\s*Display,.*?\);\s*\}')
        self.assertEqual(done.count('FPlatformMisc::RequestExit(false);'), 1)
        self.assertIn('status=FAIL reason=%s step=%d', done)
        self.assertIn('status=SUMMARY passed=6 expected=6 result=PASS', done)

    def test_repeat_waits_for_live_geometry_without_retrying_dispatch(self):
        source=(ROOT.parents[1]/'Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroPlayableSmoke.cpp').read_text()
        repeat=source.split('if(S.Step==8)',1)[1].split('return Done(TEXT("UnreachableStage"))',1)[0]
        self.assertIn('CheckPreparedDispatch(true,Hit)',repeat)
        self.assertIn('S.RepeatStarted>=5.0',repeat)
        self.assertIn('RepeatBaselineChangedWhileWaiting',repeat)
        self.assertIn('RepeatGeometryDeadline',repeat)
        self.assertIn('PlayerTraceMismatch',repeat)
        self.assertEqual(repeat.count('TryInteract()'),1)
        self.assertLess(repeat.index('return true;'),repeat.index('TryInteract()'))
        self.assertIn('RepeatTryInteractRejectedVerifiedTrace',repeat)
        self.assertIn('GetSuccessfulInspectionCount()!=S.InspectionBefore+1',repeat)
        self.assertIn('GetSampledCoreCount()!=1',repeat)
        for forbidden in ('PositionForRealDispatch(', 'SetActorLocation(', 'S.CurrentProp=', 'S.InspectionBefore=', 'S.EvidenceBefore='):
            self.assertNotIn(forbidden,repeat)

    def test_repeat_diagnostics_do_not_override_failure(self):
        diagnostic='\nPEDRO_INTERACTION_DIAG step=8 reason=PlayerTraceMismatch target=Prop hit=Observer'
        self.assertEqual(VERIFY.verify(valid_log()+diagnostic,0,MAP)['status'],'PASS')
        with self.assertRaises(VERIFY.InvalidReport):
            VERIFY.verify(valid_log()+diagnostic+'\nPEDRO_INTERACTION_TEST status=FAIL reason=RepeatGeometryDeadline step=8',0,MAP)

    def test_original_movement_melee_cases_untouched(self):
        source=(ROOT.parents[1]/'Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroPlayableSmoke.cpp').read_text()
        original=source[:source.index('namespace PedroInteractionSmoke')]
        for case in ('AutonomousFollow','RealMeleeMiss','RealMeleeContact'):
            self.assertIn('case='+case,original)
        self.assertIn('PedroPlayableExpectedMap=',original)

if __name__=='__main__':unittest.main()
