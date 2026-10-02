// nav_query.cpp -- query implementations mapped onto the old trap_AAS_* taxonomy; see the migration plan for the mapping table.

#include "nav_local.h"
#include "nav_public.h"

#include <cstring>
#include <cstdio>

extern "C" {
#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
}

// snap box tried first; tight so stacked floors on a multi-story map don't compete for the same point.
static const float NAV_SEARCH_EXTENTS_TIGHT[3] = { 48, 48, 48 };
// fallback for points genuinely off the mesh (mid-air, falling).
static const float NAV_SEARCH_EXTENTS_WIDE[3] = { 96, 160, 96 };

// how close to a jump/drop link's takeoff point before we actually commit to it.
static const float NAV_OFFMESH_JUMP_RANGE = 48.0f;

// landing this much below the takeoff counts as a plain fall, not a jump.
static const float NAV_OFFMESH_DROP_TOLERANCE = 24.0f;

// below this fraction of desired speed, avoidance counts as deadlocked rather than just yielding.
static const float NAV_CROWD_STUCK_SPEED_FRAC = 0.15f;
// consecutive stalled calls before falling back to raw path steering for that agent.
static const int NAV_CROWD_STUCK_TICKS = 5;

// a crowd agent's goal must move this far (squared) before it is replanned.
// a crowd agent whose corridor is further than this from its entity was teleported.
static const float NAV_CROWD_DESYNC_DIST_SQ = 96.0f * 96.0f;
static const float NAV_CROWD_DESYNC_HEIGHT = 96.0f;
static const float NAV_CROWD_RETARGET_DIST_SQ = 64.0f * 64.0f;
// a walkability raycast only counts as clear if the surface it ended on is this close to the target's height.
static const float NAV_RAYCAST_LEVEL_TOLERANCE = 64.0f;

// navmesh points are floor-level; entity origins (and AAS waypoints) sit this far above.
static const float NAV_ORIGIN_ABOVE_FLOOR = 24.0f;
// standing eye height above the origin of the thing being hidden from.
static const float NAV_EYE_ABOVE_ORIGIN = 40.0f;
// how far a threat can lean sideways to look round a corner.
static const float NAV_PEEK_OFFSET = 16.0f;
// standing height of whatever an attack spot has to see (the player's bbox).
static const float NAV_TARGET_BODY_HEIGHT = 72.0f;
// contents masks of the game's sight and shot traces.
static const int NAV_MASK_SIGHT = CONTENTS_SOLID | CONTENTS_AI_NOSIGHT;
static const int NAV_MASK_SHOT = CONTENTS_SOLID | CONTENTS_CLIPSHOT;
// an attack spot is never this close to the target.
static const float NAV_ATTACK_MIN_TARGET_DIST = 64.0f;
// polys gathered per spot search, and how many ranked spots get traced.
static const int NAV_MAX_SPOT_POLYS = 256;
static const int NAV_MAX_SPOT_TESTS = 128;
// a hiding spot must be at least this far from the threat, and at least this far from where we stand.
static const float NAV_HIDE_MIN_THREAT_DIST = 96.0f;
static const float NAV_HIDE_MIN_MOVE = 48.0f;
// a hiding spot must stay hidden from where the threat can get to: this far around it and along its way to us.
static const float NAV_HIDE_CLOUD_RADIUS = 500.0f;
static const float NAV_HIDE_CLOUD_NEAR = 200.0f;
static const float NAV_HIDE_CLOUD_FAR = 400.0f;
// a ring sample is dropped if no ground lies this close to it.
static const float NAV_HIDE_CLOUD_SNAP = 150.0f;
// route samples from the threat to us: one every STEP up to REACH.
static const float NAV_HIDE_CLOUD_STEP = 100.0f;
static const float NAV_HIDE_CLOUD_REACH = 500.0f;
static const int NAV_HIDE_CLOUD_SECTORS = 8;
static const int NAV_HIDE_CLOUD_PATH_CORNERS = 16;
static const int NAV_HIDE_CLOUD_MAX = 16;
// cloud eyes a hiding spot may still be seen from.
static const int NAV_HIDE_CLOUD_MAX_SEEN = 0;

static navLineClearFn_t navLineClear = NULL;
static navGameVisibleFn_t navGameVisible = NULL;

// cap on world traces per spot query.
static const int NAV_MAX_QUERY_TRACES = 1000;
static int navTraceBudget = 0;

static void SwapYZ( const float *in, float *out ) {
	float y = in[1];
	out[0] = in[0];
	out[1] = in[2];
	out[2] = y;
}

/*
===========
Nav_FindNearest

pos and nearest are both in navmesh (Y-up) space. Tight box first: Detour scores a poly the
point is over by vertical gap alone, so a tall box picks the wrong story near stairwells and balconies.
===========
*/
static bool Nav_FindNearest( NavData_t *data, const float *pos, dtPolyRef *ref, float *nearest ) {
	dtQueryFilter filter;
	if ( dtStatusSucceed( data->query->findNearestPoly( pos, NAV_SEARCH_EXTENTS_TIGHT, &filter, ref, nearest ) ) && *ref != 0 ) {
		return true;
	}
	if ( dtStatusFailed( data->query->findNearestPoly( pos, NAV_SEARCH_EXTENTS_WIDE, &filter, ref, nearest ) ) ) {
		return false;
	}
	return *ref != 0;
}

