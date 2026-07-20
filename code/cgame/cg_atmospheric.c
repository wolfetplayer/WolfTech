#include "cg_local.h"
/*
**  	All rights reserved.
**
**  	cg_atmospheric.c
**
**  	Add atmospheric effects to view.
**
**  	Current supported effects are rain, snow and sandstorm.
**
**		Berserker: with my optimizations ;)
*/


#define MAX_ATMOSPHERIC_PARTICLES  	  	8192	/// was 1000  	// maximum # of particles

#define MAX_RAIN_DISTANCE		 	  	3000  	// maximum distance from refdef origin that RAIN particles are generated
#define MAX_RAIN_DISTANCE_2  			(MAX_RAIN_DISTANCE * MAX_RAIN_DISTANCE)  	/// for fast calculations
#define RAIN_CUTHEIGHT  		  	  	500
#define MAX_SPLASH_DISTANCE_2			(750 * 750)

#define MAX_SNOW_DISTANCE  	  			5000	// maximum distance from refdef origin that SNOW particles are generated
#define MAX_SNOW_DISTANCE_2  			(MAX_SNOW_DISTANCE * MAX_SNOW_DISTANCE)  	/// for fast calculations
#define SNOW_CUTHEIGHT  		  	  	800

#define MAX_SANDSTORM_DISTANCE  	  	2500	// maximum distance from refdef origin that SANDSTORM particles are generated
#define MAX_SANDSTORM_DISTANCE_2  		(MAX_SANDSTORM_DISTANCE * MAX_SANDSTORM_DISTANCE)  	/// for fast calculations
#define SANDSTORM_CUTHEIGHT  	  	  	220		/// sand hugs the ground, keeps it reading as horizontal not falling
#define SANDSTORM_TURBULENCE  	  	  	380		/// per-particle random horizontal kick, makes it swirl instead of moving in lockstep
#define SANDSTORM_TURBULENCE_Z  	  	70		/// vertical chaos - still lower than horizontal so it doesn't read as falling
#define SANDSTORM_WOBBLE_AMPLITUDE  	26		/// primary lateral wobble added at render time for a non-linear streak path
#define SANDSTORM_WOBBLE_FREQ  	  	  	0.05f
#define SANDSTORM_WOBBLE_AMPLITUDE2  	16		/// secondary wobble layer at a different freq/phase so the path never repeats cleanly
#define SANDSTORM_WOBBLE_FREQ2  	  	0.14f

#define MAX_ATMOSPHERIC_HEIGHT  	  	65536  	/// was 4096	// maximum world height (FIXME: since 1.27 this should be 65536)
#define MIN_ATMOSPHERIC_HEIGHT  	  	-65536  /// was -4096	// minimum world height (FIXME: since 1.27 this should be -65536)
#define ATMOSPHERIC_DROPDELAY  	  	  	1000

#define ATMOSPHERIC_RAIN_SPEED  	  	1.1f * DEFAULT_GRAVITY
#define ATMOSPHERIC_RAIN_HEIGHT  	  	96	/// was 150			размер капли дождя

#define ATMOSPHERIC_SNOW_SPEED  	  	0.1f * DEFAULT_GRAVITY
#define ATMOSPHERIC_SNOW_HEIGHT  	  	6	/// размер снежинки

#define ATMOSPHERIC_SANDSTORM_SPEED  	0.01f * DEFAULT_GRAVITY	/// almost no vertical fall, sand rides the wind horizontally
#define ATMOSPHERIC_SANDSTORM_HEIGHT  	14	/// short - the whole texture image gets stretched across this length vs. particle->weight width, so keep it close to the width or a round texture turns into a line

#define FLAG_atmosphericParticle_ACTIVE	1
#define FLAG_atmosphericParticle_SPLASH	2

typedef struct cg_atmosphericParticle_s {
  	vec3_t pos, delta, deltaNormalized, colour, surfacenormal;
  	float height, minz, weight;
	unsigned	flags;
  	int contents, surface, nextDropTime;
} cg_atmosphericParticle_t;

typedef struct cg_atmosphericEffect_s {
  	cg_atmosphericParticle_t particles[MAX_ATMOSPHERIC_PARTICLES];
  	qhandle_t effectshader;
  	qhandle_t effectsplashshader;
  	int lastRainTime, numDrops;
  	int gustStartTime, gustEndTime;
  	int baseStartTime, baseEndTime;
  	int gustMinTime, gustMaxTime;
  	int changeMinTime, changeMaxTime;
  	int baseMinTime, baseMaxTime;
  	float baseWeight, gustWeight;
  	int baseDrops, gustDrops;
  	qboolean splash;
  	vec3_t baseVec, gustVec;

	polyVert_t  verts[4];

  	qboolean (*ParticleCheckVisible)( cg_atmosphericParticle_t *particle );
  	qboolean (*ParticleGenerate)( cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight );
  	void (*ParticleRender)( cg_atmosphericParticle_t *particle );
} cg_atmosphericEffect_t;

static cg_atmosphericEffect_t cg_atmFx;

/*
**  Render utility functions
*/
// 'quick' version of the CG_ImpactMark function
void CG_EffectMark(qhandle_t markShader, const vec3_t origin, const vec3_t dir, float alpha, float radius)
{
	float  	  	  	texCoordScale, tx, ty;
	vec3_t  	  	axis[3], delta[4];
	byte  	  	  	colors[4];
	int  	  	  	i;
	polyVert_t  	*v, verts[4];

	if (radius <= 0)
		return;		///	CG_Error( "CG_EffectMark called with <= 0 radius" );

	// create the texture axis
	VectorNormalize2(dir, axis[0]);
	PerpendicularVector(axis[1], axis[0]);
#if 0
	VectorSet(axis[2], 1, 0, 0);  	  	  	// This is _wrong_, but the function is for water anyway (i.e. usually flat)
#else
	RotatePointAroundVector(axis[2], axis[0], axis[1], 0/*orientation*/);
#endif
	CrossProduct(axis[0], axis[2], axis[1]);

	texCoordScale = 0.5 / radius;

	// create the full polygon
	for (i = 0; i < 3; i++)
	{
		tx = radius * axis[1][i];
		ty = radius * axis[2][i];

		delta[0][i] = -tx - ty;
		delta[1][i] =  tx - ty;
		delta[2][i] =  tx + ty;
		delta[3][i] = -tx + ty;

		v = verts;
		v->xyz[i] = origin[i] + delta[0][i];
		v++;
		v->xyz[i] = origin[i] + delta[1][i];
		v++;
		v->xyz[i] = origin[i] + delta[2][i];
		v++;
		v->xyz[i] = origin[i] + delta[3][i];
	}

	colors[0] = colors[1] = colors[2] = 127;
	colors[3] = alpha * 255;

	for (i = 0, v = verts; i < 4; i++, v++)
	{
		v->xyz[2] += 1;	/// bias
		v->st[0] = 0.5 + DotProduct(delta[i], axis[1]) * texCoordScale;
		v->st[1] = 0.5 + DotProduct(delta[i], axis[2]) * texCoordScale;
		*(int *)v->modulate = *(int *)colors;
	}

	trap_R_AddPolyToScene(markShader, 4, (void *)verts);
}

