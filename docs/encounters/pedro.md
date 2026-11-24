# Summary

Pedro's encounter is a short, non-combat-focused encounter built around behavioral observation.

The player will be in an environment with many NPCs, objects, optional spaces, and minor activities. Minimal guidance will be given to the player, and an observer character will attempt to understand them and gather enough information by watching what they do.

After enough observations have been made, the player will receive a subjective interpretation of their actions: a biased guess from the observer character.

That guess will be one of a few strange roles. Each role will have a specific outcome tailored to it, and then the encounter resolves and the player is sent back to the game hub.

## Role Within the Full Game

The general idea for the full game is something along the lines of:

- Circus (Caoimhe) = **Perform**
- Medieval (Misha) = **Notice**
- Surreal Sandbox? (Pedro) = **Express**

Each encounter will have a unique AI idea (the main reason for this project is to explore and learn game AI). In this encounter, the AI idea depends on the player not being explicitly told what to do. The other encounters ask the player to read a performance or an environment; Pedro's encounter turns that around by having the environment observe the player.

### Player Problem

The encounter is testing what behaviors the player expresses when several legitimate things compete for their attention, rather than whether they can discover one correct way to play.

## Design Goals

- The player should gradually realize that the observer is paying attention.
- The player should feel that normal actions matter without seeing a visible personality meter.
- The observer should demonstrate memory of specific actions.
- Different natural play styles should produce different classifications.
- Deliberately trying to manipulate the observer should itself remain valid gameplay.
- No classification should be the obviously "correct" or mechanically superior one.
- The AI should be understandable and demonstrable technically, even if the player-facing result remains mysterious.
- Supporting activities should remain mechanically simple so development effort can focus on observation, perception, knowledge, and interpretation.

## Non-Goals

Pedro's encounter is not intended to provide:

- Conventional combat encounters;
- Lethal NPCs;
- A generic quest framework;
- A general inventory system;
- A project-wide player personality system;
- Machine learning;
- An LLM-driven personality classifier;
- A global gossip system;
- Save-game personality persistence;
- A universal observer framework;
- A morality system;
- An objectively correct psychological profile of the player.

## Core Observation Model

The general idea is something like:

Something happens → the observer sees it → the observer interprets it.

Or:

Something happens → the observer does not see it → the observer is missing that information.

### Observation Layers

The encounter distinguishes between three different kinds of state:

1. **World truth:** what the player actually did.
2. **Observer knowledge:** what the observer currently knows happened.
3. **Observer interpretation:** what the observer believes those known actions say about the player.

These states must remain separate.

An action that happens outside the observer's awareness should not affect the observer's interpretation unless that information later reaches them through another source.

The classifications are the observer's subjective opinions, not an objectively correct description of the player.

### Observer Knowledge

The basic goal is simply something like:

Direct observation happens → the observer has that information.

The idea in the future is to have multiple types of knowledge:

- **Direct observation** — the observer personally witnesses an action.
- **Reported (gossip)** — someone else tells the observer about an action.
- **Inferred** — the observer draws a conclusion from information they already possess.

Only direct-observation evidence is in scope for the current prototype. Reported and inferred knowledge are future possibilities, not systems that need to be implemented now. Actual visibility/awareness checks belong to the later physical-observer work; prototype events alone do not prove that the observer saw an action.

## Observation Opportunities

The moments where the observer gathers information need to be meaningful in some way, things like:

- responding to an NPC request;
- interacting with or ignoring an unusual object;
- solving, abandoning, or brute-forcing a simple puzzle;
- entering an optional space;
- interacting with breakable property;
- discovering a secret.

Opportunities may be either:

- **Core** — contributes toward encounter completion;
- **Bonus** — provides additional evidence but is not required for completion.

An opportunity can count as sampled when the player helps, refuses, ignores, or abandons it, provided they were meaningfully exposed to the choice. Merely having an interactable somewhere in the level does not mean the player has had that opportunity.

Completion will basically be achieved once the observer reaches a minimum amount of information, with **approximately 75% of core observation opportunities sampled** as the current tuning target. The observer should normally reach a conclusion before the player has exhausted everything there is to do. The percentage is not a permanent design rule.

## Behavioral Dimensions

The starting ideas for the dimensions being used to model the player currently are:

