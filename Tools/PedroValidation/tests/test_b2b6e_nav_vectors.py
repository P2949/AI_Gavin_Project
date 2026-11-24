"""Offline Unreal-Vector / JSON-anchor regression tests with simulated nav calls."""
from __future__ import annotations

import ast
import math
from pathlib import Path
import types
import unittest

TOOLS = Path(__file__).resolve().parents[1]
COMMON = TOOLS / 'b2b6e_editor_common.py'
AUTHOR = TOOLS / 'b2b6e_author_level.py'
if not AUTHOR.is_file():
    AUTHOR = Path(__file__).resolve().parents[3] / 'selftests/author_reference.py'


class FakeVector:
    def __init__(self, x, y, z):
        self.x, self.y, self.z = x, y, z


class FakeActor:
    def __init__(self, kind, name, point, extent=(100, 100, 100)):
        self.kind = kind
        self.name = name
        self.point = FakeVector(*point)
        self.extent = FakeVector(*extent)
    def get_actor_location(self):
        return self.point
    def get_actor_bounds(self, _):
        return self.point, self.extent


def extract(path, names, env):
    syntax = ast.parse(path.read_text())
    selected = [n for n in syntax.body if isinstance(n, ast.FunctionDef) and n.name in names]
    assert {n.name for n in selected} == set(names)
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(path), 'exec'), env)
    return env


class CoordinateBoundary(unittest.TestCase):
    def setUp(self):
        self.env = extract(COMMON, ('require', 'xyz', 'vec3', 'vector', 'dist'),
                           {'math': math, 'unreal': types.SimpleNamespace(Vector=FakeVector)})

    def test_unreal_vector_and_saved_json_triple_interoperate(self):
        d = self.env['dist']
        self.assertEqual(d(FakeVector(0, 0, 0), [3, 4, 0]), 5)
        self.assertEqual(d((3, 4, 0), FakeVector(0, 0, 0)), 5)
        self.assertEqual(d([0, 0, 0], (3, 4, 0)), 5)
        self.assertEqual(self.env['vec3'](FakeVector(1.23456, 2, -3)), [1.235, 2.0, -3.0])
        self.assertEqual(self.env['vec3']([1.23456, 2, -3]), [1.235, 2.0, -3.0])
        self.assertEqual(self.env['vector']((1, 2, 3)).z, 3)

    def test_actual_previous_code_reproduces_error(self):
        # Reproduce the exact previous helper logic against a serialized anchor.
        def previous_vec3(v):
            return [round(float(v.x), 3), round(float(v.y), 3), round(float(v.z), 3)]
        def previous_dist(a, b):
            return math.sqrt(sum((float(x)-float(y))**2
                                 for x, y in zip(previous_vec3(a), previous_vec3(b))))
        with self.assertRaisesRegex(AttributeError, "'list' object has no attribute 'x'"):
            previous_dist(FakeVector(0, 0, 0), [3, 4, 0])
        self.assertEqual(self.env['dist'](FakeVector(0, 0, 0), [3, 4, 0]), 5)

    def test_invalid_points_fail_closed(self):
        for point in (None, {}, '1,2,3', [], [1, 2], [1, 2, 3, 4],
                      [1, 2, float('nan')], (1, math.inf, 3), [1, True, 3]):
            with self.subTest(point=point), self.assertRaises(RuntimeError):
                self.env['xyz'](point)

    def test_distance_not_rounded_before_critical_threshold(self):
        # The saved report may round to 0.001 cm, but spacing logic must not.
        self.assertGreater(self.env['dist']([0, 0, 0], [.00049, 0, 0]), 0)
        self.assertEqual(self.env['vec3']([.00049, 0, 0]), [0.0, 0.0, 0.0])


class RealGeometryCodeSimulatedWorld(unittest.TestCase):
    def setUp(self):
        fake_unreal = types.SimpleNamespace(Vector=FakeVector)
        self.env = extract(COMMON,
            ('require', 'xyz', 'vec3', 'vector', 'dist', 'sorted_geometry_candidates'),
            {'math': math, 'unreal': fake_unreal})
        self.director = FakeActor('PedroEncounterDirector', 'PedroDirector', (0, 0, 0))
        self.source = FakeActor('PedroPrototypeInteractableActor', 'CoreEvidenceSource', (800, 0, 0))
        self.start = FakeActor('PlayerStart', 'PlayerStart', (1500, 0, 0))
        self.bounds = FakeActor('NavMeshBoundsVolume', 'NavBounds', (750, 0, 0), (900, 800, 200))
        self.actors = [self.director, self.source, self.start, self.bounds]
        self.env.update({
            'find': lambda actors, name: [a for a in actors if a.kind == name],
            'source_rows': lambda actors: [{'owner':'CoreEvidenceSource','position':[800,0,0]}],
            'has_class': lambda actor, kind: actor.kind == kind,
            'label': lambda actor: actor.name,
            'nav_project': lambda world, point: point if point is not None else None,
            'nav_path': lambda world, a, b: {'length': math.dist(self.env['xyz'](a),self.env['xyz'](b)),
                                             'start': self.env['vec3'](a),
                                             'end': self.env['vec3'](b)},
        })

    def test_real_geometry_iteration_handles_mixed_coordinates(self):
        result = self.env['sorted_geometry_candidates']('fake-world', self.actors)
        self.assertGreaterEqual(len(result['anchors']), 3)
        self.assertTrue(all(len(a['location']) == 3 for a in result['anchors']))
        self.assertTrue(any(row['length'] >= 750 for row in result['paths']))
        self.assertTrue(all(row['start'] != row['end'] for row in result['paths']))

    def test_missing_navigation_is_never_faked(self):
        self.env['nav_project'] = lambda world, point: None
        result = self.env['sorted_geometry_candidates']('fake-world', self.actors)
        self.assertEqual(result['anchors'], [])
        self.assertEqual(result['paths'], [])

    def test_author_selects_only_proven_actual_nav_paths(self):
        extract(AUTHOR, ('select_grounded_geometry',), self.env)
        result = self.env['select_grounded_geometry']('fake-world', self.actors)
        self.assertEqual(len(result['observer']), 3)
        self.assertGreaterEqual(result['longest_path']['length'], 750)
        self.assertGreaterEqual(len(result['watch_points']), 1)

    def test_build_navigation_does_not_claim_paths_without_any(self):
        extract(AUTHOR, ('build_navigation',), self.env)
        commands = []
        saved = []
        self.env['load_map'] = lambda package: ('fake-world', self.actors)
        self.env['save_map'] = lambda package: saved.append(package)
        self.env['MODE'] = 'build-test-nav'
        self.env['unreal'].SystemLibrary = types.SimpleNamespace(
            execute_console_command=lambda world, cmd: commands.append(cmd))
        self.env['nav_project'] = lambda world, point: None
        with self.assertRaisesRegex(RuntimeError, 'no traversable projected path'):
            self.env['build_navigation']('/Game/Fake')
        self.assertEqual(commands, ['RebuildNavigation'])
        self.assertEqual(saved, [])


if __name__ == '__main__':
    unittest.main()
