# Database workspace

Open a gameplay level, then press **F10**, or use **File → Database** in the level editor. Choose a database section and a record with one click. The selected record occupies the main area; deeper recovery-effect editing opens a window sharing the same draft. World simulation pauses while the workspace is open.

## Original structure and current bindings

The section list follows the original EasyRPG Editor database: Characters, Professions, Skills, Items, Enemies, Enemy Groups, Attributes, Hero Status, Battle Animation, Battle Animation 2, Battle Screen, Terrain, Chipsets, Vocabulary, System, System 2, Common Events, Switches and Variables. Effects, Fields, Levels and Sequences are additional shared-definition sections. Surge remains responsible for spatial level editing.

Items retain the original Name, Type, Price, Usages and Description grouping, plus HP Recovery, Options, Effect Range, Change to Condition and Change to groups. The source reference is [EasyRPG Editor database_dialog.cpp](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/database/database_dialog.cpp) and [item_widget.ui](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/database/item_widget.ui). This native presentation adapts their organization; it does not embed Qt or require LCF conversion.

**Bound:** existing item name, description, inventory category, stack limit, recovery-effect reference; reusable HP recovery amount. Equipment storage and slot remain protected. Unsupported price, usage-count, percentage, party-target, condition and stat controls show their reason for being unavailable. Prices remain owned by shop offers. Dialogue & Choices and Switches/Variables are bound as described below. Common events and wind fields are bound as described below; other categories remain pending. Creation/deletion and template migrations follow later.

## Documents and saves

A stable record ID has one shared draft in a workspace, including nested windows. Switching records keeps edits. **Ctrl+S** saves the focused record; **Ctrl+Shift+S** saves all dirty records; **Ctrl+Z/Y** undo/redo the record when not editing text. Text widgets retain their normal text undo. Source data allows editing Items/Effects/Dialogue/Switch/Variable/Event/Field JSON, with the same validation and history. Find usages follows registered references.

Validate checks the full saved catalog with this candidate record substituted. Save refuses malformed records, missing references, structural storage changes, newer formats and conflicting external edits. Unknown fields are preserved. Saves write a temporary file, flush and verify it, then replace the target atomically; a verified previous-good copy remains in the local cache. Save All commits records individually and reports how many succeeded if a later save fails; it is not a multi-record transaction. Review interdependent edits in dependency order.

Dirty drafts receive recovery revisions. Closing asks whether to save, keep recovery or continue editing. Discard and reload requires confirmation. An external change is reported and cannot be silently overwritten. Newer or unrecognized recovery files are preserved and reported. Copy source provides an escape route for manual conflict resolution. Reload project index only replaces a clean workspace after successful validation.

## Files, runtime snapshots and isolation

`database/project.json` indexes authoritative versioned files under `database/records/`. IDs are independent of paths and labels. The native compiler uses the existing inventory, event and effect services. Existing record IDs and the baseline catalog signature remain unchanged. A project without this index retains the compiled compatibility catalog.

The runtime consumes one immutable catalog per engine session. Saving a draft does not replace definitions beneath an active action. **Test saved project** starts a fresh disposable snapshot in the same process, retaining the authoring source workspace. Normal progress is not copied into a test. The current first slice supports testing engine-owned laboratory workspaces; general project orchestration follows later. Tests can be launched from **Test → Laboratories → Database documents/runtime check** or `--lab database-check`.

Native lab drafts live in that run's user directory. Saving or retaining a draft keeps the owned workspace; its exact location is shown. Empty disposable runs are cleaned normally. A recognized draft or unknown cache is preserved by stale-run maintenance. Outside labs, recovery is stored in the local PowerSurge authoring cache keyed by the canonical project root. No live cache belongs in project source or a release ZIP.

