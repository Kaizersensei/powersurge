# Editor view navigation

Signed: President, 2026-11-22.

Middle click samples the active tool: terrain shape/material association, entity template with instance overrides, event helper, or decoration appearance. Select/Inspect can sample the target and arm its corresponding placement tool. A six-pixel movement threshold distinguishes a click from a middle-button drag. Middle dragging pans, including from a focused panel; it never samples. Focus loss cancels the gesture.

Ctrl + mouse wheel over the level zooms around the pointer from 20% to 1000%. View offers fixed zoom values and a 100% reset. Panel scrolling stays available without Ctrl-wheel over the level. WASD/arrows move the view while a panel is merely focused; active controls, text entry and modal dialogs retain their keys. Shift accelerates keyboard panning. The view is independent of the gameplay Camera and resets to 100% on editor entry.

Esc or right click cancels a pending first line point without painting, erasing or resuming play. Once the input is released, normal tools resume. Palette drag release over the canvas places exactly one undoable instance; invalid drops and Esc cancel it. Click-only palette selection still arms placement.

Terrain, entities, grid, previews and drops use the same view mapping. Zoom changes a scoped GPU projection, not the window resolution or the physics. Fine grid display density adapts at wide zoom; the authored brush/grid does not change.

The focused native editor-contract check covers inverse mapping and limits, preserved gameplay camera, line-preview cancellation, render snapshots at 20%/1000%, real palette row drag/release, exact placement count, undo/redo, protected saving and editor re-entry. Physical-device usability remains a manual review.

Gameplay-camera expansion is a separate planned surface. Existing SurgeScript Camera position, lock/unlock and coordinate APIs are the starting bridge. Plan look-ahead from facing/velocity, dead zones, focus targets, authored interpolation, fixed viewpoints/cutscenes and eventual gameplay zoom as one camera authority. Preserve that controller state across editor entry/exit; editor navigation must never write into it. Review earlier camera prototypes when that work begins.

## Requested terrain/editor backlog — 2026-11-25

These additions are queued alongside the existing terrain/material/editor work; common-event authoring remains the current task.

- Hollow/filled mode applies **only to closed shapes**, not open lines. Exact thickness, collision and editing semantics need agreement before implementation.
- Selected terrain-shape outlines: implemented; see selection feedback below.
- Multipoint shape authoring, retaining the earlier minimum-three-points and automatic-closure requirements.
- Separate fill and outline textures/material presentation.
- Name and list terrain shapes, object placements and decorations; offer hierarchy/tree and category/list views.
- Group shapes and save groups as reusable prefabs.
- Add terrain-prefab placement/support.

Build on the existing material registry, collision/decoration separation and linked-group/instance-override decisions. Confirm remaining prefab contents, update behavior and closed-shape hollowing details when this work starts. No terrain behavior is implemented by this backlog entry. Signed: President.

## Temporary editor icons

Until the supplied icon set is ready, represent custom objects and editor functions with square placeholders containing a character or symbol. Keep readable names/tooltips alongside them; placeholder glyphs do not replace stable object identities. Replace placeholders with the supplied graphics when available. Signed: President.


## Terrain selection feedback

The selected terrain shape has a cyan contour with a dark halo in Terrain mode and in Select/Inspect when no entity or decoration is selected. Every visible disconnected region sharing that shape association is highlighted, including holes and boundaries against other shapes. The outline remains legible from20% to1000% editor zoom. Switching shape selection uses existing cached contours; brush edits invalidate only changed chunks and their immediate neighbors. Undo/reload/level changes discard old contours. Selection is a read-only overlay: authored geometry, materials, collisions and gameplay rendering are unchanged. Signed: President.

Named custom entities without existing editor art use original32px square two-letter markers from images/editor/helper-placeholders.png. Sprite aliases make palette, cursor, selection bounds and default placed markers agree. Bottom-centre anchors keep glyphs above a floor-level placement origin. TP=teleporter, EV=event, TC/PC=temporary/permanent checkpoint, SQ=sequence exit, PU=pickup, PG=progress gate; PL/LD/BR/BS/BP/BC distinguish climbing helpers. Existing actor/prop art remains in place. Temporary glyphs can be replaced by the user icon set through the aliases. Signed: President.


