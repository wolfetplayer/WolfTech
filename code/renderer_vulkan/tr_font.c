/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// tr_font.c -- ported from code/renderer/tr_font.c; FreeType glyph-atlas rasterization.

#include <string.h>

#include "tr_local.h"
#include "tr_image.h"
#include "tr_shader.h"

#ifdef BUILD_FREETYPE
#ifdef USE_LOCAL_HEADERS
  #include "../freetype-2.9/include/ft2build.h"
#else
  #include <ft2build.h>
#endif
#include FT_FREETYPE_H
#include FT_ERRORS_H
#include FT_SYSTEM_H
#include FT_IMAGE_H
#include FT_OUTLINE_H

#define _FLOOR( x )  ( ( x ) & - 64 )
#define _CEIL( x )   ( ( ( x ) + 63 ) & - 64 )
#define _TRUNC( x )  ( ( x ) >> 6 )

FT_Library ftLibrary = NULL;

// live TTF/OTF lookup: fonts/<fontName>.<ext>, first match wins
static const char *supportedFontFormats[] = { "ttf", "otf" };
#define NUM_SUPPORTED_FONT_FORMATS ( sizeof( supportedFontFormats ) / sizeof( supportedFontFormats[0] ) )
#endif

#define MAX_FONTS 32
static int registeredFontCount = 0;
static fontInfo_t registeredFont[MAX_FONTS];

// extended fonts get their own smaller cache (fontInfoExtra_t is ~100KB each, rarely used)
#define MAX_EXTENDED_FONTS 24
static int registeredFontExtendedCount = 0;
static fontInfoExtra_t registeredFontExtended[MAX_EXTENDED_FONTS];


#define FONT_SUPERSAMPLE 4.0f

#ifdef BUILD_FREETYPE
void R_GetGlyphInfo( FT_GlyphSlot glyph, int *left, int *right, int *width, int *top, int *bottom, int *height, int *pitch ) {
	*left  = _FLOOR( glyph->metrics.horiBearingX );
	*right = _CEIL( glyph->metrics.horiBearingX + glyph->metrics.width );
	*width = _TRUNC( *right - *left );

	*top    = _CEIL( glyph->metrics.horiBearingY );
	*bottom = _FLOOR( glyph->metrics.horiBearingY - glyph->metrics.height );
	*height = _TRUNC( *top - *bottom );
	*pitch  = ( qtrue ? ( *width + 3 ) & - 4 : ( *width + 7 ) >> 3 );
}


FT_Bitmap *R_RenderGlyph( FT_GlyphSlot glyph, glyphInfo_t* glyphOut ) {
	FT_Bitmap  *bit2;
	int left, right, width, top, bottom, height, pitch, size;

	R_GetGlyphInfo( glyph, &left, &right, &width, &top, &bottom, &height, &pitch );

	if ( glyph->format == ft_glyph_format_outline ) {
		size   = pitch * height;

		bit2 = ri.Z_Malloc( sizeof( FT_Bitmap ) );

		bit2->width      = width;
		bit2->rows       = height;
		bit2->pitch      = pitch;
		bit2->pixel_mode = ft_pixel_mode_grays;
		//bit2->pixel_mode = ft_pixel_mode_mono;
		bit2->buffer     = ri.Z_Malloc( pitch * height );
		bit2->num_grays = 256;

		Com_Memset( bit2->buffer, 0, size );

		FT_Outline_Translate( &glyph->outline, -left, -bottom );

		FT_Outline_Get_Bitmap( ftLibrary, &glyph->outline, bit2 );

		glyphOut->height = height;
		glyphOut->pitch = pitch;
		glyphOut->top = ( glyph->metrics.horiBearingY >> 6 ) + 1;
		glyphOut->bottom = bottom;

		return bit2;
	} else {
		ri.Printf( PRINT_ALL, "Non-outline fonts are not supported\n" );
	}
	return NULL;
}

