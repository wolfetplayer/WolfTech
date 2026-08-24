# Rend2
<insert ascii art here>

Rend2 is an alternate renderer for iortcw.  It aims to implement modern
features and technologies into the id tech 3 engine, but without sacrificing
compatibility with existing RTCW mods.


-------------------------------------------------------------------------------
  FEATURES
-------------------------------------------------------------------------------

  - Compatible with most vanilla RTCW mods.
  - HDR Rendering, and support for HDR lightmaps
  - Tone mapping and auto-exposure.
  - Cascaded shadow maps.
  - Multisample anti-aliasing.
  - Texture upsampling.
  - Advanced materials support.
  - Advanced shading and specular methods.
  - RGTC and BPTC texture compression support.
  - Screen-space ambient occlusion.
  - FXAA.
  - Multi-scale bloom.
  - Sun and dynamic light rays.
  - Color grading via LUT textures.
  - Vignette, film grain, and chromatic aberration.
  - Static world VBOs with leaf-surface merging.
  - Merged lightmap atlases.


-------------------------------------------------------------------------------
  INSTALLATION
-------------------------------------------------------------------------------

For *nix:

1. This should be identical to installing iortcw.  Check their README for more
   details.

   
For Win32:

1. Have a RTCW install, fully patched.

2. Copy the following files into RTCW's install directory: 
     
     ioWolfMP.x86.exe
     ioWolfSP.x86.exe
     ioWolfDED.x86.exe
     renderer_mp_opengl1_x86.dll
     renderer_mp_rend2_x86.dll
     renderer_sp_opengl1_x86.dll
     renderer_sp_rend2_x86.dll
     
   These can be found in build/release-mingw32-x86 after compiling, or bug
   someone to release binaries.
  

-------------------------------------------------------------------------------
  RUNNING
-------------------------------------------------------------------------------

1. Start iowolfmp/iowolfsp. (ioWolfMP.x86.exe/ioWolfSP.x86.exe on Win32)
 
2. Open the console (the default key is tilde ~) and type 
`/cl_renderer rend2` and press enter
`/vid_restart` then press enter again.

3. Enjoy.


-------------------------------------------------------------------------------
  CVARS
-------------------------------------------------------------------------------

Cvars for simple rendering features:

* `r_ext_compressed_textures`       - Automatically compress textures.
                                     0 - No texture compression. (default)
                                     1 - DXT/RGTC texture compression if
                                         supported.
                                     2 - BPTC texture compression if supported.

*  `r_ext_framebuffer_multisample`  - Multisample Anti-aliasing.
                                     0    - None. (default)
                                     1-16 - Some.
                                     17+  - Too much!

*  `r_fxaa`                         - Fast approximate anti-aliasing, a cheap
                                   post-process edge smooth.  Can be used
                                   together with MSAA.
                                     0 - No.
                                     1 - Yes. (default)

