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

## Current state (last updated 2026-11-19 — Phase 0 complete)

The Vulkan renderer is **real and live**, not a dead stub: `cl_renderer` (`code/client/cl_main.c`)
dynamically loads `renderer_vulkan_<arch>.<dll|so>`, it builds by default
(`BUILD_RENDERER_VULKAN` in the `Makefile` defaults to building), and selecting
`cl_renderer vulkan` + restart actually renders the game today. Starting from "clear the screen,"
it now has: world geometry with lightmaps (including non-stage-0 shader stages), frustum/PVS
culling, curved patches, MD3/MDC/MDS/MDM model loading + CPU skinning, tags/attachments, `.skins`
files, ambient light-grid sampling, dynamic lights (as a flat tint), wolf-fog, basic sky, scene
polys, a real (if minimal) multi-stage shader-script evaluator, mipmapping + anisotropic
filtering, baseline gamma correction, screenshot capture, and `r_showtris`/`r_showImages` debug
overlays — all of Phase 0 below, confirmed working at runtime as of this date.

It's still well behind both reference points on everything past Phase 0:
- **vs. vkQuake3**: no decals, no coronas/flares, no stencil shadows, no cinematics, frame pacing
  is single-buffered (no frames-in-flight), and resize does a full `vid_restart` instead of an
  in-place swapchain rebuild. World multi-stage drawing exists now but still shares stage 0's
  baked vertex color/UVs rather than evaluating each stage's own `rgbGen`/`tcMod` per frame.
- **vs. rend2**: no normal/specular/parallax/deluxe maps, no per-pixel lighting (entities get one
  flat RGBA tint for the whole model, world gets baked lightmap only), no tangent/normal data in
  the vertex formats at all, no HDR/bloom/tonemap, no SSAO/FXAA/DOF, no cubemap reflection probes,
  no shadow mapping, no overbright bits (Phase 1).

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

## Phase 1 — Generic multi-stage shader/draw core

The architectural centerpiece vkQuake3 calls `vk_shade_geometry.c` / `RB_StageIteratorGeneric`.
WolfTech's Vulkan renderer currently has per-caller, ad hoc stage loops (world draws stage 0 only;
entities/polys loop stages correctly but independently) instead of one shared generic path. Build
it once, make world/entity/poly submission all go through it.

- [ ] Shared multi-stage iterator: given a shader's `stages[]`, evaluate `rgbGen`/`alphaGen`,
      `tcGen`/`tcMod` (scroll/rotate/stretch/turb/transform — `entityTranslate` is currently a
      documented no-op, fix it), and emit one draw per stage through a common path used by world,
      entity, and poly geometry.
- [ ] Add real multitexture pipelines: `single_texture`, `multi_texture_mul` (modulate, e.g.
      lightmap × diffuse in one pass), `multi_texture_add` — currently only `single_texture` and
      `world_lightmap` exist as compiled shader programs.
- [ ] Dynamic lights as a real additive pass (`ProjectDlightTexture` equivalent) instead of the
      current flat-tint-per-entity hack — depends on entities/world carrying normal data (tracked
      formally in Phase 4, but a coarse per-face or per-vertex normal is enough to unblock this).
- [ ] Real fog pass: port BSP-brush volumetric fog (currently not implemented — only RTCW's global
      wolf-fog with fade transitions is ported); move from the current push-constant approximation
      to a proper fog-volume draw pass.
- [ ] `deformVertexes`: implement `wave`/`bulge`/`normal`/`autosprite`/`projectionShadow`/`text`
      (currently parsed but passthrough — only `move` actually displaces geometry).
- [ ] Portal/mirror clip-plane pipeline variants, with the same graceful fallback vkQuake3 uses for
      GPUs lacking `shaderClipDistance` (discard-in-fragment-shader via specialization constant).
      Required for RTCW mirrors, periscopes, and underwater clipping.
- [ ] Overbright bits: `tr_shade_calc.c`'s own comment already flags this
      ("`tr.identityLight` is always 1.0 here (no overbright bits)") — lightmaps are meant to store
      intensity above 1.0 and get compensated via `r_mapOverBrightBits`/`tr.overbrightBits`
      (classic id-tech-3 trick, see `code/renderer/tr_bsp.c` for the reference shift-and-scale
      logic). Without it, lightmapped world surfaces will read uniformly darker/flatter than the
      OpenGL1/rend2 output even after Phase 0's gamma fix lands — it's a separate, additive problem.

## Phase 2 — Classic feature parity (decals, flares, shadows, cinematics)

- [ ] Decals (`R_MarkFragments`, currently always returns 0): port the BSP mark-fragment clipping
      (`R_ChopPolyBehindPlane`/`R_BoxSurfaces_r` equivalents) — this is pure CPU geometry work that
      feeds the Phase 1 generic draw path; no decal-specific Vulkan code needed beyond that.
      Without this, bullet holes/scorch marks/blood never appear — highly visible in RTCW.
- [ ] Dynamic light coronas/flares (`RE_AddCoronaToScene`, currently stubbed): visibility-test
      against the depth buffer (occlusion query via `VkQueryPool`, or a depth readback/compare) with
      multi-frame fade in/out to avoid popping.
- [ ] Stencil shadow volumes (`r_shadows 2`): silhouette edge extraction, two-pass front/back-face
      stencil draw, full-screen darkening finish pass; plus the cheap `projectionShadow` blob
      fallback via `deformVertexes projectionShadow` (Phase 1 item). Needs stencil ops enabled in
      the pipeline (`stencilTestEnable` is currently hardcoded off).
- [ ] Validate curved-patch (Bezier) tessellation: `tr_patch.c` already exists and git history
      claims "curved patches" support — confirm LOD/stitching behavior matches `tr_curve.c`'s
      reference behavior (adaptive subdivision, t-junction stitching between differing LOD
      neighbors) rather than just "doesn't crash."
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
