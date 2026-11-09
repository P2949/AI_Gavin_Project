# Pedro Phase B2b-2: controlled PIE world validation

The code added here is a **temporary developer test fixture**, not gameplay.
It refuses the production Pedro and hub maps even if enabled accidentally.
It performs active-mode checks in an actual PIE `UWorld` using the real
`UGameInstance`-owned `UEncounterProgressSubsystem` without changing the Pedro
map or any asset. The fixture requires deliberate per-instance opt-in and a PIE
world; it does nothing by default.

## Setup

1. Build and run the existing 57 Pedro automation tests using the supplied script.
2. Open the Unreal Editor. **File > New Level > Empty Level** (or Basic Level).
   Keep the new map **unsaved**, away from production encounter content.
3. From Content Drawer > C++ Classes > AI_Gavin_Project, locate the
   `PedroActiveWorldValidationHarness` actor and drag ONE instance into the
   new unsaved level. You can also use the Place Actors class search.
4. In its Details panel enable `Run Validation In PIE`.
5. Start PIE. The actor creates a unique transient encounter definition per
   scenario and spawns all the necessary actors. No player input is required.
6. Leave PIE running for several frames until the Output Log shows
   `PEDRO_WORLD_TEST status=SUMMARY passed=8 failed=0 total=8`.
7. Exit PIE. **Do not save the level.** Close the Editor and run the supplied
   `verify_pie_world_report.py` against the Editor's `-abslog` file.

## Eight cases

- Seeking: 3 distinct inspection opportunities -> real progress + selected cube.
- Gentle: 3 distinct helped-NPC events -> Gentle + selected cube.
- Disruptive: 3 distinct NPC attack events -> Disruptive + selected cube.
- Reentrant: `OnEncounterResolved` listener calls the Director again and emits
  another event; a single progress broadcast and frozen interpretation remain.
- MissingOutcome: an unassigned Disruptive actor fails closed even when Seeking
  would otherwise be selected.
- AlreadyResolved: the fixture's unique encounter ID is pre-resolved; new
  evidence must not cause a second broadcast or presentation.
- InsufficientSamples: repeated actions against one core cannot satisfy 3/4.
- EmptyEncounterId: a transient definition without an ID cannot resolve.

Each case checks actor visibility/collision, classification state, resolved ID
count, multicast callback count, and progress state observed **within the
callback**. All 8 cases must pass. They run in one PIE session.

## Safety and scope

- No source/gameplay Director behavior changes, only a test-only friendship
  declaration and two new fixture C++ source files.
- The fixture defaults **off**. It only executes in `EWorldType::PIE`.
- It uses unique transient encounter IDs and **never calls ResetRun()**.
- It does not test save-game persistence, map travel, real player interactions,
  physically verified awareness/LOS, or the production level's visuals.
- It should not be placed in or saved to the production Pedro map.
- The transient actor class should be removed or editor-gated before shipping.

## Follow-up before production enablement

Real user-facing Gentle/Seeking/Disruptive presentation actors must be authored.
A reversible copy of `L_Pedro_Prototype` should be used to validate actual
player-driven Seeking completion and return-to-hub travel. Gentle/Disruptive
need corresponding genuine playable actions; fixture-generated facts are not
proof these actions are represented in the existing greybox level.
