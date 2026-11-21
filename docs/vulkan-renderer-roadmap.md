# Vulkan Renderer Roadmap

Living tracking doc for bringing `code/renderer_vulkan` up to, and then past, vkQuake3-level
parity, with the end goal of matching `code/rend2`'s visual feature set. Update this file
every session that touches the Vulkan renderer: check off finished items, add new findings,
and append a dated entry to the Changelog at the bottom. Don't let this doc drift from the code.

Reference implementations used for this plan:
- `vkQuake3-master` (external reference checkout) — baseline "classic id-tech-3 in Vulkan" target.
- `code/rend2` (this repo) — the OpenGL2 advanced renderer; the "beyond vkQuake3" target.
- `code/renderer` (this repo) — classic OpenGL1 renderer; shares shader-script semantics with
  the Vulkan renderer and is the easiest cross-check for "does this shader keyword do anything."

## Current state (last updated 2026-11-20 — Phase 0 complete, most of Phase 1 + first Phase 2 item done)

The Vulkan renderer is **real and live**, not a dead stub: `cl_renderer` (`code/client/cl_main.c`)
dynamically loads `renderer_vulkan_<arch>.<dll|so>`, it builds by default
(`BUILD_RENDERER_VULKAN` in the `Makefile` defaults to building), and selecting
`cl_renderer vulkan` + restart actually renders the game today. Beyond Phase 0's baseline (world
geometry with lightmaps, frustum/PVS culling, curved patches, MD3/MDC/MDS/MDM model loading + CPU
skinning, tags/attachments, `.skins` files, ambient light-grid sampling, mipmapping + anisotropic
filtering, gamma correction, screenshot capture, `r_showtris`/`r_showImages` debug overlays,
view-weapon `RF_DEPTHHACK` depth-range compression), it now also has: real per-frame `rgbGen`/
`tcMod` on every world stage (no more baked stage-0-only animation), overbright bits, multitexture
collapse pipelines for entities/polys, dynamic lights as a real additive world pass, mirror/portal
clip-plane rendering (world surfaces only) and sky-portal (`_skyboxOrigin`) support, brush-model
entities (func_door/func_rotating/func_plat/etc. — previously invisible), and decals (bullet
holes/blood, with `polygonOffset` support to stop them z-fighting). Two load-bearing but easy-to-miss
model/skin-loading bugs (missing cross-extension fallback in `RE_RegisterModel`, a skin-file comma
parser that didn't consume its own separator) were also found and fixed — both were silently
breaking enemy character models/skins regardless of which later phase landed.

It's still well behind both reference points on everything not yet checked off below:
- **vs. vkQuake3**: no coronas/flares, no stencil shadows, no cinematics, frame pacing is
  single-buffered (no frames-in-flight), resize does a full `vid_restart` instead of an in-place
  swapchain rebuild, no real BSP volumetric fog, and `deformVertexes` beyond `move` is blocked on
  Phase 4's vertex-normal work. Mirror/portal rendering is world-geometry-only (entities/polys/sky
  draw unclipped inside a mirror) and hasn't been visually confirmed in-game yet.
- **vs. rend2**: no normal/specular/parallax/deluxe maps, no per-pixel lighting (entities get one
  flat RGBA tint for the whole model, world gets baked lightmap only), no tangent/normal data in
  the vertex formats at all, no HDR/bloom/tonemap, no SSAO/FXAA/DOF, no cubemap reflection probes,
  no shadow mapping.
