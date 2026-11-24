// tr_image.h -- image loading and the image_t registry (TGA/JPG/PNG, opengl1/rend2 extension-fallback).

#ifndef TR_IMAGE_H_
#define TR_IMAGE_H_

#include "vk_instance.h"

#define MAX_DRAWIMAGES  4096

typedef enum {
	IMGFLAG_NONE = 0,
	IMGFLAG_NOMIPMAP = 1,
	IMGFLAG_CLAMPTOEDGE = 2,
} imgFlags_t;

typedef struct image_s {
	char name[128];
	int width, height;
	int flags;

	VkImage vk_image;
	VkDeviceMemory vk_memory;
	VkImageView vk_view;
	VkSampler vk_sampler;
	VkDescriptorSet vk_descriptor;

	qboolean registered;
} image_t;

void R_InitImages( void );
void R_ShutdownImages( void );

// Returns NULL if the file doesn't exist or isn't a supported format.
image_t *R_FindImageFile( const char *name, int flags );

// Builds an image_t from pixels already in memory (pic is RGBA8, caller keeps ownership).
image_t *R_CreateImage( const char *name, byte *pic, int width, int height, int flags );

// Decodes a file to an RGBA8 buffer (ri.Z_Malloc'd -- caller frees with ri.Free).
void R_LoadImage( const char *name, byte **pic, int *width, int *height );


image_t *R_GetDefaultImage( void );
image_t *R_GetWhiteImage( void );

// Every registered image, in load order -- used by r_showImages' texture-atlas debug overlay.
int R_GetNumImages( void );
image_t *R_GetImageByIndex( int index );

#endif // TR_IMAGE_H_
