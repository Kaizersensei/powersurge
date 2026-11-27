# Native laboratories â€” first migration gate

Signed: President. Development candidate, not an accepted stable release.

Build PowerSurge normally with CMake and the documented upstream dependencies, a static SurgeScript library and modular Allegro. Select `BSL_AUDIO_BACKEND=SOLOUD`, `SURGESCRIPT_STATIC=ON`, and `GAME_RUNINPLACE=ON`. The accepted SoLoud, DSP and Dear ImGui sources are included in bsl/. No Python, copied developer workspace or test framework is required by the resulting executable. On Windows, build target `powersurge_portable` after compiling. Its output is `out/portable` for the documented `out/native` build directory. DLL dependency discovery uses the compiler toolchain; it is a build operation, never a lab launch operation.

Double-clicking the preview executable follows the restored C2 intro, title and main menu. Level Select is a menu option. The package declares startup=frontend in powersurge-preview.ini. Explicit external-project arguments retain their own startup.

From the portable folder:

```
powersurge.exe --lab-list
powersurge.exe --lab terrain
powersurge.exe --lab physics
powersurge.exe --lab terrain-journey
powersurge.exe --lab editor-contract
powersurge.exe --lab presentation-menu-check
powersurge.exe --lab display-check
powersurge.exe --lab-storage-check
powersurge.exe --lab-switch-check
```

The interactive lab loads the selected original actor/terrain fixtures and retains F12 editing. The automatic journey uses the same live services to travel through three registered levels and check session/actor/terrain continuity. It ignores desktop controls and exits with a failure code if checks fail or time out. Closing its window early returns cancellation. `--lab-frames N` is an optional bounded interactive smoke run; this counts simulation updates, not seconds. Labs own their launch configuration; unrelated engine options are refused before creating storage. The editor Test → Laboratories menu warns that the current level and gameplay session will be replaced in this same instance. Unsaved edits offer Save and start / Discard and start / Cancel. The existing engine teardown/reinitialization path performs the transition in the same process; no second game process is launched. Automatic cases started from this menu leave their result and current level open on completion. Explicit command-line automatic cases still exit with their result code.

Each new session copies only the fixture pack into a unique temporary directory under the operating-system temp folder, PowerSurge-labs-v1. It never copies sources, compilers, native build products or an entire project. Fixture editing, progress, session data, logs and editor preferences remain inside that run. A live file lease prevents another process from cleaning an active run. Normal shutdown removes it; a later engine startup reaps positively recognized abandoned runs. Unknown directories, links/reparse points, other applications and retained runs are not recursively removed. No old BORSurgeLab AppData cleanup is performed by this engine.

Explicitly saving changed level/terrain files retains that private workspace, even if another part of Save All fails, so changing labs cannot discard successfully saved edits. The retained path appears in diagnostics and the session report. Unsaved discarded test work is removed. Use `--keep-lab` when deliberately retaining an editable run for diagnosis. The console and compact report identify its location. Retained runs are user-requested data and need explicit removal when no longer wanted. The default reports folder beside the executable retains only the latest ten owned reports, each with at most128 KiB of engine log; if the package is read-only, it falls back to the recognized temporary root. Reports do not include the full fixture pack or progress file. Canonical lab content is in the repository's labs/content, not in an output folder or temporary run.

## Migration boundaries

This is a reusable engine with selected original demonstration assets, not the Densetsu story package. Three terrain textures are selected for these fixtures; the remainder awaits manual curation. Inherited upstream sample-game directories have been removed; the preview uses the migrated demonstration asset set. The modified RPG editor remains in its previous workspace and is not silently imported. The current build includes an experimental Box2D object capability and a selectable physics test level; its editable definition provider and broader lifecycle remain work in progress. See PHYSICS_LAB.md.

Public source/content provenance is recorded in labs/content/PROVENANCE.md; detailed migration evidence stays local and private. Existing native service assertions are reused; this first automatic scenario replaces one Python orchestration path. Other legacy scenarios and authoring/compiler/package checks have not yet all migrated. Their old launchers remain only in the historical workspace. No Python test launchers are added here. The editor-contract and terrain-journey cases reset their state by owned session generation. The native switch check exercises frontend → UI pilot checks → terrain → physics → editor checks → journey → repeated journey in one process. Other historical cases still need explicit lifecycle review before they can be registered here.

Release preparation follows verification: package the Windows runtime, matching source and notices, identify it as a pre-release, and document its tested scope. No remote release is published automatically by this build target.

Palette UX: clicking an entity or decoration row, including its thumbnail, prepares placement and highlights the armed choice. Dragging and releasing over the level commits one placement through existing undoable actions. Releasing over UI or cancelling does not place. Material clicks select the active shape material; material drags apply at their destination. World input remains captured during the UI gesture, so it cannot also paint/place underneath.