- **Open/unverified from the most recent session**: the "shattered" view-weapon/held-item geometry
  report has two fixes built now (shared-buffer-cursor persistence, then per-scene entity/dlight/poly
  windowing — see Changelog below); retesting after the first fix alone surfaced some weapons
  rendering invisible (FG42 body, scope still visible), which the second fix addresses, but neither
  has been confirmed in-game yet. Separately, the user reports the skybox portal itself renders
  nothing (pitch black plus the raw render-pass clear color showing through where sky should be) —
  not yet root-caused; `r_vk_debugSky` diagnostic logging (`tr_world.c`'s `R_MarkVisibleSurfaces`,
  `vk_sky.c`'s `vk_sky_draw`) was added to investigate it, pending a log capture. A
  `r_vk_disableMultiTex` bisection cvar from an earlier (inconclusive) attempt at the shattered-weapon
  bug is still sitting in `vk_pipelines.c`, undocumented until now — safe to leave (off by default,
  no `sv_cheats` requirement) but should be removed once that investigation is fully closed out. The
  `r_vk_debugWeapon` and `r_vk_debugSky` diagnostic cvars/logging should be removed once their
  respective reports are confirmed fixed.

Full raw findings from the original audit are folded into the phases below; this doc is the actionable
version, not a dump of the investigation.

## Architecture decisions to make explicitly (don't let these happen by accident)

1. **Optional material features (normal/spec/parallax/deluxe maps) → uber-shader + UBO/push-constant
   flags**, not a pipeline permutation per combination. rend2 proves this is viable on the GL side
   via `u_EnableTextures`-style bitmasks; Vulkan should bind dummy/white fallback textures to
   unused descriptor slots and branch in-shader. Reserve pipeline permutations for things that
   actually change fixed-function state (blend mode, cull, depth, stencil, major lighting *path*).
2. **Major lighting-path selection (lightmap vs. light-vector vs. vertex-lit, × vertex-lerp anim, ×
   shadow-receive) → precompiled SPIR-V permutation table**, mirroring rend2's 128-permutation
   `LIGHTDEF_*` GLSL approach almost directly, with lazy `VkPipeline` creation cached by bitmask
   (don't compile all 128 up front). This is a direct, low-risk translation of a design already
   proven to work for this exact feature set — don't invent a new dynamic-branching scheme here.
3. **CPU skinning now, GPU skinning later, batched with the rend2 vertex-format work.** The
   current CPU skinning (MD3/MDC/MDS/MDM) works and should not be touched until Phase 4, where
   bone-index/weight vertex attributes are being added anyway for rend2 parity — do the GPU-skinning
   migration once, not twice.
4. **Stencil shadow volumes (Phase 2, vkQuake3-style) vs. pshadows (Phase 5, rend2-style) are two
   different features, not sequential replacements** — rend2 uses projected per-entity shadow
   blobs sampled from an offscreen target, *not* cascaded sun shadow maps and *not* stencil
   volumes. Decide once Phase 5 is reached whether to keep both behind `r_shadows` modes (matching
   upstream cvar semantics) or drop stencil shadows in favor of pshadows for the final build.
5. **Verify the frontend/backend split before Phase 1 gets large.** vkQuake3 and the classic
   renderer funnel all drawing through a typed render-command buffer
   (`SET_COLOR`/`STRETCH_PIC`/`DRAW_SURFS`/`SWAP_BUFFERS`, built in `tr_cmds.c`/`render_export.c`)
   so the frontend never touches the graphics API directly. Confirm `renderer_vulkan`'s
   `tr_scene.c`/`tr_main.c` follow the same contract (it should, since it shares `tr_public.h` with
   `code/renderer`) rather than calling `vk*` functions straight from frontend code — if it
   diverges, fix it before building more on top, since it'll make threaded frontend/backend
   execution and multi-scene-per-frame cases much harder to retrofit later.

## Phase 0 — Correctness & verification groundwork

Quick, high-value fixes and the tooling needed to visually verify every later phase. Do this
first; several later phases are hard to validate without it.

- [x] ~~Remove or `cvar`-gate the hardcoded debug test cube and entity-origin markers~~ — re-checked
      `vk_draw3d.c`: both `r_vk_test3d` and `r_vk_showEntityMarkers` already default to `"0"` and the
      draw calls bail out early when unset. The original audit was wrong on this one; no change needed.
- [x] Implemented screenshot capture (`screenshot`/`screenshotJPEG`): new `vk_screenshot.c` copies the
      just-presented swapchain image back via a one-shot staging buffer (added `VK_IMAGE_USAGE_TRANSFER_SRC_BIT`
      to the swapchain in `vk_swapchain.c`) and writes TGA or JPEG (reusing `tr_image_jpg.c`'s existing
      `RE_SaveJPG` encoder). Runs after `vk_end_frame()`, not inside the frame's own command buffer.
- [x] Implemented wireframe overlay (`r_showtris`) and texture-atlas dump (`r_showImages`).
      `r_showtris` covers **world geometry only** (a new `vk.pipeline_world_wireframe`,
      `VK_POLYGON_MODE_LINE`, gated on `vk.features.fillModeNonSolid`) — entity wireframe is a
      follow-up, not done here. `r_showImages` replaces the scene with a 40-column grid of every
      loaded texture (`vk_draw2d_raw_image`, bypassing the shader system).
      **`r_shownormals` is deliberately not implemented**: it needs per-vertex normals, and neither
      the world (`vkWorldVertex_t`) nor entity (`entityBaseVert_t`) vertex formats carry any today —
      that's Phase 4's vertex-format work. Implementing it now would mean faking data; skip it until
      real normals exist.
- [x] Mipmapping + anisotropic filtering: `vk_image.c` now generates a full mip chain via
      `vkCmdBlitImage` at upload time (skipped when `IMGFLAG_NOMIPMAP` is set — already wired through
      from `RE_RegisterShaderNoMip`) and the sampler uses `mipmapMode = LINEAR` with
      `maxLod = mipLevels-1`. Anisotropy turned out to already be *enabled on the device* (the device
      creation code in `vk_instance.c` enables every supported feature, not none); the sampler just
      wasn't using it — now `anisotropyEnable`/`maxAnisotropy` come from `vk.features.samplerAnisotropy`
      and the newly-queried `vk.deviceProperties.limits.maxSamplerAnisotropy`. Went straight to the
      GPU blit approach rather than a CPU box-filter stopgap.
- [x] Fixed `animmap`: `tr_shader.c` now parses and stores every frame
      (`shaderStage_t.animImages[]`/`numAnimImages`/`animFrequency`, capped at `MAX_IMAGE_ANIMATIONS`
      16, matching `code/renderer`), and a new `R_GetStageImage()` in `tr_shade_calc.c` resolves the
      current frame from shader time. Wired into entities, scene polys, and 2D pics (`vk_entity.c`,
      `vk_poly.c`, `vk_draw2d.c`). **World surfaces still only show frame 0** — their descriptor sets
      are built once at level-load time (see the multi-stage fix below), so per-frame animated world
      textures are still blocked on that static-descriptor architecture.
- [x] Fixed `R_ModelBounds`: returns frame-0 local-space bounds for MD3/MDC/MDS/MDX (same semantics as
      `code/renderer`/`code/rend2`). MDM has no bounds data of its own (mesh+tags only, no frames —
      bounds live on its separate MDX skeleton) and still returns zero; that's an honest gap, not a bug.
- [x] Baseline gamma/brightness correction: added `r_gamma` (`"1.4"`, `CVAR_ARCHIVE | CVAR_LATCH`,
      matching `code/renderer`/`code/rend2`'s default) and a CPU gamma table applied to every texture's
      pixels at upload time in `vk_image.c` (one choke point: both `R_CreateImage` and `vk_world.c`'s
      direct lightmap uploads funnel through `vk_create_texture_image`, so this covers regular
      textures, fonts, *and* lightmaps uniformly). `CVAR_LATCH` because a changed value only affects
      textures loaded afterward — there's no live postprocess pass yet, as planned. Phase 5's final
      pass still supersedes this later.
      **Correctness trap hit and fixed here, worth remembering for any future per-pixel work on
      upload buffers**: the first version ran the gamma lookup *in place* on the staging buffer's
      mapped pointer — `HOST_VISIBLE` GPU-upload memory, which is commonly write-combined/uncached.
      That's fine for the plain sequential `memcpy()` this code used to do, but the lookup
      (`pixels[i] = table[pixels[i]]`) requires *reading* that memory back first, and reads from
      write-combined memory can be dramatically slower than normal RAM — this alone turned into a
      ~50ms-per-texture regression (×hundreds/thousands of textures = the "way slower after Phase 0"
      report), independent of mipmapping, descriptor allocation, or sync, all of which were red
      herrings chased before isolating this with per-sub-phase timing. Fixed by gamma-correcting
      *while copying* from the normal-RAM source into the staging buffer (`R_CopyWithGammaCorrection`
      in `vk_image.c`), so the GPU-visible memory is only ever written, never read. **Rule for later
      phases**: any future per-pixel transform on upload data (color-space conversion, compression,
      etc.) must read from the CPU-side source and write to the mapped GPU buffer in one pass — never
      read-modify-write in place on the mapped pointer.
- [x] Fixed the world-surface multi-stage bug: `vk_world_draw` now loops a shader's stages 1..N after
      drawing stage 0's baked diffuse×lightmap pass, drawing each extra stage with its own blend
      state/texture via a generalized `vk_getPipeline3D` (now takes a `vertexStride` so it can target
      either `vk2d_vertex_t` or `vkWorldVertex_t`). Known limitation carried over, not fixed here:
      extra stages share stage 0's baked vertex color (correct for the common `rgbGen identity`
      case, wrong for a stage with its own distinct `rgbGen`) and reuse stage 0's static UVs (no
      per-frame `tcMod` on extra stages) — both are explicitly Phase 1's job (the generic multi-stage
      iterator), not re-solved here.
- [x] View-weapon depth hack (`RF_DEPTHHACK`, flagged by the user: "my gun drowns in walls, never
      happens in OpenGL1"): this renderer had *zero* handling of the flag at all. code/renderer
      compresses the depth range to `glDepthRange(0, 0.3)` for depth-hacked entities so the weapon
      always wins the depth test against nearby geometry; ported as a per-draw-call
      `vkCmdSetViewport` switch in `vk_entity.c` (`SetDepthRange`), threaded through the existing
      vertex/index batching (`entityDrawCall_t`/`entityMTDrawCall_t` now carry a `depthHack` flag
      set from `ent->renderfx` at commit time, since draws are batched across entities and emitted
      later, not issued per-entity) with change-tracking so the viewport only gets reset when the
      flag actually flips, and restored to normal before returning so polys/2D HUD aren't affected.
      **Not ported**: code/renderer also swaps to a flatter projection matrix for depth-hacked draws
      (reduces visual distortion on the weapon model specifically) — a separate, more cosmetic
      concern from the reported bug (geometry disappearing into walls), left as a follow-up. Not yet
      confirmed in-game.

## Phase 1 — Generic multi-stage shader/draw core

The architectural centerpiece vkQuake3 calls `vk_shade_geometry.c` / `RB_StageIteratorGeneric`.
WolfTech's Vulkan renderer currently has per-caller, ad hoc stage loops (world draws stage 0 only;
entities/polys loop stages correctly but independently) instead of one shared generic path. Build
it once, make world/entity/poly submission all go through it.