/*
**  	Raindrop management functions
*/
static qboolean CG_RainParticleCheckVisible( cg_atmosphericParticle_t *particle )
{
  	// Check the raindrop is visible and still going, wrapping if necessary.
  	float moved, d2;
  	vec3_t distance;

	if (!particle || !(particle->flags & FLAG_atmosphericParticle_ACTIVE))
  	  	return( qfalse );

  	moved = (cg.time - cg_atmFx.lastRainTime) * 0.001;  	// Units moved since last frame
  	VectorMA( particle->pos, moved, particle->delta, particle->pos );
	if (particle->pos[2] + RAIN_CUTHEIGHT < particle->minz)
	{
		particle->flags &= ~FLAG_atmosphericParticle_ACTIVE;
		return qfalse;
	}

  	VectorSubtract( cg.refdef.vieworg, particle->pos, distance );
	d2 = distance[0] * distance[0] + distance[1] * distance[1];
#if 0
	if( sqrt( distance[0] * distance[0] + distance[1] * distance[1] ) > MAX_RAIN_DISTANCE )
#else
	//d2 = qmin(FogDensity * FogDensity, d2);	/// если есть туман, то ограничим дальность осадков границей тумана
	if (d2 > MAX_RAIN_DISTANCE_2)
#endif
	{
		particle->flags &= ~FLAG_atmosphericParticle_ACTIVE;
		return qfalse;
	}

	if (cg_atmFx.effectsplashshader)
	{
		if (d2 < MAX_SPLASH_DISTANCE_2)
			particle->flags |= FLAG_atmosphericParticle_SPLASH;
		else
			particle->flags &= ~FLAG_atmosphericParticle_SPLASH;
	}
	else
		particle->flags &= ~FLAG_atmosphericParticle_SPLASH;	/// нет материала, нет брызг

	return(qtrue);
}

static qboolean CG_SnowParticleCheckVisible(cg_atmosphericParticle_t *particle)
{
	// Check the raindrop is visible and still going, wrapping if necessary.
	float moved, d2;
	vec3_t distance;

	if (!particle || !(particle->flags & FLAG_atmosphericParticle_ACTIVE))
		return(qfalse);

	moved = (cg.time - cg_atmFx.lastRainTime) * 0.001;  	// Units moved since last frame
	VectorMA(particle->pos, moved, particle->delta, particle->pos);
	if (particle->pos[2] + SNOW_CUTHEIGHT < particle->minz)
	{
		particle->flags &= ~FLAG_atmosphericParticle_ACTIVE;
		return qfalse;
	}

	VectorSubtract(cg.refdef.vieworg, particle->pos, distance);
	d2 = distance[0] * distance[0] + distance[1] * distance[1];
#if 0
	if (sqrt(distance[0] * distance[0] + distance[1] * distance[1]) > MAX_SNOW_DISTANCE)
#else
	//d2 = qmin(FogDensity * FogDensity, d2);	/// если есть туман, то ограничим дальность осадков границей тумана
	if (d2 > MAX_SNOW_DISTANCE_2)
#endif
	{
		particle->flags &= ~FLAG_atmosphericParticle_ACTIVE;
		return qfalse;
	}

	particle->flags &= ~FLAG_atmosphericParticle_SPLASH;	/// Berserker: у снега нет брызг ))

	return(qtrue);
}

static qboolean CG_RainParticleGenerate(cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight)
{
  	// Attempt to 'spot' a raindrop somewhere below a sky texture.
  	float angle, distance, d;
  	vec3_t testpoint, testend;
  	trace_t tr;

  	angle = random() * 2*M_PI;

	d = MAX_RAIN_DISTANCE;		/// если есть туман, то ограничим дальность осадков границей тумана
	distance = 20 + d * random();

  	testpoint[0] = testend[0] = cg.refdef.vieworg[0] + sin(angle) * distance;
  	testpoint[1] = testend[1] = cg.refdef.vieworg[1] + cos(angle) * distance;
  	testpoint[2] = cg.refdef.vieworg[2];
  	testend[2] = testpoint[2] + MAX_ATMOSPHERIC_HEIGHT;

  	while( 1 )
  	{
  	  	if( testpoint[2] >= MAX_ATMOSPHERIC_HEIGHT )
  	  	  	return( qfalse );
  	  	if( testend[2] >= MAX_ATMOSPHERIC_HEIGHT )
  	  	  	testend[2] = MAX_ATMOSPHERIC_HEIGHT - 1;
  	  	CG_Trace( &tr, testpoint, NULL, NULL, testend, ENTITYNUM_NONE, MASK_SOLID|MASK_WATER );
  	  	if( tr.startsolid )  	  	  	// Stuck in something, skip over it.
  	  	{
  	  	  	testpoint[2] += 64;
  	  	  	testend[2] = testpoint[2] + MAX_ATMOSPHERIC_HEIGHT;
  	  	}
  	  	else if( tr.fraction == 1 )  	  	// Didn't hit anything, we're (probably) outside the world
  	  	  	return( qfalse );
		else if ((tr.surfaceFlags & SURF_SKY) && tr.entityNum == ENTITYNUM_WORLD)  	// Hit sky, this is where we start.	BERSERKER: added check for ENTITYNUM_WORLD
  	  	  	break;
  	  	else return( qfalse );
  	}

	particle->flags = FLAG_atmosphericParticle_ACTIVE;
  	particle->colour[0] = 0.6 + 0.2 * random();
  	particle->colour[1] = 0.6 + 0.2 * random();
  	particle->colour[2] = 0.6 + 0.2 * random();

/// Berserker: choose a spawn point randomly between sky and viewpoint (but not more than RAIN_CUTHEIGHT)
tr.endpos[2] = testpoint[2] + random() * (min(tr.endpos[2] - testpoint[2], RAIN_CUTHEIGHT));
	VectorCopy(tr.endpos, particle->pos);

  	VectorCopy( currvec, particle->delta );
  	particle->delta[2] += crandom() * 100;
  	VectorNormalize2( particle->delta, particle->deltaNormalized );
  	particle->height = ATMOSPHERIC_RAIN_HEIGHT + crandom() * 32;	/// was 100
  	particle->weight = currweight;

  	distance = ((float)(tr.endpos[2] - MIN_ATMOSPHERIC_HEIGHT)) / -particle->delta[2];
  	VectorMA( tr.endpos, distance, particle->delta, testend );

  	CG_Trace( &tr, particle->pos, NULL, NULL, testend, ENTITYNUM_NONE, MASK_SOLID|MASK_WATER );
  	particle->minz = tr.endpos[2];
  	tr.endpos[2]--;
  	VectorCopy( tr.plane.normal, particle->surfacenormal );
  	particle->surface = tr.surfaceFlags;
  	particle->contents = CG_PointContents( tr.endpos, ENTITYNUM_NONE );

  	return( qtrue );
}

