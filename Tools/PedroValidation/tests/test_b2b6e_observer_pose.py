"""Guard UE 5.8.2 physical-observer one-call teleporting pose authoring.

Behavioral Unreal Editor integration is independently exercised by the report
runner. These source contracts prevent reintroducing the two-call, nonteleporting
attached-eye bug. They never claim actual UE execution.
"""
from __future__ import annotations
import ast
from pathlib import Path
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'b2b6e_observer_placement_editor.py'

class EditorPoseContract(unittest.TestCase):
    def test_atomic_teleport_transform_in_author(self):
        tree = ast.parse(SCRIPT.read_text())
        run = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'run')
        pose_calls = [n for n in ast.walk(run) if isinstance(n, ast.Call) and
                      isinstance(n.func, ast.Attribute) and
                      n.func.attr in {'set_actor_location_and_rotation',
                                      'set_actor_location', 'set_actor_rotation'}]
        self.assertEqual(len(pose_calls), 1)
        call = pose_calls[0]
        self.assertEqual(call.func.attr, 'set_actor_location_and_rotation')
        self.assertEqual(len(call.args), 4)
        self.assertIsInstance(call.args[-2], ast.Constant)
        self.assertIs(call.args[-2].value, False)
        self.assertIsInstance(call.args[-1], ast.Constant)
        self.assertIs(call.args[-1].value, True)

    def test_readback_checks_real_root_and_eye_before_any_save(self):
        tree = ast.parse(SCRIPT.read_text())
        run = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'run')
        calls = [n for n in ast.walk(run) if isinstance(n, ast.Call)]
        self.assertEqual(sum(isinstance(n.func,ast.Name) and n.func.id=='verify_actual' for n in calls), 1)
        self.assertEqual(sum(isinstance(n.func,ast.Name) and n.func.id=='save_map' for n in calls), 1)
        text=SCRIPT.read_text()
        self.assertLess(text.index('sight=verify_actual(base,candidate)'),text.index("save_map(base['package'])"))
        for token in ('get_actor_eyes_view_point()', 'get_actor_rotation()',
                      'expected_eye_yaw=', 'actual_eye_yaw=', 'root_yaw=',
                      'physical observer eye offset changed unexpectedly'):
            self.assertIn(token,text)

    def test_no_fabricated_knowledge_or_unapproved_binary_writes(self):
        text=SCRIPT.read_text()
        for bad in ('RecordEvidence(', 'ReportConfirmedAction(',
                    'ResolveEncounter(', 'SetNavigationEnabled(', 'git reset', 'git clean'):
            self.assertNotIn(bad,text)
        self.assertIn('compare_plans(approved,public)',text)
        self.assertIn("'authoring':'NONE_READ_ONLY'",text)
        self.assertIn("'OBSERVER_ROOT_AND_YAW_ONLY_SAVED'",text)

if __name__ == '__main__': unittest.main()
