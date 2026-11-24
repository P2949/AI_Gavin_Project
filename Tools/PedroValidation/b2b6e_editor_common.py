"""Purely Editor-side Pedro asset helpers; unavailable in game and Shipping.

All changes must use Unreal APIs. Never manipulate .umap/.uasset bytes here.
"""
from __future__ import annotations
import math
import re

import unreal

from b2b6e_contract import BASELINE, BASELINE_DATA, BASELINE_HUB, PRODUCTION, TEST, TEST_DATA, TEST_HUB, TEST_ID

ROOT = '/Game/AI_Gavin/Encounters/Pedro'
EAL = unreal.EditorAssetLibrary
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EDITOR = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
NAV = unreal.NavigationSystemV1


def require(ok, reason):
    if not ok:
        raise RuntimeError('PEDRO_B2B6E STOP:' + str(reason))


def prop(obj, *names):
    errors = []
    for n in names:
        try:
            return obj.get_editor_property(n)
        except (AttributeError, RuntimeError, TypeError) as exc:
            errors.append(f'{n}={type(exc).__name__}({exc})')
    raise RuntimeError(f'PEDRO_B2B6E STOP:property {obj.get_name()} {names}: {errors}')


def setprop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except (AttributeError, RuntimeError, TypeError) as exc:
        raise RuntimeError(f'PEDRO_B2B6E STOP:set {obj.get_path_name()}.{name}: {exc}') from exc


def classname(actor):
    return str(actor.get_class().get_name())


def has_class(actor, typename):
    cls = getattr(unreal, typename, None)
    require(cls is not None, 'Unreal Python class unavailable: ' + typename)
    return isinstance(actor, cls)


def get_actors():
    return list(ACTORS.get_all_level_actors())


def label(actor):
    return str(actor.get_actor_label())


def find(actors, typename):
    return [a for a in actors if has_class(a, typename)]


def unique(actors, typename):
    found = find(actors, typename)
    require(len(found) == 1, f'expected exactly one {typename}; found {[(label(a), classname(a)) for a in found]}')
    return found[0]


def path_of(obj):
    return str(obj.get_path_name()) if obj is not None else None


def asset_exists(package):
    return bool(EAL.does_asset_exist(package))


def load_map(package):
    require(asset_exists(package), 'missing map package ' + package)
    require(LEVEL.load_level(package), 'cannot load level ' + package)
    world = EDITOR.get_editor_world()
    require(world is not None, 'no active Editor world after loading ' + package)
    current = world.get_outermost().get_name()
    require(current == package, f'loaded {current}, expected {package}')
    return world, get_actors()


def save_map(package):
    world = EDITOR.get_editor_world()
    require(world is not None and world.get_outermost().get_name() == package, 'save map identity mismatch')
    require(LEVEL.save_current_level(), 'could not save ' + package)


def xyz(value):
    """Accept a live Unreal Vector OR its serialized [x, y, z] snapshot.

    Nav projection returns unreal.Vector; geometry reports deliberately store
    triples as JSON lists. Keep conversion strict and *unrounded* for all
    geometry thresholds, and never silently truncate a malformed triple.
    """
    if isinstance(value, (list, tuple)):
        require(len(value) == 3, 'invalid three-coordinate point: ' + repr(value))
        coords = value
    else:
        try:
            coords = (value.x, value.y, value.z)
        except (AttributeError, TypeError) as exc:
            raise RuntimeError('PEDRO_B2B6E STOP:invalid vector or coordinate triple: ' +
                               repr(value)) from exc
    require(all(isinstance(n, (int, float)) and not isinstance(n, bool) and
                math.isfinite(float(n)) for n in coords),
            'nonfinite or nonnumeric coordinate triple: ' + repr(value))
    return tuple(float(n) for n in coords)


def vec3(value):
    return [round(n, 3) for n in xyz(value)]


def vector(value):
    # Unreal's NavigationSystemV1 accepts FVector, not Python lists. Convert
    # only at that API boundary; retain plain triples in JSON reports.
    return unreal.Vector(*xyz(value))


def dist(a, b):
    # No round-trip through JSON or quantization before path/spacing checks.
    aa, bb = xyz(a), xyz(b)
    return math.sqrt(sum((x - y)**2 for x, y in zip(aa, bb)))


