# Pedro completion gate: Phase B1

## What this milestone delivers

`PedroCompletionGate::Evaluate(const FPedroObservationState&, float)` is a pure,
non-Blueprint decision function that reports whether Pedro has enough accepted
knowledge to *form a candidate interpretation*. It composes the existing
`FPedroObservationState`, `FPedroOpportunityState`, and
`PedroClassification::Evaluate()` introduced in Phase A.

It makes **no changes to the live encounter**. In particular, it does not
subscribe to events, check line of sight, run timers, change outcome actors,
resolve progress in `UEncounterProgressSubsystem`, or replace the current
interaction-count prototype. It does not need map asset changes.

## Inputs and invariants

- Authored core/bonus opportunity definitions are already installed on the
  observation state by the Director.
- A core opportunity is sampled only when both `bPlayerExposed` and
  `bPedroAcquiredResponse` are set by the existing opportunity machinery.
- The full sampling threshold uses the existing fraction clamp and upward
  rounding. The current tuning value is 0.75, so three of four core choices
  suffice; bonus choices never substitute for core choices.
- Accepted evidence lives in `EvidenceHistory`; `RecordEvidence()` is its
  gatekeeper for event IDs and supported knowledge provenance.
- Each sampled core must also have an accepted evidence record with the same
  opportunity ID. This final check catches misuse of the low-level
  `NoteAcquiredResponse()` API when record acquisition has not occurred.
- Only a Ready result has a meaningful `Classification` value.

## Status values

| Status | Meaning |
| --- | --- |
| `NoCoreOpportunities` | No authored core choices; never enough information |
| `InvalidSampleFraction` | The passed fraction is non-finite (NaN or infinity) |
| `NoAcquiredEvidence` | Pedro has zero accepted evidence records |
| `InsufficientCoreSamples` | Fewer than the required core choices are sampled |
| `InconsistentSampleEvidence` | Sampled core flag has no corresponding accepted record |
| `Ready` | The sampling and evidence gates pass; a provisional interpretation is available |

No status performs a transition or grants encounter completion. A future
Director integration may consume `CanConclude()` and `Classification`, but must
still check one-shot presentation, required outcome actors, and shared progress
lifecycle separately.

## Test expectations

The added `AI_Gavin_Project.Pedro.CompletionGate` suite has **18** tests:
empty configuration, bonus-only, no evidence, exposure-only, evidence-only,
below threshold, exactly at threshold, all four core, bonus not counted,
evidence-before-exposure, exposure-before-evidence, duplicate/repetition,
provenance rejection, fractional rounding, reset, Gentle/Disruptive forwarding,
inconsistent manually set sample flags, and non-finite fractions (NaN, positive and negative infinity),
plus lower- and upper-bound clamping of finite fraction values.

Expected full regression suite after installing Phase B1: **41 Pedro tests**
(12 existing observation/prototype, 11 Phase A classification, 18 Phase B1 gate).
Only an actual Unreal build and automation run can establish that these pass.

## Phase B2 handoff (not implemented here)

1. Author a separate completion policy on the Director, preserving legacy
   behavior as an explicit selectable mode during transition.
2. Evaluate readiness after confirmed accepted evidence and after meaningful
   exposure; the two signals can arrive in either order. Do not resolve purely
   from a dispatched interaction.
3. Defer evaluation until any in-progress interaction has emitted its confirmed
   action, as the current prototype does using next-tick scheduling.
4. Check `bHasPresentedOutcome` and the presentation actors before applying a
   classification. Do not accidentally resolve while no three-way presentation
   policy exists.
5. Resolve shared progress through `UEncounterProgressSubsystem` exactly once.
6. Add integration tests for gameplay lifecycle, no double-resolution, missing
   actors, and return-to-hub behavior before switching the production map.
7. Preserve `PrototypeAssumed` provenance labeling until physical observer
   awareness is genuinely implemented.

See the central encounter concept document for longer-term goals.
