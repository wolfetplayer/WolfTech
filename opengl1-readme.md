# OpenGL1
<insert ascii art here>

OpenGL1 (`code/renderer`) is the original/legacy renderer for iortcw, kept
around for compatibility and performance on older hardware.  This file only
covers what's been added on top of the base iortcw feature set -- see
rend2-readme.md for the alternate, more feature-heavy renderer.


-------------------------------------------------------------------------------
  FEATURES
-------------------------------------------------------------------------------

  - Offscreen FBO rendering pipeline with a gamma-correct final blit.
  - ARB bloom.
  - HDR rendering.
  - MSAA and SSAA, both routed through the FBO pipeline.
  - Merged lightmap atlases.
  - Clip-brush collision debug view.


-------------------------------------------------------------------------------
  CVARS
-------------------------------------------------------------------------------

Cvars for the FBO pipeline (requires vid_restart to toggle):

*  `r_fbo`                           - Route rendering through an offscreen
                                   framebuffer instead of drawing straight to
                                   the backbuffer, with a gamma-correct final
                                   blit.  Required by r_hdr, r_ext_supersample,
                                   r_bloom, and MSAA-in-FBO below.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_hdr`                           - Render the FBO's scene buffer in a
                                   high dynamic range format.  Requires r_fbo.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_ext_supersample`               - Render at a higher internal resolution
                                   and downsample on the final blit.
                                   Requires r_fbo.  Expensive.
                                     0 - No. (default)
                                     1 - 2x supersample.

*  `r_ext_multisample`               - Multisample anti-aliasing.  With
                                   `r_fbo 1`, multisamples the FBO's own
                                   color target instead of the default
                                   framebuffer.
                                     0    - None. (default)
                                     1-16 - Some, clamped to what the GPU
                                            driver reports as GL_MAX_SAMPLES.

Cvars for bloom (requires r_fbo and r_bloom):

*  `r_bloom`                         - Enable ARB bloom on the FBO pipeline.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_bloom_threshold`               - Brightness a pixel needs to exceed
                                   before it contributes to the bloom.
                                     0.6 - Default.

*  `r_bloom_intensity`               - Strength of the bloom glow once
                                   applied back onto the scene.
                                     0.5 - Default.

*  `r_bloom_passes`                  - Number of blur passes applied to the
                                   bloom buffer.  More passes, softer glow,
                                   more cost.
                                     2 - Default.

Cvars for world rendering:

*  `r_mergeLightmaps`                - Merge the small per-surface lightmaps
                                   into a handful of shared atlas textures
                                   instead of one GL texture per lightmap.
                                   Easy speedup on lightmap-heavy maps.
                                     0 - Don't. (default)
                                     1 - Do.

Cvars for debugging collision (shared with rend2, see rend2-readme.md):

*  `r_drawClips`                     - Cheat.  Draw clip brush (playerclip/
                                   monsterclip) polygons.
                                     0 - No. (default)
                                     1 - Playerclip only, see through walls.
                                     2 - All clip types, see through walls.
                                     3 - Playerclip only, depth-tested.
                                     4 - All clip types, depth-tested.


-------------------------------------------------------------------------------
  ENGINE (not renderer-specific, listed here for lack of a better home)
-------------------------------------------------------------------------------

*  `cl_perfGraph`                    - Draw a real-time performance graph
                                   (frame time / fps) overlay.
                                     0 - No. (default)
                                     1 - Yes.

*  `cl_perfGraphW`                   - Width of the graph, in pixels.
                                     300 - Default.

*  `cl_perfGraphH`                   - Height of the graph, in pixels.
                                     80 - Default.

*  `cl_perfGraphX` / `cl_perfGraphY` - Position of the graph, in pixels.
                                     332 / 475 - Default.

*  `cl_perfGraphAlpha`               - Opacity of the graph background.
                                     0.8 - Default.

Frame pacing: the `com_maxfps` wait loop now exits on a microsecond-precise
clock (`Sys_Microseconds`) instead of the old whole-millisecond one, on both
Windows and Unix.  Reduces frame-time jitter when an fps cap is set.  No new
cvar -- same `com_maxfps` as before.
