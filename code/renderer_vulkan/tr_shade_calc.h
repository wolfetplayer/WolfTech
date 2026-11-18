// tr_shade_calc.h -- CPU evaluation of rgbGen/alphaGen/tcMod/deformVertexes, see tr_shade_calc.c for what's real vs. fallback.

#ifndef TR_SHADE_CALC_H_
#define TR_SHADE_CALC_H_

#include "tr_shader.h"

// fd->time (ms); call once per frame before drawing anything that uses the functions below.
void R_SetShaderTime( int timeMs );

// entityColor: ent->shaderRGBA or NULL. lightingTint: CGEN_LIGHTING_DIFFUSE substitute or NULL. baseColor: the mesh's own vertex color.
void R_CalcStageColorAlpha( const shaderStage_t *stage, const byte *entityColor, const byte *lightingTint,
							 const byte baseColor[4], byte outColor[4] );

// worldPos is the vertex's world-space xyz (TMOD_TURBULENT wiggles by position, not just by UV).
void R_CalcStageTexCoord( const shaderStage_t *stage, float baseS, float baseT, const float worldPos[3], float outST[2] );

// Only deformVertexes move is a real deformation; every other type passes basePos through unchanged.
void R_DeformPosition( const shader_t *shader, const float basePos[3], float outPos[3] );

#endif // TR_SHADE_CALC_H_
