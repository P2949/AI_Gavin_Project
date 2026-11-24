"""Editor-only saved-world inspection and strict B2b-6e verification.

Modes: inventory, probe, verify-test, verify-production. The optional test map
MUST exist in either verify mode. The existing baseline never needs playable
actors. This script never modifies a level or project asset.
"""
from __future__ import annotations
import json
import os
from pathlib import Path
import sys
import traceback

# Unreal embeds Python and may ignore PYTHONDONTWRITEBYTECODE at startup.
# Set the interpreter policy BEFORE importing either project-local module.
# Even if a future interpreter re-enables bytecode, redirect caches to Saved.
sys.dont_write_bytecode = True
_cache_prefix = os.environ.get('B2B6E_PYCACHE_PREFIX')
if _cache_prefix:
    _cache_dir = Path(_cache_prefix)
    if not _cache_dir.is_absolute():
        raise RuntimeError('PEDRO_B2B6E STOP:pycache prefix must be absolute')
    sys.pycache_prefix = str(_cache_dir)

# -ExecutePythonScript does not guarantee that this script's parent is in
# sys.path. The Editor launcher provides an absolute approved script root.
_script_dir = os.environ.get('B2B6E_SCRIPT_DIR')
sys.path.insert(0, _script_dir if _script_dir else str(Path(__file__).resolve().parent))

import unreal

from b2b6e_contract import BASELINE, BASELINE_HUB, PRODUCTION, TEST, TEST_DATA, TEST_HUB, TEST_ID
from b2b6e_editor_common import (
    EAL, asset_exists, director_facts, find, get_actors, label, load_map, path_of,
    prop, read_soft, require, source_rows, sorted_geometry_candidates, unique, details,
)

MODE = os.environ.get('B2B6E_AUDIT_MODE', 'inventory')
REPORT = Path(os.environ.get('B2B6E_AUDIT_REPORT', '/tmp/b2b6e-audit.json'))
MODES = ('inventory', 'probe', 'verify-test', 'verify-production', 'verify')


def inspect_map(package, *, geometry=False):
    world, actors = load_map(package)
    groups = {k: find(actors, k) for k in (
        'PedroEncounterDirector', 'PedroObserverCharacter',
        'PedroPlayableIntegrationActor', 'PedroObserverWatchPoint',
        'NavMeshBoundsVolume', 'RecastNavMesh', 'PedroValidationChoiceActor',
        'PedroPrototypeReturnVolume', 'EncounterEntranceActor')}
    facts = {
        'map': package,
        'actors': [details(a) for a in actors] if geometry else [
            {'label': label(a), 'class': str(a.get_class().get_name()), 'path': path_of(a),
             'location': [float(a.get_actor_location().x), float(a.get_actor_location().y), float(a.get_actor_location().z)]}
            for a in actors],
        'inventory': {
            'directors': [label(a) for a in groups['PedroEncounterDirector']],
            'observers': [label(a) for a in groups['PedroObserverCharacter']],
            'integrations': [label(a) for a in groups['PedroPlayableIntegrationActor']],
            'watch_points': [label(a) for a in groups['PedroObserverWatchPoint']],
            'navmesh_bounds': [label(a) for a in groups['NavMeshBoundsVolume']],
            'recast': [label(a) for a in groups['RecastNavMesh']],
            'validation_choice_actors': [label(a) for a in groups['PedroValidationChoiceActor']],
            'return_volumes': [label(a) for a in groups['PedroPrototypeReturnVolume']],
            'encounter_entrances': [label(a) for a in groups['EncounterEntranceActor']],
        },
        'sources': source_rows(actors),
    }
    if len(groups['PedroEncounterDirector']) == 1:
        facts['director'] = director_facts(groups['PedroEncounterDirector'][0])
    if len(groups['PedroObserverCharacter']) == 1:
        o = groups['PedroObserverCharacter'][0]
        facts['observer'] = {
            'path': path_of(o),
            'class': str(o.get_class().get_name()),
            'controller_class': str(prop(o, 'ai_controller_class')),
            'auto_possess_ai': str(prop(o, 'auto_possess_ai')),
            'can_be_damaged': bool(prop(o, 'can_be_damaged')),
        }
    if len(groups['PedroPlayableIntegrationActor']) == 1:
        i = groups['PedroPlayableIntegrationActor'][0]
        facts['integration'] = {
            'path': path_of(i),
            'observer': path_of(prop(i, 'observer')),
            'director': path_of(prop(i, 'director')),
            'watch_points': [path_of(w) for w in prop(i, 'watch_points')],
        }
    # Authoring records a cosmetic movable mesh as a child of the real Pawn.
    # Query the freshly reopened level, not an in-memory attachment claim.
    visual_actors = [a for a in actors if label(a) == 'PedroObserverVisual']
    facts['observer_visuals'] = [
        {'path': path_of(a),
         'parent': path_of(a.get_attach_parent_actor()),
         'mobility': str(prop(prop(a, 'static_mesh_component'), 'mobility')),
         'collision_enabled': bool(a.get_actor_enable_collision()),
         'class': str(a.get_class().get_name())}
        for a in visual_actors
    ]
    facts['watch_point_details'] = [
        {'path': path_of(w), 'id': str(prop(w, 'watch_point_id')),
         'enabled': bool(prop(w, 'enabled', 'b_enabled'))}
        for w in groups['PedroObserverWatchPoint']
    ]
    facts['return_maps'] = [read_soft(v, 'return_map') for v in groups['PedroPrototypeReturnVolume']]
    facts['entrance_definitions'] = [path_of(prop(e, 'encounter_definition')) for e in groups['EncounterEntranceActor']]
    if geometry:
        # A probe remains useful before navigation has ever been built. Its
        # geometry record explicitly reports the missing path rather than
        # claiming a fake projected point or aborting the whole inventory.
        try:
            facts['geometry'] = sorted_geometry_candidates(world, actors)
            facts['geometry_status'] = 'AVAILABLE'
        except (RuntimeError, AttributeError, TypeError) as exc:
            facts['geometry'] = {'anchors': [], 'paths': []}
            facts['geometry_status'] = 'NO_PROVEN_NAVIGATION: ' + str(exc)
        facts['editor_api'] = {
            'duplicate_asset': hasattr(unreal.EditorAssetLibrary, 'duplicate_asset'),
            'level_load': hasattr(unreal.LevelEditorSubsystem, 'load_level'),
            'level_save': hasattr(unreal.LevelEditorSubsystem, 'save_current_level'),
            'navigation_path': hasattr(unreal.NavigationSystemV1, 'find_path_to_location_synchronously'),
            'navigation_project': hasattr(unreal.NavigationSystemV1, 'project_point_to_navigation'),
            'console_command': hasattr(unreal.SystemLibrary, 'execute_console_command'),
        }
    return facts


