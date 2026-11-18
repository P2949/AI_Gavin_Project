"""B2b-5b2: UE 5.8.2 EDITOR-only, fail-closed map/Blueprint authoring.

Run ONLY using Tools/PedroValidation/b2b5b2_run.sh, which sets environment
variables and temporarily enables the necessary Editor plugins.

Modes:
  inspect -- source map inventory, no asset writes
  build   -- new map/asset creation and binding, never alter source levels
  verify  -- independently reopen destination assets and check references
  resume  -- guarded continuation after the known two-map-only failed build
  probe   -- read-only real-UE soft-reference path diagnostics

This script intentionally does not create gameplay evidence, simulate mouse
input, claim PIE results, remove any production asset, or touch another map.
"""

from __future__ import annotations

import json
import os
import re
from pathlib import Path
import traceback

import unreal

ROOT = "/Game/AI_Gavin/Encounters/Pedro/Validation"
SOURCE_PEDRO = ROOT + "/L_Pedro_DirectPlayTest"
SOURCE_HUB = ROOT + "/L_Hub_Pedro_DirectPlayTest"
SOURCE_DATA = ROOT + "/DA_Encounter_Pedro_DirectPlayTest"
TARGET_PEDRO = ROOT + "/L_Pedro_ObserverActorTest"
TARGET_HUB = ROOT + "/L_Hub_Pedro_ObserverActorTest"
TARGET_DATA = ROOT + "/DA_Encounter_Pedro_ObserverActorTest"
TARGET_BP = ROOT + "/BP_PedroObserver_Validation"
TARGET_ID = "PedroObserverActorTest"
OBSERVER_LABEL = "PEDRO_OBSERVER_ACTOR_Test"
ORIGINAL_EYE_LABEL = "PEDRO_DIRECT_PLAY_ObserverEye"

MODE = os.environ.get("B2B5B2_MODE", "inspect")
RUN_ID = os.environ.get("B2B5B2_RUN_ID", "manual")
RESULT_PATH = Path(os.environ.get("B2B5B2_RESULT_PATH", "/tmp/b2b5b2_report.json"))
PROJECT = Path(os.environ.get("B2B5B2_PROJECT", str(Path(unreal.Paths.project_dir())))).resolve()

EAL = unreal.EditorAssetLibrary
LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def info(message):
    unreal.log("B2B5B2: " + message)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def class_name(actor):
    return str(actor.get_class().get_name())


def actor_label(actor):
    return str(actor.get_actor_label())


def actor_id(actor):
    return {"class": class_name(actor), "label": actor_label(actor)}


def property_value(obj, *names):
    """Use a small, explicit alias set for UE Python property-name variations."""
    errors = []
    for name in names:
        try:
            return obj.get_editor_property(name)
        except (AttributeError, RuntimeError, TypeError) as exc:
            errors.append("%s: %s" % (name, exc))
    raise RuntimeError("Property lookup failed on %s. Tried %s. %s" %
                       (obj.get_name(), names, " | ".join(errors)))


def set_property(obj, value, *names):
    errors = []
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return name
        except (AttributeError, RuntimeError, TypeError) as exc:
            errors.append("%s: %s" % (name, exc))
    raise RuntimeError("Property write failed on %s. Tried %s. %s" %
                       (obj.get_name(), names, " | ".join(errors)))


_SOFT_ASSET_PATTERN = re.compile(r"/Game/[A-Za-z0-9_/-]+(?:\.[A-Za-z0-9_]+)?")


def _canonical_game_asset_path(candidate):
    """Normalize a full /Game path to its package name, without guessing.

    Examples: /Game/Maps/L_Hub and /Game/Maps/L_Hub.L_Hub are equivalent.
    A mismatched object name (such as .OtherAsset) is NOT equivalent.
    """
    if not isinstance(candidate, str):
        return None
    match = _SOFT_ASSET_PATTERN.fullmatch(candidate.strip())
    if match is None:
        return None
    path = match.group(0)
    if "." in path.rsplit("/", 1)[-1]:
        package, object_name = path.rsplit(".", 1)
        if object_name != package.rsplit("/", 1)[-1]:
            return None
        return package
    return path


def _extract_soft_path_from_text(text):
    """Parse a UE export/soft-reference string, not Python's struct repr.

    Unreal's StructBase __str__ may only show '<Struct SoftObjectPath {}>'.
    export_text() may instead include a full object path or a PackageName.
    The exact reference is checked later against the expected path.
    """
    if not isinstance(text, str):
        return None
    matches = set()
    for match in _SOFT_ASSET_PATTERN.finditer(text):
        normalized = _canonical_game_asset_path(match.group(0))
        if normalized:
            matches.add(normalized)
    # Never pick a candidate arbitrarily if export contains multiple assets.
    return next(iter(matches)) if len(matches) == 1 else None