*  `r_ssao`                         - Enable screen-space ambient occlusion.
                                   Runs at quarter resolution with a
                                   depth-aware blur, but still a real cost;
                                   requires r_hdr and r_depthPrepass (the
                                   options menu enables both automatically).
                                   Requires vid_restart to toggle.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_ssaoIntensity`                 - Strength of the occlusion darkening.
                                   Hot-tunable, no vid_restart needed.
                                     0.0  - No visible effect.
                                     0.85 - Default.
                                     1.0  - Full strength.

*  `r_ssaoRadius`                    - Sample spread distance.  Larger values
                                   pick up occlusion from farther away, at
                                   the cost of looking less precise up close.
                                     1024 - Default.

Cvars for HDR and tonemapping:

 * `r_hdr`                          - Do scene rendering in a framebuffer with
                                   high dynamic range.  (Less banding, and
                                   exposure changes look much better, at a
                                   real bandwidth/performance cost)
                                     0 - No. (default)
                                     1 - Yes.

*  `r_cameraExposure`               - Cheat.  Alter brightness, in powers of two.
                                     -2  - 4x as dark.
                                     0   - Normal. (default)
                                     0.5 - Sqrt(2)x as bright.
                                     2   - 4x as bright.

*  `r_postProcess`                  - Enable post-processing.
                                     0 - No.
                                     1 - Yes. (default)

*  `r_greyscale`                    - Desaturate the final image, including HUD/UI.
                                     0    - No. (default)
                                     0-1  - Partial desaturation.
                                     1    - Full greyscale.
                                     2    - Selective-color grade: greys out
                                          everything except reds (flags, blood, etc).

*  `r_toneMap`                      - Enable tone mapping.  Requires 
                                   r_hdr and r_postProcess.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_forceToneMap`                 - Cheat. Override built-in and map tonemap settings and use cvars r_forceToneMapAvg, r_forceToneMapMin, and r_forceToneMapMax.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_forceToneMapAvg`              - Cheat.  Map average scene luminance to this
                                   value, in powers of two.  Requires 
                                   r_forceToneMap.
                                    -2.0 - Dark.
                                    -1.0 - Kinda dark. (default).
                                     2.0 - Too bright.

*  `r_forceToneMapMin`              - Cheat.  After mapping average, luminance
                                   below this level is mapped to black.
                                   Requires r_forceToneMap.
                                    -5    - Not noticeable.
                                    -3.25 - Normal. (default)
                                     0.0  - Too dark.

*  `r_forceToneMapMin`              - Cheat.  After mapping average, luminance
                                   above this level is mapped to white.
                                   Requires r_forceToneMap.
                                    0.0 - Too bright.
                                    1.0 - Normal. (default).
                                    2.0 - Washed out.

*  `r_autoExposure`                 - Do automatic exposure based on scene
                                   brightness.  Hardcoded to -2 to 2 on maps
                                   that don't specify otherwise.  Requires
                                   r_hdr, r_postprocess, and r_toneMap.
                                   Adaptation speed is framerate-independent,
                                   so exposure ramps at the same real-world
                                   rate regardless of fps.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_forceAutoExposure`            - Cheat.  Override built-in and map auto
                                   exposure settings and use cvars
                                   r_forceAutoExposureMin and 
                                   r_forceAutoExposureMax.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_forceAutoExposureMin`         - Cheat.  Set minimum exposure to this value,
                                   in powers of two.  Requires
                                   r_forceAutoExpsure.
                                    -3.0 - Dimmer.
                                    -2.0 - Normal. (default)
                                    -1.0 - Brighter.

*  `r_forceAutoExposureMax`         - Cheat.  Set maximum exposure to this value,
                                   in powers of two.  Requires
                                   r_forceAutoExpsure.
                                     1.0 - Dimmer.
                                     2.0 - Normal. (default)
                                     3.0 - Brighter.

Cvars for bloom:

*  `r_bloom`                         - Enable bloom.  Bright areas of the
                                   scene bleed a soft glow onto their
                                   surroundings.  Multi-scale: downsamples
                                   through a mip chain and upsamples back,
                                   for a softer, more natural glow than a
                                   single-pass blur.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_bloomThreshold`                - Brightness a pixel needs to exceed
                                   before it contributes to the bloom.
                                     1.0 - Default.

*  `r_bloomIntensity`                - Strength of the bloom glow once
                                   applied back onto the scene.
                                     0.5 - Default.

*  `r_bloomDebug`                    - Cheat.  Show the isolated bloom
                                   bright-pass in the top-left corner,
                                   for tuning the threshold.
                                     0 - No. (default)
                                     1 - Yes.

Cvars for advanced material usage:

*  `r_normalMapping`                - Enable normal maps for materials that
                                   support it.
                                     0 - No.
                                     1 - Yes. (default)

*  `r_specularMapping`              - Enable specular maps for materials that
                                   support it.
                                     0 - No.
                                     1 - Yes. (default)

