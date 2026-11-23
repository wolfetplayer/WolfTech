# Vulkan Renderer Roadmap

Living tracking doc for bringing `code/renderer_vulkan` up to, and then past, vkQuake3-level
parity, with the end goal of matching `code/rend2`'s visual feature set. Update this file every
session that touches the Vulkan renderer: check off finished items, add new findings, and append
a dated entry to the Changelog. Don't let this doc drift from the code.

Reference implementations:
- `vkQuake3-master` (external checkout) — baseline "classic id-tech-3 in Vulkan" target.
- `code/rend2` (this repo) — OpenGL2 advanced renderer, the "beyond vkQuake3" target.
- `code/renderer` (this repo) — classic OpenGL1 renderer; shares shader-script semantics with the
  Vulkan renderer and is the fastest cross-check for "does this shader keyword do anything."

## Current state (last updated 2026-11-23)

The Vulkan renderer is real and live, not a stub: `cl_renderer vulkan` + restart renders the game
today (`code/client/cl_main.c` dynamically loads it; builds by default). Phase 0 is complete, and
Phase 1/2/3 are essentially done; Phase 4 is in progress (vertex-format work complete, lighting
work partially done).

Still behind the reference points on everything not checked off below. Notably:
- **vs. vkQuake3**: no cinematics; `deformVertexes` beyond `move`/`wave`/`bulge` (and those two only
  for world geometry, not entities) is incomplete. Mirror/portal clipping is world-geometry-only.
- **vs. rend2**: no normal/specular/parallax/deluxe maps, no per-pixel lighting (world = baked
  lightmap only; entities with `rgbGen lightingDiffuse` get real per-**vertex** diffuse lighting as
  of 2026-11-22, nothing per-pixel yet). No HDR/bloom/tonemap/SSAO/FXAA/DOF/cubemap probes/shadow
  mapping.
- **Open/unverified**: `r_vk_disableMultiTex` (`vk_pipelines.c`) and `r_vk_debugWeapon`
  (`vk_entity.c`) are leftover bisection/debug cvars from earlier investigations — harmless (off by
  default) but should be removed as cleanup. A temporary diagnostic block in `vk_world_build`
  (`vk_world.c`, logs vertex-lit/CGEN_VERTEX surfaces) is still in place from the 2026-11-23
  barrel-darkness investigation below and should be removed once that's resolved.
