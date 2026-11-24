"""Fail-closed report and source-seam tests for the new real-world flow fixture."""
from __future__ import annotations
import importlib.util
from pathlib import Path
import unittest

TOOLS = Path(__file__).resolve().parents[1]
PROJECT = TOOLS.parents[1]
spec = importlib.util.spec_from_file_location('verify_pedro_flow',TOOLS/'verify_pedro_flow.py')
V = importlib.util.module_from_spec(spec)
spec.loader.exec_module(V)
G=[f'{i:032x}' for i in range(1,5)]

def example(scenario='completion'):
    hub,pedro=V.PRODUCTION
    rows=[f'LogTemp: Display: PEDRO_FLOW_TEST status=BEGIN scenario={scenario} hub={hub} pedro={pedro} expected={len(V.CASES[scenario])}']
    details={'HubEntry':'travel=RealPlayerInteraction',
             'PedroConfiguration':'policy=EvidenceDriven core=4 required=3',
             'EarlyReturn':'travel=RealOverlap',
             'HubUnresolved':'sessionCount=0 callbacks=0',
             'FreshReentry':'localEvidence=0 sessionCount=0 callbacks=0 active=yes',
             'NoPrematureCompletion':'after_two_distinct=yes progress=0',
             'ExactlyOneResolution':'classification=1 callbacks=1 sessionCount=1',
             'Satisfied':'ai_mode=Satisfied',
             'ReturnToHub':'travel=RealOverlap',
             'HubProgressPersisted':'sessionCount=1 callbacks=1',
             'CompletedReentry':'localEvidence=0 sessionCount=1 callbacks=1 active=no'}
    for case,core,evidence,op,uid in [('Evidence01',1,1,'A',G[0]),
                                      ('RepeatNoExtraCore',1,2,'A',G[1]),
                                      ('Evidence02',2,3,'B',G[2]),
                                      ('Evidence03',3,4,'C',G[3])]:
        details[case]=f'opportunity={op} event={uid} core={core} evidence={evidence} provenance=DirectObservation'
    for c in V.CASES[scenario]:
        if c.startswith('Evidence') or c=='RepeatNoExtraCore':
            rows.append('PEDRO_DIRECT acquired=yes kind=1 subject=Correct opportunity=Correct provenance=DirectObservation')
        if c=='ExactlyOneResolution':
            rows.append('PEDRO_OUTCOME policy=EvidenceDriven interpretation=Seeking sampledCore=3 requiredCore=3 resolved=yes actualLOS=checked')
        rows.append(f'LogTemp: Display: PEDRO_FLOW_TEST status=PASS scenario={scenario} case={c} {details[c]}')
    rows.append(f'LogTemp: Display: PEDRO_FLOW_TEST status=SUMMARY scenario={scenario} passed={len(V.CASES[scenario])} expected={len(V.CASES[scenario])} result=PASS')
    rows.append('LogCore: Engine exit requested (reason: GenericPlatform RequestExit)')
    return '\n'.join(rows)