- **Curiosity:** Seeking optional information, investigating unusual objects, and exploring beyond the obvious path.
- **Care:** Helping characters, respecting their property, and responding considerately.
- **Disruption:** Altering, breaking, disturbing, or interfering with the environment.
- **Aggression:** Deliberately attacking characters or directing violence at social targets.
- **Patience:** Taking time, observing, and approaching situations deliberately.
- **Play:** Experimenting for its own sake, especially with things that provide no obvious progression.
- **Repetition:** Repeatedly returning to or interacting with the same thing.
- **Apathy:** Passing over meaningful opportunities after being exposed to them.

These are internal behavioral dimensions, not labels that should be shown to the player. Their exact values and weights remain undecided.

### Evidence Principles

Evidence should be a way to categorize **concrete behavior**, things like:

- `DestroyedOwnedProperty`
- `DestroyedUnownedProperty`
- `HelpedNPC`
- `IgnoredNPCRequest`
- `RepeatedInteraction`
- `DiscoveredSecret`

The interpretation system is responsible for deciding what those facts mean. Individual world objects should not directly define the player's final classification or assign their own personality scores.

Repeated actions may provide additional evidence, but repeating one action indefinitely should not give it unlimited influence over the final classification. The exact limit is left to tuning.

### Contextual Behavior

This will be part of the interpretation mentioned earlier. Depending on the context, it would only make sense for the observer to have a different opinion about what a certain action represents.

For example, keeping an item requested by an NPC may be:

- **Selfish** if the player has something meaningful to gain by keeping it;
- **Cruel** if keeping it meaningfully harms the NPC without a meaningful benefit to the player;
- **Neutral** if the player has nothing to gain and the NPC is not meaningfully harmed.

Destroying clearly NPC-owned property is meaningfully different from destroying an abandoned or unowned object. Ownership should be communicated clearly enough that the player can understand the choice.

Destroying clearly NPC-owned property should make Gentle / Considerate unavailable. Destroying unowned property may still be disruptive or playful, but does not automatically disqualify Gentle / Considerate.

## Provisional Internal Classifications

These are internal, developer-facing categories. The final roles presented to the player should have strange, abstract names that fit the world rather than simply exposing these labels. The observer ultimately chooses one classification, even when the evidence is mixed or incomplete.

### Gentle / Considerate

Associated with:

- helping;
- patience;
- respectful behavior;
- care;
- low aggression;
- avoiding harmful destruction.

Some actions should make this classification unavailable regardless of other positive evidence. Current examples include:

- deliberately harming or attacking NPCs;
- deliberately attacking the observer;
- cruel behavior toward an NPC;
- destroying clearly NPC-owned property.

### Seeking / Meddling

Associated with:

- curiosity;
- exploration;
- touching everything;
- repetition;
- secrets;
- playful experimentation.

### Disruptive / Forceful

Associated with:

- aggression;
- destructive interaction;
- disregard for property;
- deliberate disturbance;
- potentially chaotic experimentation.

A strongly considerate or altruistic action may count as evidence against this interpretation, but does not automatically make it unavailable. The observer may still classify someone as disruptive despite contradictory actions.

## Outcome Philosophy

The basic idea is to have no good or bad endings; each classification should be interesting in its own way.

The outcome of each classification should be tailored to what that player expressed. The three outcomes should feel meaningfully different, though the exact NPCs, scenes, rooms, or other presentations have not been decided yet.

No hidden behavioral classification should grant a mechanically superior encounter-completion reward. Classification-specific presentation remains separate from shared game progression.

### Final Reveal

The final classification should demonstrate that the observer remembers specific things the player did.

Whenever practical, the reveal should reference a small number of salient observations before presenting the final role.

The player does not need to see the internal scores or classification logic.

The goal is to make them understand that their actual behavior was noticed.

## Full-Game Integration Constraints

- Pedro's encounter remains resolvable with baseline player abilities.
- Pedro's encounter role-specific outcome should not gate the other encounters.
- Shared progression goes through the existing encounter-progress contract.
- Pedro's encounter-specific behavioral state remains encounter-local.
- Pedro's encounter should preserve the shared player controls and interaction language.
- Pedro's encounter should not require changes to the other encounter maps or the hub as part of normal encounter development.

## Combat Rules

The encounter has no conventional combat objective.

The player's normal attack remains available because using violence is itself a meaningful behavioral choice.

Having the attack enabled does not mean all the options normally available through attacking will be there. NPCs (including the observer) will not be conventionally killable.

Characters may react to being attacked, and attacking NPCs or the observer may provide aggression evidence, but the encounter should not transition into conventional combat. Repeated attacks against the observer may cause them to retreat or relocate rather than retaliate.

The encounter should not unexpectedly damage the player.

## Current Prototype Scope

