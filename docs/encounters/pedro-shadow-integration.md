# Pedro Phase B2a: shadow-only Director completion gate

This milestone connects `APedroEncounterDirector` to the existing Phase B1
`PedroCompletionGate::Evaluate()` **for observation only**. It changes no
existing completion policy, outcome actors, hub progression, map, or
Blueprint. **Do not treat `ready=yes` as actual completion.**

## Behavior

- `bEnableCompletionGateShadowEvaluation` defaults to `true`, so the existing
  greybox map exercises the new diagnostics without saving an asset.
- After meaningful opportunity exposure or *accepted* observer evidence,
  the Director schedules a single shadow evaluation for the following tick.
  This groups exposure and evidence emitted during one interaction.
- Standalone worldless C++ tests evaluate immediately instead of using a timer.
- Shadow evaluation updates a read-only native snapshot and logs when status,
  sampled counts, or a ready interpretation changes.
- `PEDRO_SHADOW ... ready=yes candidate=Seeking` means the **candidate** gate
  could conclude in a later milestone; the legacy completion mechanism is still
  solely responsible for presenting and resolving the encounter.
- Real perception remains unimplemented. Existing prototype acquisition uses
  `PrototypeAssumed`; `actualLOS=no` explicitly communicates that limitation.
- When the existing legacy outcome is presented, the Director rejects further
  source events as before. A shadow evaluation already queued on that same
  frame can still record the final snapshot after the legacy timer runs.
- EndPlay cancels both the original legacy timer and the new shadow timer.

## Verification

The existing suite is 41 tests. This milestone adds 6 tests under
`AI_Gavin_Project.Pedro.DirectorShadow`, expecting 47 total:

1. AcquisitionRequired (world event alone cannot imply knowledge).
2. ExposureThenEvidence (three-of-four threshold, Seeking candidate).
3. EvidenceThenExposure (both signal orders work).
4. Disabled (diagnostics off but evidence tracking is unchanged).
5. DuplicatesAndPostOutcome (duplicates and post-resolution events ignored).
6. LegacyRemainsAuthoritative (shadow readiness does not present an outcome).

Run a full native Editor build and all 47 automation tests. The archive's
`run_b2a_validation.sh` verifies the generated JSON report, not merely
UnrealEditor-Cmd exit status. It does not make a claim about physical perception,
asset validation, or gameplay PIE; conduct those independently.

## Manual PIE expectations

The prototype map previously had four core inspection props and one bonus,
with legacy completion at eight interactions. Under the existing 0.75 fraction,
three *different core props* should produce `PEDRO_SHADOW status=Ready ...
ready=yes candidate=Seeking` while **the encounter remains unresolved until
legacy interaction number eight**. Repeating just one prop eight times should
still resolve through the existing repetitive presentation, with shadow sampling
remaining below the core threshold. Continue confirming return-to-hub progression.

These are expected behaviors, not claims of having run PIE in this archive.

## Next milestone

Phase B2b should introduce an explicit completion policy (legacy default),
three valid distinct presentation targets, and guarded, exactly-once shared
progression. Do not enable that policy on the production level until it has
separate functional PIE coverage and outcome assets.