/*
====================
Nav_ComputeStraightPath

start, goal and straight are all in navmesh (Y-up) space. outFlags[i] carries
DT_STRAIGHTPATH_OFFMESH_CONNECTION when straight[i] is the takeoff point of a
bridged jump/step-across link (navgen_offmesh.cpp), not ordinary floor.
====================
*/
static bool Nav_ComputeStraightPath( NavData_t *data, const float *start, const float *goal,
									  float *straight, unsigned char *outFlags, dtPolyRef *outRefs,
									  int maxStraight, int *straightCount ) {
	dtQueryFilter filter;
	dtPolyRef startRef, endRef;
	float startNearest[3], endNearest[3];

	if ( !Nav_FindNearest( data, start, &startRef, startNearest ) ) {
		return false;
	}
	if ( !Nav_FindNearest( data, goal, &endRef, endNearest ) ) {
		return false;
	}

	// 512 polys: a smaller buffer makes findPath truncate on long cross-map routes, which looks like "unreachable".
	dtPolyRef path[512];
	int pathCount = 0;
	if ( dtStatusFailed( data->query->findPath( startRef, endRef, startNearest, endNearest, &filter, path, &pathCount, 512 ) ) || pathCount == 0 ) {
		return false;
	}
	// a partial path (ending short of endRef) means start/goal aren't connected, not a real route.
	if ( path[pathCount - 1] != endRef ) {
		return false;
	}

	if ( dtStatusFailed( data->query->findStraightPath( startNearest, endNearest, path, pathCount,
														 straight, outFlags, outRefs, straightCount, maxStraight ) ) ) {
		return false;
	}
	if ( *straightCount <= 0 ) {
		return false;
	}
	return true;
}

/*
================
Nav_RaycastClear

true if a straight walkability ray from fromPos (in poly fromRef) reaches toPos with no wall hit.
A walkability test, not line of sight - and raycast is 2D, so the surface it ended on must also
be near toPos's height. Only the no-world-trace fallback and Nav_GetRouteFirstVisPos use this.
================
*/
static bool Nav_RaycastClear( NavData_t *data, dtPolyRef fromRef, const float *fromPos, const float *toPos ) {
	dtQueryFilter filter;
	float t = 0.0f;
	float hitNormal[3];
	dtPolyRef path[64];
	int pathCount = 0;
	if ( dtStatusFailed( data->query->raycast( fromRef, fromPos, toPos, &filter, &t, hitNormal, path, &pathCount, 64 ) ) ) {
		return false;
	}
	if ( t < 1.0f || pathCount == 0 ) {
		return false;
	}

	float surfacePt[3];
	bool overPoly;
	if ( dtStatusFailed( data->query->closestPointOnPoly( path[pathCount - 1], toPos, surfacePt, &overPoly ) ) ) {
		return false;
	}

	float levelDelta = surfacePt[1] - toPos[1];
	if ( levelDelta < 0.0f ) {
		levelDelta = -levelDelta;
	}
	return levelDelta <= NAV_RAYCAST_LEVEL_TOLERANCE;
}

/*
==============
Nav_PointToPoly
==============
*/
int Nav_PointToPoly( const float *point ) {
	NavData_t *data = Nav_CurrentData();
	if ( !data ) {
		return 0;
	}
	vec3_t navPoint;
	SwapYZ( point, navPoint );

	dtPolyRef ref;
	float nearest[3];
	return Nav_FindNearest( data, navPoint, &ref, nearest ) ? (int)ref : 0;
}

/*
============
Nav_Reachable
============
*/
int Nav_Reachable( const float *point ) {
	return Nav_PointToPoly( point ) != 0;
}

/*
========================
Nav_GetOrAddCrowdAgent

Looks up agentId's crowd-agent index for data's class, lazily adding it if this is the first
call for that agent. Returns -1 if there's no crowd for this class, agentId isn't trackable
(Nav_TestPath's debug calls pass -1), or the add failed - callers fall back to raw straight-path
steering in that case, same as if the crowd module weren't vendored at all.
========================
*/
static int Nav_GetOrAddCrowdAgent( NavData_t *data, int agentId, const float *navPos ) {
	if ( !data->crowd || agentId < 0 || agentId >= NAV_MAX_TRACKED_AGENTS ) {
		return -1;
	}

	int idx = data->crowdAgentIdx[agentId];
	if ( idx >= 0 ) {
		const dtCrowdAgent *ag = data->crowd->getAgent( idx );
		if ( ag && ag->active ) {
			// a respawned AI keeps its agent but its corridor is still where it died, so drop it
			const float *corridorPos = ag->corridor.getPos();
			if ( dtVdist2DSqr( corridorPos, navPos ) <= NAV_CROWD_DESYNC_DIST_SQ && fabsf( corridorPos[1] - navPos[1] ) <= NAV_CROWD_DESYNC_HEIGHT ) {
				return idx;
			}
			data->crowd->removeAgent( idx );
		}
		// slot was reused/removed out from under us; fall through and re-add.
		data->crowdAgentIdx[agentId] = -1;
	}

	const navGenClass_t *cls = &navGenClasses[navCurrentClass];
	dtCrowdAgentParams params;
	memset( &params, 0, sizeof( params ) );
	params.radius = cls->radius;
	params.height = cls->height;
	// well above maxSpeed so direction reversals don't take ~2s (vel is rate-limited, not snapped).
	params.maxAcceleration = 3000.0f;
	params.maxSpeed = 400.0f; // matches trap_EA_Move's existing hardcoded speed cap
	params.collisionQueryRange = params.radius * 8.0f;
	params.pathOptimizationRange = params.radius * 30.0f;
	params.separationWeight = 2.0f;
	params.updateFlags = DT_CROWD_ANTICIPATE_TURNS | DT_CROWD_OBSTACLE_AVOIDANCE |
						  DT_CROWD_SEPARATION | DT_CROWD_OPTIMIZE_VIS | DT_CROWD_OPTIMIZE_TOPO;
	params.obstacleAvoidanceType = 0;

	idx = data->crowd->addAgent( navPos, &params );
	data->crowdAgentIdx[agentId] = idx;
	data->crowdStuckTicks[agentId] = 0; // fresh agent - don't inherit a stale slot's stuck count
	return idx;
}