Limits: 4,096 indexed records, 1 MiB per record, 64 MiB aggregate record data, JSON nesting 32, 128 undo entries per document. Linked record paths and path traversal are refused. Directory/file replacement by uncooperative external tools is detected on ordinary save paths; this is a local editor, not an adversarial filesystem sandbox.

## Traceable next surfaces

1. Items/effects: shared documents and initial runtime round trip.
2. Dialogue/choices: message, choice order/information and outcome references are bound for existing records; the current runtime supports 2–64 choices.
3. Switches/variables, common events and wind-field definitions are bound.
4. Inspector navigation opens supported shared documents in this application. Broader project orchestration and actor/combat/world/mode/presentation groups follow their authoritative services.

The JSON parser is the pinned MIT nlohmann JSON 3.12.0 dependency, with source and license under `bsl/third_party/json` and runtime notices under `licenses/dependencies/nlohmann-json`.

Signed: President.

## Dialogue and choices — E02

Choose **Dialogue & Choices** and an existing record. **Show Message** edits plain message text. **Show Choices** supplies a scrolling list, Add choice, Duplicate, Remove and Move up/down. The selected choice has its text, optional information text and a searchable outcome-event picker. Reordering moves all three together. **Inspect outcome** opens the same registered event as a shared-document detail window with the same event draft and command controls. Find usages follows saved references back to this section.

The layout references the pinned original [Show Message form](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/commands/show_message_widget.ui), [Show Choices form](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/commands/show_choices_widget.ui), and [Message Options form](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/commands/message_options_widget.ui). Choices extend beyond the original four using the runtime's existing 2–64 bound. Standalone messages without choices need a later runtime binding; they are not synthesized using fake options.

Cancellation currently closes without dispatching an outcome. Redirect-to-choice, ignore-cancel and custom handlers remain disabled. Per-message transparency/position and event-concurrency overrides remain disabled; the current game skin, layout, participant input locking and calendar policy retain ownership. Message and selected-choice information can display together with the choice list. An empty outcome means close without an event. Validation rejects missing event references and mismatched choice/help/outcome lists; unknown record fields remain preserved.

Save and start a new test to use the authored snapshot. The native database check now also reorders a choice, saves edited message/label/information, displays it through the actual game widgets, dispatches the selected calendar event and verifies cancellation has no outcome. All edits, clock changes and progress belong to the disposable test.

Signed: President.

## Authoring task ledger

