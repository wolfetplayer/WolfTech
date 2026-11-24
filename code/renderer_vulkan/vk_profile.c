// vk_profile.c -- opt-in CPU/GPU timing for the Vulkan renderer's major per-frame passes, toggled
// with r_vk_profile. GPU timestamps reuse vk_flares.c's established trick: write this frame's
// queries into this frame slot's region, then read back whatever was written into that same
// region VK_FRAMES_IN_FLIGHT frames ago -- vk_begin_frame's fence wait for this slot already
// guarantees that GPU work is done, so the readback never blocks.
//
// Known limitation: a mirror/portal recursion (tr_portal.c) can run R_RenderViewInternal -- and so
// some of these zones -- twice in one real frame. Each zone has room for exactly one begin/end
// query pair per frame slot, so in that case only the *last* call's GPU duration survives (the
// first is overwritten before ever being read back); CPU time, which is summed rather than
// overwritten, is unaffected. Rare content (actual mirrors/portals), not worth doubling the query
// pool and the bookkeeping to fix.

#ifdef USE_LOCAL_HEADERS
#	include "SDL3/SDL.h"
#else
#	include <SDL3/SDL.h>
#endif

#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_frame.h"
#include "vk_profile.h"

#define GPU_QUERIES_PER_ZONE 2 // begin, end
#define GPU_QUERY_COUNT ( VKPROF_ZONE_COUNT * GPU_QUERIES_PER_ZONE * VK_FRAMES_IN_FLIGHT )
#define PROFILE_PRINT_INTERVAL_FRAMES 60

static const char *s_zoneNames[VKPROF_ZONE_COUNT] = {
	"sky", "world", "foliage", "entities", "entity-bones", "entity-lerp",
	"transparent", "shadows", "poly", "flares"
};

static cvar_t *r_vk_profile;

static Uint64 s_perfFreq;

// CPU side: wall-clock accumulation, summed across every zone_begin/zone_end pair seen this
// interval (including repeats from portal recursion -- unlike the GPU side, nothing is lost here).
static Uint64 s_cpuZoneStartTicks[VKPROF_ZONE_COUNT];
static double s_cpuZoneAccumMs[VKPROF_ZONE_COUNT];
static Uint64 s_frameStartTicks;
static double s_frameCpuAccumMs;

// GPU side.
static VkQueryPool s_queryPool;
static qboolean s_queryPoolReady;
static qboolean s_gpuTimingSupported;
static double s_timestampPeriodNs; // vk.deviceProperties.limits.timestampPeriod; ns per tick
static double s_gpuZoneAccumMs[VKPROF_ZONE_COUNT];
// Whether zone_begin/zone_end wrote into the currently-recording frame slot's queries this frame --
// read back (and then cleared) the next time that same slot comes around.
static qboolean s_zoneRecorded[VK_FRAMES_IN_FLIGHT][VKPROF_ZONE_COUNT];
static qboolean s_slotEverUsed[VK_FRAMES_IN_FLIGHT];
static uint32_t s_currentSlot;

static int s_intervalFrameCount;