/*
=============
Nav_MoveToGoal

agentId (the entity number; -1 for none) tracks a persistent dtCrowd agent across calls so
nearby bots steer around each other (RVO/separation) instead of all following the identical
straight-path line. The crowd is advisory only - Nav_CrowdUpdate() (sv_bot.c's SV_BotFrame)
computes agent->vel once a frame, this just reads it back as the steering direction; actual
movement still runs through the same trap_EA_Move -> Pmove path as always, so a crowd failure
here never stops movement, it just falls back to the original raw straight-path direction.
=============
*/
int Nav_MoveToGoal( navMoveResult_t *result, const float *start, const float *goal, int agentId ) {
	memset( result, 0, sizeof( *result ) );

	NavData_t *data = Nav_CurrentData();
	vec3_t navStart, navGoal;
	SwapYZ( start, navStart );
	SwapYZ( goal, navGoal );

	float straight[32 * 3];
	unsigned char straightFlags[32];
	dtPolyRef straightRefs[32];
	int straightCount = 0;
	if ( !data || !Nav_ComputeStraightPath( data, navStart, navGoal, straight, straightFlags, straightRefs, 32, &straightCount ) ) {
		result->failure = 1;
		return 0;
	}

	// off-mesh links get flagged one step early, at index 1, or it's too late to grab.
	int offMeshIdx = -1;
	if ( straightFlags[0] & DT_STRAIGHTPATH_OFFMESH_CONNECTION ) {
		offMeshIdx = 0;
	} else if ( straightCount > 1 && ( straightFlags[1] & DT_STRAIGHTPATH_OFFMESH_CONNECTION ) ) {
		offMeshIdx = 1;
	}
	bool offMeshCommitted = false; // close enough to the link to steer at its landing point
	if ( offMeshIdx >= 0 ) {
		const dtOffMeshConnection *conn = data->mesh->getOffMeshConnectionByRef( straightRefs[offMeshIdx] );
		if ( conn ) {
			const NavCacheOffMeshConn *rec = Nav_LookupOffMeshConn( data, conn->userId );
			if ( rec && rec->isLadder ) {
				result->onOffMeshConnection = 1;
				result->onLadderConnection = 1;
				SwapYZ( &conn->pos[0], result->ladderStart );
				SwapYZ( &conn->pos[3], result->ladderEnd );
				SwapYZ( rec->wallNormal, result->ladderWallNormal );
				return 1;
			}
		}
		// jump/drop links need to be close before we commit, unlike ladders above.
		if ( VectorDistance( navStart, &straight[offMeshIdx * 3] ) <= NAV_OFFMESH_JUMP_RANGE ) {
			offMeshCommitted = true;
			// a plain drop just needs a walk off the ledge, not a jump.
			float takeoffY = straight[offMeshIdx * 3 + 1];
			float landingY = ( offMeshIdx + 1 < straightCount ) ? straight[( offMeshIdx + 1 ) * 3 + 1] : takeoffY;
			if ( landingY >= takeoffY - NAV_OFFMESH_DROP_TOLERANCE ) {
				result->onOffMeshConnection = 1;
			}
		}
	}

	const float *target = ( straightCount > 1 ) ? &straight[3] : &straight[0];
	// committed: steer at the landing point, not the now-underfoot takeoff point.
	if ( offMeshCommitted && offMeshIdx + 1 < straightCount ) {
		target = &straight[( offMeshIdx + 1 ) * 3];
	}

	vec3_t navDir;
	bool haveDir = false;

	// skip crowd steering once committed to a jump/drop link (ladders already returned above).
	if ( !offMeshCommitted ) {
		int agentIdx = Nav_GetOrAddCrowdAgent( data, agentId, navStart );
		if ( agentIdx >= 0 ) {
			dtCrowdAgent *ag = data->crowd->getEditableAgent( agentIdx );
			// resync every call so the crowd's own tracked position can't drift from the real one.
			VectorCopy( navStart, ag->npos );
			if ( !ag->targetRef || dtVdist2DSqr( ag->targetPos, navGoal ) > NAV_CROWD_RETARGET_DIST_SQ ) {
				dtPolyRef endRef;
				float endNearest[3];
				if ( Nav_FindNearest( data, navGoal, &endRef, endNearest ) ) {
					data->crowd->requestMoveTarget( agentIdx, endRef, endNearest );
				}
			}

			// whatever Nav_CrowdUpdate() computed last frame - one frame of latency.
			VectorCopy( ag->vel, navDir );
			navDir[1] = 0.0f;
			haveDir = VectorNormalize2( navDir, navDir ) > 0.0001f;

			// deadlocked (not just yielding to a neighbor) if avoidance holds speed near zero.
			float actualSpeed = haveDir ? VectorLength( ag->vel ) : 0.0f;
			if ( ag->desiredSpeed > 10.0f && actualSpeed < ag->desiredSpeed * NAV_CROWD_STUCK_SPEED_FRAC ) {
				if ( data->crowdStuckTicks[agentId] < 255 ) {
					data->crowdStuckTicks[agentId]++;
				}
			} else {
				data->crowdStuckTicks[agentId] = 0;
			}
			if ( data->crowdStuckTicks[agentId] > NAV_CROWD_STUCK_TICKS ) {
				haveDir = false;
			}
		}
	}

	if ( !haveDir ) {
		VectorSubtract( target, navStart, navDir );
		navDir[1] = 0.0f; // navmesh Y = vertical (quake Z) after SwapYZ
		if ( VectorNormalize2( navDir, navDir ) < 0.0001f ) {
			result->failure = 1;
			return 0;
		}
	}

	SwapYZ( navDir, result->movedir );
	return 1;
}