| ID | Surface / dependency | Authoritative data | Runtime binding | Status | Evidence / remaining gap |
|---|---|---|---|---|---|
| S05 | Shared documents | Versioned record files | Native catalog | Verified for current surfaces | Shared drafts, undo/recovery, conflict and atomic-save scenarios; multi-record atomic transactions remain separate. |
| E01.1 | Item/effect layout | Original EasyRPG item form groups | Inventory/effects | Verified | Unsupported original fields are disabled with explanations. |
| E01.2 | Item/effect binding; S05 | Item/effect JSON | Native catalog and existing services | Verified | Metadata, stack limit, effect reference and HP recovery; structural migration and other effect types pending. |
| E01.3 | Item/effect demonstration | Saved content snapshot | Real inventory consumption | Verified | Native database case applies17HP authored recovery in a fresh session. |
| E02.1 | Message/choice layout | Original message/choice/options forms | Existing game windows | Verified | Message text, scrolling choice list, per-choice help and event picker; unavailable options identified. |
| E02.2 | Message/choice binding; S05 | Dialogue JSON | Native catalog / Conversation | Verified | Add, duplicate, remove and reorder aligned rows; shared source/undo/save; existing record IDs retained. |
| E02.3 | Conversation demonstration | Saved content snapshot | Conversation → existing event runner | Verified | Authored message/help render; reordered branch advances one configured game hour; cancel dispatches nothing. |
| E03.1 | Switch/variable layout; S05 | Original Name forms plus definition defaults | Existing scoped-state service | Verified | Scope is read-only; defaults and widened bounds are authored values. |
| E03.2 | Switch/variable binding | Switch/variable JSON | Native catalog / Events | Verified | Safe saves reject scope changes, narrower ranges and invalid defaults. |
| E03.3 | Scoped-state demonstration | Saved content snapshot | Existing event runner and selected-progress codec | Verified | Native scope/condition and changed-default save checks; actual runtime default7 → event8 without source writes. |
| E04.1 | Common-event layout; E03 | Original Name/Execution/Switch/Event Commands groups | Native event runner | Verified | Unsupported execution modes remain identified. |
| E04.2 | Common-event binding | Event JSON | Shared command registry / existing services | Verified | Typed parameters, registered references and branch-preserving sequence edits. |
| E04.3 | Authored sequence demonstration | Saved content snapshot | Existing native runner | Verified | Native151 assertions; authored wait/condition/increment, cancellation and reference/branch checks. |
| E05.1 | Field layout; S05 | Name and original weather Type/Power grouping | Existing WindHelper | Verified | Wind-specific controls; unrelated weather types are not inferred. |
| E05.2 | Field binding | Field JSON | Native catalog / WindHelper | Verified | Fractional strength, Left/Right direction, source validation and shared safe saves. |
| E05.3 | Field service demonstration | Saved content snapshot | Existing request/lifecycle service | Verified | Native182 assertions; fresh runtime consumes authored left wind; actor movement coupling remains a separate prerequisite. |
| B01.1 | Inspector layout | Selected placement and stable event binding | In-app database request | Verified | Current binding is visible; unsupported targets have disabled explanations. |
| B01.2 | Inspector binding | Mounted project / explicit staged-test source | Shared Workspace / Document | Verified | Versioned requests, copied origin context, explicit status replies, dirty-draft preservation and queued-selection guard. |
| B01.3 | Inspector demonstration | Disposable placed event anchor | Real native command → shared event form | Verified | Native database195 / editor59; navigation/return and unsupported-target checks; no external process. |
| B02.1 | Owned-test layout | Test controls / confirmation / result panel | Same-process controller | Verified (owned labs) | Launch, reload, stop and source-return descriptions. |
| B02.2 | Owned-test binding | Saved content snapshot and parked source Run | Existing engine restart and services | Verified (owned labs) | Separate leases/paths, source-save retention and stale-request guards. |
| B02.3 | Connected lifecycle demonstration | Disposable source/test/reload/return | Native owned-test scenario | Verified (owned labs) | Connected40-check case plus combined30-check journey; general project orchestration remains pending. |

In the current laboratory, **T** opens Training choice and **Shift+T** opens the scrolling-choice fixture. Save and start a fresh test before checking changed text or branches. Signed: President.

## Switches and variables — E03

The original [switch form](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/database/switch_widget.ui) and [variable form](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/database/variable_widget.ui) each provide a Name field. The native surfaces retain it and add our definition defaults. Switches use a boolean initial value; variables use signed 32-bit initial/minimum/maximum values.

These controls edit definitions, not current gameplay state. A Player scope has a separate value for each explicit player subject; Session scope shares one value across subjects. Existing scopes stay read-only until an explicit save migration is available. Variable bounds can be expanded; narrowing them through forms or Source data is refused to protect existing saved values. Invalid range drafts remain editable, but cannot be saved. Validation also checks existing event constants/references against the resulting catalog.

Save and start a fresh test to consume changed defaults. Runtime events remain the only owner of live values. Explicit values in loaded progress take precedence over changed defaults; keys absent from progress use the current definition default. The native scenario checks both cases through the real selected-progress codec, rather than introducing another save system.

Find usages and shared event details provide reference navigation. Editing event commands is described under E04 below. Record creation/deletion, scope migration, range-narrowing migration and live-state debugging controls remain separate work. Signed: President.

## Common events — E04