void vk_profile_init( void ) {
	uint32_t queueFamilyCount;
	VkQueueFamilyProperties queueFamilies[32];
	VkQueryPoolCreateInfo poolDesc;

	r_vk_profile = ri.Cvar_Get( "r_vk_profile", "0", 0 ); // not CVAR_ARCHIVE -- a diagnostic toggle, not a user setting

	s_perfFreq = SDL_GetPerformanceFrequency();

	memset( s_cpuZoneAccumMs, 0, sizeof( s_cpuZoneAccumMs ) );
	memset( s_gpuZoneAccumMs, 0, sizeof( s_gpuZoneAccumMs ) );
	memset( s_zoneRecorded, 0, sizeof( s_zoneRecorded ) );
	memset( s_slotEverUsed, 0, sizeof( s_slotEverUsed ) );
	s_frameCpuAccumMs = 0.0;
	s_intervalFrameCount = 0;

	s_gpuTimingSupported = qfalse;
	if ( vk.deviceProperties.limits.timestampPeriod > 0.0f ) {
		queueFamilyCount = 0;
		qvkGetPhysicalDeviceQueueFamilyProperties( vk.physical_device, &queueFamilyCount, NULL );
		if ( queueFamilyCount > 0 && queueFamilyCount <= ARRAY_LEN( queueFamilies ) ) {
			qvkGetPhysicalDeviceQueueFamilyProperties( vk.physical_device, &queueFamilyCount, queueFamilies );
			if ( vk.queue_family_index < queueFamilyCount &&
				 queueFamilies[vk.queue_family_index].timestampValidBits > 0 ) {
				s_gpuTimingSupported = qtrue;
				s_timestampPeriodNs = (double)vk.deviceProperties.limits.timestampPeriod;
			}
		}
	}

	if ( s_gpuTimingSupported ) {
		memset( &poolDesc, 0, sizeof( poolDesc ) );
		poolDesc.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		poolDesc.queryType = VK_QUERY_TYPE_TIMESTAMP;
		poolDesc.queryCount = GPU_QUERY_COUNT;
		VK_CHECK( qvkCreateQueryPool( vk.device, &poolDesc, NULL, &s_queryPool ) );
		s_queryPoolReady = qtrue;
	} else {
		ri.Printf( PRINT_ALL, "vk_profile: device lacks usable timestamp queries -- r_vk_profile will only report CPU times\n" );
	}
}

void vk_profile_shutdown( void ) {
	if ( s_queryPoolReady ) {
		qvkDestroyQueryPool( vk.device, s_queryPool, NULL );
		s_queryPoolReady = qfalse;
	}
}

static void ReadbackSlot( uint32_t slot ) {
	uint64_t results[VKPROF_ZONE_COUNT * GPU_QUERIES_PER_ZONE];
	uint32_t base;
	int i;
	VkResult result;

	if ( !s_gpuTimingSupported || !s_slotEverUsed[slot] ) {
		return;
	}

	base = slot * VKPROF_ZONE_COUNT * GPU_QUERIES_PER_ZONE;
	result = qvkGetQueryPoolResults( vk.device, s_queryPool, base, VKPROF_ZONE_COUNT * GPU_QUERIES_PER_ZONE,
		sizeof( results ), results, sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT );
	if ( result != VK_SUCCESS ) {
		return; // shouldn't happen -- vk_begin_frame's fence wait already guarantees this slot's GPU work is done
	}

	for ( i = 0; i < VKPROF_ZONE_COUNT; i++ ) {
		uint64_t beginTicks, endTicks;

		if ( !s_zoneRecorded[slot][i] ) {
			continue; // this zone didn't run the last time this slot recorded -- nothing valid to read
		}
		beginTicks = results[i * GPU_QUERIES_PER_ZONE + 0];
		endTicks = results[i * GPU_QUERIES_PER_ZONE + 1];
		if ( endTicks > beginTicks ) {
			s_gpuZoneAccumMs[i] += (double)( endTicks - beginTicks ) * s_timestampPeriodNs * 1e-6;
		}
	}
}

static void ResetInterval( void ) {
	memset( s_cpuZoneAccumMs, 0, sizeof( s_cpuZoneAccumMs ) );
	memset( s_gpuZoneAccumMs, 0, sizeof( s_gpuZoneAccumMs ) );
	s_frameCpuAccumMs = 0.0;
	s_intervalFrameCount = 0;
}

static void PrintInterval( void ) {
	int i;

	ri.Printf( PRINT_ALL, "vk_profile: cpu/gpu ms, avg over %d frames --\n", s_intervalFrameCount );
	for ( i = 0; i < VKPROF_ZONE_COUNT; i++ ) {
		double cpuAvg = s_cpuZoneAccumMs[i] / s_intervalFrameCount;
		if ( s_gpuTimingSupported ) {
			double gpuAvg = s_gpuZoneAccumMs[i] / s_intervalFrameCount;
			ri.Printf( PRINT_ALL, "  %-12s cpu %6.3f  gpu %6.3f\n", s_zoneNames[i], cpuAvg, gpuAvg );
		} else {
			ri.Printf( PRINT_ALL, "  %-12s cpu %6.3f  gpu n/a\n", s_zoneNames[i], cpuAvg );
		}
	}
	ri.Printf( PRINT_ALL, "  %-12s cpu %6.3f\n", "frame-total", s_frameCpuAccumMs / s_intervalFrameCount );
}