def _soft_reference_details(value):
    """Read soft ref with supported UE 5.8 Python conversions.

    Attempt the explicitly documented soft-reference-to-string helper first;
    also try Unreal's struct export, which is NOT equivalent to str(struct).
    Never report success merely because a value was assigned to a UPROPERTY.
    """
    samples = {}
    paths = set()

    def attempt(label, producer):
        try:
            raw = producer()
            rendered = raw if isinstance(raw, str) else str(raw)
            samples[label] = rendered
            candidate = _extract_soft_path_from_text(rendered)
            if candidate:
                paths.add(candidate)
        except (AttributeError, RuntimeError, TypeError, ValueError) as exc:
            samples[label] = "ERROR: %s: %s" % (type(exc).__name__, exc)

    attempt("soft_reference_to_string",
            lambda: unreal.SystemLibrary.conv_soft_object_reference_to_string(value))
    attempt("raw_struct_export_text", lambda: value.export_text())

    def converted_path():
        return unreal.SystemLibrary.conv_soft_obj_ref_to_soft_obj_path(value)

    try:
        converted = converted_path()
    except (AttributeError, RuntimeError, TypeError, ValueError) as exc:
        samples["soft_ref_to_path"] = "ERROR: %s: %s" % (type(exc).__name__, exc)
    else:
        attempt("converted_struct_export_text", lambda: converted.export_text())
        attempt("converted_struct_to_string", lambda: str(converted))

    if isinstance(value, unreal.SoftObjectPath):
        attempt("soft_path_to_ref_to_string", lambda:
                unreal.SystemLibrary.conv_soft_object_reference_to_string(
                    unreal.SystemLibrary.conv_soft_obj_path_to_soft_obj_ref(value)))

    attempt("raw_python_str", lambda: str(value))
    # A conflicting result indicates that a conversion was not faithful.
    require(len(paths) <= 1,
            "Inconsistent UE soft-reference conversion paths: %s" % samples)
    return (next(iter(paths)) if paths else None), samples


def _read_soft_world_property(obj, prop):
    value = obj.get_editor_property(prop)
    path, diagnostics = _soft_reference_details(value)
    return path, diagnostics


def check_soft_world_property(obj, prop, asset_path):
    expected = _canonical_game_asset_path(asset_path)
    require(expected == asset_path, "Non-canonical expected map: " + asset_path)
    actual, diagnostics = _read_soft_world_property(obj, prop)
    require(actual == expected,
            "%s.%s: got %s, expected %s; UE conversion details: %s" %
            (obj.get_name(), prop, actual, expected, diagnostics))
    return actual


def set_soft_world_property(obj, prop, asset_path):
    """Write a level soft reference, verifying exact path after each attempt.

    Creation may persist partial assets. Refuse ambiguous values and log all
    conversion attempts rather than accepting a Python struct representation.
    """
    require(_canonical_game_asset_path(asset_path) == asset_path,
            "Non-canonical destination map: " + asset_path)
    full_path = asset_path + "." + asset_path.rsplit("/", 1)[-1]
    path = unreal.SoftObjectPath(full_path)
    candidates = [("SoftObjectPath", path)]
    try:
        candidates.append(("SoftObjectReference",
                           unreal.SystemLibrary.conv_soft_obj_path_to_soft_obj_ref(path)))
    except (AttributeError, RuntimeError, TypeError, ValueError) as exc:
        info("SoftObjectReference conversion unavailable: %s" % exc)
    diagnostics = []
    for label, candidate in candidates:
        try:
            obj.set_editor_property(prop, candidate)
            actual, samples = _read_soft_world_property(obj, prop)
            if actual == asset_path:
                info("Verified %s.%s = %s via %s" %
                     (obj.get_name(), prop, actual, label))
                return actual
            diagnostics.append("%s read %s (%s)" % (label, actual, samples))
        except (AttributeError, RuntimeError, TypeError, ValueError) as exc:
            diagnostics.append("%s failed: %s: %s" % (label, type(exc).__name__, exc))
    raise RuntimeError("Cannot set %s.%s to %s: %s" %
                       (obj.get_name(), prop, full_path, "; ".join(diagnostics)))

def asset_disk_path(asset_path, extension):
    require(asset_path.startswith("/Game/"), "Unexpected asset root: " + asset_path)
    return PROJECT / "Content" / (asset_path[len("/Game/"):] + extension)


