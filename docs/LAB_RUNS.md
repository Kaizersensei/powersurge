# Native laboratories — first migration gate

Signed: President. Development candidate, not an accepted stable release.

Build PowerSurge normally with CMake and the documented upstream dependencies, a static SurgeScript library and modular Allegro. Select `BSL_AUDIO_BACKEND=SOLOUD`, `SURGESCRIPT_STATIC=ON`, and `GAME_RUNINPLACE=ON`. The accepted SoLoud, DSP and Dear ImGui sources are included in bsl/. No Python, copied developer workspace or test framework is required by the resulting executable. On Windows, build target `powersurge_portable` after compiling. Its output is `out/portable` for the documented `out/native` build directory. DLL dependency discovery uses the compiler toolchain; it is a build operation, never a lab launch operation.

From the portable folder:

```
powersurge.exe --lab-list
powersurge.exe --lab terrain
powersurge.exe --lab terrain-journey
powersurge.exe --lab-storage-check
```

The interactive lab loads the selected original actor/terrain fixtures and retains F12 editing. The automatic journey uses the same live services to travel through three registered levels and check session/actor/terrain continuity. It ignores desktop controls and exits with a failure code if checks fail or time out. Closing its window early returns cancellation. `--lab-frames N` is an optional bounded interactive smoke run; this counts simulation updates, not seconds. Labs own their launch configuration; unrelated engine options are refused before creating storage. The editor Test menu can launch either lab in a fresh instance without altering its current document or progress.

Each process copies only the small fixture pack into a unique temporary directory under the operating-system temp folder, PowerSurge-labs-v1. It never copies sources, compilers, native build products or an entire project. Fixture editing, progress, session data, logs and editor preferences remain inside that run. A live file lease prevents another process from cleaning an active run. Normal shutdown removes it; a later engine startup reaps positively recognized abandoned runs. Unknown directories, links/reparse points, other applications and retained runs are not recursively removed. No old BORSurgeLab AppData cleanup is performed by this engine.

Use `--keep-lab` only when deliberately retaining an editable run for diagnosis. The console and compact report identify its location. Retained runs are user-requested data and need explicit removal when no longer wanted. The default reports folder beside the executable retains only the latest ten owned reports, each with at most128 KiB of engine log; if the package is read-only, it falls back to the recognized temporary root. Reports do not include the full fixture pack or progress file. Canonical lab content is in the repository's labs/content, not in an output folder or temporary run.

## Migration boundaries

This is a reusable engine with selected original demonstration assets, not the Densetsu story package. Three terrain textures are selected for these fixtures; the remainder awaits manual curation. Existing upstream sample content is retained during this source migration. The modified RPG editor remains in its previous workspace and is not silently imported. Experimental Box2D remains in the separately verified historical lab until its provider/lifecycle promotion is decided.

The accepted native source baseline is recorded in MIGRATION_BASELINE.json. Existing native service assertions are reused; this first automatic scenario replaces one Python orchestration path. Other legacy scenarios and authoring/compiler/package checks have not yet all migrated. Their old launchers remain only in the historical workspace. No Python test launchers are added here. Turning old per-process static test state into reusable in-process objects is later work; fresh processes deliberately enforce the current boundary.

Release preparation follows verification: package the Windows runtime, matching source and notices, identify it as a pre-release, and document its tested scope. No remote release is published automatically by this build target.
