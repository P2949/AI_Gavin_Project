"""Mechanically author Pedro-owned maps inside Unreal Editor 5.8.2.

Operations are explicitly separated across fresh Editor processes:
    prepare-test: duplicate three baseline/prototype assets; rewrite test refs
    build-test-nav / build-production-nav: request an Editor nav build and save
    author-test / author-production: place all actors from proven nav geometry

No raw .umap/.uasset copies, manually entered coordinates, or synthetic actions.
Every mutating operation rejects an already completed/ambiguous state.
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

from b2b6e_contract import (BASELINE_DATA, BASELINE_HUB, PRODUCTION, TEST,
                            TEST_DATA, TEST_HUB, TEST_ID)
from b2b6e_editor_common import (
    ACTORS, EAL, asset_exists, director_facts, dist, find, label, load_map, nav_path,
    nav_project, path_of, prop, read_soft, require, save_map, make_soft_world_reference, set_soft_world,
    setprop, sorted_geometry_candidates, source_rows, unique, vector, vec3,
)

MODE = os.environ.get('B2B6E_AUDIT_MODE', '')
REPORT = Path(os.environ.get('B2B6E_AUDIT_REPORT', '/tmp/pedro-b2b6e-author.json'))
WRITES = ('prepare-test', 'build-test-nav', 'author-test',
          'build-production-nav', 'author-production')


def ensure_test_clone_source():
    required = (PRODUCTION, BASELINE_HUB, BASELINE_DATA)
    require(all(asset_exists(a) for a in required),
            'required saved source assets missing: ' + str([a for a in required if not asset_exists(a)]))
    world, actors = load_map(PRODUCTION)
    d = unique(actors, 'PedroEncounterDirector')
    require(len(find(actors, 'NavMeshBoundsVolume')) > 0,
            'existing production nav bounds are missing; cannot derive a safe clone')
    require(len(find(actors, 'PedroPrototypeReturnVolume')) == 1,
            'production must have one saved return volume')
    require(len(find(actors, 'PedroValidationChoiceActor')) == 0,
            'production source contains simulated choice actors')
    defs = director_facts(d)['opportunities']
    core = {x['id'] for x in defs if 'CORE' in x['tier'].upper()}
    require(len(core) == 4 and all(x in {s['opportunity'] for s in source_rows(actors)} for x in core),
            'four actual core opportunity sources are required')
    _, hub = load_map(BASELINE_HUB)
    require(len(find(hub, 'EncounterEntranceActor')) == 1,
            'test hub source must have exactly one real encounter entrance')
    data = EAL.load_asset(BASELINE_DATA)
    require(data is not None and str(prop(data, 'encounter_id')) not in ('', 'None'),
            'test definition baseline is missing encounter identity')
    return {'source_map': PRODUCTION, 'source_hub': BASELINE_HUB,
            'source_definition': BASELINE_DATA, 'core': sorted(core)}


def duplicate_exact(source, dest):
    require(not asset_exists(dest), 'refuse to overwrite existing asset ' + dest)
    require(EAL.duplicate_asset(source, dest) is not None,
            f'Editor duplicate_asset failed: {source} -> {dest}')
    require(asset_exists(dest), 'duplicated asset not in Registry: ' + dest)
    require(EAL.save_asset(dest, only_if_is_dirty=False), 'unable to save copied asset ' + dest)


def _stable_source_signature(actors):
    # Only Pedro's authored event sources are compared. Engine-generated nav
    # actors may differ between editor loads and are not reliable signatures.
    return sorted((s['owner'], s['subject'], s['opportunity'], s['enabled'],
                   tuple(s['position'])) for s in source_rows(actors))


def _check_pristine_partial_clone(proof):
    """Accept precisely the 3 duplicates left by the failed soft-ref write.

    This is a read-only preflight. It is NOT a generic reconcile/overwrite.
    In particular, it rejects any test map that has already been authored,
    rebound to its test hub/definition, or modified from the source schema.
    """
    require(all(asset_exists(p) for p in (TEST, TEST_HUB, TEST_DATA)),
            'partial test clone must contain all three new assets')
    data = EAL.load_asset(TEST_DATA)
    baseline = EAL.load_asset(BASELINE_DATA)
    require(data is not None and baseline is not None,
            'unable to load cloned/baseline definitions')
    # The interrupted run set EncounterId only in memory before throwing;
    # it did not save the definition. Accept only the original ID on disk.
    require(str(prop(data, 'encounter_id')) == str(prop(baseline, 'encounter_id')),
            'partial definition was edited; refusing automatic resume')
    require(read_soft(data, 'encounter_map') == read_soft(baseline, 'encounter_map'),
            'partial encounter_map differs from baseline; refusing resume')

    _, source_actors = load_map(PRODUCTION)
    source_director = unique(source_actors, 'PedroEncounterDirector')
    source_return = unique(source_actors, 'PedroPrototypeReturnVolume')
    source_facts = director_facts(source_director)
    source_return_map = read_soft(source_return, 'return_map')
    source_sources = _stable_source_signature(source_actors)

    _, cloned_actors = load_map(TEST)
    require(not find(cloned_actors, 'PedroObserverCharacter') and
            not find(cloned_actors, 'PedroPlayableIntegrationActor') and
            not find(cloned_actors, 'PedroObserverWatchPoint'),
            'partial test clone already contains playable authoring')
    cloned_director = unique(cloned_actors, 'PedroEncounterDirector')
    cloned_return = unique(cloned_actors, 'PedroPrototypeReturnVolume')
    clone_facts = director_facts(cloned_director)
    for key in ('definition', 'direct', 'assumed', 'policy',
                'sample_fraction', 'opportunities'):
        require(clone_facts[key] == source_facts[key],
                f'partial test map differs from original Director field {key}')
    require(clone_facts['outcomes'] == source_facts['outcomes'],
            'partial test map has modified outcome links')
    require(read_soft(cloned_return, 'return_map') == source_return_map,
            'partial test map return link was modified')
    require(_stable_source_signature(cloned_actors) == source_sources,
            'partial test map observation sources differ from production')

    _, baseline_hub = load_map(BASELINE_HUB)
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
    require(len(find(cloned_hub, 'EncounterEntranceActor')) == 1,
            'partial test hub has ambiguous entrances')
    return {'preflight': 'PRISTINE_DUPLICATES',
            'source': proof, 'assets': [TEST, TEST_HUB, TEST_DATA]}


def prepare_test():
    proof = ensure_test_clone_source()
    existence = [asset_exists(x) for x in (TEST, TEST_HUB, TEST_DATA)]
    require(all(existence) or not any(existence),
            'partially duplicated assets: expected either zero or exactly three; do not overwrite')
    resume = all(existence)
    if resume:
        _check_pristine_partial_clone(proof)
    else:
        # Asset APIs, never Python file copies. Each duplicate is a *new* asset.
        duplicate_exact(PRODUCTION, TEST)
        duplicate_exact(BASELINE_HUB, TEST_HUB)
        duplicate_exact(BASELINE_DATA, TEST_DATA)

    definition = EAL.load_asset(TEST_DATA)
    require(definition is not None, 'copied encounter definition could not load')
    # Validate the target reference via the official soft-ref converter before
    # changing any fields. Unlike loading a World, this cannot switch maps.
    make_soft_world_reference(TEST)
    setprop(definition, 'encounter_id', unreal.Name(TEST_ID))
    set_soft_world(definition, 'encounter_map', TEST)
    require(str(prop(definition, 'encounter_id')) == TEST_ID, 'test EncounterId did not roundtrip')
    require(EAL.save_loaded_asset(definition, only_if_is_dirty=False), 'cannot save test definition')
    _, level_actors = load_map(TEST)
    d = unique(level_actors, 'PedroEncounterDirector')
    r = unique(level_actors, 'PedroPrototypeReturnVolume')
    setprop(d, 'encounter_definition', definition)
    set_soft_world(r, 'return_map', TEST_HUB)
    require(prop(d, 'encounter_definition') == definition, 'Director retained production definition')
    save_map(TEST)
    _, hub = load_map(TEST_HUB)
    entrance = unique(hub, 'EncounterEntranceActor')
    setprop(entrance, 'encounter_definition', definition)
    require(prop(entrance, 'encounter_definition') == definition,
            'test hub entrance did not rebind to copied encounter definition')
    save_map(TEST_HUB)
    return {'operation': MODE, 'assets': [TEST, TEST_HUB, TEST_DATA],
            'test_encounter_id': TEST_ID, 'source': proof,
            'resumed_pristine_duplicates': resume,
            'saved_asset_links': 'NEEDS_INDEPENDENT_REOPEN'}


def build_navigation(package):
    world, actors = load_map(package)
    require(len(find(actors, 'NavMeshBoundsVolume')) >= 1,
            'cannot build navigation without grounded NavMeshBoundsVolume')
    require(hasattr(unreal.SystemLibrary, 'execute_console_command'),
            'installed Unreal API has no execute_console_command')
    # A console command has no success return. Never call this proof that nav
    # was saved; the actual path query in a fresh game process is mandatory.
    unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
    candidates = sorted_geometry_candidates(world, actors)
    require(candidates['paths'],
            'RebuildNavigation returned but no traversable projected path was found. '
            'Editor may still be building or navigation build may be locked; DO NOT mark PASS')
    save_map(package)
    return {'operation': MODE, 'map': package, 'sample_paths': candidates['paths'][:12],
            'navigation': 'EDITOR_QUERY_ONLY__FRESH_GAME_REQUIRED'}


def spawn_actor(actor_cls, where, name):
    actor = ACTORS.spawn_actor_from_class(actor_cls, where)
    require(actor is not None, 'failed spawning ' + name)
    actor.set_actor_label(name)
    require(label(actor) == name, f'wrong saved label for {name}: {label(actor)}')
    return actor


def visual_cube(where, name, scale, hidden):
    mesh = EAL.load_asset('/Engine/BasicShapes/Cube')
    require(mesh is not None, 'Engine BasicShapes/Cube missing')
    a = spawn_actor(unreal.StaticMeshActor, where, name)
    component = prop(a, 'static_mesh_component')
    require(component.set_static_mesh(mesh), f'failed setting visual mesh on {name}')
    a.set_actor_scale3d(unreal.Vector(*scale))
    a.set_actor_enable_collision(False)
    a.set_actor_hidden_in_game(hidden)
    return a


def attach_observer_visual(visual, observer):
    """Attach a purely cosmetic mesh actor to a moving physical Pedro pawn.

    Unreal's StaticMeshActor is Static by default. A Static child cannot attach
    to a Movable character. Change ONLY this visual (never the outcome props),
    disable collision/welding, then verify the actual attachment parent.
    """
    mesh = prop(visual, 'static_mesh_component')
    require(mesh is not None and prop(visual, 'root_component') == mesh,
            'observer visual must have the StaticMeshComponent as its root')
    visual.set_mobility(unreal.ComponentMobility.MOVABLE)
    require(prop(mesh, 'mobility') == unreal.ComponentMobility.MOVABLE,
            'observer visual mobility did not become Movable')
    require(not visual.get_actor_enable_collision(),
            'observer visual must remain noncolliding')
    require(visual.get_attach_parent_actor() is None,
            'observer visual unexpectedly has another attach parent')
    attached = visual.attach_to_actor(
        observer, unreal.Name(''),
        unreal.AttachmentRule.KEEP_WORLD,
        unreal.AttachmentRule.KEEP_WORLD,
        unreal.AttachmentRule.KEEP_WORLD,
        False,  # No physics welding for a cosmetic actor.
    )
    require(attached, 'movable visual actor attachment was rejected by Unreal')
    require(visual.get_attach_parent_actor() == observer,
            'visual attachment returned success without an observer parent')
    require(not visual.get_actor_enable_collision(),
            'observer visual enabled collision during attachment')
    return visual


def select_grounded_geometry(world, actors):
    rows = sorted_geometry_candidates(world, actors)
    require(len(rows['paths']) >= 1,
            'no proven nav paths between saved level actors; authoring cannot choose coordinates')
    # Prefer a path long enough to test autonomous following without teleport.
    paths = sorted(rows['paths'], key=lambda x: (-x['length'], x['from'], x['to']))
    proof = next((p for p in paths if p['length'] >= 750), None)
    require(proof is not None,
            'no genuine long path (>=750cm) between saved actor anchors; cannot prove follow')
    original = vector(proof['start'])
    verify = nav_project(world, original)
    require(verify is not None and dist(verify, original) < 30,
            'selected physical observer origin failed nav projection')
    points = []
    for row in rows['anchors']:
        loc = vector(row['location'])
        reachable = nav_path(world, original, loc)
        if reachable and reachable['length'] >= 150 and all(
            dist(loc, vector(p['location'])) > 120 for p in points
        ):
            points.append({'id': f'Auto_{len(points)+1:02}',
                           'source': row['source'], 'location': vec3(loc),
                           'length': reachable['length']})
        if len(points) >= 3:
            break
    require(points, 'no reachable distinct watch point')
    return {'observer': vec3(original), 'watch_points': points,
            'longest_path': proof}


def author(package):
    require(package in (PRODUCTION, TEST), 'unexpected authoring map')
    if package == TEST:
        require(all(asset_exists(p) for p in (TEST, TEST_HUB, TEST_DATA)),
                'test map/hub/definition not prepared')
    world, actors = load_map(package)
    d = unique(actors, 'PedroEncounterDirector')
    require(not find(actors, 'PedroValidationChoiceActor'), 'fake choice actor not allowed')
    require(not find(actors, 'PedroPlayableIntegrationActor'), 'integration already exists; refuse duplicate')
    require(len(find(actors, 'PedroObserverCharacter')) <= 1, 'duplicate physical observer')
    existing_outcomes = [prop(d, name + '_outcome_actor') for name in ('gentle', 'seeking', 'disruptive')]
    require(all(x is None for x in existing_outcomes) or
            (all(x is not None for x in existing_outcomes) and len({path_of(x) for x in existing_outcomes}) == 3),
            'partially authored or aliased outcomes; fail closed')
    defs = director_facts(d)['opportunities']
    core = {p['id'] for p in defs if 'CORE' in p['tier'].upper()}
    require(len(core) == 4 and len({x['id'] for x in defs}) == len(defs),
            'must preserve exactly four nonduplicate authored core opportunities')
    srows = source_rows(actors)
    require(core.issubset({s['opportunity'] for s in srows}),
            'missing real observation sources for core IDs')
    require(find(actors, 'NavMeshBoundsVolume'), 'no authored nav bounds')
    geometry = select_grounded_geometry(world, actors)
    origin = vector(geometry['observer'])
    all_sources = [s for a in actors for s in a.get_components_by_class(unreal.PedroObservationSourceComponent)]
    physical = find(actors, 'PedroObserverCharacter')
    # All validations above happen before any actor is spawned.
    with unreal.ScopedEditorTransaction('B2b6e mechanical Pedro playable authoring'):
        observer = physical[0] if physical else spawn_actor(unreal.PedroObserverCharacter,
                                                              origin, 'PedroPlayableObserver')
        if not physical:
            observer.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
        setprop(observer, 'ai_controller_class', unreal.PedroObserverAIController.static_class())
        setprop(observer, 'auto_possess_ai', unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
        setprop(observer, 'can_be_damaged', False)
        setprop(d, 'direct_observation_actor', observer)
        setprop(d, 'use_direct_observation_acquisition', True)
        setprop(d, 'use_prototype_assumed_acquisition', False)
        setprop(d, 'completion_policy', unreal.PedroCompletionPolicy.EVIDENCE_DRIVEN)
        setprop(d, 'core_sample_fraction', 0.75)
        for source in all_sources:
            if str(prop(source, 'opportunity_id')) in core:
                setprop(source, 'evidence_enabled', True)
        # A native Character is the real sensing/moving Pawn. The attached
        # mesh is strictly a visual shell: no colliding/logic proxy actor.
        if not physical:
            visual = visual_cube(unreal.Vector(origin.x, origin.y, origin.z + 85),
                                 'PedroObserverVisual', (0.65, 0.65, 1.7), False)
            attach_observer_visual(visual, observer)
        wp = []
        for point in geometry['watch_points']:
            a = spawn_actor(unreal.PedroObserverWatchPoint, vector(point['location']),
                            'PedroWatch_' + point['id'])
            setprop(a, 'watch_point_id', unreal.Name(point['id']))
            setprop(a, 'observation_radius', 1200.0)
            setprop(a, 'base_priority', 0.5)
            setprop(a, 'enabled', True)
            wp.append(a)
        if all(x is None for x in existing_outcomes):
            # Displayable, separately identified outcomes; hidden until Pedro
            # selects exactly one. Geometry comes from real nav anchors.
            placements = geometry['watch_points']
            for i, name in enumerate(('gentle', 'seeking', 'disruptive')):
                grounded = vector(placements[i % len(placements)]['location'])
                a = visual_cube(unreal.Vector(grounded.x, grounded.y, grounded.z + 120),
                                'PedroOutcome_' + name.title(),
                                ((0.7, 0.7, 0.7), (0.85, 0.85, 0.85), (1, 1, 1))[i],
                                True)
                setprop(d, name + '_outcome_actor', a)
        integration = spawn_actor(unreal.PedroPlayableIntegrationActor, origin,
                                  'PedroPlayableIntegration')
        setprop(integration, 'observer', observer)
        setprop(integration, 'director', d)
        setprop(integration, 'watch_points', wp)
        require(prop(integration, 'observer') == observer and prop(integration, 'director') == d,
                'integration refs did not roundtrip')
        require(len(prop(integration, 'watch_points')) == len(wp),
                'integration watch-point refs incomplete')
    save_map(package)
    return {'operation': MODE, 'map': package, 'derived_geometry': geometry,
            'saved_map': True, 'fresh_editor_reopen': 'REQUIRED',
            'game_process_nav_and_motion': 'REQUIRED'}


def run():
    require(MODE in WRITES, 'unsupported author mode ' + MODE)
    if MODE == 'prepare-test': return prepare_test()
    if MODE == 'author-test': return author(TEST)
    if MODE == 'author-production': return author(PRODUCTION)
    if MODE == 'build-test-nav': return build_navigation(TEST)
    if MODE == 'build-production-nav': return build_navigation(PRODUCTION)
    raise AssertionError('unreachable')


def main():
    require(REPORT.is_absolute(), 'report path must be absolute')
    try:
        result = {'status': 'PASS', 'mode': MODE, **run()}
    except Exception as exc:
        result = {'status': 'FAIL', 'mode': MODE,
                  'reason': str(exc), 'traceback': traceback.format_exc()}
        REPORT.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
        unreal.log_error('PEDRO_B2B6E_AUTHOR status=FAIL mode=' + MODE + ' reason=' + str(exc))
        raise
    REPORT.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    unreal.log('PEDRO_B2B6E_AUTHOR status=PASS mode=' + MODE +
               ' map=' + str(result.get('map', TEST)))


if __name__ == '__main__':
    main()
