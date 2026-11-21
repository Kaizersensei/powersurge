# Contributing to PowerSurge

Read AGENTS.md, CLAIMS.md and HANDOFF.md before changing shared code. Claim work, preserve authored content, and leave signed evidence and handoffs. Mechanics/physics, registry/editor bindings and presentation have separate owners.

The repository is the native engine source. Use CMake directly; CMakePresets.json describes the development configuration. Install the documented Open Surge build dependencies, including a static SurgeScript library and modular Allegro. The selected audio/DSP/toolkit sources live in bsl/. Build target powersurge_portable assembles the Windows development preview. No Python pipeline is required for native lab execution.

Use the native lab options documented in docs/LAB_RUNS.md. New lab scenarios must own disposable state, release resources, preserve canonical fixtures and never access normal progress. Keep output, retained runs and generated reports outside Git. Do not promote experimental physics or generated fixture records into canonical authoring contracts without a separate reviewed change.

Selected demonstration assets live in labs/content. The inherited upstream sample game is not part of this working tree. Preserve required copyright/license notices for all retained engine code and dependencies. Texture selection remains manually curated.

Signed: President.