static void CG_RainParticleRender( cg_atmosphericParticle_t *particle )
{
  	// Draw a raindrop
	vec3_t  	  	forward, right;
  	vec2_t  	  	line;
	float  	  		len, frac;
  	vec3_t  	  	start, finish;

	if (!(particle->flags & FLAG_atmosphericParticle_ACTIVE))
  	  	return;

	/// Berserker: если проблема с регистрацией материала, не будем спамить
	if (cg_atmFx.effectshader < 0)
		return;

  	VectorCopy( particle->pos, start );
  	len = particle->height;
  	if( start[2] <= particle->minz )
  	{
  	  	// Stop rain going through surfaces.
  	  	len = particle->height - particle->minz + start[2];
  	  	frac = start[2];
  	  	VectorMA( start, len - particle->height, particle->deltaNormalized, start );
start[2] = particle->minz;	/// FIXED: не допускать ухода брызг под пол, случается при смене направления падения капель.

		if (!cg_atm_effects_low.integer && (particle->flags & FLAG_atmosphericParticle_SPLASH))
  	  	{
			/// На плоскостях сильно наклоненных (больше 45 градусов) не будет брызг
			if (particle->surfacenormal[2] > 0.7)
			{
				frac = (RAIN_CUTHEIGHT - particle->minz + frac) / (float)RAIN_CUTHEIGHT;
				// Splash effects on different surfaces
				if (particle->contents & (CONTENTS_WATER | CONTENTS_SLIME))
				{
					// Water splash
					if (frac > 0 && frac <= 1)
						CG_EffectMark(cg_atmFx.effectsplashshader, start, particle->surfacenormal, frac * 0.5, 9 - frac * 8);
				}
				else if (!(particle->contents & CONTENTS_LAVA) && !(particle->surface & (/*SURF_NODAMAGE|SURF_NOIMPACT|SURF_NOMARKS|*/SURF_SKY)))
				{
					// Solid splash
					if (frac > 0 && frac <= 1)
#if 0
						CG_ImpactMark(cg_atmFx.effectsplashshader, start, particle->surfacenormal, 0, 1, 1, 1, frac * 0.5, false, 3 - frac * 2, true, 0, false);		/// that's very SLOWLY!!!
#else
						CG_EffectMark(cg_atmFx.effectsplashshader, start, particle->surfacenormal, frac * 0.5, 5 - frac * 4);
#endif
				}
			}
		}
	}
  	if( len <= 0 )
  	  	return;

  	VectorCopy( particle->deltaNormalized, forward );
  	VectorMA( start, -len, forward, finish );

  	line[0] = DotProduct( forward, cg.refdef.viewaxis[1] );
  	line[1] = DotProduct( forward, cg.refdef.viewaxis[2] );

  	VectorScale( cg.refdef.viewaxis[1], line[1], right );
  	VectorMA( right, -line[0], cg.refdef.viewaxis[2], right );
  	VectorNormalize( right );

	VectorMA(finish, particle->weight, right, cg_atmFx.verts[0].xyz);
	VectorMA(finish, -particle->weight, right, cg_atmFx.verts[1].xyz);
	VectorMA(start, -particle->weight, right, cg_atmFx.verts[2].xyz);
	VectorMA(start, particle->weight, right, cg_atmFx.verts[3].xyz);

	trap_R_AddPolyToScene(cg_atmFx.effectshader, 4, (void *)cg_atmFx.verts);
}

/*
**  	Snow management functions
*/

