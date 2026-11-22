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

Public source/content provenance is recorded in labs/content/PROVENANCE.md; detailed migration evidence stays local and private. Existing native service assertions are reused; this first automatic scenario replaces one Python orchestration path. Other legacy scenarios and authoring/compiler/package checks have not yet all migrated. Their old launchers remain only in the historical workspace. No Python test launchers are added here. The editor-contract and terrain-journey cases reset their state by owned session generation. The native switch check exercises frontend → terrain → physics → editor checks → journey → repeated journey in one process. Other historical cases still need explicit lifecycle review before they can be registered here.

Release preparation follows verification: package the Windows runtime, matching source and notices, identify it as a pre-release, and document its tested scope. No remote release is published automatically by this build target.

Palette UX: clicking an entity or decoration row, including its thumbnail, prepares placement and highlights the armed choice. Dragging and releasing over the level commits one placement through existing undoable actions. Releasing over UI or cancelling does not place. Material clicks select the active shape material; material drags apply at their destination. World input remains captured during the UI gesture, so it cannot also paint/place underneath.

The presentation-menu-check case navigates the real menu in both directions, waits through normal garbage collection, verifies five visible artwork actors remain, and activates the native Level Select. It uses private progress and no injected desktop input. It is a focused presentation regression check, not full keyboard/gamepad or character-selector acceptance. Signed: President.

For release candidates use the explicit powersurge_public target after public/accepted.json is updated from passing native checks. It writes a separate out/public preview and source archive; private records and experiment sources are excluded. See PUBLICATION.md and EDITOR_VIEW.md. Signed: President.

The display-check case moves only its own window across every attached monitor. It checks borderless fullscreen bounds and windowed restoration from normal, boundary-straddling and maximized windows, plus negative/off-desktop monitor-selection cases. It restores its original window state on success or failure. Run it from Test > Laboratories > Fullscreen monitor checks or the native CLI; raw results remain private. F11 uses the monitor containing the greatest portion of the current window and preserves manual window size/position when returning; monitor coordinates may be negative. Explicit resolution changes still take effect. Signed: President.
