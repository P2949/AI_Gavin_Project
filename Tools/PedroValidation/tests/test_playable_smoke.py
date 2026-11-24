"""Negative and positive independent log-verifier tests."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from verify_playable_smoke import verify
from b2b6e_contract import PRODUCTION, TEST


def good():
    return '\n'.join([
        'PEDRO_PLAYABLE_TEST status=BEGIN map=/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype expected=6',
        'PEDRO_PLAYABLE_TEST status=PASS case=Configuration',
        'PEDRO_PLAYABLE_TEST status=PASS case=Activation',
        'PEDRO_PLAYABLE_TEST status=PASS case=SavedNavmesh',
        'PEDRO_PLAYABLE_TEST status=PASS case=AutonomousFollow displacement=130.0 submissions=1',
        'PEDRO_PLAYABLE_TEST status=PASS case=RealMeleeMiss',
        'PEDRO_PLAYABLE_TEST status=PASS case=RealMeleeContact attackCount=1 provenance=DirectContact',
        'PEDRO_PLAYABLE_TEST status=SUMMARY passed=6 expected=6 result=PASS',
        'LogCore: Engine exit requested (reason: GenericPlatform RequestExit)',
    ])+'\n'

class SmokeVerifierTests(unittest.TestCase):
    def test_valid(self):self.assertEqual(verify(good(),0)['expected'],6)
    def test_missing(self):
        with self.assertRaises(ValueError):verify(good().replace('PEDRO_PLAYABLE_TEST status=PASS case=Activation\n',''),0)
    def test_duplicate(self):
        line='PEDRO_PLAYABLE_TEST status=PASS case=Activation\n'
        with self.assertRaises(ValueError):verify(good().replace(line,line+line),0)
    def test_failed(self):
        with self.assertRaises(ValueError):verify(good().replace('status=PASS case=Activation','status=FAIL case=Activation'),0)
    def test_wrong_order(self):
        with self.assertRaises(ValueError):verify(good().replace('case=Activation','case=Configuration'),0)
    def test_no_move(self):
        with self.assertRaises(ValueError):verify(good().replace('displacement=130.0','displacement=0.0'),0)
    def test_no_path(self):
        with self.assertRaises(ValueError):verify(good().replace('submissions=1','submissions=0'),0)
    def test_fatal(self):
        with self.assertRaises(ValueError):verify(good()+'Fatal error: asserted\n',0)
    def test_nonzero(self):
        with self.assertRaises(ValueError):verify(good(),1)
    def test_no_exit(self):
        with self.assertRaises(ValueError):verify(good().replace('Engine exit requested','Exited unusually'),0)
    def test_missing_contact(self):
        with self.assertRaises(ValueError):verify(good().replace('PEDRO_PLAYABLE_TEST status=PASS case=RealMeleeContact attackCount=1 provenance=DirectContact\n',''),0)
    def test_fake_contact(self):
        with self.assertRaises(ValueError):verify(good().replace('attackCount=1','attackCount=2'),0)
    def test_isolated_map(self):
        self.assertEqual(verify(good().replace(PRODUCTION, TEST), 0, TEST)['map'], TEST)
    def test_wrong_provenance(self):
        with self.assertRaises(ValueError):
            verify(good().replace('DirectContact', 'PrototypeAssumed'), 0)
    def test_partial_failed_run_cannot_pass(self):
        partial = '\n'.join(good().splitlines()[:4])
        partial += '\nPEDRO_PLAYABLE_TEST status=FAIL reason=NoVisibleReachablePlayerTestPosition\nLogCore: Engine exit requested\n'
        with self.assertRaises(ValueError):
            verify(partial, 0)
    def test_fake_extra_case(self):
        with self.assertRaises(ValueError):verify(good().replace('status=SUMMARY','status=PASS case=Extra\nPEDRO_PLAYABLE_TEST status=SUMMARY'),0)

if __name__=='__main__':unittest.main()
