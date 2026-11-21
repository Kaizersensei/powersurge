<img width="128" height="128" alt="PowerSurge logo" src="https://github.com/user-attachments/assets/90daf393-7ddb-48b7-9a94-bf639e4e528f" /> 
<img width="600" height="128" alt="PowerSurge logo Label hollow" src="https://github.com/user-attachments/assets/cc270e9a-4540-4dd9-a92d-a83d38252442" />

# Key Features and Design Philosophy

PowerSurge aims to bridge the gap between the immediacy of classic arcade games and the systemic depth of modern action RPGs and Metroidvanias. Built upon Open Surge and incorporating technologies from OpenBOR, EasyRPG, Box2D and SoLoud, it seeks to provide a unified development environment where responsive action gameplay, complex world interactions and RPG progression can coexist without requiring developers to assemble an entirely new engine for each project.

Rather than pursuing complexity for its own sake, PowerSurge emphasizes **modularity, reusable systems, data-driven design and accessible authoring tools**. Its development is guided by the practical requirements of the Densetsu games, while keeping the underlying technology flexible enough to support other projects and genres.

## 1. Redesigned Level Editor and Terrain Creation

<img width="1920" height="1054" alt="physics-1-1920-editor" src="https://github.com/user-attachments/assets/589eba3a-9b4d-491a-bb4b-f3928fabf63b" />


One of PowerSurge's most substantial improvements is the ongoing redesign of Open Surge's level editor. The goal is to transform it from a traditional object-placement tool into a comprehensive, in-world environment authoring system.

The redesigned editor operates directly inside the loaded game world, allowing creators to switch between playing and editing without moving to a separate level-design application.

Its expanded toolset includes:

- **Direct in-world editing:** Place, move, duplicate, group and inspect objects while viewing the actual game environment.
- **Advanced terrain painting:** Create continuous terrain using freehand strokes, straight or curved lines, square and round brushes, and adjustable construction presets.
- **Material-driven surfaces:** Assign textures, colors and material properties to terrain regions, with adjustable blending between adjacent materials.
- **Reusable compositions:** Create linked groups and independent placements, with inherited defaults and per-instance overrides.
- **Separate collision and decoration:** Edit visual elements without unintentionally modifying the underlying terrain collision.
- **Collision routes:** Organize overlapping collision geometry into independently selectable routes.
- **Non-destructive editing:** Undo and redo changes, preserve stable object identities, and save authored data independently of temporary gameplay state.
- **Integrated playtesting:** Experiment with edited levels in isolated test sessions without altering normal game progress.

The underlying terrain system is also being redesigned around continuous cell coverage, allowing terrain geometry to be derived from authored shapes rather than relying exclusively on individually placed tiles and collision bricks.

These capabilities have working implementations in focused development builds. Advanced geometry generation, shader integration, physics synchronization and broader material behavior remain under development.

**The long-term objective is to make level creation feel closer to drawing and sculpting a world than assembling it from disconnected building blocks.**

## 2. Arcade Precision Meets Brawler Combat

<img width="576" height="101" alt="image" src="https://github.com/user-attachments/assets/787d49c6-b2bd-45da-b01e-4cbf6fb5d30a" />

PowerSurge combines Open Surge's responsive platform movement with OpenBOR's extensive fighting and animation systems.

This creates a foundation for combat that goes beyond the conventional jump-and-attack mechanics of many platform games.

Its combat framework supports directional attacks, armed and unarmed fighting, chained combinations, grappling, throws, blocking, parrying, evasive maneuvers and context-sensitive hit reactions. Character actions are driven by animation timing and explicit state transitions, allowing attacks to have meaningful startup, active and recovery phases.

Traversal and combat share the same actor framework. Characters can perform platforming actions while retaining their combat capabilities, opening the door to encounters involving vertical terrain, moving platforms, environmental obstacles and multiple opponents.

The system is also designed to support configurable characters with different action sets, movement properties and fighting styles, rather than enforcing a single universal controller.

**The objective is to combine the fluidity of a platformer, the mechanical precision of a fighting game and the crowd-oriented combat of a beat 'em up.**

## 3. Extended Physics and Interactive Environments

The original Open Surge physics system excels at fast platform movement, but does not provide the general-purpose rigid-body simulation required for more physically interactive environments.

PowerSurge addresses this limitation through the integration of Box2D.

The intended hybrid architecture preserves Surge's established movement behavior while introducing rigid bodies, physical forces, collision contacts, joints and constrained assemblies for objects that require more complex simulation.

This opens possibilities for movable obstacles, rolling objects, swinging platforms, hanging bridges, breakable constructions and physically driven puzzles.

A central design principle is that each object must have one authoritative physics controller. PowerSurge is therefore developing explicit boundaries between traditional character movement, OpenBOR action states and Box2D simulation.

Experimental implementations already demonstrate terrain-derived physics contours, rigid-body contacts, joints and actor-driven object interaction. General-purpose physics object authoring, complete actor physics-mode switching and production-level integration remain ongoing work.

**The goal is to make physics a natural extension of level design and gameplay, rather than a collection of special-case scripted effects.**

## 4. Integrated RPG Systems and Database-Driven Development

<img width="1661" height="1058" alt="first-practice-awards" src="https://github.com/user-attachments/assets/f555b928-1bea-49d8-a620-11fe78a5f82f" />


PowerSurge extends traditional platforming with a configurable RPG foundation, drawing on EasyRPG's database-oriented approach to game development.

Its systems cover character attributes, experience and skill growth, equipment, inventories, consumables, enemy loot, quests, dialogue, event commands, shops, currencies, checkpoints and persistent progress.