/*
=====================
Nav_TravelTimeEstimate

Returns path length in world units, not real travel time; good enough for
the relative "did this move help" comparisons ai_cast_funcs.c makes today.
Segment length is invariant under the Y/Z swap, so no conversion needed.
=====================
*/
int Nav_TravelTimeEstimate( const float *start, const float *goal ) {
	NavData_t *data = Nav_CurrentData();
	vec3_t navStart, navGoal;
	SwapYZ( start, navStart );
	SwapYZ( goal, navGoal );

	float straight[32 * 3];
	unsigned char straightFlags[32];
	dtPolyRef straightRefs[32];
	int straightCount = 0;
	if ( !data || !Nav_ComputeStraightPath( data, navStart, navGoal, straight, straightFlags, straightRefs, 32, &straightCount ) ) {
		return -1;
	}

	float dist = 0.0f;
	for ( int i = 1; i < straightCount; i++ ) {
		vec3_t seg;
		VectorSubtract( &straight[i * 3], &straight[( i - 1 ) * 3], seg );
		dist += VectorLength( seg );
	}
	return (int)dist;
}

/*
====================
Nav_SetLineClearFn

The host supplies world line of sight.
====================
*/
void Nav_SetLineClearFn( navLineClearFn_t fn ) {
	navLineClear = fn;
}

/*
====================
Nav_SetGameVisibleFn

The host supplies the game visibility test.
====================
*/
void Nav_SetGameVisibleFn( navGameVisibleFn_t fn ) {
	navGameVisible = fn;
}

/*
===================
Nav_BodyVisible

True if eyeQ can see any part of a standing body whose origin is at originQ.
===================
*/
static bool Nav_BodyVisible( NavData_t *data, const float *eyeQ, const float *originQ, float bodyHeight, float peek, bool whenOutOfBudget ) {
	if ( !navLineClear ) {
		vec3_t navEye, navTarget;
		SwapYZ( eyeQ, navEye );
		SwapYZ( originQ, navTarget );
		navTarget[1] -= NAV_ORIGIN_ABOVE_FLOOR;
		dtPolyRef eyeRef, targetRef;
		float eyeNearest[3], targetNearest[3];
		if ( !Nav_FindNearest( data, navEye, &eyeRef, eyeNearest ) || !Nav_FindNearest( data, navTarget, &targetRef, targetNearest ) ) {
			return true; // can not tell, so do not claim it is hidden
		}
		return Nav_RaycastClear( data, eyeRef, eyeNearest, targetNearest );
	}

	const float zOfs[3] = { bodyHeight * 0.5f - NAV_ORIGIN_ABOVE_FLOOR,
							bodyHeight - 4.0f - NAV_ORIGIN_ABOVE_FLOOR,
							8.0f - NAV_ORIGIN_ABOVE_FLOOR };

	float shift[3][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f } };
	int numEyes = 1;
	if ( peek > 0.0f ) {
		float dx = originQ[0] - eyeQ[0];
		float dy = originQ[1] - eyeQ[1];
		float len = sqrtf( dx * dx + dy * dy );
		if ( len > 1.0f ) {
			shift[1][0] = -dy / len * peek;
			shift[1][1] = dx / len * peek;
			shift[2][0] = -shift[1][0];
			shift[2][1] = -shift[1][1];
			numEyes = 3;
		}
	}

	for ( int e = 0; e < numEyes; e++ ) {
		float eye[3] = { eyeQ[0] + shift[e][0], eyeQ[1] + shift[e][1], eyeQ[2] };
		for ( int z = 0; z < 3; z++ ) {
			float p[3] = { originQ[0], originQ[1], originQ[2] + zOfs[z] };
			if ( navTraceBudget-- <= 0 ) {
				return whenOutOfBudget;
			}
			if ( navLineClear( eye, p, NULL, NULL, NAV_MASK_SIGHT ) ) {
				return true;
			}
		}
	}
	return false;
}

