# Pedro B2b-3b — isolated playable outcome coverage

Baseline: merged `main` at `65adf4e` (PR #40). Working branch:
`feature/pedro-active-outcome-coverage`.

## Scope

Add an independent copied hub, Pedro level, and encounter definition to
validate **Gentle**, **Disruptive**, and **insufficient unique core samples**
using the existing real player interaction line trace. The merged B2b-3a
**Seeking** validation level, production Pedro/hub maps, active Director,
classification, completion gate, progression subsystem and player input
remain unchanged.

The new native `APedroValidationChoiceActor` implements `IInteractable` and
has a fixed mesh and a visible label. Each instance has exactly one Pedro
observation component, uses an authored **core** `OpportunityId`, and
**simulates** a successful `HelpedNPC` or `AttackedNPC` world event. It
reports actual interaction with the simulator, not actual NPC help/attack.

The actor is additionally guarded: only a player-controlled Pawn may use it,
it runs only in a PIE world whose name ends in `L_Pedro_ChoiceTest`, it only
accepts the two intended event kinds and it disables itself after a single
successful source emission. This test actor is part of the runtime C++
module for compilation purposes but is **not placed in production maps**;
remove or editor-gate it before a shipping build.

## Expected changes (exactly six committed files)

- `Source/AI_Gavin_Project/Encounters/Pedro/PedroValidationChoiceActor.h`
- `Source/AI_Gavin_Project/Encounters/Pedro/PedroValidationChoiceActor.cpp`
- `Content/AI_Gavin/Encounters/Pedro/Validation/L_Pedro_ChoiceTest.umap`
- `Content/AI_Gavin/Encounters/Pedro/Validation/L_Hub_Pedro_ChoiceTest.umap`
- `Content/AI_Gavin/Encounters/Pedro/Validation/DA_Encounter_Pedro_ChoiceTest.uasset`
- `docs/encounters/pedro-choice-validation.md` (this document)

Creation, audit and log-verification scripts are distributed in a separate
local ZIP under ignored `Saved/PedroValidation` and are **not committed**.

## Gameplay scenarios

1. Gentle: from copied ChoiceTest hub, enter ChoiceTest Pedro map; use
   three labelled `SIM HELP` cubes for `Inspect_Statue`, `Inspect_Radio`,
   `Inspect_Lantern`, once each; confirm exactly one **Gentle** result,
   only Gentle outcome visible/collidable; return to ChoiceTest hub and
   confirm resolved indicator displayed.
2. Disruptive: completely new PIE session; use the three corresponding
   `SIM ATTACK` cubes, once each; require exactly one **Disruptive** result,
   only Disruptive outcome visible/collidable and resolved hub indicator.
3. Insufficient samples: new PIE session, interact only with the original
   statue **at least eight times**; 1/4 distinct core sample, no active
   outcome, no hub resolved indicator.
4. Seeking regression: separately replay merged B2b-3a ActiveTest map and
   run its original seeking log verifier; do not edit it.
5. Native tests: all 57 Pedro automated tests pass with zero errors,
   warnings, skips and no assertion failures. The existing B2b-2 world
   harness has eight additional PIE cases which are not counted among the 57.

## Runtime facts and interpretation

- Core gate is **three distinct of four configured core opportunities**;
  repeated actions on a single core never satisfy it.
- Test actions produce world event kinds `HelpedNPC` (enum 5) or
  `AttackedNPC` (enum 15); the Director uses `PrototypeAssumed` as its
  deliberately temporary knowledge policy, not a line-of-sight test.
- The revised parser checks **emission** (`PEDRO_CHOICE_TEST ... emitted=yes`)
  and **acquisition** (`Pedro PROTOTYPE-ASSUMED evidence ...`) separately;
  neither proves true NPC gameplay or what Pedro can physically see.
- Test encounter definition must use ID `PedroChoiceTest` and reference the
  copied Pedro map; its return volume must point to the copied hub.
- The Editor configuration verifier loads both levels and can discard
  unsaved edits. Save everything before running it.
- Actual visibility, collision from the player's perspective, spatial
  accessibility, and hub indicator require **manual visual signoff**. The
  parser cannot prove these.

## Caveats and next stage

The copied maps and simulated events demonstrate evidence-driven routing,
travel and session progression on a separate test surface only. This does
not implement genuine NPC assistance, combat, observer perception, activity
choice/denial, persistence to disk, or a finished game encounter. Production
promotion is a separate reviewed stage, not part of this PR.