The presentation-menu-check case navigates the real menu in both directions, waits through normal garbage collection, verifies five visible artwork actors remain, and activates the native Level Select. It uses private progress and no injected desktop input. It is a focused presentation regression check, not full keyboard/gamepad or character-selector acceptance. Signed: President.

For release candidates use the explicit powersurge_public target after public/accepted.json is updated from passing native checks. It writes a separate out/public preview and source archive; private records and experiment sources are excluded. See PUBLICATION.md and EDITOR_VIEW.md. Signed: President.

The display-check case moves only its own window across every attached monitor. It checks borderless fullscreen bounds and windowed restoration from normal, boundary-straddling and maximized windows, plus negative/off-desktop monitor-selection cases. It restores its original window state on success or failure. Run it from Test > Laboratories > Fullscreen monitor checks or the native CLI; raw results remain private. F11 uses the monitor containing the greatest portion of the current window and preserves manual window size/position when returning; monitor coordinates may be negative. Explicit resolution changes still take effect. Signed: President.

Shared game/editor widget pilot and native UI/save checks: see [SHARED_UI.md](SHARED_UI.md). The same-instance switch check now also verifies pilot teardown before ordinary gameplay. Signed: President.

## Database documents and runtime

Use `--lab database` for an isolated authoring workspace, or F10 in a loaded gameplay level. `--lab database-check` checks shared documents, recovery, conflicts and a saved Items/Effects round trip into a new same-instance runtime snapshot. It consumes a real inventory item and verifies its authored recovery. All files and progress belong to the disposable run. See [Database workspace](DATABASE_WORKSPACE.md) for bound surfaces, shortcuts and retention. Signed: President.

The database round trip also checks dialogue order/help/outcomes, displays the authored message and information through game widgets, confirms a linked calendar event, and cancels a reopened conversation without dispatch. `--keep-lab` explicitly retains rendered evidence; default runs clean up normally. Signed: President.

The database scenario also covers authored switch/variable defaults, protected scope/ranges, per-player versus session ownership, native conditions, explicit saved-value precedence and a real event changing a default7 to runtime10 without modifying the source definition. Signed: President.

The database scenario includes common-event sequence editing: branch target/End rebasing, failed-edit preservation, typed/external-reference validation, wait deferral and cancellation with committed effects retained. Its saved event reaches runtime10 from default7, while source remains7. Tests use the existing runner; no automatic scheduler or extra state owner is added. Signed: President.

The database scenario also exercises Fields: fractional strength/direction validation, unchanged active instances, occupant loop/request cleanup, and a fresh snapshot producing the authored-240.5px/s² request for the runtime player. Actor movement coupling remains unbound; this check does not apply a second force authority. Signed: President.

Inspector navigation is covered by the native database case (version/context/status, dirty and malformed shared drafts, missing/unavailable targets, no definition writes) and editor-contract (place/bind an event anchor, invoke the real inspector command, verify origin context, return to editing, reject an unbound anchor). Staged tests record explicit authoring lineage; no external editor process or request files are needed. Signed: President.

`--lab test-session-check` runs the owned-test lifecycle in-engine: saved-source launch, parked lease, disposable progress, source save during the clone, failed preparation without teardown, fresh reload and Stop returning to the original root/cache with drafts retained. The source scene restarts; test progress is not imported. This is also available through Test → Laboratories. No external test process or framework is used. Signed: President.

The combined same-instance checkpoint includes the owned-test lifecycle between the editor case and the repeated terrain journeys: seven laboratory transitions plus the owned launch/reload/return restarts. It verifies that no owned session blocks later labs. The focused40-check fixture detects saved clone edits but restores its marker before replacement; source-save preservation checks storage identity and the absence/unchanged contents of the source progress file. Signed: President.

The owned lifecycle scenario also starts a registered sequence, saves level/weather metadata and a different first member in source, verifies the active sequence stays unchanged, reloads into the new native first scene, then stops back at the original source level. These checks use the actual quest loader and weather projection; they do not rebuild geometry. Signed: President.

The document scenario adds real filesystem creation tests: dirty/unsafe/duplicate/missing-dependency refusal; case-safe no-overwrite targets; shared-document/index-extension preservation; starter terrain; partial/prepared/committed/future/conflicting journals; and recoverable index bounds. The owned lifecycle scenario creates a new source level, proves it is absent from the active snapshot, opens it through a guarded source restart and verifies its runtime registry, cell-editor availability, shared document owner and unchanged progress storage. No external test framework or production fault switch is used. Signed: President.


## Persistent source projects

File > New project creates a durable source folder from the bundled starter content. File > Open project (or `--project <folder-or-manifest>`) opens it directly; Recent projects reopens a selected root. File > Save project copy copies the mounted source content into a NEW folder with a fresh project identity, excluding local progress, preferences and draft recovery. Destinations must have an existing parent, must not exist and must not be inside disposable lab storage or the current source/run. The source is never moved. Unknown/newer projects are refused without rewriting them.