/*
===================
Nav_CanAttackFrom

True if an AI at spotQ could shoot a target at targetQ, mirroring AICast_CheckAttack_real.
===================
*/
static bool Nav_CanAttackFrom( NavData_t *data, const float *spotQ, const float *targetQ ) {
	const float eye[3] = { spotQ[0], spotQ[1], spotQ[2] + NAV_EYE_ABOVE_ORIGIN };
	if ( !navLineClear ) {
		return Nav_BodyVisible( data, eye, targetQ, NAV_TARGET_BODY_HEIGHT, 0.0f, false );
	}

	static const float shotMins[3] = { -6.0f, -6.0f, -6.0f };
	static const float shotMaxs[3] = { 6.0f, 6.0f, 6.0f };
	const float halfWidth = 18.0f * 0.9f;
	const float halfHeight = NAV_TARGET_BODY_HEIGHT * 0.5f * 0.9f;

	float dir[3] = { targetQ[0] - eye[0], targetQ[1] - eye[1], targetQ[2] + NAV_EYE_ABOVE_ORIGIN - eye[2] };
	const float dirLen = sqrtf( dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2] );
	if ( dirLen < 1.0f ) {
		return true;
	}
	dir[0] /= dirLen;
	dir[1] /= dirLen;
	dir[2] /= dirLen;
	float right[3] = { dir[1], -dir[0], 0.0f };
	const float rightLen = sqrtf( right[0] * right[0] + right[1] * right[1] );
	if ( rightLen < 0.001f ) {
		right[0] = 0.0f;
		right[1] = -1.0f;
	} else {
		right[0] /= rightLen;
		right[1] /= rightLen;
	}

	const float muzzle[3] = { eye[0] + right[0] * 6.0f, eye[1] + right[1] * 6.0f, eye[2] - 4.0f };
	float aim[3] = { targetQ[0] - muzzle[0], targetQ[1] - muzzle[1], targetQ[2] - muzzle[2] };
	const float aimLen = sqrtf( aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2] );

	for ( int i = 0; i <= 6; i++ ) {
		float end[3] = { muzzle[0] + dir[0] * aimLen, muzzle[1] + dir[1] * aimLen, muzzle[2] + dir[2] * aimLen };
		if ( i > 0 ) {
			const float side = (float)( ( i % 2 ) * 2 - 1 ) * halfWidth;
			end[0] += right[0] * side;
			end[1] += right[1] * side;
			end[2] = targetQ[2] - NAV_ORIGIN_ABOVE_FLOOR + NAV_TARGET_BODY_HEIGHT * 0.5f + halfHeight * (float)( ( ( i - 1 ) - ( ( i - 1 ) % 2 ) ) / 2 - 1 );
		}
		if ( navTraceBudget-- <= 0 ) {
			return false;
		}
		if ( navLineClear( muzzle, end, shotMins, shotMaxs, NAV_MASK_SHOT ) ) {
			return true;
		}
	}
	return false;
}

static bool Nav_PolyCentroidOnFloor( NavData_t *data, dtPolyRef ref, float *out );

struct NavThreatCloud_t {
	int num;
	float eye[NAV_HIDE_CLOUD_MAX][3];
};

static void Nav_AddCloudEye( NavThreatCloud_t *cloud, const float *floorPt ) {
	if ( cloud->num < NAV_HIDE_CLOUD_MAX ) {
		float *e = cloud->eye[cloud->num++];
		e[0] = floorPt[0];
		e[1] = floorPt[2];
		e[2] = floorPt[1] + NAV_ORIGIN_ABOVE_FLOOR + NAV_EYE_ABOVE_ORIGIN;
	}
}

/*
===================
Nav_BuildThreatCloud

Eye positions the threat is likely to look from by the time we are in cover.
===================
*/
static void Nav_BuildThreatCloud( NavData_t *data, const float *navThreat, const float *navFrom, NavThreatCloud_t *cloud ) {
	static const float ringDir[8][2] = { { 1.0f, 0.0f }, { 0.7071f, 0.7071f }, { 0.0f, 1.0f }, { -0.7071f, 0.7071f },
										 { -1.0f, 0.0f }, { -0.7071f, -0.7071f }, { 0.0f, -1.0f }, { 0.7071f, -0.7071f } };
	dtQueryFilter filter;
	dtPolyRef threatRef;
	float threatNearest[3];

	cloud->num = 0;
	if ( !Nav_FindNearest( data, navThreat, &threatRef, threatNearest ) ) {
		return;
	}

	float straight[NAV_HIDE_CLOUD_PATH_CORNERS * 3];
	unsigned char straightFlags[NAV_HIDE_CLOUD_PATH_CORNERS];
	dtPolyRef straightRefs[NAV_HIDE_CLOUD_PATH_CORNERS];
	int corners = 0;
	if ( Nav_ComputeStraightPath( data, navThreat, navFrom, straight, straightFlags, straightRefs, NAV_HIDE_CLOUD_PATH_CORNERS, &corners ) ) {
		float walked = 0.0f;
		float next = NAV_HIDE_CLOUD_STEP;
		for ( int i = 1; i < corners && next <= NAV_HIDE_CLOUD_REACH; i++ ) {
			const float *a = &straight[( i - 1 ) * 3];
			const float *b = &straight[i * 3];
			const float len = dtVdist( a, b );
			for ( ; len > 1.0f && next <= walked + len && next <= NAV_HIDE_CLOUD_REACH; next += NAV_HIDE_CLOUD_STEP ) {
				float p[3];
				dtVlerp( p, a, b, ( next - walked ) / len );
				Nav_AddCloudEye( cloud, p );
			}
			walked += len;
		}
	}

	dtPolyRef refs[NAV_MAX_SPOT_POLYS];
	dtPolyRef parents[NAV_MAX_SPOT_POLYS];
	float costs[NAV_MAX_SPOT_POLYS];
	int count = 0;
	if ( dtStatusFailed( data->query->findPolysAroundCircle( threatRef, threatNearest, NAV_HIDE_CLOUD_RADIUS, &filter,
															  refs, parents, costs, &count, NAV_MAX_SPOT_POLYS ) ) ) {
		return;
	}
	float centroids[NAV_MAX_SPOT_POLYS][3];
	for ( int i = 0; i < count; i++ ) {
		if ( !Nav_PolyCentroidOnFloor( data, refs[i], centroids[i] ) ) {
			centroids[i][0] = centroids[i][1] = centroids[i][2] = 1.0e9f;
		}
	}

	for ( int s = 0; s < NAV_HIDE_CLOUD_SECTORS; s++ ) {
		const float r = ( s & 1 ) ? NAV_HIDE_CLOUD_FAR : NAV_HIDE_CLOUD_NEAR;
		const float want[3] = { threatNearest[0] + ringDir[s][0] * r, threatNearest[1], threatNearest[2] + ringDir[s][1] * r };
		int best = -1;
		float bestDist = NAV_HIDE_CLOUD_SNAP * NAV_HIDE_CLOUD_SNAP;
		for ( int i = 0; i < count; i++ ) {
			const float dx = centroids[i][0] - want[0];
			const float dz = centroids[i][2] - want[2];
			const float d = dx * dx + dz * dz;
			if ( d < bestDist && fabsf( centroids[i][1] - want[1] ) < navGenClasses[navCurrentClass].height ) {
				bestDist = d;
				best = i;
			}
		}
		float p[3];
		bool overPoly;
		if ( best >= 0 && dtStatusSucceed( data->query->closestPointOnPoly( refs[best], want, p, &overPoly ) ) ) {
			Nav_AddCloudEye( cloud, p );
		}
	}
}

