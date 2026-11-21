// tr_fog.h -- RTCW's global "wolf fog" (trigger_fog etc) plus a camera-containment check for BSP fog brushes.

#ifndef TR_FOG_H_
#define TR_FOG_H_

void R_InitFog( void );
void R_SetFog( int fogvar, int var1, int var2, float r, float g, float b, float density );

// Call once per rendered frame, before R_GetFogPushConstants, so R_SetFog's transition timing stays in sync.
void R_SetFogFrameTime( int time );

// Call once per frame with the camera's eye position; overrides the scripted fog for this frame if inside a BSP fog volume.
void R_SetBSPFogForPoint( const vec3_t point );

// Fills the 12-float {camPos,pad / fogColor,pad / param0,param1,mode,enabled} block every 3D pipeline's shared push constant uses.
void R_GetFogPushConstants( const vec3_t camPos, float out[12] );

#endif // TR_FOG_H_
