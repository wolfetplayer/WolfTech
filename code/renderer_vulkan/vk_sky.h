// vk_sky.h -- static, camera-centered sky box (see tr_world.hasSky/skyOuterBox, set from a shader's "skyparms").

#ifndef VK_SKY_H_
#define VK_SKY_H_

void vk_sky_init( void );
void vk_sky_shutdown( void );

// No-op if the loaded world has no sky shader. fogPush is ignored (sky isn't fogged); zFar sizes the box so its farthest corner always stays inside the current scene's far clip plane.
void vk_sky_draw( const vec3_t camPos, const float mvp[16], const float fogPush[12], float zFar );

#endif // VK_SKY_H_
