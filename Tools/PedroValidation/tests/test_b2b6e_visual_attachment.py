"""Offline Unreal-Python contract tests; real Editor and game tests still needed."""
from __future__ import annotations
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[1]
AUTHOR = (ROOT / 'b2b6e_author_level.py').read_text()
AUDIT = (ROOT / 'b2b6e_saved_map_audit.py').read_text()


def function(source, name, env):
    node = next(node for node in ast.parse(source).body if isinstance(node, ast.FunctionDef)
                and node.name == name)
    code = compile(ast.Module(body=[node], type_ignores=[]), '<tested Unreal function>', 'exec')
    exec(code, env)
    return env[name]


class FakeMesh:
    def __init__(self):
        self.mobility = 'STATIC'
    def get_editor_property(self, name):
        if name == 'mobility':
            return self.mobility
        raise AttributeError(name)


class FakeVisual:
    def __init__(self):
        self.mesh = FakeMesh()
        self.parent = None
        self.collision = False
        self.attach_calls = []
        self.fail_attach = False
    def get_editor_property(self, name):
        if name in ('root_component', 'static_mesh_component'):
            return self.mesh
        raise AttributeError(name)
    def set_mobility(self, mobility):
        self.mesh.mobility = mobility
    def get_actor_enable_collision(self):
        return self.collision
    def get_attach_parent_actor(self):
        return self.parent
    def attach_to_actor(self, parent, socket, loc, rot, scale, weld):
        self.attach_calls.append((parent,socket,loc,rot,scale,weld))
        if self.fail_attach or self.mesh.mobility != 'MOVABLE':
            return False
        self.parent = parent
        return True


def fake_env():
    def prop(obj, name):
        return obj.get_editor_property(name)
    def require(valid, why):
        if not valid:
            raise RuntimeError(why)
    unreal = SimpleNamespace(ComponentMobility=SimpleNamespace(MOVABLE='MOVABLE'),
        AttachmentRule=SimpleNamespace(KEEP_WORLD='KEEP_WORLD'), Name=str)
    return dict(prop=prop, require=require, unreal=unreal)


class AttachmentTests(unittest.TestCase):
    def setUp(self):
        self.func = function(AUTHOR, 'attach_observer_visual', fake_env())
    def test_old_behavior_fails_on_static_root(self):
        visual = FakeVisual()
        self.assertFalse(visual.attach_to_actor(object(), '', '', '', '', False))
    def test_movable_cosmetic_actor_attaches_without_welding(self):
        visual, observer = FakeVisual(), object()
        self.assertIs(self.func(visual, observer), visual)
        self.assertIs(visual.parent, observer)
        self.assertEqual(visual.mesh.mobility, 'MOVABLE')
        self.assertEqual(visual.attach_calls[0][-1], False)
        self.assertEqual(visual.attach_calls[0][2:5], ('KEEP_WORLD',)*3)
        self.assertFalse(visual.collision)
    def test_actual_attachment_failure_still_blocks(self):
        visual=FakeVisual(); visual.fail_attach=True
        with self.assertRaisesRegex(RuntimeError, 'attachment was rejected'):
            self.func(visual, object())
    def test_unknown_existing_parent_rejected(self):
        visual=FakeVisual(); visual.parent=object()
        with self.assertRaisesRegex(RuntimeError, 'another attach parent'):
            self.func(visual, object())
    def test_collision_blocks(self):
        visual=FakeVisual(); visual.collision=True
        with self.assertRaisesRegex(RuntimeError, 'noncolliding'):
            self.func(visual, object())
    def test_only_visual_not_outcomes_moves(self):
        self.assertIn('attach_observer_visual(visual, observer)', AUTHOR)
        self.assertIn('visual_cube(unreal.Vector(grounded.x', AUTHOR)
        self.assertNotIn('a.set_mobility(', AUTHOR)
        self.assertEqual(AUTHOR.count('            attach_observer_visual(visual, observer)'), 1)
    def test_saved_audit_is_independent(self):
        self.assertIn('get_attach_parent_actor()', AUDIT)
        self.assertIn("visual['parent'] == o['path']", AUDIT)
        self.assertIn("'MOVABLE' in visual['mobility'].upper()", AUDIT)
        self.assertIn("not visual['collision_enabled']", AUDIT)


if __name__ == '__main__':
    unittest.main()
