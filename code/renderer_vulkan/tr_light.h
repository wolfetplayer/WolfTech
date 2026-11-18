// tr_light.h -- light grid sampling, for shading entities (which have no lightmap of their own).

#ifndef TR_LIGHT_H_
#define TR_LIGHT_H_

// Trilinearly samples the BSP light grid at point; falls back to black/up if the map has no grid data (e.g. -nolight).
int R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );

#endif // TR_LIGHT_H_