def exists(asset_path, extension):
    if asset_disk_path(asset_path, extension).exists():
        return True
    try:
        return bool(EAL.find_asset_data(asset_path).is_valid())
    except (AttributeError, RuntimeError, TypeError):
        return False


def require_destination_absent():
    for path, ext in ((TARGET_PEDRO, ".umap"), (TARGET_HUB, ".umap"),
                      (TARGET_DATA, ".uasset"), (TARGET_BP, ".uasset")):
        require(not exists(path, ext),
                "Refusing to overwrite a pre-existing asset: " + path)


def require_sources():
    for path, ext in ((SOURCE_PEDRO, ".umap"), (SOURCE_HUB, ".umap"),
                      (SOURCE_DATA, ".uasset")):
        require(exists(path, ext), "Source asset unavailable: " + path)


def load_level(asset_path):
    info("Loading level " + asset_path)
    require(LEVELS.load_level(asset_path), "Could not load level " + asset_path)


def get_actors():
    return list(ACTORS.get_all_level_actors())


def exact_class(actors, expected):
    return [actor for actor in actors if class_name(actor) == expected]


def one_class(actors, expected):
    matches = exact_class(actors, expected)
    require(len(matches) == 1, "Expected 1 %s, found %d: %s" %
            (expected, len(matches), [actor_label(a) for a in matches]))
    return matches[0]


def one_label(actors, label):
    matches = [a for a in actors if actor_label(a) == label]
    require(len(matches) == 1,
            "Expected one actor named %s, found %d" % (label, len(matches)))
    return matches[0]


def vec(value):
    return [round(float(value.x), 3), round(float(value.y), 3),
            round(float(value.z), 3)]


def rot(value):
    return {"pitch": round(float(value.pitch), 3),
            "yaw": round(float(value.yaw), 3),
            "roll": round(float(value.roll), 3)}


def basic_director_facts(director):
    definitions = list(property_value(director, "opportunity_definitions"))
    opportunities = []
    for definition in definitions:
        opportunities.append({
            "id": str(property_value(definition, "opportunity_id")),
            "tier": str(property_value(definition, "tier")),
        })
    outcomes = {}
    for label in ("gentle", "seeking", "disruptive"):
        actor = property_value(director, label + "_outcome_actor")
        require(actor is not None, "Missing %s outcome actor" % label)
        require(actor.get_level() == director.get_level(),
                "%s outcome actor points outside the Director's level" % label)
        outcomes[label] = actor_label(actor)
    require(len(set(outcomes.values())) == 3,
            "Evidence-driven completion requires three distinct outcomes")
    return {
        "opportunities": opportunities,
        "outcomes": outcomes,
        "source_direct_observation": bool(property_value(
            director, "use_direct_observation_acquisition",
            "b_use_direct_observation_acquisition")),
        "source_assumed": bool(property_value(
            director, "use_prototype_assumed_acquisition",
            "b_use_prototype_assumed_acquisition")),
        "core_sample_fraction": float(property_value(
            director, "core_sample_fraction")),
        "direct_max_distance": float(property_value(
            director, "direct_observation_max_distance")),
        "direct_half_fov": float(property_value(
            director, "direct_observation_view_half_angle_degrees")),
    }


def check_director_facts(facts):
    core = [o for o in facts["opportunities"] if "core" in o["tier"].lower()]
    require(len(core) == 4, "Expected 4 existing core opportunities; got %d. "
            "Review DirectPlayTest before automating." % len(core))
    require(len(set(o["id"] for o in facts["opportunities"])) ==
            len(facts["opportunities"]), "Duplicate opportunity IDs")
    require(facts["source_direct_observation"] is True,
            "DirectPlayTest is not configured for direct sight")
    require(facts["source_assumed"] is False,
            "DirectPlayTest enables assumed knowledge unexpectedly")
    require(abs(facts["core_sample_fraction"] - 0.75) < 0.0001,
            "Expected DirectPlayTest completion threshold 0.75")


def inspect_pedro(asset_path, is_source):
    load_level(asset_path)
    actors = get_actors()
    director = one_class(actors, "PedroEncounterDirector")
    return_volume = one_class(actors, "PedroPrototypeReturnVolume")
    interactables = exact_class(actors, "PedroPrototypeInteractableActor")
    require(len(interactables) >= 3, "Expected at least 3 real inspection actors")
    facts = basic_director_facts(director)
    check_director_facts(facts)
    eye = one_label(actors, ORIGINAL_EYE_LABEL) if is_source else one_label(actors, OBSERVER_LABEL)
    if is_source:
        require(class_name(eye) == "TargetPoint",
                "Original observation actor must be a TargetPoint")
    return {
        "map": asset_path, "actor_count": len(actors),
        "director": actor_id(director),
        "director_facts": facts,
        "return_volume": actor_id(return_volume),
        "interactables": [actor_id(a) for a in interactables],
        "eye": {**actor_id(eye), "position": vec(eye.get_actor_location()),
                "rotation": rot(eye.get_actor_rotation())},
    }