*  `r_deluxeMapping`                - Enable deluxe mapping.  (Map is compiled
                                   with light directions.)  Even if the map 
                                   doesn't have deluxe mapping compiled in,
                                   an approximation based on the lightgrid
                                   will be used.
                                     0 - No.
                                     1 - Yes. (default)

*  `r_parallaxMapping`              - Enable parallax mapping for materials that
                                   support it.
                                     0 - No. (default)
                                     1 - Use parallax occlusion mapping.
                                     2 - Use relief mapping. (slower)

*  `r_parallaxMapOffset`            - Set the parallax height offset.
                                     0    - Values map to -255 - 0. (default)
                                     0.5  - Values map to -127 - 127.
                                     1.0  - Values map to 0 - 255.

*  `r_baseSpecular`                 - Set the specular reflectance of materials
                                   which don't include a specular map or
                                   use the specularReflectance keyword.
                                     0    - No.
                                     0.04 - Realistic. (default)
                                     1.0  - Ack.

*  `r_baseGloss`                    - Set the glossiness of materials which don't
                                   include a specular map or use the
                                   specularExponent keyword.
                                     0   - Rough.
                                     0.3 - Default.
                                     1.0 - Shiny.

*  `r_baseNormalX`                  - Set the scale of the X values from normal
                                   maps when the normalScale keyword is not
                                   used.
                                     -1  - Flip X.
                                     0   - Ignore X.
                                     1   - Normal X. (default)
                                     2   - Double X.

*  `r_baseNormalY`                  - Set the scale of the Y values from normal
                                   maps when the normalScale keyword is not
                                   used.
                                     -1  - Flip Y.
                                     0   - Ignore Y.
                                     1   - Normal Y. (default)
                                     2   - Double Y.

*  `r_baseParallax`                 - Sets the scale of the parallax effect for
                                   materials when the parallaxDepth keyword
                                   is not used.
                                     0    - No depth.
                                     0.01 - Pretty smooth.
                                     0.05 - Standard depth. (default)
                                     0.1  - Looks broken.

*  `r_pbr`                          - Enable physically based rendering.
                                   Experimental, will not look correct without
                                   assets meant for it.
                                     0 - No. (default)
                                     1 - Yes.

Cvars for image interpolation and generation:

*  `r_imageUpsample`                - Use interpolation to artifically increase
                                   the resolution of all textures.  Looks good
                                   in certain circumstances.
                                     0 - No. (default)
                                     1 - 2x size.
                                     2 - 4x size.
                                     3 - 8x size, etc

*  `r_imageUpsampleMaxSize`         - Maximum texture size when upsampling
                                   textures.
                                     1024 - Default.
                                     2048 - Really nice.
                                     4096 - Really slow.
                                     8192 - Crash.

*  `r_imageUpsampleType`            - Type of interpolation when upsampling
                                   textures.
                                     0 - None. (probably broken)
                                     1 - Bad but fast (default,
                                         FCBI without second derivatives)
                                     2 - Okay but slow (normal FCBI)

*  `r_genNormalMaps`                - Naively generate normal maps for all
                                   textures.
                                     0 - Don't. (default)
                                     1 - Do.

Cvars for the sunlight and cascaded shadow maps:

*  `r_forceSun`                     - Force sunlight and shadows, using sun position from sky material.
                                     0 - Don't. (default)
                                     1 - Do.
                                     2 - Sunrise, sunset.

*  `r_forceSunLightScale`           - Cheat. Scale sun brightness by this factor
                                   when r_forceSun 1.
                                     1.0 - Default

*  `r_forceSunAmbientScale`         - Cheat. Scale sun ambient brightness by this factor when r_forceSun 1. 0.5 - Default

