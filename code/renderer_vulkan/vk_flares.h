// vk_flares.h -- coronas (RE_AddCoronaToScene): occlusion-query visibility test + multi-frame fade, see vk_flares.c.

#ifndef VK_FLARES_H_
#define VK_FLARES_H_

void vk_flares_init( void );
void vk_flares_shutdown( void );

// Drops all persistent per-corona fade/occlusion state; call once per RE_LoadWorldMap (ids are entity numbers, reused across maps).
void vk_flares_clear( void );

// Resets this frame's occlusion queries; must run before vk_begin_render_pass (vkCmdResetQueryPool can't run inside one).
void vk_flares_begin_frame( void );

// Tests/fades/draws this frame's scene coronas; call once per real scene (skip the sky room and mirror/portal recursion).
void vk_flares_draw( const refdef_t *fd, const float viewProj[16], const float fogPush[12] );

#endif // VK_FLARES_H_
