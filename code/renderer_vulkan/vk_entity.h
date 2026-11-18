// vk_entity.h -- CPU-skinned entity mesh rendering (MD3/MDC frame-morph, MDS/MDM bone-weighted). MDX entities (skeleton-only, no mesh) still fall back to a marker cube (vk_draw3d.c).

#ifndef VK_ENTITY_H_
#define VK_ENTITY_H_

void vk_entity_init( void );
void vk_entity_shutdown( void );

// qtrue if this entity's model has real mesh data this module can draw (MD3/MDC/MDS/MDM).
qboolean vk_entity_has_mesh( const refEntity_t *ent );

// Draws every scene entity for which vk_entity_has_mesh is true.
void vk_entity_draw_all( const refEntity_t *entities, int numEntities, const float mvp[16] );

#endif // VK_ENTITY_H_