/*
===================
Nav_SpotHidden

True if spotQ is out of sight of the threat and of the threat cloud.
===================
*/
static bool Nav_SpotHidden( NavData_t *data, const float *threatQ, const float *spotQ, float bodyHeight, const NavThreatCloud_t *cloud ) {
	float eye[3] = { threatQ[0], threatQ[1], threatQ[2] + NAV_EYE_ABOVE_ORIGIN };
	if ( Nav_BodyVisible( data, eye, spotQ, bodyHeight, NAV_PEEK_OFFSET, true ) ) {
		return false;
	}
	if ( !navLineClear ) {
		return true; // the raycast fallback has no way to ask about other positions
	}
	int seen = 0;
	for ( int i = 0; i < cloud->num; i++ ) {
		if ( Nav_BodyVisible( data, cloud->eye[i], spotQ, bodyHeight, 0.0f, true ) && ++seen > NAV_HIDE_CLOUD_MAX_SEEN ) {
			return false;
		}
	}
	return true;
}

struct NavSpotCandidate_t {
	float score;
	float pt[3];
};

static int Nav_CompareCandidates( const void *a, const void *b ) {
	float sa = ( (const NavSpotCandidate_t *)a )->score;
	float sb = ( (const NavSpotCandidate_t *)b )->score;
	return ( sa > sb ) - ( sa < sb );
}

/*
===================
Nav_PolyCentroidOnFloor

Centroid of a ground poly, snapped to the poly's surface.
===================
*/
static bool Nav_PolyCentroidOnFloor( NavData_t *data, dtPolyRef ref, float *out ) {
	const dtMeshTile *tile;
	const dtPoly *poly;
	if ( dtStatusFailed( data->mesh->getTileAndPolyByRef( ref, &tile, &poly ) ) || poly->getType() != DT_POLYTYPE_GROUND || poly->vertCount == 0 ) {
		return false;
	}
	float c[3] = { 0.0f, 0.0f, 0.0f };
	for ( int v = 0; v < poly->vertCount; v++ ) {
		const float *vert = &tile->verts[poly->verts[v] * 3];
		c[0] += vert[0];
		c[1] += vert[1];
		c[2] += vert[2];
	}
	const float inv = 1.0f / (float)poly->vertCount;
	c[0] *= inv;
	c[1] *= inv;
	c[2] *= inv;
	bool overPoly;
	return dtStatusSucceed( data->query->closestPointOnPoly( ref, c, out, &overPoly ) );
}

/*
==================
Nav_FindHidePosition

Nearest spot the threat cannot see, now or from where it is likely to be; the game has the last word.
==================
*/
int Nav_FindHidePosition( const float *from, const float *threat, float radius, int selfNum, int enemyNum, float *outPos ) {
	NavData_t *data = Nav_CurrentData();
	if ( !data ) {
		return 0;
	}
	vec3_t navFrom, navThreat;
	SwapYZ( from, navFrom );
	SwapYZ( threat, navThreat );

	// a point underground is never visible; if the game says it is, no spot can pass its check
	const bool askGame = navGameVisible && selfNum >= 0 && enemyNum >= 0;
	if ( askGame ) {
		const float underground[3] = { threat[0], threat[1], threat[2] - 4000.0f };
		if ( navGameVisible( threat, enemyNum, underground, selfNum ) ) {
			return 0;
		}
	}

	dtQueryFilter filter;
	dtPolyRef startRef;
	float startNearest[3];
	if ( !Nav_FindNearest( data, navFrom, &startRef, startNearest ) ) {
		return 0;
	}

	dtPolyRef resultRefs[NAV_MAX_SPOT_POLYS];
	dtPolyRef resultParents[NAV_MAX_SPOT_POLYS];
	float resultCosts[NAV_MAX_SPOT_POLYS];
	int resultCount = 0;
	if ( dtStatusFailed( data->query->findPolysAroundCircle( startRef, startNearest, radius, &filter,
															  resultRefs, resultParents, resultCosts, &resultCount, NAV_MAX_SPOT_POLYS ) ) ) {
		return 0;
	}

	float dirX = navThreat[0] - navFrom[0];
	float dirZ = navThreat[2] - navFrom[2];
	const float threatDist = sqrtf( dirX * dirX + dirZ * dirZ );
	if ( threatDist > 1.0f ) {
		dirX /= threatDist;
		dirZ /= threatDist;
	}

	NavSpotCandidate_t cands[NAV_MAX_SPOT_POLYS * 2];
	int numCands = 0;

	for ( int i = 0; i < resultCount; i++ ) {
		float entry[3], centroid[3];
		bool overPoly;
		if ( dtStatusFailed( data->query->closestPointOnPoly( resultRefs[i], startNearest, entry, &overPoly ) ) ) {
			continue;
		}
		if ( !Nav_PolyCentroidOnFloor( data, resultRefs[i], centroid ) ) {
			continue;
		}

		const float *samples[2] = { entry, centroid };
		for ( int s = 0; s < 2; s++ ) {
			const float *pt = samples[s];

			float offX = pt[0] - navFrom[0];
			float offZ = pt[2] - navFrom[2];
			if ( offX * offX + offZ * offZ < NAV_HIDE_MIN_MOVE * NAV_HIDE_MIN_MOVE ) {
				continue;
			}
			if ( threatDist > 32.0f && offX * dirX + offZ * dirZ > threatDist * 0.5f ) {
				continue;
			}
			float tx = pt[0] - navThreat[0];
			float tz = pt[2] - navThreat[2];
			float threatToSpot = sqrtf( tx * tx + tz * tz );
			if ( threatToSpot < NAV_HIDE_MIN_THREAT_DIST ) {
				continue;
			}

			float ex = pt[0] - entry[0];
			float ez = pt[2] - entry[2];
			float score = resultCosts[i] + sqrtf( ex * ex + ez * ez );
			if ( threatToSpot < threatDist ) {
				score += ( threatDist - threatToSpot ) * 2.0f;
			}

			NavSpotCandidate_t *c = &cands[numCands++];
			c->score = score;
			VectorCopy( pt, c->pt );
		}
	}

	qsort( cands, numCands, sizeof( cands[0] ), Nav_CompareCandidates );

	const float bodyHeight = navGenClasses[navCurrentClass].height;
	navTraceBudget = NAV_MAX_QUERY_TRACES;

	NavThreatCloud_t cloud;
	cloud.num = 0;
	if ( navLineClear && numCands > 0 ) {
		Nav_BuildThreatCloud( data, navThreat, navFrom, &cloud );
	}

	int tested = 0;
	int found = 0;
	for ( ; tested < numCands && tested < NAV_MAX_SPOT_TESTS; tested++ ) {
		vec3_t spotQ;
		SwapYZ( cands[tested].pt, spotQ );
		spotQ[2] += NAV_ORIGIN_ABOVE_FLOOR;
		if ( Nav_SpotHidden( data, threat, spotQ, bodyHeight, &cloud ) &&
			 !( askGame && navGameVisible( threat, enemyNum, spotQ, selfNum ) ) ) {
			VectorCopy( spotQ, outPos );
			found = 1;
			break;
		}
	}

	return found;
}