void vk_profile_frame_begin( void ) {
	s_currentSlot = vk_current_frame_slot();

	ReadbackSlot( s_currentSlot );

	memset( s_zoneRecorded[s_currentSlot], 0, sizeof( s_zoneRecorded[s_currentSlot] ) );
	if ( s_gpuTimingSupported ) {
		qvkCmdResetQueryPool( vk.command_buffer, s_queryPool,
			s_currentSlot * VKPROF_ZONE_COUNT * GPU_QUERIES_PER_ZONE, VKPROF_ZONE_COUNT * GPU_QUERIES_PER_ZONE );
	}
	s_slotEverUsed[s_currentSlot] = qtrue;

	s_frameStartTicks = SDL_GetPerformanceCounter();
}

void vk_profile_frame_end( void ) {
	Uint64 now = SDL_GetPerformanceCounter();
	s_frameCpuAccumMs += (double)( now - s_frameStartTicks ) * 1000.0 / (double)s_perfFreq;
	s_intervalFrameCount++;

	if ( s_intervalFrameCount >= PROFILE_PRINT_INTERVAL_FRAMES ) {
		if ( r_vk_profile->integer ) {
			PrintInterval();
		}
		ResetInterval();
	}
}

void vk_profile_zone_begin( vkProfZone_t zone ) {
	if ( !r_vk_profile->integer ) {
		return; // some zones (entity-bones/entity-lerp) run per-surface -- must cost nothing when off
	}

	s_cpuZoneStartTicks[zone] = SDL_GetPerformanceCounter();

	if ( s_gpuTimingSupported ) {
		uint32_t idx = s_currentSlot * VKPROF_ZONE_COUNT * GPU_QUERIES_PER_ZONE + (uint32_t)zone * GPU_QUERIES_PER_ZONE;
		qvkCmdWriteTimestamp( vk.command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s_queryPool, idx );
		s_zoneRecorded[s_currentSlot][zone] = qtrue;
	}
}

void vk_profile_zone_end( vkProfZone_t zone ) {
	Uint64 now;

	if ( !r_vk_profile->integer ) {
		return;
	}

	now = SDL_GetPerformanceCounter();
	s_cpuZoneAccumMs[zone] += (double)( now - s_cpuZoneStartTicks[zone] ) * 1000.0 / (double)s_perfFreq;

	if ( s_gpuTimingSupported ) {
		uint32_t idx = s_currentSlot * VKPROF_ZONE_COUNT * GPU_QUERIES_PER_ZONE + (uint32_t)zone * GPU_QUERIES_PER_ZONE + 1;
		qvkCmdWriteTimestamp( vk.command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_queryPool, idx );
	}
}

// CPU-only -- see vk_profile.h. Deliberately never touches the GPU query pool/s_zoneRecorded:
// these can run many times per frame for the same zone, and a GPU query must be reset between
// writes, so reusing the single-pair-per-zone-per-slot scheme above here would be invalid Vulkan
// usage, not just imprecise.
void vk_profile_cpu_zone_begin( vkProfZone_t zone ) {
	if ( !r_vk_profile->integer ) {
		return;
	}
	s_cpuZoneStartTicks[zone] = SDL_GetPerformanceCounter();
}

void vk_profile_cpu_zone_end( vkProfZone_t zone ) {
	Uint64 now;

	if ( !r_vk_profile->integer ) {
		return;
	}
	now = SDL_GetPerformanceCounter();
	s_cpuZoneAccumMs[zone] += (double)( now - s_cpuZoneStartTicks[zone] ) * 1000.0 / (double)s_perfFreq;
}
