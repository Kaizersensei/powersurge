# Static terrain lighting

Open **F10 > Test > Laboratories > Terrain lighting course**, or launch `--lab terrain-lighting`. The registered level also works from Level Select and persistent projects; its saved level/material/profile records supply the binding. This is a playable terrain course, with normal relief, axes, an unlit comparison, mirrored/rotated maps, DirectX conversion and material seams. F12 edits the same geometry. Existing courses retain their original data and appearance.

A level opts in by referencing a Lighting profile whose driver is **Static**. Indexed terrain materials use the builtin terrain-lit shader and **Receives light** to participate. The profile's daylight reference supplies a directional light; an absent reference gives ambient-only lighting. Unlit materials and failed normal maps retain their transformed albedo. Material definitions and shader defaults come from the immutable saved registry. F10 exposes Terrain, Lights, Lighting and Shader Profiles; save definitions, then start/reload a test to adopt them. Ctrl+Shift+R refreshes image files using the current session definitions. No preview operation writes gameplay progress.

The normal-map study and terrain pass share the same GLSL lighting function. Color is sRGB; normals are raw data. UVs repeat in world space using color-image dimensions, across negative coordinates and64px chunk boundaries. Normals transform with the authored UV rotation/mirror convention. Different normal-image dimensions are supported. Optional bevel relief derives normals from actual shape coverage, including holes; it is separate from repeating detail-normal textures.

Baking updates visual chunks after edits or image refresh. Each chunk retains albedo plus three nearest-sampled data images: world normal RGB/roughness alpha, and specular/16-bit scalar emission/lighting weight. Normals, roughness, specular and lighting weight have8-bit precision; emission covers0–100 with16-bit precision. Material seams mix albedo, normalized normals and scalar properties into an effective surface response, while keeping authored shape/material identities. Coverage alpha remains independent. Adjusting a light changes shader uniforms, without regenerating pixels, collision masks or obstacles. Normal libraries are bounded to8M unique decoded pixels.

Missing/invalid normal maps fall back per material and report diagnostics; shader failure falls back to cached albedo. Neither failure removes collision. Image reload stages all candidate visual resources and retains existing visuals on failure. Old sampler references clear before images are destroyed. Light blocking metadata is independent of collision and acoustic materials.

`--lab terrain-lighting-check` checks actual chunk GPU output against a CPU lighting reference, signed UV continuity, normal conventions, seams/unlit coverage, image refresh without collision changes, bad-map fallbacks, ambient-only profiles, new painting and undo. Broader checks retain the shared surface study and existing editor/source/test/terrain journeys. Evidence stays private.

Calendar-driven sun/ambient projection, regional/indoor/event overrides, point/spot placement, cast shadows, gameplay-light queries and height/AO/roughness/emission **map** sampling remain pending. Calendar profiles currently retain legacy unlit terrain rather than pretending to evaluate time. This stage adds no physics/controller changes and makes no sustained-performance or cross-platform certification claim.

## Contour bevels

Open **Test > Laboratories > Terrain bevel course**, or `--lab bevel`. The three paired rows demonstrate Separate, Continuous and Soft seam from top to bottom. They use distinct materials, holes and curved outlines. A separate receiving material demonstrates bevel plus non-flat detail normals.

In F12 Select/Inspect, choose a terrain shape and expand **Bevel (visual only)**. Enable it and tune Linear/Round/Concave profile, Width, Depth and Smoothness. Width/depth are metres at32px/m. Shape overrides can return to material defaults; material defaults are in F10 > Terrain. Edits update the real visual cache, support undo/redo and never change collision. Definitions saved in F10 still require a fresh test. Legacy terrain files without overrides remain valid and unchanged until an edit is saved.

Separate retains each visible shape's own border. Continuous suppresses contact bevels against any occupied neighbor. Soft seam retains one quarter of the seam slope. Each side controls its own response: the neighbor need not opt in or share its material. Contact relief is independent of texture blending; covered portions of an overlapped shape do not render through the visible shape. Holes retain inward-facing borders.

Bevel normals use a bounded signed-distance transform on the existing2px authored coverage, with bilinear sampling and adjustable gradient smoothing. Distances include neighboring chunks. Texture detail normals are reoriented over the generated bevel normal. The outside silhouette and flat interior remain unchanged; this is visual relief, not extrusion or new physics.