*  `r_sunShadows`                   - Enable sunlight and cascaded shadow maps for
                                   it on maps that support it.  One of the more
                                   expensive rend2 features.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_sunlightMode`                 - Specify the method used to add sunlight to
                                   the scene.
                                     0 - No.
                                     1 - Multiply lit areas by light scale, and
                                         shadowed areas by ambient scale.
                                         (default)
                                     2 - Add light.  Looks better, but is slower
                                         and doesn't integrate well with existing
                                         maps.

*  `r_shadowFilter`                 - Enable filtering shadows for a smoother
                                   look.
                                     0 - No.
                                     1 - Some. (default)
                                     2 - Much.

*  `r_shadowMapSize`                - Size of each cascaded shadow map.
                                     256  - 256x256, ugly, probably shouldn't
                                             go below this.
                                     512  - 512x512, passable.
                                     1024 - 1024x1024, good. (default)
                                     2048 - 2048x2048, extreme.
                                     4096 - 4096x4096, indistinguishable from
                                            2048.

Cvars for light rays (requires vid_restart to toggle):

*  `r_drawSunRays`                   - Radial light shafts from the sun.
                                   Only visible when the sun is roughly
                                   facing the camera; requires a sky shader
                                   with q3gl2_sun (see DYNAMIC SUNLIGHT above).
                                     0 - No. (default)
                                     1 - Yes.

*  `r_dlightRays`                    - Radial light shafts from the current
                                   "primary" dynamic light (explosions,
                                   muzzle flashes, etc).
                                     0 - No. (default)
                                     1 - Yes.

*  `r_dlightRaysMinRadius`           - Minimum dlight radius before it's
                                   eligible to cast rays.  Keeps small,
                                   unimportant lights from flaring up.
                                     200 - Default.

*  `r_dlightRaysIntensity`           - Strength of dlight-cast rays.
                                     1.0 - Default.

*  `r_dlightRaysMuzzleflashIntensity` - Strength of dlight rays specifically
                                   from muzzle flashes.
                                     1.0 - Default.

Cvars for color grading:

*  `r_colorGrading`                  - Enable color grading via a LUT texture.
                                   See the COLOR GRADING section below.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_colorGradeLUT`                 - Path to the color grading LUT to use,
                                   no extension needed.  Hot-reloads when
                                   changed -- no vid_restart required.
                                     gfx/luts/neutral - No-op grade. (default)

Cvars for screen effects (all hot-toggleable, no vid_restart needed):

*  `r_vignette`                      - Darken the screen edges/corners.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_vignetteIntensity`             - Strength of the vignette darkening.
                                     0.0 - None.
                                     0.4 - Default.
                                     1.0 - Corners go black.

*  `r_filmGrain`                     - Add animated per-pixel grain/noise.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_filmGrainIntensity`            - Strength of the grain.
                                     0.05 - Default, subtle.
                                     0.2  - Heavy, visibly noisy.

*  `r_chromaticAberration`           - Fringe color channels apart toward
                                   the screen edges, like an imperfect lens.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_chromaticAberrationIntensity`  - Strength of the color fringing.
                                     0.002 - Default, subtle.
                                     0.01  - Strong, edges clearly split.

Cvars that you probably don't care about or shouldn't mess with:

*  `r_depthPrepass`                 - Do a depth-only pass before rendering.
                                   Speeds up rendering in cases where advanced
                                   features are used.  Required for
                                   r_sunShadows and r_ssao; not worth enabling
                                   unless one of those is on.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_mergeLightmaps`               - Merge the small (128x128) lightmaps into 
                                   2 or fewer giant (4096x4096) lightmaps.
                                   Easy speedup.
                                     0 - Don't.
                                     1 - Do. (default)

*  `r_mergeLeafSurfaces`            - Merge static world surfaces that share a
                                   leaf and shader into fewer draw calls, and
                                   upload the world to static VBOs.  Cuts
                                   draw call count on detailed maps.
                                   Requires vid_restart.
                                     0 - No. (default)
                                     1 - Yes.

*  `r_foliageMinLight`              - Minimum ambient light floor applied to
                                   foliage instances, so grass/scatter models
                                   don't go fully black in shadow.
                                     0.0 - No floor.
                                     0.6 - Default.
                                     1.0 - Always fully lit.