static qboolean CG_SnowParticleGenerate( cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight )
{
  	// Attempt to 'spot' a raindrop somewhere below a sky texture.
  	float angle, distance, d;
  	vec3_t testpoint, testend;
  	trace_t tr;

  	angle = random() * 2*M_PI;

	d = MAX_SNOW_DISTANCE;		/// если есть туман, то ограничим дальность осадков границей тумана
	distance = 20 + d * random();

  	testpoint[0] = testend[0] = cg.refdef.vieworg[0] + sin(angle) * distance;
  	testpoint[1] = testend[1] = cg.refdef.vieworg[1] + cos(angle) * distance;
  	testpoint[2] = cg.refdef.vieworg[2];
  	testend[2] = testpoint[2] + MAX_ATMOSPHERIC_HEIGHT;

  	while( 1 )
  	{
  	  	if( testpoint[2] >= MAX_ATMOSPHERIC_HEIGHT )
  	  	  	return( qfalse );
  	  	if( testend[2] >= MAX_ATMOSPHERIC_HEIGHT )
  	  	  	testend[2] = MAX_ATMOSPHERIC_HEIGHT - 1;
  	  	CG_Trace( &tr, testpoint, NULL, NULL, testend, ENTITYNUM_NONE, MASK_SOLID|MASK_WATER );
  	  	if( tr.startsolid )  	  	  	// Stuck in something, skip over it.
  	  	{
  	  	  	testpoint[2] += 64;
  	  	  	testend[2] = testpoint[2] + MAX_ATMOSPHERIC_HEIGHT;
  	  	}
  	  	else if( tr.fraction == 1 )  	  	// Didn't hit anything, we're (probably) outside the world
  	  	  	return( qfalse );
		else if ((tr.surfaceFlags & SURF_SKY) && tr.entityNum == ENTITYNUM_WORLD)  	// Hit sky, this is where we start.	BERSERKER: added check for ENTITYNUM_WORLD
  	  	  	break;
  	  	else return( qfalse );
  	}

	particle->flags = FLAG_atmosphericParticle_ACTIVE;
	particle->colour[0] = 0.6 + 0.2 * random();
  	particle->colour[1] = 0.6 + 0.2 * random();
  	particle->colour[2] = 0.6 + 0.2 * random();

/// Berserker: choose a spawn point randomly between sky and viewpoint (but not more than SNOW_CUTHEIGHT)
tr.endpos[2] = testpoint[2] + random() * (min(tr.endpos[2] - testpoint[2], SNOW_CUTHEIGHT));
	VectorCopy(tr.endpos, particle->pos);

  	VectorCopy( currvec, particle->delta );
  	particle->delta[2] += crandom() * 25;
  	VectorNormalize2( particle->delta, particle->deltaNormalized );
  	particle->height = ATMOSPHERIC_SNOW_HEIGHT + crandom() * 3;
  	particle->weight = particle->height * 0.5f;

  	distance = ((float)(tr.endpos[2] - MIN_ATMOSPHERIC_HEIGHT)) / -particle->delta[2];
  	VectorMA( tr.endpos, distance, particle->delta, testend );
  	CG_Trace( &tr, particle->pos, NULL, NULL, testend, ENTITYNUM_NONE, MASK_SOLID|MASK_WATER );
	particle->minz = tr.endpos[2];
  	VectorCopy( tr.plane.normal, particle->surfacenormal );

///		not used for snow!
///	tr.endpos[2]--;
///	particle->surface = tr.surfaceFlags;
///	particle->contents = CG_PointContents( tr.endpos, ENTITYNUM_NONE );

  	return( qtrue );
}

static void CG_SnowParticleRender( cg_atmosphericParticle_t *particle )
{
  	// Draw a snowflake
  	vec3_t  	  	forward, right;
  	vec2_t  	  	line;
  	float  	  		len, sinTumbling, cosTumbling, particleWidth;
  	vec3_t  	  	start, finish;

	if (!(particle->flags & FLAG_atmosphericParticle_ACTIVE))
  	  	return;

	/// Berserker: если проблема с регистрацией материала, не будем спамить
	if (cg_atmFx.effectshader < 0)
		return;

  	VectorCopy( particle->pos, start );

  	sinTumbling = sin( particle->pos[2] * 0.03125f );
  	cosTumbling = cos( ( particle->pos[2] + particle->pos[1] )  * 0.03125f );

  	start[0] += 24 * ( 1 - particle->deltaNormalized[2] ) * sinTumbling;
  	start[1] += 24 * ( 1 - particle->deltaNormalized[2] ) * cosTumbling;

  	len = particle->height;
  	if( start[2] <= particle->minz )
  	{
  	  	// Stop snow going through surfaces.
  	  	len = particle->height - particle->minz + start[2];
  	  	VectorMA( start, len - particle->height, particle->deltaNormalized, start );
  	}
  	if( len <= 0 )
  	  	return;

  	VectorCopy( particle->deltaNormalized, forward );
  	VectorMA( start, -( len * sinTumbling ), forward, finish );

  	line[0] = DotProduct( forward, cg.refdef.viewaxis[1] );
  	line[1] = DotProduct( forward, cg.refdef.viewaxis[2] );

  	VectorScale( cg.refdef.viewaxis[1], line[1], right );
  	VectorMA( right, -line[0], cg.refdef.viewaxis[2], right );
  	VectorNormalize( right );

  	particleWidth = cosTumbling * particle->weight;

	VectorMA(finish, particleWidth, right, cg_atmFx.verts[0].xyz);
	VectorMA(finish, -particleWidth, right, cg_atmFx.verts[1].xyz);
	VectorMA(start, -particleWidth, right, cg_atmFx.verts[2].xyz);
	VectorMA(start, particleWidth, right, cg_atmFx.verts[3].xyz);

	trap_R_AddPolyToScene(cg_atmFx.effectshader, 4, (void *)cg_atmFx.verts);
}

/*
**  	Sandstorm management functions
*/

static qboolean CG_SandstormParticleCheckVisible( cg_atmosphericParticle_t *particle )
{
  	// Check the sand particle is visible and still going, wrapping if necessary.
  	float moved, d2;
  	vec3_t distance;

	if (!particle || !(particle->flags & FLAG_atmosphericParticle_ACTIVE))
  	  	return( qfalse );

  	moved = (cg.time - cg_atmFx.lastRainTime) * 0.001;  	// Units moved since last frame
  	VectorMA( particle->pos, moved, particle->delta, particle->pos );
	if (particle->pos[2] + SANDSTORM_CUTHEIGHT < particle->minz)
	{
		particle->flags &= ~FLAG_atmosphericParticle_ACTIVE;
		return qfalse;
	}

  	VectorSubtract( cg.refdef.vieworg, particle->pos, distance );
	d2 = distance[0] * distance[0] + distance[1] * distance[1];
	if (d2 > MAX_SANDSTORM_DISTANCE_2)
	{
		particle->flags &= ~FLAG_atmosphericParticle_ACTIVE;
		return qfalse;
	}

	particle->flags &= ~FLAG_atmosphericParticle_SPLASH;	/// песок не образует брызг

	return(qtrue);
}

