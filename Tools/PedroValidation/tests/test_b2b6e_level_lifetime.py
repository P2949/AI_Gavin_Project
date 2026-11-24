"""Regression: a ULevel Actor wrapper must not survive load_level(map2).

The authoring helpers are extracted with AST so this test can simulate UE's
actual level-lifetime rule without importing an imaginary `unreal` module.
"""
from __future__ import annotations

import ast
from pathlib import Path
import unittest

TOOLS = Path(__file__).resolve().parents[1]
AUTHOR = TOOLS / 'b2b6e_author_level.py'

PRODUCTION = '/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype'
TEST = '/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest'
BASELINE_HUB = '/Game/AI_Gavin/Encounters/Pedro/Validation/L_Hub_Pedro_ObserverActorTest'
TEST_HUB = '/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Hub_Pedro_AutonomousTest'
BASELINE_DATA = '/Game/AI_Gavin/Encounters/Pedro/Validation/DA_Encounter_Pedro_ObserverActorTest'
TEST_DATA = '/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/DA_Encounter_Pedro_AutonomousTest'


class Actor:
    def __init__(self, kind: str, **properties):
        self.kind, self.properties, self.alive = kind, properties, True

    def get_editor_property(self, name):
        if not self.alive:
            raise RuntimeError(f'{self.kind}: Internal Error - ObjectInstance is null!')
        return self.properties[name]


class DataAsset:
    def __init__(self, path: str, encounter_id: str, encounter_map: str):
        self.path = path
        self.properties = {'encounter_id': encounter_id, 'encounter_map': encounter_map}

    def get_editor_property(self, name):
        return self.properties[name]

    def get_path_name(self):
        return self.path


class LevelLifetimeFixture:
    def __init__(self):
        self.original_definition = DataAsset(BASELINE_DATA + '.DA_Encounter_Pedro_ObserverActorTest',
                                             'PedroBaseline', PRODUCTION)
        self.copy_definition = DataAsset(TEST_DATA + '.DA_Encounter_Pedro_AutonomousTest',
                                         'PedroBaseline', PRODUCTION)
        self.definition_map = {BASELINE_DATA: self.original_definition, TEST_DATA: self.copy_definition}
        source_facts = {
            'definition': self.original_definition.path,
            'direct': False, 'assumed': True, 'policy': 'LegacyInteractionCount',
            'sample_fraction': 0.75,
            'opportunities': [{'id': 'A', 'tier': 'CORE'}],
            'outcomes': {'gentle': None, 'seeking': None, 'disruptive': None},
        }
        self.source_rows = [{'owner': 'A', 'subject': 'item', 'opportunity': 'A',
                             'enabled': True, 'position': [400, 100, 0]}]
        self.worlds = {
            PRODUCTION: [Actor('PedroEncounterDirector', facts=dict(source_facts)),
                         Actor('PedroPrototypeReturnVolume', return_map=BASELINE_HUB)],
            TEST: [Actor('PedroEncounterDirector', facts=dict(source_facts)),
                   Actor('PedroPrototypeReturnVolume', return_map=BASELINE_HUB)],
            BASELINE_HUB: [Actor('EncounterEntranceActor',
                                 encounter_definition=self.original_definition)],
            TEST_HUB: [Actor('EncounterEntranceActor',
                             encounter_definition=self.original_definition)],
        }
        self.active = None
        self.loads = []

    def load_map(self, name):
        if self.active is not None:
            for actor in self.worlds[self.active]:
                actor.alive = False
        self.active = name
        self.loads.append(name)
        # Simulate freshly loaded actors, separate UObject lifetimes.
        next_actors = [Actor(a.kind, **a.properties) for a in self.worlds[name]]
        self.worlds[name] = next_actors
        return object(), next_actors

    def compiled_check(self, source=AUTHOR, *, source_text=None):
        code_text = source.read_text(encoding='utf-8') if source_text is None else source_text
        root = ast.parse(code_text, filename=str(source))
        defs = [n for n in root.body if isinstance(n, ast.FunctionDef) and n.name in (
            '_stable_source_signature', '_check_pristine_partial_clone')]
        assert len(defs) == 2
        env = {
            'TEST': TEST, 'TEST_HUB': TEST_HUB, 'TEST_DATA': TEST_DATA,
            'PRODUCTION': PRODUCTION, 'BASELINE_HUB': BASELINE_HUB, 'BASELINE_DATA': BASELINE_DATA,
            'asset_exists': lambda p: p in self.definition_map or p in self.worlds,
            'EAL': type('EAL', (), {'load_asset': lambda _, p: self.definition_map.get(p)})(),
            'load_map': self.load_map,
            'require': lambda ok, message: None if ok else (_ for _ in ()).throw(RuntimeError(message)),
            'prop': lambda a, name: a.get_editor_property(name),
            'path_of': lambda obj: obj.get_path_name() if obj is not None else None,
            'read_soft': lambda a, name: a.get_editor_property(name),
            'find': lambda actors, kind: [a for a in actors if a.kind == kind],
            'unique': lambda actors, kind: next(a for a in actors if a.kind == kind),
            'director_facts': lambda actor: actor.get_editor_property('facts'),
            'source_rows': lambda actors: list(self.source_rows),
        }
        code = compile(ast.Module(body=defs, type_ignores=[]), str(AUTHOR), 'exec')
        exec(code, env)
        return env['_check_pristine_partial_clone']


