# Pedro Phase B2b-1: evidence-driven C++ policy and guarded finalization

## Scope

This adds opt-in, evidence-driven completion to `APedroEncounterDirector`.
`LegacyInteractionCount` remains the default. Existing maps, Blueprints,
shared progression code and prototype outcome actors are unchanged.

B2b-1 is **not** permission to switch the shipped Pedro level to the new mode.
A later asset-validation stage must author and test three distinct outcome actors.

## Runtime contract

1. The old `OnInteractionDispatched` path is used only in legacy mode.
2. Accepted evidence and meaningful opportunity exposure queue a coalesced
   next-tick active evaluation only in evidence-driven mode.
3. `PedroCompletionGate::Evaluate()` must report `CanConclude()`.
4. `PedroActivePresentation::Prepare()` requires three valid, distinct targets
   regardless of which interpretation was selected.
5. World, same-world actor references, a nonempty encounter ID, GameInstance,
   progress subsystem and unresolved encounter are checked before mutation.
6. Director guards against reentrancy, freezes the winning classifier result,
   activates exactly one interpretation actor, then calls
   `UEncounterProgressSubsystem::ResolveEncounter()`.
7. On a rejected progression call, the three visuals and local finalized state
   are rolled back. On success, the Director logs `PEDRO_OUTCOME ... resolved=yes`.
8. EndPlay removes both old timers and the new active timer.

`PrototypeAssumed` is still a deliberate stand-in for observer awareness.
None of this work implements physical line-of-sight perception.

## Existing legacy contract

The original outcome actors remain authoritative when CompletionPolicy is
`LegacyInteractionCount`. The eight-interaction greybox level must continue
working unchanged. The new three outcome fields default to null on the
production map, and that is valid while legacy policy remains selected.
Legacy mode does not touch the new active outcome actors even if optional
active assignments exist.

## Tests

Previous baseline: 47 Pedro tests. This change adds 10 more:

- Six pure presentation tests (gate not ready, missing actor, duplicate actor,
  and one correct mapping for each interpretation).
- Four Director tests (legacy default, clicks ignored in active mode,
  incomplete evidence blocked, ready gate blocked without a real World).

Expected total: **57**. Unit tests are worldless and do not prove successful
world-backed progression or render visibility. Test all three interpretations,
precondition failures, repeated events and hub progression in a controlled PIE
configuration before enabling EvidenceDriven on the production level.

## Recommended next stage

B2b-2: Create three clear outcome visuals in a dedicated test level or in a
reversible per-level configuration; run PIE from the hub for Gentle, Seeking
and Disruptive; verify no partial resolution on missing dependencies; verify
exactly-once progression. Keep all asset changes in a separate pull request.

## Review caveat

The active completion path updates presentation immediately before calling
`ResolveEncounter()`, and rolls local state back if resolution is refused.
This has not yet been validated against world-backed callbacks such as
`OnEncounterResolved` listeners that trigger map travel, or collision-related
callbacks from outcome activation. B2b-2 must exercise these cases before
active mode is used in the production level. A successful Editor compile and
57/57 automation tests do **not** prove the active presentation transaction.