def inspect_hub(asset_path):
    load_level(asset_path)
    actors = get_actors()
    entrance = one_class(actors, "EncounterEntranceActor")
    return {"map": asset_path, "actor_count": len(actors),
            "entrance": actor_id(entrance)}


def inspect():
    require_sources()
    require_destination_absent()
    pedro = inspect_pedro(SOURCE_PEDRO, is_source=True)
    hub = inspect_hub(SOURCE_HUB)
    info("Source inspection found 4 core opportunities and the physical "
         "observer TargetPoint to replace")
    return {"source_pedro": pedro, "source_hub": hub}


def ensure_api():
    require(hasattr(LEVELS, "new_level_from_template"),
            "LevelEditorSubsystem.new_level_from_template unavailable")
    require(hasattr(unreal, "BlueprintEditorLibrary"),
            "BlueprintEditorLibrary unavailable")
    require(hasattr(unreal.BlueprintEditorLibrary,
                    "create_blueprint_asset_with_parent"),
            "Blueprint creation helper unavailable")
    require(hasattr(unreal, "SubobjectDataSubsystem"),
            "SubobjectDataSubsystem unavailable")
    require(hasattr(unreal, "SubobjectDataBlueprintFunctionLibrary"),
            "SubobjectDataBlueprintFunctionLibrary unavailable")
    require(hasattr(unreal, "AddNewSubobjectParams"),
            "AddNewSubobjectParams unavailable")
    require(hasattr(unreal, "PedroObserverCharacter"),
            "PedroObserverCharacter UClass not exported to Python. "
            "Check Editor module build and restart.")
    require(unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem) is not None,
            "SubobjectDataSubsystem not initialized")


def create_bp():
    info("Creating Blueprint child " + TARGET_BP)
    bp = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(
        TARGET_BP, unreal.PedroObserverCharacter)
    require(bp is not None, "Blueprint creation failed")
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    bf = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = list(subsystem.k2_gather_subobject_data_for_blueprint(bp))
    require(handles, "No blueprint subobject handles")
    # Native ACharacter calls its inherited capsule CollisionCylinder, not
    # CapsuleComponent. Selecting by class as well as the two known engine
    # component names avoids a false negative on UE 5.8.2.
    capsule_handles = []
    for handle in handles:
        candidate = bf.get_object(bf.get_data(handle))
        if (candidate is not None
                and isinstance(candidate, unreal.CapsuleComponent)
                and candidate.get_name() in ("CollisionCylinder", "CapsuleComponent")):
            capsule_handles.append(handle)
    require(len(capsule_handles) == 1,
            "Cannot identify exactly one inherited Character capsule "
            "(CollisionCylinder/CapsuleComponent); subobjects: %s" %
            [str(bf.get_object(bf.get_data(h))) for h in handles])
    parent = capsule_handles[0]
    info("Selected inherited Character capsule: " +
         str(bf.get_object(bf.get_data(parent)).get_name()))
    new_handle, failure = subsystem.add_new_subobject(
        unreal.AddNewSubobjectParams(parent_handle=parent,
                                     new_class=unreal.StaticMeshComponent,
                                     blueprint_context=bp))
    require(new_handle is not None and not str(failure or "").strip(),
            "Could not add ValidationBody: " + str(failure))
    rename_result = subsystem.rename_subobject(new_handle, unreal.Text("ValidationBody"))
    require(rename_result is not False,
            "Cannot rename validation body subobject")
    # add_new_subobject already creates the component beneath parent_handle.
    # Do NOT redundantly attach the same subobject: some engine versions
    # reject repeated attachment even though the component is already a child.
    body = bf.get_object(bf.get_data(new_handle))
    require(body is not None, "Cannot retrieve ValidationBody component")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    require(cube is not None, "Engine Cube static mesh unavailable")
    body.set_editor_property("static_mesh", cube)
    body.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
    body.set_editor_property("relative_scale3d", unreal.Vector(0.4, 0.4, 1.2))
    body.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    require(unreal.BlueprintEditorLibrary.compile_blueprint(bp) is not False,
            "Blueprint did not compile")
    require(EAL.save_loaded_asset(bp), "Could not save Blueprint")
    bp = None
    loaded_class = EAL.load_blueprint_class(TARGET_BP)
    require(loaded_class is not None, "Blueprint generated class not found")
    return loaded_class