def verify_playable(facts, expected_definition=None, expected_return=None):
    inv = facts['inventory']
    key = facts['map']
    require(len(inv['directors']) == 1 and len(inv['observers']) == 1 and len(inv['integrations']) == 1,
            f'{key}: expected one Director/physical Observer/integration; got {inv}')
    d, o, i = facts['director'], facts['observer'], facts['integration']
    require(d['direct'] and not d['assumed'] and 'EVIDENCEDRIVEN' in d['policy'].upper().replace('_', ''),
            f'{key}: acquisition/completion policy incorrect: {d}')
    require(i['observer'] == d['eye_actor'] == o['path'] and i['director'] == d['path'],
            f'{key}: Director/Observer/Integration reference mismatch')
    require('PedroObserverAIController' in o['controller_class'] and 'DISABLED' not in o['auto_possess_ai'].upper(),
            f'{key}: observer AI configuration invalid')
    require(not o['can_be_damaged'], f'{key}: observer still damageable')
    visuals = facts.get('observer_visuals', [])
    # The isolated test map has a newly authored native pawn and visual.
    # Production may reuse an existing physical observer with its own mesh.
    require(len(visuals) <= 1 and (len(visuals) == 1 or key != TEST),
            f'{key}: missing or duplicate Pedro observer visual')
    if visuals:
        visual = visuals[0]
        require(visual['class'] == 'StaticMeshActor' and
                visual['parent'] == o['path'] and
                'MOVABLE' in visual['mobility'].upper() and
                not visual['collision_enabled'],
                f'{key}: saved Pedro visual attachment/mobility/collision invalid: {visual}')
    require(not inv['validation_choice_actors'], f'{key}: scripted simulated-choice actors prohibited')
    require(inv['navmesh_bounds'], f'{key}: no saved navigation bounds')
    require(1 <= len(inv['watch_points']) <= 32, f'{key}: invalid watch-point count')
    require(len(set(i['watch_points'])) == len(i['watch_points']) == len(inv['watch_points']),
            f'{key}: integration watch-point list mismatch')
    actual_watch = facts['watch_point_details']
    require(set(i['watch_points']) == {w['path'] for w in actual_watch},
            f'{key}: integration watch-point object refs do not match saved actors')
    require(all(w['enabled'] and w['id'] not in ('', 'None') for w in actual_watch) and
            len({w['id'] for w in actual_watch}) == len(actual_watch),
            f'{key}: disabled/duplicate/unidentified watch point')
    require(d['sample_fraction'] == 0.75, f'{key}: core sampling threshold must be 0.75')
    defs = d['opportunities']
    ids = [x['id'] for x in defs]
    require(len(ids) == len(set(ids)) and all(x not in ('', 'None') for x in ids),
            f'{key}: invalid opportunity identities')
    core = [x['id'] for x in defs if 'CORE' in x['tier'].upper()]
    require(len(core) == 4, f'{key}: expected four original distinct core opportunities, got {core}')
    enabled = {x['opportunity'] for x in facts['sources'] if x['enabled']}
    require(set(core).issubset(enabled), f'{key}: missing real enabled core evidence sources {sorted(set(core)-enabled)}')
    outcomes = list(d['outcomes'].values())
    require(all(outcomes) and len(set(outcomes)) == 3,
            f'{key}: Gentle/Seeking/Disruptive outcome actors missing or aliased')
    if expected_definition is not None:
        require(d['definition'] == expected_definition, f'{key}: expected Director definition {expected_definition}, found {d["definition"]}')
    if expected_return is not None:
        require(facts['return_maps'] == [expected_return],
                f'{key}: return map must be exactly {expected_return}: {facts["return_maps"]}')
    # Fresh-game saved Recast/path feasibility is a separate required gate.
    return {'saved_actor_configuration': 'PASS', 'navigation': 'UNVERIFIED_UNTIL_GAME_PROCESS'}


