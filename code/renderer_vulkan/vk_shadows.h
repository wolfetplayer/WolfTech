// vk_shadows.h -- stencil shadow volumes (r_shadows 2); see vk_shadows.c.

#ifndef VK_SHADOWS_H_
#define VK_SHADOWS_H_

void vk_shadows_init( void );
void vk_shadows_shutdown( void );

// qtrue if ent currently casts a stencil shadow volume (r_shadows 2, not RF_NOSHADOW/RF_DEPTHHACK).
qboolean vk_shadows_entity_casts( const refEntity_t *ent );

// Starts accumulating one entity's surfaces; call once before its first vk_shadows_add_surface.
void vk_shadows_begin_entity( void );

// Appends one surface's already-world-space positions (posStride apart) and triangles (md3Triangle_t-compatible).
void vk_shadows_add_surface( const float *positions, size_t posStride, int numVerts,
							   const void *trianglesPtr, int numTriangles );

// Stamps the entity's silhouette into the stencil buffer; call once per entity after its last vk_shadows_add_surface.
void vk_shadows_finish_entity( const refEntity_t *ent, const float viewProj[16], const float fogPush[12] );

// Resets the "any entity stamped this frame" flag; call once per real frame, before any entity is drawn.
void vk_shadows_begin_frame( void );

// Darkens the screen wherever this view's stencil is nonzero; call once per real scene, after its entities are stamped.
void vk_shadows_draw_darken( const float viewProj[16], const float fogPush[12] );

#endif // VK_SHADOWS_H_