static glyphInfo_t *RE_ConstructGlyphInfo( unsigned char *imageOut, int *xOut, int *yOut, int *maxHeight, FT_Face face, const unsigned long c, qboolean calcHeight ) {
	int i;
	static glyphInfo_t glyph;
	unsigned char *src, *dst;
	float scaled_width, scaled_height;
	FT_Bitmap *bitmap = NULL;

	Com_Memset( &glyph, 0, sizeof( glyphInfo_t ) );
	// make sure everything is here
	if ( face != NULL ) {
		FT_Load_Glyph( face, FT_Get_Char_Index( face, c ), FT_LOAD_DEFAULT );
		bitmap = R_RenderGlyph( face->glyph, &glyph );
		if ( bitmap ) {
			glyph.xSkip = ( face->glyph->metrics.horiAdvance >> 6 ) + 1;
		} else {
			return &glyph;
		}

		if ( glyph.height > *maxHeight ) {
			*maxHeight = glyph.height;
		}

		if ( calcHeight ) {
			ri.Free( bitmap->buffer );
			ri.Free( bitmap );
			return &glyph;
		}

		scaled_width = glyph.pitch;
		scaled_height = glyph.height;

		// we need to make sure we fit
		if (*xOut + scaled_width + 1 >= 255) {
			*xOut = 0;
			*yOut += *maxHeight + 1;
		}

		if (*yOut + *maxHeight + 1 >= 255) {
			*yOut = -1;
			*xOut = -1;
			ri.Free( bitmap->buffer );
			ri.Free( bitmap );
			return &glyph;
		}

		src = bitmap->buffer;
		dst = imageOut + ( *yOut * 256 ) + *xOut;

		if ( bitmap->pixel_mode == ft_pixel_mode_mono ) {
			for ( i = 0; i < glyph.height; i++ ) {
				int j;
				unsigned char *_src = src;
				unsigned char *_dst = dst;
				unsigned char mask = 0x80;
				unsigned char val = *_src;
				for ( j = 0; j < glyph.pitch; j++ ) {
					if ( mask == 0x80 ) {
						val = *_src++;
					}
					if ( val & mask ) {
						*_dst = 0xff;
					}
					mask >>= 1;

					if ( mask == 0 ) {
						mask = 0x80;
					}
					_dst++;
				}

				src += glyph.pitch;
				dst += 256;
			}
		} else {
			for ( i = 0; i < glyph.height; i++ ) {
				Com_Memcpy( dst, src, glyph.pitch );
				src += glyph.pitch;
				dst += 256;
			}
		}

		// now an 8-bit-per-pixel greyscale bitmap, width wide
		glyph.imageHeight = scaled_height;
		glyph.imageWidth = scaled_width;
		glyph.s = (float)*xOut / 256;
		glyph.t = (float)*yOut / 256;
		glyph.s2 = glyph.s + (float)scaled_width / 256;
		glyph.t2 = glyph.t + (float)scaled_height / 256;

		*xOut += scaled_width + 1;

		ri.Free(bitmap->buffer);
		ri.Free(bitmap);
	}

	return &glyph;
}
#endif

static int fdOffset;
static byte	*fdFile;

int readInt( void ) {
	int i = ( (unsigned int)fdFile[fdOffset] | ( (unsigned int)fdFile[fdOffset + 1] << 8 ) | ( (unsigned int)fdFile[fdOffset + 2] << 16) | ( (unsigned int)fdFile[fdOffset + 3] << 24 ) );
	fdOffset += 4;
	return i;
}

typedef union {
	byte	fred[4];
	float	ffred;
} poor;

float readFloat( void ) {
	poor me;
#if defined Q3_BIG_ENDIAN
	me.fred[0] = fdFile[fdOffset + 3];
	me.fred[1] = fdFile[fdOffset + 2];
	me.fred[2] = fdFile[fdOffset + 1];
	me.fred[3] = fdFile[fdOffset + 0];
#elif defined Q3_LITTLE_ENDIAN
	me.fred[0] = fdFile[fdOffset + 0];
	me.fred[1] = fdFile[fdOffset + 1];
	me.fred[2] = fdFile[fdOffset + 2];
	me.fred[3] = fdFile[fdOffset + 3];
#endif
	fdOffset += 4;
	return me.ffred;
}

