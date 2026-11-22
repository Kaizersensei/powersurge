# Physics objects laboratory

Signed: Mech. Experimental shared-engine scenario; not a finished general-purpose physics authoring system.

Select **F12 → Test → Laboratories → Physics objects** in the native editor, or run powersurge.exe --lab physics from the portable package. The editor switches to a disposable lab session in the same engine instance, with save/discard/cancel handling for unsaved work. Saved workspaces are retained; see LAB_RUNS.md. CLI launches also support --keep-lab. The scenario is levels/physics-lab.lev, with its own terrain document.

In the new editor choose **Objects** and search for BSL Physics: Ball, Stone, Cupboard, Boulder and Star are ordinary entity palette entries. They share one definition-backed native factory and use placeholder graphics. F12 resumes play. Actor hurtboxes push objects using a finite-mass driven proxy. Active BOR attack hitboxes apply a stronger point impulse once per attack activation and prop. The attack query uses a rounded capsule derived from the authored frame, while box props have rounded corners; off-center impulses can spin them. Stone and boulder have increased densities. Object support/blocking of actors, jointed prefabs, and multiplayer ownership remain incomplete.

The physics world is rebuilt after editing the level so its contour snapshot follows the committed terrain. The current implementation supports the existing cell-terrain routes and only the currently controlled actor's proxy. Terrain updates during active gameplay, persistence/reconstruction, scene transitions, actor physics-mode switching, contact-to-damage policy, physical materials, buoyancy and flammability remain open. Definitions are a compiled experimental table derived from profiles.experimental.json, not an editable registry or stable schema. Numeric values and the 8 m discontinuity safeguard are provisional and require playtesting.

Box2D 3.2.0 is pinned by bsl/third_party/box2d.lock.json; its MIT notice is in labs/content/licenses. The shared executable uses the same code for all scenarios. No local Python/CMD build automation is added to the repository.

Verification: a disposable native editor fixture completed 36 checks, including palette discovery, a new ball placement, save, resume and reconstruction of the five existing objects and new placement. The existing connected terrain journey completed 32 checks on the updated executable. A bounded 180-frame physics-lab startup passed. These checks do not establish feel, object-by-object behavior, long-session performance, or human creator acceptance.

## Controlled-actor proxy lifecycle

The experimental physics world keeps at most one controlled-actor hurtbox proxy. If the actor or its valid hurtbox disappears, the proxy is disabled before the next solver step. It retains its world-lifetime identity and is repositioned while disabled before being re-enabled for a valid actor. This prevents a stale proxy from continuing to obstruct objects; it does not add actor backreaction, support, attack forces or multi-actor simulation. Signed: Mech.

## Attack force and authoring boundary

The current test impulse is provisional: at most 300 N·s and capped to a 12 m/s change for very light props, with a small upward component. It is applied at the closest contact point, not at the body center. Impact location, shape and collisions affect the resulting trajectory; no random force is introduced. Repeated callbacks in one active BOR hitbox epoch do not repeatedly impulse the same prop. The driven actor proxy has finite mass so a stone or boulder resists the same push more than the ball. This is one-way influence on props; it is not player blocking, carrying or reciprocal actor physics.

Once character attack-power stats and the shared combat formula are ready, derive the force from the attacker's effective attack power and relevant authored move properties through a shared combat-to-physics formula. The editor should expose the associated move/formula references and meaningful per-attack modifiers once their authoritative schema exists; props should expose mass, shape, material and physical response through stable definitions. Do not encode character-specific power inside a prop or expose the current constants as a final authoring contract. The current prototype neither snapshots final stats nor establishes a definitive conversion scale. Signed: Mech.