def create_data():
    info("Duplicating encounter Data Asset")
    data = EAL.duplicate_asset(SOURCE_DATA, TARGET_DATA)
    require(data is not None, "Could not duplicate encounter definition")
    data.set_editor_property("encounter_id", unreal.Name(TARGET_ID))
    set_soft_world_property(data, "encounter_map", TARGET_PEDRO)
    require(str(data.get_editor_property("encounter_id")) == TARGET_ID,
            "Encounter ID did not persist in memory")
    require(EAL.save_loaded_asset(data), "Could not save copied encounter definition")
    return data


def build_hub(data):
    info("Opening copied hub level to bind entrance")
    load_level(TARGET_HUB)
    actors = get_actors()
    entrance = one_class(actors, "EncounterEntranceActor")
    entrance.set_editor_property("encounter_definition", data)
    require(property_value(entrance, "encounter_definition") == data,
            "Hub entrance failed to bind copied definition")
    require(LEVELS.save_current_level(), "Failed to save copied hub map")
    info("Saved copied hub with updated entrance")


def build_pedro(data, bp_class):
    info("Opening copied Pedro encounter level to bind observer")
    load_level(TARGET_PEDRO)
    actors = get_actors()
    director = one_class(actors, "PedroEncounterDirector")
    return_volume = one_class(actors, "PedroPrototypeReturnVolume")
    previous_eye = one_label(actors, ORIGINAL_EYE_LABEL)
    require(class_name(previous_eye) == "TargetPoint",
            "Previous observer eye was not the expected TargetPoint")
    location = previous_eye.get_actor_location()
    rotation = previous_eye.get_actor_rotation()

    # The Character's ObserverEye has a local (0,0,64) offset. For the
    # standard DirectPlay TargetPoint (no eye offset), align the new eye at
    # its world position. The validation transform is not used for AI movement.
    local_eye_offset = unreal.Vector(0.0, 0.0, 64.0)
    up = unreal.MathLibrary.get_up_vector(rotation)
    world_eye_offset = unreal.Vector(up.x * local_eye_offset.z,
                                     up.y * local_eye_offset.z,
                                     up.z * local_eye_offset.z)
    root_location = unreal.Vector(location.x - world_eye_offset.x,
                                  location.y - world_eye_offset.y,
                                  location.z - world_eye_offset.z)
    observer = ACTORS.spawn_actor_from_class(bp_class, root_location, rotation)
    require(observer is not None, "Cannot spawn physical observer")
    observer.set_actor_label(OBSERVER_LABEL, mark_dirty=True)
    new_eye = property_value(observer, "observer_eye")
    require(new_eye is not None, "Physical observer has no ObserverEye")
    actual_eye = new_eye.get_world_location()
    displacement = ((actual_eye.x - location.x) ** 2 +
                    (actual_eye.y - location.y) ** 2 +
                    (actual_eye.z - location.z) ** 2) ** 0.5
    require(displacement <= 5.0,
            "Physical eye differs from old TargetPoint by %.2f cm. "
            "Review Character transform rather than baking a bad test." % displacement)

    director.set_editor_property("encounter_definition", data)
    set_property(director, False,
                 "use_prototype_assumed_acquisition", "b_use_prototype_assumed_acquisition")
    set_property(director, True,
                 "use_direct_observation_acquisition", "b_use_direct_observation_acquisition")
    director.set_editor_property("direct_observation_actor", observer)
    director.set_editor_property("completion_policy",
                                 unreal.PedroCompletionPolicy.EVIDENCE_DRIVEN)
    director.set_editor_property("core_sample_fraction", 0.75)
    # B2b-5b2 playable observer: forward-facing, not omnidirectional.
    director.set_editor_property(
        "direct_observation_view_half_angle_degrees", 75.0)
    set_soft_world_property(return_volume, "return_map", TARGET_HUB)
    require(property_value(director, "direct_observation_actor") == observer,
            "Director is not bound to physical observer")
    require(ACTORS.destroy_actor(previous_eye),
            "Cannot remove old TargetPoint in copied map")
    require(LEVELS.save_current_level(), "Could not save copied Pedro map")
    info("Saved copied Pedro map with physically hosted observation eye")


