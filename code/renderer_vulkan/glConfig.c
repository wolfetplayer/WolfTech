// glConfig.c -- window-mode/glconfig_t bookkeeping, adapted from vkQuake3 (GPLv2-or-later).

#include "tr_local.h"
#include "glConfig.h"

// outside of this file shouldn't modify glConfig directly
static glconfig_t glConfig;

static cvar_t *r_customwidth;
static cvar_t *r_customheight;
static cvar_t *r_customaspect;

typedef struct vidmode_s {
	const char *description;
	int width, height;
	float pixelAspect; // pixel width / height
} vidmode_t;

static const vidmode_t r_vidModes[] = {
	{ "Mode  0: 320x240",           320,  240,  1 },
	{ "Mode  1: 400x300",           400,  300,  1 },
	{ "Mode  2: 512x384",           512,  384,  1 },
	{ "Mode  3: 640x480 (480p)",    640,  480,  1 },
	{ "Mode  4: 800x600",           800,  600,  1 },
	{ "Mode  5: 1024x768",         1024,  768,  1 },
	{ "Mode  6: 1280x720 (720p)",  1280,  720,  1 },
	{ "Mode  7: 1280x1024",        1280, 1024,  1 },
	{ "Mode  8: 1600x900",         1600,  900,  1 },
	{ "Mode  9: 1920x1080 (1080p)",1920, 1080,  1 },
	{ "Mode 10: 1920x1200",        1920, 1200,  1 },
	{ "Mode 11: 2560x1440",        2560, 1440,  1 },
	{ "Mode 12: 3840x2160 (4K)",   3840, 2160,  1 },
};
static const int s_numVidModes = ARRAY_LEN( r_vidModes );

void R_SetWinMode( int mode, unsigned int width, unsigned int height, unsigned int hz ) {
	if ( mode < -2 || mode >= s_numVidModes ) {
		mode = 3;
	}

	if ( mode == -2 ) {
		// use desktop video resolution
		glConfig.vidWidth = width;
		glConfig.vidHeight = height;
		glConfig.windowAspect = (float)width / (float)height;
		glConfig.displayFrequency = hz;
	} else if ( mode == -1 ) {
		glConfig.vidWidth = r_customwidth->integer;
		glConfig.vidHeight = r_customheight->integer;
		glConfig.windowAspect = r_customaspect->value;
		glConfig.displayFrequency = 60;
	} else {
		glConfig.vidWidth = r_vidModes[mode].width;
		glConfig.vidHeight = r_vidModes[mode].height;
		glConfig.windowAspect = (float)r_vidModes[mode].width / ( r_vidModes[mode].height * r_vidModes[mode].pixelAspect );
		glConfig.displayFrequency = 60;
	}

	ri.Printf( PRINT_ALL, " MODE: %d, %d x %d, refresh rate: %dHz\n",
			   mode, glConfig.vidWidth, glConfig.vidHeight, glConfig.displayFrequency );
}

void R_GetWinResolution( int *w, int *h ) {
	*w = glConfig.vidWidth;
	*h = glConfig.vidHeight;
}

void R_SetWinResolution( int w, int h ) {
	glConfig.vidWidth = w;
	glConfig.vidHeight = h;
	glConfig.windowAspect = (float)w / (float)h;
}

void R_InitDisplayResolution( void ) {
	r_customwidth = ri.Cvar_Get( "r_customwidth", "960", CVAR_ARCHIVE | CVAR_LATCH );
	r_customheight = ri.Cvar_Get( "r_customheight", "540", CVAR_ARCHIVE | CVAR_LATCH );
	r_customaspect = ri.Cvar_Get( "r_customaspect", "1.78", CVAR_ARCHIVE | CVAR_LATCH );
}

void R_glConfigClear( void ) {
	memset( &glConfig, 0, sizeof( glConfig ) );
}

void R_GetGlConfig( glconfig_t *const pOut ) {
	*pOut = glConfig;
}

void R_glConfigInit( void ) {
	// these values force the UI to disable driver selection
	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;

	glConfig.deviceSupportsGamma = qfalse;
	glConfig.textureEnvAddAvailable = qfalse;
	glConfig.textureCompression = TC_NONE;
	glConfig.stereoEnabled = qfalse;
	glConfig.smpActive = qfalse;

	glConfig.colorBits = 32;
	glConfig.depthBits = 24;
	glConfig.stencilBits = 8;
	glConfig.numTextureUnits = 1;
}
