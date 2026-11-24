"""Regression of the exact UE 5.8 Python sightline ray against a strict stub.

The Python docs specify HitResult | None for line_trace_single; the UE 5.8
HitResult's constructor fields are not necessarily reflected editor properties.
These tests never claim to execute the installed Unreal Editor.
"""
from __future__ import annotations
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest

ROOT=Path(__file__).resolve().parents[1]
EDITOR=ROOT/'b2b6e_visibility_editor.py'
PROJECT=ROOT.parents[1]


def load_actual_ray(response):
    module=ast.parse(EDITOR.read_text(encoding='utf-8'))
    fn=next(n for n in module.body if isinstance(n,ast.FunctionDef) and n.name=='inspect')
    ray=next(n for n in fn.body if isinstance(n,ast.FunctionDef) and n.name=='visibility_ray')
    code=compile(ast.fix_missing_locations(ast.Module(body=[ray], type_ignores=[])),str(EDITOR),'exec')
    class HitResult:  # No get_editor_property; reproduce the reported API limitation.
        pass
    calls=[]
    def trace(*args):
        calls.append(args)
        return response
    unreal=SimpleNamespace(
        Vector=lambda *x:x, HitResult=HitResult,
        TraceTypeQuery=SimpleNamespace(ECC_VISIBILITY='ECC_VISIBILITY'),
        DrawDebugTrace=SimpleNamespace(NONE='NONE'),
        SystemLibrary=SimpleNamespace(line_trace_single=trace))
    observer=object()
    env={'unreal':unreal,'world':object(),'observer':observer,
         'require':lambda ok,reason: None if ok else (_ for _ in ()).throw(RuntimeError(reason))}
    exec(code,env)
    actor=object()
    return env['visibility_ray'], HitResult, calls, observer, actor


class ActualEditorRay(unittest.TestCase):
    def test_no_blocking_hit_is_clear(self):
        ray,_,calls,observer,actor=load_actual_ray(None)
        self.assertEqual(ray((1,2,3),(9,8,7),{'actor':actor}),(True,''))
        self.assertEqual(len(calls),1)
        self.assertEqual(calls[0][3],'ECC_VISIBILITY')
        self.assertEqual(calls[0][5],[observer,actor])
        self.assertEqual(calls[0][6],'NONE')
        self.assertIs(calls[0][7],False)
    def test_real_occluding_hit_is_never_accepted(self):
        ray,HitResult,_,_,actor=load_actual_ray(None)
        # A real blocking result is opaque even when it has no exposed properties.
        ray.__globals__['unreal'].SystemLibrary.line_trace_single=lambda *args:HitResult()
        self.assertEqual(ray((0,0,0),(5,0,0),{'actor':actor}),
                         (False,'InterveningVisibilityBlocker'))
    def test_unexpected_api_type_fails_closed(self):
        ray,_,_,_,actor=load_actual_ray(False)
        with self.assertRaisesRegex(RuntimeError,'neither HitResult nor None'):
            ray((0,0,0),(2,0,0),{'actor':actor})
    def test_no_unreflected_hit_result_fields(self):
        s=EDITOR.read_text()
        self.assertNotIn("result.get_editor_property('blocking_hit')",s.split('def visibility_ray',1)[1].split('result=plan_rotation',1)[0])
        self.assertNotIn("get_editor_property('hit_actor')",s)
        self.assertIn('TraceTypeQuery.ECC_VISIBILITY',s)
        self.assertNotIn('TraceTypeQuery.TRACE_TYPE_QUERY1',s)
    def test_trace_not_softened_by_extra_ignore_actors(self):
        ray,_,calls,observer,actor=load_actual_ray(None)
        ray((0,0,0),(9,0,0),{'actor':actor})
        self.assertEqual(len(calls[0][5]),2)
        self.assertNotEqual(observer,actor)


if __name__=='__main__':
    unittest.main()