*  `r_shadowCascadeZNear`           - Near plane for shadow cascade frustums.
                                     4 - Default.

*  `r_shadowCascadeZFar`            - Far plane for shadow cascade frustums.
                                     3072 - Default.

*  `r_shadowCascadeZBias`           - Z-bias for shadow cascade frustums.
                                     -256 - Default.

Cvars that have broken bits:

*  `r_dlightMode`                   - Change how dynamic lights look.
                                     0 - RTCW style dlights, fake
                                         brightening. (default)
                                     1 - Actual lighting, no shadows.
                                     2 - Light and shadows. (broken)

*  `r_pshadowDist`                  - Virtual camera distance when creating shadowmaps for projected shadows.  Deprecated.

*  `cg_shadows`                     - Old shadow code.  Deprecated.

Cvars for debugging collision (also available in the opengl1 renderer, see
opengl1-readme.md):

*  `r_drawClips`                     - Cheat.  Draw clip brush (playerclip/
                                   monsterclip) polygons.
                                     0 - No. (default)
                                     1 - Playerclip only, see through walls.
                                     2 - All clip types, see through walls.
                                     3 - Playerclip only, depth-tested.
                                     4 - All clip types, depth-tested.


-------------------------------------------------------------------------------
  MATERIALS
-------------------------------------------------------------------------------

Rend2 supports .mtr files, which are basically the same as .shader files, and
are located in the same place, but override existing .shader files if they 
exist.  This is to allow maps and mods to use the new material features without
breaking the map when using the old renderer.

Here's an example of a material stored in one, showing off some new features:

    textures/abandon/grass
    {
        qer_editorimage textures/abandon/grass.jpg
        {
            map textures/abandon/grass3_256_d.jpg
            rgbgen identity
        }
        {
            stage normalparallaxmap
            map textures/abandon/grass3_1024_n.png
            normalScale 1 1
            parallaxDepth 0.05
        }
        {
            stage specularmap
            map textures/abandon/grass3_256_s.png
            specularReflectance 0.12
            specularExponent 16
        }
        {
            map $lightmap
            blendfunc GL_DST_COLOR GL_ZERO
        }
    }

The first thing to notice is that this is basically the same as old RTCW 
shader files.  The next thing to notice are the new keywords.  Here is what 
they mean:

  `stage <type>`
    - State how this imagemap will be used by Rend2:
        diffuseMap        - Standard, same as no stage entry
        normalMap         - Image will be used as a normal map
        normalParallaxMap - Image will be used as a normal map with 
                            alpha treated as height for parallax mapping
        specularMap       - Image will be used as a specular map with
                            alpha treated as shininess.

  `specularReflectance <value>`
    - State how metallic this material is.  Metals typically have a high 
      specular and a low diffuse, so this is typically high for them, and low
      for other materials, such as plastic.  For typical values for various
      materials, see http://refractiveindex.info , pick a material, then scroll
      down to the reflection calculator and look up its reflectance.  Default
      is 0.04, since most materials aren't metallic.
  
  `specularExponent <value>`
    - State how shiny this material is.  Note that this is modulated by the 
      alpha channel of the specular map, so if it were set to 16, and the alpha
      channel of the specular map was set to 0.5, then the shininess would be
      set to 8.  Default 256.

  `normalScale <x> <y>`
    - State the X and Y scales of the normal map.  This is useful for increasing
      or decreasing the "strength" of the normal map, or entering negative values
      to flip the X and/or Y values.  Default 1 1.

  `parallaxDepth <value>`
    - State the maximum depth of the parallax map.  This is a fairly sensitive
      value, and I recommend the default or lower.  Default 0.05.