def verify():
    for path, ext in ((TARGET_PEDRO, ".umap"), (TARGET_HUB, ".umap"),
                      (TARGET_DATA, ".uasset"), (TARGET_BP, ".uasset")):
        require(exists(path, ext), "Missing destination asset: " + path)
    data = EAL.load_asset(TARGET_DATA)
    require(data is not None, "Could not load cloned Data Asset")
    require(str(data.get_editor_property("encounter_id")) == TARGET_ID,
            "Copied encounter definition uses wrong ID")
    encounter_path = check_soft_world_property(data, "encounter_map", TARGET_PEDRO)
    bp_class = EAL.load_blueprint_class(TARGET_BP)
    require(bp_class is not None, "Could not load generated observer Blueprint class")

    load_level(TARGET_PEDRO)
    actors = get_actors()
    director = one_class(actors, "PedroEncounterDirector")
    return_volume = one_class(actors, "PedroPrototypeReturnVolume")
    observer = one_label(actors, OBSERVER_LABEL)
    require("BP_PedroObserver_Validation" in str(observer.get_class().get_name()),
            "Observer actor does not use the requested Blueprint class")
    require(not any(actor_label(a) == ORIGINAL_EYE_LABEL for a in actors),
            "Old TargetPoint still present in copied Pedro map")
    require(property_value(director, "direct_observation_actor") == observer,
            "Director physical observer binding not persisted")
    require(property_value(director, "encounter_definition") == data,
            "Pedro director encounter definition reference wrong")
    require(bool(property_value(director,
                                "use_direct_observation_acquisition",
                                "b_use_direct_observation_acquisition")),
            "Direct observation disabled")
    require(not bool(property_value(director,
                                   "use_prototype_assumed_acquisition",
                                   "b_use_prototype_assumed_acquisition")),
            "Prototype assumed mode should be disabled")
    require(str(property_value(director, "completion_policy")).lower().replace("_", "").find(
        "evidencedriven") >= 0,
        "Evidence-driven completion not enabled")
    facts = basic_director_facts(director)
    check_director_facts(facts)

    # Reject temporary negative-test settings left in the saved map.
    require(
        abs(facts["direct_max_distance"] - 5000.0) < 0.001,
        "Final playable observation distance must be 5000 cm"
    )
    require(
        abs(facts["direct_half_fov"] - 75.0) < 0.001,
        "Final playable observation half-angle must be 75 degrees"
    )

    return_map_path = check_soft_world_property(return_volume, "return_map", TARGET_HUB)
    require(len(exact_class(actors, "PedroPrototypeInteractableActor")) >= 3,
            "Playable inspected props missing")
    # UFUNCTION GetObserverEye is reflected in Python. Read the component's
    # world transform if possible. A missing API is reported as diagnostic,
    # not silently assumed to mean correct geometry.
    eye_component = property_value(observer, "observer_eye")
    require(eye_component is not None, "ObserverEye component missing")
    eye_location = eye_component.get_world_location()
    eye_forward = eye_component.get_world_rotation()

    expected_eye = (-910.0, 0.0, 210.0)
    actual_eye = (eye_location.x, eye_location.y, eye_location.z)

    require(
        sum((a - b) ** 2 for a, b in
            zip(actual_eye, expected_eye)) < 0.01,
        "Final observer eye position was not restored"
    )

    for axis, expected in (
        ("pitch", -23.199),
        ("yaw", 0.0),
        ("roll", 0.0),
    ):
        actual = getattr(eye_forward, axis)
        difference = abs((actual - expected + 180) % 360 - 180)

        require(
            difference < 0.1,
            "Final observer eye %s was not restored" % axis
        )

    actor_eye_location, actor_eye_rotation = observer.get_actor_eyes_view_point()
    distance = ((eye_location.x - actor_eye_location.x) ** 2 +
                (eye_location.y - actor_eye_location.y) ** 2 +
                (eye_location.z - actor_eye_location.z) ** 2) ** 0.5
    require(distance < 0.1,
            "Character GetActorEyesViewPoint does not use ObserverEye component")
    require(abs(eye_forward.yaw - actor_eye_rotation.yaw) < 0.1,
            "Character eye rotation unexpectedly differs from ObserverEye")
    pedro_result = {
        "map": TARGET_PEDRO,
        "director": actor_id(director),
        "observer": {**actor_id(observer),
                     "position": vec(observer.get_actor_location()),
                     "eye_position": vec(eye_location),
                     "eye_rotation": rot(eye_forward)},
        "facts": facts, "return_map": return_map_path,
        "inspections": [actor_id(a) for a in
                        exact_class(actors, "PedroPrototypeInteractableActor")],
    }

    load_level(TARGET_HUB)
    entrance = one_class(get_actors(), "EncounterEntranceActor")
    require(property_value(entrance, "encounter_definition") == data,
            "Copied hub entrance points to wrong encounter definition")
    return {"copied_data_path": encounter_path,
            "copied_pedro": pedro_result,
            "copied_hub": {"map": TARGET_HUB,
                           "entrance": actor_id(entrance)}}