## Lighting and bevel direction — requirements, implementation deferred

Confirmed: terrain receives lighting first; actors/objects/decorations remain later receivers. World-light data must also support camera-independent gameplay queries (for example future stealth visibility), with rendering and gameplay consumers separate. Vision AI is not included. Define light behavior and interaction before implementing bevel lighting. Bevel contact choices are mutually exclusive: Separate, Continuous surface, or Soft seam. The exact contact-ownership rules remain to be agreed.

The supplied graphics-suite reference exposes bevel profile/width; image smoothness/depth/ambience/shininess; and light color/angle/intensity/elevation. Proposed ownership is shape/material surface parameters versus scene/light parameters. In particular, distinguish an artistic ambience bias from ambient-light energy. Units, defaults, specular response and conversion curves are not fixed yet.

Normal mapping is feasible in the existing programmable OpenGL renderer and managed shader sampler API (src/core/video.c, shader.h/.c); terrain materials do not yet bind normal maps. The first material study should reuse/reference the existing Godot material library: color, normal, height and roughness textures where compatible, mapped material settings and selectively ported shader effects. Validate normal-map orientation, UV/scale conventions, data-versus-color texture handling and resource licensing per asset. Godot material/shader files are reference inputs; direct runtime compatibility has not been implemented. Allegro's sampler API is documented at https://liballeg.org/a5docs/5.2.9/shader.html .

Still open: initial light types, spatial position/height/direction, range/falloff, color/intensity units, global ambient/daylight/weather ownership, shadow/occlusion/transmission behavior, runtime switching/dimming/flicker and attachments, material normal/specular response, light-query sampling/budgets and authoring surfaces. Proposed bevel rendering leaves the authored shape identities and collision coverage intact; physical extrusion is not established by this requirement. Signed: President,2026-11-26.


### Companion-map authoring — planned toolkit port

The Godot Densetsu Tool Suite and its companion-map generator are reference implementations for a native asset-authoring surface. The requested goal is customizable normal maps from user-supplied textures, not only fixed batch conversion. Existing generator code already offers height inversion, preblur, normal strength, wrap/clamp sampling and green-channel inversion, with additional AO/specular/displacement/ORM outputs. This read-only inventory does not certify those algorithms or ports as complete.

Proposed workflow: preserve the supplied source; select texture-derived or supplied height input; adjust strength, height contrast/inversion, smoothing/detail, boundary/tiling behavior and normal orientation; preview with the agreed lighting model; save separate outputs plus a versioned editable recipe; register the output through the material definition. Rebuilding should retain source identity and generation settings. Image-processing behavior can be adapted independently from Godot's EditorPlugin widgets. Source/output ownership, persistent project paths and safe writes are prerequisites for polished authoring; generated assets must not silently live only in disposable tests. Mesh/terrain/kitbash bakers and material extraction remain additional candidates for static2D resource-generation workflows. Signed: President,2026-11-26.


## Rendering definition surfaces

Database > Terrain edits the authoritative indexed material library where available. Color/tint changes reach a fresh test; the current session snapshot stays unchanged. Lights, Lighting and Shader Profiles expose versioned data with explicit renderer-pending labels. Level properties can reference a lighting profile. Local light placement, light queries, normal/bevel shading and shadow rendering are not implemented by these forms. Source errors stay recoverable through Source data/Undo. Legacy projects without indexed libraries continue using their existing material files and are not migrated automatically.

Show-all-shape-outlines remains queued: subdued unselected outlines with stronger selection feedback, separate from gameplay rendering.

Signed: President.

Material controls show selected-image previews and thumbnail rows with search. Hover a dropdown and use wheel Up/Down to choose previous/next; list endpoints clamp. Open popup lists retain normal scrolling. Ctrl+wheel stays editor zoom. Shape Properties exposes visual-only bevels; see TERRAIN_LIGHTING.md.

Straight square strokes use a continuous swept brush, including at larger brush sizes. Curves retain the selected square/round preset; previously saved square curves replay with this corrected kernel. The world still spans -2048..2048px; its sparse field supports the complete4096-chunk extent rather than the old256-chunk prototype cap. Raster contours/collision retain2px sample resolution. UI releases are observed without painting; refused gestures clear their pending point, report a log error and wait for button release.
