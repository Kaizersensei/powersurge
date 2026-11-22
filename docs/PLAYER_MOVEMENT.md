# Player movement baseline

The shared PowerSurge preview uses `labs/content/physics/michio_sword.physics` for Michio's authored movement tuning. The portable assembly copies it to `physics/michio_sword.physics`; a missing player profile produces a runtime warning and falls back to the untuned controller. An invalid profile prevents controller activation and is reported. Other actors can intentionally omit profiles.

The profile was restored byte-for-byte from the pre-Box2D BORSurgeLab packaged build after it was found missing from the PowerSurge content migration. Its key values include speed 330, jump 500, dash 400 and air brake 120. The pre-Box2D source archive's physics actor, traversal and contact implementations match their current counterparts. The current bridge has additional editor, pause and Box2D integration, so this comparison alone does not establish equivalent behavior in every scenario.

The combined binary loaded the profile in the physics lab and the connected terrain journey; the latter reported 32 passing checks. These are startup and continuity checks, not a hands-on assessment of movement feel or a comprehensive test of every attack and traversal state. Review walking, running, dash, jumps, slopes, roll, ledges and combat in both the normal course and physics lab, including near physics objects. Report deviations with the level and action sequence.

For future actor authoring, expose the profile as actor-owned editable data with validation, units and a clear missing/invalid diagnostic. Do not silently overwrite authored tuning or assume every actor uses Michio's profile. The profile and engine controller remain authoritative until a validated editor/provider contract exists.

Signed: Mech
