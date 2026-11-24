// tr_shade_calc.h -- CPU evaluation of rgbGen/alphaGen/tcMod/deformVertexes, see tr_shade_calc.c for what's real vs. fallback.

#ifndef TR_SHADE_CALC_H_
#define TR_SHADE_CALC_H_

#include "tr_shader.h"

// fd->time (ms); call once per frame before drawing anything that uses the functions below.
void R_SetShaderTime( int timeMs );

// Resolves animmap's current frame from shaderTime; returns stage->image unchanged for non-animated stages.
image_t *R_GetStageImage( const shaderStage_t *stage );

// entityColor: ent->shaderRGBA or NULL. baseColor: the mesh's own vertex color. CGEN_LIGHTING_DIFFUSE
// resolves to flat white here -- entities needing real per-vertex diffuse lighting get it from the
// GPU instead (entity_lerp.vert/entity_skin.vert), see vk_entity.c's ComputeEntityLighting.
void R_CalcStageColorAlpha( const shaderStage_t *stage, const byte *entityColor,
							 const byte baseColor[4], byte outColor[4] );

// worldPos is the vertex's world-space xyz (TMOD_TURBULENT wiggles by position, not just by UV).
// entityTranslate is refEntity_t.shaderTexCoord (TMOD_ENTITY_TRANSLATE's scroll offset), or NULL where
// there's no entity context (world surfaces, scene polys).
void R_CalcStageTexCoord( const shaderStage_t *stage, float baseS, float baseT, const float worldPos[3],
						   const float entityTranslate[2], float outST[2] );

void R_CalcEnvironmentTexCoord( const float normal[3], const float viewerOrigin[3], const float pos[3], float outST[2] );

void R_DeformPosition( const shader_t *shader, const float basePos[3], const float normal[3], float texS, float outPos[3] );

#endif // TR_SHADE_CALC_H_
