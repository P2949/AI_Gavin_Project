# Pedro B2b-6a — autonomous observer awareness

Baseline: `afb94d071d359ad01ff10967a13f65b3bdaa0c2d` (merged PR #46).
Development branch: `feature/pedro-observer-awareness`.
Engine: local Unreal Engine 5.8.2, Linux, Nix development shell and steam-run.
Status: implementation candidate; Unreal acceptance must be executed locally.

## 1–3. Encounter design, architecture and scope

Read `docs/game_design.md`, `docs/encounters/pedro.md`, and the preceding
Pedro milestone documents. Pedro's role is **Express**: a peaceful observer
forms a subjective interpretation of genuinely known actions. World truth,
spatial awareness, event-time acquired behavioural evidence, classification,
and shared progress remain separate. Seeing a player is not seeing an action.
A missed action is not replayed as knowledge after reacquisition.

This stage implements spatial awareness only, with a dedicated native
`APedroObserverAIController`. No movement/focus, navigation, search,
watch-point choice, suspicion, hearing, combat, gossip, inference, new
classification rules, reward, player UI or production asset enablement.
Do not inherit the hostile `AEnemyAIController`. The Director and physical
Character remain unchanged; no dependency from the controller to the Director.
The controller does reuse the existing pure direct-observation geometry
helper (not the evidence/event machinery) for its supplementary sensory ray.

## 4–6. Controller, state and lifecycle

Opt-in: possession of a real `APedroObserverCharacter` by the new controller
starts awareness. Existing Character defaults, the older placed Blueprint,
all previous fixture maps, shared player class, plugin list, build modules,
and all `Config/` files are unchanged. The test driver attaches the controller
explicitly at runtime. It does not change any saved Blueprint default.

Provisional native sight configuration: 1500 cm initial radius, 1800 cm lose
radius, 75-degree half-FOV (150 total), 5 s max stimulus age, 0.1 s location
refresh, all affiliations enabled; the player selection policy alone restricts
the candidate to the first local player-controlled Pawn. No hearing or damage
sense. Auto-success near the last seen location is disabled with the native
invalid-range sentinel; viewpoint backward offset and near clipping are zero.
These are not permanent game-balancing decisions. The existing Director range
and cone remain independently authored (5000 cm and 75 degrees in the saved
ObserverActorTest).

`Unaware` has no player identity or position. `Perceived` retains a valid
last-known position and successful-sighting timestamp. `LostSight` preserves
that position and timestamp exactly; an unsuccessful stimulus cannot write a
location. Invalid/non-finite data is rejected. Memory clears on target
replacement/destruction, invalid observer, unpossession and EndPlay. No old
position survives a new possession. Lost memory is retained until lifecycle
reset, not discarded at the 5-second *stimulus* expiration. Possession enables
and registers the sight delegate and refresh timer; stop disables sight,
removes the delegate, clears the timer and forgets native data.

Native AI Perception is required for detection. Because stored stimuli can
lag current transforms and callbacks can be change-only, each accepted
location additionally requires a synchronous, physical eye-to-current-player
root cone/range and Visibility ray. Current position is read **only inside the
sensory query** after a successful, nonexpired stored sight stimulus and is
never published on a failed check. The native sample location and age are
logged independently. This deliberate conservative policy might reject a
partially visible pawn which Unreal's native sense accepts; it is a single
root-point check, not final multi-point perception. A retained native positive
plus a new physically clear check may revalidate sight before the next native
notification; no hidden live position is exposed. Native notifications and the
0.1-second refresh interval are sampled, not a guarantee of subframe truth.
The controller delegates both viewpoint methods to the actual ObserverEye and
suppresses automatic control rotation; future attention must explicitly own
facing. No SetFocus, MoveTo, or StateTree task is used. No engine config changes.

## 7–10. Files and native verification

The kit adds precisely eleven files (no baseline modifications):

- `Source/AI_Gavin_Project/Encounters/Pedro/PedroAwarenessState.h`
- `Source/AI_Gavin_Project/Encounters/Pedro/PedroObserverAIController.h`
- `Source/AI_Gavin_Project/Encounters/Pedro/PedroObserverAIController.cpp`
- `Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroAwarenessStateTests.cpp`
- `Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroObserverAIControllerTests.cpp`
- `Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroAwarenessWorldTests.cpp`
- `Tools/PedroValidation/b2b6a_run.sh`
- `Tools/PedroValidation/b2b6a_audit.py`
- `Tools/PedroValidation/verify_awareness_report.py`
- `Tools/PedroValidation/tests/test_awareness_report.py`
- `docs/encounters/pedro-observer-awareness.md`

No required `Build.cs` or `.uproject` changes (AIModule already present).
Offline verification includes syntax, 30 parser and scope-audit selftests (synthetic
reports, not Unreal results), exact branch/HEAD/source scope, and LFS. The
native report must contain the exact 67 previously passing tests plus all
14 new `AI_Gavin_Project.Pedro.Awareness.*` tests. The list is explicit in
the verifier; no unexpected names, warnings, errors, failures or skips.
The unit tests cover initial state, valid and updated observations, loss without
position mutation, reacquisition, invalid locations/times/target, reset,
controller configuration, filtering, unpossessed state and unchanged defaults.

## 11–12. World-backed runtime validation

The native driver is developer-only (`WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
&& !UE_BUILD_SHIPPING`). It uses a process ticker (proven by B2b-5b2) guarded
by command-line flags `-PedroAwarenessWorld` or `-PedroAwarenessIntegration`,
and the exact saved approved map in `EWorldType::Game`. A normal process
removes the ticker on its first call; it never drives the ordinary encounter.
The transient arena is constructed far from saved geometry in a separate
standalone process. It deliberately disables Character movement/gravity for
stable transforms and uses actual time-spanning engine updates, a real
player-controller possession, native perception and movable query collision.
No fabricated perception callback, fake Director evidence or saved asset.
The original player is used initially; the target-destruction/replacement
case then possesses a transient Character with the real player controller.
The fixture teleports/rotates actors as test stimuli, **not** navigation AI.
Recovery v2 freezes movement on fixture Characters after possession and
repossession (including player replacement), with component ticking disabled.
The 0.1 cm/degree pose guard is unchanged; fixture-motion diagnostics record
movement mode, velocity, tick state, and transforms on failure. No production
controller or Character default changes are made.

| Case | Requirement |
| --- | --- |
| W01 | Initial positive native sight and location |
| W02 | Fresh observer facing away, no native sight or knowledge |
| W03 | Fresh observer beyond initial range, no knowledge |
| W04 | Fresh observer behind physical cover, no knowledge |
| W05 | Visible player moved, independently checked new position |
| W06 | Physical observer turns away; loss, frozen location/time |
| W07 | Turn back and reacquire |
| W08 | Enter range after prior loss |
| W09 | Retain at 1650 cm, lose at 2050 cm, no new sight at 1650 cm, reacquire at 600 cm |
| W10 | Blocking Visibility wall plus same-step hidden movement causes native and spatial loss without updating memory |
| W11 | Actual occluded movement; position/time unchanged throughout observation window |
| W12 | Wall removed, native sight and current position restored |
| W13 | Change only eye relative rotation then offset; actual native sight changes and viewpoint matches |
| W14 | A native-visible non-player distractor does not become known player |
| W15 | Actual target actor destroyed; awareness cleared |
| W16 | Replacement pawn starts hidden, then needs its own real sighting |
| W17 | Unpossess clears state, delegate, refresh timer |
| W18 | Repossess hidden, then reacquire with fresh memory |
| W19 | Two real observers have independent native and spatial state |
| W20 | Controller destruction invalidates actor and removes copied timer handle |
| W21 | Player sight alone produces no evidence or resolution |
| W22 | Visible player, occluded actual inspection event rejected |
| W23 | Unseen player, visible source accepted by original event-time contract |
| W24 | Fresh evidence rig: detect, inspect visible A, lose sight, hidden B inspection, reacquire without replay |
| W25 | New visible B inspection is a new accepted occurrence |

There are 25 named cases and 14 explicit checked substeps. Positive waits
require a stable predicate across multiple engine updates. Negative initial
cases have sustained no-acquisition windows; memory-freeze cases check
invariants continuously. Each step has an 8-second wall-clock bound and the
runner an outer process timeout. The test driver records native sight,
raw stored sample, observer eye, actual fixture target position, remembered
position/time, lifecycle state, event IDs, accepted IDs, core samples and
progress. It independently observes actual `ReportConfirmedAction` broadcasts
through real `IInteractable::Execute_Interact` and verifies their event IDs,
subject, kind, provenance and multiplicity. The Director is not fed evidence
from the fixture. The demo in W24/W25 verifies accepted counts 0 → 1 → 1 → 1
→ 1 → 2; the hidden event is absent from acquired IDs throughout. The earlier
W22/W23 rig is reset only by destroying/recreating transient actors, not by
resetting shared progression. W23 is intentional compatibility, not a claim
that event-source sight without player sight should remain final design.

## 13. Saved-map integration

Separate process loads `L_Pedro_ObserverActorTest` itself (not a copy).
I01 checks original Director/definition/observer/outcomes and four core plus
one bonus opportunities. I02 attaches the awareness controller at runtime,
proves current native sight of the actual player-controlled pawn. I03 inspects
original Statue. I04 blocks original Radio's event ray with a physical query
box and inspects it for a genuine but rejected occurrence. I05 removes the
box, verifies current native sight and no replay. Two new valid inspections of
Radio and Lantern (I06) must yield three distinct accepted core opportunities,
Seeking, exactly one progression resolution and only its outcome actor
active. The test checks the actual definition ID, progress subsystem, final
classification, visibility/collision flags and accepted event IDs. Controlled
runtime aiming and pawn placement are allowed, but saved asset bytes and
configuration remain unchanged. This is actual interactable gameplay code,
**not** keyboard input, player trace accessibility, pixel/raster visibility,
hub travel or a new gameplay presentation test. Previous manual B2b-5b2 hub
and input evidence remains historical baseline. NullRHI and normal rendered
runs execute both suites; the rendered run is an engine integration repeat,
not a pixel-level visual assertion.

## 14–16. Mechanical runner and reports

With editor closed, on the exact prepared branch:

```bash
cd "$HOME/Development/Unreal/Projects/AI_Gavin_Project"
bash Tools/PedroValidation/b2b6a_run.sh
```

`all` is the default. It performs read-only engine version/API/source probe,
strict offline validation, working-tree snapshot, ordinary Editor Development
build, native 81-test report, world 25-case and saved-map six-case NullRHI
runs, both suites rendered, actual forced Unity translation-unit compilation
(`-ForceUnity -DisableAdaptiveUnity -gather` with new-source mtimes touched, content unchanged),
retested 81-test native suite, final byte hashes/status comparison and LFS
integrity. No temporary native source, no temporary plugin enablement and no
asset creation, saving or deletion. Ordinary UE build outputs, ignored Saved
logs and source-file mtimes may change. The engine probe checks actual installed
headers and records relevant local `.cpp` excerpts when distributed; it is a
capability check, not a semantic parser or compile substitute. Runtime tests
prove the installed engine behavior.

Iteration commands (partial acceptance only):

```bash
bash Tools/PedroValidation/b2b6a_run.sh probe
bash Tools/PedroValidation/b2b6a_run.sh unit
bash Tools/PedroValidation/b2b6a_run.sh world
bash Tools/PedroValidation/b2b6a_run.sh integration
bash Tools/PedroValidation/b2b6a_run.sh rendered
bash Tools/PedroValidation/b2b6a_run.sh unity
bash Tools/PedroValidation/b2b6a_run.sh all
```

Reports: `Saved/PedroValidation/b2b6a-<mode>-<unique>/`, with the path in
`b2b6a-latest.txt` and mode-specific latest pointer. Each native/runtime
subdirectory retains the full log, terminal log, exit status, verifier output
and structured `results.json`; raw automation `index.json` is retained as
well. Native tests use the documented `Automation RunTest ...;Quit` command
rather than legacy `-TestExit`; both a zero process exit and a complete clean
report remain mandatory. Failed/warning test entries and process status are
printed separately. Unity verification retains `unity/results.json` with generated blob hashes and
source membership from the actual UBT compile-action list. Root contains source inspection, compile logs, offline tests, before
and after status/hashes, final audit and `summary.json`. Only a full `all`
run with `status=PASS` and `full_acceptance=true` satisfies the mechanical
acceptance target. A failed process, missing marker, wrong case inventory,
malformed evidence, fatal/ensure/assert, incomplete JSON, duplicate resolution
or changed repository content aborts. Preserve all logs on failure. Do not
modify existing files to make the validator pass. Source fixes in these new
files may be applied on the same pre-commit branch and rerun using a new
unique report. The script does not automatically commit, push or merge.

## 17–18. Implementation/review sequence and acceptance

1. Installer verifies clean exact branch and engine API before writing files.
2. Compile state and dedicated controller; run the 81-test native inventory.
3. Run actual world fixture with native perception; inspect evidence reports.
4. Run real saved-map tests and both rendered repetitions.
5. Force real Unity compilation with adaptive Unity disabled, verify each of the
   four new translation units is included exactly once in a compiled project
   Unity blob, rerun native automation and check repository integrity.
6. Review source and logs, then fill the real results below; rerun the final
   audit and stage only the eleven paths. Documentation result edit changes
   its hash intentionally **after** the test run, so do not claim the earlier
   snapshot includes edited results. Review staged diff before commit.
7. Push a focused PR. Review and merge independently; do not assert that a
   local test suite has already merged the PR.

Actual local validation (2026-11-18, Unreal Engine 5.8.2):

- Offline automation: 42/42 PASS.
- Total test executions: 266/266 PASS.
- All 13 required validation stages completed.
- Full acceptance: true.
- Full validation report: `b2b6a-all-EiVe3epg`.
- Targeted Unity report: `b2b6a-unity-TSHR1V08`.


- Ordinary and forced Unity Editor build: PASS; all four new translation units verified in Unity.
- Native 67 baseline + 14 new, clean reports before and after Unity: PASS; 81/81 in each execution, zero warnings, failures or skips.
- World 25/25 NullRHI and rendered: PASS in both execution modes.
- Saved-map 6/6 NullRHI and rendered: PASS in both execution modes.
- Event identity, memory freeze and progression: PASS; no hidden-position or retroactive evidence acquisition.
- File-scope/time-of-test byte-hash and LFS audit: PASS; repository unchanged throughout validation.
- Pull request: NOT YET CREATED.

## 19–20. Handoff and remaining limitations

B2b-6b: deliberate attention/turning, consuming only valid spatial knowledge.
B2b-6c: navigation/follow, without omniscient MoveToActor on hidden target.
B2b-6d: observation positions and contextual policies; B2b-6e: isolated saved
playable autonomous integration. Later: adaptive observation, authored NPC
choices/puzzles, salient memories, unusual roles and outcome presentation.
No new gameplay of those future milestones is included here. The controller
is available but not enabled on any production asset. The sample/ray policy
and timing/geometry defaults are provisional and reviewable. Native reports
and source code must be checked on the user's actual UE build; offline success
alone is not validation of Unreal compilation, game behavior, or visual quality.