// Legacy pre-baked font (fontImage_<pointSize>.dat/.tga), fallback for assets with no .ttf/.otf
static qboolean R_LoadPreRenderedFont( const char *datName, fontInfo_t *font ) {
	void *faceData;
	int i, len;

	len = ri.FS_ReadFile( datName, NULL );
	if ( len != sizeof( fontInfo_t ) ) {
		return qfalse;
	}

	ri.FS_ReadFile( datName, &faceData );
	fdOffset = 0;
	fdFile = faceData;
	for ( i = 0; i < GLYPHS_PER_FONT; i++ ) {
		font->glyphs[i].height      = readInt();
		font->glyphs[i].top         = readInt();
		font->glyphs[i].bottom      = readInt();
		font->glyphs[i].pitch       = readInt();
		font->glyphs[i].xSkip       = readInt();
		font->glyphs[i].imageWidth  = readInt();
		font->glyphs[i].imageHeight = readInt();
		font->glyphs[i].s           = readFloat();
		font->glyphs[i].t           = readFloat();
		font->glyphs[i].s2          = readFloat();
		font->glyphs[i].t2          = readFloat();
		font->glyphs[i].glyph       = readInt();
		Q_strncpyz(font->glyphs[i].shaderName, (const char *)&fdFile[fdOffset], sizeof(font->glyphs[i].shaderName));
		fdOffset += sizeof(font->glyphs[i].shaderName);
	}
	font->glyphScale = readFloat();
	Com_Memcpy( font->name, &fdFile[fdOffset], MAX_QPATH );

	Q_strncpyz( font->name, datName, sizeof( font->name ) );
	for ( i = GLYPH_START; i <= GLYPH_END; i++ ) {
		font->glyphs[i].glyph = RE_RegisterShaderInternal( font->glyphs[i].shaderName, qtrue );
	}
	ri.FS_FreeFile( faceData );
	return qtrue;
}

