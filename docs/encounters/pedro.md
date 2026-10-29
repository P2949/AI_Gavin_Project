# Summary

Pedro's encounter is a short, non-combat-focused encounter built around behavioral observation.

The player will be in a enviroment with many NPCs, objects, optional spaces and minor activities, minimal guidance will be given to the player and a observer character will attempt to understand and gather enought information on the player by observing what they are doing 

After enough observations are done the player will receive a subjective interpretation of their actions, a biased guess from the observer character. 

That guess will be one of a few strange roles and each role will have a specific outcome tailored to that role, and then the encounter resolves and the player is sent back to the game hub.  

## Role Within the Full Game

The general idea for the full game is something around the lines of:

Circus(Caoimhe) = Perform
Medieval(Misha) = Notice
Surreal Sandbox? (Pedro) = Express

Each encounter will have a unique AI idea (the main reason for this project is to explore and learn game AI), In this encounter the AI idea depends on the idea that the player has not being explicitly told what to do.

### Player Problem

The encounter is testing what behaviors the player expresses when several legitimate things compete for their attention.

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

Something happens -> The observer see's it -> observer interprets 

or 

Something happens -> The observer does not see it -> observer is missing that information 

### Observation Layers

The encounter distinguishes between three different kinds of state:

1. World truth: what the player actually did.
2. Observer knowledge: what the observer currently knows happened.
3. Observer interpretation: what the observer believes those known actions say about the player.

These states must remain separate.

An action that happens outside the observer's awareness should not affect the
observer's interpretation unless that information later reaches them through
another source.

### Observer Knowledge

The basic goal is simply something like:

Direct Observation happens -> Observer has that information

But the idea in the future is to have multiple types of sources:

- Direct Observation

- Reported (gossip)

- Inferred

## Observation

The moments where the observer gathers information need to meaningful in some way, things like:


- responding to an NPC request;
- Interacting with or ignoring an unusual object;
- Solving, abandoning, or brute-forcing a simple puzzle;
- entering an optional space;
- interacting with breakable property;
- Discovering a secret.

Opportunities may be either:

- **Core** — contributes toward encounter completion;
- **Bonus** — provides additional evidence but is not required for completion.

Completion will basically be achieved once the observer reaches a minimum amount of data, approximately 75% of core observation opportunities.

## Behavioral Dimensions

The starting ideas for the dimensions being used to model the player currently are:

- Curiosity
: Seeking optional information, investigating unusual objects and exploring beyond the obvious path.

- Care
: Helping characters, respecting their property and responding considerately.

- Disruption
: Altering, breaking, disturbing, or interfering with the environment.

- Aggression
: Deliberately attacking characters or directing violence at social targets.

- Patience
: Taking time, observing, and approaching situations deliberately.

- Play
: Experimenting for its own sake, especially with things that provide no obvious progression.

- Repetition
: Repeatedly returning to or interacting with the same thing.

- Apathy
: Passing over meaningful opportunities after being exposed to them.

### Evidence Principles

Evidence should be a way to categorize behavior, things like:

- `DestroyedOwnedProperty`
- `DestroyedUnownedProperty`
- `HelpedNPC`
- `IgnoredNPCRequest`
- `RepeatedInteraction`
- `DiscoveredSecret`

The interpretation system is responsible for deciding what those facts mean. Individual world objects should not directly define the player's final classification.

### Contextual Behavior

This will be part of the interpretation mentioned earlier, depending on the context it would only make sense for the observer to have different final opinion on what a certain action represents.

Destroying clearly NPC-owned property is meaningfully different from destroying an abandoned or unowned object.

## Provisional Internal Classifications

### Gentle / Considerate

Associated with:
- helping;
- patience;
- respectful behavior;
- care;
- low aggression;
- Avoiding harmful destruction.

Some actions should make this classification unavailable regardless of other positive evidence, a simple example is:

- Harming NPCs in a deliberate and intentional way.

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

## Outcome Philosophy

The basic idea is to have no good or bad endings, each classification should be interesting in its own way. 

The outcome of each classification should be tailored to what that player expressed. 

No hidden behavioral classification should grant a mechanically superior encounter-completion reward.

## Full-Game Integration Constraints

- Pedro's encounter remains resolvable with baseline player abilities.
- Pedro's encounter role-specific outcome should not gate the other encounters.
- Shared progression goes through the existing encounter-progress contract.

- Pedro's encounter specific behavioral state remains encounter-local.
- Pedro's encounter should preserve the shared player controls and interaction language.
- Pedro's encounter should not require changes to the other encounter maps or the hub as part of normal encounter development.

## Combat Rules

The encounter has no conventional combat objective.

The player's normal attack remains available because using violence is itself a meaningful behavioral choice.

Having the attack enabled does not mean all the options normally available by attacking will be there, NPCs (including the observer) will not be conventionally killable.

### Final Reveal

The final classification should demonstrate that the observer remembers specific things the player did.

Whenever practical, the reveal should reference a small number of salient observations before presenting the final role.

The player does not need to see the internal scores or classification logic.

The goal is to make them understand that their actual behavior was noticed.

## Current Prototype Scope

in scope currently: 

- Structured evidence types;
- Observation history;
- Behavioral dimensions;
- Direct-observation provenance;
- Authored interaction evidence;
- Repeated-interaction evidence;
- Core/bonus opportunity tracking;
- Greybox prototype opportunities;
- Debug visibility.

out of scope currently:

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

Basically everything that is not on the scope of the prototype:

Final strange names for the three roles;
Exact point values;
Exact weighting formula;
Tie-breaking policy beyond "Pedro always chooses";
Exact number of observations required;
Exact repetition cap;
Final NPC identities;
Exact NPC quest;
Final item contexts;
Final puzzle;
Final map layout;
Exact number of rooms;
Final visual palette;
Observer model/design;
Exact dialogue;
Observer animations;
Perception radius/FOV;
Watch-point selection algorithm;
Whether suspicion/adaptation ships;
Whether gossip ships;
Exact outcome presentations;
Shared progression reward;
Exact HUD treatment;
Music/audio;
Exact encounter length beyond the rough short-prototype target.