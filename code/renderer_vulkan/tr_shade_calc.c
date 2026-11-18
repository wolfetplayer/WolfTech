// tr_shade_calc.c -- see tr_shade_calc.h. Wave tables and formulas match code/renderer/tr_init.c and vkQuake3's tr_shade_calc.c.

#include <math.h>

#include "tr_local.h"
#include "tr_shade_calc.h"

#define FUNCTABLE_SIZE 1024
#define FUNCTABLE_MASK ( FUNCTABLE_SIZE - 1 )

static float s_sinTable[FUNCTABLE_SIZE];
static float s_squareTable[FUNCTABLE_SIZE];
static float s_triangleTable[FUNCTABLE_SIZE];
static float s_sawToothTable[FUNCTABLE_SIZE];
static float s_inverseSawToothTable[FUNCTABLE_SIZE];
static qboolean s_tablesBuilt = qfalse;

static float s_shaderTime; // seconds

static void BuildTables( void ) {
	int i;

	if ( s_tablesBuilt ) {
		return;
	}

	for ( i = 0; i < FUNCTABLE_SIZE; i++ ) {
		s_sinTable[i] = sinf( DEG2RAD( i * 360.0f / (float)( FUNCTABLE_SIZE - 1 ) ) );
		s_squareTable[i] = ( i < FUNCTABLE_SIZE / 2 ) ? 1.0f : -1.0f;
		s_sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
		s_inverseSawToothTable[i] = 1.0f - s_sawToothTable[i];

		if ( i < FUNCTABLE_SIZE / 2 ) {
			s_triangleTable[i] = ( i < FUNCTABLE_SIZE / 4 ) ?
				(float)i / ( FUNCTABLE_SIZE / 4 ) : 1.0f - s_triangleTable[i - FUNCTABLE_SIZE / 4];
		} else {
			s_triangleTable[i] = -s_triangleTable[i - FUNCTABLE_SIZE / 2];
		}
	}

	s_tablesBuilt = qtrue;
}

void R_SetShaderTime( int timeMs ) {
	BuildTables();
	s_shaderTime = timeMs * 0.001f;
}

static const float *TableForFunc( genFunc_t func ) {
	switch ( func ) {
	case GF_SIN: return s_sinTable;
	case GF_TRIANGLE: return s_triangleTable;
	case GF_SQUARE: return s_squareTable;
	case GF_SAWTOOTH: return s_sawToothTable;
	case GF_INVERSE_SAWTOOTH: return s_inverseSawToothTable;
	case GF_NOISE: return s_sinTable; // no Perlin noise subsystem here -- sine is a documented stand-in
	default: return s_sinTable;
	}
}

static float EvalWaveForm( const waveForm_t *wf ) {
	const float *table = TableForFunc( wf->func );
	float phase = wf->phase + s_shaderTime * wf->frequency;
	return wf->base + table[ (int)( phase * FUNCTABLE_SIZE ) & FUNCTABLE_MASK ] * wf->amplitude;
}

static float EvalWaveFormClamped( const waveForm_t *wf ) {
	float v = EvalWaveForm( wf );
	return ( v < 0.0f ) ? 0.0f : ( v > 1.0f ) ? 1.0f : v;
}

static byte ByteFromUnit( float v ) {
	if ( v < 0.0f ) v = 0.0f;
	else if ( v > 1.0f ) v = 1.0f;
	return (byte)( v * 255.0f );
}

void R_CalcStageColorAlpha( const shaderStage_t *stage, const byte *entityColor, const byte *lightingTint,
							 const byte baseColor[4], byte outColor[4] ) {
	byte rgb[3];
	byte alpha;

	switch ( stage->rgbGen ) {
	default:
	case CGEN_IDENTITY_LIGHTING: // tr.identityLight -- a global brightness scalar (always 1.0 here), not per-entity lighting
	case CGEN_IDENTITY:
	case CGEN_FOG: // fallback -- no fog-volume integration here
		rgb[0] = rgb[1] = rgb[2] = 255;
		break;
	case CGEN_LIGHTING_DIFFUSE:
		// Real version needs a per-vertex normal; lightingTint stands in.
		if ( lightingTint ) { rgb[0] = lightingTint[0]; rgb[1] = lightingTint[1]; rgb[2] = lightingTint[2]; }
		else { rgb[0] = rgb[1] = rgb[2] = 255; }
		break;
	case CGEN_ENTITY:
		if ( entityColor ) { rgb[0] = entityColor[0]; rgb[1] = entityColor[1]; rgb[2] = entityColor[2]; }
		else { rgb[0] = rgb[1] = rgb[2] = 255; }
		break;
	case CGEN_ONE_MINUS_ENTITY:
		if ( entityColor ) { rgb[0] = 255 - entityColor[0]; rgb[1] = 255 - entityColor[1]; rgb[2] = 255 - entityColor[2]; }
		else { rgb[0] = rgb[1] = rgb[2] = 255; }
		break;
	case CGEN_EXACT_VERTEX:
	case CGEN_VERTEX: // tr.identityLight is always 1.0 here (no overbright bits), so this matches EXACT_VERTEX
		rgb[0] = baseColor[0]; rgb[1] = baseColor[1]; rgb[2] = baseColor[2];
		break;
	case CGEN_ONE_MINUS_VERTEX:
		rgb[0] = 255 - baseColor[0]; rgb[1] = 255 - baseColor[1]; rgb[2] = 255 - baseColor[2];
		break;
	case CGEN_WAVEFORM: {
		byte v = ByteFromUnit( EvalWaveForm( &stage->rgbWave ) ); // tr.identityLight == 1.0
		rgb[0] = rgb[1] = rgb[2] = v;
		break;
	}
	case CGEN_CONST:
		rgb[0] = stage->constantColor[0]; rgb[1] = stage->constantColor[1]; rgb[2] = stage->constantColor[2];
		break;
	}

	switch ( stage->alphaGen ) {
	default:
	case AGEN_IDENTITY:
		alpha = 255;
		break;
	case AGEN_SKIP:
		alpha = baseColor[3];
		break;
	case AGEN_ENTITY:
		alpha = entityColor ? entityColor[3] : 255;
		break;
	case AGEN_ONE_MINUS_ENTITY:
		alpha = entityColor ? (byte)( 255 - entityColor[3] ) : 255;
		break;
	case AGEN_VERTEX:
		alpha = baseColor[3];
		break;
	case AGEN_ONE_MINUS_VERTEX:
		alpha = (byte)( 255 - baseColor[3] );
		break;
	case AGEN_WAVEFORM:
		alpha = ByteFromUnit( EvalWaveFormClamped( &stage->alphaWave ) );
		break;
	case AGEN_LIGHTING_SPECULAR: // fallback -- needs a normal + view vector
	case AGEN_PORTAL:            // fallback -- no portal/mirror views here
		alpha = 255;
		break;
	case AGEN_CONST:
		alpha = stage->constantColor[3];
		break;
	}

	outColor[0] = rgb[0]; outColor[1] = rgb[1]; outColor[2] = rgb[2]; outColor[3] = alpha;
}