static qboolean CG_SandstormParticleGenerate( cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight )
{
  	// Attempt to 'spot' a sand particle somewhere below a sky texture, staying close to the ground.
  	float angle, distance, d;
  	vec3_t testpoint, testend;
  	trace_t tr;

  	angle = random() * 2*M_PI;

	d = MAX_SANDSTORM_DISTANCE;
	distance = 20 + d * random();

  	testpoint[0] = testend[0] = cg.refdef.vieworg[0] + sin(angle) * distance;
  	testpoint[1] = testend[1] = cg.refdef.vieworg[1] + cos(angle) * distance;
  	testpoint[2] = cg.refdef.vieworg[2];
  	testend[2] = testpoint[2] + MAX_ATMOSPHERIC_HEIGHT;

  	while( 1 )
  	{
  	  	if( testpoint[2] >= MAX_ATMOSPHERIC_HEIGHT )
  	  	  	return( qfalse );
  	  	if( testend[2] >= MAX_ATMOSPHERIC_HEIGHT )
  	  	  	testend[2] = MAX_ATMOSPHERIC_HEIGHT - 1;
  	  	CG_Trace( &tr, testpoint, NULL, NULL, testend, ENTITYNUM_NONE, MASK_SOLID|MASK_WATER );
  	  	if( tr.startsolid )  	  	  	// Stuck in something, skip over it.
  	  	{
  	  	  	testpoint[2] += 64;
  	  	  	testend[2] = testpoint[2] + MAX_ATMOSPHERIC_HEIGHT;
  	  	}
  	  	else if( tr.fraction == 1 )  	  	// Didn't hit anything, we're (probably) outside the world
  	  	  	return( qfalse );
		else if ((tr.surfaceFlags & SURF_SKY) && tr.entityNum == ENTITYNUM_WORLD)  	// Hit sky, this is where we start.
  	  	  	break;
  	  	else return( qfalse );
  	}

	particle->flags = FLAG_atmosphericParticle_ACTIVE;
  	particle->colour[0] = 0.7 + 0.2 * random();
  	particle->colour[1] = 0.6 + 0.2 * random();
  	particle->colour[2] = 0.4 + 0.15 * random();

/// keep sand close to the ground, doesn't need to fall from the full sky height like rain/snow
tr.endpos[2] = testpoint[2] + random() * (min(tr.endpos[2] - testpoint[2], SANDSTORM_CUTHEIGHT));
	VectorCopy(tr.endpos, particle->pos);

  	VectorCopy( currvec, particle->delta );
	/// strong per-particle turbulence so particles don't all move in lockstep with the wind - this is
	/// what makes it read as a chaotic swirling sandstorm instead of diagonal rain
  	particle->delta[0] += crandom() * SANDSTORM_TURBULENCE;
  	particle->delta[1] += crandom() * SANDSTORM_TURBULENCE;
  	particle->delta[2] += crandom() * SANDSTORM_TURBULENCE_Z;
  	VectorNormalize2( particle->delta, particle->deltaNormalized );
  	particle->height = ATMOSPHERIC_SANDSTORM_HEIGHT + crandom() * 6;
	/// randomize grain size too - a mix of fine dust and coarser grit reads as more chaotic than uniform sized particles
  	particle->weight = currweight * (0.5f + random());
	/// stash a random phase in the (otherwise unused for sand) surface field so each particle's
	/// render-time wobble is out of sync with the others instead of all swirling in unison
	particle->surface = rand() % 6283;

  	distance = ((float)(tr.endpos[2] - MIN_ATMOSPHERIC_HEIGHT)) / -particle->delta[2];
  	VectorMA( tr.endpos, distance, particle->delta, testend );

  	CG_Trace( &tr, particle->pos, NULL, NULL, testend, ENTITYNUM_NONE, MASK_SOLID|MASK_WATER );
  	particle->minz = tr.endpos[2];
  	VectorCopy( tr.plane.normal, particle->surfacenormal );

  	return( qtrue );
}

static void CG_SandstormParticleRender( cg_atmosphericParticle_t *particle )
{
  	// Draw a sand streak
	vec3_t  	  	forward, right;
  	vec2_t  	  	line;
	float  	  		len, wobble;
  	vec3_t  	  	start, finish;

	if (!(particle->flags & FLAG_atmosphericParticle_ACTIVE))
  	  	return;

	if (cg_atmFx.effectshader < 0)
		return;

  	VectorCopy( particle->pos, start );
  	len = particle->height;
  	if( start[2] <= particle->minz )
  	{
  	  	// Stop sand going through surfaces.
  	  	len = particle->height - particle->minz + start[2];
  	  	VectorMA( start, len - particle->height, particle->deltaNormalized, start );
		start[2] = particle->minz;
	}
  	if( len <= 0 )
  	  	return;

  	VectorCopy( particle->deltaNormalized, forward );
  	VectorMA( start, -len, forward, finish );

  	line[0] = DotProduct( forward, cg.refdef.viewaxis[1] );
  	line[1] = DotProduct( forward, cg.refdef.viewaxis[2] );

  	VectorScale( cg.refdef.viewaxis[1], line[1], right );
  	VectorMA( right, -line[0], cg.refdef.viewaxis[2], right );
  	VectorNormalize( right );

	/// two desynced wobble layers (different freq/rate, offset by each particle's random phase) so the
	/// streak path never settles into a clean repeating pattern - reads as turbulent, not just windy
	wobble  = SANDSTORM_WOBBLE_AMPLITUDE  * sin( (start[0] + start[1]) * SANDSTORM_WOBBLE_FREQ  + cg.time * 0.010f + particle->surface );
	wobble += SANDSTORM_WOBBLE_AMPLITUDE2 * sin( (start[0] - start[1]) * SANDSTORM_WOBBLE_FREQ2 + cg.time * 0.017f - particle->surface * 0.5f );
	VectorMA( start, wobble, right, start );
	VectorMA( finish, wobble, right, finish );

	VectorMA(finish, particle->weight, right, cg_atmFx.verts[0].xyz);
	VectorMA(finish, -particle->weight, right, cg_atmFx.verts[1].xyz);
	VectorMA(start, -particle->weight, right, cg_atmFx.verts[2].xyz);
	VectorMA(start, particle->weight, right, cg_atmFx.verts[3].xyz);

	trap_R_AddPolyToScene(cg_atmFx.effectshader, 4, (void *)cg_atmFx.verts);
}

/*
**  	Set up gust parameters.
*/

static void CG_EffectGust()
{
	// Generate random values for the next gust (BERSERKER: fixed division by zero for cg_atmFx.baseEndTime)

	int diff;

	diff = cg_atmFx.baseMaxTime - cg_atmFx.baseMinTime;
	cg_atmFx.baseEndTime = cg.time + cg_atmFx.baseMinTime + (diff ? (rand() % diff) : 0);
	diff = cg_atmFx.changeMaxTime - cg_atmFx.changeMinTime;
	cg_atmFx.gustStartTime = cg_atmFx.baseEndTime + cg_atmFx.changeMinTime + (diff ? (rand() % diff) : 0);
	diff = cg_atmFx.gustMaxTime - cg_atmFx.gustMinTime;
	cg_atmFx.gustEndTime = cg_atmFx.gustStartTime + cg_atmFx.gustMinTime + (diff ? (rand() % diff) : 0);
	diff = cg_atmFx.changeMaxTime - cg_atmFx.changeMinTime;
	cg_atmFx.baseStartTime = cg_atmFx.gustEndTime + cg_atmFx.changeMinTime + (diff ? (rand() % diff) : 0);
}