class FlowVerifier(unittest.TestCase):
    def reject(self, log, scenario='completion', rc=0, hub=None, pedro=None):
        hub=hub or V.PRODUCTION[0];pedro=pedro or V.PRODUCTION[1]
        with self.assertRaises(V.FlowVerificationError):V.verify(log,rc,scenario,hub,pedro)
    def test_valid_completion(self):
        r=V.verify(example(),0,'completion',*V.PRODUCTION)
        self.assertEqual(r['real_distinct_core_count'],3)
        self.assertEqual(r['progress_notifications'],1)
    def test_valid_early(self):
        r=V.verify(example('early'),0,'early',*V.PRODUCTION)
        self.assertTrue(r['early_exit_and_fresh_reentry'])
    def test_reject_nonzero_exit(self):self.reject(example(),rc=1)
    def test_reject_wrong_pair(self):self.reject(example(),hub=V.ISOLATED[0])
    def test_reject_missing_case(self):self.reject(example().replace('case=Evidence02','case=Forged'))
    def test_reject_extra_case(self):self.reject(example().replace('case=Evidence02','case=Evidence02\nPEDRO_FLOW_TEST status=PASS scenario=completion case=Evidence02'))
    def test_reject_duplicate_field(self):self.reject(example().replace('core=1 evidence=1','core=1 core=1 evidence=1',1))
    def test_reject_guid_replay(self):self.reject(example().replace('event='+G[2], 'event='+G[0]))
    def test_reject_opportunity_replay(self):self.reject(example().replace('opportunity=C','opportunity=A'))
    def test_reject_forged_counts(self):self.reject(example().replace('core=3 evidence=4','core=2 evidence=4'))
    def test_reject_missing_direct_observation(self):self.reject(example().replace('PEDRO_DIRECT acquired=yes','PEDRO_DIRECT acquired=no',1))
    def test_reject_missing_resolution_callback(self):self.reject(example().replace('callbacks=1 sessionCount=1','callbacks=0 sessionCount=1'))
    def test_reject_multiple_outcomes(self):self.reject(example()+'\nPEDRO_OUTCOME policy=EvidenceDriven interpretation=Seeking sampledCore=3 requiredCore=3 resolved=yes actualLOS=checked')
    def test_reject_duplicate_resolution_case(self):
        log = example()
        row = next(line for line in log.splitlines() if 'case=ExactlyOneResolution' in line)
        self.reject(log.replace(row, row + '\n' + row))
    def test_reject_missing_satisfied(self):self.reject(example().replace('ai_mode=Satisfied','ai_mode=FollowPlayer'))
    def test_reject_bad_reentry(self):self.reject(example().replace('active=no','active=yes'))
    def test_reject_fatal_even_after_pass(self):self.reject(example()+'\nEnsure condition failed: something broke')
    def test_reject_no_clean_exit(self):self.reject(example().replace('Engine exit requested','Process still alive'))
    def test_failed_game_reports_exact_reason(self):
        log=example().replace('PEDRO_FLOW_TEST status=SUMMARY',
            'PEDRO_FLOW_TEST status=FAIL scenario=completion reason=NoThirdObservedReachableCore phase=4\nPEDRO_FLOW_TEST status=SUMMARY')
        with self.assertRaisesRegex(V.FlowVerificationError,'NoThirdObservedReachableCore'):
            V.verify(log,0,'completion',*V.PRODUCTION)
    def test_failed_game_reports_subcondition(self):
        log=example().replace('PEDRO_FLOW_TEST status=SUMMARY',
            'PEDRO_FLOW_DIAG scenario=completion check=Completion reason=OutcomeActorState index=1 winner=1 hidden=yes\n'
            'PEDRO_FLOW_TEST status=FAIL scenario=completion reason=ClassificationOutcomeOrDelegateMismatch phase=8\n'
            'PEDRO_FLOW_TEST status=SUMMARY')
        with self.assertRaisesRegex(V.FlowVerificationError,'OutcomeActorState'):
            V.verify(log,0,'completion',*V.PRODUCTION)
    def test_reject_early_resolution(self):self.reject(example('early')+'\nPEDRO_OUTCOME policy=EvidenceDriven',scenario='early')
    def test_reject_early_state_leak(self):self.reject(example('early').replace('localEvidence=0','localEvidence=1'),scenario='early')
    def test_reject_early_dual_callback(self):self.reject(example('early').replace('callbacks=0','callbacks=2'),scenario='early')

