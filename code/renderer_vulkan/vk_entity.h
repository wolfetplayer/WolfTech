// vk_entity.h -- CPU-skinned entity mesh rendering (MD3/MDC frame-morph, MDS/MDM bone-weighted). MDX entities (skeleton-only, no mesh) still fall back to a marker cube (vk_draw3d.c).

#ifndef VK_ENTITY_H_
#define VK_ENTITY_H_

#include "tr_model.h" // model_t

void vk_entity_init( void );
void vk_entity_shutdown( void );

// Builds this model's static per-surface GPU vertex-animation buffer(s) (MD3/MDC vertex-lerp frames, or MDS/MDM bind pose -- stage 3) right after tr_model.c finishes parsing it; stores the result in model->vkAnimData. No-op for model types this doesn't apply to.
void vk_entity_register_model( model_t *mod );

// Resets the shared vertex/index write cursors; call once per real frame (vk_entity_draw_all can run more than once per frame).
void vk_entity_begin_frame( void );

// qtrue if this entity's model has real mesh data this module can draw (MD3/MDC/MDS/MDM).
qboolean vk_entity_has_mesh( const refEntity_t *ent );

// Draws every scene entity for which vk_entity_has_mesh is true (fogPush is tr_fog.h's push-constant block).
void vk_entity_draw_all( const refEntity_t *entities, int numEntities, const float mvp[16], const float fogPush[12] );

#endif // VK_ENTITY_H_