A project contains `powersurge.project.json`, `content/` and local `.powersurge/` state. Source records/levels/assets live in content; local preferences, database recovery and source-session progress stay in .powersurge. An exclusive lease prevents two processes editing one project. Project manifests preserve unknown fields, check external edits, back up the previous manifest and replace atomically. Save project saves database drafts then level edits and publishes the saved entry setting last; commits are per file and failures can leave earlier files saved. Individual level/database save commands remain available. Source editing opens the authored entry rather than letting progress-boot choose a different level.

Tests still use same-process disposable content snapshots and separate progress. Stop reopens the source scene and retains its document owner; it does not import test progress or restore a full live-world snapshot. A laboratory can be saved as a project copy, but its retained directory is not itself a permanent project. Default presentation startup remains unchanged. Retained historical user roots are preserved; migration/import from an arbitrary old run remains an explicit later operation.

Generated-output storage prepares the companion-map workflow: versioned recipes reference immutable output revisions and the source fingerprint captured before computation. Rebuild preserves supplied images, earlier revisions and unknown recipe fields; invalid/stale writes are refused. Interrupted revisions have recognizable ownership and can be retried without deleting their files. This is the storage API, not a completed normal-map generator or material binding.

Native checks: `--project-storage-check` exercises filesystem/refusal/recovery/ownership cases using binary storage payloads, not generated-image correctness. `--lab project-session-check` exercises creation, source saving, level creation, copy/reopen and disposable test return in one process. These scenarios allocate only owned test fixtures and never open retained user projects. Project history is one bounded portable `.userdata/project-history.json` file; it and local project state must not be included in public packages.

Signed: President.


## Rendering record check

`--lab render-record-check`, also available in Test > Laboratories, checks typed rendering records and the real terrain color/tint consumer. It uses a fresh lab, edits the shared material document, confirms that the running snapshot stays unchanged, launches an isolated saved-content test, checks the actual decoded texel, returns and restores the original source through the same document service. It also checks invalid paths/references/parameters, incompatible native PNGs, undo/conflicts/unknown fields, structured navigation and malformed-form UI-stack recovery. Geometry and the old material file remain unchanged. Lighting rendering is explicitly reported false; this is not a normal-map/bevel visual acceptance case.

Signed: President.

## Surface lighting

`lighting-study` is the opt-in record-driven comparison. `lighting-check` checks actual GPU pixels, normal/UV conventions, state/lifetime and material-selector/hide-show input. See [the study contract](NORMAL_MAP_STUDY.md). These do not activate lighting on ordinary terrain.

`terrain-lighting` is the playable static terrain course; `terrain-lighting-check` verifies its real GPU chunks, cache refresh, fallback and collision isolation. See [Static terrain lighting](TERRAIN_LIGHTING.md).

`material-catalog-check` validates the creator texture import through the native decoder and exercises bounded residency, safe assignment and thumbnail browsing. See [Material library](MATERIAL_LIBRARY.md).

`--lab bevel` opens the contour-relief course. `--lab bevel-check` runs native contour, GPU, cache/history and material-picker interaction checks in an owned disposable workspace.

`--lab line-check` uses synthetic geometry beyond256 chunks and the same sampled input handler as real Line/Shift-click tools. It covers square/capsule strokes, all line modes, saved replay, UI-captured release, rejected-input recovery and undo/redo. No private user course is part of the fixture.


## Shared environment proof

Use **Test > Laboratories > Shared environment / clocks and weather**, or `--lab environment-check`. It runs in the current process through the normal disposable laboratory flow. It checks synthetic unequal-month calendars, fractional rates, rollover, celestial directions, atmosphere/local separation, external weather overrides and actual menu/dialogue pause policies. It reports its result in the existing laboratory log; it does not alter normal progress or enable new visuals.

The proof introduces no external test framework. Its celestial cycle is explicit test input, not a new registered gameplay definition or a lighting override. Signed: President.


## Registered sky course

Open **Test > Laboratories > Sky / calendar and clouds course**, or `--lab sky`. This new course opts into a registered sky profile and uses a transparent authored background. Existing courses retain their backgrounds. F12 edits the course; F10 > Skies edits its saved profile, and F10 > Levels > Sky / far background controls the level attachment. Start a fresh test to apply saved definitions.

`--lab sky-check` verifies profile validation, optional attachments, shared-document save/reopen, actual GPU gradients/clouds, calendar palettes, viewport-independent horizon displacement, cloud phase across wind changes, graphics-state restoration and transparent/opaque background layering. It captures the course, definition surface and four time-of-day samples in the disposable lab workspace when explicitly retained. No external test runner is required.
