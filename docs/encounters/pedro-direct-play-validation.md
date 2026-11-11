# Pedro B2b-4b: playable direct observation (isolated validation map)

Baseline: merged `main` commit `f8ea643` / PR #42.

## Scope

This adds three **separate** validation assets under `Content/AI_Gavin/Encounters/Pedro/Validation`:

- `L_Pedro_DirectPlayTest.umap`, cloned from `L_Pedro_ActiveTest`;
- `L_Hub_Pedro_DirectPlayTest.umap`, cloned from `L_Hub_Pedro_ActiveTest`;
- `DA_Encounter_Pedro_DirectPlayTest.uasset`, with ID `PedroDirectPlayTest`.

There are no production-map, earlier validation-map or native C++ changes.
The test map has a placed `TargetPoint` called `PEDRO_DIRECT_PLAY_ObserverEye`
and a toggleable Visibility-only test wall. The copied Director uses
`EvidenceDriven` + **DirectObservation only**, not `PrototypeAssumed`.
The original `PedroPrototypeInteractableActor` actors are used for real player
inspection actions. No simulated Help/Attack controls are added. Three distinct
core inspections are still necessary for the 75% gate.

## Required tests

- Visible: three directly observed original Statue, Radio, Lantern inspections;
  one Seeking resolution at sampledCore=3, requiredCore=3; copied hub indicator ON.
- Behind: same original Statue inspection outside narrow cone; zero acquired;
  no outcome, indicator OFF.
- TooFar: original Statue inspection beyond deliberate 1 cm range; zero acquired;
  no outcome, indicator OFF.
- Occluded: one original Statue inspection with a blocking Visibility wall
  between event location and observer eye; zero acquired; no outcome,
  indicator OFF. First prove the same Statue is visible with the wall OFF.
- Conflict: both acquisition policies ON (intentional invalid configuration);
  no acquired knowledge or resolution; restore direct-only afterward.

All scenarios start from `L_Hub_Pedro_DirectPlayTest`, travel to the copied Pedro
map, and return to the copied hub in independent one-player PIE sessions.
The log verifier checks the final session of each separately archived log.
Manual verification must establish accessibility, visual outcome, and copied
hub indicator. Restore the test map to visible/direct-only before committing.

## Limitations

This proves direct line trace acquisition for actual *prop inspections*,
not for genuine NPC help, combat, puzzles or arbitrary actions. `TargetPoint`
is a stationary configured witness eye, **not a physically moving Pedro NPC**.
Observation is at the event source's actor origin, not the player hand/eyes.
No AI Perception, hearing, attention or delayed knowledge is included.
The full creation and test instructions are in the separately distributed
`Pedro_B2b4b_PlayableDirect_REVIEW_v1` local kit under ignored `Saved/`.
