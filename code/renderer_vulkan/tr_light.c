// tr_light.c -- light grid sampling, see tr_light.h.

#include <string.h>
#include <math.h>

#include "tr_local.h"
#include "tr_bsp.h"
#include "tr_light.h"

// Grid sample's packed direction (data[6]=lng, data[7]=lat, each 0-255) back to a unit vector.
static void LatLongToNormal( int lat, int lng, vec3_t normal ) {
	float latRad = (float)lat * ( 2.0f * (float)M_PI / 256.0f );
	float lngRad = (float)lng * ( 2.0f * (float)M_PI / 256.0f );

	normal[0] = cosf( latRad ) * sinf( lngRad );
	normal[1] = sinf( latRad ) * sinf( lngRad );
	normal[2] = cosf( lngRad );
}

int R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir ) {
	vec3_t relOrigin;
	int pos[3];
	float frac[3];
	int gridStep[3];
	const byte *gridData;
	vec3_t direction;
	float totalFactor;
	int i, j;

	VectorClear( ambientLight );
	VectorClear( directedLight );
	VectorSet( lightDir, 0.0f, 0.0f, 1.0f );

	if ( !tr_world.loaded || !tr_world.lightGridData ) {
		return qfalse;
	}

	VectorSubtract( point, tr_world.lightGridOrigin, relOrigin );
	for ( i = 0; i < 3; i++ ) {
		float v = relOrigin[i] * tr_world.lightGridInverseSize[i];
		pos[i] = (int)floorf( v );
		frac[i] = v - (float)pos[i];
		if ( pos[i] < 0 ) {
			pos[i] = 0;
		} else if ( pos[i] > tr_world.lightGridBounds[i] - 1 ) {
			pos[i] = tr_world.lightGridBounds[i] - 1;
		}
	}

	gridStep[0] = 8;
	gridStep[1] = 8 * tr_world.lightGridBounds[0];
	gridStep[2] = 8 * tr_world.lightGridBounds[0] * tr_world.lightGridBounds[1];
	gridData = tr_world.lightGridData + pos[0] * gridStep[0] + pos[1] * gridStep[1] + pos[2] * gridStep[2];

	VectorClear( direction );
	totalFactor = 0.0f;

	// Trilerp the 8 neighboring grid points, skipping any that fall outside the grid or inside solid geometry.
	for ( i = 0; i < 8; i++ ) {
		float factor = 1.0f;
		const byte *data = gridData;
		vec3_t normal;

		for ( j = 0; j < 3; j++ ) {
			if ( i & ( 1 << j ) ) {
				if ( pos[j] + 1 > tr_world.lightGridBounds[j] - 1 ) {
					break;
				}
				factor *= frac[j];
				data += gridStep[j];
			} else {
				factor *= ( 1.0f - frac[j] );
			}
		}
		if ( j != 3 ) {
			continue;
		}
		if ( !( data[0] + data[1] + data[2] ) ) {
			continue;
		}

		totalFactor += factor;
		ambientLight[0] += factor * (float)data[0];
		ambientLight[1] += factor * (float)data[1];
		ambientLight[2] += factor * (float)data[2];
		directedLight[0] += factor * (float)data[3];
		directedLight[1] += factor * (float)data[4];
		directedLight[2] += factor * (float)data[5];

		LatLongToNormal( data[7], data[6], normal );
		VectorMA( direction, factor, normal, direction );
	}

	if ( totalFactor > 0.0f && totalFactor < 0.99f ) {
		float inv = 1.0f / totalFactor;
		VectorScale( ambientLight, inv, ambientLight );
		VectorScale( directedLight, inv, directedLight );
	}

	VectorNormalize2( direction, lightDir );

	return qtrue;
}