/*
=================
Nav_FindAttackSpot

Nearest spot within maxRange of from that an AI could shoot the target from, like AAS.
=================
*/
int Nav_FindAttackSpot( const float *from, const float *target, float minRange, float maxRange, float *outPos ) {
	NavData_t *data = Nav_CurrentData();
	if ( !data ) {
		return 0;
	}
	vec3_t navFrom;
	SwapYZ( from, navFrom );

	dtQueryFilter filter;
	dtPolyRef startRef;
	float startNearest[3];
	if ( !Nav_FindNearest( data, navFrom, &startRef, startNearest ) ) {
		return 0;
	}

	dtPolyRef resultRefs[NAV_MAX_SPOT_POLYS];
	dtPolyRef resultParents[NAV_MAX_SPOT_POLYS];
	float resultCosts[NAV_MAX_SPOT_POLYS];
	int resultCount = 0;
	if ( dtStatusFailed( data->query->findPolysAroundCircle( startRef, startNearest, maxRange, &filter,
															  resultRefs, resultParents, resultCosts, &resultCount, NAV_MAX_SPOT_POLYS ) ) ) {
		return 0;
	}

	const float minTargetDist = minRange > NAV_ATTACK_MIN_TARGET_DIST ? minRange : NAV_ATTACK_MIN_TARGET_DIST;

	NavSpotCandidate_t cands[NAV_MAX_SPOT_POLYS * 2];
	int numCands = 0;

	for ( int i = 0; i < resultCount; i++ ) {
		float entry[3], centroid[3];
		bool overPoly;
		if ( dtStatusFailed( data->query->closestPointOnPoly( resultRefs[i], startNearest, entry, &overPoly ) ) ) {
			continue;
		}
		if ( !Nav_PolyCentroidOnFloor( data, resultRefs[i], centroid ) ) {
			continue;
		}

		const float *samples[2] = { entry, centroid };
		for ( int s = 0; s < 2; s++ ) {
			const float *pt = samples[s];
			vec3_t spotQ;
			SwapYZ( pt, spotQ );
			spotQ[2] += NAV_ORIGIN_ABOVE_FLOOR;

			vec3_t toFrom, toTarget;
			VectorSubtract( spotQ, from, toFrom );
			VectorSubtract( spotQ, target, toTarget );
			if ( VectorLength( toFrom ) > maxRange || VectorLength( toTarget ) < minTargetDist ) {
				continue;
			}

			float ex = pt[0] - entry[0];
			float ez = pt[2] - entry[2];

			NavSpotCandidate_t *c = &cands[numCands++];
			c->score = resultCosts[i] + sqrtf( ex * ex + ez * ez );
			VectorCopy( pt, c->pt );
		}
	}

	qsort( cands, numCands, sizeof( cands[0] ), Nav_CompareCandidates );

	navTraceBudget = NAV_MAX_QUERY_TRACES;
	int tested = 0;
	int found = 0;
	for ( ; tested < numCands && tested < NAV_MAX_SPOT_TESTS; tested++ ) {
		vec3_t spotQ;
		SwapYZ( cands[tested].pt, spotQ );
		spotQ[2] += NAV_ORIGIN_ABOVE_FLOOR;
		if ( Nav_CanAttackFrom( data, spotQ, target ) ) {
			VectorCopy( spotQ, outPos );
			found = 1;
			break;
		}
	}

	return found;
}

