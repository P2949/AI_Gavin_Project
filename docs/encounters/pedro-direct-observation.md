# Pedro B2b-4a — opt-in direct line-of-sight acquisition

Starting baseline: PR #41 merged into `main` at `e54efac`.
Development branch: `feature/pedro-direct-observation-validation`.

## Scope

The existing Director receives confirmed `FPedroBehaviourEvent` world facts
from registered `UPedroObservationSourceComponent` components. Previously,
those events could only become observer knowledge when the map enabled
`bUsePrototypeAssumedAcquisition` (not actual awareness). B2b-4a adds an
**independent, opt-in** `bUseDirectObservationAcquisition` acquisition path.

In the new path, the Director checks the recorded event position at the
moment the event is emitted, from a specified observer actor eye point and
forward direction (`GetActorEyesViewPoint`). It requires:

- valid World, observer, identities, event and settings;
- observer actor in the Director's current World;
- distance <= `DirectObservationMaxDistance` (Unreal centimeters);
- direction inside `DirectObservationViewHalfAngleDegrees` on **both** sides
  of forward (full field of view = twice that angle);
- an `ECC_Visibility` line trace with no intervening blocking actor. A ray
  first hitting the event's own subject actor is accepted; the subject is
  *not ignored* in the trace, so a wall between the eye and subject still wins.

Only after these checks does the Director record the event with provenance
`EPedroKnowledgeSource::DirectObservation`. It keeps the original event ID,
kind, subject, opportunity and event-time position unchanged. Rejected events
remain world truth but are not learned and do not advance acquired samples.

Two simultaneous acquisition modes are a configuration error and *neither*
is allowed to acquire. Both disabled also acquires nothing. In legacy maps,
`bUseDirectObservationAcquisition` defaults **false** and the previous
assumed-acquisition behavior is unchanged. `CompletionPolicy` still defaults
to `LegacyInteractionCount` on unchanged maps; active completion is not
unconditionally enabled.

## Runtime settings (placed Pedro Encounter Director)

- `Use Prototype Assumed Acquisition`: **off** for the new direct policy.
- `Use Direct Observation Acquisition`: **on** for a deliberate test instance.
- `Direct Observation Actor`: assign a **placed** actor or TargetPoint.
  Its eye location and rotation determine the geometric sightline.
- `Direct Observation Max Distance`: default `1500` cm (15 m).
- `Direct Observation View Half Angle Degrees`: default `75` (150-degree
  complete view cone). `180` allows any direction, but still requires distance
  and collision tests.
- Confirm that relevant occluders **Block Visibility**. Geometry configured to
  Ignore Visibility cannot physically occlude this policy's trace.

This release does NOT modify any `.umap`, `.uasset`, player controls, outcome
actor, existing observation source or saved gameplay state.

## Validation

1. Build the `AI_Gavin_ProjectEditor` target in the same Nix/steam-run shell.
2. Run the entire Pedro native Automation suite: 57 legacy + 8 newly added
   direct-observation geometry cases = **65**. The eight B2b-2 PIE tests
   are a different suite, not included in these 65.
3. Create a **new unsaved Empty Level** in Unreal Editor, normally named
   `Untitled_1`. Do not open or save over production/ActiveTest/ChoiceTest maps.
4. Place exactly ONE `PedroDirectObservationValidationHarness` actor and enable
   `Run Validation In PIE` in its instance Details.
5. Start PIE, allow the fixture to run, confirm the Output Log summary:
   `PEDRO_DIRECT_WORLD_TEST status=SUMMARY passed=8 failed=0 total=8`.
6. Stop PIE. **Do not save this map.** Verify the standalone Editor log with
   the kit's `verify_direct_pie_log.py`.
7. Run the unchanged B2b-3a and B2b-3b regression tests only if the broader
   development review requires them; the new policy must not alter old logs.

The fixture requires a PIE World with `Untitled` in the map name. It spawns
and immediately destroys all actors, creates **no** assets, and cannot run
inside a saved production or copied-validation map.

### Native tests (8)

Geometry.Visible, Geometry.Range, Geometry.Angle, Geometry.Boundary,
Geometry.FullCircle, Geometry.InvalidSettings, Geometry.NonFinite and
Geometry.ZeroForward.

### World-backed PIE tests (8)

- Visible: one acquired `DirectObservation` record;
- Occluded: a solid mesh wall blocks the Visibility trace;
- TooFar: range fails;
- Behind: view cone fails;
- MissingObserver: no acquired record;
- ConflictingPolicies: no acquired record despite successful world event;
- PrototypeCompatibility: exactly one `PrototypeAssumed` record with new
  mode off;
- InvalidSettings: zero maximum sight distance fails closed.

A successful fixture confirms direct acquisition in real PIE World collision
queries. It does NOT establish that the actual gameplay Pedro actor is placed,
that the player genuinely helped/attacked an NPC, or that the observer has
attention/awareness state beyond these fixed eye, cone, and ray checks.

## Technical caveats / next phase

- `FPedroBehaviourEvent::EventLocation` is currently sourced from the event
  source actor position. It does **not** automatically include the player's
  hand, sword, or a dedicated event-space witness point. B2b-4b must choose
  meaningful event locations for real gameplay actions.
- The direct query samples one ray toward one event position. Complex events,
  tall/large subjects, doors, moving occluders and partial cover need additional
  controlled tests. `Visibility` collision settings are part of the contract.
- `GetActorEyesViewPoint` may differ from actor origin/rotation for Pawn-derived
  observers. A temporary TargetPoint represents a fixed witness eye, not a
  moving/attending Pedro character.
- This mode does not implement hearing, AI Perception, attention scheduling,
  remembered delayed discoveries, or gossip. It does not retroactively learn
  about actions that occurred while occluded.
- `APedroDirectObservationValidationHarness`, the existing B2b-2 harness,
  and the B2b-3b simulated choice controls are development fixtures that
  currently compile into the runtime module. Remove or compile-gate the
  development-only classes before shipping.
- The existing copied validation maps and production levels must not be
  modified in this PR. B2b-4b will use a separate reversible map for
  player-driven direct-sight checks.

## Definition of done

- Exactly eight intended C++/doc files changed; no Content/ diffs.
- UE Editor build succeeds.
- 65/65 Pedro native tests pass (zero warnings, zero failures, zero skips).
- 8/8 direct World-backed PIE cases pass.
- Confirm default prototype policy is preserved and no development map saved.
- Review staged patch and create a PR; do not merge solely on a static
  script selftest or a successful compilation without PIE coverage.
