/*
===========================================================================
Copyright (C) 2006 Kirk Barnes
Copyright (C) 2006-2008 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of XreaL source code.

XreaL source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

XreaL source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XreaL source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_fbo.c -- minimal framebuffer-object core backing tr.mainFbo and the \r_bloom scratch FBOs, trimmed from code/rend2/tr_fbo.c; see tr_arb.c for the ARB fragment program effects that sample these

#include "tr_local.h"

qboolean fboEnabled = qfalse;

static FBO_t *currentFbo = NULL;


/*
==============
FBO_CreateColorImage

Allocates an empty, unmipmapped, uncompressed RGBA texture sized exactly to
width/height, for use as a color attachment. Deliberately bypasses
R_CreateImage()/Upload32(): that path resamples to power-of-two dimensions,
may apply picmip/compression, and isn't NULL-pic safe -- none of which is
appropriate (or safe) for a render target that must stay pixel-exact.
==============
*/
static image_t *FBO_CreateColorImage( const char *name, int width, int height, GLint internalFormat )
{
	image_t *image;

	image = ri.Hunk_Alloc( sizeof( *image ), h_low );
	Com_Memset( image, 0, sizeof( *image ) );
	Q_strncpyz( image->imgName, name, sizeof( image->imgName ) );
	image->width = image->uploadWidth = width;
	image->height = image->uploadHeight = height;
	image->type = IMGTYPE_COLORALPHA;
	image->flags = IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE;
	image->internalFormat = internalFormat;

	qglGenTextures( 1, &image->texnum );
	GL_Bind( image );

	qglTexImage2D( GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	glState.currenttextures[glState.currenttmu] = 0;
	qglBindTexture( GL_TEXTURE_2D, 0 );

	return image;
}


/*
==============
FBO_Create
==============
*/
FBO_t *FBO_Create( const char *name, int width, int height )
{
	FBO_t *fbo;

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Error( ERR_DROP, "FBO_Create: \"%s\" is too long", name );
	}

	fbo = ri.Hunk_Alloc( sizeof( *fbo ), h_low );
	Com_Memset( fbo, 0, sizeof( *fbo ) );
	Q_strncpyz( fbo->name, name, sizeof( fbo->name ) );
	fbo->width = width;
	fbo->height = height;

	qglGenFramebuffers( 1, &fbo->frameBuffer );

	return fbo;
}


/*
==============
FBO_AttachImage
==============
*/
void FBO_AttachImage( FBO_t *fbo, struct image_s *image, GLenum attachment )
{
	qglBindFramebuffer( GL_FRAMEBUFFER, fbo->frameBuffer );
	qglFramebufferTexture2D( GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, ( (image_t *)image )->texnum, 0 );

	if ( attachment == GL_COLOR_ATTACHMENT0 ) {
		fbo->colorImage = image;
	}
}


/*
==============
FBO_CreateColorBuffer

Attaches a renderbuffer as the color target, used instead of FBO_AttachImage's
texture attachment for multisample FBOs (a plain GL_ARB_framebuffer_object
multisample renderbuffer can't be sampled directly -- see FBO_ResolveMSAA).
==============
*/
void FBO_CreateColorBuffer( FBO_t *fbo, GLenum format, int samples )
{
	if ( !fbo->colorBuffer ) {
		qglGenRenderbuffers( 1, &fbo->colorBuffer );
	}

	qglBindRenderbuffer( GL_RENDERBUFFER, fbo->colorBuffer );
	if ( samples >= 2 ) {
		qglRenderbufferStorageMultisample( GL_RENDERBUFFER, samples, format, fbo->width, fbo->height );
	} else {
		qglRenderbufferStorage( GL_RENDERBUFFER, format, fbo->width, fbo->height );
	}

	qglBindFramebuffer( GL_FRAMEBUFFER, fbo->frameBuffer );
	qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fbo->colorBuffer );
}