def _normalized_inventory(actors):
    """Check that the saved map copies have not gained/lost gameplay actors."""
    return sorted((class_name(a), actor_label(a)) for a in actors
                  if class_name(a) != "WorldSettings")


def _resume_pedro_inventory(path):
    # Reuse existing source inspection: it asserts the old TargetPoint still
    # exists, one Director and ReturnVolume exist, and 4 core opportunities
    # retain correct source acquisition settings. This must be true in BOTH.
    summary = inspect_pedro(path, is_source=True)
    level_actors = get_actors()
    director = one_class(level_actors, "PedroEncounterDirector")
    return_volume = one_class(level_actors, "PedroPrototypeReturnVolume")
    eye = one_label(level_actors, ORIGINAL_EYE_LABEL)
    require(property_value(director, "direct_observation_actor") == eye,
            "Partial map no longer references its original observer TargetPoint")
    require(check_soft_world_property(return_volume, "return_map", SOURCE_HUB),
            "Partial Pedro return volume no longer points at source hub")
    source_data = EAL.load_asset(SOURCE_DATA)
    require(source_data is not None, "Source encounter definition not found")
    require(property_value(director, "encounter_definition") == source_data,
            "Partial Pedro map Director was already rebound; manual review needed")
    return summary, _normalized_inventory(level_actors)


def _resume_hub_inventory(path):
    summary = inspect_hub(path)
    level_actors = get_actors()
    source_data = EAL.load_asset(SOURCE_DATA)
    require(source_data is not None, "Source encounter definition not found")
    entrance = one_class(level_actors, "EncounterEntranceActor")
    require(property_value(entrance, "encounter_definition") == source_data,
            "Partial hub entrance has been rebound; manual review needed")
    return summary, _normalized_inventory(level_actors)


def probe():
    """Non-writing diagnostic for soft-map references in the known partial state.

    Proves that the saved source and copied Pedro maps really point at the old
    hub, and that the source Data Asset really references the old Pedro map.
    Executes before resume can create ANY additional assets.
    """
    require_sources()
    for path, ext in ((TARGET_PEDRO, ".umap"), (TARGET_HUB, ".umap")):
        require(exists(path, ext), "Missing partial map for probe: " + path)
    for path in (TARGET_DATA, TARGET_BP):
        require(not exists(path, ".uasset"),
                "Probe expects two-map-only partial state: " + path)
    references = []
    for path in (SOURCE_PEDRO, TARGET_PEDRO):
        load_level(path)
        volume = one_class(get_actors(), "PedroPrototypeReturnVolume")
        actual, methods = _read_soft_world_property(volume, "return_map")
        item = {"map": path, "actor": actor_label(volume),
                "property": "return_map", "actual": actual,
                "expected": SOURCE_HUB, "methods": methods}
        references.append(item)
        info("PROBE: %s return_map: %s (expected %s)" %
             (path, actual, SOURCE_HUB))
    data = EAL.load_asset(SOURCE_DATA)
    require(data is not None, "Cannot load source encounter definition")
    actual, methods = _read_soft_world_property(data, "encounter_map")
    references.append({"asset": SOURCE_DATA, "property": "encounter_map",
                       "actual": actual, "expected": SOURCE_PEDRO,
                       "methods": methods})
    info("PROBE: %s encounter_map: %s (expected %s)" %
         (SOURCE_DATA, actual, SOURCE_PEDRO))
    failures = [r for r in references if r["actual"] != r["expected"]]
    require(not failures,
            "Soft-reference probe found an absent/unreadable/mismatched "
            "reference; no assets written. Details: %s" % failures)
    return {"references": references, "writes": 0}