An important note is that normal and specular maps influence the diffuse map
declared before them, so materials like this are possible:

    textures/terrain/grass
    {
        qer_editorimage textures/terrain/grass.jpg

        {
            map textures/terrain/rock.jpg
        }
        {
            stage normalparallaxmap
            map textures/terrain/rock_n.png
        }
        {
            stage specularmap
            map textures/terrain/rock_s.jpg
        }
        {
            map textures/terrain/grass.jpg
            blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
            alphaGen vertex
        }
        {
            stage normalparallaxmap
            map textures/terrain/grass_n.png
        }
        {
            stage specularmap
            map textures/terrain/grass_s.png
            specularReflectance 0.12
        }
        {
            map $lightmap
            blendfunc GL_DST_COLOR GL_ZERO
        }
    }
    
Though note due to the complexity of lighting, dynamic light (including
sunlight with cascaded shadow maps) currently only works 100% on materials like
this, where the second diffuse map doesn't have its own alpha, and only
uses vertex alpha.  YMMV.

Another addition to materials is working normal/specular maps on vertex lit
surfaces.  To enable this, make your material look like this:

    textures/vehicles/car
    {
        qer_editorimage textures/vehicles/car.jpg

        {
            map textures/vehicles/car.jpg
            rgbGen vertexLit
        }
        {
            stage normalparallaxmap
            map textures/vehicles/car_n.jpg
        }
        {
            stage specularmap
            map textures/vehicles/car_s.jpg
        }
    }

Note the new keyword, 'vertexLit' after rgbGen.  This is analogous to 
'rgbGen vertex', except a light direction will be determined from the lightgrid
and used with the normal and specular maps.  'exactVertexLit' exists as well,
and is the equivalent for 'exactVertex'.


-------------------------------------------------------------------------------
  DYNAMIC SUNLIGHT AND CASCADED SHADOW MAPS
-------------------------------------------------------------------------------

This adds a new keyword to sky materials, q3gl2_sun.  The syntax is:

    q3gl2_sun <red> <green> <blue> <intensity> <degrees> <elevation> <ambientLightScale>
  
Note the first six parameters are the same as in q3map_sun or q3map_sunExt,
and the last two indicate scaling factors for the map brightness and an ambient
light of the same color as the sun.

