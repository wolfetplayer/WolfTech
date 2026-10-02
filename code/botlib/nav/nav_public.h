// nav_public.h -- plain-C entry points for the Recast/Detour nav module.

#ifndef __NAV_PUBLIC_H
#define __NAV_PUBLIC_H

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	int failure;
	float movedir[3];
	// set when the next step is an off-mesh link (navgen_offmesh.cpp) - needs an explicit jump, not just movement.
	int onOffMeshConnection;
	int onLadderConnection; // set when that link is a ladder - forced forward/back + view lock instead of a jump
	float ladderStart[3];
	float ladderEnd[3];
	float ladderWallNormal[3]; // outward normal of the ladder's wall face, quake space
} navMoveResult_t;

void Nav_Init( void );
void Nav_Shutdown( void );

// loads <mapname>'s per-class .navcache files; call once per map load.
void Nav_LoadMap( const char *mapname );

// selects which class's navmesh subsequent Nav_* calls query
void Nav_SelectClass( int classIndex );

int Nav_PointToPoly( const float *point );
int Nav_MoveToGoal( navMoveResult_t *result, const float *start, const float *goal, int agentId );
int Nav_TravelTimeEstimate( const float *start, const float *goal );
int Nav_Reachable( const float *point );
typedef int ( *navLineClearFn_t )( const float *from, const float *to, const float *mins, const float *maxs, int contentMask );
void Nav_SetLineClearFn( navLineClearFn_t fn );
typedef int ( *navGameVisibleFn_t )( const float *srcPos, int srcNum, const float *destPos, int destNum );
void Nav_SetGameVisibleFn( navGameVisibleFn_t fn );
int Nav_FindHidePosition( const float *from, const float *threat, float radius, int selfNum, int enemyNum, float *outPos );
int Nav_FindAttackSpot( const float *from, const float *target, float minRange, float maxRange, float *outPos );
int Nav_GetRouteFirstVisPos( const float *srcpos, const float *destpos, float *outPos );
int Nav_AddObstacle( const float *absmin, const float *absmax );
void Nav_RemoveObstacle( int handle );

// ticks pending obstacle add/removes into incremental tile rebuilds; call once per server frame.
void Nav_UpdateObstacles( void );

// call once, when an AI dies/disconnects, to drop its crowd agent (see Nav_MoveToGoal's agentId).
void Nav_RemoveAgent( int agentId );
// advances local avoidance for every loaded class; call once per server frame, after Nav_UpdateObstacles.
void Nav_CrowdUpdate( float dt );

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