- [x] Fixed `tcMod entityTranslate`: threaded `refEntity_t.shaderTexCoord` through
      `R_CalcStageTexCoord` (new parameter, `NULL` for world/poly which have no entity context) and
      into `vk_entity.c`'s `WriteStageVertices`. Scrolls correctly for entity surfaces now (e.g.
      conveyor-belt-style shaders); world/poly shaders using `entityTranslate` still see no offset,
      which matches upstream semantics (it's inherently an entity-only tcMod).
- [x] Overbright bits: added `R_ShiftLightingBytes` (`tr_light.c`/`.h`), a direct port of
      `code/renderer`'s `R_ColorShiftLightingBytes` minus the gamma call (gamma happens separately
      at Vulkan texture-upload time, see Phase 0). `r_mapOverBrightBits` defaults to `"2"`, matching
      `code/renderer`. Applied at BSP load time to: vertex colors (`tr_bsp.c`'s `R_LoadDrawVerts`),
      light-grid ambient+directed samples (`tr_bsp.c`'s `R_LoadLightGrid`), and lightmap texels
      (`vk_world.c`'s `BuildLightmapTexture`) — the same three places the reference engine shifts.
- [ ] `deformVertexes` — **investigated, mostly blocked, not faked**:
      - `wave`/`bulge`/`normal` all displace a vertex along its own normal
        (`xyz[i] += normal[i] * scale` in `code/renderer/tr_shade_calc.c`'s
        `RB_CalcDeformVertexes`/`RB_CalcBulgeVertexes`/`RB_CalcDeformNormals`) — blocked on the same
        missing per-vertex normal data as `r_shownormals` (Phase 0) and dynamic lighting (below).
        Don't implement until Phase 4's vertex-format work lands.
      - `autosprite`/`autosprite2` billboard a whole **quad** (both triangles, 4 shared corners) to
        face the camera — they need the primitive's other 3 corners and their shared-edge topology,
        not just one vertex. `R_DeformPosition`'s signature (`shader, basePos[3] → outPos[3]`) is
        fundamentally per-vertex and can't express this; it needs a per-primitive calling
        convention instead. That's exactly the kind of restructuring the "shared multi-stage
        iterator" item below would naturally introduce — fold autosprite into that work rather than
        bolting a separate quad-aware path onto the current per-vertex one.
      - `projectionShadow` (cheap "blob" shadow, no stencil buffer needed) is feasible without
        normals — it needs the entity's local axis, origin, `shadowPlane`, and a light direction
        (`code/renderer/tr_shadows.c`'s `RB_ProjectionShadowDeform`) — but `R_DeformPosition` has no
        entity context today, and the coordinate-space assumptions (what space `backEnd.or`/`tess.xyz`
        are in relative to this renderer's model-local CPU-side vertices) need careful verification
        this session couldn't do (no way to visually confirm a shadow lands in the right place).
        Pair this with Phase 2's real stencil shadows so both get tested together.
      - `text0`-`text7` (in-world floating text, e.g. killer-tomato-style effects) needs a
        `refdef.text[]` data path this renderer doesn't have. Rare usage; lowest priority.
- [x] Shared multi-stage iterator (world side): **rewrote `vk_world.c`**. World rendering no longer
      bakes a static GPU vertex/index buffer at map load. `vk_world_build` now only sorts/batches
      surfaces and caches tessellated (patch) geometry as plain CPU arrays (`s_cachedVerts`/
      `s_cachedIndices` — load-time cost, unchanged). Every frame, `vk_world_draw` expands each
      *visible* surface's cached geometry into a per-frame dynamic vertex buffer, calling the same
      `R_CalcStageColorAlpha`/`R_CalcStageTexCoord` entities and polys already use, so `rgbGen`/
      `tcMod` (scroll, rotate, turbulent, transform, stretch, `entityTranslate`) now animate on world
      surfaces exactly like they do on entities. Two dynamic buffers: `s_dynLMBuffer`
      (`vkWorldVertex_t`, stage 0's diffuse×lightmap pass) and `s_dyn2DBuffer` (`vk2d_vertex_t`,
      stages 1+ — these now get their *own* independent `rgbGen`/`tcMod` instead of sharing stage
      0's baked data, fixing the limitation noted in Phase 0). Both are sized from
      `s_numCachedIndices` (a hard per-map upper bound, known at load time) with overflow protection
      (warn-once-and-drop, matching this codebase's existing style) rather than an unbounded/dynamic
      allocation. Contiguous-visible-surface draw-call merging is preserved (same technique as the
      old `DrawSurfRefRun`, just merging dynamic-buffer *writes* instead of static index ranges), so
      draw-call count shouldn't regress — only added CPU cost is the per-visible-vertex `rgbGen`/
      `tcMod` evaluation itself. **Not yet performance-tested on a real map** — user explicitly
      accepted this risk and will test frame time after. If it's too slow, the two likely fixes are
      (a) skip recomputation for stages whose `rgbGen`/`tcMod` are provably static (identity/none)
      and cache those like before, or (b) partially revert to static-with-exceptions (only dynamic
      for shaders that actually have animating `tcMod`/non-identity `rgbGen`).
      `deformVertexes` was *not* added to world surfaces in this pass (world never called
      `R_DeformPosition` before; out of scope here, tracked separately above).
- [x] Real multitexture pipelines for entities/polys: new `multi_texture.vert`/`.frag` shader pair
      (`layout(constant_id=0) combine_mode`: 0=modulate/"filter" `GL_DST_COLOR,GL_ZERO`, 1=add
      `GL_ONE,GL_ONE`; `constant_id=1` carries stage 0's `alphaTestFunc`) plus `vk_getPipelineMultiTexture`/
      `vk_getMultiTextureDescriptor` lazy caches in `vk_pipelines.c` (the latter allocates from its own
      256-set pool, `vk.lightmap_set_layout` reused as-is: binding 0/1 = texture0/texture1). `vk_canCollapseMultiTex`
      (`vk_pipelines.c`/`.h`) gates it: shader must have *exactly* 2 stages, stage 0 opaque, stage 1
      blended with trivial (`CGEN_IDENTITY`) color and one of the two blend modes above — the collapsed
      shader only ever samples stage 0's color (see `multi_texture.frag`), so this must stay false
      whenever stage 1's own tint could actually show. `vk_entity.c` and `vk_poly.c` each got a second
      vertex buffer (`vkMultiTexVertex_t`: pos/color/uv0/uv1) and branch per-surface/per-poly into the
      collapsed single-draw path or the existing per-stage path. **Deliberately not touched: `vk_world.c`**
      — piling more complexity onto the just-rewritten (Phase 1, static→dynamic) world path before it's
      stabilized wasn't worth it; world stage 0+lightmap already gets one draw via `world_lightmap`
      anyway, so the remaining win there (collapsing world stage 1+ pairs) is smaller. Compiles clean
      debug+release; not yet visually confirmed in-game (look for a shader using a modulate/add filter
      stage on a model or a scene decal/poly to verify combine_mode looks right).
- [x] Dynamic lights as a real additive pass **for world geometry**: world surfaces had *zero*
      dlight effect before (dlights only ever reached entities, via the flat-tint hack). Added
      `ExpandAndDrawWorldDlights` in `vk_world.c` — a third dynamic buffer, drawn after the normal
      per-batch loop only when `R_GetSceneDlights` returns `numDlights > 0` (no cost when nothing's
      exploding/firing). Per-vertex falloff uses the exact same formula as `vk_entity.c`'s
      `ComputeEntityTint` (not true projected-texture dlights like vkQuake3's `ProjectDlightTexture`
      — a scoped simplification: real per-vertex distance falloff, diffuse-texture-modulated,
      additively blended on top of the lit surface, just not a radial-blob projection). Entities
      still use the pre-existing flat-tint hack (unchanged) — replacing that with its own real
      additive pass is a separate, larger change (would need splitting `ComputeEntityTint`'s
      ambient/directed base lighting from a per-dlight overlay with its own draw call) and is still
      open below.
      Dynamic lights as a real additive pass **for entities** (replacing the flat-tint-per-entity
      hack) is still open — also depends on entities carrying normal data for a believable result
      (tracked formally in Phase 4; a coarse per-face normal would be enough to unblock this).
- [ ] Real fog pass: port BSP-brush volumetric fog. **Investigated this session**: this is a
      genuinely separate system from the wolf-fog already ported — RTCW keeps *both* the classic
      id-tech-3 `fogParms`-shader-keyword + BSP fog-brush-volume system (used for things like
      underwater boxes) and its own `trigger_fog`/`target_fog` global system (already done, Phase
      0-adjacent). Real BSP fog needs: parsing `fogParms` in `tr_shader.c` (currently parsed and
      discarded), loading the BSP's fog lump, tagging each world surface with a `fogIndex`, and
      determining which fog volume (if any) the camera is currently inside each frame. Scoped as
      its own item, not a quick add.
- [x] Portal/mirror clip-plane pipeline variants, with the same graceful fallback vkQuake3 uses for
      GPUs lacking `shaderClipDistance` (discard-in-fragment-shader via specialization constant).
      **Turned out to need far more than a pipeline variant**: this renderer had *zero* mirror/portal
      scaffolding going in (no shader `sort`/`isPortal` parsing, no `RT_PORTALSURFACE` handling, no
      recursive view-render, no `viewParms_t` concept at all — `RE_RenderScene` was one flat
      single-pass function). Built, in order: (1) `shader_t.isPortal`/`portalRange` + `portal`/`sort
      portal` keyword parsing (`tr_shader.c`); (2) new `tr_portal.c`/`.h` — ports
      `R_SetupPortalPlane`/`R_GetPortalOrientations` from vkQuake3/code/renderer (GPLv2-or-later),
      trimmed to *world surfaces only* (an entity-attached portal surface, e.g. a handheld mirror,
      isn't handled — see the file header); (3) `RE_RenderScene` split into a thin wrapper +
      `R_RenderViewInternal` (`tr_main.h`), callable recursively so a found portal/mirror renders its
      destination view into the same target before the real one; (4) `vk_world_findVisiblePortalSurface`
      (`vk_world.c`) to locate a visible `isPortal` surface's plane from its cached first triangle;
      (5) backend clip-plane support added to `single_texture`/`world_lightmap`'s shaders (a
      `clipPlane` vec4 reusing 16 bytes of push-constant padding that were already zeroed, not a size
      increase) plus new `*_clip.vert` hardware-`gl_ClipDistance` twins, selected by
      `vk.features.shaderClipDistance` exactly like vkQuake3; (6) `vk.pipeline_3d_lightmap_portal/_mirror[cullType]`
      eager variants (mirror also flips cull mode) plus `clipPlane`/`mirror` params on
      `vk_getPipeline3D`, so world stage 1+/dlight draws and (if ever passed `qtrue`) entity/poly
      draws can opt in. **Scoped out, documented, not a silent gap**: entities/polys/sky are drawn
      unclipped inside a mirror view (only world geometry clips) — call sites in `vk_entity.c`/
      `vk_poly.c` pass `VK_FALSE, VK_FALSE` explicitly; an entity-attached (moving) portal surface;
      `deformVertexes projectionShadow`'s pairing with real shadows (unrelated, already tracked under
      Phase 1's deform item). The world-space clip-plane math (test done directly in world space,
      not vkQuake3's eye-space one — proven equivalent in a comment in `world_lightmap.vert`) and the
      plane's sign/orientation are derived, not yet visually confirmed in-game — if a mirror shows the
      wrong half of the scene (or none of it), the fix is a one-line sign flip in
      `R_TryRenderPortalView`'s `s_portalNormal`/`s_portalDist` setup (`tr_portal.c`). Compiles clean;
      not yet tested against a real mirror/portal shader in a map.
- [x] Sky-portal (`RDF_SKYBOXPORTAL`/`RDF_DRAWSKYBOX`, the `_skyboxOrigin` worldspawn key's distant
      "sky room") — **a completely different mechanism from the mirror/portal surfaces above**,
      flagged by the user as something they rely on for their skybox and folded into this pass once
      confirmed unrelated to (and much smaller than) the clip-plane work. It's just two
      back-to-back `RE_RenderScene` calls per frame from cgame's `CG_DrawSkyBoxPortal`: a small
      "sky room" scene first, then the real scene on top *without re-clearing color* so the sky
      room's painted background survives wherever the real scene draws nothing. Added: a sticky
      `R_SkyboxPortalActive()` flag (`tr_scene.c`, reset per `RE_LoadWorldMap`) set once a scene has
      both `RDF_SKYBOXPORTAL` and `RDF_DRAWSKYBOX`; `R_RenderViewInternal` then skips the real
      scene's own sky draw (the sky room already painted it) and does a mid-render-pass
      `vkCmdClearAttachments` depth-only clear (new `vk_clear_depth_only`, `vk_frame.c` — our render
      pass only clears at load, once per frame, unlike classic GL's per-call `glClear`) before the
      real scene's own draws. Deliberately conservative: the sticky flag requires *both* flags (not
      just `RDF_SKYBOXPORTAL` alone) specifically so a map with a `_skyboxOrigin` key but
      `cg_skybox 0` can't accidentally suppress its own normal sky — unverified against upstream's
      exact behavior in that edge case, but safe by construction. Compiles clean; not yet tested
      against a map that actually sets `_skyboxOrigin`.

- [x] Brush-model entities (func_door/func_rotating/func_plat/func_train/etc.): **found by the user as
      "doors are invisible," root cause was a complete gap this audit missed entirely, not a rendering
      bug** -- `tr_model.c`'s `RE_RegisterModel` had no case for inline-BSP-submodel names (`"*N"`), so
      every brush entity registered as `MOD_OTHER` (empty placeholder). Even if it had, `tr_world.c`'s
      `R_MarkVisibleSurfaces` only walks the static world's BSP leaf tree, which never references
      brush-entity surfaces at all -- they'd be permanently marked invisible regardless. Fixed: added
      `MOD_BRUSH` (`tr_model.h`/`.c`, stores a `tr_world.submodels[]` index) and a new
      `vk_world_draw_brushmodel` (`vk_world.c`, dispatched from `vk_entity.c`'s `MOD_BRUSH` case) that
      transforms each surface in the submodel's range by the entity's current origin/axis every frame
      (own dynamic buffers + a lazy shader/lightmap descriptor cache, since a door's geometry changes
      position every frame unlike the static world's load-time cache) and draws it through the same
      lightmap/multi-stage machinery as static world surfaces -- brush entities keep their baked
      lightmap/vertex color rather than sampling the light grid, which is real id-tech-3 behavior (bmodels
      never queried the grid), not a shortcut. Also restricted `vk_world_build`'s static-world batching to
      submodel 0's surface range only (it was previously scanning *all* surfaces including brush-entity
      ones, harmlessly dead-batching them since they were never visible -- now moot, but correct either
      way) and added the `MOD_BRUSH` case to `R_ModelBounds` (`tr_init.c`) for completeness. Compiled
      clean (full `mingw32-make` release build under MSYS2 MINGW64, zero new warnings). **Not yet
      confirmed in-game** -- next step is opening a map with a func_door and func_rotating and checking
      both render and animate correctly; patches-on-brush-models are untested (rare in practice, most
      door/plat brushes are planar).

## Phase 2 — Classic feature parity (decals, flares, shadows, cinematics)

- [x] Decals (`R_MarkFragments`, previously always returned 0): new `tr_marks.c`/`.h` ports
      `R_ChopPolyBehindPlane` verbatim from code/renderer (GPLv2-or-later). Everything else is
      simplified rather than ported 1:1, since `bspSurface_t` is already uniformly pre-triangulated
      (no `SF_FACE`/`SF_GRID`/`SF_TRIANGLES` split to dispatch on) and surfaces aren't walkable via a
      node tree the way classic `R_BoxSurfaces_r` needs: candidate surfaces are found with a flat
      linear scan over `tr_world.surfaces` plus a per-surface AABB-vs-decal-bounds reject, and output
      is always upstream's "old mapping" convention (raw xyz only, no fitted ST) — cgame's
      `CG_ImpactMark` already computes a perfectly fine planar UV itself when a fragment's
      `numPoints` comes back positive, so this isn't a hack, just always taking the simpler of
      upstream's two legitimate modes. **Scoped out, not a silent gap**: `MST_PATCH` surfaces (curved
      geometry) don't get decals — patches store raw control points here, not triangles, and
      tessellating them too (like `vk_world.c` does for drawing) is a real follow-up, not done this
      pass; the classic engine's per-node "recently rendered" leaf freshness filter and
      `CONTENTS_TRANSLUCENT` leaf check are also dropped (the flat scan has no leaf context to check
      either against) — worst case is a slightly wider candidate-surface set, not a correctness bug.
      No new Vulkan draw code needed: decals submit through the existing `RE_AddPolyToScene`/poly
      draw path once `R_MarkFragments` returns real fragments.
      **In-game testing found two real bugs, both fixed**: (1) the 4 side-clip-plane normals came
      out pointing outward instead of inward — the cross-product formula matches code/renderer's
      literally, but something about how it transfers here flips the sign (not fully root-caused;
      see the git history for the debug-logging session that caught it: every candidate triangle
      was being clipped to zero points). Fixed by making the planes self-correcting: each one is
      flipped if the quad's own center ends up on its wrong side, rather than trusting the
      cross-product order. (2) Working decals then z-fought/flickered against the coplanar wall
      underneath them, because this renderer had no `polygonOffset` support at all
      (`VkPipelineRasterizationStateCreateInfo.depthBiasEnable` was hardcoded off everywhere). Added
      `shader_t.polygonOffset` (parses the `polygonOffset` shader keyword) and a `depthBiasEnable`
      pipeline-variant axis on `vk_getPipeline3D` (new `r_offsetfactor -1` / `r_offsetunits -2`
      cvars, matching code/renderer's defaults) — entity/poly/world-stage1+ draws all pick it up for
      free from their shader's own flag. Confirmed working in-game by the user (bullet holes and
      blood decals both render); not yet re-confirmed that the polygon-offset fix fully eliminates
      the flicker (added right after the report, not separately re-tested in isolation yet).
- [ ] Dynamic light coronas/flares (`RE_AddCoronaToScene`, currently stubbed): visibility-test
      against the depth buffer (occlusion query via `VkQueryPool`, or a depth readback/compare) with
      multi-frame fade in/out to avoid popping.
- [ ] Stencil shadow volumes (`r_shadows 2`): silhouette edge extraction, two-pass front/back-face
      stencil draw, full-screen darkening finish pass; plus the cheap `projectionShadow` blob
      fallback via `deformVertexes projectionShadow` (Phase 1 item). Needs stencil ops enabled in
      the pipeline (`stencilTestEnable` is currently hardcoded off).
- [x] Validated curved-patch (Bezier) tessellation and found a real bug, confirmed in-game by the
      user: "arches that are supposed to be curved render triangular/faceted" — `r_subdivisions`
      is `CVAR_ARCHIVE` in the reference renderer (`code/renderer/tr_init.c`), so it's saved in the
      user's config from prior OpenGL1/rend2 play and shared across all three renderer DLLs. The
      reference (`code/renderer/tr_curve.c`'s `R_SubdividePatchToGrid`) treats that value as a
      **world-unit curvature error tolerance** — adaptively subdividing each patch until its actual
      deviation from a flat chord is under that many units, so flat patches get collapsed to no
      extra subdivision and sharply curved ones get many. `tr_patch.c` instead treated the same
      number as a **literal fixed segment count**, identical for every patch regardless of
      curvature — so a typical saved value like `"4"` (plenty smooth under the reference's
      tolerance semantics, which could mean 8-16+ actual segments for a wide arch) produced exactly
      4 straight segments no matter how sharply the patch curved, visibly faceting any wide/tight
      arch. Fixed by porting the reference's actual curvature measurement (`Sagitta()`, the t=0.5
      Bezier midpoint's perpendicular deviation from its chord — identical formula to
      `tr_curve.c`'s midpoint check) into a new `R_ComputeAdaptiveSubdiv()`, which measures the
      worst-case curvature anywhere in a patch's control grid and derives a subdivision level from
      it (quadratic Bezier error roughly quarters per binary split, so `segments = 2^k` for the
      smallest `k` keeping worst-case error under tolerance), clamped to the same `[1,16]` range
      `r_subdivisions` was already clamped to. `R_PatchTessVertCount`/`R_PatchTessIndexCount` now
      take the control-vert array too (not just the dimensions), so their vertex/index counts stay
      consistent with what `R_TessellatePatch` actually emits — all three call sites (`vk_world.c`,
      3 places: the static-world sizing pass, `EmitSurfaceGeometry`, and
      `vk_world_draw_brushmodel`'s patch branch) updated to match. **Known gap, not fixed here**:
      the subdivision level is now picked *per patch surface*, uniform within it (so internal biquad
      edges still always match, same as before) — but two *different* adjacent patches can still
      pick different levels from each other and crack at their shared edge, same risk the reference
      solves with a load-time `R_StitchAllPatches` pass this port doesn't have. Not yet observed in
      practice (most adjacent patches in typical maps share similar curvature); worth adding if a
      visible seam turns up. Compiled clean (full `mingw32-make -j4 BUILD_RENDERER_VULKAN=1 release`
      build, MSYS2 MINGW64, zero new warnings). Not yet re-confirmed in-game after the fix.
- [ ] Cinematics/video playback (`RE_UploadCinematic`, `RE_StretchRaw`, both currently stubbed) —
      RTCW SP uses in-engine cutscenes/briefing videos; this is gameplay-visible, not cosmetic.
- [ ] 2D completeness: `RE_RotatedPic`, `RE_StretchPicGradient` (currently stubbed).
- [ ] Sweep remaining stubs for actual usage before investing further: `RE_RenderToTexture`,
      `R_GetTextureId`, `RE_TakeVideoFrame`, `RE_SetWorldVisData` — confirm whether game/cgame code
      paths this project actually exercises call these; implement only what's load-bearing.

## Phase 3 — Frame pipelining & throughput

- [ ] Real frames-in-flight (≥2 command buffers/fences/semaphores) instead of the current single
      command buffer + single fence that fully serializes CPU and GPU per frame.
- [ ] In-place swapchain resize instead of the current full `vid_restart` on every resize or
      `VK_ERROR_OUT_OF_DATE_KHR`/suboptimal result.
- [ ] Revisit the pipeline cache once Phase 1/2 grow the pipeline count: current cache is a
      64-entry linear scan; vkQuake3-scale content hits 60-100+ live pipelines. Hash-based lookup
      if the linear scan starts showing up in profiles.
- [ ] GPU mip generation (`vkCmdBlitImage` chain) to replace the Phase 0 CPU box-filter stopgap, if
      load-time cost becomes a problem.

## Phase 4 — rend2 parity: material & lighting tier

This is the first tier that goes beyond vkQuake3. Foundational — gates everything in Phase 5.

- [ ] Extend vertex formats (world *and* entity — entity vertex format today is position+UV only,
      no normals at all): add tangent, normal, lightdirection, bone-index/weight attributes, and a
      second position/normal/tangent frame for vertex-lerp animation blending.
- [ ] Tangent/bitangent precompute at load time (one-time cost, not per-frame) for BSP surfaces,
      patches, and models — mirrors rend2's `R_CalcTangentVectors`.
- [ ] Normal map / specular map / parallax(relief) map / deluxe map support in a new uber-fragment-
      shader, following architecture decision #1 above (flag-driven, not permutation-exploded).
- [ ] Real per-pixel Blinn-Phong lighting (rend2 is not PBR — don't build a metal/roughness
      pipeline that doesn't exist upstream) replacing the Phase-1-era flat tint, now with real
      normals to work from.
- [ ] Port light-grid ambient+directed+direction sampling (`tr_light.c` logic already partially
      present) into per-draw UBO/push-constant feed for the new lighting model.
- [ ] Precompiled lighting-path permutation table per architecture decision #2 above.

## Phase 5 — rend2 parity: advanced rendering tier

- [ ] FBO/render-target chain as a sequence of Vulkan render passes with explicit barriers: depth
      prepass → forward HDR color pass → (pshadow pass, if a shadow caster is visible) → SSAO →
      bloom chain → tonemap/resolve → FXAA/color-grade/vignette/grain pass. This is a forward
      renderer with bolted-on passes, not a deferred/G-buffer renderer — no MRT complexity needed.
- [ ] HDR render target (`R16G16B16A16_SFLOAT`) + auto-exposure tonemapping.
- [ ] Bloom: build one reusable "fullscreen blit with a custom fragment shader" Vulkan primitive,
      then reuse it for the bloom downsample/blur/upsample mip chain, SSAO, FXAA, tonemap, and
      color-grade passes instead of one-off plumbing per effect.
- [ ] SSAO, FXAA, depth-of-field/bokeh screen-space passes. (No SSR, no motion blur, no cascaded
      sun shadow maps exist in rend2 — don't scope those in; they're not part of the parity target.)
- [ ] Cubemap reflection probes: add `VkImageViewType_CUBE` support to the image/sampler layer
      (currently 2D-only), bake probes at load time (6-face render reusing the main pipeline, not
      per-frame), parallax-corrected sampling in the material shader, probe placement sanity
      checks against the light grid (mirroring rend2's "probe outside lightgrid/inside wall"
      warning).
- [ ] pshadows: offscreen per-entity shadow-blob render target + sampling in the main pass, per
      architecture decision #4.
- [ ] Final postprocess pass: gamma correction + color-grading LUT + vignette + film grain +
      chromatic aberration. In rend2 these are literally one shader (`gamma_fp.glsl`, folded into
      `tonemap_fp.glsl` when HDR is active) — build it as one pass here too, and retire the Phase 0
      CPU-baked-gamma-table stopgap once this lands (`u_InvGamma = 1.0 / r_gamma`).

## Phase 6 — Polish, validation, rollout

- [ ] Audit RTCW-SP-specific UI/gameplay rendering hooks (briefing-room cinematics, binoculars/
      sniper-scope overlays, any other RTCW-unique HUD/UI draw path) against the Vulkan renderer
      specifically, not just generic 2D pic drawing.
- [ ] Map-by-map visual regression pass vs. OpenGL1 and rend2 output, using the Phase 0 screenshot
      tooling.
- [ ] Performance pass: frame-pacing measurement, GPU profiling, pipeline-cache live-count check,
      clean run under `VK_LAYER_KHRONOS_validation` (zero errors/warnings).
- [ ] Decide on promoting `vulkan` as a user-facing/default `cl_renderer` option once parity-plus
      is reached.

## Changelog

- **2026-11-18** — Initial audit and roadmap created. Surveyed current `renderer_vulkan` state
  (live/selectable via `cl_renderer`, 17 commits over 2 days, world+entities+sky+fog+dlights+poly
  all partially working), diffed against vkQuake3-master's full feature set, and catalogued
  `code/rend2`'s advanced-renderer feature set as the "beyond vkQuake3" target. No code changes
  made this session — analysis only.
- **2026-11-18** — Added gamma/brightness/overbright coverage (flagged as missing by the user):
  confirmed `renderer_vulkan` applies zero gamma correction and has no overbright-bits lightmap
  scaling, vs. both `code/renderer` and `code/rend2` treating these as load-bearing. Added a
  baseline gamma/brightness item to Phase 0 (CPU gamma-table stopgap, vkQuake3-style), an
  overbright-bits item to Phase 1, and expanded Phase 5's postprocess bullet to fold gamma into the
  same final pass as color-grade/vignette/grain, matching rend2's actual `gamma_fp.glsl` design.
- **2026-11-18** — Implemented all of Phase 0 except overbright bits (that's a Phase 1 item, not
  Phase 0): fixed `R_ModelBounds`, `animmap`, and the world multi-stage bug; added mipmapping +
  anisotropic filtering, baseline gamma correction, screenshot capture, and `r_showtris`/`r_showImages`
  debug overlays. `r_shownormals` deliberately skipped (needs vertex normals that don't exist until
  Phase 4). The debug-test-cube item turned out to already be fixed (cvar-gated, defaults off) —
  corrected the roadmap rather than redoing it. All changes compiled clean (MSYS2 MinGW64,
  `BUILD_RENDERER_VULKAN=1` debug build) after each step; not yet runtime-tested in the actual game.
  See each item's strikethrough/checkmark above for specifics and known carried-over limitations.
- **2026-11-19** — Runtime-tested Phase 0 and found a severe real-world regression: menu load went
  from fine to 30-60+ seconds. Chased three wrong theories first (per-texture `vkQueueWaitIdle`
  sync overhead → added upload batching, no effect; font-atlas mip chains → `IMGFLAG_NOMIPMAP` fix,
  no effect; a "fat" multi-stage shader pulling in many textures → added per-call diagnostics, found
  none). Added real sub-phase timing to `vk_create_texture_image` and got a definitive answer: the
  staging-buffer phase was 99% of the cost, and specifically the gamma-correction lookup running
  **in place on the mapped (likely write-combined) GPU staging buffer** — reading that memory
  back byte-by-byte is drastically slow, unrelated to mip count, image size, or allocation count.
  Fixed by gamma-correcting while copying from the CPU-side source into the staging buffer instead
  of after (`R_CopyWithGammaCorrection`), so the GPU-visible memory is only ever written. User
  confirmed load time is back to normal. All temporary diagnostic instrumentation (the
  `s_diag*`/`R_Print*Stats`/`vk_print_upload_*` additions across `vk_image.c/h`, `tr_image.c/h`,
  `tr_shader.c/h`, `tr_init.c`) was then removed; only the real fixes (gamma-on-copy, upload
  batching) remain. See the "Correctness trap" note under Phase 0's gamma item for the generalized
  lesson. Lesson for *this* doc: when a regression is reported, get real timing data before
  theorizing — three plausible-sounding mechanisms in a row were wrong.
- **2026-11-19** — Started Phase 1. Shipped two clean, low-risk, verified-to-compile fixes:
  `tcMod entityTranslate` and overbright bits (see checkmarks above for both). Investigated the
  rest of Phase 1 in depth before touching more code and found real scope/risk issues the original
  bullet list didn't capture: `deformVertexes`' remaining types split into "blocked on Phase 4
  normals" (wave/bulge/normal), "needs a per-primitive not per-vertex calling convention"
  (autosprite/autosprite2 — fold into the shared-iterator work), "feasible but needs paired testing
  with Phase 2's real shadows" (projectionShadow), and "rare, lowest priority" (text0-7). The
  "shared multi-stage iterator" item turns out to require converting world rendering from a static
  load-time-baked vertex buffer to vkQuake3's per-frame-regenerated model (same as entities/polys
  already do) to get real per-frame `tcMod` on world surfaces — a genuine performance-profile
  change, not just a correctness fix, right after this session's long unverified-performance-
  regression debugging arc. Flagged rather than assumed; see Phase 1's iterator bullet. "Real BSP
  fog volumes" also turned out to be its own multi-part feature (shader `fogParms` parsing + BSP fog
  lump + per-surface fog tagging + camera-in-fog-zone test), not a quick add. Multitexture pipelines
  were scoped as independently doable (pure draw-count optimization, no static/dynamic question)
  with `glslangValidator`/`glslc` confirmed available via MSYS2 for compiling new shaders.
- **2026-11-19** — Asked the user directly about the world static-vs-dynamic performance tradeoff
  rather than assuming; they said go ahead and accepted testing performance afterward. Rewrote
  `vk_world.c` accordingly (see the iterator item's checkmark above for the full design). Compiled
  clean in both debug and release. **User tested in-game**: Castle holds a constant 250 FPS, River
  Outpost runs 150-250 FPS — no dramatic regression. Caveat (the user's own, worth keeping in mind):
  most of the renderer's eventual workload (shadows, normal maps, real fog, etc.) still doesn't
  exist, so this isn't a final verdict, just confirmation the per-frame world expansion itself isn't
  immediately disqualifying. Re-test again once more Phase 1/4/5 per-frame work lands on top.
- **2026-11-19** — Added dynamic lights as a real additive pass for world geometry (see checkmark
  above) now that the world-expansion architecture makes it natural: a third dynamic buffer, same
  per-vertex falloff formula as `vk_entity.c`, only costs anything when `numDlights > 0`. Compiled
  clean in debug and release; not yet visually confirmed in-game (should show as muzzle
  flashes/explosions lighting up nearby world surfaces, which they couldn't do at all before).
  Entity dlights still use the older flat-tint approach — left as its own open item rather than
  scope-creeping this change further.
- **2026-11-19** — Implemented multitexture pipelines for entities/polys (see checkmark above):
  new `multi_texture` shader pair + `vk_getPipelineMultiTexture`/`vk_getMultiTextureDescriptor`
  lazy caches in `vk_pipelines.c`, gated by a shared `vk_canCollapseMultiTex` eligibility check, with
  `vk_entity.c`/`vk_poly.c` each branching per-surface into a collapsed single-draw path. World
  deliberately left alone (see the checkmark's note on why). Compiled clean in both debug and
  release via direct `renderer_vulkan_x64.dll` make targets (the full `debug`/`release` meta-targets
  hit an unrelated pre-existing `PRODUCT_VERSION` quoting failure in `common.c` under this session's
  MSYS2 shell invocation — not caused by this change, and not worth chasing since it doesn't touch
  anything `renderer_vulkan` depends on). Not yet visually confirmed in-game.
- **2026-11-19** — Re-verified this doc against the code at the start of a new session (user asked
  for a status check before continuing): confirmed every checkmark above still matches
  `code/renderer_vulkan`, confirmed via a full `release` build (`BUILD_RENDERER_VULKAN=1`) that it
  still compiles clean end to end. No code changes.
- **2026-11-19** — Implemented Phase 1's portal/mirror clip-plane item, plus sky-portal support
  (flagged by the user as a separate thing they rely on for their skybox, confirmed unrelated and
  folded in alongside it) — see both items' checkmarks above for the full design and the known
  gaps/risks (notably: the clip-plane's sign/orientation is derived, not yet visually confirmed).
  Compiled clean (full `release` build, zero warnings in `renderer_vulkan`). Neither feature has
  been tested in-game yet — next session should load a map with an actual mirror/portal shader
  and, separately, one with a `_skyboxOrigin` key, before trusting either further.
- **2026-11-19** — Implemented Phase 2's decals item (`R_MarkFragments`), jumping ahead of the rest
  of Phase 1 at the user's request. New `tr_marks.c`/`.h`; see its checkmark above for the full
  design and scoped-out gaps (no patch/curved-surface decals yet). Compiled clean. Not yet fired in
  a real game session — next step is shooting a wall and confirming a decal actually shows up.
- **2026-11-19** — User tested decals in-game (side-by-side against OpenGL1, which worked
  "flawlessly") and found none appeared on Vulkan. Added temporary stage-counter diagnostic
  logging to `tr_marks.c` rather than guess; the user's log showed candidate surfaces and triangles
  were being found (`boundsOk=2 tris=74`) but 100% clipped away (`fragments=0`), pointing straight
  at the clip-plane math. Fixed the side-plane sign bug (see the decal checkmark above) and removed
  the diagnostic logging once confirmed. User then reported decals appearing but flickering; traced
  to a second real gap (no `polygonOffset`/depth-bias support anywhere in this renderer) and fixed
  that too (same checkmark). Both fixes compiled clean; decals confirmed showing up in-game, the
  polygon-offset flicker fix itself not yet separately re-confirmed.
- **2026-11-19** — User reported the view-weapon model sinking into walls at close range on
  Vulkan, never on OpenGL1. Root cause: `RF_DEPTHHACK` (the view-weapon depth-range hack) was
  completely unimplemented in `renderer_vulkan`. Ported it to `vk_entity.c` — see the new Phase 0
  checkmark above for the design. Compiled clean; not yet confirmed in-game.
- **2026-11-19** — User reported func_door and func_rotating being completely invisible
  ("transparent") in-game. Root cause was not a rendering bug but a missing feature the original
  audit never caught: brush-model entities (inline BSP submodels, name convention `"*N"`) had zero
  support anywhere in `renderer_vulkan` — no `MOD_BRUSH` model type existed, so they registered as
  empty placeholders, and even with geometry the static world's visibility marking never reaches
  brush-entity surfaces (they're not in any BSP leaf's surface list). Added full brush-model entity
  rendering: see the new Phase 1 checkmark above for the design (`MOD_BRUSH`, `vk_world_draw_brushmodel`,
  the `vk_world_build` submodel-0 scoping fix, `R_ModelBounds` coverage). Compiled clean (full release
  build, MSYS2 MINGW64, zero new warnings). **User confirmed working in-game** (doors and rotating
  doors render and animate correctly).
- **2026-11-20** — User reported weapon view-models looking "shattered"/misplaced/badly textured
  (varying by weapon and location) and enemy characters rendering plain white with no heads.
  Diagnosed from the user's console log plus direct inspection of the mod's pk3 content (not
  guesswork): found two real, previously-unnoticed bugs, both pre-existing (not introduced this
  session) and both outside `code/renderer_vulkan` proper but load-bearing for it.
  **(1) `RE_RegisterModel` (`tr_model.c`) had no cross-extension fallback.** The reference engine
  (`code/renderer/tr_model.c`) tries every supported model extension (iqm/mdr/mds/mdm/mdx/md3/mdc,
  in that preference order) when the exact named file is missing — content routinely references one
  extension (e.g. a `.skin`'s `md3_part` value, or a weapon's configured hand model) while only a
  different one ships (confirmed directly: `models/players/officerss/head.mdc` exists in `pak0.pk3`
  but every head skin's `md3_part` names `head.md3`, which doesn't; same pattern for several
  weapons' `_hand` models, e.g. `v_luger_hand.md3` ships in `z_pack.pk3` but `.mdc` gets requested).
  Without the fallback this renderer had been silently failing to load these models (`RE_RegisterModel`
  returning 0), which cgame treats as "don't draw this part" — explaining the missing enemy heads
  outright, and likely contributing to the weapon oddities. Ported the reference engine's fallback
  logic (try the named extension first, then every other supported one) into `RE_RegisterModel`.
  **(2) `CommaParse` (`tr_skin.c`) didn't consume the field separator.** Every `.skin` file is
  `surfaceName,shaderPath` pairs; the parser's token-scanning loop stopped exactly *at* a comma
  without advancing past it, so the *next* field always started with a leading `,` glued onto it.
  Confirmed directly in the user's log: `R_LoadImage: no .tga/.jpg/.png found for
  ',models/players/trench/t_legsordnungspolizei2.tga'` — a literal leading comma baked into the
  path. This silently failed every second field of every skin file (shader/image names), explaining
  the uniform white characters (texture lookups failing and falling back to the engine's 1x1 white
  default image, `tr_image.c`'s `R_CreateDefaultImage`). Fixed by advancing past the comma once the
  token loop stops on one.
  Both fixes compiled clean (full `mingw32-make` release build, MSYS2 MINGW64, zero new warnings).
  Not yet re-confirmed in-game by the user as of this entry — next step is relaunching and checking
  both enemy heads and weapon appearance. The "shattered" weapon geometry specifically (as opposed to
  missing/wrong-textured parts) is not fully explained by either fix and may need its own follow-up
  if it persists.
- **2026-11-20** — User asked for a repo audit + doc sync after work landed outside this session
  (two commits: brush-model entities, then the model-extension-fallback/skin-comma fixes). Verified
  both commits' existing roadmap entries directly against the code (`MOD_BRUSH`, the
  `RE_RegisterModel` fallback loop, `tr_skin.c`'s `CommaParse` separator-consume) — all accurate, no
  drift. Found one real gap: `vk_pipelines.c` has an undocumented `r_vk_disableMultiTex` bisection
  cvar left over from the shattered-weapon investigation (ruled out, no effect, but never written up
  — see the new "Open/unverified" bullet above). Refreshed the "Current state" summary at the top of
  this doc, which neither of the last two commits had touched and which had drifted well behind the
  Phase 1/2 checklists (still said "Phase 0 complete" and listed decals/overbright as missing, both
  long since done). No code changes this session.
- **2026-11-20** — Root-caused and fixed the "shattered weapon/held-item" report (streaked/exploded
  geometry, wrong texture glued onto an unrelated held item, an MDS character's body disappearing
  while its tag-attached head rendered fine, a world prop disappearing when a held item that uses
  the same mechanism was equipped). Diagnostic logging (`r_vk_debugWeapon`, see "Open/unverified")
  plus a user-provided `rtcwconsole.log` caught a real discrepancy: between two consecutive logged
  entities in the same frame, `vertWrite`/`numDraws` had advanced by more than the entities actually
  logged in between could account for. Tracing that against `tr_init.c`'s `R_RenderViewInternal`
  confirmed the mechanism: this map uses a skybox portal (`RDF_SKYBOXPORTAL`), so cgame calls
  `RE_RenderScene` (and therefore `R_RenderViewInternal`) **twice per real frame** — once for the
  sky-room scene, once for the real scene (a mirror/portal destination view adds a third, recursive
  call via `R_TryRenderPortalView`). `vk_world_draw`, `vk_entity_draw_all`, and `vk_poly_draw` each
  reset their shared dynamic-buffer write cursors (`vertWrite`/`idxWrite`/`mtVertWrite` in
  `vk_entity.c`, `writeOffset`/`mtWriteOffset` in `vk_poly.c`, `s_dynLMWriteOffset`/`s_dyn2DWriteOffset`/
  `s_dynBrushLMWriteOffset`/`s_dynBrushExtraWriteOffset`/`s_dynDlightWriteOffset` in `vk_world.c`) to 0
  on **every call**, but all three write into persistently-mapped GPU buffers that are only actually
  read when the whole frame's command buffer is submitted — after *every* scene this frame has already
  recorded its draw commands. So the second scene's reset-to-0 silently overwrote the first scene's
  vertex/index data before the GPU ever read it, while the first scene's already-recorded draw calls
  still pointed at those now-overwritten buffer offsets — exactly the "architecture decision #5"
  multi-scene-per-frame risk this doc already flagged when sky-portal support was added, now confirmed
  as the actual cause. Fixed by moving all these resets out of the per-scene draw functions into new
  `vk_world_begin_frame()` / `vk_entity_begin_frame()` / `vk_poly_begin_frame()` calls, invoked once
  per real frame from `RE_BeginFrame` (`tr_init.c`), so the cursors now correctly accumulate across
  every scene rendered within one frame instead of restarting at 0. Compiled clean (full
  `mingw32-make -j4 BUILD_RENDERER_VULKAN=1 release`, zero new warnings). Not yet re-confirmed in-game
  by the user — next step is retesting the mine/Thompson/MDS-soldier repro, then removing the
  `r_vk_debugWeapon` diagnostic logging and the unrelated `r_vk_disableMultiTex` bisection cvar.
- **2026-11-20** — Retesting the fix above (same session) showed real improvement but surfaced a new
  symptom: some weapons (FG42) render with their body invisible while an attached part (scope) still
  renders. A second `rtcwconsole.log` traced this to a *different* bug the first fix's design exposed:
  `tr_scene.c`'s `R_GetSceneEntities`/`R_GetSceneDlights`/`R_GetScenePolys` return the whole
  accumulated-this-frame list on every call, with no per-scene windowing. Cross-checked against
  `code/cgame/cg_view.c`'s `CG_DrawSkyBoxPortal` (called from `CG_DrawActiveFrame` right after
  `trap_R_ClearScene()`, before any entities/polys/lights are added) and classic `code/renderer`'s
  `tr_scene.c` (`r_firstSceneEntity`/`r_firstSceneDlight`/`r_firstScenePoly`, advanced after every
  `RE_RenderScene` call): the intended behavior is that the skybox-portal scene sees an *empty* list
  (cgame hasn't added anything yet) and the main scene afterward sees only what's added since —
  never both scenes drawing the same full entity list. Without that windowing, every entity (the view
  weapon included) was being fully processed twice per frame, eating roughly double the per-frame
  vertex/index budget now that the first fix makes that budget shared across a frame's scenes — and a
  real `scratch buffer full, dropping remaining entity surfaces this frame` overflow turned up later
  in the same log, confirming the budget was in fact being exceeded. Fixed by giving `tr_scene.c` the
  same sliding-window mechanism as the classic renderer (`s_firstSceneEntity`/`s_firstSceneDlight`/
  `s_firstScenePoly`, reset in `R_ClearScene`, advanced by a new `R_AdvanceSceneWindow()` called from
  `tr_init.c`'s `RE_RenderScene` right after `R_RenderViewInternal` returns). `R_GetScenePolyVerts`
  needed no change (`scenePoly_t.firstVert` already indexes the poly-vert array absolutely). This is
  additive to the first fix, not a replacement for it — the shared-buffer-cursor persistence is still
  required so that whatever each windowed scene *does* draw doesn't overwrite another scene's data
  before the GPU reads it. Compiled clean. Also added temporary `r_vk_debugSky` diagnostic logging
  (`tr_world.c` / `vk_sky.c`) for a separately-reported, not-yet-root-caused issue: the skybox portal
  itself renders nothing in-game (pitch black, with the render pass's raw clear color showing through
  where sky should be) — unclear yet whether this is a pre-existing gap (sky-portal was never visually
  confirmed before this session) or related to the entity-windowing change. Next step: retest with
  this build, and if the sky issue persists, capture a log with `r_vk_debugSky 1` set.
- **2026-11-20** — Retesting the windowing fix showed real improvement (most frames now correctly
  start at `vertWrite=0`) but FG42/MP44 were still reported with their body invisible (scope/mag
  visible), plus a new symptom: any weapon disappears after sustained firing and only reappears
  10-15s later. A third `rtcwconsole.log`, read raw around the climbing `vertWrite` values, found the
  real cause: `code/cgame/cg_weapons.c`'s `CG_RenderViewWeapon()` — "Renders the entities cached by
  CG_AddViewWeapon ... in their own scene pass ... Must be called after the main world scene has been
  rendered" — is a **third** `RE_RenderScene` call every frame (confirmed via `code/cgame/cg_draw.c`:
  main scene render immediately followed by `CG_RenderViewWeapon()`), with its own
  `trap_R_ClearScene()` right before it. So a frame is really sky-portal → main (NPCs, items, effects)
  → view-weapon, not just two scenes, and the view weapon — drawn *last* — is the one whose entities
  get silently dropped first once the frame's shared vertex/index budget (now correctly shared across
  all three scenes by the first fix in this session) runs out. `ENTITY_MAX_VERTS`/`ENTITY_MAX_INDEXES`
  in `vk_entity.c` were still sized for a single scene's worth of content (65536/262144) from before
  that fix; a busy main scene alone during combat can approach that on its own, leaving the
  view-weapon's few thousand verts no room, and a real `scratch buffer full` overflow in the log
  confirmed the cap was in fact being hit. This also fully explains the FG42/MP44 symptom at the
  per-surface level: the scope/mag belong to whichever of the weapon's several linked entities happens
  to be processed before the cap is reached, the gun-body surface is whichever one hits it. Fixed by
  giving `ENTITY_MAX_VERTS`/`ENTITY_MAX_INDEXES` real headroom for three scenes instead of one (4x,
  to 262144/1048576 — trivial extra GPU memory, ~19MB total across both vertex buffers and the index
  buffer). `vk_world.c`'s dynamic world buffers were checked too: already sized at `2x` the whole
  map's total index count, comfortably covering "main pass + small sky-room pass" without a change.
  `vk_poly.c`'s scratch capacity was left alone — no poly/world overflow warnings appeared in any of
  the three logs collected so far, only the entity one. Compiled clean. Also chased, then ruled out, a
  false lead here: raw `vk_sky dbg` lines briefly looked like the sky-portal and main scenes render in
  the wrong order (main before sky), which would misexplain the still-open black-sky report — a wider
  sample of the same log's `rdflags=` sequence showed the true, correctly-repeating order is sky (24)
  then main (16), matching `cg_view.c`'s actual call order (`CG_DrawSkyBoxPortal()` then
  `CG_DrawActive()`); the two-line excerpt that suggested otherwise just happened to span a frame
  boundary. The black-sky report itself is still open and unexplained — not yet re-investigated this
  round, since the user's latest report focused on the weapon disappearance instead.
- **2026-11-21** — Three fixes. (1) Validated curved-patch tessellation (Phase 2 item) and confirmed
  a real bug: `r_subdivisions` is a world-unit error tolerance in the reference engine, but a literal
  fixed segment count here — made it adaptive per patch (`tr_patch.c`'s new `R_ComputeAdaptiveSubdiv`),
  fixing user-reported faceted/triangular arches. (2) `make deploysteam` was missing the
  `BUILD_RENDERER_VULKAN` copy block that `copyfiles` already had, so the vulkan DLL never reached the
  user's Steam install — added it to `Makefile`. (3) `misc_mg42` wasn't rendering: its model
  (`mg42a.mdc`) is actually raw MD3 data shipped under a `.mdc` name (confirmed by reading its header:
  ident/version are MD3's, not MDC's). `RE_RegisterModel` dispatched by file extension, so it always
  went through `R_LoadMDC` and failed the version check. Fixed to sniff the file's own ident and
  dispatch to the matching loader, same as the reference engine's `R_RegisterMD3`. All three compiled
  clean; none re-confirmed in-game yet.
- **2026-11-21** — Follow-up: mg42a now partially rendered (mag visible, main body missing), same
  symptom on mp44/fg42. Parsed `mg42a`'s real MD3 surfaces directly: its body surface
  (`mg34_reference`) has 11582 verts, over both the format's documented 8192-per-surface max and this
  renderer's `SKIN_SCRATCH_VERTS` scratch-buffer cap (`vk_entity.c`), so it silently hit the
  `goto nextMD3` overflow guard every frame while the smaller mag surfaces didn't. Reference engine
  uses `SHADER_MAX_VERTEXES` (40000) and doesn't enforce the format limit at all, which is why OpenGL1
  renders this off-spec content fine. Bumped `SKIN_SCRATCH_VERTS` to match. Compiled clean.