#ifdef BUILD_FREETYPE
// Live-rasterizes fonts/<fontName>.ttf or .otf via FreeType into a runtime glyph atlas.
static qboolean R_LoadScalableFont( const char *fontName, int pointSize, fontInfo_t *font ) {
	FT_Face face;
	int j, k, xOut, yOut, lastStart, imageNumber;
	int scaledSize, newSize, maxHeight, left;
	unsigned char *out, *imageBuff;
	glyphInfo_t *glyph;
	image_t *image;
	qhandle_t h;
	float max;
	float dpi = 72.0f * FONT_SUPERSAMPLE;
	float glyphScale;
	void *faceData;
	int i, len, fmt;
	char path[1024];
	char name[1024];
	qboolean found;

	if ( ftLibrary == NULL ) {
		return qfalse;
	}

	found = qfalse;
	for ( fmt = 0; fmt < NUM_SUPPORTED_FONT_FORMATS; fmt++ ) {
		Com_sprintf( path, sizeof( path ), "fonts/%s.%s", fontName, supportedFontFormats[fmt] );
		if ( ri.FS_ReadFile( path, NULL ) > 0 ) {
			found = qtrue;
			break;
		}
	}

	if ( !found ) {
		return qfalse;
	}

	len = ri.FS_ReadFile( path, &faceData );
	if ( len <= 0 ) {
		ri.Printf( PRINT_WARNING, "R_LoadScalableFont: Unable to read font file '%s'\n", path );
		return qfalse;
	}

	// allocate on the stack first in case we fail
	if ( FT_New_Memory_Face( ftLibrary, faceData, len, 0, &face ) ) {
		ri.FS_FreeFile( faceData );
		ri.Printf( PRINT_WARNING, "R_LoadScalableFont: FreeType, unable to allocate new face for '%s'.\n", path );
		return qfalse;
	}

	if ( FT_Set_Char_Size( face, pointSize << 6, pointSize << 6, dpi, dpi ) ) {
		ri.FS_FreeFile( faceData );
		ri.Printf( PRINT_WARNING, "R_LoadScalableFont: FreeType, unable to set face char size for '%s'.\n", path );
		return qfalse;
	}

	// 256x256 buffer; once full, register/clean/keep going until done
	out = ri.Z_Malloc( 256 * 256 );
	if ( out == NULL ) {
		ri.FS_FreeFile( faceData );
		ri.Printf( PRINT_WARNING, "R_LoadScalableFont: ri.Z_Malloc failure during output image creation.\n" );
		return qfalse;
	}
	Com_Memset( out, 0, 256 * 256 );

	maxHeight = 0;

	for ( i = GLYPH_START; i <= GLYPH_END; i++ ) {
		RE_ConstructGlyphInfo( out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qtrue );
	}

	xOut = 0;
	yOut = 0;
	i = GLYPH_START;
	lastStart = i;
	imageNumber = 0;

	while ( i <= GLYPH_END + 1 ) {

		if ( i == GLYPH_END + 1 ) {
			// upload/save current image buffer
			xOut = yOut = -1;
		} else {
			glyph = RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qfalse);
		}

		if ( xOut == -1 || yOut == -1 ) {
			// ran out of room -- build an image from the bitmap, assign handles so far
			scaledSize = 256 * 256;
			newSize = scaledSize * 4;
			imageBuff = ri.Z_Malloc( newSize );
			left = 0;
			max = 0;
			for ( k = 0; k < ( scaledSize ) ; k++ ) {
				if ( max < out[k] ) {
					max = out[k];
				}
			}

			if ( max > 0 ) {
				max = 255 / max;
			}

			for ( k = 0; k < ( scaledSize ) ; k++ ) {
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;

				imageBuff[left++] = ( (float)out[k] * max );
			}

			Com_sprintf( name, sizeof( name ), "fonts/%s_%i_%i.tga", fontName, imageNumber++, pointSize );

			image = R_CreateImage( name, imageBuff, 256, 256, IMGFLAG_CLAMPTOEDGE );
			h = RE_RegisterShaderFromImage( name, image );
			for ( j = lastStart; j < i; j++ ) {
				font->glyphs[j].glyph = h;
				Q_strncpyz( font->glyphs[j].shaderName, name, sizeof( font->glyphs[j].shaderName ) );
			}
			lastStart = i;
			Com_Memset( out, 0, 256 * 256 );
			xOut = 0;
			yOut = 0;
			ri.Free( imageBuff );
			if ( i == GLYPH_END + 1 )
				i++;
		} else {
			Com_Memcpy( &font->glyphs[i], glyph, sizeof( glyphInfo_t ) );
			i++;
		}
	}

	// change the scale to be relative to 1 based on 72 dpi ( so dpi of 144 means a scale of .5 )
	glyphScale = 72.0f / dpi;

	// we also need to adjust the scale based on point size relative to 48 points as the ui scaling is based on a 48 point font
	glyphScale *= 48.0f / pointSize;

	font->glyphScale = glyphScale;
	Com_sprintf( font->name, sizeof( font->name ), "fonts/%s_%i.dat", fontName, pointSize );

	ri.Free( out );
	ri.FS_FreeFile( faceData );
	FT_Done_Face( face );

	return qtrue;
}
#endif

void RE_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font ) {
	char cacheName[1024];
	int i;

	if (!fontName) {
		ri.Printf(PRINT_ALL, "RE_RegisterFont: called with empty name\n");
		return;
	}

	if ( pointSize <= 0 ) {
		pointSize = 12;
	}

	if ( registeredFontCount >= MAX_FONTS ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterFont: Too many fonts registered already.\n" );
		return;
	}

	// cache key is fontName+pointSize so two faces at the same point size don't collide
	Com_sprintf( cacheName, sizeof( cacheName ), "fonts/%s_%i.dat", fontName, pointSize );
	for ( i = 0; i < registeredFontCount; i++ ) {
		if ( Q_stricmp( cacheName, registeredFont[i].name ) == 0 ) {
			Com_Memcpy( font, &registeredFont[i], sizeof( fontInfo_t ) );
			return;
		}
	}

#ifdef BUILD_FREETYPE
	if ( R_LoadScalableFont( fontName, pointSize, font ) ) {
		Com_Memcpy( &registeredFont[registeredFontCount++], font, sizeof( fontInfo_t ) );
		return;
	}
#endif

	// fall back to a legacy pre-baked font (e.g. retail fontImage_<size>.dat/.tga)
	if ( R_LoadPreRenderedFont( va( "fonts/fontImage_%i.dat", pointSize ), font ) ) {
		Com_Memcpy( &registeredFont[registeredFontCount++], font, sizeof( fontInfo_t ) );
		return;
	}

	ri.Printf( PRINT_WARNING, "RE_RegisterFont: failed to register font '%s' at point size %i\n", fontName, pointSize );
}

