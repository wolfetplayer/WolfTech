/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// vk_create_window.c -- SDL3 window + Vulkan surface creation, adapted from vkQuake3's SDL2 version (GPLv2-or-later).

#ifdef USE_LOCAL_HEADERS
#	include "SDL3/SDL.h"
#	include "SDL3/SDL_vulkan.h"
#else
#	include <SDL3/SDL.h>
#	include <SDL3/SDL_vulkan.h>
#endif

#include "VKimpl.h"
#include "vk_instance.h"
#include "glConfig.h"
#include "../sdl/sdl_icon.h"

cvar_t *r_mode;
cvar_t *r_fullscreen;
cvar_t *r_allowResize;
cvar_t *r_centerWindow;
cvar_t *r_displayIndex;

static SDL_Window *window_sdl = NULL;

static int VKimp_SetMode( int mode, qboolean fullscreen ) {
	Uint32 flags = SDL_WINDOW_VULKAN;
	SDL_DisplayMode desktopMode;
	const SDL_DisplayMode *pDesktopMode;
	SDL_DisplayID display;
	int x, y;

	if ( r_allowResize->integer ) {
		flags |= SDL_WINDOW_RESIZABLE;
	}

	display = SDL_GetPrimaryDisplay();
	pDesktopMode = display ? SDL_GetDesktopDisplayMode( display ) : NULL;

	if ( pDesktopMode ) {
		desktopMode = *pDesktopMode;
	} else {
		ri.Printf( PRINT_ALL, " SDL_GetDesktopDisplayMode failed: %s\n", SDL_GetError() );
		desktopMode.w = 640;
		desktopMode.h = 480;
		desktopMode.refresh_rate = 60;
		mode = 3;
		fullscreen = qfalse;
	}

	R_SetWinMode( mode, desktopMode.w, desktopMode.h, (unsigned int)desktopMode.refresh_rate );

	if ( window_sdl != NULL ) {
		return 0;
	}

	if ( fullscreen ) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}

	int width, height;
	R_GetWinResolution( &width, &height );

	window_sdl = SDL_CreateWindow( CLIENT_WINDOW_TITLE, width, height, flags );
	if ( window_sdl == NULL ) {
		ri.Printf( PRINT_WARNING, " Couldn't create a window: %s\n", SDL_GetError() );
		return -1;
	}

	if ( r_centerWindow->integer && !fullscreen ) {
		x = ( desktopMode.w / 2 ) - ( width / 2 );
		y = ( desktopMode.h / 2 ) - ( height / 2 );
		SDL_SetWindowPosition( window_sdl, x, y );
	}

	return 0;
}

void vk_createWindow( void ) {
	SDL_Surface *icon = NULL;

	ri.Printf( PRINT_ALL, "\n...Creating window (Vulkan, SDL3)...\n" );

	r_displayIndex = ri.Cvar_Get( "r_displayIndex", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_mode = ri.Cvar_Get( "r_mode", "-2", CVAR_ARCHIVE | CVAR_LATCH );
	r_fullscreen = ri.Cvar_Get( "r_fullscreen", "0", CVAR_ARCHIVE );
	r_allowResize = ri.Cvar_Get( "r_allowResize", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_centerWindow = ri.Cvar_Get( "r_centerWindow", "1", CVAR_ARCHIVE );

	R_InitDisplayResolution();

#ifdef USE_ICON
	icon = SDL_CreateSurfaceFrom(
		CLIENT_WINDOW_ICON.width,
		CLIENT_WINDOW_ICON.height,
		SDL_PIXELFORMAT_RGBA32,
		(void *)CLIENT_WINDOW_ICON.pixel_data,
		CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width );

	if ( icon == NULL ) {
		ri.Printf( PRINT_ALL, " SDL_CreateSurfaceFrom failed: %s\n", SDL_GetError() );
	}
#endif

	if ( 0 == SDL_WasInit( SDL_INIT_VIDEO ) ) {
		if ( !SDL_Init( SDL_INIT_VIDEO ) ) {
			ri.Error( ERR_FATAL, "SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError() );
		}
	}

	if ( 0 != VKimp_SetMode( r_mode->integer, r_fullscreen->integer ) ) {
		ri.Printf( PRINT_ALL, " Setting r_mode=%d, r_fullscreen=%d failed, falling back to windowed 640x480\n",
				   r_mode->integer, r_fullscreen->integer );

		if ( 0 != VKimp_SetMode( 3, qfalse ) ) {
			ri.Error( ERR_FATAL, "vk_createWindow() - could not create a window: %s", SDL_GetError() );
		}
	}

	if ( icon != NULL ) {
		SDL_SetWindowIcon( window_sdl, icon );
		SDL_DestroySurface( icon );
	}

	ri.IN_Init( window_sdl );
}

void vk_destroyWindow( void ) {
	ri.Printf( PRINT_ALL, " Destroy Window Subsystem.\n" );

	ri.IN_Shutdown();
	SDL_QuitSubSystem( SDL_INIT_VIDEO );

	SDL_DestroyWindow( window_sdl );
	window_sdl = NULL;
}

void vk_getInstanceProcAddrImpl( void ) {
	ri.Printf( PRINT_ALL, " *** Vulkan Initialization ***\n" );

	if ( !SDL_Vulkan_LoadLibrary( NULL ) ) {
		ri.Error( ERR_FATAL, "Failed to load Vulkan library: %s", SDL_GetError() );
	}

	qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) SDL_Vulkan_GetVkGetInstanceProcAddr();
	if ( qvkGetInstanceProcAddr == NULL ) {
		ri.Error( ERR_FATAL, "Failed to find entrypoint vkGetInstanceProcAddr" );
	}

	ri.Printf( PRINT_ALL, " Got instance proc address (SDL3).\n" );
}

void vk_createSurfaceImpl( void ) {
	ri.Printf( PRINT_ALL, " Create Surface: vk.surface.\n" );

	if ( !SDL_Vulkan_CreateSurface( window_sdl, vk.instance, NULL, &vk.surface ) ) {
		vk.surface = VK_NULL_HANDLE;
		ri.Error( ERR_FATAL, "SDL_Vulkan_CreateSurface(): %s", SDL_GetError() );
	}
}

void vk_minimizeWindow( void ) {
	qboolean isFullscreen = ( SDL_GetWindowFlags( window_sdl ) & SDL_WINDOW_FULLSCREEN ) != 0;

	if ( isFullscreen ) {
		SDL_SetWindowFullscreen( window_sdl, false );
	}

	SDL_MinimizeWindow( window_sdl );
}