def inspect_test_asset_links():
    require(all(asset_exists(p) for p in (TEST, TEST_HUB, TEST_DATA)),
            'test map, test hub and test definition must all exist')
    data = EAL.load_asset(TEST_DATA)
    require(data is not None, 'test definition could not be loaded')
    ident = str(prop(data, 'encounter_id'))
    require(ident == TEST_ID, f'nonunique test encounter ID {ident}; expected {TEST_ID}')
    require(read_soft(data, 'encounter_map') == TEST,
            f'test definition must reference isolated map {TEST}')
    production = inspect_map(PRODUCTION)
    require('director' in production, 'production Director missing for test identity comparison')
    source_path = production['director']['definition']
    require(source_path and source_path != path_of(data),
            'test definition must be different from production')
    source_def = EAL.load_asset(source_path)
    require(source_def is not None, 'cannot load production definition for test identity check')
    prod_id = str(prop(source_def, 'encounter_id'))
    require(prod_id != ident, f'test EncounterId duplicates production identity {ident}')
    hub = inspect_map(TEST_HUB)
    require(hub['entrance_definitions'].count(path_of(data)) == 1,
            'test hub must have exactly one entrance referencing test definition')
    require(len(hub['entrance_definitions']) == 1,
            'test hub still contains unrelated encounter entrance refs; cannot prove isolated progression')
    return {'definition': path_of(data), 'encounter_id': ident, 'encounter_map': TEST,
            'hub': TEST_HUB, 'entrance_definitions': hub['entrance_definitions'],
            'production_encounter_id': prod_id}


def run(mode):
    require(mode in MODES, 'invalid audit mode ' + mode)
    modes = ('verify-test', 'verify-production') if mode == 'verify' else (mode,)
    rows = {}
    for request in modes:
        if request in ('inventory', 'probe'):
            rows['production'] = inspect_map(PRODUCTION, geometry=request == 'probe')
            rows['observer_baseline'] = inspect_map(BASELINE, geometry=request == 'probe')
            if asset_exists(TEST):
                rows['autonomous_test'] = inspect_map(TEST, geometry=request == 'probe')
        elif request == 'verify-test':
            require(asset_exists(TEST), f'missing required test map {TEST}')
            rows['autonomous_test'] = inspect_map(TEST)
            links = inspect_test_asset_links()
            rows['test_asset_links'] = links
            verify_playable(rows['autonomous_test'], expected_definition=links['definition'], expected_return=TEST_HUB)
        elif request == 'verify-production':
            require(all(asset_exists(x) for x in (TEST, TEST_HUB, TEST_DATA)),
                    'production acceptance also requires isolated test assets')
            rows['autonomous_test'] = inspect_map(TEST)
            rows['test_asset_links'] = inspect_test_asset_links()
            verify_playable(rows['autonomous_test'],
                            expected_definition=rows['test_asset_links']['definition'],
                            expected_return=TEST_HUB)
            rows['production'] = inspect_map(PRODUCTION)
            verify_playable(rows['production'])
    return {
        'status': 'PASS', 'mode': mode, 'maps': rows,
        'limitations': 'No claim of real path following or game-process input; runtime smoke still required',
    }


def main():
    require(REPORT.is_absolute(), 'result path must be absolute')
    try:
        payload = run(MODE)
    except Exception as exc:
        payload = {'status': 'FAIL', 'mode': MODE, 'reason': str(exc), 'traceback': traceback.format_exc()}
        REPORT.parent.mkdir(parents=True, exist_ok=True)
        REPORT.write_text(json.dumps(payload, indent=2, sort_keys=True) + '\n')
        unreal.log_error('PEDRO_PLAYABLE_AUDIT status=FAIL mode=' + MODE + ' reason=' + str(exc))
        raise
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(payload, indent=2, sort_keys=True) + '\n')
    unreal.log('PEDRO_PLAYABLE_AUDIT status=PASS mode=' + MODE)


if __name__ == '__main__':
    main()