A static level profile supplies illumination. Where none can be evaluated, the editable registered **Bevel-only fallback** profile and its directional light supply bevel lighting only. It does not simulate calendar light. Unlit materials retain their interior color and apply lighting relative to a flat face on the bevel band. A project lacking both a static profile and the fallback receives an explicit refusal; projects are not migrated implicitly.

The complete document is capped at8M distance samples for a full rebuild, even during incremental edits; wide or numerous effects may exceed this. Work preflights before visual uploads. Cache refresh, failed edits, history and reload retain a consistent document/visual state. The native `--lab bevel-check` exercises contour direction, holes, contact modes, chunk seams, budget refusal, GPU/reference shading, detail normals, fallback undo, collision ownership and real picker interaction. This does not claim physically accurate bevels, cast shadows or sustained performance.


## Shared environment boundary

The read-only environment snapshot in `src/environment/environment.h` copies the existing session generation, level, clocks and weather. It exposes continuous day phase, authored-day annual phase and a smooth equal-month weather seasonal phase separately. The latter is raw, before Weather applies the southern half-year shift; the existing Weather selector still uses whole seconds and retains its behavior. Atmosphere means effective level weather before indoor suppression, retaining level modifiers and external overrides. Local weather retains indoor masking. Queries do not create sessions or advance time. Unavailable/error results contain initialized output and a diagnostic without cancelling current modal/UI state.

`src/environment/projection.hpp` derives camera-independent celestial directions and wind inputs from that snapshot. Celestial axes are east +X, up +Y, north +Z. Wind vectors retain world/screen X right and Y down. Calendar time drives celestial phase; simulation time is the supplied animation clock for future clouds/cards/waves. UI and wall-clock time are not substitutes.

The configurable cycle is a stylized projection over an arbitrary authored year, not an Earth ephemeris. The first proof uses an explicitly antipodal moon without lunar phase/orbit. Normal-map lighting elevation means angle out of the surface plane, so it cannot consume astronomical altitude directly. The opt-in sky/cloud pass now consumes this projection. Calendar terrain-light binding, multiple additive lights, shadows and gameplay illumination queries remain separate work. Existing static lighting, terrain geometry and material caches are unchanged. Signed: President.


## Sky profiles and horizon

A `sky_profile` (`sky:` ID) supplies a cyclic2–16-stop sRGB palette, the shared celestial cycle, sun/moon appearance, bounded four-octave procedural clouds and weather appearance mappings. Cloud maps consume effective level atmosphere before indoor masking; they do not select or advance weather. Night cloud brightness is authored separately. The initial moon remains an explicit antipodal approximation without lunar phases. View azimuth controls the stylized horizontal projection; altitude affects the view only.

Levels opt in through `properties.sky`, containing `profile`, `origin_y_px`, `px_per_m` and `visible_indoors`. Missing/null attachments or a null profile preserve previous behavior. The signed-world course explicitly uses native origin4096 and32 pixels/metre; legacy scenes can supply a different origin. Parent levels do not inherit sky settings. Enabling a minimal disabled attachment through Forms fills only missing standard fields and retains unknown properties.

The default horizon ratio is **0.01 (1%)**:1 metre of horizon motion for100 metres of foreground movement. It is stored as `horizon.altitude_parallax` and displayed as a percentage in Forms. Positive camera altitude moves the horizon down. The reference horizon is a viewport-height fraction; altitude displacement uses pixels before normalization, so resizing retains the same displacement in pixels. Explicit minimum/maximum fractions bound the view. This supersedes the earlier provisional percent interpretation.

The reserved pass renders after the render queue clears and before every authored background. Transparent artwork reveals it; opaque artwork conceals it. Drawing restores target, shader, transforms, clipping, blend, depth/write mask and bitmap-hold state. Shader failure retains a nonfatal gradient fallback with a log diagnostic. Unavailable environment input leaves normal backgrounds intact and reports that the sky is waiting.

Cloud phase integrates canonical simulation-time deltas. Changing wind never resets phase; pause freezes it, and generation changes/rewind safely reset its local presentation state. This phase is not another clock and does not enter player saves. Four-octave sampling is fixed and bounded; this proof makes no sustained rendering-performance or physical-atmosphere claim.

Background layer templates/composition tools, stars, lunar phase/orbit, multiple cloud layers, cast shadows and sky-driven terrain lighting remain later work. Signed: President.
