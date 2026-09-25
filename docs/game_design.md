### Core idea

# TLDR

First person dungeon crawler, with simple real time combat, sandbox style world design, with non-linear exploration, including 3 main encounters that can be done in any order, having each encounter add a unique spin to the game. 

# Inspirations 

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

# Design/arch choices 

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
Find/Win all encounters

- dimension
simple low poly 3d 

# Some longer things 

- Attack and Defense 
Attack should be something simple and easy to learn for every class (we can add complexity as a reward for an encounter), something like swinging a sword or shooting an arrow, we add a recovery time and it should be enought. Defense should probably work the same, you defend (hold to block, we make the character a little slower when blocking darks souls style) that makes you take reduced damage (75% phys and 50% elemental sounds good to me as a baseline) and then you have a recovery time (cooldown) and then you can defend again. later we can make classes that have better shields or better wepons but we need to balance tradeoffs, to start lets keep it simple.

- Strategy
Probably should try to integrate some kind of "you are weak the world is strong" vibe into the game, so it feels more like a strategy game where you are trying to trick and win over the enemies by being smart, combat should be possible do just "go an kill everything" but that should not be the main focus here, skyrim is something we want to be different from, not similar. 

- Collectables:
we could make these something interesting later (like a reward if you get all of them) but to start let's just make it a fun little "you found a funny thing" and it's a piece of salami.

- Hub
a hub seems like the ideal choice so the player could just simply go to a place and do that area + encounter, but we can make it so the map just has paths that go to each of the areas and encounters, but a hub seems less frustrating (less of the vibe of "i have been looking for the last encounter for so long in this area, where is it?").

- Progression
Probably something like a class specific new skill for each enconter you beat/find, something that adds complexity but also makes the game more interesting, charged attack, speels, dash, double jump, minion, all of those things.

- Dialogue
this will be area and encounter specific, but we are mostly going in the direction of making an interesting encounter that has a continous dialog during or as pauses to the encounter, undertale style.

- Combat
combat will mainly be prevelant and happening in the common areas we make (with more basic enemies, serving as simple filler) and on the interesting encounters that should have some kind of unique mechanic or situation to it.

- Passive regeneration
This is a complicated one, but the general idea we had was to have the player always have a decent passive regen so we don't have to plan and focus to much on giving the player health, but we wanted to punish the player if they abused it, so having some kind of timer that checks how long the player stayed in a room and once it reaches a certain number of seconds we would have a dedicted AI to decided the best way to attack or punish the player in some way (adding to the AI part of the game).

# First deliverable 

- Class 
One crusarder/knight class, basic sword attack and defense 

- UI
basically what we need at the end of the game, just health and a class icon

- enemy 
just a slime cube, no contact damage, it sees you, it decides to attack, it does damage and knockback.

- Area
Greybox dungeon, just 4 walls, basic textures and lights.