#ifdef BUILD_FREETYPE
// Same as R_LoadScalableFont, but rasterizes the full extended Unicode range into a fontInfoExtra_t instead of just ASCII
static qboolean R_LoadScalableFontExtended( const char *fontName, int pointSize, fontInfoExtra_t *font ) {
	FT_Face face;
	int j, k, xOut, yOut, lastStart, imageNumber;
	int scaledSize, newSize, maxHeight, left;
	unsigned char *out, *imageBuff;
	glyphInfo_t *glyph;
	image_t *image;
	qhandle_t h;
	float max;
	float dpi = 72.0f * FONT_SUPERSAMPLE;
	float glyphScale;
	void *faceData;
	int i, len, fmt;
	char path[1024];
	char name[1024];
	qboolean found;

	if ( ftLibrary == NULL ) {
		return qfalse;
	}

	found = qfalse;
	for ( fmt = 0; fmt < NUM_SUPPORTED_FONT_FORMATS; fmt++ ) {
		Com_sprintf( path, sizeof( path ), "fonts/%s.%s", fontName, supportedFontFormats[fmt] );
		if ( ri.FS_ReadFile( path, NULL ) > 0 ) {
			found = qtrue;
			break;
		}
	}

	if ( !found ) {
		return qfalse;
	}

	len = ri.FS_ReadFile( path, &faceData );
	if ( len <= 0 ) {
		ri.Printf( PRINT_WARNING, "R_LoadScalableFontExtended: Unable to read font file '%s'\n", path );
		return qfalse;
	}

	if ( FT_New_Memory_Face( ftLibrary, faceData, len, 0, &face ) ) {
		ri.FS_FreeFile( faceData );
		ri.Printf( PRINT_WARNING, "R_LoadScalableFontExtended: FreeType, unable to allocate new face for '%s'.\n", path );
		return qfalse;
	}

	if ( FT_Set_Char_Size( face, pointSize << 6, pointSize << 6, dpi, dpi ) ) {
		ri.FS_FreeFile( faceData );
		ri.Printf( PRINT_WARNING, "R_LoadScalableFontExtended: FreeType, unable to set face char size for '%s'.\n", path );
		return qfalse;
	}

	out = ri.Z_Malloc( 256 * 256 );
	if ( out == NULL ) {
		ri.FS_FreeFile( faceData );
		ri.Printf( PRINT_WARNING, "R_LoadScalableFontExtended: ri.Z_Malloc failure during output image creation.\n" );
		return qfalse;
	}
	Com_Memset( out, 0, 256 * 256 );

	maxHeight = 0;

	for ( i = GLYPH_START; i <= GLYPH_UTF_END; i++ ) {
		RE_ConstructGlyphInfo( out, &xOut, &yOut, &maxHeight, face, (unsigned long)i, qtrue );
	}

	xOut = 0;
	yOut = 0;
	i = GLYPH_START;
	lastStart = i;
	imageNumber = 0;

	while ( i <= GLYPH_UTF_END + 1 ) {

		if ( i == GLYPH_UTF_END + 1 ) {
			// upload/save current image buffer
			xOut = yOut = -1;
		} else {
			glyph = RE_ConstructGlyphInfo( out, &xOut, &yOut, &maxHeight, face, (unsigned long)i, qfalse );
		}

		if ( xOut == -1 || yOut == -1 ) {
			// ran out of room -- build an image from the bitmap, assign handles so far
			scaledSize = 256 * 256;
			newSize = scaledSize * 4;
			imageBuff = ri.Z_Malloc( newSize );
			left = 0;
			max = 0;
			for ( k = 0; k < ( scaledSize ) ; k++ ) {
				if ( max < out[k] ) {
					max = out[k];
				}
			}

			if ( max > 0 ) {
				max = 255 / max;
			}

			for ( k = 0; k < ( scaledSize ) ; k++ ) {
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;

				imageBuff[left++] = ( (float)out[k] * max );
			}

			Com_sprintf( name, sizeof( name ), "fonts/%s_%i_%i_utf8.tga", fontName, imageNumber++, pointSize );

			image = R_CreateImage( name, imageBuff, 256, 256, IMGFLAG_CLAMPTOEDGE );
			h = RE_RegisterShaderFromImage( name, image );
			for ( j = lastStart; j < i; j++ ) {
				if ( j <= GLYPH_END ) {
					font->glyphs[j].glyph = h;
					Q_strncpyz( font->glyphs[j].shaderName, name, sizeof( font->glyphs[j].shaderName ) );
				} else {
					font->glyphsUTF8[j].glyph = h;
					Q_strncpyz( font->glyphsUTF8[j].shaderName, name, sizeof( font->glyphsUTF8[j].shaderName ) );
				}
			}
			lastStart = i;
			Com_Memset( out, 0, 256 * 256 );
			xOut = 0;
			yOut = 0;
			ri.Free( imageBuff );
			if ( i == GLYPH_UTF_END + 1 )
				i++;
		} else {
			if ( i <= GLYPH_END ) {
				Com_Memcpy( &font->glyphs[i], glyph, sizeof( glyphInfo_t ) );
			} else {
				Com_Memcpy( &font->glyphsUTF8[i], glyph, sizeof( glyphInfo_t ) );
			}
			i++;
		}
	}

	// change the scale to be relative to 1 based on 72 dpi ( so dpi of 144 means a scale of .5 )
	glyphScale = 72.0f / dpi;

	// we also need to adjust the scale based on point size relative to 48 points as the ui scaling is based on a 48 point font
	glyphScale *= 48.0f / pointSize;

	font->glyphScale = glyphScale;
	Com_sprintf( font->name, sizeof( font->name ), "fonts/%s_%i_utf8.dat", fontName, pointSize );

	ri.Free( out );
	ri.FS_FreeFile( faceData );
	FT_Done_Face( face );

	return qtrue;
}
#endif