- **Known real bug, root cause still unconfirmed**: static map decorations that bake lighting into
  vertex color (`rgbGen vertex` + a second `map $lightmap`/`blendFunc filter` stage, e.g.
  q3map2-merged `misc_model` props) render darker than the reference engine. See the last two
  Changelog entries — the "overbright is only half-implemented" theory was investigated and then
  *ruled out* (the reference engine's overbright compensation is a whole-frame post-process, not a
  per-blend-pass multiplier, so it can't explain a per-object brightness difference). Real cause
  still open as of 2026-11-23.

## Architecture decisions (don't let these happen by accident)

1. **Material flags (normal/spec/parallax/deluxe maps) → uber-shader + push-constant/UBO flags**,
   not a pipeline permutation per combination (rend2's `u_EnableTextures` approach). Bind
   dummy/white fallback textures to unused slots and branch in-shader. Reserve pipeline variants
   for actual fixed-function state changes (blend/cull/depth/stencil/major lighting path).
2. **Lighting-path selection → precompiled SPIR-V permutation table**, mirroring rend2's 128-way
   `LIGHTDEF_*` scheme, lazily cached by bitmask. Direct low-risk translation — don't invent a new
   dynamic-branching scheme.
3. ~~CPU skinning now, GPU skinning batched with the vertex-format work~~ — done, see Phase 4.
4. **Stencil shadow volumes (Phase 2) vs. pshadows (Phase 5) are different features**, not
   sequential replacements — rend2 uses offscreen per-entity shadow blobs, not cascaded maps or
   stencil volumes. Decide at Phase 5 whether to keep both behind `r_shadows` modes or drop stencil
   shadows for the final build.
5. **Keep the frontend/backend split intact.** `tr_scene.c`/`tr_main.c` should never call `vk*`
   functions directly from frontend code (matches `code/renderer`'s typed-command-buffer contract
   via shared `tr_public.h`) — verified true so far; re-check if anything starts diverging, since it
   matters for future threaded frontend/backend work.

## Phase 0 — Correctness & verification groundwork

- [x] Debug test cube / entity-origin markers: already cvar-gated and off by default — no change
      needed, original audit was wrong about this.
- [x] Screenshot capture (`screenshot`/`screenshotJPEG`): `vk_screenshot.c`, copies the
      just-presented swapchain image via a staging buffer, writes TGA/JPEG.
- [x] `r_showtris` (world geometry only — a line-mode pipeline, gated on `fillModeNonSolid`; entity
      wireframe not implemented) and `r_showImages` (40-column texture-atlas grid dump).
      `r_shownormals` deliberately *not* implemented here — no vertex normals existed yet
      (Phase 4 unblocked this but it's still not wired up as of this writing).
- [x] Mipmapping + anisotropic filtering: `vk_image.c` generates mips via `vkCmdBlitImage` at
      upload; anisotropy was already device-enabled, just not used by the sampler — fixed.
- [x] `animmap`: frame list/frequency parsed and stored (`shaderStage_t`), resolved per-frame in
      `R_GetStageImage`. Wired into entities/polys/2D pics. World surfaces still only show frame 0
      (their descriptors are built once at load, a separate limitation).
- [x] `R_ModelBounds`: frame-0 local bounds for MD3/MDC/MDS/MDX. MDM has none of its own (bounds
      live on its MDX skeleton) — returns zero, an honest gap not a bug.
- [x] Baseline gamma: `r_gamma` + a CPU gamma table applied at texture-upload time
      (`vk_image.c`), covering regular textures/fonts/lightmaps through one choke point
      (`vk_create_texture_image`). **Gotcha worth remembering**: an early version ran the gamma
      lookup *in place* on the mapped (write-combined) staging buffer — reads from write-combined
      GPU-upload memory are drastically slow, causing a ~50ms/texture load-time regression that took
      real sub-phase timing to isolate (three other plausible theories were wrong first). Fixed by
      gamma-correcting while copying from the CPU source into the staging buffer, never reading the
      mapped pointer back. **Rule for later per-pixel upload-time work**: always read-from-source/
      write-to-mapped-buffer in one pass, never read-modify-write the mapped pointer.
- [x] World multi-stage bug: `vk_world_draw` now loops stages 1..N after stage 0's lightmap pass,
      each with its own blend/texture. (Superseded by Phase 1's full rewrite below; this was the
      first, narrower fix.)
- [x] View-weapon depth hack (`RF_DEPTHHACK`): ported `glDepthRange(0,0.3)`-equivalent as a
      per-draw-call `vkCmdSetViewport` switch in `vk_entity.c`. Not ported: the reference's flatter
      projection matrix for depth-hacked draws (cosmetic, separate from the reported bug).

## Phase 1 — Generic multi-stage shader/draw core

- [x] `tcMod entityTranslate`: threaded `refEntity_t.shaderTexCoord` through
      `R_CalcStageTexCoord`/`WriteStageVertices` for entities (world/poly have no entity context,
      pass `NULL`, matching upstream).
- [x] Overbright bits — **only half-implemented, found 2026-11-23**: `R_ShiftLightingBytes`
      (load-time byte-shift of vertex colors/light-grid samples/lightmap texels) is a faithful port
      and *is* done. Missing: the reference engine's other half, a render-time multiplier applied at
      the final post-process/gamma stage. Investigated whether this explains per-object brightness
      differences (it initially looked like it might, via a per-blend-pass `GL_RGB_SCALE` theory) —
      **ruled out**: `code/renderer` has zero `GL_RGB_SCALE` usage; the real mechanism is a
      whole-frame post-process (hardware gamma ramp or an FBO gamma shader) that can't explain one
      object being darker than another in the same frame. See the 2026-11-23 Changelog entries —
      the actual cause of the vertex-lit-decoration darkness bug is still open.
- [ ] `deformVertexes`:
      - [x] `wave`/`bulge` for **world surfaces** (static BSP/patches/brush entities): implemented
            2026-11-22 once real per-vertex normals existed (Phase 4). `R_DeformPosition`
            (`tr_shade_calc.c`) ports the reference's `RB_CalcDeformVertexes`/`RB_CalcBulgeVertexes`
            math directly, minus the RTCW "Ridah fire-rise" negative-frequency hack (needs an
            entity `fireRiseDir` this renderer has no equivalent for). Entity/poly call sites still
            pass `NULL` for normal (see below), unaffected.
      - [ ] `wave`/`bulge` for **entities**: harder than before Phase 4, not just still-blocked —
            Stage 2/3 moved rendered position onto the GPU, so a CPU-side deform wouldn't affect
            what's drawn. Needs to move into `entity_lerp.vert`/`entity_skin.vert` instead.
      - [ ] `normal` (wiggles the normal for wavy env-map sparkle, not position): needs 3D Perlin
            noise this renderer has never had (`GF_NOISE` already stands in with sine for the same
            reason); also zero visible effect today since nothing consumes normals for lighting
            per-pixel yet.
      - `autosprite`/`autosprite2`: needs a per-primitive (quad), not per-vertex, calling
        convention — fold into the shared multi-stage iterator work below rather than bolting on a
        separate path.
      - `projectionShadow`: feasible without normals (needs entity axis/origin/light direction) but
        `R_DeformPosition` has no entity context today and the coordinate-space assumptions need
        verification; pair with Phase 2's stencil shadows when tackled.
      - `text0`-`text7`: needs a `refdef.text[]` data path that doesn't exist. Rare; lowest priority.
- [x] Shared multi-stage iterator (world side): rewrote `vk_world.c` — no more static GPU buffer at
      load; `vk_world_build` sorts/batches + caches tessellated geometry as CPU arrays,
      `vk_world_draw` expands visible surfaces into a per-frame dynamic buffer every frame using the
      same `R_CalcStageColorAlpha`/`R_CalcStageTexCoord` entities/polys use, so `rgbGen`/`tcMod` now
      animate on world surfaces too. Stages 1+ get their own independent `rgbGen`/`tcMod` (previously
      shared stage 0's baked data). Performance: user tested, Castle/River Outpost held 150-250+ FPS,
      no dramatic regression (re-test once more per-frame work lands on top).
      **Bug found and fixed 2026-11-22**: stage 0 was always routed through a fixed opaque pipeline
      regardless of the shader's own blend state — any "trans"-style shader with a real `blendFunc`
      on stage 0 rendered fully opaque. Fixed by routing a shader through the real per-stage blend
      path when `stages[0].blendEnabled && explicitlyDefined` (see below for why the second flag was
      needed) is true, otherwise unchanged.
      **Follow-up bug 2026-11-22**: the above fix's `blendEnabled` check alone wasn't safe — the two
      *implicit* shader paths (`R_InitShaders`'s `*default`, and `R_CreateSingleStageShader`, used
      for any bare-image-path texture with no `.shader` script — i.e. most ordinary world textures)
      both set `blendEnabled = qtrue` unconditionally by design (so alpha channels "just work"
      without a script). Reading that flag for routing wrongly rerouted ordinary textures away from
      their lightmap, making the whole map look washed out. Fixed with a new
      `shader_t.explicitlyDefined` field (`qtrue` only for a real parsed `.shader` block), required
      alongside `blendEnabled` at both fix sites.
      **Third bug 2026-11-22**: `textures/wt_alpha/mesh_c02`-style `alphaFunc`-cutout shaders (no
      `blendFunc` at all, so unaffected by the above) silently lost their cutout — the fixed
      lightmap pipeline had *no alpha-test support at all* (`world_lightmap.frag` only had a
      `clip_plane` spec constant). Added `alpha_test_func` as a second spec constant, same 4-way
      discard logic `single_texture.frag` already uses; `vk.pipeline_3d_lightmap*` grew from
      `[cullType]` to `[cullType][alphaTestFunc]` (12 eager variants each).
- [x] Real multitexture pipelines for entities/polys: `multi_texture.vert`/`.frag` (constant_id 0 =
      combine mode: modulate or add), `vk_canCollapseMultiTex` gates it (exactly 2 stages, stage 0
      opaque, stage 1 blended with `CGEN_IDENTITY`). World deliberately left alone (not worth the
      complexity on the just-rewritten world path for a smaller win there).
- [x] Dynamic lights as a real additive pass for **world geometry** (`ExpandAndDrawWorldDlights`,
      same per-vertex falloff formula as entities' flat-tint hack). Entities still use the
      pre-existing flat-tint hack for dlights — real additive pass for entities is still open.
- [x] Real BSP-brush volumetric fog: `fogParms` now actually parsed (was discarded), `R_LoadFogs`
      builds per-volume AABBs, camera-containment check overrides the frame's fog push constants.
      Scoped simplification vs. the reference's true per-surface distance gradient (uses the
      existing linear-falloff fog plumbing instead of a new permutation/pass).
- [x] Portal/mirror clip-plane rendering: this renderer had zero mirror/portal scaffolding going in
      (no `sort`/`isPortal` parsing, no recursive view-render). Built `tr_portal.c` (ported from
      vkQuake3/code/renderer), split `RE_RenderScene` into a recursive-capable
      `R_RenderViewInternal`, added clip-plane shader variants (`*_clip.vert` for hardware
      `gl_ClipDistance`, spec-constant discard fallback otherwise). Scoped out: entities/polys/sky
      draw unclipped inside a mirror (world-geometry-only); entity-attached portal surfaces. Plane
      sign/orientation derived but not yet visually confirmed — if a mirror shows the wrong half of
      the scene, check `tr_portal.c`'s `s_portalNormal`/`s_portalDist` setup first.
- [x] Sky-portal (`RDF_SKYBOXPORTAL`, distant "sky room") — separate mechanism from mirror/portal
      above. Two back-to-back `RE_RenderScene` calls per frame without re-clearing color. **Two real
      bugs found and fixed** (see 2026-11-22 Changelog): a scene-order assumption that didn't match
      this build's actual cgame call order, and a fixed-size sky cube getting clipped by a
      per-scene-dynamic far plane (fixed by sizing the cube from `R_ComputeZFar`, matching the
      reference's `zFar/1.75` formula). **User confirmed fixed in-game.**
- [x] Brush-model entities (func_door/func_rotating/func_plat/etc.): found via user report ("doors
      invisible") — `tr_model.c` had no case for inline-BSP-submodel names (`"*N"`), and even if it
      had, visibility marking never walked brush-entity surfaces at all. Added `MOD_BRUSH` +
      `vk_world_draw_brushmodel` (per-frame origin/axis transform, own dynamic buffers + lazy
      descriptor cache — brush entities keep baked lightmap/vertex color, never sample the light
      grid, matching real id-tech-3 behavior). **User confirmed working in-game.**

## Phase 2 — Classic feature parity (decals, flares, shadows, cinematics)

- [x] Decals (`R_MarkFragments`): new `tr_marks.c` ports `R_ChopPolyBehindPlane` verbatim; candidate
      surfaces found via flat linear scan + AABB reject (no BSP node walk). Scoped out: `MST_PATCH`
      surfaces don't get decals (patches aren't pre-triangulated here). **Two real bugs found
      in-game testing, both fixed**: inverted side-clip-plane normals (fixed by making planes
      self-correcting against the quad's own center) and decal/wall z-fighting (added real
      `polygonOffset` support, previously nonexistent — `shader_t.polygonOffset` +
      `depthBiasEnable` pipeline axis). **User confirmed working.**
- [x] Dynamic light coronas/flares (`vk_flares.c`, new): visibility via a 1-sample `VkQueryPool`
      occlusion query per corona, reading back the *previous* frame's result (safe since
      `vk_begin_frame` already fences on it — no double-buffered ping-pong needed here unlike
      rend2). Billboard drawn through the real shader stages, not a dedicated screen-space pass
      (scope cut: shrinks with distance like everything else here, not RTCW's distance-independent
      curve). **User confirmed working.**
- [x] Stencil shadow volumes (`r_shadows 2`): classic two-sided z-pass volumes, ported from
      `code/renderer/tr_shadows.c`. **Found and fixed a real prerequisite bug**: the depth-format
      picker tried `VK_FORMAT_D32_SFLOAT` first (no stencil bits) — this renderer had *no usable
      stencil buffer on any GPU* before this. Reordered to stencil-bearing formats, fixed the depth
      view's aspect mask and the render pass's `stencilLoadOp`. Scoped out `deformVertexes
      projectionShadow`'s blob fallback: its registration is commented out in the reference engine
      itself, so `r_shadows 3` is dead code there too — no working baseline to port against.
- [x] Curved-patch (Bezier) tessellation: found and fixed a real bug from user report ("arches
      render faceted") — `r_subdivisions` is a world-unit curvature *tolerance* in the reference
      engine, but this port treated it as a literal fixed segment count. Ported the reference's real
      `Sagitta()` curvature measurement into `R_ComputeAdaptiveSubdiv`. Known gap: per-patch
      adaptive level can still crack at a shared edge between two differently-curved adjacent
      patches (reference solves this with a load-time stitching pass this port lacks) — not
      observed in practice yet.
- [ ] Cinematics/video playback (`RE_UploadCinematic`/`RE_StretchRaw`, stubbed) — gameplay-visible.
- [x] 2D completeness: `RE_RotatedPic`/`RE_StretchPicGradient` ported from the reference's backend
      math. **User confirmed working.**
- [x] Swept `RE_RenderToTexture`/`R_GetTextureId`/`RE_TakeVideoFrame`/`RE_SetWorldVisData` for real
      callers: none load-bearing under default settings (one has zero callers at all; the others are
      dev-tool or `cg_fancyfx`-gated cosmetic paths). Left all four stubbed per user's choice.

## Phase 3 — Frame pipelining & throughput

- [x] Real frames-in-flight (`VK_FRAMES_IN_FLIGHT = 2`): every per-frame CPU-written/GPU-read
      dynamic buffer across the renderer (draw2d/draw3d/poly/entity/world incl. brush
      models/flares/shadows) now has one region per `vk_current_frame_slot()`, generalizing the
      existing fence-wait safety from "1 frame ago" to "N frames ago."
- [x] In-place swapchain resize (replaces full `vid_restart` on `OUT_OF_DATE`/surface-lost):
      `vk_recreateSwapChain` rebuilds only the swapchain/views/semaphores/depth/framebuffers.
      Zero-sized (minimized) surface handled by leaving `vk.swapchain` null and retrying each frame.
      `r_mode`-driven resolution changes still go through `vid_restart`. `VK_SUBOPTIMAL_KHR` stays
      tolerated, not a recreate trigger (some drivers report it persistently and harmlessly).
- [x] Revisited the pipeline cache: `vk_getPipeline3D` has more key axes than vkQuake3's own and
      vkQuake3-scale content already hit 60-100+ entries on a smaller key, so raised
      `MAX_CACHED_3D_PIPELINES` 64→128. Multi-tex cache's cap (32) left alone — genuinely bounded at
      24 max combinations. No hash-based lookup — no profiling data showing the linear scan is hot.

## Phase 4 — rend2 parity: material & lighting tier

This is the first tier beyond vkQuake3; gates everything in Phase 5.

- [x] Extend vertex formats (world + entity — entity format was position+UV only, no normals at
      all): tangent/normal/lightdirection/bone-index/weight, plus a second frame for vertex-lerp
      blending. Staged into 3 parts (full design in the 2026-11-22 Changelog entries):
      - [x] Stage 1 — world (`vkWorldVertex_t`): packed snorm16 `normal`/`tangent`/`lightdir` added.
            `normal` is real (threaded from already-loaded-and-discarded BSP/patch/brush-entity
            data); `tangent`/`lightdir` stay zeroed for their own later items.
      - [x] Stage 2 — entity GPU vertex-lerp (MD3/MDC): static per-model buffers built once at load
            (decoded normal/tangent via Lengyel+Gram-Schmidt), blended with `mix()` in
            `entity_lerp.vert`/`entity_lerp_multitex.vert`. Entities now draw immediately with a
            per-entity `cameraMVP * entityModel` push constant (reuses the existing 128-byte
            layout — `camPos` repurposed to entity-local-space camera, the always-zero trailing
            slot carries `backlerp` — no size growth) instead of a shared-mvp deferred batch.
            **Design finding**: CPU still computes per-frame interpolated position
            (`LerpSurfaceVerts` etc., kept, unchanged) purely to feed `tcMod turb`/shadow volumes —
            it just stops writing that position into the GPU-drawn vertex. MDS/MDM still used the
            old CPU-skinned deferred path until Stage 3.
      - [x] Stage 3 — entity GPU bone skinning (MDS/MDM): bind-pose buffers + top-4 bone
            index/weight, skinned in `entity_skin.vert`/`entity_skin_multitex.vert`; CPU still
            computes bone *matrices* (tags need them) but not the per-vertex skin loop.
            **Format discovery**: `mdsWeight_t`/`mdmWeight_t` store a *separate* offset vector per
            bone influence (not one shared bind-pose position an IQM-style blended matrix could
            transform) — confirmed via the working CPU code's own math. User chose the correct,
            heavier implementation (`offset[4][3]`, 4 real per-vertex matrix transforms, not
            `mix()`) over stopping at Stage 2 like rend2 itself does for this format.
            **Real correctness bug found and fixed**: once all 4 entity model types drew
            immediately, the now-fully-dead deferred-draw tail code still ran
            `s_idxWrite = idxWrite`, stomping the live shared index-write cursor back to a stale
            value every call — would have corrupted the shared index buffer across frames. Fixed by
            deleting the entire now-dead deferred-draw path rather than patching around it.
- [ ] Tangent/bitangent precompute at load time for BSP surfaces/patches/models (rend2's
      `R_CalcTangentVectors`).
- [ ] Normal/specular/parallax/deluxe map support in a new uber-fragment-shader (decision #1).
- [x] Entity per-**vertex** diffuse lighting (stepping stone toward full per-pixel lighting, done
      2026-11-22): `CGEN_LIGHTING_DIFFUSE` stages now compute real
      `ambient + max(dot(normal,lightDir),0)*directed` in `entity_lerp.vert`/`entity_skin.vert`
      (using Phase 4's normal data + the light grid), replacing a flat same-for-every-vertex tint.
      Full design (push-constant packing, new `lighting_diffuse` spec-constant/cache axis, skinned
      normal transform) in the 2026-11-22 Changelog entry. Still Gouraud, not per-pixel — no
      specular/normal maps.
- [ ] Real per-pixel Blinn-Phong lighting (rend2 is not PBR — no metal/roughness pipeline),
      replacing the per-vertex diffuse above with specular + normal-map detail per pixel.
- [ ] Port light-grid ambient+directed+direction sampling into a per-draw UBO/push-constant feed
      for the per-pixel model (partially present already, feeding the per-vertex item above).
- [ ] Precompiled lighting-path permutation table (decision #2).

## Phase 5 — rend2 parity: advanced rendering tier

- [ ] FBO/render-pass chain: depth prepass → forward HDR color → pshadow pass → SSAO → bloom chain
      → tonemap/resolve → FXAA/color-grade/vignette/grain. Forward renderer with bolted-on passes,
      no deferred/G-buffer/MRT needed.
- [ ] HDR render target (`R16G16B16A16_SFLOAT`) + auto-exposure tonemapping.
- [ ] Bloom: one reusable "fullscreen blit + custom fragment shader" primitive, reused for
      bloom/SSAO/FXAA/tonemap/color-grade.
- [ ] SSAO, FXAA, DOF/bokeh. (No SSR/motion blur/cascaded shadow maps — not part of rend2's parity
      target, don't scope them in.)
- [ ] Cubemap reflection probes: `VkImageViewType_CUBE` support, load-time baking, parallax-corrected
      sampling.
- [ ] pshadows: offscreen per-entity shadow-blob target + sampling in the main pass (decision #4).
- [ ] Final postprocess pass: gamma + color-grade LUT + vignette + grain + chromatic aberration
      (one shader in rend2, `gamma_fp.glsl`/`tonemap_fp.glsl`) — retires the Phase 0 CPU gamma-table
      stopgap once this lands.

## Phase 6 — Polish, validation, rollout

- [ ] Audit RTCW-SP-specific UI/gameplay rendering hooks (cinematics, binoculars/scope overlays)
      against the Vulkan renderer specifically.
- [ ] Map-by-map visual regression pass vs. OpenGL1/rend2 (Phase 0 screenshot tooling).
- [ ] Performance pass: frame-pacing, GPU profiling, pipeline-cache live-count check, clean
      `VK_LAYER_KHRONOS_validation` run.
- [ ] Decide on promoting `vulkan` as a user-facing/default `cl_renderer` option.

## Changelog

- **2026-11-18** — Initial audit; roadmap created from a diff against vkQuake3/rend2. No code.
- **2026-11-18** — Added gamma/overbright items to the roadmap (user-flagged gaps). No code.
- **2026-11-18** — Implemented all of Phase 0 except overbright bits. Compiled clean; not yet
  runtime-tested.
- **2026-11-19** — Runtime-tested Phase 0: menu load regressed to 30-60s. Three wrong theories
  chased (texture sync overhead, font mip chains, "fat" multi-stage shaders) before real sub-phase
  timing found the actual cause (see Phase 0's gamma item — write-combined-memory read). Fixed;
  user confirmed load time back to normal. **Lesson**: get real timing data before theorizing.
- **2026-11-19** — Shipped `tcMod entityTranslate` + overbright bits (Phase 1). Scoped the rest of
  Phase 1's risk/complexity in depth before touching more code (deformVertexes split four ways,
  world iterator requires a static→dynamic architecture change, BSP fog is multi-part) — see Phase
  1 items for outcomes.
- **2026-11-19** — User approved the world static→dynamic rewrite's performance risk. Rewrote
  `vk_world.c` (see Phase 1 checkmark). User tested: Castle 250 FPS, River Outpost 150-250 FPS, no
  dramatic regression (caveat: most future per-frame workload doesn't exist yet).
- **2026-11-19** — World dlights, multitexture pipelines, portal/mirror clip-plane + sky-portal,
  decals, view-weapon depth hack, brush-model entities: all implemented this session, in that
  order, largely from direct user bug reports. See each item's Phase 1/2 checkmark above for
  design and the two real bugs found/fixed in decals (side-plane sign, missing polygonOffset) and
  one in depth-hack porting. All compiled clean; brush-model entities **confirmed working
  in-game**, others pending test at time of writing (later confirmed, see subsequent entries).
- **2026-11-20** — Diagnosed "shattered"/missing-texture weapons and plain-white headless enemies
  directly from logs + pk3 content (not guesswork): (1) `RE_RegisterModel` had no cross-extension
  model fallback (reference tries iqm/mdr/mds/mdm/mdx/md3/mdc in order; content routinely
  references an extension that isn't what's shipped) — ported the fallback. (2) `tr_skin.c`'s
  `CommaParse` didn't consume its own separator, breaking every second field of every `.skin` file
  — fixed. Both outside `code/renderer_vulkan` proper but load-bearing for it. Root cause of the
  "shattered" geometry itself (as opposed to missing/wrong parts) remained open.
- **2026-11-20** — Repo/doc audit after out-of-session commits; found one undocumented leftover
  (`r_vk_disableMultiTex`). Refreshed the stale "Current state" summary. No code.
- **2026-11-20** — Root-caused "shattered weapon" via diagnostic logging + user log: this map's
  skybox portal makes `RE_RenderScene` run 2-3x per real frame (sky room, main scene, then cgame's
  separate view-weapon scene), but every per-frame dynamic-buffer write cursor
  (`vertWrite`/`idxWrite`/etc. across `vk_entity.c`/`vk_poly.c`/`vk_world.c`) reset to 0 on *every*
  call instead of once per real frame — later scenes silently overwrote earlier scenes' already-
  recorded GPU data before it was ever read. Fixed by moving all resets into new
  `vk_*_begin_frame()` functions called once per frame from `RE_BeginFrame`.
- **2026-11-20** — Follow-up: a second bug from the same root architecture — `tr_scene.c`'s
  `R_GetSceneEntities`/etc. returned the *whole* frame's list on every call (no per-scene
  windowing), so every entity was processed 2-3x per frame, eating the shared vertex/index budget
  that the first fix now made shared. Added the reference engine's sliding-window mechanism
  (`s_firstSceneEntity`/etc., advanced after each `RE_RenderScene`).
- **2026-11-20** — Follow-up: found the real, final cause of the FG42/MP44 body-invisible symptom —
  `CG_RenderViewWeapon` is a *third* scene per frame, drawn last, and `ENTITY_MAX_VERTS`/
  `ENTITY_MAX_INDEXES` were still sized for one scene's worth of content, so the view-weapon (last
  to be processed) silently overflowed and got dropped first. Bumped both caps 4x. Also chased and
  ruled out a false lead (scene draw order looked wrong in a 2-line log excerpt; a wider sample
  showed the real order was correct). The separately-reported black-sky-portal bug remained open.
- **2026-11-21** — Three fixes: (1) Phase 2's curved-patch adaptive-subdivision bug (see its
  checkmark). (2) `make deploysteam` was missing the Vulkan-DLL copy block `copyfiles` already had
  — added to Makefile. (3) `misc_mg42`'s model is raw MD3 data shipped under a `.mdc` filename;
  `RE_RegisterModel` dispatched by extension instead of sniffing the file's own ident — fixed to
  sniff and dispatch to the matching loader.
- **2026-11-21** — Follow-up: mg42a/mp44/fg42 bodies still partially missing — their largest
  surface (11582 verts) exceeded this renderer's `SKIN_SCRATCH_VERTS` cap (silently hitting the
  overflow guard), while the reference engine's much larger `SHADER_MAX_VERTEXES` (40000) doesn't.
  Bumped the cap to match.
- **2026-11-21** — Follow-up: NPC heads showed a solid white mask over the eyes — a 5th model
  surface (`h_blink`) should only draw when `RF_BLINK` is set (reference engine skips it
  otherwise); this renderer had no such check. Added it to all 4 model-type surface loops. **User
  confirmed major improvement.** Residual, deferred-at-user's-request issue: during the actual
  blink moment `h_blink` itself shows wrong/white instead of the correct eyelid texture — ruled out
  texture content, all skin variants, and pk3 case-sensitivity; next lead (not yet chased) is that
  `h_blink` has 17 real animated delta frames sharing `ent->frame`/`oldframe`, not a simple overlay.
- **2026-11-21** — Implemented real BSP-brush volumetric fog (Phase 1 item) and dynamic light
  coronas/flares (Phase 2 item) — see their checkmarks. Flares hit one build issue (`vk_flares.c`
  missing from the Makefile's `Q3RVOBJ` list) — fixed. **User confirmed coronas working in-game.**
- **2026-11-21** — Implemented stencil shadow volumes (Phase 2 item, see checkmark for the
  stencil-buffer prerequisite bug found along the way). Remembered to add `vk_shadows.o` to the
  Makefile this time (missed `vk_flares.o` in a prior entry). Not self-built this session
  (background-shell `mingw32-make` environment issue, unrelated to the change).
- **2026-11-21** — Removed two leftover debug prints in `tr_bones.c` (`R_GetBoneTag`/
  `R_MDM_GetTag` logging every missing-tag lookup) — confirmed against the reference engine that a
  missing optional tag is normal/silent, not a bug; the logging just should never have stayed in.
- **2026-11-21** — Implemented `RE_RotatedPic`/`RE_StretchPicGradient` (Phase 2 item). **User
  confirmed working.**
- **2026-11-22** — Root-caused the long-standing skybox-portal visual bug (black sky → later a
  solid-color wedge cutting into the starfield). Ruled out, in order: asset content, cube face
  UV/orientation math (found and fixed a real bug here too, but it didn't explain this symptom),
  scene-arrival order (also a real bug, fixed — this build's cgame renders portal-then-main, not
  main-then-portal as assumed), and fog state. **Actual cause**: `vk_sky.c`'s sky cube used a fixed
  half-size (4096), but the reference sizes it dynamically from the *current scene's* far clip
  plane (`zFar/1.75`) specifically so it's never clipped — and this engine's zFar (computed from
  world bounds relative to the camera) comes out much smaller for a tiny sky-portal scene than the
  fixed cube's farthest corner, clipping part of it away and showing the render pass's clear color
  through the gap. Fixed by exposing `R_ComputeZFar` and sizing the cube from it. **User confirmed
  fixed.** All temporary diagnostics from this investigation removed.
- **2026-11-22** — Swept the 4 remaining render-to-texture-family stubs for real callers; none
  load-bearing under default settings. Left stubbed per user's choice (Phase 2 item).
- **2026-11-22** — Implemented real frames-in-flight and in-place swapchain resize (Phase 3 items,
  see checkmarks). Also revisited the pipeline cache (raised `MAX_CACHED_3D_PIPELINES` to 128) and
  removed a stale Phase 3 item that turned out to already be done in Phase 0 (GPU mip generation).
  Not self-built this session (standing instruction).
- **2026-11-22** — Started and completed Phase 4's vertex-format item across the day in 3 staged
  passes (world, then entity GPU vertex-lerp 2a/2b, then entity GPU bone-skinning 3a/3b) — see the
  Phase 4 checkmark above for the consolidated design and the two real findings worth remembering
  (the MDS/MDM per-weight-offset format, and the dead-deferred-draw-path index-cursor-stomp bug).
  Full research/staging notes in `C:\Users\wolfetplayer\.claude\plans\lazy-swinging-noodle.md`.
  Needed 4 shader recompiles (`shaders/gen_spv_c.py`) across the stages; not self-built. **User
  later confirmed Stage 2b/3b tested fine** after compiling the shaders by hand.
- **2026-11-22** — Implemented `deformVertexes wave`/`bulge` for world surfaces (Phase 1 item, see
  checkmark) using the now-available normal data. No shader recompile needed (pure CPU-side math).
- **2026-11-22** — User reported `textures/realm_tda/dqn_tongue` (a blended "trans" shader) wasn't
  transparent; root-caused and fixed (see Phase 1's world-iterator checkmark, bug #1). User's
  retest then surfaced bug #2 (ordinary world textures went dark/washed out — the
  `explicitlyDefined` fix) and bug #3 (`alphaFunc`-cutout shaders not cutting out — the
  `world_lightmap.frag` alpha-test gap), both also fixed same session. See the Phase 1 checkmark
  for all three; each needed a `world_lightmap.frag` or draw-dispatch-only fix (last one needed a
  shader recompile, the other two didn't).
- **2026-11-22** — User reported `misc_model`s looking overbright or flatly dark. Implemented real
  per-vertex diffuse lighting for `CGEN_LIGHTING_DIFFUSE` entities (Phase 4 item, see checkmark for
  the full design: `ComputeEntityLighting` replacing `ComputeEntityTint`, push-constant packing of
  ambient/directed/lightDir into previously-unused bytes, new `lighting_diffuse` pipeline axis,
  skinned-normal transform for MDS/MDM). Needed 4 shader recompiles.
- **2026-11-23** — User tested the above: no visible change anywhere, and flagged a *different*,
  broader symptom — most/all static map decorations (not just `rgbGen lightingDiffuse` entities)
  render darker than the reference engine, e.g. `models/mapobjects/malta/merch_barrel`
  (`rgbGen vertex` + `map $lightmap, blendFunc filter`). Investigated at length; several theories
  ruled out before landing on the current open question:
  - Confirmed this shader has nothing to do with `CGEN_LIGHTING_DIFFUSE` — the per-vertex lighting
    fix was never going to touch it.
  - Confirmed (via `r_vk_debugWeapon`) the object is never processed as a runtime entity at all —
    ruled out "no per-instance lightmap support for entities."
  - User clarified `misc_model` (unlike `misc_gamemodel`) is baked into static BSP geometry by
    q3map2 at compile time — confirmed this object is genuine `tr_world.surfaces[]` content
    (`MST_TRIANGLE_SOUP`, 434 such surfaces present in the load log, all accounted for in
    `vk_world_build`'s batch count), going through the same already-correct lightmap-fallback path
    as ordinary walls.
  - Chased an "overbright is only half-implemented" theory (a `GL_RGB_SCALE`-style per-blend-pass
    render-time multiplier that this port never built) — this looked promising enough to update
    Phase 1's checkmark, but further research **ruled it out**: the reference engine's actual
    overbright compensation is a whole-frame post-process (gamma ramp or FBO gamma shader), which
    can't explain one object being darker than another in the same frame.
  - A temporary diagnostic (`vk_world_build`, logs shader name/lightmapNum/sample vertex color for
    `CGEN_VERTEX` surfaces) confirmed real, moderate (not corrupted/near-zero) baked vertex-color
    values for this shader family — not an obviously-broken data-loading bug either.
  - **Root cause still open.** Next steps: compare `r_vertexLight`/`r_mapOverBrightBits`/`r_gamma`
    cvar values between the user's OpenGL1 and Vulkan test sessions (differing cvars could fully
    explain this with zero renderer bug involved); if they match, keep digging with the still-in-
    place diagnostic. Remove the diagnostic once resolved.
