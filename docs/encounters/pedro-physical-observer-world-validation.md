# Pedro B2b-5b1 — physical observer, world-backed movement and sight checks

Base: UE 5.8.2, merged PR #44 (`92e9833`). B2b-5a's
`APedroObserverCharacter` supplies a real character eye transform.

## Scope

Add one opt-in `APedroPhysicalObserverWorldValidationHarness`, plus a
read-only log verifier. This changes no existing native implementation,
completion rule, evidence provenance, Blueprint, `.umap`, `.uasset` or
player controls. The fixture lives in an *unsaved new Empty Level* and
requires explicit `Run Validation In PIE`. It refuses saved/production maps.

The fixture creates actual `APedroObserverCharacter` instances in a real PIE
world. It changes the observer's actor transform and emits confirmed facts
through `UPedroObservationSourceComponent`, with the existing Director
configured for `DirectObservation` only. Collision checks use the real
Visibility query. Every case is isolated in its own region and destroys
its temporary actors. It does not fabricate evidence in the Director.

## Acceptance cases (8)

- Visible: a confirmed prop event in front of the character is acquired once.
- TurnAway: rotate character 180 degrees before the event; reject with
  `OutsideViewCone`, zero acquired.
- TurnBack: verify initially outside cone, rotate toward the event, then
  confirm one acquired record.
- MoveOutOfRange: teleport character beyond 1000 cm, then report event;
  reject with `TooFar`.
- MoveIntoRange: first confirm TooFar, move close before the event, acquire.
- CoverBlocks: a Visibility-only wall between eye and source blocks sight.
- NoRetroactive: event while facing away is not acquired; turning toward
  the old event does not change evidence count; a **new** occurrence can
  then be acquired.
- EyeFollowsPose: actual `GetActorEyesViewPoint()` changes after character
  translation/rotation, and a new event is acquired at that pose.

## Required local verification

1. From clean `main` at `92e9833893ad79e4d2636ef6dc1d5536f6478288`,
   create `feature/pedro-physical-observer-world`.
2. Install only the two harness files, this document, and the verifier.
3. Build the UE `AI_Gavin_ProjectEditor` target successfully.
4. Rerun **67/67** existing Pedro native automation tests, no warnings or skips.
5. Open UE 5.8.2. File -> New Level -> **Empty Level**; do not save.
6. Place exactly ONE `PedroPhysicalObserverWorldValidationHarness` actor.
   On that instance check **Run Validation In PIE**.
7. Start PIE. Observe log summary:
   `PEDRO_PHYSICAL_WORLD_TEST status=SUMMARY passed=8 failed=0 total=8`.
8. Stop PIE, close Editor without saving the Untitled level, archive its
   full log and run the committed `Tools/PedroValidation/verify_physical_observer_world_log.py`.
9. Verify five exact intended repository files. Do not commit if either
   native or world-backed validation fails.

## Limitations

The fixture changes the character pose synchronously during PIE, at event
emission, so it validates actual actor-transform-dependent acquisition and
negative cases. It is not yet a player's visible NPC interaction, actual
navigation/follow AI, an attention policy, memory, or a saved playable map.
The next milestone uses separate, copied ObserverActorTest Pedro/hub maps
and original player inspection actions. All previous maps remain protected.

Runtime fixture classes are development-only; remove/compile-gate prior to
shipping production builds.

## Unity Build compatibility

This branch also updates the existing
`Tests/PedroDirectObservationTests.cpp` file.

The anonymous-namespace `Eye` test constant was renamed to
`TestEyeLocation`, including its two references.

Unreal's adaptive Unity Build combined that source with
`PedroObserverCharacterTests.cpp`. Clang reported that the local
`Eye` component pointers shadowed the constant and promoted
the warnings to errors.

This is a test-only naming fix. Test geometry, assertions,
production code and expected results remain unchanged.
