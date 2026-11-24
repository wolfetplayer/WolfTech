// tr_image.c -- image_t registry and TGA/JPG/PNG file loading.

#include <string.h>

#include "tr_local.h"
#include "tr_image.h"
#include "vk_image.h"

void R_LoadTGA( const char *name, byte **pic, int *width, int *height );
void R_LoadJPG( const char *name, byte **pic, int *width, int *height );
void R_LoadPNG( const char *name, byte **pic, int *width, int *height );

static image_t r_images[MAX_DRAWIMAGES];
static int r_numImages;

static image_t *default_image;
static image_t *white_image;

typedef struct {
	const char *ext;
	void ( *loader )( const char *, byte **, int *, int * );
} imageLoader_t;

// Fallback order tried when the name's own extension doesn't resolve; matches opengl1/rend2.
static const imageLoader_t imageLoaders[] = {
	{ "tga", R_LoadTGA },
	{ "jpg", R_LoadJPG },
	{ "jpeg", R_LoadJPG },
	{ "png", R_LoadPNG },
};
static const int numImageLoaders = sizeof( imageLoaders ) / sizeof( imageLoaders[0] );

void R_LoadImage( const char *name, byte **pic, int *width, int *height ) {
	const char *ext;
	char localName[MAX_QPATH];
	int i;

	*pic = NULL;
	if ( width ) *width = 0;
	if ( height ) *height = 0;

	Q_strncpyz( localName, name, sizeof( localName ) );
	ext = COM_GetExtension( localName );

	if ( ext[0] != '\0' ) {
		for ( i = 0; i < numImageLoaders; i++ ) {
			if ( !Q_stricmp( ext, imageLoaders[i].ext ) ) {
				imageLoaders[i].loader( localName, pic, width, height );
				if ( *pic != NULL ) {
					return;
				}
				// Loader found but no valid file -- fall through and try every other extension.
				break;
			}
		}
	}

	COM_StripExtension( name, localName, sizeof( localName ) );

	for ( i = 0; i < numImageLoaders; i++ ) {
		char testName[MAX_QPATH];
		Com_sprintf( testName, sizeof( testName ), "%s.%s", localName, imageLoaders[i].ext );
		imageLoaders[i].loader( testName, pic, width, height );
		if ( *pic != NULL ) {
			return;
		}
	}

	// missing asset, or shipped as an unsupported format (PCX/BMP)
	ri.Printf( PRINT_WARNING, "R_LoadImage: no .tga/.jpg/.png found for '%s'\n", name );
}

static image_t *R_AllocImage( const char *name ) {
	image_t *image;

	if ( r_numImages >= MAX_DRAWIMAGES ) {
		ri.Printf( PRINT_WARNING, "R_AllocImage: MAX_DRAWIMAGES hit, can't load '%s'\n", name );
		return NULL;
	}

	image = &r_images[r_numImages++];
	memset( image, 0, sizeof( *image ) );
	Q_strncpyz( image->name, name, sizeof( image->name ) );
	return image;
}

// Builds an image_t from an already-decoded RGBA8 buffer (caller keeps ownership of pic).
image_t *R_CreateImage( const char *name, byte *pic, int width, int height, int flags ) {
	image_t *image;

	image = R_AllocImage( name );
	if ( !image ) {
		return NULL;
	}

	image->width = width;
	image->height = height;
	image->flags = flags;

	if ( !vk_create_texture( image, pic, width, height, flags ) ) {
		r_numImages--;
		return NULL;
	}

	return image;
}

image_t *R_FindImageFile( const char *name, int flags ) {
	int i;
	byte *pic;
	int width, height;
	image_t *image;

	if ( !name ) {
		return NULL;
	}

	for ( i = 0; i < r_numImages; i++ ) {
		if ( !Q_stricmp( r_images[i].name, name ) ) {
			return &r_images[i];
		}
	}

	R_LoadImage( name, &pic, &width, &height );
	if ( !pic ) {
		return NULL;
	}

	image = R_CreateImage( name, pic, width, height, flags );
	ri.Free( pic );
	return image;
}

image_t *R_GetDefaultImage( void ) {
	return default_image;
}

image_t *R_GetWhiteImage( void ) {
	return white_image;
}

int R_GetNumImages( void ) {
	return r_numImages;
}

image_t *R_GetImageByIndex( int index ) {
	if ( index < 0 || index >= r_numImages ) {
		return NULL;
	}
	return &r_images[index];
}

#define DEFAULT_IMAGE_SIZE 16

static void R_CreateDefaultImage( void ) {
	byte pixels[DEFAULT_IMAGE_SIZE * DEFAULT_IMAGE_SIZE * 4];
	int x, y;

	for ( y = 0; y < DEFAULT_IMAGE_SIZE; y++ ) {
		for ( x = 0; x < DEFAULT_IMAGE_SIZE; x++ ) {
			byte *p = &pixels[( y * DEFAULT_IMAGE_SIZE + x ) * 4];
			qboolean border = ( x == 0 || y == 0 || x == DEFAULT_IMAGE_SIZE - 1 || y == DEFAULT_IMAGE_SIZE - 1 );
			byte v = border ? 0 : 32;
			p[0] = p[1] = p[2] = v;
			p[3] = 255;
		}
	}

	default_image = R_AllocImage( "*default" );
	if ( default_image ) {
		default_image->width = DEFAULT_IMAGE_SIZE;
		default_image->height = DEFAULT_IMAGE_SIZE;
		vk_create_texture( default_image, pixels, DEFAULT_IMAGE_SIZE, DEFAULT_IMAGE_SIZE, IMGFLAG_NOMIPMAP );
	}
}


static void R_CreateWhiteImage( void ) {
	byte pixels[4] = { 255, 255, 255, 255 };

	white_image = R_AllocImage( "*white" );
	if ( white_image ) {
		white_image->width = 1;
		white_image->height = 1;
		vk_create_texture( white_image, pixels, 1, 1, IMGFLAG_NOMIPMAP );
	}
}

void R_InitImages( void ) {
	r_numImages = 0;
	memset( r_images, 0, sizeof( r_images ) );
	default_image = NULL;
	white_image = NULL;

	R_CreateDefaultImage();
	R_CreateWhiteImage();
}

void R_ShutdownImages( void ) {
	int i;
	for ( i = 0; i < r_numImages; i++ ) {
		vk_destroy_texture( &r_images[i] );
	}
	r_numImages = 0;
	default_image = NULL;
	white_image = NULL;
}