There are currently two ways to use this in your own (and other people's) maps.

  1. Create your map as normal, set r_sunlightMode to 1, and add a 
     'q3gl2_sun' line after your 'q3map_sun' line in your sky material, like
     so:
     
        textures/skies/bluesky
        {
          qer_editorimage textures/skies/bluesky.jpg

          surfaceparm nomarks
          surfaceparm noimpact
          surfaceparm nolightmap
          surfaceparm sky
          q3map_sunExt 240 238 200 100 195 35 3 16
          q3gl2_sun 240 238 200 50 195 35 0.2
          q3map_skylight 50 16
          q3map_lightimage $whiteimage

          skyparms env/bluesky - -
        }

     The advantages with this method are that your map will continue to work
     with the old renderer with the sunlight baked into the lightmap, and it
     can be used with existing maps without recompilation.  The downside is
     artifacts like doubled shadows and uneven shadow edges.
     
  2. Set r_sunlightMode to 2 and use 'q3gl2_sun' instead of 'q3map_sun' or
     'q3map_sunExt', like so:
  
        textures/skies/bluesky
        {
          qer_editorimage textures/skies/bluesky.jpg

          surfaceparm nomarks
          surfaceparm noimpact
          surfaceparm nolightmap
          surfaceparm sky
          q3gl2_sun 240 238 200 50 195 35 0.2
          q3map_skylight 50 16
          q3map_lightimage $whiteimage

          skyparms env/bluesky - -
        }

     The advantages with this method are that you don't get the artifacts that
     characterize the other method, and your map compiles a lot faster without
     the sunlight bouncing calculations.  The downsides are that your map will
     not display properly with the old renderer, and you lose the bounced light
     that compiling the map with q3map_sun* in it would have.

     
-------------------------------------------------------------------------------
  TONE MAPPING AND AUTO EXPOSURE
-------------------------------------------------------------------------------

This adds a new keyword to sky materials, q3gl2_tonemap.  The syntax is:

    q3gl2_tonemap <toneMapMin> <toneMapAvg> <toneMapMax> <autoExposureMin> <autoExposureMax>
  
Each of these settings corresponds to a matching cvar, so you can view and
adjust the effect before settling on fixed settings.


-------------------------------------------------------------------------------
  COLOR GRADING
-------------------------------------------------------------------------------

Rend2 can apply a color grading LUT (look-up table) as the final step of the
postprocess pipeline, right after tone mapping/gamma.  It's a cheap way to
give a level its own mood (cold and icy, warm desert sun, damp forest, a
tinted "hurt" state, etc.) without touching lighting or materials.

LUT format: a 256x16 PNG, a 16x16x16 color cube flattened into 16 tiles of
16x16 (the classic Unity-style 2D strip).  Bundled presets live in
`main/gfx/luts/`:

  - `neutral`     - identity, no visual change (the default)
  - `bacta`       - green healing-tank tint
  - `hoth`        - cold, icy, high-contrast exteriors
  - `desert`      - warm, sun-baked, richly saturated
  - `rainyforest` - damp, overcast, muted olive-grey woodland
  - `lowhealth`   - desaturated, red-pushed tension grade

To try one at the console:

    r_colorGrading 1
    r_colorGradeLUT gfx/luts/hoth

Changing `r_colorGradeLUT` reloads the texture live, no `vid_restart` needed
-- switch between presets as many times as you like.  To give a map its own
default grade, set a `colorgradelut` key on worldspawn (same workflow as
`sun` or `ambient`):

    "colorgradelut" "gfx/luts/rainyforest"

An empty/omitted key leaves grading off, so existing maps are unaffected.

To author your own LUT: take `neutral.png` (or generate a fresh identity
strip), load it into Photoshop/DaVinci/whatever as if it were footage, push
whatever curves/color-balance/saturation you want, and export back at the
same 256x16 size over the original file.


-------------------------------------------------------------------------------
  THANKS
-------------------------------------------------------------------------------

I'd like to take this part of the readme to thank the numerous people who
contributed thoughts, ideas, and whole swaths of code to this project.

  - Id Software, for creating RTCW and releasing its source code under a
    GPL license, without which this project would not be possible.

  - Zachary 'Zakk' Slater, Thilo Schulz, Tim Angus, and the rest of the
    ioquake3 team and contributors, for improving massively upon the raw Quake
    3 source, and accepting my and gimhael's modular renderer patch.

  - Robert 'Tr3B' Beckebans and the other contributors to XReaL, for letting me
    liberally copy code from you. :)
  
  - Andrew 'Black Monk' Prosnik, Andrei 'Makro' Drexler, Tomi 'T.T.I.' Isoaho,
    Richard 'JBravo' Allen, Walter 'Johnny Rocket' Somol, and the rest of the
    Boomstick Studios, for contributing code, feature requests, and testing.
    
  - Yoshiharu Gotanda, Tatsuya Shoji, and the rest of tri-Ace's R&D Department,
    for creating the tri-Ace shading equations and posting their derivations in
    simple English.
    
  - Matthias 'gimhael' Bentrup, for random ideas and bits of code.
  
  - Evan 'megatog615' Goers, for testing, ideas, and bugging me just enough
    that I'd write documentation. :)

  - The folks at #ioquake3, who don't seem to mind when I suddenly drop a
    screenshot and insist on talking about it. :)

  - And lots of various other random people, who posted on forums, blogs, and
    Wikipedia, who helped in small but numerous ways.

If I missed you in this section, feel free to drop me a line and I'll add you.


-------------------------------------------------------------------------------
  CONTACT
-------------------------------------------------------------------------------

My name is James Canete, and I wrote most of this readme.  Also, a renderer.

If you wish to get in touch with me, try my GMail at use.less01 (you should be
able to solve this), or look for SmileTheory in #ioquake3 on irc.freenode.net.