/*
==============
FBO_CreateDepthBuffer

Attaches a renderbuffer as the depth target. GL_DEPTH24_STENCIL8 (or
GL_DEPTH_STENCIL) additionally binds it as the stencil target, since
r_shadows/r_measureOverdraw need a working stencil buffer under \r_fbo 1 too.
==============
*/
void FBO_CreateDepthBuffer( FBO_t *fbo, GLenum format, int samples )
{
	if ( !fbo->depthBuffer ) {
		qglGenRenderbuffers( 1, &fbo->depthBuffer );
	}

	qglBindRenderbuffer( GL_RENDERBUFFER, fbo->depthBuffer );
	if ( samples >= 2 ) {
		qglRenderbufferStorageMultisample( GL_RENDERBUFFER, samples, format, fbo->width, fbo->height );
	} else {
		qglRenderbufferStorage( GL_RENDERBUFFER, format, fbo->width, fbo->height );
	}

	qglBindFramebuffer( GL_FRAMEBUFFER, fbo->frameBuffer );
	qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo->depthBuffer );

	if ( format == GL_DEPTH_STENCIL || format == GL_DEPTH24_STENCIL8 ) {
		qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fbo->depthBuffer );
	}
}


/*
==============
R_CheckFBO
==============
*/
qboolean R_CheckFBO( const FBO_t *fbo )
{
	GLenum code;

	qglBindFramebuffer( GL_FRAMEBUFFER, fbo->frameBuffer );
	code = qglCheckFramebufferStatus( GL_FRAMEBUFFER );

	if ( code == GL_FRAMEBUFFER_COMPLETE ) {
		return qtrue;
	}

	switch ( code ) {
		case GL_FRAMEBUFFER_UNSUPPORTED:
			ri.Printf( PRINT_WARNING, "R_CheckFBO: (%s) unsupported framebuffer format\n", fbo->name );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			ri.Printf( PRINT_WARNING, "R_CheckFBO: (%s) incomplete attachment\n", fbo->name );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			ri.Printf( PRINT_WARNING, "R_CheckFBO: (%s) missing attachment\n", fbo->name );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
			ri.Printf( PRINT_WARNING, "R_CheckFBO: (%s) missing draw buffer\n", fbo->name );
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
			ri.Printf( PRINT_WARNING, "R_CheckFBO: (%s) missing read buffer\n", fbo->name );
			break;
		default:
			ri.Printf( PRINT_WARNING, "R_CheckFBO: (%s) unknown error 0x%x\n", fbo->name, code );
			break;
	}

	return qfalse;
}


/*
==============
FBO_Bind
==============
*/
void FBO_Bind( FBO_t *fbo )
{
	if ( currentFbo == fbo ) {
		return;
	}

	qglBindFramebuffer( GL_FRAMEBUFFER, fbo ? fbo->frameBuffer : 0 );
	currentFbo = fbo;
}


/*
==============
FBO_FastBlit
==============
*/
void FBO_FastBlit( const FBO_t *src, const FBO_t *dst, GLbitfield buffers, GLenum filter )
{
	int srcW, srcH, dstW, dstH;

	srcW = src ? src->width  : glConfig.vidWidth;
	srcH = src ? src->height : glConfig.vidHeight;
	dstW = dst ? dst->width  : glConfig.vidWidth;
	dstH = dst ? dst->height : glConfig.vidHeight;

	qglBindFramebuffer( GL_READ_FRAMEBUFFER, src ? src->frameBuffer : 0 );
	qglBindFramebuffer( GL_DRAW_FRAMEBUFFER, dst ? dst->frameBuffer : 0 );

	// reset scissor so a stale reduced-viewport rect (cg_viewsize < 100) doesn't clip the blit
	qglScissor( 0, 0, dstW, dstH );

	qglBlitFramebuffer( 0, 0, srcW, srcH, 0, 0, dstW, dstH, buffers, filter );

	qglBindFramebuffer( GL_FRAMEBUFFER, 0 );
	currentFbo = NULL;
}


