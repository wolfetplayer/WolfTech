// nav_public.h -- plain-C entry points for the Recast/Detour nav module.

#ifndef __NAV_PUBLIC_H
#define __NAV_PUBLIC_H

#ifdef __cplusplus
extern "C" {
#endif

// deliberately not botlib's bot_moveresult_t: Phase 3 copies these two
// fields across at the ai_cast_funcs.c call site instead of coupling
// this module to botlib's header, which isn't safe to include standalone.
typedef struct {
	int failure;
	float movedir[3];
	// set when the next step is an off-mesh link (navgen_offmesh.cpp) - needs an explicit jump, not just movement.
	int onOffMeshConnection;
} navMoveResult_t;

void Nav_Init( void );
void Nav_Shutdown( void );

// loads <mapname>'s per-class .navcache files; call once per map load.
void Nav_LoadMap( const char *mapname );

// selects which class's navmesh subsequent Nav_* calls query, mirroring trap_AAS_SetCurrentWorld.
void Nav_SelectClass( int classIndex );

int Nav_PointToPoly( const float *point );
int Nav_MoveToGoal( navMoveResult_t *result, const float *start, const float *goal );
int Nav_TravelTimeEstimate( const float *start, const float *goal );
int Nav_Reachable( const float *point );
int Nav_FindHidePosition( const float *from, const float *threat, float radius, float *outPos );
int Nav_FindAttackSpot( const float *from, const float *target, float minRange, float maxRange, float *outPos );

// Phase 4: dynamic obstacles for doors/movers/constructibles. absmin/absmax are in
// quake space; returns a handle (>=1) for Nav_RemoveObstacle, or 0 on failure.
int Nav_AddObstacle( const float *absmin, const float *absmax );
void Nav_RemoveObstacle( int handle );

// ticks pending obstacle add/removes into incremental tile rebuilds; call once per server frame.
void Nav_UpdateObstacles( void );

// debug: runs Nav_MoveToGoal/Nav_TravelTimeEstimate between two points and prints to the engine console.
void Nav_TestPath( const float *start, const float *end );

// debug: writes classIndex's baked navmesh polygons to navdump_<class>.obj (quake space) for visual inspection.
void Nav_DumpMesh( int classIndex );

// live debug: draws nearby navmesh polys (green=reachable/red=not) via the engine's bot debug-polygon channel.
void Nav_DebugShowNearby( const float *origin, float radius );
// live debug: draws the straight path from start to goal as lines; slot 0..3 (see nav_debug.cpp) keeps AI separate.
void Nav_DebugShowPath( const float *start, const float *goal, int slot );
// clears anything currently shown by either of the above.
void Nav_DebugClear( void );

#ifdef __cplusplus
}
#endif

#endif // __NAV_PUBLIC_H
