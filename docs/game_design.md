# Game Design

## Core Idea

### TL;DR

First person dungeon crawler, with simple real time combat, sandbox style world design, with non-linear exploration, including 3 main encounters that can be done in any order, having each encounter add a unique spin to the game. 

## Inspirations 

- Darkest Dungeon:
Dungeon crawler, explorating while managing resources, unique skills per class, strategy to each encounter. 

- D&D:
Combat and damage system (not the randomized dice damage, count this as the AVG), leveling up in a non-numerical way (skills and special abilities), same a DD in some things like classes, resources, strategy.

- Undertale/Deltarune:
Special unique encounters that add a something unique to the game (bosses in Undertale/Deltarune are a very good example), chracterizations and personality of encounters for the main encounters.

- Persona/Hades:
NPC presentations and interactions involving characters reacting and changing depending on choices, art style for character Character/Dialogue Portrait.

- ENA: Dream BBQ:
Art style with the Low poly + cartoon vibe, the quirky Dialogue with chracters, and the mixed 2d 3d assets.

- Barony:
also a dungeon crawler, also low res/low poly, real time combat, "traps everywhere, nothing is safe" vibe is something we want.

- Dark souls:
Simply hard not to mention, gameplay wise this would be the target (but first person).

- Commedia dell'arte:
old italian style theatre, enviromental desing inspiration (so we won't so another medival looking dungeon crawler), a part of the game can have this look, circus and theare go into this part of the enviromental desing i belive. 

- ULTRAKILL:
fast paced first person (we are not doing a shooter, but looking for something like ULTRAKILL in terms of gameplay), lack of area cohesion (something we want to make the game feel weird and unseteling with the enviromental desing  always changing).

- PS2 LSD: Dream Emulator:
Low poly, weird, full of confusing encounters, more of a experience than a game (we are going for a little bit of an experience, and a little bit of gameplay, so different but some similarities) and very non linear.

## Design/arch choices 

- Perspective

First-person

- Genre

Dungeon crawler

- Combat

Real-time

- Structure

Sandbox-style exploration

- NPCs

Can interact with and/or battle them

- AI

Several encounters with deliberately different AI

- Encounter order

non-linear

- World

hub (demons sould esch "can go anywhere" hub) or connected sandbox

- Collectables

Italian food 

- Player baseline

attack + defend

- UI baseline

Health + class icon. No stamina (game will probably not have stamina)

- Enemy baseline

Simple enemy that attacks, no contact damage

- Visual baseline

Simple/low-poly (maybe do an interesting clash of high end lighing with low poly models)

- end condition

Find/resolve all encounters

- dimension

simple low poly 3d 

## Some longer things 

- Attack, Defense, Damage :

Attack should be something simple and easy to learn for every class (we can add complexity as a reward for an encounter), something like swinging a sword or shooting an arrow, we add a recovery time and it should be enought. Defense should probably work the same, you defend (hold to block, we make the character a little slower when blocking darks souls style) that makes you take reduced damage (75% phys and 50% elemental sounds good to me as a baseline) and then you have a recovery time (cooldown) and then you can defend again. later we can make classes that have better shields or better wepons but we need to balance tradeoffs, to start lets keep it simple.

The player and the Enemies should probably have acess to different types of damage, we can just make it two 'phys' 'elemental' and elemental includes everything from electro, fire, acid, ice, etc. just so we can make attacks from enemies that are not as blockable, and making enemies with weaknesses gives an extra opnion for the strategy part of it. for now we should probably stick to one elemental defense and one physical defense, later we can look into making something like Elemental.Fire Elemental.Ice Elemental.Electric Elemental.Acid

- Strategy:

Probably should try to integrate some kind of "you are weak the world is strong" vibe into the game, so it feels more like a strategy game where you are trying to trick and win over the enemies by being smart, combat should be possible do just "go an kill everything" but that should not be the main focus here, skyrim is something we want to be different from, not similar. 

- Collectables:
we could make these something interesting later (like a reward if you get all of them) but to start let's just make it a fun little "you found a funny thing" and it's a piece of salami.

- Hub (World integration: TBD before encounter integration milestone.):

a hub seems like the ideal choice so the player could just simply go to a place and do that area + encounter, but we can make it so the map just has paths that go to each of the areas and encounters, but a hub seems less frustrating (less of the vibe of "i have been looking for the last encounter for so long in this area, where is it?").

- Progression:

Probably something like a class specific new skill for each enconter you beat/find, something that adds complexity but also makes the game more interesting, charged attack, speels, dash, double jump, minion, all of those things. those abilities should not be necessary for the player to get to an encounter, we are not doing a metroid/castlevania type game, the abilities should be only to make the gameplay more interesting, not unclock encounters.

Every main encounter must be reachable and resolvable using only the baseline player abilities. Rewards from completed encounters may make other encounters easier or provide alternative approaches, but cannot be mandatory.

- Dialogue:

this will be area and encounter specific, but we are mostly going in the direction of making an interesting encounter that has a continous dialog during or as pauses to the encounter, undertale style.

- Combat:

combat will mainly be prevelant and happening in the common areas we make (with more basic enemies, serving as simple filler) and on the interesting encounters that should have some kind of unique mechanic or situation to it. One thing is that the genral gameplay will try to be responsive, low recovery, and quick, but each dev is free to change their encounter to make it more strategic and slow, or more chaotic, it will be a matter of balancing, a deliberate choices for each dev when making their encounter.

- Passive regeneration:

This is a complicated one, but the general idea we had was to have the player always have a decent passive regen so we don't have to plan and focus to much on giving the player health, but we wanted to punish the player if they abused it, so having some kind of timer that checks how long the player stayed in a room and stayed out of combat and once it reaches a certain number of seconds we would have a dedicted AI to decided the best way to attack or punish the player in some way (adding to the AI part of the game).

the idea is more "room pressure" then "Anti-camping" it's not about moving, it's about staying in the same room and staying out of combat for too long, just trying to stop the abuse of the regen, and make the player move foward, and for that reason i also think it is best to keep the regen going during combat, maybe a 2-5 seconds stop if you get hit as a punish but not too long, we want the player to move on, not hide away. 

One addition, this is supposed to be here as an idea to be used in some situations by any dev that wants to use it. it is not something to be running on every room at all times.

- Encounters:

The main idea here is to allow each person the decide how they will handle the encounter, the idea of "beating" an encounter is ambiguous on porpuse, so you might be able to talk it out with the npc that started the encounter, or circunvent in some other pacifist way, or maybe it is an encounter that you can only win by fighting. 

# Encounters

## Add info about your encounter here

- Encounter 1 (Misha) Medieval / Trap Area

TEMPORARY (Misha needs to go over this and refine it, this is what i understood from our conversations, i'm just leaving the information here to make everything easier)

From my understanding Misha is making a medieval focused idea, a dark and quiet area, having enemies appear and attack you, an enemy that keeps appearing and luring you into traps, with progress focused on finding out where the traps are and learning how to navigate around them. this is supposed to be a hard dark souls style encounter.

AI idea: stalker/lurer plus ambush enemies.

Unique mechanic: Map, traps and enemies and a kind of interesting puzzle.

Player problem being tested: Can the player stop reacting impulsively, learn how the hostile environment works, and navigate it deliberately?

- Encounter 2 (Caoimhe) Circus / Theatre

TEMPORARY (Caoimhe needs to go over this and refine it, this is what i understood from our conversations, i'm just leaving the information here to make everything easier)

From my understanding Caoimhe is making a big boss fight, with a narrative build up, and a more scenic vibe than a complicated combat one. this will be focused on the circus, italian, Russian/circus vibe, having very silly and odd looking aesthetic but still having one concise theme of being old style theater, where you will have a long boss fight with two clowns.

AI idea: Two highly characterized boss actors, and they interact with eachother and that changes with the fight.

Unique mechanic: Everything works as a circus/theatre with a progressing story/environment.

Player problem being tested: Can the player read two interacting enemies and adapt as the fight/performance changes?

- Encounter 3 (Pedro)

I am making a 'No Combat' encounter, having a observer character that follows you and watches everything you do for that section (interacting with objects, NPCs, environment, movement, etc) and analyzes what you do and decides on a certain "vibe" for you and presenting to you something unique for your "vibe".

AI idea: The observer watches player behaviour, builds an interpretation from it, and presents something unique to the player based on that interpretation.

Unique mechanic: No combat, interactive experience, 'someone is watching you' thing.

Player problem being tested: What does the player naturally do when the game stops explicitly telling them what matters?

## First deliverable 

- Class:


One crusarder/knight class, basic sword attack and defense 

- UI


basically what we need at the end of the game, just health and a class icon

- enemy:


just a slime cube, no contact damage, it sees you, it decides to attack, it does damage and knockback.

- Area:


Greybox dungeon, just 4 walls, basic textures and lights.

- Movement:


we start just with walk and jump for movent, add sprint and crouch as soon as possible.

- Death:


restart from the start of the room, keep it simple for now and later we can move this around and change it, and reset means we reset player health, and location. slime health/state (dead or alive) and location.


FIRST PLAYABLE IS DONE WHEN:

**Status: Complete.**

[x] Player spawns in first-person.

[x] Player can walk/look/jump.

[x] Player has health.

[x] Health is represented by the HUD.

[x] Player can attack.

[x] Attack can damage the slime.

[x] Player can hold block.

[x] Blocking reduces incoming physical damage.

[x] Slime detects/acquires player.

[x] Slime intentionally starts an attack.

[x] Merely touching the slime causes no damage.

[x] Slime attack damages and knocks back player.

[x] Slime can die.

[x] Player can die/reset.

[x] Everything works in the greybox room in PIE.

[x] Player attack has a defined recovery period.

[x] Blocking slows the player and reduces physical damage.

[x] HUD displays the knight/crusader class icon.
