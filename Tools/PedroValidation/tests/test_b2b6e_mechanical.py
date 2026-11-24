"""Offline fail-closed contract/regression tests for mechanical playable authoring."""
from __future__ import annotations
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from b2b6e_contract import (PRODUCTION, TEST, BASE_RUNTIME, COMMON,
    asset_disk_path, check_full_acceptance, expected_playable_map,
    expected_reports, expected_stages)
from b2b6e_write_scope import (TEST_NAMES, PROD_NAMES, check)
from verify_playable_smoke import verify


def good(map_path=PRODUCTION):
    return '\n'.join((
        f'PEDRO_PLAYABLE_TEST status=BEGIN map={map_path} expected=6',
        'PEDRO_PLAYABLE_TEST status=PASS case=Configuration',
        'PEDRO_PLAYABLE_TEST status=PASS case=Activation',
        'PEDRO_PLAYABLE_TEST status=PASS case=SavedNavmesh',
        'PEDRO_PLAYABLE_TEST status=PASS case=AutonomousFollow displacement=130 submissions=1',
        'PEDRO_PLAYABLE_TEST status=PASS case=RealMeleeMiss',
        'PEDRO_PLAYABLE_TEST status=PASS case=RealMeleeContact attackCount=1 provenance=DirectContact',
        'PEDRO_PLAYABLE_TEST status=SUMMARY passed=6 expected=6 result=PASS',
        'LogCore: Engine exit requested (reason: GenericPlatform RequestExit)',
    )) + '\n'


class MapContract(unittest.TestCase):
    def test_exact_selection(self):
        self.assertEqual(expected_playable_map('playable'), PRODUCTION)
        self.assertEqual(expected_playable_map('playable-rendered'), PRODUCTION)
        self.assertEqual(expected_playable_map('playable-test'), TEST)
        self.assertEqual(expected_playable_map('playable-test-rendered'), TEST)
        self.assertEqual(expected_playable_map('playable-test', TEST), TEST)
    def test_override_refuses_substitution(self):
        for mode, path in [('playable', TEST), ('playable-test', PRODUCTION),
                           ('playable-test', TEST + '_Fake'), ('playable', '../../escape')]:
            with self.subTest(mode=mode, path=path), self.assertRaises(ValueError):
                expected_playable_map(mode, path)
    def test_mode_invalid(self):
        with self.assertRaises(ValueError): expected_playable_map('integration', TEST)
    def test_asset_paths(self):
        self.assertEqual(asset_disk_path(Path('/tmp/P'), TEST, '.umap'),
                         Path('/tmp/P/Content/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest.umap'))
        for path in ('../notgame', '/Game/X/../A', '/Game/X.Asset', '/Other/Y'):
            with self.subTest(path=path), self.assertRaises(ValueError):
                asset_disk_path(Path('/tmp'), path, '.umap')


class StageContract(unittest.TestCase):
    def test_all_real_execution_order(self):
        expected=expected_stages('all')
        self.assertLess(expected.index('playable'),expected.index('rendered-world'))
        self.assertLess(expected.index('rendered-context-integration'),expected.index('rendered-playable'))
        self.assertLess(expected.index('rendered-playable'),expected.index('forced-unity-build'))
        self.assertEqual(expected[-1], 'repository-audit')
        self.assertEqual(len(set(expected)),len(expected))
    def test_rendered_contains_playable(self):
        self.assertIn('rendered-playable',expected_stages('rendered'))
    def test_test_modes_are_independent(self):
        self.assertIn('playable-test',expected_stages('playable-test'))
        self.assertNotIn('playable',expected_stages('playable-test'))
        self.assertIn('rendered-playable-test',expected_stages('playable-test-rendered'))
    def test_all_report_inventory(self):
        r=expected_reports('all')
        self.assertEqual(len(r), 4+len(BASE_RUNTIME)*2+2)
        self.assertTrue(check_full_acceptance('all',0,expected_stages('all'),r))
        self.assertFalse(check_full_acceptance('all',0,expected_stages('all'),r-{'playable'}))
        self.assertFalse(check_full_acceptance('all',0,expected_stages('all'),r|{'faked'}))
        self.assertFalse(check_full_acceptance('all',0,expected_stages('all')[:-1],r))
        self.assertFalse(check_full_acceptance('all',1,expected_stages('all'),r))


class BinaryScope(unittest.TestCase):
    def test_valid_noop_rebuild(self):
        self.assertEqual(check('build-test-nav', {}, {})['navigation_unchanged'], True)
        self.assertEqual(check('build-production-nav', {}, {})['changed'], [])


    def test_isolated_prepare_three_new_assets(self):
        b={'Tools/PedroValidation/file.py':'a'}
        a={**b, **{name:'hash'+str(i) for i,name in enumerate(TEST_NAMES)}}
        self.assertEqual(check('prepare-test',b,a)['status'],'PASS')
    def test_no_silent_missing_source(self):
        with self.assertRaises(ValueError):
            check('prepare-test',{}, {next(iter(TEST_NAMES)):'new'})
    def test_reject_unrelated_binary(self):
        p=next(iter(TEST_NAMES))
        with self.assertRaises(ValueError):
            check('author-test',{p:'a'},{p:'b','Content/Shared/Hub.umap':'x'})
    def test_production_does_not_change_test(self):
        p=next(iter(PROD_NAMES));t=next(iter(TEST_NAMES))
        with self.assertRaises(ValueError):check('author-production',{p:'a'},{p:'b',t:'c'})
    def test_no_op_rejected(self):
        with self.assertRaises(ValueError):check('author-production',{}, {})


class StrictPlayableLog(unittest.TestCase):
    def test_correct_test_map(self):
        self.assertEqual(verify(good(TEST),0,TEST)['map'],TEST)
    def test_wrong_begin_map_fails(self):
        with self.assertRaises(ValueError):verify(good(PRODUCTION),0,TEST)
    def test_unknown_expected_map_fails(self):
        with self.assertRaises(ValueError):verify(good(TEST),0,TEST+'_Fake')
    def test_duplicate_status_key_fails(self):
        with self.assertRaises(ValueError):verify(good().replace('status=BEGIN','status=BEGIN status=BEGIN'),0)
    def test_failed_real_process(self):
        with self.assertRaises(ValueError):verify(good(),1)
    def test_false_navigation(self):
        with self.assertRaises(ValueError):verify(good().replace('displacement=130','displacement=0'),0)
    def test_fatal_after_summary(self):
        with self.assertRaises(ValueError):verify(good()+'Fatal error: after pass',0)


if __name__ == '__main__':
    unittest.main()