**F10 → Common Events** retains the original [common-event form](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/database/common_event_widget.ui) groups: Name, Execution, Switch for Execution and Event Commands. The [original command list](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/event/event_commands_widget.cpp), [switch operations](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/commands/switch_operations_widget.ui) and [variable operations](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/commands/variable_operations_widget.ui) supply workflow/field references.

Select a command with one click. **Insert command** searches supported operations and inserts before the selected row; **Append Wait** adds a wait at the end. Duplicate, Remove and Move up/down edit the same shared document. Parameters appear below the list. Changing type deliberately replaces its known parameters while retaining unrelated command extensions; choosing the current type is a no-op. Source and forms share native validation and undo/recovery. Event sequences contain1–256 commands.

Conditional branches compare a registered switch to ON/OFF. A match continues; a mismatch skips the authored number of following commands. The list shows the mismatch destination. Sequence operations preserve the identity of that destination, including End; inserting at a destination keeps the original command as the target. Deleting a targeted command or introducing a backward/empty branch is refused without changing the sequence. Retarget the branch explicitly before such an edit. End remains End when new commands are appended. This is the runtime's bounded forward-skip model, not a general graph/loop interpreter.

| Supported group | Current operations |
|---|---|
| State | Switch ON/OFF, set variable to integer constant, add integer constant (negative subtracts) |
| Flow | Wait in60Hz simulation ticks; conditional switch branch |
| Inventory | Grant/remove stack items; buy/sell a registered shop offer |
| Progression | Registered trusted-script reward/receipt, accept/claim quest, grant/revoke registered ability |
| World | Advance calendar by game seconds, spawn a registered player character or actor template |

The editor/parser share one command vocabulary. Pickers use existing registries and show stable IDs; item commands exclude unique equipment. External reward/quest/shop/offer/character/actor/ability references are checked before save. Operations with no registered targets are disabled; the current actor-template registry is empty, so actor-template spawning is unavailable. Valid references do not guarantee runtime eligibility: player/character ownership, profile assignment, quest progress, stock and other conditions remain owned by existing services.

Execution currently means an explicit call through a registered action/interaction. Foreground/automatic and background/parallel scheduling, execution-switch triggers, nested event calls, general loops/scripts, advanced variable operands and message/choice event commands remain unbound. Dialogue records remain editable separately. The UI identifies those gaps instead of creating hidden runtime services.

Save and start a fresh test. The current native demonstration edits the existing training reward: wait3 ticks, grant one item, set its switch, condition on annex access, then add3 to the authored counter default7. The real runtime reaches10 and the source default stays7. Native cases also confirm that cancelling a waiting sequence preserves committed effects and cancels remaining steps. All demonstration changes stay in disposable content/progress. Signed: President.

## Wind fields — E05

