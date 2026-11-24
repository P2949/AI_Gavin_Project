"""B2b-6e source-only regression tests; no Unreal binary/editor required."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import types
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class ScopeContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sc = module('scope_contract_test', TOOLS / 'b2b6e_write_scope.py')

    def test_first_run_may_create_only_3(self):
        after = {p: '1' for p in self.sc.TEST_NAMES}
        self.assertEqual(set(self.sc.check('prepare-test', {}, after)['changed']),
                         self.sc.TEST_NAMES)

    def test_resume_requires_report_and_all_three_changes(self):
        before = {p: '1' for p in self.sc.TEST_NAMES}
        after = {p: '2' for p in self.sc.TEST_NAMES}
        good = {'mode': 'prepare-test', 'status': 'PASS',
                'resumed_pristine_duplicates': True}
        self.assertEqual(len(self.sc.check('prepare-test', before, after, good)['changed']), 3)
        for bad in (None, {}, {'mode': 'prepare-test', 'status': 'FAIL',
                               'resumed_pristine_duplicates': True},
                    {'mode': 'prepare-test', 'status': 'PASS',
                     'resumed_pristine_duplicates': False}):
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    self.sc.check('prepare-test', before, after, bad)

    def test_resume_requires_all_three_saved_diffs(self):
        before = {p: '1' for p in self.sc.TEST_NAMES}
        after = dict(before)
        after[next(iter(self.sc.TEST_NAMES))] = '2'
        with self.assertRaisesRegex(ValueError, 'every test asset'):
            self.sc.check('prepare-test', before, after, {
                'mode': 'prepare-test', 'status': 'PASS',
                'resumed_pristine_duplicates': True})

    def test_partial_or_unrelated_write_refused(self):
        data = list(self.sc.TEST_NAMES)
        before = {data[0]: '1'}
        after = {p: '2' for p in data}
        with self.assertRaises(ValueError):
            self.sc.check('prepare-test', before, after, {
                'mode': 'prepare-test', 'status': 'PASS',
                'resumed_pristine_duplicates': True})
        with self.assertRaisesRegex(ValueError, 'Unexpected'):
            self.sc.check('prepare-test', {},
                          {**{p: '1' for p in data}, 'Content/Other/A.uasset': '1'})

    def test_noop_still_refused(self):
        before = {p: '1' for p in self.sc.TEST_NAMES}
        with self.assertRaises(ValueError):
            self.sc.check('prepare-test', before, before, {
                'mode': 'prepare-test', 'status': 'PASS',
                'resumed_pristine_duplicates': True})


class UnrealSoftPropertyContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        class SoftObjectPath:
            def __init__(self, path):self.path = path
        class SoftReference:
            def __init__(self, path):self.path = path
            def export_text(self):return self.path
        cls.SoftObjectPath = SoftObjectPath
        cls.SoftReference = SoftReference
        cls.path = '/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest'
        cls.assets = types.SimpleNamespace(does_asset_exist=lambda _: True)
        ue = types.ModuleType('unreal')
        ue.EditorAssetLibrary = cls.assets
        ue.LevelEditorSubsystem = object
        ue.EditorActorSubsystem = object
        ue.UnrealEditorSubsystem = object
        ue.NavigationSystemV1 = object
        ue.get_editor_subsystem = lambda c: object()
        ue.SoftObjectPath = SoftObjectPath
        ue.SystemLibrary = types.SimpleNamespace(
            conv_soft_obj_path_to_soft_obj_ref=lambda p: SoftReference(p.path),
            conv_soft_object_reference_to_string=lambda r: r.path)
        contract = types.ModuleType('b2b6e_contract')
        for name in ('BASELINE', 'BASELINE_DATA', 'BASELINE_HUB', 'PRODUCTION',
                     'TEST', 'TEST_DATA', 'TEST_HUB', 'TEST_ID'):
            setattr(contract, name, '/Game/Mock/' + name)
        cls.ue = ue
        with patch.dict(sys.modules, {'unreal': ue, 'b2b6e_contract': contract}):
            cls.helpers = module('editor_common_test', TOOLS / 'b2b6e_editor_common.py')

    def test_conversion_object_passed_not_raw_path_struct(self):
        Ref = self.SoftReference
        class Definition:
            value = None
            def set_editor_property(self, name, value):
                if not isinstance(value, Ref):
                    raise TypeError('expected converted SoftObjectReference')
                self.value = value
            def get_editor_property(self, name):return self.value
            def get_name(self):return 'TestDefinition'
        x = Definition()
        self.helpers.set_soft_world(x, 'encounter_map', self.path)
        self.assertIsInstance(x.value, Ref)
        self.assertEqual(x.value.path, self.path + '.' + self.path.rsplit('/',1)[-1])

    def test_converter_bad_path_rejected_before_property_write(self):
        original = self.helpers.unreal.SystemLibrary
        try:
            self.helpers.unreal.SystemLibrary = types.SimpleNamespace(
                conv_soft_obj_path_to_soft_obj_ref=lambda p: self.SoftReference('/Game/Wrong.A'),
                conv_soft_object_reference_to_string=lambda r: r.path)
            with self.assertRaisesRegex(RuntimeError, 'converter returned'):
                self.helpers.make_soft_world_reference(self.path)
        finally:
            self.helpers.unreal.SystemLibrary = original

    def test_missing_converter_is_fail_closed(self):
        original = self.helpers.unreal.SystemLibrary
        try:
            self.helpers.unreal.SystemLibrary = types.SimpleNamespace()
            with self.assertRaisesRegex(RuntimeError, 'conversion unavailable'):
                self.helpers.make_soft_world_reference(self.path)
        finally:
            self.helpers.unreal.SystemLibrary = original


class AuthorResumeContract(unittest.TestCase):
    def test_resume_checks_multiple_saved_assets_before_mutation(self):
        s = (TOOLS / 'b2b6e_author_level.py').read_text()
        preflight = s[s.index('def _check_pristine_partial_clone('):s.index('def prepare_test():')]
        self.assertIn("'encounter_map'", preflight)
        self.assertIn("'PedroPrototypeReturnVolume'", preflight)
        self.assertIn("'EncounterEntranceActor'", preflight)
        self.assertIn("'_stable_source_signature'".strip("'"), preflight)
        self.assertIn('refusing resume', preflight)
        self.assertIn('all(existence) or not any(existence)', s)
        self.assertIn('resumed_pristine_duplicates', s)

    def test_exact_source_no_path_struct_assignment(self):
        s = (TOOLS / 'b2b6e_editor_common.py').read_text()
        f = s[s.index('def set_soft_world('):s.index('def nav_project(')]
        self.assertIn('make_soft_world_reference(package)', f)
        self.assertNotIn('setprop(obj, property_name, path)', f)
        self.assertIn('setprop(obj, property_name, ref)', f)

    def test_runner_requires_report_proof_for_write_scope(self):
        s = (TOOLS / 'b2b6e_editor.sh').read_text()
        self.assertIn('"$RUN/write-scope.json" "$RUN/results.json"', s)


if __name__ == '__main__':
    unittest.main()