**In scope currently:**

- Structured evidence types;
- Observation history;
- Behavioral dimensions;
- Direct-observation provenance;
- Authored interaction evidence;
- Repeated-interaction evidence;
- Core/bonus opportunity tracking;
- Greybox prototype opportunities;
- Debug visibility.

**Out of scope currently:**

- Final Gentle / Seeking / Disruptive classification logic;
- Final score weights;
- Physical observer AI;
- AI Perception;
- Gossip / reported knowledge implementation;
- Inferred knowledge implementation;
- Final NPC activities;
- Final puzzles;
- Final breakable-object gameplay;
- Final dialogue;
- Final player-facing role names;
- Final outcome scenes;
- Encounter rewards;
- HUD changes;
- Visual and audio polish.

## Deferred Decisions / Open Questions

The following decisions are deliberately deferred until prototype work provides enough information to make informed choices:

- Final strange names for the three roles;
- Exact point values;
- Exact weighting formula;
- Tie-breaking policy beyond "Pedro always chooses";
- Exact number of observations required;
- Exact repetition cap;
- Final NPC identities;
- Exact NPC quest;
- Final item contexts;
- Final puzzle;
- Final map layout;
- Exact number of rooms;
- Final visual palette;
- Observer model/design;
- Exact dialogue;
- Observer animations;
- Perception radius/FOV;
- Watch-point selection algorithm;
- Whether suspicion/adaptation ships;
- Whether gossip ships;
- Exact outcome presentations;
- Shared progression reward;
- Exact HUD treatment;
- Music/audio;
- Exact encounter length beyond the rough short-prototype target.

## Implementation status (2026-11-24)

The design above records the original intent; the prototype scope and deferred decisions are historical, not a list of work still outstanding.

- The production greybox is playable from the hub. Pedro autonomously notices, follows, observes activities, relocates to watch points, withdraws and becomes satisfied after completion.
- The encounter-local integration actor manages activation and teardown; the Director owns evidence and completion; the AI controller owns perception, attention and movement. Unseen actions are not replayed, and lost sight does not expose the player's live position.
- Four core inspections and one bonus opportunity are authored. Three distinct witnessed core samples resolve the encounter once. Repetition and bonus evidence do not add distinct core progress. A real melee hit on Pedro supplies nonlethal `DirectContact`, not visual knowledge or a core sample. Misses grant no evidence.
- The provisional classifier supports Gentle, Seeking and Disruptive. Current inspection-based gameplay favors Seeking; the three outcomes are still placeholders. Session progress survives hub travel; unresolved reentry starts fresh and resolved reentry does not reactivate the observer.

### Development and testing

Use UE 5.8.2, materialized Git LFS assets and a clean or fully staged checkout. Run from the project root; close Unreal before automated validation. On the existing Linux/NixOS setup:

```bash
# Manual production play, starting in the hub
unreal-engine "$PWD/AI_Gavin_Project.uproject" /Game/AI_Gavin/World/Hub/Maps/L_Hub_Prototype -game -log

# Offline tests
nix develop '/etc/nixos#default' --command python3 -B -m unittest discover -s Tools/PedroValidation/tests -q

# Native and world regression; run the additional suites when relevant
bash Tools/PedroValidation/validate.sh world all
bash Tools/PedroValidation/validate.sh interaction production-rendered
bash Tools/PedroValidation/validate.sh flow all
bash Tools/PedroValidation/validate.sh maps verify-production
```

The validation entry point also supports `interaction test`, `test-rendered`, `production`, `maps verify-test`, and `legacy-map verify` (each with its corresponding suite). Reports are in `Saved/PedroValidation/`. Use the isolated validation maps for authoring experiments; do not regenerate accepted maps as routine testing. Automated fixtures use real components but are not physical-input tests.

The pre-cleanup standalone prototype passed all 18 operator checks. After cleanup, 387 offline tests, 172 native tests in each compilation mode, 18 world processes, five interaction processes, eight flow processes and three saved-map audits passed. The old Linux Shipping package passed cooking, map inventory and process stability, but its startup map override did not load Pedro; packaged Pedro gameplay remains unverified. Detailed historical acceptance and the tested Vulkan workaround are archived outside the active documentation; do not reuse checkpoint-specific recovery kits as release tools.

### Next gameplay work

Add genuine NPC assistance, property interactions and simple puzzles; demonstrate all three interpretations through legitimate gameplay; give each outcome a distinct presentation, then tune scoring. Keep progression shared, behaviour encounter-local, and the original design goals intact.
