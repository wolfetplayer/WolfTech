// vk_entity.h -- CPU-lerped MD3/MDC entity mesh rendering. MDS/MDM/MDX entities still fall back to a marker cube (vk_draw3d.c) until real skinning lands.

#ifndef VK_ENTITY_H_
#define VK_ENTITY_H_

void vk_entity_init( void );
void vk_entity_shutdown( void );

// qtrue if this entity's model has real mesh data this module can draw (currently: MD3/MDC).
qboolean vk_entity_has_mesh( const refEntity_t *ent );

// Draws every scene entity for which vk_entity_has_mesh is true.
void vk_entity_draw_all( const refEntity_t *entities, int numEntities, const float mvp[16] );

#endif // VK_ENTITY_H_
