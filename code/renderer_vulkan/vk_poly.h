// vk_poly.h -- draws the scene's poly list (tr_scene.h's RE_AddPolyToScene/RE_AddPolysToScene): tracers, particles, trails, beams, and impact marks, all submitted by cgame as plain textured polys.

#ifndef VK_POLY_H_
#define VK_POLY_H_

void vk_poly_init( void );
void vk_poly_shutdown( void );

// fogPush is tr_fog.h's push-constant block.
void vk_poly_draw( const float mvp[16], const float fogPush[12] );

#endif // VK_POLY_H_
