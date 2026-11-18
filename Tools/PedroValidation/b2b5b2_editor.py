"""Read-only saved ObserverActorTest map and Blueprint verification."""

from __future__ import annotations

import json
import os
import re
from pathlib import Path
import traceback

import unreal

ROOT = "/Game/AI_Gavin/Encounters/Pedro/Validation"
TARGET_PEDRO = ROOT + "/L_Pedro_ObserverActorTest"
TARGET_HUB = ROOT + "/L_Hub_Pedro_ObserverActorTest"
TARGET_DATA = ROOT + "/DA_Encounter_Pedro_ObserverActorTest"
TARGET_BP = ROOT + "/BP_PedroObserver_Validation"
TARGET_ID = "PedroObserverActorTest"
OBSERVER_LABEL = "PEDRO_OBSERVER_ACTOR_Test"
ORIGINAL_EYE_LABEL = "PEDRO_DIRECT_PLAY_ObserverEye"

MODE = os.environ.get("B2B5B2_MODE", "")
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


def main():
    version = str(unreal.SystemLibrary.get_engine_version())
    require(version.startswith("5.8.2"), "Expected UE 5.8.2, got " + version)
    require(MODE == "verify", "Run the saved-map verifier using its shell runner")
    require(LEVELS is not None and ACTORS is not None, "Required Editor subsystems unavailable")
    return {"status": "PASS", "mode": MODE, "run_id": RUN_ID,
            "engine": version, "details": verify()}


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
