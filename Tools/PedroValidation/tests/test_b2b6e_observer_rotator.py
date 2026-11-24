"""UE 5.8 Python Rotator axis contract for guarded Pedro Editor placement.

The Python constructor's positional order is roll, pitch, yaw. C++ FRotator's
order differs; this test rejects positional construction of the authoring pose.
"""
from __future__ import annotations
import ast
from pathlib import Path
import unittest

SCRIPT=Path(__file__).resolve().parents[1]/'b2b6e_observer_placement_editor.py'

class RotatorAxisContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tree=ast.parse(SCRIPT.read_text())
        cls.run_node=next(n for n in cls.tree.body if isinstance(n,ast.FunctionDef) and n.name=='run')

    def test_editor_pose_uses_named_python_rotator_axes(self):
        calls=[n for n in ast.walk(self.run_node) if isinstance(n,ast.Call) and
               isinstance(n.func,ast.Attribute) and n.func.attr=='Rotator' and
               isinstance(n.func.value,ast.Name) and n.func.value.id=='unreal']
        self.assertEqual(len(calls),1)
        self.assertEqual(calls[0].args,[])
        self.assertEqual({k.arg for k in calls[0].keywords},{'roll','pitch','yaw'})
        self.assertEqual(len(calls[0].keywords),3)

    def test_axis_readback_precedes_teleport_and_save(self):
        text=SCRIPT.read_text()
        self.assertLess(text.index('requested_rotation=unreal.Rotator('),
                        text.index('actor.set_actor_location_and_rotation('))
        self.assertLess(text.index('Python Rotator axis mismatch before authoring'),
                        text.index('actor.set_actor_location_and_rotation('))
        self.assertLess(text.index('sight=verify_actual(base,candidate)'),
                        text.index("save_map(base['package'])"))
        self.assertIn('compare_plans(approved,public)',text)
        self.assertIn('False, True)',text)

    def test_invalid_pitch_order_is_not_reintroduced(self):
        # Reject the exact previous mistake, and keep the saved-map gate.
        text=SCRIPT.read_text()
        self.assertNotIn("unreal.Rotator(float(previous.pitch), float(candidate['sight']['yaw'])",text)
        self.assertIn('root_heading_error',text)
        self.assertIn('actual saved-world sight does not match approved',text)

if __name__=='__main__':unittest.main()