Choose **Fields** and an existing definition. The form retains Name and adapts the Type/Power grouping from the pinned original [Weather Effects form](https://github.com/EasyRPG/Editor/blob/4a83bb038275395b7e2bd166c0f15d4554783793/src/ui/commands/weather_effects_widget.ui). Fields is a project-specific registry section; the original weather choices are not silently mapped to wind. Horizontal wind is the current type. Strength accepts fractional values from0 to1000000 px/s²; Left/Right stores direction-1/+1. A signed acceleration preview explains the result. Source and forms share the same contract. Invalid range drafts remain editable, while saving refuses them.

Save and start a fresh test to consume the field. Existing WindHelper instances retain their copied definition. The existing service creates one acceleration request per occupant and emits loop-start/loop-stop notifications on first entry/last exit. Repeated entry does not retrigger a loop; clearing releases participants and requests.

This binding reaches the field service, not actor motion: actor-force application, spatial-area placement, audio/particle assignments and weather/region overrides remain separate prerequisites and are identified in the form. Strength is an authored acceleration request, not a global weather setting. The native round trip saves240.5 with Left direction and verifies-240.5 requests using the active runtime player's identity. Other database/runtime checks remain included. No normal progress is used. Signed: President.

## Inspector navigation — B01

In the level editor, select a bound **Event Anchor** and choose **Edit linked definition**. Its shared common-event draft opens in this application's database, with the originating level, placement ID and object class shown above the record. Closing the database returns to the same level editor. **Open database** activates the workspace without toggling it closed. The original external-editor launcher and temporary request/reply files are not used by this path.

Repeated navigation keeps the same shared draft, including invalid source that still needs repair. It does not save, reload or replace the active gameplay catalog. Requests contain a version, explicit project, record ID and optional level/placement/class context; replies distinguish opened, invalid, wrong-project, unavailable, missing-target and failure states. Caller strings are copied before the request returns. A queued selection that no longer matches is refused.

Requests are limited to the mounted project or an explicit source recorded by this process's saved-project test. If the retained authoring workspace owns that source, inspector navigation opens the source draft rather than making the disposable playtest snapshot a second authoring owner. Another unrelated workspace's dirty drafts cannot be silently replaced. Clean remounting validates the new target before replacing the workspace. A missing index/record or unsupported surface leaves existing documents/selection intact.

Unbound event anchors require a binding first. Teleporter and actor database editors remain unavailable until their record adapters exist; instance parameters remain editable in the level inspector. Generic workspace activation is available separately. No second editor process, native GUI embedding or shared native pointers are involved. Signed: President.

Current labs retain one authoring-cache owner per run. The ordinary retained-source test flow reuses that owner. Explicitly switching between two allowed clean roots that already have different cache identities can be refused safely; general multi-workspace switching and per-root lab caches remain later orchestration work. Signed: President.

## Owned saved-project tests — B02

**Test saved project** (or **Test → Test saved source**) prepares and validates a consistent content snapshot before replacing the live level. Save database, terrain and placement edits first. The current source Run, user/progress paths, recovery cache and exclusive lease are parked. The test uses a fresh content copy and separate user/progress paths. Normal player progress is not copied into it.

**Reload saved source** creates another fresh test from the saved source; pending source database drafts must be saved or discarded first. **Stop test / return to source** restarts the saved source level and opens the retained authoring workspace. It preserves source drafts, including those edited while the test ran. This restarts the world scene rather than restoring a full live-world snapshot. Source progress/preferences storage identity is restored; clone progress is never imported.

Replacement confirmation reports dirty or saved test-only content. Unsaved runtime edits and test progress are disposable. Saved clone edits are retained separately under their owned temporary workspace and are not merged into the source. Source saves during a test retain the parked source before writing. Failed snapshot preparation leaves the current runtime, source lease and drafts available for retry. A host failure after teardown preserves source data for recovery and may require restarting the application.

The status panel shows the source, snapshot version, reload count, outcome and last native report path. Requests carry session/generation identity; stale requests and unrelated laboratory switching are refused. One owned test per engine instance is supported. This first controller accepts owned laboratory projects only; general project loading, cross-project cache ownership and arbitrary test loadouts remain later work. The existing automatic database check retains its own continuation contract. Snapshots reject linked files and unfinished saves, with limits of20000 files,512MiB per file and2GiB total. The version string is a change detector, not a security signature. Signed: President.


## Existing level properties and sequences — B03.1–B03.3

F10 > Levels adapts the original map-properties Name and property groups to the registered scene model. Edit location, organizational parent, editability and weather context (region, hemisphere, indoor, preset, multipliers). Parent links organize records without inheriting their properties. Tile sets, dimensions, wrapping, BGM, encounters and panorama groups remain native scene concerns or unbound registry surfaces. Level > Level properties opens the current registered record; spatial composition stays in Surge.

F10 > Sequences edits the name and ordered registered-level IDs. Duplicate a member, select its replacement, then reorder or remove it. Repeated visits are allowed. The registered sequence route is consumed by the existing native loader; it copies resolved scene paths at load. Indexed JSON is authoritative over its legacy `.qst` fallback. A running sequence and level properties remain unchanged until a fresh test/reload.

Test saved sequence uses the existing confirmed owned-test controller with disposable progress. Reload reads the saved sequence again. Stop returns to the original source level, even when the tested sequence begins elsewhere. Database drafts must be resolved before launch/reload. Native `.qst` files without an indexed record retain their original parser, including presentation routing.

| Task | Surface / data owner | Binding | State | Remaining gap |
|---|---|---|---|---|
| B03.1 | Levels / versioned level records | Immutable session level registry; weather context | Structured, bound, native verified | See creation step below; deletion and coordinated scene/terrain migration remain pending |
| B03.2 | Sequences / versioned sequence records | Existing native sequence loader and exit helper | Structured, bound, native verified | New sequences, nested/built-in entries and broader project opening |
| B03.3 | Saved-sequence test | Existing owned lab controller | Bound, native verified | General project loading and dependency-complete preflight |

Indexed packaged level records replace the obsolete compiled-only level inventory for this project. Projects without indexed level records retain the compiled fallback. Unknown record fields survive form edits, undo and saves. Scene/terrain paths and sequence routes are protected from partial migrations. This step does not create a second spatial editor or alter geometry, actor behavior, normal saves or the presentation sequence. Signed: President.


## New source levels — B03 creation step

Open **Levels → New level**, enter a display name and a stable lowercase key, and optionally choose an organizational parent. Save/discard database drafts and stop an owned test first. The new `level:user-<key>` and `sequence:user-<key>` appear immediately in the same workspace. Existing open records retain their shared document identity.

Creation writes an independent native scene, cell-terrain document, level record, starter sequence record and native sequence fallback. It reuses the shared material library, default player fixture, runtime services, camera and pause setup. The initial flat stone floor can be erased or extended in Surge. The native origin is4096,4096; cell coordinates remain signed around world zero. The starter sequence has one level; existing sequence forms can also reference it. Independent sequence creation, deletion and structural path migration remain pending. Creation is an explicit structural commit, not an ordinary record undo action.

**Open for spatial editing** confirms a source-level replacement, refuses unsaved terrain/placements/database drafts and opens the registered source scene with its cell editor. Files, the source lease, storage identity and shared authoring workspace remain the same. A fresh asset-session snapshot discovers the new registry entry. Runtime progress is reconstructed from the lab's storage, rather than restoring a live-world snapshot; normal game saves remain separate. This source-opening bridge currently supports engine-owned laboratory projects. It opens the level directly; use **Test saved sequence** to exercise authored sequence traversal.

**Test saved sequence** continues using a disposable snapshot. Edits saved inside that test are retained separately; they do not update the authoring source. Stop returns to the source scene from which that test began.

Creation never overwrites an existing scene, record, terrain, route or staging file, including case aliases. It validates definitions, shared starter dependencies and both before/after index bounds before installing its recovery journal. New files are installed without replacement; atomic replacement of `database/project.json` is the single publication point. The index and authoritative records remain versioned disk files, not a second cache/database owner. Existing geometry/assets, IDs, progress formats and presentation routes are unchanged.

An interrupted operation shows **Resolve creation recovery**. If the index was not published, recovery removes only the verified new payloads and recognized staging prefixes. If already published, it preserves the level and finishes cleanup. All files are checked before deletion. Conflicting index/payloads, unknown journals and newer formats are preserved and reported; keep/export or discard drafts before recovery. Record saves and test snapshots are blocked until recovery is resolved. Lab stale-run cleanup conservatively retains pending/unknown creation recovery, even without a database-draft cache.

Limits: both index versions must fit1MiB, with at most4096 indexed records and the existing level/sequence registry bounds. Shared starter assets are checked; this is not yet a complete dependency-closure validator. Saves assume cooperating editors respect the project lock and are not an adversarial filesystem transaction system. A failure after registry publication explicitly reports committed work needing recovery/refresh; do not retry creation under a new key to hide that failure. Signed: President.

Transition readiness: the shared database is suspended only after a transition has passed preparation; its documents/drafts stay owned by the same Workspace. Authoring resumes after the level render timer reaches its native readiness threshold. This prevents pausing a newly loaded scene while its renderer still intentionally suppresses initial frames. Native capture checks actual non-black content in the unobscured world viewport. Signed: President.

| Task | Surface/data | Binding/evidence | Status / remaining gap |
|---|---|---|---|
| B03.4 | New source level and starter sequence | Registry-last transaction; native creation/dependency/no-overwrite checks | Bound and verified; additional presets/independent sequence creation pending |
| B03.5 | Interrupted creation recovery | Prepared/partial/committed/conflicting/future filesystem scenarios | Bound and verified; conflicting/unknown recovery deliberately preserved |
| B03.6 | Source spatial opening | Same-root restart, shared drafts, render-ready editor and actual viewport check | Bound and verified for owned labs; general project opening remains pending |


## Persistent source projects

File > New project creates a durable source folder from the bundled starter content. File > Open project (or `--project <folder-or-manifest>`) opens it directly; Recent projects reopens a selected root. File > Save project copy copies the mounted source content into a NEW folder with a fresh project identity, excluding local progress, preferences and draft recovery. Destinations must have an existing parent, must not exist and must not be inside disposable lab storage or the current source/run. The source is never moved. Unknown/newer projects are refused without rewriting them.

A project contains `powersurge.project.json`, `content/` and local `.powersurge/` state. Source records/levels/assets live in content; local preferences, database recovery and source-session progress stay in .powersurge. An exclusive lease prevents two processes editing one project. Project manifests preserve unknown fields, check external edits, back up the previous manifest and replace atomically. Save project saves database drafts then level edits and publishes the saved entry setting last; commits are per file and failures can leave earlier files saved. Individual level/database save commands remain available. Source editing opens the authored entry rather than letting progress-boot choose a different level.

Tests still use same-process disposable content snapshots and separate progress. Stop reopens the source scene and retains its document owner; it does not import test progress or restore a full live-world snapshot. A laboratory can be saved as a project copy, but its retained directory is not itself a permanent project. Default presentation startup remains unchanged. Retained historical user roots are preserved; migration/import from an arbitrary old run remains an explicit later operation.

Generated-output storage prepares the companion-map workflow: versioned recipes reference immutable output revisions and the source fingerprint captured before computation. Rebuild preserves supplied images, earlier revisions and unknown recipe fields; invalid/stale writes are refused. Interrupted revisions have recognizable ownership and can be retried without deleting their files. This is the storage API, not a completed normal-map generator or material binding.

Native checks: `--project-storage-check` exercises filesystem/refusal/recovery/ownership cases using binary storage payloads, not generated-image correctness. `--lab project-session-check` exercises creation, source saving, level creation, copy/reopen and disposable test return in one process. These scenarios allocate only owned test fixtures and never open retained user projects. Project history is one bounded portable `.userdata/project-history.json` file; it and local project state must not be included in public packages.

Signed: President.


## Rendering record contracts

Terrain now adapts an indexed `terrain_material_library` document, with one draft for the whole library and a material selector inside it. `legacy_path` identifies the native terrain attachment; the indexed record is authoritative for that path, including case aliases. A nine-field compatibility projection feeds the existing terrain consumer. The old terrain JSON is unchanged and is used only when no indexed library owns its path. Find usages follows level attachments. New-level dependency validation uses the indexed material definition too.

| Surface | Saved record data | Current runtime support |
| --- | --- | --- |
| Terrain | Existing color texture/tint and profile references; companion maps, UVs, parameters, light flags and bevel defaults | Color/tint bound; normals, UVs and scalar surface response bind through static level profiles. Other effects remain pending |
| Lights | Directional, point and spot definitions; color, energy, direction/elevation, virtual height/range/falloff, cone and shadow request | Directional definitions evaluate on static-profile terrain and in the study; local placement remains pending |
| Lighting | Ambient color/energy, static/calendar driver, sun/moon references | Level profile attachment and reference validation bound; calendar/weather projection and light queries pending |
| Shader Profiles | Registered builtin/trusted-source program, source reference and typed parameters/defaults | Builtin surface/terrain programs bound with nonfatal compilation; trusted-source execution remains pending |

Optional level `properties.lighting.profile` refers to a lighting profile; it does not add another spatial level editor. Point/spot positions belong to future Surge light instances. The original Terrain encounter/vehicle groups remain identifiable and unavailable until runtime adapters exist. Source data remains editable, with shared undo/conflict/recovery behavior. Source syntax/type errors remain recoverable; rendering forms restore UI stacks safely.

All records use `version:1`, stable IDs and project-index ownership. IDs use `material-library:`, `material:`, `light:`, `lighting:` and `shader:`. A library's compatibility path and member identity/order are protected against uncoordinated migration. Save rejects missing references/assets, incompatible builtin parameter ABI, invalid paths/ranges and unsupported color images. Unknown source fields survive round trips; they are not silently treated as implemented features. Existing projects without these records retain legacy behavior; this stage does not automatically install or migrate their data. New bundled projects include the eight initial records.

Colors are three sRGB channels0–1; energy is relative. Directions are clockwise degrees from world right; elevation0 grazes the2D surface and90 points toward the viewer. Virtual height and range use metres, at the existing32px/m convention. This defines data units, not a finished physically accurate renderer. Sun/moon references require directional lights. Ambient/calendar/weather state remains owned by the existing environment services.

Map roles are normal, height, roughness, AO and emission; color uses the existing texture field. Companion maps are data textures. Normal convention is explicitly OpenGL or DirectX; signed nonzero UV scales support mirroring. Color-space conversion and transformed normals are verified by the surface study and static terrain scenario. Shader uniforms use identifiers rather than record-ID syntax, with float/int/bool/vec2/color values. Builtin parameter names/types/bounds remain compatible with their registered program. Trusted-source registration alone never executes code.

Bevel defaults store enable/profile/width/depth/smoothness and separate/continuous/soft_seam contact mode. Width/depth use world units; relief remains visual. Any touching neighbor can participate without a shared group. Per-shape overrides, actual relief normals and light occlusion are subsequent adapters. Existing physical/acoustic/feedback references remain separate from visual response.

Color-image validation is shared between Save and playback. Current native color limits are opaque PNG,1024px maximum dimension,16MiB encoded size and8M aggregate pixels per library; these pre-existing consumer limits are now reported before source publication. Alpha/expanded texture formats remain deferred. Companion-map decoding is not claimed by path validation.

Saved rendering records are deep-copied into the same session-owned source snapshot as the other definitions. Editing a source record does not replace the current cache; a fresh test/source reload consumes it. Texture refresh reads the current session's material definitions, preserving the single authority. No collision, actor controller or gameplay-light exposure rule changes in this stage.

Signed: President.

## Opt-in surface renderer

The registered normal-map study consumes the same saved Terrain material libraries, builtin shader defaults, directional Lights and ambient Lighting profiles. Its preview controls are temporary copies; normal maps, scalar roughness/specular/emission and UV transforms now also bind to receiving terrain materials with a static level Lighting profile. Legacy/calendar-profile terrain retains its appearance. Calendar projection, local lights, shadows, data-map roles other than normals and gameplay light queries remain pending. See [the study contract](NORMAL_MAP_STUDY.md).

Static terrain profile setup, data precision and remaining boundaries are documented in [Static terrain lighting](TERRAIN_LIGHTING.md).