def resume():
    """Continue ONLY the known B2b-5b2 two-pristine-maps partial creation.

    Nothing is deleted or recreated. If a previous build proceeded farther,
    or either copied map was changed, fail and request manual recovery.
    """
    require_sources()
    require(exists(TARGET_PEDRO, ".umap") and exists(TARGET_HUB, ".umap"),
            "Resume requires BOTH previously copied destination maps")
    for path in (TARGET_DATA, TARGET_BP):
        require(not exists(path, ".uasset"),
                "Refusing resume: destination asset already exists: " + path)
    ensure_api()

    info("Resume preflight: inspect both original maps and both partial copies")
    source_pedro, source_pedro_actors = _resume_pedro_inventory(SOURCE_PEDRO)
    source_hub, source_hub_actors = _resume_hub_inventory(SOURCE_HUB)
    partial_pedro, partial_pedro_actors = _resume_pedro_inventory(TARGET_PEDRO)
    partial_hub, partial_hub_actors = _resume_hub_inventory(TARGET_HUB)

    require(source_pedro_actors == partial_pedro_actors,
            "Destination Pedro map actor inventory differs from source; "
            "refuse to overwrite authored edits")
    require(source_hub_actors == partial_hub_actors,
            "Destination hub map actor inventory differs from source; "
            "refuse to overwrite authored edits")
    require(source_pedro["director_facts"] == partial_pedro["director_facts"],
            "Destination Pedro Director settings differ from source")
    require(source_pedro["eye"]["position"] == partial_pedro["eye"]["position"]
            and source_pedro["eye"]["rotation"] == partial_pedro["eye"]["rotation"],
            "Destination original TargetPoint was moved or rotated")
    require(source_pedro["interactables"] == partial_pedro["interactables"],
            "Destination Pedro inspectable props differ from source")
    require(source_hub["entrance"] == partial_hub["entrance"],
            "Destination hub entrance differs from source")
    info("PASS: Both saved partial map copies match their pristine sources")

    # Begin exactly where the prior build failed. Do not duplicate maps again.
    create_bp()
    data = create_data()
    build_hub(data)
    bp_class = EAL.load_blueprint_class(TARGET_BP)
    require(bp_class is not None, "Cannot reload generated observer class")
    build_pedro(data, bp_class)
    return {
        "resume_preflight": "PASS: exactly two untouched cloned maps; no BP/Data",
        "source": {"source_pedro": source_pedro, "source_hub": source_hub},
        "verified": verify(),
    }


def build():
    require_sources()
    require_destination_absent()
    ensure_api()
    # Inventory both source maps BEFORE writing any asset. Never rely on
    # guessed actor counts or labels when modifying a binary Unreal world.
    source = inspect()
    # Use the Level Editor subsystem, not EditorAssetLibrary. Map duplication
    # is not supported by EditorAssetLibrary. Build maps first so the soft
    # EncounterMap and ReturnMap targets already exist in the asset registry.
    info("Creating isolated hub level from template")
    require(LEVELS.new_level_from_template(TARGET_HUB, SOURCE_HUB),
            "Could not create new hub level from template")
    info("Creating isolated Pedro level from template")
    require(LEVELS.new_level_from_template(TARGET_PEDRO, SOURCE_PEDRO),
            "Could not create new Pedro level from template")
    create_bp()
    data = create_data()
    # Do not hold references to actors from a different world while loading.
    build_hub(data)
    bp_class = EAL.load_blueprint_class(TARGET_BP)
    build_pedro(data, bp_class)
    return {"source": source, "verified": verify()}


def main():
    version = str(unreal.SystemLibrary.get_engine_version())
    require(version.startswith("5.8.2"),
            "This kit requires UE 5.8.2, got " + version)
    require(MODE in ("inspect", "build", "verify", "resume", "probe"), "Unknown mode " + MODE)
    require(LEVELS is not None and ACTORS is not None,
            "Required Editor subsystems unavailable")
    info("mode=%s run_id=%s engine=%s" % (MODE, RUN_ID, version))
    if MODE == "inspect":
        payload = inspect()
    elif MODE == "build":
        payload = build()
    elif MODE == "resume":
        payload = resume()
    elif MODE == "probe":
        payload = probe()
    else:
        payload = verify()
    return {"status": "PASS", "mode": MODE, "run_id": RUN_ID,
            "engine": version, "details": payload}


if __name__ == "__main__":
    RESULT_PATH.parent.mkdir(parents=True, exist_ok=True)
    try:
        result = main()
        info("PASS: " + MODE)
    except Exception as exc:
        message = "%s: %s" % (exc.__class__.__name__, exc)
        unreal.log_error("B2B5B2 FAIL: " + message)
        unreal.log_error(traceback.format_exc())
        result = {"status": "FAIL", "mode": MODE, "run_id": RUN_ID,
                  "error": message, "traceback": traceback.format_exc()}
    RESULT_PATH.write_text(json.dumps(result, indent=2, sort_keys=True),
                           encoding="utf-8")
    if result["status"] != "PASS":
        # UE command-line Python may still exit with zero, hence the wrapper
        # MUST inspect the dedicated JSON result; never trust process code.
        unreal.log_error("B2B5B2: validation FAILED; see JSON report")
