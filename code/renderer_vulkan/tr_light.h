// tr_light.h -- light grid sampling, for shading entities (which have no lightmap of their own).

#ifndef TR_LIGHT_H_
#define TR_LIGHT_H_

// Trilinearly samples the BSP light grid at point; falls back to black/up if the map has no grid data (e.g. -nolight).
int R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );

// Classic id-tech-3 "overbright bits": map-authored lightmap/vertex-color/grid data is baked assuming
// the engine boosts it back up at load time (originally compensated by a hardware gamma ramp at
// render time; this renderer has none, so the boost always applies -- see docs/vulkan-renderer-roadmap.md).
// in/out may alias (safe to call in place).
void R_ShiftLightingBytes( const byte in[3], byte out[3] );

#endif // TR_LIGHT_H_
