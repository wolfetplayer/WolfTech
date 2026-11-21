// vk_draw2d.h -- immediate-mode 2D quad recording for the UI/console; no deferred/sorted batching.

#ifndef VK_DRAW2D_H_
#define VK_DRAW2D_H_

#include "tr_shader.h"

void vk_draw2d_init( void );
void vk_draw2d_shutdown( void );

// Call once per frame, after vk_begin_render_pass() and before any quads.
void vk_draw2d_begin_frame( void );

void vk_draw2d_set_color( const float *rgba );

void vk_draw2d_quad( float x, float y, float w, float h,
					  float s1, float t1, float s2, float t2,
					  shader_t *shader );

// Top edge corners shift by skewLeft/skewRight, bottom edge stays put.
void vk_draw2d_quad_skewed( float x, float y, float w, float h,
							 float s1, float t1, float s2, float t2,
							 shader_t *shader, float skewLeft, float skewRight );

// angle is in turns (0..1 = a full rotation), about the quad's own center.
void vk_draw2d_quad_rotated( float x, float y, float w, float h,
							   float s1, float t1, float s2, float t2,
							   shader_t *shader, float angle );

// gradientColor may be NULL (defaults to white); see vk_draw2d.c for why gradientType isn't a parameter.
void vk_draw2d_quad_gradient( float x, float y, float w, float h,
								float s1, float t1, float s2, float t2,
								shader_t *shader, const float *gradientColor );

// Center fan + a 1px alpha-feathered boundary ring (anti-aliases without MSAA).
void vk_draw2d_quad_rounded( float x, float y, float w, float h,
							  float s1, float t1, float s2, float t2,
							  shader_t *shader, float radius );

// Draws image's whole texture into the given screen rect, bypassing the shader system -- for r_showImages.
void vk_draw2d_raw_image( float x, float y, float w, float h, image_t *image );

#endif // VK_DRAW2D_H_
