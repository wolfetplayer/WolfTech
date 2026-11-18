// vk_sky.h -- static, camera-centered sky box (see tr_world.hasSky/skyOuterBox, set from a shader's "skyparms").

#ifndef VK_SKY_H_
#define VK_SKY_H_

void vk_sky_init( void );
void vk_sky_shutdown( void );

// No-op if the loaded world has no sky shader. fogPush is tr_fog.h's push-constant block (ignored -- sky isn't fogged).
void vk_sky_draw( const vec3_t camPos, const float mvp[16], const float fogPush[12] );

#endif // VK_SKY_H_