/*
=======================
Nav_GetRouteFirstVisPos
=======================
*/
int Nav_GetRouteFirstVisPos( const float *srcpos, const float *destpos, float *outPos ) {
	NavData_t *data = Nav_CurrentData();
	if ( !data ) {
		return 0;
	}
	vec3_t navSrc, navDest;
	SwapYZ( srcpos, navSrc );
	SwapYZ( destpos, navDest );

	dtPolyRef srcRef, destRef;
	float srcNearest[3], destNearest[3];
	if ( !Nav_FindNearest( data, navSrc, &srcRef, srcNearest ) ) {
		return 0;
	}
	if ( !Nav_FindNearest( data, navDest, &destRef, destNearest ) ) {
		return 0;
	}

	// already visible directly from dest? just use src as-is.
	if ( Nav_RaycastClear( data, destRef, destNearest, srcNearest ) ) {
		SwapYZ( srcpos, outPos );
		return 1;
	}

	float straight[32 * 3];
	unsigned char straightFlags[32];
	dtPolyRef straightRefs[32];
	int straightCount = 0;
	if ( !Nav_ComputeStraightPath( data, navSrc, navDest, straight, straightFlags, straightRefs, 32, &straightCount ) ) {
		return 0;
	}

	// first corridor point (walking from src) that dest can see wins.
	for ( int i = 0; i < straightCount; i++ ) {
		if ( Nav_RaycastClear( data, destRef, destNearest, &straight[i * 3] ) ) {
			SwapYZ( &straight[i * 3], outPos );
			return 1;
		}
	}
	return 0;
}

/*
============
Nav_TestPath
============
*/
void Nav_TestPath( const float *start, const float *end ) {
	if ( !Nav_CurrentData() ) {
		Com_Printf( "Nav_TestPath: no navmesh loaded for class %d\n", navCurrentClass );
		return;
	}

	navMoveResult_t result;
	int ok = Nav_MoveToGoal( &result, start, end, -1 );
	int dist = Nav_TravelTimeEstimate( start, end );

	if ( !ok ) {
		Com_Printf( "Nav_TestPath: FAILED start (%.0f %.0f %.0f) -> end (%.0f %.0f %.0f)\n",
					start[0], start[1], start[2], end[0], end[1], end[2] );
		return;
	}

	Com_Printf( "Nav_TestPath: OK dist=%d movedir=(%.2f %.2f %.2f)\n",
				dist, result.movedir[0], result.movedir[1], result.movedir[2] );
}

/*
================
Nav_RemoveAgent

Removes agentId's crowd agent from whichever class it was tracked under (only NAV_MAX_CLASSES
of them, cheap to check both). Must be called when an AI dies/disconnects - without it, a long
wave-based level cycling many spawns through a few client slots would leak crowd agents until
NAV_CROWD_MAX_AGENTS is exhausted.
================
*/
void Nav_RemoveAgent( int agentId ) {
	if ( agentId < 0 || agentId >= NAV_MAX_TRACKED_AGENTS ) {
		return;
	}
	for ( int i = 0; i < NAV_MAX_CLASSES; i++ ) {
		NavData_t *data = &navData[i];
		if ( !data->loaded || !data->crowd ) {
			continue;
		}
		int idx = data->crowdAgentIdx[agentId];
		if ( idx >= 0 ) {
			data->crowd->removeAgent( idx );
			data->crowdAgentIdx[agentId] = -1;
		}
	}
}

/*
============
Nav_DumpMesh

Writes classIndex's baked navmesh polygons (not navgen's raw input geometry)
to an .obj in quake space, for visual inspection in any 3D viewer.
============
*/
void Nav_DumpMesh( int classIndex ) {
	if ( classIndex < 0 || classIndex >= NAV_MAX_CLASSES ) {
		Com_Printf( "Nav_DumpMesh: invalid class %d\n", classIndex );
		return;
	}

	NavData_t *data = &navData[classIndex];
	if ( !data->loaded || !data->mesh ) {
		Com_Printf( "Nav_DumpMesh: no navmesh loaded for class %d\n", classIndex );
		return;
	}

	char filename[256];
	snprintf( filename, sizeof( filename ), "navdump_%s.obj", navGenClasses[classIndex].name );
	FILE *f = fopen( filename, "w" );
	if ( !f ) {
		Com_Printf( "Nav_DumpMesh: could not open %s for writing\n", filename );
		return;
	}

	const dtNavMesh *mesh = data->mesh;
	int vertBase = 1; // OBJ indices are 1-based
	int totalPolys = 0, totalVerts = 0;

	for ( int i = 0; i < mesh->getMaxTiles(); i++ ) {
		const dtMeshTile *tile = mesh->getTile( i );
		if ( !tile || !tile->header ) {
			continue;
		}

		for ( int j = 0; j < tile->header->vertCount; j++ ) {
			const float *v = &tile->verts[j * 3];
			vec3_t quakeV;
			SwapYZ( v, quakeV ); // navmesh is Y-up; write back out in quake's Z-up space
			fprintf( f, "v %f %f %f\n", quakeV[0], quakeV[1], quakeV[2] );
		}

		for ( int j = 0; j < tile->header->polyCount; j++ ) {
			const dtPoly *poly = &tile->polys[j];
			if ( poly->getType() != DT_POLYTYPE_GROUND ) {
				continue;
			}
			fprintf( f, "f" );
			for ( int k = 0; k < poly->vertCount; k++ ) {
				fprintf( f, " %d", vertBase + poly->verts[k] );
			}
			fprintf( f, "\n" );
			totalPolys++;
		}

		vertBase += tile->header->vertCount;
		totalVerts += tile->header->vertCount;
	}

	fclose( f );
	Com_Printf( "Nav_DumpMesh: wrote %s (%d verts, %d polys)\n", filename, totalVerts, totalPolys );
}
