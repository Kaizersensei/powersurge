# Native actor PoC provenance

2026-10-18. Signed: Atlas.

- Host: official Open Surge source, commit `61975c23e7e877a55dfcab0e10f035907ae4ecb4`, version0.6.1.3. Original checkout remains in `upstream/opensurge`; the patched build copy is under `%LOCALAPPDATA%/BORSurgeLab/surge-poc/source`. Engine code is GPL-3.0-or-later; the full GPL text is `SURGE_GPL3.txt`. Existing stock asset/script/dependency notices remain in the runtime and upstream source.
- Actor semantic reference: OpenBOR4.0 build7555, commit prefix `b1676f9`, matching strings from the executable supplied with `F:/Retraissance/OpenBOR`. Historical `openbor.c` and `openbor.h` were acquired separately into the isolated cache. SHA256: `127dfa1850a91b06b01b62b0382ec40554293000a5efcd859475b1164646bea5` and `ccc963b6e21525e25ec920c969d58f01d48ec662806fc88fb64cdeb33b9283a2`. The separate current OpenBOR checkout is preserved.
- BSD reference/adaptation locations include model defaults/parsing, `update_frame`, `check_attack`, `perform_atchain`, `normal_prepare`, `normal_attack`, `common_attack_proc`, `common_attack_finish`, `dograbattack`, `dothrow`, legacy air-control conversion and `player_jump_check`. New `integration/bor_core.*` and `bor_bridge.c` are selected adaptations, not an embedded copy of the full runtime. Retain `OPENBOR_BSD.txt` with source/binary copies. The combined host remains under Surge's GPL terms.
- Models and images/audio are the author's supplied BOR PoC content, from the five paths recorded in `generated/manifest.json`. That manifest records SHA256 for every imported source file and reports missing audio and PCM conversion. The original baseline used only BOR artwork; the current C2 palette addition is documented below. No original content was edited. Generated model tables/JSON and sheets are derivatives of those inputs; they are reproducible through `tools/surge_poc.py convert`.
- The converter uses Pillow11.3.0 in a cache-local installation. MSYS2 UCRT64 GCC/CMake/Ninja, Allegro5, PhysFS and the static SurgeScript prefix come from the recorded M0 environment. `machine.local.json` supplies host-specific paths and remains ignored. Original Surge dependency and asset license records remain available alongside their sources.

## Host patch boundary

The build copies upstream into the cache and adds `src/bor/`. The baseline changes these host files; additional movement patches are listed below:

1. `src/misc/cmake/srcs.cmake`: add the two native actor source files to the build.
2. `src/scripting/scripting.c`: register the native BOR coordinator/controller and climb-region classes.
3. `src/core/video.c`: existing Win64 compatibility repair, `GetWindowLong/GWL_HINSTANCE` to `GetWindowLongPtr/GWLP_HINSTANCE`.
4. `src/core/input.c`: `patches/input-wheel.patch` fixes retained wheel state. AXES wheel deltas accumulate until all mouse consumers update, then expire. Current active directions get fresh press edges; neutral/opposite release edges and held buttons/modifiers are preserved. The existing boolean wheel API still coalesces deltas within one host update. Signed: Atlas.

5. `src/scripting/entitycontainer.c`: `patches/editor-gizmos.patch` adds opt-in diagnostic rendering for editor-gizmo entities during F12. BOR wrappers forward the read-only preview to their native controller.

Terrain solver, editor composition controls, level/brickset formats, camera, stock gameplay objects and renderer are otherwise unchanged. Content wrappers/inputs/fixture/mod configuration are additive. The default camera uses an invisible disabled stock player as an anchor. The adapter filters permitted motion; the stock Surge physics body owns terrain solving and actual velocity.

This local experiment is not a public release. Browser support is not assessed by this build. Its documented missing behavior must remain visible when comparing it with the original BOR package.


## Native BOR hardening provenance

The follow-up changes remain entirely inside the added native adapter, converter, wrappers and generated definitions; no further host hooks or terrain/editor changes were required.7555's `perform_atchain`, combo/cancel handling, normal AI branches, reciprocal link and throw/release behavior inform the data/controller changes. Default grab distance36 is at historical openbor.c268; default horizontal range derives from jump height at14695–14702. Final-declaration normalization is the converter's responsibility. Action-generation guards, callback/timeline budgets, fail-safe capability checks and split reaction-aging phases are explicit adapter safeguards, not claims that the entire historical source uses the same architecture.