class SourceContracts(unittest.TestCase):
    def setUp(self):
        self.cpp=(PROJECT/'Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroFlowSmoke.cpp').read_text()
        self.probe=(PROJECT/'Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroFlowResolvedProbe.h').read_text()
    def test_real_entry_and_return(self):
        for proof in ('S.Interaction->TryInteract()', 'Box->IsOverlappingActor(P)',
                      'FPlatformMisc::RequestExit(false)', 'GetResolvedEncounterCount()',
                      'GetSuccessfulInspectionCount()', 'OnEncounterResolved.AddDynamic',
                      'GetContextMemory().bSatisfied', 'GetObjectPropertyValue_InContainer',
                      'FindPathSync(', 'OverlapBlockingTestByChannel(',
                      'LineTraceSingleByChannel(', 'PedroSmokeVisibility::CanSee('):
            self.assertIn(proof,self.cpp)
    def test_no_synthetic_world_facts_or_forced_travel(self):
        for forbidden in ('RecordEvidence(', 'ResolveEncounter(', 'ReportConfirmedAction(',
                          'ReportOpportunityExposed(', 'OpenLevel(', 'OpenLevelBySoftObjectPtr(',
                          'S.Observer->SetActorLocation(', 'S.Observer->SetActorRotation(',
                          'OnEncounterResolved.Broadcast(', 'SetActorHiddenInGame('):
            self.assertNotIn(forbidden,self.cpp)
    def test_source_checks_current_world_encounter_definition(self):
        self.assertIn('ReadActorProperty<UEncounterDefinition>(D,TEXT("EncounterDefinition"))',self.cpp)
        self.assertIn('Definition->GetEncounterId()!=S.EncounterId',self.cpp)
        self.assertIn('OutcomeActorState index=',self.cpp)
        self.assertIn('PEDRO_FLOW_DIAG scenario=',self.cpp)
    def test_observer_acts_naturally_before_a_real_directly_observed_inspection(self):
        self.assertIn('PreparePosition(Target,false,true)',self.cpp)
        self.assertIn('case EPhase::WaitPedroSight:',self.cpp)
        self.assertIn('ObserverSees(S.PendingProp.Get())',self.cpp)
        self.assertIn('S.Interaction->TryInteract()',self.cpp)
        self.assertIn('FPlatformTime::Seconds()-S.InteractionStarted>38.0',self.cpp)
    def test_resolution_marker_after_wait_guards(self):
        phase = self.cpp.split('case EPhase::WaitSatisfied:', 1)[1].split('case EPhase::ReturnToHub:', 1)[0]
        marker = 'Pass(TEXT("ExactlyOneResolution")'
        self.assertEqual(phase.count(marker), 1)
        self.assertEqual(phase[:phase.index(marker)].count('return true;'), 2)
        self.assertEqual(phase[phase.index(marker):].count('return true;'), 1)
        self.assertGreater(phase.index(marker), phase.index('ObserverDidNotEnterSatisfied'))
        self.assertLess(phase.index(marker), phase.index('Pass(TEXT("Satisfied")'))
        self.assertLess(phase.index('Pass(TEXT("Satisfied")'), phase.index('Next(EPhase::ReturnToHub)'))
    def test_shared_visibility_preflight(self):
        native = PROJECT/'Source/AI_Gavin_Project/Encounters/Pedro/Tests'
        helper = (native/'PedroSmokeVisibility.h').read_text()
        other = (native/'PedroPlayableSmoke.cpp').read_text()
        self.assertIn('PedroSmokeVisibility::CanSee(', self.cpp)
        self.assertIn('PedroSmokeVisibility::CanSee(', other)
        self.assertIn('PedroDirectObservation::CheckGeometry', helper)
        self.assertIn('Params.AddIgnoredActor(Observer)', helper)
        self.assertIn('ECC_Visibility, Params) || Hit.GetActor() == Target', helper)
        for forbidden in ('RecordEvidence(', 'ReportConfirmedAction(', 'SetActorLocation(', 'AddIgnoredActor(Target)'):
            self.assertNotIn(forbidden, helper)
    def test_probe_is_not_resolver(self):
        self.assertIn('UFUNCTION()',self.probe)
        self.assertIn('OnEncounterResolved(UEncounterDefinition* Encounter)',self.probe)
        self.assertNotIn('ResolveEncounter(',self.probe)

if __name__ == '__main__':unittest.main()