static qboolean CG_EffectGustCurrent( vec3_t curr, float *weight, int *num )
{
  	// Calculate direction for new drops.

  	vec3_t temp;
  	float frac;

  	if( cg.time < cg_atmFx.baseEndTime )
  	{
  	  	VectorCopy( cg_atmFx.baseVec, curr );
  	  	*weight = cg_atmFx.baseWeight;
  	  	*num = cg_atmFx.baseDrops;
  	}
  	else {
  	  	VectorSubtract( cg_atmFx.gustVec, cg_atmFx.baseVec, temp );
  	  	if( cg.time < cg_atmFx.gustStartTime )
  	  	{
  	  	  	frac = ((float)(cg.time - cg_atmFx.baseEndTime))/((float)(cg_atmFx.gustStartTime - cg_atmFx.baseEndTime));
  	  	  	VectorMA( cg_atmFx.baseVec, frac, temp, curr );
  	  	  	*weight = cg_atmFx.baseWeight + (cg_atmFx.gustWeight - cg_atmFx.baseWeight) * frac;
  	  	  	*num = cg_atmFx.baseDrops + ((float)(cg_atmFx.gustDrops - cg_atmFx.baseDrops)) * frac;
  	  	}
  	  	else if( cg.time < cg_atmFx.gustEndTime )
  	  	{
  	  	  	VectorCopy( cg_atmFx.gustVec, curr );
  	  	  	*weight = cg_atmFx.gustWeight;
  	  	  	*num = cg_atmFx.gustDrops;
  	  	}
  	  	else
  	  	{
  	  	  	frac = 1.0 - ((float)(cg.time - cg_atmFx.gustEndTime))/((float)(cg_atmFx.baseStartTime - cg_atmFx.gustEndTime));
  	  	  	VectorMA( cg_atmFx.baseVec, frac, temp, curr );
  	  	  	*weight = cg_atmFx.baseWeight + (cg_atmFx.gustWeight - cg_atmFx.baseWeight) * frac;
  	  	  	*num = cg_atmFx.baseDrops + ((float)(cg_atmFx.gustDrops - cg_atmFx.baseDrops)) * frac;
  	  	  	if( cg.time >= cg_atmFx.baseStartTime )
  	  	  	  	return( qtrue );
  	  	}
  	}
  	return( qfalse );
}