void RE_RegisterFontExtended( const char *fontName, int pointSize, fontInfoExtra_t *font ) {
	char cacheName[1024];
	int i;

	if ( !fontName ) {
		ri.Printf( PRINT_ALL, "RE_RegisterFontExtended: called with empty name\n" );
		return;
	}

	if ( pointSize <= 0 ) {
		pointSize = 12;
	}

	// cache key mirrors what R_LoadScalableFontExtended writes into font->name
	Com_sprintf( cacheName, sizeof( cacheName ), "fonts/%s_%i_utf8.dat", fontName, pointSize );
	for ( i = 0; i < registeredFontExtendedCount; i++ ) {
		if ( Q_stricmp( cacheName, registeredFontExtended[i].name ) == 0 ) {
			Com_Memcpy( font, &registeredFontExtended[i], sizeof( fontInfoExtra_t ) );
			return;
		}
	}

	Com_Memset( font, 0, sizeof( fontInfoExtra_t ) );

#ifdef BUILD_FREETYPE
	if ( R_LoadScalableFontExtended( fontName, pointSize, font ) ) {
		if ( registeredFontExtendedCount < MAX_EXTENDED_FONTS ) {
			Com_Memcpy( &registeredFontExtended[registeredFontExtendedCount++], font, sizeof( fontInfoExtra_t ) );
		} else {
			ri.Printf( PRINT_WARNING, "RE_RegisterFontExtended: extended font cache full, '%s' won't be cached (will re-rasterize if requested again)\n", fontName );
		}
		return;
	}
#endif

	ri.Printf( PRINT_WARNING, "RE_RegisterFontExtended: failed to register font '%s' at point size %i (FreeType only; no legacy .dat fallback for extended fonts)\n", fontName, pointSize );
}



void R_InitFreeType( void ) {
#ifdef BUILD_FREETYPE
	if ( FT_Init_FreeType( &ftLibrary ) ) {
		ri.Printf(PRINT_WARNING, "R_InitFreeType: Unable to initialize FreeType.\n");
	}
#endif
	registeredFontCount = 0;
	registeredFontExtendedCount = 0;
}


void R_DoneFreeType( void ) {
#ifdef BUILD_FREETYPE
	if ( ftLibrary ) {
		FT_Done_FreeType( ftLibrary );
		ftLibrary = NULL;
	}
#endif
	registeredFontCount = 0;
	registeredFontExtendedCount = 0;
}
