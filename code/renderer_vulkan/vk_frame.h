// vk_frame.h -- per-frame acquire/render-pass/present cycle.

#ifndef VK_FRAME_H_
#define VK_FRAME_H_

void vk_create_sync_primitives( void );
void vk_destroy_sync_primitives( void );

// Returns qfalse if the frame was aborted (e.g. swapchain resize) -- don't call vk_end_frame() then.
qboolean vk_begin_frame( void );
void vk_begin_render_pass( float r, float g, float b, float a );
void vk_end_render_pass( void );
void vk_end_frame( void );

#endif // VK_FRAME_H_