static void CG_EP_ParseFloats( char *floatstr, float *f1, float *f2 )
{
  	// Parse the float or floats

  	char *middleptr;
  	char buff[64];

  	Q_strncpyz( buff, floatstr, sizeof(buff) );
  	for( middleptr = buff; *middleptr && *middleptr != ' '; middleptr++ );
  	if( *middleptr )
  	{
  	  	*middleptr++ = 0;
  	  	*f1 = atof( floatstr );
  	  	*f2 = atof( middleptr );
  	}
  	else {
  	  	*f1 = *f2 = atof( floatstr );
  	}
}
void CG_EffectParse( const char *effectstr )
{
  	// Split the string into it's component parts.

	float bmin, bmax, cmin, cmax, gmin, gmax, bdrop, gdrop, splash;
	int count, type;
  	char *startptr, *eqptr, *endptr;
  	char workbuff[128];

	trap_Cvar_Set("com_atmosphericEffect", effectstr);

	if( CG_AtmosphericKludge() )
  	  	return;

	// Set up some default values
  	cg_atmFx.baseVec[0] = cg_atmFx.baseVec[1] = 0;
  	cg_atmFx.gustVec[0] = cg_atmFx.gustVec[1] = 100;
  	bmin = 5;
  	bmax = 10;
  	cmin = 1;
  	cmax = 1;
  	gmin = 0;
  	gmax = 2;
	bdrop = gdrop = 1000;	/// was 300;
  	cg_atmFx.baseWeight = 0.7f;
  	cg_atmFx.gustWeight = 1.5f;
  	splash = 1;
	type = 0;

	// Parse the parameter string
  	Q_strncpyz( workbuff, effectstr, sizeof(workbuff) );
  	for( startptr = workbuff; *startptr; )
  	{
  	  	for( eqptr = startptr; *eqptr && *eqptr != '=' && *eqptr != ','; eqptr++ );
  	  	if( !*eqptr )
  	  	  	break;  	  	  	// No more string
  	  	if( *eqptr == ',' )
  	  	{
  	  	  	startptr = eqptr + 1;  	// Bad argument, continue
  	  	  	continue;
  	  	}
  	  	*eqptr++ = 0;
  	  	for( endptr = eqptr; *endptr && *endptr != ','; endptr++ );
  	  	if( *endptr )
  	  	  	*endptr++ = 0;

  	  	if( !type )
  	  	{
  	  	  	if( Q_stricmp( startptr, "T" ) ) {
  	  	  	  	cg_atmFx.numDrops = 0;
  	  	  	  	CG_Printf( "Atmospheric effect must start with a type.\n" );
  	  	  	  	return;
  	  	  	}
  	  	  	if( !Q_stricmp( eqptr, "RAIN" ) ) {
				type = 1;	/// "rain";
  	  	  	  	cg_atmFx.ParticleCheckVisible = &CG_RainParticleCheckVisible;
  	  	  	  	cg_atmFx.ParticleGenerate = &CG_RainParticleGenerate;
  	  	  	  	cg_atmFx.ParticleRender = &CG_RainParticleRender;

  	  	  	  	cg_atmFx.baseVec[2] = cg_atmFx.gustVec[2] = - ATMOSPHERIC_RAIN_SPEED;
  	  	  	} else if( !Q_stricmp( eqptr, "SNOW" ) ) {
				type = 2;	/// "snow";
  	  	  	  	cg_atmFx.ParticleCheckVisible = &CG_SnowParticleCheckVisible;
  	  	  	  	cg_atmFx.ParticleGenerate = &CG_SnowParticleGenerate;
  	  	  	  	cg_atmFx.ParticleRender = &CG_SnowParticleRender;

  	  	  	  	cg_atmFx.baseVec[2] = cg_atmFx.gustVec[2] = - ATMOSPHERIC_SNOW_SPEED;
  	  	  	} else if( !Q_stricmp( eqptr, "SANDSTORM" ) ) {
					type = 3;	/// "sandstorm";
  	  	  	  	cg_atmFx.ParticleCheckVisible = &CG_SandstormParticleCheckVisible;
  	  	  	  	cg_atmFx.ParticleGenerate = &CG_SandstormParticleGenerate;
  	  	  	  	cg_atmFx.ParticleRender = &CG_SandstormParticleRender;

  	  	  	  	cg_atmFx.baseVec[2] = cg_atmFx.gustVec[2] = - ATMOSPHERIC_SANDSTORM_SPEED;
  	  	  	} else {
  	  	  	  	cg_atmFx.numDrops = 0;
  	  	  	  	CG_Printf( "Only effect type 'rain', 'snow' and 'sandstorm' are supported.\n" );
  	  	  	  	return;
  	  	  	}
  	  	}
  	  	else {
  	  	  	if( !Q_stricmp( startptr, "B" ) )
  	  	  	  	CG_EP_ParseFloats( eqptr, &bmin, &bmax );
  	  	  	else if( !Q_stricmp( startptr, "C" ) )
  	  	  	  	CG_EP_ParseFloats( eqptr, &cmin, &cmax );
  	  	  	else if( !Q_stricmp( startptr, "G" ) )
  	  	  	  	CG_EP_ParseFloats( eqptr, &gmin, &gmax );
  	  	  	else if( !Q_stricmp( startptr, "BV" ) )
  	  	  	  	CG_EP_ParseFloats( eqptr, &cg_atmFx.baseVec[0], &cg_atmFx.baseVec[1] );
  	  	  	else if( !Q_stricmp( startptr, "GV" ) )
  	  	  	  	CG_EP_ParseFloats( eqptr, &cg_atmFx.gustVec[0], &cg_atmFx.gustVec[1] );
  	  	  	else if( !Q_stricmp( startptr, "W" ) )
  	  	  	  	CG_EP_ParseFloats( eqptr, &cg_atmFx.baseWeight, &cg_atmFx.gustWeight );
  	  	  	else if( !Q_stricmp( startptr, "S" ) )
  	  	  	  	CG_EP_ParseFloats( eqptr, &splash, &splash );	/// Berserker: for compatibility
  	  	  	else if( !Q_stricmp( startptr, "D" ) )
  	  	  	  	CG_EP_ParseFloats( eqptr, &bdrop, &gdrop );
			else CG_Printf("Unknown effect key '%s'.\n", startptr);
		}
  	  	startptr = endptr;
  	}

  	if( !type )
  	{
  	  	// No effects

  	  	cg_atmFx.numDrops = -1;
  	  	return;
  	}
  	  	
  	cg_atmFx.baseMinTime = 1000 * bmin;
  	cg_atmFx.baseMaxTime = 1000 * bmax;
  	cg_atmFx.changeMinTime = 1000 * cmin;
  	cg_atmFx.changeMaxTime = 1000 * cmax;
  	cg_atmFx.gustMinTime = 1000 * gmin;
  	cg_atmFx.gustMaxTime = 1000 * gmax;
  	cg_atmFx.baseDrops = bdrop;
  	cg_atmFx.gustDrops = gdrop;
  	cg_atmFx.splash = splash;

  	cg_atmFx.numDrops = (cg_atmFx.baseDrops > cg_atmFx.gustDrops) ? cg_atmFx.baseDrops : cg_atmFx.gustDrops;
  	if( cg_atmFx.numDrops > MAX_ATMOSPHERIC_PARTICLES )
  	  	cg_atmFx.numDrops = MAX_ATMOSPHERIC_PARTICLES;

  	// Load graphics
	if (type == 1)	///"rain")		PVS-Studio: v547
	{
		if (!(cg_atmFx.effectshader = trap_R_RegisterShader("gfx/atmosphere/raindrop")))
			cg_atmFx.effectshader = -1;
		if (cg_atmFx.splash)
			cg_atmFx.effectsplashshader = trap_R_RegisterShader("gfx/atmosphere/rainsplash");

		cg_atmFx.verts[0].st[0] = 1;
		cg_atmFx.verts[0].st[1] = 0;
		cg_atmFx.verts[0].modulate[0] =
		cg_atmFx.verts[0].modulate[1] =
		cg_atmFx.verts[0].modulate[2] = 255;
		cg_atmFx.verts[0].modulate[3] = 0;

		cg_atmFx.verts[1].st[0] =
		cg_atmFx.verts[1].st[1] = 0;
		cg_atmFx.verts[1].modulate[0] =
		cg_atmFx.verts[1].modulate[1] =
		cg_atmFx.verts[1].modulate[2] = 255;
		cg_atmFx.verts[1].modulate[3] = 0;

		cg_atmFx.verts[2].st[0] = 0;
		cg_atmFx.verts[2].st[1] = 1;
		cg_atmFx.verts[2].modulate[0] =
		cg_atmFx.verts[2].modulate[1] =
		cg_atmFx.verts[2].modulate[2] = 255;
		cg_atmFx.verts[2].modulate[3] = 127;

		cg_atmFx.verts[3].st[0] =
		cg_atmFx.verts[3].st[1] = 1;
		cg_atmFx.verts[3].modulate[0] =
		cg_atmFx.verts[3].modulate[1] =
		cg_atmFx.verts[3].modulate[2] = 255;
		cg_atmFx.verts[3].modulate[3] = 127;
  	}
	else if (type == 2)	///"snow")
	{
  		if( !( cg_atmFx.effectshader = trap_R_RegisterShader( "gfx/atmosphere/snowflake" )) )
  	  	  	cg_atmFx.effectshader = -1;  	// we had some kind of a problem
  	  	cg_atmFx.splash = 0;

		cg_atmFx.verts[0].st[0] = 1;
		cg_atmFx.verts[0].st[1] = 0;
		cg_atmFx.verts[0].modulate[0] =
		cg_atmFx.verts[0].modulate[1] =
		cg_atmFx.verts[0].modulate[2] =
		cg_atmFx.verts[0].modulate[3] = 255;

		cg_atmFx.verts[1].st[0] =
		cg_atmFx.verts[1].st[1] = 0;
		cg_atmFx.verts[1].modulate[0] =
		cg_atmFx.verts[1].modulate[1] =
		cg_atmFx.verts[1].modulate[2] =
		cg_atmFx.verts[1].modulate[3] = 255;

		cg_atmFx.verts[2].st[0] = 0;
		cg_atmFx.verts[2].st[1] = 1;
		cg_atmFx.verts[2].modulate[0] =
		cg_atmFx.verts[2].modulate[1] =
		cg_atmFx.verts[2].modulate[2] =
		cg_atmFx.verts[2].modulate[3] = 255;

		cg_atmFx.verts[3].st[0] =
		cg_atmFx.verts[3].st[1] = 1;
		cg_atmFx.verts[3].modulate[0] =
		cg_atmFx.verts[3].modulate[1] =
		cg_atmFx.verts[3].modulate[2] =
		cg_atmFx.verts[3].modulate[3] = 255;
  	}
	else ///if( type == 3	"sandstorm" )
	{
		if (!(cg_atmFx.effectshader = trap_R_RegisterShader("gfx/atmosphere/sandparticle")))
			cg_atmFx.effectshader = -1;  	// we had some kind of a problem
		cg_atmFx.splash = 0;

		/// tan/sand tint, sand streaks fade out towards the tail like rain
		cg_atmFx.verts[0].st[0] = 1;
		cg_atmFx.verts[0].st[1] = 0;
		cg_atmFx.verts[0].modulate[0] = 194;
		cg_atmFx.verts[0].modulate[1] = 165;
		cg_atmFx.verts[0].modulate[2] = 110;
		cg_atmFx.verts[0].modulate[3] = 0;

		cg_atmFx.verts[1].st[0] =
		cg_atmFx.verts[1].st[1] = 0;
		cg_atmFx.verts[1].modulate[0] = 194;
		cg_atmFx.verts[1].modulate[1] = 165;
		cg_atmFx.verts[1].modulate[2] = 110;
		cg_atmFx.verts[1].modulate[3] = 0;

		cg_atmFx.verts[2].st[0] = 0;
		cg_atmFx.verts[2].st[1] = 1;
		cg_atmFx.verts[2].modulate[0] = 194;
		cg_atmFx.verts[2].modulate[1] = 165;
		cg_atmFx.verts[2].modulate[2] = 110;
		cg_atmFx.verts[2].modulate[3] = 160;

		cg_atmFx.verts[3].st[0] =
		cg_atmFx.verts[3].st[1] = 1;
		cg_atmFx.verts[3].modulate[0] = 194;
		cg_atmFx.verts[3].modulate[1] = 165;
		cg_atmFx.verts[3].modulate[2] = 110;
		cg_atmFx.verts[3].modulate[3] = 160;
	}

  	  	// Initialise atmospheric effect to prevent all particles falling at the start
  	for( count = 0; count < cg_atmFx.numDrops; count++ )
  	  	cg_atmFx.particles[count].nextDropTime = ATMOSPHERIC_DROPDELAY + (rand() % ATMOSPHERIC_DROPDELAY);

  	CG_EffectGust();
}

