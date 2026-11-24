// vk_profile.h -- opt-in CPU/GPU timing for the Vulkan renderer's major per-frame passes (r_vk_profile). See vk_profile.c.

#ifndef VK_PROFILE_H_
#define VK_PROFILE_H_

// One entry per major pass in tr_init.c's R_RenderViewInternal, in the order they run each frame.
// Brush-model entities draw inside vk_entity_draw_all (see vk_entity.c), so VKPROF_ENTITIES already
// covers them -- no separate zone. 2D (HUD/menu) drawing is out of scope: it has no single call site
// to bracket, since RE_StretchPic et al. are invoked piecemeal by cgame/UI across the whole frame.
//
// VKPROF_ENTITY_BONES/VKPROF_ENTITY_LERP are sub-zones *inside* VKPROF_ENTITIES (vk_entity.c calls
// them once per entity / once per surface, so their CPU time sums across the whole frame same as
// any other zone) -- added to break down where MDS/MDM (bone-skinned AI) cost actually goes: CPU
// bone-matrix computation (WriteBoneMatricesMDS/MDM) vs. the CPU vertex-lerp kept around only to
// feed tcMod turb/shadow volumes (LerpMDSSurfaceVerts etc. -- GPU skinning already replaced its
// role in the actual draw, see Phase 4's Changelog). Both are pure CPU work with no GPU commands in
// between, so their "gpu" column will read ~0 -- that's expected, not a bug.
typedef enum {
	VKPROF_SKY,
	VKPROF_WORLD,
	VKPROF_FOLIAGE,
	VKPROF_ENTITIES,
	VKPROF_ENTITY_BONES,
	VKPROF_ENTITY_LERP,
	VKPROF_TRANSPARENT,
	VKPROF_SHADOWS,
	VKPROF_POLY,
	VKPROF_FLARES,
	VKPROF_ZONE_COUNT
} vkProfZone_t;

void vk_profile_init( void );
void vk_profile_shutdown( void );

// Call once per real frame: begin right after vk_begin_frame but before vk_begin_render_pass
// (resetting last frame's GPU queries has to run outside a render pass instance -- the same
// constraint vk_flares_begin_frame already works around). End right before vk_end_frame.
void vk_profile_frame_begin( void );
void vk_profile_frame_end( void );

// Wrap each pass's draw call: CPU wall-clock time plus a GPU timestamp pair. Both are no-ops (no
// timer reads, no GPU commands recorded) unless r_vk_profile is nonzero. Each zone has room for
// exactly one begin/end query pair per frame slot -- call this at most once per zone per real
// frame (the top-level passes in R_RenderViewInternal all qualify; a mirror/portal recursion can
// violate this rarely, see vk_profile.c's "Known limitation" comment for what that does to the
// GPU numbers). For anything called more than once per frame (per-entity, per-surface), use
// vk_profile_cpu_zone_begin/end below instead -- writing the same GPU query twice without
// resetting it in between is invalid Vulkan usage.
void vk_profile_zone_begin( vkProfZone_t zone );
void vk_profile_zone_end( vkProfZone_t zone );

// CPU-only timing, safe to call any number of times per frame for the same zone (e.g. once per
// entity or once per surface) -- no GPU query involved, so nothing to reset between calls. Time
// is summed exactly like vk_profile_zone_begin/end's CPU side. Only meaningful for zones that do
// no GPU work between begin/end (VKPROF_ENTITY_BONES/VKPROF_ENTITY_LERP) -- their "gpu" column in
// the printed summary will always read ~0, which is correct, not "not measured".
void vk_profile_cpu_zone_begin( vkProfZone_t zone );
void vk_profile_cpu_zone_end( vkProfZone_t zone );

#endif // VK_PROFILE_H_
