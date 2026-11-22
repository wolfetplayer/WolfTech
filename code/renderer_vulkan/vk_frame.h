// vk_frame.h -- per-frame acquire/render-pass/present cycle. See vk_instance.h's VK_FRAMES_IN_FLIGHT.

#ifndef VK_FRAME_H_
#define VK_FRAME_H_

void vk_create_sync_primitives( void );
void vk_destroy_sync_primitives( void );

// Which of the VK_FRAMES_IN_FLIGHT slots the frame being recorded owns; every per-frame dynamic buffer elsewhere indexes its region by this, not by swapchain image (the two counts can differ).
uint32_t vk_current_frame_slot( void );

// Returns qfalse if the frame was aborted (e.g. swapchain resize) -- don't call vk_end_frame() then.
qboolean vk_begin_frame( void );
void vk_begin_render_pass( float r, float g, float b, float a );
void vk_end_render_pass( void );
void vk_end_frame( void );

// Mid-render-pass depth-only clear (the render pass itself only clears at load); see tr_scene.h's sky-portal tracking.
void vk_clear_depth_only( void );

#endif // VK_FRAME_H_