The pre-hardening source/generated data and runnable binary are preserved in the cache milestone ZIP with SHA256 `4b46c5b858d66af76189144ea20b0fc07b6f6a6ccc1299e53ba3f0c8d91d25c3`. Current build/source/test hashes and the bounded advisory review are recorded in `evidence/bor-hardening/summary.json`. The combined source still retains the OpenBOR BSD notice and Surge GPL terms. No C2 source/assets were newly imported. Signed: Atlas.


## Preliminary physics checkpoint

Surge physics now owns body velocity and filtered input. The adapter preserves normal takeoff/cancel momentum, uses no-drag value1 and explicitly projects ground commands/reactions. The semantic motion resolver and script bindings are original adapter code; they are not a full BORScript port. Explicit7555 jumpframe velocity replacement is preserved (historical check_jumpframe at23547ff), including X=0. Atlas mirrored-hotspot compensation aligns full-cell rendering with source origins. All source content and licenses remain unchanged. Evidence and current binary/source hashes are in evidence/preliminary-physics/summary.json. Signed: Atlas.


## C2 movement / full palette provenance — 2026-10-19

The author supplied ACTOR_Michio in the Testing Construct2 project. `generated/c2_catalog.json` retains all32 animation definitions/135 source frame hashes, dimensions, hotspots, image points, collision polygons, fractional durations, looping and repeat metadata. PNG alpha is preserved; BOR palette-index0 transparency applies only to BOR inputs. Native timeline expansion and a fallback guard produce Michio66 clips/327 records, all actors96/408. Blank BlockSword is retained/unbound, with an explicit BOR idle armed-guard placeholder. Derived roll32px crop is nondestructive; original source art/hotspot/polygon remain unchanged. Importer revisions of supplemental actor definitions are backed up before replacement.

New original adapter code provides physics profiles, bounded traversal states, input policy, hand anchors, oriented combat boxes and lifetime-based support attachment. Source C2 event timing informs selected rules; symmetric friction, bounded region exits and explicit ownership avoid copying accidental event-sheet behavior. BOR matching7555 remains the action-semantic reference, including500 normal/400 running-jump strength; source current upstream parsing is not assumed identical. Numeric BOR/C2 tuning is deliberate adaptation, not proof of parity.

Additional host patches: `physics-profile.patch` exposes the existing top-Y-speed parameter; `ledge-support.patch` adds obstacle lifetime IDs, script Brick lifetime serials and rectangular clearance queries; `bor-post-script-step.patch` queues one BOR tick after VM movement and a fresh final terrain map. This fixes stale moving-platform coordinates without replacing the terrain solver. Existing input/editor-gizmo and Win64 compatibility patches remain. Normal Surge levels without a BOR queued tick retain the previous path. Host changes touch physicsactor/obstacle/map APIs, script Brick lifecycle, level scheduling and the additive BOR sources. The combined native work retains Surge GPL and BOR BSD notices. No files were edited in original source projects or upstream checkouts. Signed: Atlas.


## Continuous traversal/combat, training log and material audio — 2026-10-19

Current native data has114 clips/470 records across six models. The source C2 passive training log contributes its50x50 PNG, bottom-center hotspot and collision bounds as a sixth authored enemy-shaped passive prop, with explicit trainingdummy/grabbable/material policy. Original BOR animations remain unchanged; new directional variants retain BOR five-stage timing/scalars/actions and select C2 art/socket geometry. The barclimb placeholder copies source C2 Ledge idle; curve traversal is original polyline adapter code. New finite actionrole/combo/connectwindow and impact fields are adapter extensions, not blanket historical BOR compatibility.

Impact profiles reference38 unique loaded samples total, including pre-existing frame sounds. New assets use original C2 Ogg wood/flesh/blade/blunt/block/water files and BOR WAV stabbing/metal/rock/dirt/hard-contact fallbacks. generated/manifest.json pins145 original BOR inputs,154 C2 inputs and the authored training model; repeated profile entries are declared fallbacks. Ogg bytes are copied unchanged; existing WAV PCM conversion remains. Audio references balance on unload/reload. Matched source locations and remaining limits are in docs/CONTINUOUS_TRAVERSAL_COMBAT.md.

Native persistence now suspends off-camera bodies inside a conservative subset of the pinned host's terrain-update ROI, preserving live identities without modifying Surge's world border or terrain scheduler. Selected lab encounters use a wrapper-configured activation range. Source/license boundaries remain those above; originals/upstream/Godot candidate remain preserved. Signed: Atlas.