void R_CalcStageTexCoord( const shaderStage_t *stage, float baseS, float baseT, const float worldPos[3], float outST[2] ) {
	float s = baseS, t = baseT;
	int m;

	for ( m = 0; m < stage->numTexMods; m++ ) {
		const texModInfo_t *tmi = &stage->texMods[m];
		float s0, t0;

		switch ( tmi->type ) {
		default:
		case TMOD_NONE:
		case TMOD_ENTITY_TRANSLATE: // fallback -- needs the entity's own shader-time offset, not threaded in yet
			break;

		case TMOD_TRANSFORM:
			s0 = s; t0 = t;
			s = s0 * tmi->matrix[0][0] + t0 * tmi->matrix[1][0] + tmi->translate[0];
			t = s0 * tmi->matrix[0][1] + t0 * tmi->matrix[1][1] + tmi->translate[1];
			break;

		case TMOD_TURBULENT: {
			float now = tmi->wave.phase + s_shaderTime * tmi->wave.frequency;
			s += s_sinTable[ (int)( ( ( worldPos[0] + worldPos[2] ) * ( 1.0f / 128.0f ) * 0.125f + now ) * FUNCTABLE_SIZE ) & FUNCTABLE_MASK ] * tmi->wave.amplitude;
			t += s_sinTable[ (int)( ( worldPos[1] * ( 1.0f / 128.0f ) * 0.125f + now ) * FUNCTABLE_SIZE ) & FUNCTABLE_MASK ] * tmi->wave.amplitude;
			break;
		}

		case TMOD_SCROLL: {
			float ss = tmi->scroll[0] * s_shaderTime;
			float tt = tmi->scroll[1] * s_shaderTime;
			ss -= floorf( ss );
			tt -= floorf( tt );
			s += ss; t += tt;
			break;
		}

		case TMOD_SCALE:
			s *= tmi->scale[0]; t *= tmi->scale[1];
			break;

		case TMOD_STRETCH: {
			float p = 1.0f / EvalWaveForm( &tmi->wave );
			s0 = s; t0 = t;
			s = s0 * p + ( 0.5f - 0.5f * p );
			t = t0 * p + ( 0.5f - 0.5f * p );
			break;
		}

		case TMOD_ROTATE: {
			float degs = -tmi->rotateSpeed * s_shaderTime;
			int index = (int)( degs * ( FUNCTABLE_SIZE / 360.0f ) );
			float sinValue = s_sinTable[ index & FUNCTABLE_MASK ];
			float cosValue = s_sinTable[ ( index + FUNCTABLE_SIZE / 4 ) & FUNCTABLE_MASK ];
			s0 = s; t0 = t;
			s = s0 * cosValue + t0 * ( -sinValue ) + ( 0.5f - 0.5f * cosValue + 0.5f * sinValue );
			t = s0 * sinValue + t0 * cosValue + ( 0.5f - 0.5f * sinValue - 0.5f * cosValue );
			break;
		}
		}
	}

	outST[0] = s; outST[1] = t;
}

void R_DeformPosition( const shader_t *shader, const float basePos[3], float outPos[3] ) {
	int i;

	outPos[0] = basePos[0]; outPos[1] = basePos[1]; outPos[2] = basePos[2];

	for ( i = 0; i < shader->numDeforms; i++ ) {
		const deformStage_t *ds = &shader->deforms[i];

		if ( ds->deformation == DEFORM_MOVE ) {
			float scale = EvalWaveForm( &ds->deformationWave );
			outPos[0] += ds->moveVector[0] * scale;
			outPos[1] += ds->moveVector[1] * scale;
			outPos[2] += ds->moveVector[2] * scale;
		}
		// every other deform type passes the position through unchanged, see tr_shade_calc.h
	}
}
