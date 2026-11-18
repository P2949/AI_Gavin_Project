# Pedro B2b-5b2 — reproducible playable physical-observer integration

**Baseline:** merged `main` at `8a6f31356ebd9972279d37cb9fd6777d7df05b60` (PR #45).
**Branch:** `feature/pedro-physical-observer-playable`.
**Required engine:** Unreal Engine **5.8.2**, Linux editor with Nix + `steam-run`.

## Intent and boundaries

Copy the already validated B2b-4b player-driven `DirectPlayTest` levels into **new** test levels. Replace their fixed observer `TargetPoint` with a visible Blueprint child of the already merged `APedroObserverCharacter`; preserve the existing authored inspection sources, 4 core opportunities, 75% completion threshold, three outcome actor references, and session-level travel/return flow. Use real player inspections; never inject behavioural evidence through editor scripts.

This is a map/Blueprint integration stage, **not** autonomous AI or new gameplay C++ logic. Do not modify any production map, earlier validation map, shared player controller, Director source, or observation code. Do not claim that inspections implement genuine NPC care or aggression. The main player-driven completion expectation here is **Seeking**.

## Expected new assets

All new UE assets live under `/Game/AI_Gavin/Encounters/Pedro/Validation/`:

| Asset | Created from |
|---|---|
| `L_Pedro_ObserverActorTest.umap` | `L_Pedro_DirectPlayTest.umap` via `LevelEditorSubsystem.new_level_from_template` |
| `L_Hub_Pedro_ObserverActorTest.umap` | `L_Hub_Pedro_DirectPlayTest.umap` via `new_level_from_template` |
| `DA_Encounter_Pedro_ObserverActorTest.uasset` | `DA_Encounter_Pedro_DirectPlayTest.uasset` via Editor asset duplication |
| `BP_PedroObserver_Validation.uasset` | New Blueprint child of `APedroObserverCharacter` |

The Blueprint adds `ValidationBody` (engine cube, upright scale, no collision), leaving native `ObserverEye` attached to the Character capsule. The new Character replaces and is positioned to match the former `PEDRO_DIRECT_PLAY_ObserverEye` reference in the **copied** level. All map and asset operations happen through Unreal's Editor APIs, never by copying `.umap`/`.uasset` bytes on disk.

## Required preparation

The kit installs these files (no automatic Git staging/committing):

- `Tools/PedroValidation/b2b5b2_editor.py`: Unreal Editor Python authoring/inspection/verification.
- `Tools/PedroValidation/b2b5b2_run.sh`: Nix/Steam launcher, transient plugin enablement, report verification and `.uproject` restoration.
- `Tools/PedroValidation/b2b5b2_audit.py`: offline changed-file and Git LFS attribute audit.
- This document.

Use the kit's `Scripts/install.sh` once, from a **clean** new branch. Required commands after installation:

```bash
cd "$HOME/Development/Unreal/Projects/AI_Gavin_Project"

git lfs pull
git lfs fsck

# Read-only inventory first. Result is Saved/.../result.json.
bash Tools/PedroValidation/b2b5b2_run.sh inspect

# Review the inventory: 4 core opportunities, 3 distinct outcomes,
# 1 TargetPoint labelled PEDRO_DIRECT_PLAY_ObserverEye,
# >=3 real Pedro prototype interactables, one Director, one return volume,
# one copied-hub entrance.
RUN="$(cat Saved/PedroValidation/b2b5b2-inspect-latest.txt)"
cat "$RUN/result.json"

# One-shot creation. Fails if any destination already exists.
bash Tools/PedroValidation/b2b5b2_run.sh build

# Independent reload and reference checks.
bash Tools/PedroValidation/b2b5b2_run.sh verify

# Audit which files were created/modified.
nix develop '/etc/nixos#default' --command python3 \
  Tools/PedroValidation/b2b5b2_audit.py
```

The runner enables `PythonScriptPlugin` and `EditorScriptingUtilities` *temporarily* in the local `.uproject` when it launches the full Unreal Editor. It backs up the descriptor in ignored `Saved/`, restores it **byte for byte** when the process exits, and validates the Editor Python output using an explicit JSON report. Do not run a second Unreal Editor or another script against this project concurrently. If a process is forcibly killed while the runner is also killed (e.g., power loss), manually restore the descriptor using `Saved/PedroValidation/b2b5b2-<mode>-<timestamp>/original.uproject` after verifying it belongs to the current project.

### UE 5.8.2 known two-map partial-build recovery (2026-11-13)

In the first run (`b2b5b2-build-20261113T132559Z`), Unreal successfully created
`L_Pedro_ObserverActorTest.umap` and `L_Hub_Pedro_ObserverActorTest.umap`, then
stopped before saving the Blueprint because the inherited Character capsule is
named **`CollisionCylinder`**, not `CapsuleComponent`.

The v2 correction selects the inherited capsule by actual `CapsuleComponent`
class and accepts the native `CollisionCylinder` name. It also removes an
unnecessary second `attach_subobject` call: `add_new_subobject` already creates
the component under its requested `parent_handle`.

After applying the v2 patch, **do not rerun `build` or `inspect`**. Both are
deliberately incompatible with pre-existing destination maps. Instead, with
all Unreal Editor instances closed, run:

```bash
cd "$HOME/Development/Unreal/Projects/AI_Gavin_Project"

bash Tools/PedroValidation/b2b5b2_run.sh resume
RUN="$(cat Saved/PedroValidation/b2b5b2-resume-latest.txt)"
cat "$RUN/result.json"

# Run independently even if resume performed an in-process verification.
bash Tools/PedroValidation/b2b5b2_run.sh verify
RUN="$(cat Saved/PedroValidation/b2b5b2-verify-latest.txt)"
cat "$RUN/result.json"
```

`resume` requires exactly the two saved new maps, and **neither** of the two
new `.uasset` files. Before saving anything, it independently reloads both
original DirectPlayTest levels and both partially created maps. It checks actor
inventories, core opportunities, existing Director/entrance references,
TargetPoint pose and return-map references. Any mismatch aborts without trying
to overwrite existing assets. Once this preflight passes, it creates the
physical Character Blueprint and Data Asset, binds the new level references,
and validates the result.

The v2 installer makes backups of the original installed scripts and this
document under the ignored `Saved/PedroValidation/` tree and refuses to patch
unknown versions of the files. It never deletes a `.umap`/`.uasset` file.

If a partial `resume` fails *after* the Blueprint or Data Asset has been saved,
**do not repeat resume**. Preserve the JSON/editor logs and inspect the new
assets in Unreal Editor before choosing a new repair. The recovery mode is
intentionally limited to the observed two-map state.

The Python script is fail-closed. It tests the source map shape before writing, rejects preexisting destination assets, and never silently overwrites prior work. **It is not a transactional database**: partial new assets may survive a crash or failed Editor operation. Investigate the exact error and clean up only the new destination assets in Unreal Editor if a fresh build is needed; never delete source maps or use a bulk shell `rm` under Content. `verify` can be rerun after repair. Any failure in PIE means no commit until fixed.

### Soft-reference rendering repair (v3, 2026-11-13)

A v2 `resume` preflight aborted **before asset creation** when it tried to
verify `PedroPrototypeReturnVolume_1.return_map` by converting an Unreal
`SoftObjectPath` struct with Python `str(...)`. The engine returned the
struct wrapper representation (`<Struct 'SoftObjectPath' ... {}>`), which
is not a meaningful path string. This is **not evidence** that the source
ReturnMap is missing or invalid. `verify` still cannot pass before the
Blueprint/Data Asset are created.

v3 uses Unreal's documented soft-reference-to-string conversion and
`StructBase.export_text()` with strict package-path normalization. Both
`check_soft_world_property` **and** `set_soft_world_property` use it, so a
reference is never considered correct solely because a `set_editor_property`
call returned without exception.

The v3 launcher has a new **read-only `probe` mode** which works with the
existing two-map-only partial state. Run `probe` first; it verifies both
source/copy return maps and the original Data Asset's map reference. It
writes only a JSON result and logs under ignored `Saved/`; it does not
save, create, modify or delete Unreal Content assets.

```bash
cd "$HOME/Development/Unreal/Projects/AI_Gavin_Project"

bash Tools/PedroValidation/b2b5b2_run.sh probe
RUN="$(cat Saved/PedroValidation/b2b5b2-probe-latest.txt)"
cat "$RUN/result.json"

# ONLY AFTER status=PASS, run the existing guarded continuation.
bash Tools/PedroValidation/b2b5b2_run.sh resume
RUN="$(cat Saved/PedroValidation/b2b5b2-resume-latest.txt)"
cat "$RUN/result.json"

# After resume PASSES, run an independent reload.
bash Tools/PedroValidation/b2b5b2_run.sh verify
RUN="$(cat Saved/PedroValidation/b2b5b2-verify-latest.txt)"
cat "$RUN/result.json"
```

Do not run `build` on an existing destination map, or `verify` before
creation has finished. If `probe` fails, read `result.json`: it reports
all attempted conversions and the actual path if retrievable. If `resume`
fails after saving a Blueprint/Data Asset, do **not** repeat it without a
new inspection. Preserve those assets, logs and Git state for a targeted fix.

## Authored final configuration

`DA_Encounter_Pedro_ObserverActorTest`:

- `EncounterId = PedroObserverActorTest`
- `EncounterMap = /Game/AI_Gavin/Encounters/Pedro/Validation/L_Pedro_ObserverActorTest`

Copied hub `AEncounterEntranceActor`:

- `EncounterDefinition = DA_Encounter_Pedro_ObserverActorTest`
- Preserve resolved-indicator actor from DirectPlayTest.

Copied Pedro map `APedroEncounterDirector`:

- `EncounterDefinition = DA_Encounter_Pedro_ObserverActorTest`
- `CompletionPolicy = EvidenceDriven`
- `bUsePrototypeAssumedAcquisition = false`
- `bUseDirectObservationAcquisition = true`
- `DirectObservationActor = BP_PedroObserver_Validation` **placed actor**
- 4 configured distinct core opportunities, ratio = `0.75`
- Preserve DirectPlayTest's 5000 cm maximum observation distance.
- Set view half-angle to 75 degrees on the copied Pedro Director after validating player-driven Seeking and facing-away scenarios.
- Preserve **three different**, valid Gentle/Seeking/Disruptive outcome actors.

Copied Pedro `APedroPrototypeReturnVolume`:

- `ReturnMap = /Game/AI_Gavin/Encounters/Pedro/Validation/L_Hub_Pedro_ObserverActorTest`

The observer's root transform is derived from the original TargetPoint, compensating for the native `(0,0,64)` observer-eye offset. The original TargetPoint is removed only from the **new copied** Pedro map. The scripts verify that all critical references persist after reload.

## Player-driven PIE acceptance

**Each scenario begins with a new Play-In-Editor session from** `L_Hub_Pedro_ObserverActorTest`. Always stop PIE before editing actor transforms. Restore the visible default configuration before final save/commit. For independent negative scenarios, either adjust the *copied* observer pose temporarily and **do not save** or use dedicated local test variants; do not mutate earlier validation or production maps.

1. **Seeking / positive**: With Pedro oriented as in the source DirectPlay test, use actual player input to inspect original Statue, Radio, and Lantern once each. Expect `PEDRO_DIRECT status=Visible`, `PEDRO_DIRECT acquired=yes` for three distinct core opportunities and exactly one `PEDRO_OUTCOME policy=EvidenceDriven interpretation=Seeking sampledCore=3 requiredCore=3 resolved=yes actualLOS=checked`. Verify only Seeking outcome visible/collidable. Enter return volume, arrive back at copied hub, and check the resolved indicator is ON.
2. **Facing away / negative**: Start new PIE with Pedro facing away (or turn it via the Details panel before PIE). Inspect Statue once. Expect `OutsideViewCone`, zero acquired evidence from that inspection, no active outcome, copied hub indicator OFF.
3. **Range / negative**: New PIE. Move observer's placed Character beyond `DirectObservationMaxDistance` of Statue, leaving source prop unchanged. Inspect once. Expect `TooFar`, zero acquired, no completion/indicator.
4. **Cover / negative**: New PIE. In the *copied* level place/move a Visibility-blocking test wall such that it interrupts the eye-to-**Statue actor origin** segment (not merely the player's view). Inspect once. Expect `Occluded`, zero acquired; before this test prove the same inspection is visible with the wall disabled.
5. **No-retroactive regression**: The previously merged B2b-5b1 8/8 PIE harness already tests missing an old event and acquiring only a new event after turning. The player-driven B2b-5b2 fixture can also manually confirm that simply turning back without a new inspection does not create evidence. Do not report this as a full player-driven automated test without performing it.
6. **Old-map regression**: Open the untouched `L_Hub_Pedro_DirectPlayTest` and replay its original Seeking scenario. No new observer or Data Asset is permitted in old maps.
7. **Native regression**: Build `AI_Gavin_ProjectEditor` and obtain **67/67 Pedro native automation tests** with no failure/warnings/skips. Existing B2b-5b1 world test evidence remains valid if its source did not change.

For a manual test log, launch the Editor as usual using `nix develop ... steam-run UnrealEditor` with `-abslog=<ignored Saved path>`. The new test maps are *not* made the startup map, so open the copied hub manually before PIE.

An offline grep may inspect the lines below, but it **does not prove** correct player interactions or actor visibility by itself:

```bash
grep -E 'PEDRO_DIRECT status=|PEDRO_DIRECT acquired=|PEDRO_SHADOW|PEDRO_ACTIVE|PEDRO_OUTCOME|Browse:|LoadMap:' "${EDITOR_LOG}"
```

## Review and commit boundary

1. Run `inspect`, `build`, `verify` and the offline audit; all must pass.
2. Play each PIE scenario, retain logs under ignored `Saved/PedroValidation/`, and record manual sign-off separately.
3. Review the new .umap/.uasset files inside Unreal Editor, check all references and visible outcomes.
4. Run existing native test script and require 67/67.
5. `git lfs fsck`, `git lfs status`, `git status --short --untracked-files=all`.
6. Stage only the four new Unreal assets, this doc, and the three automation scripts, plus any *reviewed* legitimate external actor packages. Do not stage generated files, modified existing maps, `.uproject`, or arbitrary external actors.
7. Commit and push only after successful results; provide the validation report and a reviewable description of binary changes in the PR.

## Known limitations

- Unreal Editor Python can automate asset/level authoring but does **not** run as gameplay Python during PIE. This kit deliberately does not fake a player's interactions.
- Native observer is a physical viewpoint host but has **no autonomous AI movement, attention state, or AI perception memory**. The appearance is a static validation cube.
- Direct observation is a synchronous single `ECC_Visibility` ray toward the source actor's origin. It is not camera rendering visibility or an AI Perception sight model.
- Existing Gentle/Disruptive playable fixtures use simulated event-kind emitters, **not** real NPC help or attacks. Their behavior is not extended by this asset-only PR.
- Level duplication or SubobjectData APIs may differ in project builds. When the provided script reports a specific mismatch, inspect the report and adjust the script against the actual UE 5.8.2 Python API; do not bypass validation or overwrite source assets.
- The kit's source has been syntax-checked outside Unreal, but its Editor asset operations cannot be executed in this environment. **Your machine must establish the actual `build`, `verify`, PIE, and 67/67 outcomes.**

## Mechanical occlusion validation results

Validated on 2026-11-18 using Unreal Engine 5.8.2 on Linux.

The mechanical integration test supersedes manual wall-placement
occlusion acceptance testing.

The test executes the saved ObserverActorTest encounter using
its actual Director, observer Character, player pawn and
Statue interaction implementation.

| Case | NullRHI | Rendered |
| --- | --- | --- |
| BaselineClear | PASS | PASS |
| BlockedFirst | PASS | PASS |
| NoRetroactive | PASS | PASS |
| IgnoreAllowsNew | PASS | PASS |
| ReblockRejectsNew | PASS | PASS |
| RemovalAllowsNew | PASS | PASS |

Both executions established:

- Four successful inspections.
- Four unique inspection events.
- Exactly two acquired direct observations.
- Correct occlusion rejection.
- No retroactive evidence acquisition.
- No duplicate core opportunity sampling.
- No premature encounter resolution.
- Successful restoration of the ordinary editor build.
- Unchanged repository file hashes and Git status.
- Successful Git LFS integrity verification.

Local validation reports:

- b2b5b2-mechanical-occlusion-rfqSopzS
- b2b5b2-mechanical-occlusion-TYl13bZG

The temporary native fixture remains external to this milestone.
This test validates collision-based observation, not rendered
visibility or the player input interface.


## Final regression results

Original DirectPlayTest encounter:

- Original hub-to-encounter travel: PASS.
- Statue, Radio and Lantern direct observations: PASS.
- Three distinct sampled core opportunities: PASS.
- Evidence-driven Seeking resolution: PASS.
- Return to original hub: PASS.
- Clean PIE and editor shutdown: PASS.

Native automation:

- Editor Development build: PASS.
- Pedro native automation tests: 67/67 PASS.
- Test warnings, failures and skips: zero.

No existing production or DirectPlayTest assets were modified.

Native validation report: `b2b5b2-native-npBfvr2P`.