/*
** Main render loop
*/
void CG_AddAtmosphericEffects()
{
  	// Add atmospheric effects (e.g. rain, snow etc.) to view
  	int curr, max, currnum;
  	cg_atmosphericParticle_t *particle;
  	vec3_t currvec;
  	float currweight;

	if (cg_atmFx.numDrops <= 0)
		return;

	if (cg_atm_effects_low.integer == 2)
		return;

	max = cg_atm_effects_low.integer ? (cg_atmFx.numDrops >> 1) : cg_atmFx.numDrops;
  	if( CG_EffectGustCurrent( currvec, &currweight, &currnum ) )
  	  	CG_EffectGust();  	  	  	// Recalculate gust parameters

  	for( curr = 0; curr < max; curr++ )
  	{
  	  	particle = &cg_atmFx.particles[curr];
  	  	if( !cg_atmFx.ParticleCheckVisible( particle ) )
  	  	{
  	  	  	// Effect has terminated / fallen from screen view
  	  	  	if( !particle->nextDropTime )
  	  	  	{
  	  	  	  	// Stop rain being synchronized 
  	  	  	  	particle->nextDropTime = rand() % ATMOSPHERIC_DROPDELAY;
  	  	  	}
  	  	  	else if( currnum < curr || particle->nextDropTime > cg.time )
  	  	  	  	continue;
  	  	  	if( !cg_atmFx.ParticleGenerate( particle, currvec, currweight ) )
  	  	  	{
  	  	  	  	// Ensure it doesn't attempt to generate every frame, to prevent
  	  	  	  	// 'clumping' when there's only a small sky area available.
  	  	  	  	particle->nextDropTime = cg.time + ATMOSPHERIC_DROPDELAY;
  	  	  	  	continue;
  	  	  	}
  	  	}

  	  	cg_atmFx.ParticleRender( particle );
  	}

	cg_atmFx.lastRainTime = cg.time;
}


/*
**  	G_AtmosphericKludge
*/

static qboolean kludgeChecked, kludgeResult;
qboolean CG_AtmosphericKludge()
{
  	// Activate effects for specified kludge maps that don't
  	// have it specified for them.

  	if( kludgeChecked )
  	  	return( kludgeResult );
  	kludgeChecked = qtrue;
  	kludgeResult = qfalse;

	if ((cg_atm_effects_force.string[0] | 0x20) == 't')
	{
		CG_EffectParse(cg_atm_effects_force.string);
		return(kludgeResult = qtrue);
	}
	else
	{
		if (cg_atm_effects_force.integer == 1)
		{
			CG_EffectParse("T=RAIN,B=5 10,C=0.5,G=0.5 2,BV=50 50,GV=200 200,W=1 2,D=2000,TLP=15,TLI=0.3 2,TLM=0.5");
			return(kludgeResult = qtrue);
		}
		else if (cg_atm_effects_force.integer == 2)
		{
			CG_EffectParse("T=SNOW,B=5 10,C=0.5,G=0.3 2,BV=20 30,GV=25 40,W=3 5,D=2400");
			return(kludgeResult = qtrue);
		}
		else if (cg_atm_effects_force.integer == 3)
		{
			CG_EffectParse("T=SANDSTORM,B=1 2,C=0.2,G=0.3 0.7,BV=260 220,GV=700 650,W=2 5,D=7800");
			return(kludgeResult = qtrue);
		}
	}

	return( kludgeResult = qfalse );
}