class ActorLifetimeContract(unittest.TestCase):
    def test_preserves_pristine_hub_without_dereferencing_old_map_actor(self):
        f = LevelLifetimeFixture()
        result = f.compiled_check()({'from': PRODUCTION})
        self.assertEqual(result['preflight'], 'PRISTINE_DUPLICATES')
        self.assertEqual(f.loads, [PRODUCTION, TEST, BASELINE_HUB, TEST_HUB])
        self.assertTrue(all(not a.alive for a in f.worlds[BASELINE_HUB]))

    def test_mutated_clone_entrance_is_rejected_before_any_authoring(self):
        f = LevelLifetimeFixture()
        f.worlds[TEST_HUB][0].properties['encounter_definition'] = f.copy_definition
        with self.assertRaisesRegex(RuntimeError, 'modified entrance definition'):
            f.compiled_check()({'from': PRODUCTION})
        self.assertEqual(f.loads[-1], TEST_HUB)

    def test_mutated_director_is_rejected_before_hub_loading(self):
        f = LevelLifetimeFixture()
        f.worlds[TEST][0].properties['facts']['direct'] = True
        with self.assertRaisesRegex(RuntimeError, 'Director field direct'):
            f.compiled_check()({'from': PRODUCTION})
        self.assertEqual(f.loads, [PRODUCTION, TEST])

    def test_old_order_reproduces_actual_stale_object_failure(self):
        new_source = AUTHOR.read_text(encoding='utf-8')
        # Exact pre-patch block from the verified installed version, inverted
        # solely inside this test; no dependency on installer-only preimages.
        fixed = """    _, baseline_hub = load_map(BASELINE_HUB)
    # LevelEditorSubsystem.load_level() closes the previous Persistent Level.
    # Save a primitive value while this Actor is alive: a Python UObject wrapper
    # retained across the map change may throw "ObjectInstance is null".
    baseline_definition_path = path_of(prop(
        unique(baseline_hub, 'EncounterEntranceActor'), 'encounter_definition'))
    _, cloned_hub = load_map(TEST_HUB)
    cloned_entrance = unique(cloned_hub, 'EncounterEntranceActor')
    require(path_of(prop(cloned_entrance, 'encounter_definition')) ==
            baseline_definition_path,
            'partial test hub already has a modified entrance definition')
"""
        previous = """    _, baseline_hub = load_map(BASELINE_HUB)
    baseline_entrance = unique(baseline_hub, 'EncounterEntranceActor')
    _, cloned_hub = load_map(TEST_HUB)
    cloned_entrance = unique(cloned_hub, 'EncounterEntranceActor')
    require(path_of(prop(cloned_entrance, 'encounter_definition')) ==
            path_of(prop(baseline_entrance, 'encounter_definition')),
            'partial test hub already has a modified entrance definition')
"""
        self.assertEqual(new_source.count(fixed), 1)
        previous_source = new_source.replace(fixed, previous)
        with self.assertRaisesRegex(RuntimeError, 'ObjectInstance is null'):
            LevelLifetimeFixture().compiled_check(source_text=previous_source)({'from': PRODUCTION})
        self.assertEqual(LevelLifetimeFixture().compiled_check()({'from': PRODUCTION})['preflight'],
                         'PRISTINE_DUPLICATES')


if __name__ == '__main__':
    unittest.main()
