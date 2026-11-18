# Pedro implementation

Design: [pedro.md](pedro.md). Production remains the legacy greybox; awareness is opt-in. UE 5.8.2.

## Runtime

- `UPedroObservationSourceComponent` emits confirmed world events. `APedroEncounterDirector` acquires them separately from exposure and world truth.
- Acquisition: `PrototypeAssumed` (legacy test policy) or `DirectObservation` (real sight); both enabled or both disabled acquire nothing. Direct sight checks event-time eye position, cone, distance and one `ECC_Visibility` ray to the source actor origin. A first hit on the subject is allowed; intervening cover is not. Missed events are never replayed.
- `FPedroObservationState`, `PedroEvidenceRules` and `PedroClassification` retain accepted event IDs/provenance, cap repetition and classify Gentle, Seeking or Disruptive. `PedroCompletionGate` requires exposure plus accepted evidence for distinct core opportunities (currently 3/4; bonus does not count). Shadow readiness is diagnostic, not completion.
- `EvidenceDriven` completion validates three distinct outcome actors, presents one and resolves shared progress once; rejected progression rolls back presentation. `LegacyInteractionCount` remains the production default.
- `APedroObserverCharacter::GetActorEyesViewPoint` uses the physical `ObserverEye`. `APedroObserverAIController` uses native sight plus a fresh cone/range/Visibility check to the player root; spatial awareness never grants behavioural evidence. The two sight policies are independent. First local player-controlled pawn only.
- Awareness: `Unaware` -> `Perceived` -> `LostSight`; loss freezes last-known location and timestamp, reacquisition updates them. Target replacement/destruction and possession lifecycle clear state. No live hidden position is exposed. Native sight: 1500/1800 cm acquire/retain, 75-degree half-angle, 5 s stimulus age, 0.1 s refresh. Memory survives stimulus expiry until lifecycle reset. No auto-success, hearing or movement.

## Validation assets

Under `Content/AI_Gavin/Encounters/Pedro/Validation/`, each set has a Pedro map, copied hub and encounter definition. Keep them separate from production.

| Set | Purpose |
| --- | --- |
| `ActiveTest` | Seeking, assumed acquisition, evidence-driven progression |
| `ChoiceTest` | Simulated Help/Attack, Gentle/Disruptive and insufficient unique samples |
| `DirectPlayTest` | Fixed TargetPoint; event-time sight and travel |
| `ObserverActorTest` | Physical observer Blueprint; real inspections and runtime awareness opt-in |

`ObserverActorTest` uses direct-only acquisition, 5000 cm/75-degree half-angle, 4 core and 1 bonus opportunity, 0.75 threshold. The saved observer still uses its original controller; the runtime fixture opts in to awareness. Choice controls are not genuine NPC help/attack. Do not save test-time overrides to these maps.

## Regression

Close Unreal Editor. From a clean checkout or fully staged candidate (no unstaged/untracked files):

```bash
bash Tools/PedroValidation/b2b6a_run.sh all
bash Tools/PedroValidation/b2b5b2_run.sh verify
```

The first runner supports `probe`, `unit`, `world`, `integration`, `rendered`, `unity`, `all` (default). It uses the existing Nix development shell and steam-run; `PROJECT` and `ENGINE` can override their paths. `all` requires the exact current native inventory, real multi-frame sight/evidence tests, saved-map regression in NullRHI and rendered modes, forced Unity compilation and unchanged repository snapshots. Update the native inventory deliberately when adding tests. Reports: `Saved/PedroValidation/b2b6a-*/summary.json`; require `status=PASS` and `full_acceptance=true` for full acceptance. The second command independently reopens the existing saved map/Blueprint/hub references, checks the original observer eye, and restores the temporary Editor plugin configuration. It does not author assets.

Optional PIE fixtures: in a new unsaved Empty Level, place one of `PedroActiveWorldValidationHarness`, `PedroDirectObservationValidationHarness` or `PedroPhysicalObserverWorldValidationHarness`, enable Run Validation In PIE and run. Each has eight cases; inspect `PEDRO_WORLD_TEST`, `PEDRO_DIRECT_WORLD_TEST` or `PEDRO_PHYSICAL_WORLD_TEST` respectively. Physical fixture log verifier: `python3 Tools/PedroValidation/verify_physical_observer_world_log.py <editor.log>`. Do not save fixture levels. For player input/travel checks, open the relevant copied hub and use actual interactions; exit through its return volume.

## Last accepted baseline

B2b-6a (2026-11-18, UE 5.8.2, merged PR #47): 42 offline tests, 81 native before and after verified Unity, 25 world and 6 saved-map cases in each rendering mode (266 total executions). All passed. Report `b2b6a-all-EiVe3epg`. Earlier B2b-5b2 physical map verification, 6/6 occlusion cases per mode, original DirectPlayTest travel and 67 native tests passed. Milestone docs and one-time authoring/recovery procedures remain in Git history before cleanup.

## Remaining scope

Next: autonomous attention/turning, then navigation, contextual observation policies and playable autonomous integration. Use only legitimate spatial knowledge; no omniscient chasing. Later: genuine NPC activities, puzzles and final role presentation. Current single-ray sight/root sampling is conservative and not pixel visibility. Runtime fixtures bypass keyboard aiming; no visual signoff or Shipping build coverage is implied. Legacy development harnesses/choice actors compile into the runtime module; review editor/shipping boundaries and validation asset cooking before packaging.