An important design decision is the separation of reusable definitions from live instances. A character, item or ability can be defined once and instantiated with its own state, ownership and progression. This reduces duplicated data and makes the same underlying systems usable across multiple game modes.

The external authoring environment is based on a modified EasyRPG Editor. It retains familiar RPG Maker-style forms and database workflows while gradually connecting them to PowerSurge's own gameplay services.

The objective is to let creators configure complex behavior through accessible interfaces rather than hardcoding every item, character or mechanic.

PowerSurge also uses a selective persistence model: instead of indiscriminately saving the entire running simulation, it records the progress needed to reconstruct a consistent gameplay state.

Several RPG subsystems have been demonstrated in connected native tests, although broader editor coverage and a complete, extended RPG gameplay journey remain development targets.

**This approach makes RPG mechanics first-class engine systems rather than optional additions assembled separately for each game.**

## 5. Spatial Audio, Acoustic Propagation and Stealth

<img width="958" height="602" alt="panel-small" src="https://github.com/user-attachments/assets/573db409-f125-4e53-aaa7-103b32e4c8ba" />


PowerSurge's audio architecture aims to make sound a meaningful part of the simulated game world, not merely a presentation effect.

SoLoud provides an expanded foundation for audio playback, mixing and real-time digital signal processing. On top of this, PowerSurge introduces a spatial propagation model that accounts for the geometry and acoustic properties of the environment.

Unlike simple distance-based audio attenuation, the experimental propagation system evaluates how sound travels through open spaces, around obstacles and across materials with different transmission characteristics.

Its principal capabilities include:

- Spatial sound positioning, balance and frequency filtering.
- Material- and thickness-dependent transmission loss.
- Acoustic paths accounting for obstacles and corners.
- Separate low-, mid- and high-frequency strengths.
- Dynamic recalculation when geometry or blockers change.
- Independent handling of audible sound and gameplay hearing events.
- Configurable hearing thresholds and sound categories for NPCs.

This separation between *what the player hears* and *what an NPC can detect* is particularly important.

A sound can exist as a gameplay event even when it is outside the player's audible range or camera view. Conversely, a cosmetic sound does not automatically reveal the player's position to enemies.

A working demonstration already includes an NPC responding to sound, investigating its perceived source, stopping at obstacles or unsafe terrain, searching briefly and returning to its previous behavior.

This establishes the groundwork for more elaborate stealth mechanics, including noise-sensitive enemies, distractions, environmental sound masking and differentiated hearing capabilities. These broader stealth systems are future development goals, not completed features.

**PowerSurge treats sound as information traveling through the world, allowing hearing to become a gameplay mechanic alongside vision, collision and combat.**

## 6. Dynamic Worlds, Time and Environmental Systems

PowerSurge is also developing an environment framework capable of supporting worlds that respond to changing conditions.

Its calendar system tracks in-game time, dates and seasonal progression. Regional weather combines calendar information, hemisphere, local climate definitions and environmental overrides to produce conditions such as rain, snow, wind and ash.

Weather presentation is supported by a reusable particle-effect system designed around efficient native rendering rather than treating every particle as an independent gameplay actor.

The broader material architecture is intended to connect terrain definitions with visual appearance, collision properties, acoustic transmission and eventually more advanced environmental reactions.

This creates opportunities for environments that change throughout gameplay: seasonal landscapes, dynamic weather, different soundscapes, shifting traversal conditions and encounters influenced by the surrounding world.

Calendar, weather and particle functionality already exist in scoped implementations. More advanced physical material behavior, environmental reactions and fully integrated world simulation remain areas of active development.

## 7. A Unified Metroidvania Framework

The larger ambition behind PowerSurge is to make these systems work as parts of one coherent gameplay environment.

Metroidvanias depend on the relationship between movement, world design, player abilities and progression. New skills create new traversal opportunities, previously inaccessible locations become reachable, and returning to familiar places reveals possibilities that were not available before.

PowerSurge aims to support that structure through its combination of:

- **Flexible traversal and combat**, where a character's abilities influence both exploration and encounters.
- **Interconnected levels**, supported by registered destinations, teleporters, checkpoints and persistent progression.
- **Reusable actors and abilities**, allowing character capabilities to evolve without replacing the underlying gameplay framework.
- **Data-driven progression**, connecting gameplay achievements to new abilities, equipment and interactions.
- **Interactive environments**, where terrain, objects, physics and sound can contribute to exploration and puzzles.
- **Unified authoring tools**, keeping level composition and gameplay definitions accessible to creators.

A particularly important difference is the relationship between these systems. An ability need not exist exclusively as a combat attack or a traversal move. The long-term architecture is intended to allow the same capabilities to influence world interactions, exploration, puzzles and combat through shared definitions and consistent rules.

This philosophy extends to game modes. By separating core engine behavior from game-specific rules, PowerSurge seeks to accommodate traditional platformers, arcade brawlers, action RPGs and exploration-heavy Metroidvanias without maintaining entirely different technical foundations.

## Development Philosophy and Long-Term Vision

PowerSurge is being developed incrementally through isolated prototypes, integration tests and progressively more complete gameplay scenarios. Not every system described here is available in a single production-ready build; several features remain experimental, partially integrated or planned.

Its central ambition, however, is consistent: **to create a unified, extensible 2D game engine that gives developers the tools to build mechanically rich, interconnected worlds without sacrificing the responsiveness and accessibility of classic arcade games.**

For Densetsu, PowerSurge serves as both a development platform and an opportunity to consolidate years of gameplay experimentation into reusable technology. For other creators, the long-term goal is to provide an open-source foundation that encourages experimentation, customization and the creation of games that combine genres rather than remaining confined by them.