/*
==============
FBO_Delete
==============
*/
static void FBO_Delete( FBO_t *fbo )
{
	if ( !fbo ) {
		return;
	}

	if ( fbo->colorImage ) {
		qglDeleteTextures( 1, &( (image_t *)fbo->colorImage )->texnum );
		fbo->colorImage = NULL;
	}
	if ( fbo->colorBuffer ) {
		qglDeleteRenderbuffers( 1, &fbo->colorBuffer );
		fbo->colorBuffer = 0;
	}
	if ( fbo->depthBuffer ) {
		qglDeleteRenderbuffers( 1, &fbo->depthBuffer );
		fbo->depthBuffer = 0;
	}
	if ( fbo->frameBuffer ) {
		qglDeleteFramebuffers( 1, &fbo->frameBuffer );
		fbo->frameBuffer = 0;
	}
}


/*
==============
FBO_Init
==============
*/
void FBO_Init( void )
{
	int width, height;
	int bloomWidth, bloomHeight;
	int samples;
	GLint maxSamples;
	image_t *colorImage;
	int i;

	ri.Printf( PRINT_ALL, "------- FBO_Init -------\n" );

	fboEnabled = qfalse;
	tr.mainFbo = NULL;
	tr.msaaFbo = NULL;
	currentFbo = NULL;

	if ( !glRefConfig.framebufferObject ) {
		return;
	}

	if ( !r_fbo->integer ) {
		return;
	}

	width = glConfig.vidWidth;
	height = glConfig.vidHeight;

	colorImage = FBO_CreateColorImage( "_main", width, height, GL_RGBA8 );

	tr.mainFbo = FBO_Create( "_main", width, height );
	FBO_AttachImage( tr.mainFbo, colorImage, GL_COLOR_ATTACHMENT0 );
	FBO_CreateDepthBuffer( tr.mainFbo, GL_DEPTH24_STENCIL8, 0 );

	if ( !R_CheckFBO( tr.mainFbo ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: main FBO incomplete, disabling \\r_fbo\n" );
		FBO_Delete( tr.mainFbo );
		tr.mainFbo = NULL;
		return;
	}

	// multisample render target the 3D scene actually draws into (FBO_RenderTarget()), resolved
	// into tr.mainFbo's texture once per frame before gamma/bloom sample it (FBO_ResolveMSAA)
	samples = 0;
	if ( glRefConfig.framebufferMultisample && r_ext_multisample->integer >= 2 ) {
		maxSamples = 0;
		qglGetIntegerv( GL_MAX_SAMPLES, &maxSamples );
		samples = r_ext_multisample->integer;
		if ( samples > maxSamples ) {
			samples = maxSamples;
		}
	}

	if ( samples >= 2 ) {
		tr.msaaFbo = FBO_Create( "_msaa", width, height );
		FBO_CreateColorBuffer( tr.msaaFbo, GL_RGBA8, samples );
		FBO_CreateDepthBuffer( tr.msaaFbo, GL_DEPTH24_STENCIL8, samples );

		if ( !R_CheckFBO( tr.msaaFbo ) ) {
			ri.Printf( PRINT_WARNING, "WARNING: multisample FBO incomplete, disabling \\r_ext_multisample\n" );
			FBO_Delete( tr.msaaFbo );
			tr.msaaFbo = NULL;
		}
	}

	// half-res ping-pong scratch for \r_bloom, always created alongside so it can toggle live
	bloomWidth = width / 2;
	if ( bloomWidth < 4 ) {
		bloomWidth = 4;
	}
	bloomHeight = height / 2;
	if ( bloomHeight < 4 ) {
		bloomHeight = 4;
	}

	for ( i = 0; i < 2; i++ ) {
		image_t *bloomColorImage = FBO_CreateColorImage( va( "_bloom%d", i ), bloomWidth, bloomHeight, GL_RGBA8 );

		tr.bloomFbo[i] = FBO_Create( va( "_bloom%d", i ), bloomWidth, bloomHeight );
		FBO_AttachImage( tr.bloomFbo[i], bloomColorImage, GL_COLOR_ATTACHMENT0 );

		if ( !R_CheckFBO( tr.bloomFbo[i] ) ) {
			ri.Printf( PRINT_WARNING, "WARNING: bloom FBO %i incomplete, disabling \\r_bloom\n", i );
			FBO_Delete( tr.bloomFbo[0] );
			FBO_Delete( tr.bloomFbo[1] );
			tr.bloomFbo[0] = tr.bloomFbo[1] = NULL;
			break;
		}
	}

	fboEnabled = qtrue;

	ARB_InitPrograms();

	FBO_Bind( FBO_RenderTarget() );
}


/*
==============
FBO_RenderTarget

Returns the FBO the 3D scene should actually render into: tr.msaaFbo when
multisampling is active, otherwise tr.mainFbo directly.
==============
*/
FBO_t *FBO_RenderTarget( void )
{
	return tr.msaaFbo ? tr.msaaFbo : tr.mainFbo;
}


/*
==============
FBO_ResolveMSAA

Resolves tr.msaaFbo's multisampled color into tr.mainFbo's texture, once per
frame -- a multisample renderbuffer can't be sampled directly, so this must
run before anything (gamma correction, bloom) reads tr.mainFbo->colorImage.
No-op when multisampling isn't active.
==============
*/
void FBO_ResolveMSAA( void )
{
	if ( !tr.msaaFbo || backEnd.doneMSAAResolve ) {
		return;
	}
	backEnd.doneMSAAResolve = qtrue;

	FBO_FastBlit( tr.msaaFbo, tr.mainFbo, GL_COLOR_BUFFER_BIT, GL_NEAREST );
}


/*
==============
FBO_Shutdown
==============
*/
void FBO_Shutdown( void )
{
	ri.Printf( PRINT_ALL, "------- FBO_Shutdown -------\n" );

	if ( !glRefConfig.framebufferObject ) {
		return;
	}

	ARB_ShutdownPrograms();

	FBO_Bind( NULL );

	FBO_Delete( tr.mainFbo );
	tr.mainFbo = NULL;

	FBO_Delete( tr.msaaFbo );
	tr.msaaFbo = NULL;

	FBO_Delete( tr.bloomFbo[0] );
	FBO_Delete( tr.bloomFbo[1] );
	tr.bloomFbo[0] = tr.bloomFbo[1] = NULL;

	fboEnabled = qfalse;
}


/*
==============
R_FBOList_f
==============
*/
void R_FBOList_f( void )
{
	if ( !fboEnabled ) {
		ri.Printf( PRINT_ALL, "FBOs are not enabled (\\r_fbo 0, or GL_ARB_framebuffer_object unavailable).\n" );
		return;
	}

	int count = 1;

	ri.Printf( PRINT_ALL, "             size       name\n" );
	ri.Printf( PRINT_ALL, "----------------------------------------------------------\n" );
	ri.Printf( PRINT_ALL, "  %4i %4i  %s\n", tr.mainFbo->width, tr.mainFbo->height, tr.mainFbo->name );
	if ( tr.msaaFbo ) {
		ri.Printf( PRINT_ALL, "  %4i %4i  %s\n", tr.msaaFbo->width, tr.msaaFbo->height, tr.msaaFbo->name );
		count++;
	}
	if ( tr.bloomFbo[0] ) {
		ri.Printf( PRINT_ALL, "  %4i %4i  %s\n", tr.bloomFbo[0]->width, tr.bloomFbo[0]->height, tr.bloomFbo[0]->name );
		ri.Printf( PRINT_ALL, "  %4i %4i  %s\n", tr.bloomFbo[1]->width, tr.bloomFbo[1]->height, tr.bloomFbo[1]->name );
		count += 2;
	}
	ri.Printf( PRINT_ALL, " %i FBO%s\n", count, count == 1 ? "" : "s" );
}