def details(actor):
    origin, extent = actor.get_actor_bounds(False)
    return {'label': label(actor), 'class': classname(actor), 'path': path_of(actor),
            'location': vec3(actor.get_actor_location()),
            'rotation': str(actor.get_actor_rotation()),
            'bounds': {'center': vec3(origin), 'extent': vec3(extent)}}


def source_rows(actors):
    rows = []
    for actor in actors:
        for source in actor.get_components_by_class(unreal.PedroObservationSourceComponent):
            rows.append({
                'owner': label(actor), 'owner_path': path_of(actor),
                'subject': str(prop(source, 'subject_id')),
                'opportunity': str(prop(source, 'opportunity_id')),
                'enabled': bool(prop(source, 'evidence_enabled', 'b_evidence_enabled')),
                'position': vec3(actor.get_actor_location()),
                'source': path_of(source),
            })
    return rows


def director_facts(d):
    defs = list(prop(d, 'opportunity_definitions'))
    return {
        'path': path_of(d),
        'definition': path_of(prop(d, 'encounter_definition')),
        'direct': bool(prop(d, 'use_direct_observation_acquisition', 'b_use_direct_observation_acquisition')),
        'assumed': bool(prop(d, 'use_prototype_assumed_acquisition', 'b_use_prototype_assumed_acquisition')),
        'eye_actor': path_of(prop(d, 'direct_observation_actor')),
        'policy': str(prop(d, 'completion_policy')),
        'sample_fraction': float(prop(d, 'core_sample_fraction')),
        'opportunities': [{'id': str(prop(x, 'opportunity_id')), 'tier': str(prop(x, 'tier'))} for x in defs],
        'outcomes': {x: path_of(prop(d, x + '_outcome_actor')) for x in ('gentle', 'seeking', 'disruptive')},
    }


_SOFT = re.compile(r'/Game/[A-Za-z0-9_/-]+(?:\.[A-Za-z0-9_]+)?')


def canonical_soft_path(text):
    paths = set()
    for match in _SOFT.finditer(str(text)):
        val = match.group(0)
        if '.' in val.rsplit('/', 1)[-1]:
            pkg, name = val.rsplit('.', 1)
            if pkg.rsplit('/', 1)[-1] != name:
                continue
            val = pkg
        paths.add(val)
    require(len(paths) <= 1, 'ambiguous soft asset path: ' + str(text))
    return next(iter(paths)) if paths else None


def read_soft(obj, name):
    value = prop(obj, name)
    candidates = []
    try:
        candidates.append(unreal.SystemLibrary.conv_soft_object_reference_to_string(value))
    except (RuntimeError, AttributeError, TypeError):
        pass
    try:
        candidates.append(value.export_text())
    except (RuntimeError, AttributeError, TypeError):
        pass
    candidates.append(str(value))
    paths = {p for p in (canonical_soft_path(s) for s in candidates) if p}
    require(len(paths) == 1, f'{obj.get_name()}.{name}: cannot prove one soft world ref: {candidates}')
    return paths.pop()


def make_soft_world_reference(package):
    """Construct a SoftObjectProperty-compatible reference without loading levels.

    FSoftObjectPath is a *struct* and cannot itself be assigned to the UE 5.8
    Python SoftObjectProperty (the exact error in the failed prepare-test).
    UKismetSystemLibrary's documented conversion returns TSoftObjectPtr<UObject>,
    which Python exposes as an Object-valued soft reference. Crucially, this
    does NOT load/switch the target UWorld in the Editor. Verify the resulting
    path before allowing a property write, and verify the property afterwards.
    """
    require(asset_exists(package), 'soft target World must already exist: ' + package)
    name = package.rsplit('/', 1)[-1]
    path = unreal.SoftObjectPath(package + '.' + name)
    try:
        ref = unreal.SystemLibrary.conv_soft_obj_path_to_soft_obj_ref(path)
    except (AttributeError, RuntimeError, TypeError) as exc:
        raise RuntimeError('PEDRO_B2B6E STOP:soft-reference conversion unavailable: ' + str(exc)) from exc
    # This conversion returns a Soft Object Reference, not the FSoftObjectPath
    # struct. A null unresolved reference may still be valid if its path is real.
    try:
        rendered = unreal.SystemLibrary.conv_soft_object_reference_to_string(ref)
    except (AttributeError, RuntimeError, TypeError) as exc:
        raise RuntimeError('PEDRO_B2B6E STOP:soft-reference verification unavailable: ' + str(exc)) from exc
    require(canonical_soft_path(rendered) == package,
            f'soft-reference converter returned {rendered!r}, expected {package}')
    return ref


