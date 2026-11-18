# Pedro implementation

Design: [pedro.md](pedro.md). Production remains the legacy greybox; awareness, attention, navigation and contextual observation are opt-in. UE 5.8.2.

## Runtime

- `UPedroObservationSourceComponent` emits confirmed world events. `APedroEncounterDirector` acquires them separately from exposure and world truth.
- Acquisition: `PrototypeAssumed` (legacy test policy) or `DirectObservation` (real sight); both enabled or both disabled acquire nothing. Direct sight checks event-time eye position, cone, distance and one `ECC_Visibility` ray to the source actor origin. A first hit on the subject is allowed; intervening cover is not. Missed events are never replayed.
- `FPedroObservationState`, `PedroEvidenceRules` and `PedroClassification` retain accepted event IDs/provenance, cap repetition and classify Gentle, Seeking or Disruptive. `PedroCompletionGate` requires exposure plus accepted evidence for distinct core opportunities (currently 3/4; bonus does not count). Shadow readiness is diagnostic, not completion.
- `EvidenceDriven` completion validates three distinct outcome actors, presents one and resolves shared progress once; rejected progression rolls back presentation. `LegacyInteractionCount` remains the production default.
- `APedroObserverCharacter::GetActorEyesViewPoint` uses the physical `ObserverEye`. `APedroObserverAIController` uses native sight plus a fresh cone/range/Visibility check to the player root; spatial awareness never grants behavioural evidence. The two sight policies are independent. First local player-controlled pawn only.
- Awareness: `Unaware` -> `Perceived` -> `LostSight`; loss freezes last-known location and timestamp, reacquisition updates them. Target replacement/destruction and possession lifecycle clear state. No live hidden position is exposed. Native sight: 1500/1800 cm acquire/retain, 75-degree half-angle, 5 s stimulus age, 0.1 s refresh. Memory survives stimulus expiry until lifecycle reset. No auto-success or hearing.
- Attention (default disabled; `SetAttentionEnabled(true)`): `PedroAttentionPolicy` selects Idle, TrackObserved or HoldLastKnown using only awareness memory and current player identity. Freshness 0.35 s; hold expires 2 s after last successful sighting, without erasing memory. Idle returns to the body heading captured on enable/possession. Controller applies physical yaw (90°/s tracking/hold, 45°/s idle, 2° dead zone, 0.1 s timestep cap), including lateral eye offsets; unreachable/near-vertical/coincident targets do not turn. No player-transform query, focus, movement or evidence in attention. Disable/unpossess clears the anchor; repeated enable does not recapture it.

- Navigation (default disabled; `SetNavigationEnabled(true)`): native Recast path following to fixed stand-off locations derived solely from approved spatial knowledge. Start/stop 650/500 cm, preferred 450 cm, personal space 250 cm; 180 cm/s walk, 500/600 cm/s² acceleration/braking. Five deterministic candidates; bounded projection, complete paths, no links, max 2200 cm. Replan after 0.35 s and 125 cm displacement; failure cooldown 1 s; stall window 1.25 s/20 cm; 15 s request limit. Only an existing fixed request may continue during loss, expiring 2 s after last sight. Cancel owned requests on disable, target change and lifecycle; restore movement settings. Attention is sole yaw owner. No actor movement target or evidence changes. Navigation is not enabled in saved assets.

- Context (B2b-6d, disabled by default): explicit same-world Director binding and attention/navigation prerequisites; accepted direct evidence only, fixed event locations, bounded activity holds (1.2/2/2.5 s, cap 3 s), authored watch points with bounded path queries and deterministic score, decaying visit/failure/sight-loss memory, sustained close-proximity withdrawal on escape-safe paths, temporary disengagement and post-commit satisfaction. No hidden player queries, evidence replay, new scoring rules or attack execution. Watch-point locations denote feet positions; use transient points in validation only. Genuine attack sources and playable activation are deferred.

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

The first runner supports `probe`, `unit`, `world`, `integration`, `attention`, `attention-integration`, `navigation-probe`, `navigation`, `navigation-integration`, `context-probe`, `context`, `context-integration`, `rendered`, `unity`, `all` (default). It uses the existing Nix development shell and steam-run; `PROJECT` and `ENGINE` can override their paths. `all` requires the exact current native inventory, real multi-frame sight/evidence/attention/navigation/context tests (25+6, 24+6, 28+6, 30+6), all eight runtime suites in NullRHI and rendered modes, verified individual and forced Unity compilation and unchanged repository snapshots. Explicit builds preserve the previous generated build cache under `Intermediate/.pedro-build-cache-*/`. Run `context-probe` first: it exercises the existing transient Dynamic Recast capability plus four contextual interactions (follow, activity interruption, watch-point travel and withdrawal); process-only engine ini override is restricted to navigation/context runs. No saved geometry changes. Accepted native inventory: 165. Full regression: 592 Unreal executions and 176 offline checks (768 total, 25 stages, 20 report groups, 12 required translation units); probe separate. Update the inventory deliberately when adding tests. Reports: `Saved/PedroValidation/b2b6a-*/summary.json`; require `status=PASS` and `full_acceptance=true` for full acceptance. The second command independently reopens the existing saved map/Blueprint/hub references, checks the original observer eye, and restores the temporary Editor plugin configuration. It does not author assets.

Optional PIE fixtures: in a new unsaved Empty Level, place one of `PedroActiveWorldValidationHarness`, `PedroDirectObservationValidationHarness` or `PedroPhysicalObserverWorldValidationHarness`, enable Run Validation In PIE and run. Each has eight cases; inspect `PEDRO_WORLD_TEST`, `PEDRO_DIRECT_WORLD_TEST` or `PEDRO_PHYSICAL_WORLD_TEST` respectively. Physical fixture log verifier: `python3 Tools/PedroValidation/verify_physical_observer_world_log.py <editor.log>`. Do not save fixture levels. For player input/travel checks, open the relevant copied hub and use actual interactions; exit through its return volume.

## Last accepted baseline

B2b-6d (2026-11-18, UE 5.8.2): 768/768 test executions passed (176 offline, 330 native, 262 runtime), all 25 stages and 20 report groups. Individual and forced Unity compilation verified 12 required translation units. Both headless and rendered contextual suites verified activity interruption, watch-point ranking and physical relocation, disconnected-path rejection, adaptation, bounded memory and safe non-hostile withdrawal. Both saved-map contextual suites acquired three of four emitted event identities (Statue, new Radio, Lantern; occluded Radio rejected), resolved Seeking exactly once and entered Satisfied. Independent saved-map and repository integrity verification passed (568 files hashed). Reports: `b2b6a-all-MFuHZH2b`, `b2b5b2-verify-rE3yk5OZ`.

## Remaining scope

Next (B2b-6e): playable autonomous integration. Use only legitimate spatial knowledge; no omniscient chasing. Later: genuine NPC activities, puzzles and final role presentation. Current single-ray sight/root sampling is conservative and not pixel visibility. Runtime fixtures bypass keyboard aiming; no visual signoff or Shipping build coverage is implied. Legacy development harnesses/choice actors compile into the runtime module; review editor/shipping boundaries and validation asset cooking before packaging.