def set_soft_world(obj, property_name, package):
    # Never pass the unreal.SoftObjectPath struct directly to a
    # TSoftObjectPtr<UWorld> editor property: UE 5.8 rejects it.
    ref = make_soft_world_reference(package)
    setprop(obj, property_name, ref)
    require(read_soft(obj, property_name) == package,
            f'{obj.get_name()}.{property_name}: soft reference did not roundtrip to {package}')


def nav_project(world, point, radius=140.0):
    try:
        projected = NAV.project_point_to_navigation(world, point, None, None,
                                                     unreal.Vector(radius, radius, 250.0))
    except (RuntimeError, AttributeError, TypeError) as exc:
        raise RuntimeError('PEDRO_B2B6E STOP:nav projection API unavailable: ' + str(exc)) from exc
    return projected


def nav_path(world, a, b):
    try:
        result = NAV.find_path_to_location_synchronously(world, a, b, None, None)
    except (RuntimeError, AttributeError, TypeError) as exc:
        raise RuntimeError('PEDRO_B2B6E STOP:nav path API unavailable: ' + str(exc)) from exc
    if result is None or not result.is_valid() or result.is_partial():
        return None
    length = float(result.get_path_length())
    return {'length': round(length, 3), 'start': vec3(a), 'end': vec3(b)} if 150.0 <= length < 10000.0 else None


def sorted_geometry_candidates(world, actors):
    """Derive placement only from actual saved map actors and *verified* nav data.

    A nonempty outcome is evidence of saved geometry and path connectivity,
    not a claim about the character's actual AI path following.
    """
    directors = find(actors, 'PedroEncounterDirector')
    require(len(directors) == 1, 'geometry requires exactly one real Director')
    d = directors[0]
    points = []
    anchors = [('Director', d.get_actor_location())]
    for row in source_rows(actors):
        anchors.append((row['owner'], vector(row['position'])))
    for actor in actors:
        if has_class(actor, 'TargetPoint') or has_class(actor, 'PlayerStart'):
            anchors.append((label(actor), actor.get_actor_location()))
    # Do not choose arbitrary coordinates from the origin or default fallbacks.
    for name, pos in anchors:
        nav = nav_project(world, pos)
        if nav is None:
            continue
        if not any(dist(nav, entry['location']) < 95.0 for entry in points):
            points.append({'source': name, 'location': vec3(nav)})
    # Expand candidate coverage using the *measured* bounds and an actual
    # projected floor elevation. These are geometric samples, not guessed
    # world coordinates or synthetic navigation. Each must project to Recast.
    if points:
        floor_z = float(points[0]['location'][2])
        for bound in find(actors, 'NavMeshBoundsVolume'):
            center, extents = bound.get_actor_bounds(False)
            if not all(math.isfinite(float(x)) and float(x) > 0 for x in (extents.x, extents.y)):
                continue
            for xfrac in (-0.7, 0.0, 0.7):
                for yfrac in (-0.7, 0.0, 0.7):
                    nav = nav_project(world, unreal.Vector(
                        float(center.x) + float(extents.x) * xfrac,
                        float(center.y) + float(extents.y) * yfrac,
                        floor_z))
                    if nav is not None and not any(
                        dist(nav, entry['location']) < 95.0 for entry in points
                    ):
                        points.append({'source': f'{label(bound)}:{xfrac:+.1f},{yfrac:+.1f}',
                                       'location': vec3(nav)})
    require(len(points) <= 48, 'unexpectedly dense navigation geometry candidates')
    reachable = []
    for row in points:
        for other in points:
            if row is other or dist(vector(row['location']), vector(other['location'])) < 250.0:
                continue
            path = nav_path(world, vector(row['location']), vector(other['location']))
            if path:
                reachable.append({'from': row['source'], 'to': other['source'], **path})
    return {'anchors': points, 'paths': reachable}
