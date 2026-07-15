//Rich - 03-05-03 Updated Chris' pump shaders
//Rich - 10-05-03 Added mg42 turret mount shaders
//William Faure - 24-04-21 Converted all shaders to RTCW

models/mapobjects/pump_sd/bottom
{
	{
	         	map textures/effects/envmap_slate_90.tga
	         	rgbGen lightingdiffuse
	         	tcmod scale 1.5 1.5
	         	tcGen environment
	}
	{
	         	map models/mapobjects/pump_sd/bottom.tga
	         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
	         	//rgbgen lightingDiffuse
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
		
}

models/mapobjects/pump_sd/top
{
	{
			map textures/effects/envmap_slate_90.tga
	         	rgbGen lightingdiffuse
	         	tcmod scale 1.5 1.5
	         	tcGen environment
	}
	{
	         	map models/mapobjects/pump_sd/top.tga
	         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
	         	//rgbgen lightingDiffuse
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
		
}


models/mapobjects/tanks_sd/mg42turret
{
	{
	         	map textures/effects/envmap_slate_90.tga
	         	rgbGen lightingdiffuse
	         	tcGen environment
	}
	{
	         	map models/mapobjects/tanks_sd/mg42turret.tga
	         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
	         	//rgbgen lightingDiffuse 
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
		
}



models/mapobjects/tanks_sd/churchill_flat
{
	{
	         	map textures/effects/envmap_ice2.tga
	         	rgbGen lightingdiffuse
	         	tcGen environment
	}
	{
	         	map models/mapobjects/tanks_sd/churchill_flat.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}


models/mapobjects/radios_sd/blue2
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_blue.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise 0 1 0.8 9
	}
}

models/mapobjects/radios_sd/blue1
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_blue.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise 0 1 0.2 12
	}
}

models/mapobjects/radios_sd/gold3
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_gold.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave sin 0 1 0 3
	}
}

models/mapobjects/radios_sd/gold2
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_gold.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise 0 1 0.9 9
	}
}

models/mapobjects/radios_sd/gold1
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_gold.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise 0.2 0.8 0.5 13
	}
}

models/mapobjects/radios_sd/green5
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_green.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise  0.25 0.75 0 8
	}
}

models/mapobjects/radios_sd/green4
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_green.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise 0.2 0.8 0 10

	}
}

models/mapobjects/radios_sd/green3
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_green.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise 0 9 0.66 12
	}
}

models/mapobjects/radios_sd/green2
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_green.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise 0 1 0.33 5
	}
}

models/mapobjects/radios_sd/green1
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_green.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise 0 1 0.15 20
	}
}

models/mapobjects/radios_sd/red4
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_red.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise 0 1 0.8 9
	}
}

models/mapobjects/radios_sd/red3
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_red.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise 0 1 0.2 12
	}
}

models/mapobjects/radios_sd/red2
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_red.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave sin 0 1 0 3
	}
}

models/mapobjects/radios_sd/red1
{
	polygonOffset
	surfaceparm nomarks
	{
		map models/mapobjects/radios_sd/beep_red.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave noise 0.5 0.5 0.5 10
	}
}

models/mapobjects/radios_sd/screen_broken
{
	polygonOffset
	surfaceparm nomarks
	{
		map gfx/damage/glass_mrk.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		//rgbgen lightingDiffuse 
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/radios_sd/screen_circle_fx
{
    	qer_editorimage models/mapobjects/radios_sd/screen_circle.tga
	surfaceparm nomarks
	polygonOffset

	
	{
			map models/mapobjects/radios_sd/radio_scroll1.tga
			blendFunc GL_ONE GL_ONE
			rgbGen wave noise 0.3 0.7 0 20
			tcMod scale  1 1.25
			
	
	}	
				
	{
			map models/mapobjects/radios_sd/radio_scroll2.tga
			blendFunc GL_ONE GL_ONE
			rgbGen wave noise 0.3 0.7 0 20
			tcMod scale  3.25 1.25
			tcMod scroll 2 0
	}	
			
	{
			map models/mapobjects/radios_sd/screen_circle.tga
			blendfunc blend
			//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	
	
}

models/mapobjects/radios_sd/screen_square_fx
{
    	qer_editorimage models/mapobjects/radios_sd/screen_square.tga
	surfaceparm nomarks
	polygonOffset

	{
		map models/mapobjects/radios_sd/radio_scroll1.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave noise 0.3 0.7 0 20


	}	
			
	{
		map models/mapobjects/radios_sd/radio_scroll2.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave noise 0.3 0.7 0 20
		tcMod scale  2 1
		tcMod scroll 3 0
	}	
		
	{
		map models/mapobjects/radios_sd/screen_square.tga
		blendfunc blend
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}

	
}

models/mapobjects/radios_sd/grid

{
    	qer_editorimage models/mapobjects/radios_sd/grid.tga
     	cull twosided
     	{
		map models/mapobjects/radios_sd/grid.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
	surfaceparm nomarks
}


models/mapobjects/radios_sd/iron

{
    	qer_editorimage models/mapobjects/radios_sd/iron.tga
    	cull twosided
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/iron.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/neutralcabinet

{
    	qer_editorimage models/mapobjects/radios_sd/neutralcabinet.tga
    	surfaceparm nomarks
    	{
		map models/mapobjects/radios_sd/neutralcabinet.tga
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/axiscabinet

{
    	qer_editorimage models/mapobjects/radios_sd/axiscabinet.tga
    	surfaceparm nomarks
    	{
		map models/mapobjects/radios_sd/axiscabinet.tga
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/crate1

{
     qer_editorimage models/mapobjects/radios_sd/crate.tga
		{
		map models/mapobjects/radios_sd/crate.tga
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
     surfaceparm nomarks
}

models/mapobjects/radios_sd/command7a

{
     qer_editorimage models/mapobjects/radios_sd/command7a.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command7a.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/command7

{
     qer_editorimage models/mapobjects/radios_sd/command7.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command7.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/command6a

{
     qer_editorimage models/mapobjects/radios_sd/command6a.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command6a.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/command6

{
     qer_editorimage models/mapobjects/radios_sd/command6.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command6.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/command5a
{
     qer_editorimage models/mapobjects/radios_sd/command5a.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command5a.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/command5
{
     qer_editorimage models/mapobjects/radios_sd/command5.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command5.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/command4a
{
     qer_editorimage models/mapobjects/radios_sd/command4a.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command4a.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/command4
{
     qer_editorimage models/mapobjects/radios_sd/command4.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command4.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/command3a

{
     qer_editorimage models/mapobjects/radios_sd/command3a.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command3a.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/command3

{
     qer_editorimage models/mapobjects/radios_sd/command3.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command3.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/command2a

{
    qer_editorimage models/mapobjects/radios_sd/command2a.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command2a.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}
models/mapobjects/radios_sd/command2

{
    qer_editorimage models/mapobjects/radios_sd/command2.tga
    surfaceparm nomarks
         {
         	map textures/effects/envmap_radar.tga
         	rgbGen lightingdiffuse
         	tcGen environment
         }
         {
         	map models/mapobjects/radios_sd/command2.tga
         	blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
         	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/command1a
{
     qer_editorimage models/mapobjects/radios_sd/command1a.tga
     surfaceparm nomarks
     	{
     		map textures/effects/envmap_radar.tga
     		rgbGen lightingdiffuse
     		tcGen environment
     	}
     	{
     		map models/mapobjects/radios_sd/command1a.tga
     		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
     		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	
}

models/mapobjects/radios_sd/command1
{
     qer_editorimage models/mapobjects/radios_sd/command1.tga
     surfaceparm nomarks
     	{
     		map textures/effects/envmap_radar.tga
     		rgbGen lightingdiffuse
     		tcGen environment
     	}
     	{
     		map models/mapobjects/radios_sd/command1.tga
     		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
     		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	
}

models/mapobjects/radios_sd/allied_sign

{
     qer_editorimage models/mapobjects/radios_sd/allied_sign.tga
     surfaceparm nomarks
		{
		map models/mapobjects/radios_sd/allied_sign.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/radios_sd/neutral_sign

{
	qer_editorimage models/mapobjects/radios_sd/neutral_sign.tga
	surfaceparm nomarks
     	{
		map models/mapobjects/radios_sd/neutral_sign.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/radios_sd/axis_sign

{
     qer_editorimage models/mapobjects/radios_sd/axis_sign.tga
     surfaceparm nomarks
		{
		map models/mapobjects/radios_sd/axis_sign.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/raster/moto
{
	cull twosided
     	{
		map models/mapobjects/raster/moto.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/raster/moto_bag
{
		{
		map models/mapobjects/raster/moto_bag.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/barrel_sd/barrel_side
{
	qer_editorimage textures/props/barrel_m01.tga
		{
		map textures/props/barrel_m01.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/barrel_sd/barrel_top
{
	qer_editorimage textures/props/barrel_m02.tga
		{
		map textures/props/barrel_m02.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_arches
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd.tga
	
	{
		map models/mapobjects/blitz_sd/blitz_sd.tga
		rgbGen vertex
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_arches_mm
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd.tga
	
	{
		map models/mapobjects/blitz_sd/blitz_sd.tga
		rgbGen vertex
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_arches_s
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd_s.tga
	
	{
		map models/mapobjects/blitz_sd/blitz_sd_s.tga
		rgbGen vertex
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_arches_s_mm
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd_s.tga
	
	{
		map models/mapobjects/blitz_sd/blitz_sd_s.tga
		rgbGen vertex
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_body
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd.tga
	{
		map models/mapobjects/blitz_sd/blitz_sd.tga
		rgbGen vertex
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_body_mm
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd.tga
		{
		map models/mapobjects/blitz_sd/blitz_sd.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_body_s
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd_s.tga
		{
		map models/mapobjects/blitz_sd/blitz_sd_s.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_body_s_mm
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd_s.tga
		{
		map models/mapobjects/blitz_sd/blitz_sd_s.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_interior_mm
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd_interior.tga
	cull twosided
		{
		map models/mapobjects/blitz_sd/blitz_sd_interior.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_interior02_mm
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd_interior02.tga
	cull twosided
		{
		map models/mapobjects/blitz_sd/blitz_sd_interior02.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_windows
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd.tga
	{
		map textures/effects/envmap_slate.tga
		rgbGen lightingdiffuse
		tcGen environment
	}
	{
		map models/mapobjects/blitz_sd/blitz_sd.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_windows_mm
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd.tga
	{
		map textures/effects/envmap_slate.tga
		rgbGen lightingdiffuse
		tcGen environment
	}
	{
		map models/mapobjects/blitz_sd/blitz_sd.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_windows_s
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd_s.tga
	{
		map textures/effects/envmap_slate.tga
		rgbGen lightingdiffuse
		tcGen environment
	}
	{
		map models/mapobjects/blitz_sd/blitz_sd_s.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/blitz_sd/blitz_sd_windows_s_mm
{
	qer_editorimage models/mapobjects/blitz_sd/blitz_sd_s.tga
	{
		map textures/effects/envmap_slate.tga
		rgbGen lightingdiffuse
		tcGen environment
	}
	{
		map models/mapobjects/blitz_sd/blitz_sd_s.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/bodyparts/i_body1
{
	cull twosided
		{
		map models/mapobjects/bodyparts/i_body1.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

// Railgun tug and trailer
//**********************************************************************

models/mapobjects/cab_sd/wheels
{
	qer_editorimage models/mapobjects/cab_sd/wheels.tga
	{
		map $lightmap
		rgbGen identity
	}
	{
		map models/mapobjects/cab_sd/wheels.tga
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

models/mapobjects/cab_sd/part1
{
	qer_editorimage models/mapobjects/cab_sd/part1.tga
	{
		map $lightmap
		rgbGen identity
	}
	{
		map models/mapobjects/cab_sd/part1.tga
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

models/mapobjects/cab_sd/part2	
{
	qer_editorimage models/mapobjects/cab_sd/part2.tga
	{
		map $lightmap
		rgbGen identity
	}
	{
		map models/mapobjects/cab_sd/part2.tga
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

models/mapobjects/cab_sd/trailer
{
	qer_editorimage models/mapobjects/cab_sd/trailer.tga
	{
		map $lightmap
		rgbGen identity
	}
	{
		map models/mapobjects/cab_sd/trailer.tga
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

models/mapobjects/cmarker/c_box
{
	qer_editorimage models/mapobjects/cmarker/box_m05.tga
		{
		map models/mapobjects/cmarker/box_m05.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/cmarker/c_box_allied
{
	qer_editorimage models/mapobjects/cmarker/allied_crate.tga
		{
		map models/mapobjects/cmarker/allied_crate.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/cmarker/c_box_axis
{
	qer_editorimage models/mapobjects/cmarker/axis_crate.tga
		{
		map models/mapobjects/cmarker/axis_crate.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/cmarker/c_box_neutral
{
	qer_editorimage models/mapobjects/cmarker/neutral_crate.tga
		{
		map models/mapobjects/cmarker/neutral_crate.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/cmarker/c_sandbag_allied
{
	qer_editorimage models/mapobjects/cmarker/allied_sack.tga
		{
		map models/mapobjects/cmarker/allied_sack.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/cmarker/c_sandbag_axis
{
	qer_editorimage models/mapobjects/cmarker/axis_sack.tga
		{
		map models/mapobjects/cmarker/axis_sack.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/cmarker/c_shovel
{
	qer_editorimage models/mapobjects/cmarker/shovel.tga
		{
		map models/mapobjects/cmarker/shovel.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/cmarker/cflag_allied
{
	qer_editorimage models/mapobjects/cmarker/cflagallied.tga
	//cull disable
	cull twosided
	deformVertexes wave 194 sin 0 3 0 .4
     	{
		map models/mapobjects/cmarker/cflagallied.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/cmarker/cflag_axis
{
	qer_editorimage models/mapobjects/cmarker/cflagaxis.tga
	//cull disable
	cull twosided
	deformVertexes wave 194 sin 0 3 0 .4
     	{
		map models/mapobjects/cmarker/cflagaxis.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/cmarker/cflag_neutral
{
	qer_editorimage models/mapobjects/cmarker/cflagneutral.tga
	//cull disable
	cull twosided
	deformVertexes wave 194 sin 0 3 0 .4
     	{
		map models/mapobjects/cmarker/cflagneutral.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/flag/flag_allied
{
	//cull disable
	cull twosided
	nomipmaps
	nopicmip
		{
		map models/mapobjects/flag/flag_allied.tga
			rgbGen vertex
		}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/flag/flag_axis
{
	//cull disable
	cull twosided
	nomipmaps
	nopicmip
		{
		map models/mapobjects/flag/flag_axis.tga
			rgbGen vertex
		}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/flag/flag_dam
{
	//cull disable
	cull twosided
	nomipmaps
	nopicmip
		{
		map models/mapobjects/flag/flag_dam.tga
			rgbGen vertex
		}
		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/props_sd/fuel_can
{
	surfaceparm nomarks
		{
		map models/mapobjects/props_sd/fuel_can.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/furniture/sherman_s
{
	{
		map models/mapobjects/furniture/sherman_s.tga
		blendfunc blend
		rgbGen identity
	}
}

models/mapobjects/furniture/xsink
{
	{
		map textures/effects/tinfx.tga
		rgbGen vertex
		tcGen environment
	}
	{
		map models/mapobjects/furniture/xsink.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

models/mapobjects/furniture/xsink_fac
{
	{
	map models/mapobjects/furniture/xsink_fac.tga
	blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	rgbGen identity
	}
	{
	map $lightmap
	rgbGen identity
	blendFunc GL_DST_COLOR GL_ZERO
	}
}

models/mapobjects/goldbox_sd/goldbox
{
	qer_editorimage models/mapobjects/goldbox_sd/goldbox.tga
	surfaceparm metalsteps
	{
		map models/mapobjects/goldbox_sd/goldbox.tga
		//rgbgen lightingDiffuse
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/grass_sd/grass
{
	qer_editorimage models/mapobjects/grass_sd/grass.tga
	cull twosided
	surfaceparm nomarks
	surfaceparm nonsolid
	//surfaceparm alphashadow
	surfaceparm trans
	nopicmip
	{
		map models/mapobjects/grass_sd/grass.tga
		rgbGen const ( 0.3 0.3 0.3 )
		detail
	}
}

models/mapobjects/grass_sd/grass_spike
{
	qer_editorimage models/mapobjects/grass_sd/grass_spike.tga
	cull twosided
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm nonsolid
	//surfaceparm alphashadow
	surfaceparm trans
	nopicmip
	{
		map models/mapobjects/grass_sd/grass_spike.tga
		alphaFunc GE128
		rgbGen const ( 0.4 0.4 0.4 )
	}
}



models/mapobjects/knight/knt
{
	{
		map textures/effects/tinfx.tga
		blendfunc blend
		rgbGen vertex
		tcGen environment
	}
	{
		map models/mapobjects/knight/knt.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

models/mapobjects/lamps/bel_lamp2k
{
	qer_editorimage models/mapobjects/light/bel_lamp.tga
	q3map_lightimage models/colors/amber.tga
	q3map_surfaceLight 1000
	surfaceparm nomarks
	{
		map models/mapobjects/light/bel_lamp.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/bel_lamp.blend.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/lamps/bel_lamp2k_gm		// for misc_gamemodel
{
	qer_editorimage models/mapobjects/light/bel_lamp.tga
	q3map_lightimage models/colors/amber.tga
	q3map_surfaceLight 1000
	surfaceparm nomarks
	{
		map models/mapobjects/light/bel_lamp.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	{
		map models/mapobjects/light/bel_lamp.blend.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/bel_lamp
{
	q3map_lightimage models/colors/amber.tga
	q3map_surfaceLight 5000
	surfaceparm nomarks
	{
		map models/mapobjects/light/bel_lamp.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/bel_lamp.blend.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/bel_lamp_gm		// for misc_gamemodels
{
	qer_editorimage models/mapobjects/light/bel_lamp.tga
	q3map_lightimage models/colors/amber.tga
	q3map_surfaceLight 5000
	surfaceparm nomarks
	{
		map models/mapobjects/light/bel_lamp.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	{
		map models/mapobjects/light/bel_lamp.blend.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/bel_lampb
{
	qer_editorimage models/mapobjects/light/bel_lamp.tga
	q3map_lightimage models/colors/amber.tga
	q3map_surfaceLight 10000
	surfaceparm nomarks
	{
		map models/mapobjects/light/bel_lamp.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/bel_lamp.blend.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/bel_lampm
{
	qer_editorimage models/mapobjects/light/bel_lamp.tga
	q3map_lightimage models/colors/amber.tga
	q3map_surfaceLight 7000
	surfaceparm nomarks
	{
		map models/mapobjects/light/bel_lamp.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/bel_lamp.blend.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/bel_lampm_gm		// for misc_gamemodels
{
	qer_editorimage models/mapobjects/light/bel_lamp.tga
	q3map_lightimage models/colors/amber.tga
	q3map_surfaceLight 7000
	surfaceparm nomarks
	{
		map models/mapobjects/light/bel_lamp.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	{
		map models/mapobjects/light/bel_lamp.blend.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/cage_lightn
{
	q3map_lightimage models/mapobjects/light/cage_lightn.tga
	q3map_lightrgb 0.80 0.72 0.60
	surfaceparm nomarks
	{
		map models/mapobjects/light/cage_lightn.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/cage_light.blendn.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/cage_lightna
{
	cull twosided
	surfaceparm nomarks
     	{
		map models/mapobjects/light/cage_lightna.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/light/cage_light1k
{
	q3map_lightimage models/mapobjects/light/cagelight_a.tga
	q3map_lightrgb 0.80 0.72 0.60
	q3map_surfacelight 1000
	surfaceparm nomarks
	{
		map models/mapobjects/light/cagelight_a.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/cagelight.blenda.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/cage_light2k
{
	q3map_lightimage models/mapobjects/light/cagelight_a.tga
	q3map_lightrgb 0.80 0.72 0.60
	q3map_surfacelight 2000
	surfaceparm nomarks
	{
		map models/mapobjects/light/cagelight_a.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/cagelight.blenda.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/cage_light3k
{
	q3map_lightimage models/mapobjects/light/cagelight_a.tga
	q3map_lightrgb 0.80 0.72 0.60
	q3map_surfacelight 3000
	surfaceparm nomarks
	{
		map models/mapobjects/light/cagelight_a.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/cagelight.blenda.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/cage_light4k
{
	q3map_lightimage models/mapobjects/light/cagelight_a.tga
	q3map_lightrgb 0.80 0.72 0.60
	q3map_surfacelight 4000
	surfaceparm nomarks
	{
		map models/mapobjects/light/cagelight_a.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/cagelight.blenda.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/cage_light5k
{
	q3map_lightimage models/mapobjects/light/cagelight_a.tga
	q3map_lightrgb 0.80 0.72 0.60
	q3map_surfacelight 5000
	surfaceparm nomarks
	{
		map models/mapobjects/light/cagelight_a.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/cagelight.blenda.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/cage_light7k
{
	q3map_lightimage models/mapobjects/light/cagelight_a.tga
	q3map_lightrgb 0.80 0.72 0.60
	q3map_surfacelight 7000
	surfaceparm nomarks
	{
		map models/mapobjects/light/cagelight_a.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/cagelight.blenda.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/cage_light9k
{
	q3map_lightimage models/mapobjects/light/cagelight_a.tga
	q3map_lightrgb 0.80 0.72 0.60
	q3map_surfacelight 9000
	surfaceparm nomarks
	{
		map models/mapobjects/light/cagelight_a.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/cagelight.blenda.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/cage_light12k
{
	q3map_lightimage models/mapobjects/light/cagelight_a.tga
	q3map_lightrgb 0.80 0.72 0.60
	q3map_surfacelight 12000
	surfaceparm nomarks
	{
		map models/mapobjects/light/cagelight_a.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/cagelight.blenda.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/cagelight_a
{
	q3map_lightimage models/mapobjects/light/cagelight_a.tga
	q3map_lightrgb 0.80 0.72 0.60
	q3map_surfacelight 7000
	surfaceparm nomarks
	{
		map models/mapobjects/light/cagelight_a.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/cagelight.blenda.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/cagelight_r
{
	q3map_lightimage models/mapobjects/light/cagelight_r.tga
	q3map_lightrgb 1.0 0.1 0.1
	q3map_surfacelight 7000
	surfaceparm nomarks
	{
		map models/mapobjects/light/cagelight_r.tga
		rgbGen vertex
	}
	{
		map models/mapobjects/light/cagelight.blendr.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

//models/mapobjects/light/chandlier4
//{
//	qer_alphafunc gequal 0.5
//	cull disable
//	surfaceparm nomarks
//     	{
//		map models/mapobjects/light/chandlier4.tga
//		blendfunc GL_ONE GL_ZERO
//		alphaFunc GE128
//		depthWrite
//		//rgbGen identity
//		rgbGen vertex
//		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
//}

models/mapobjects/light/light_m4
{
	q3map_lightimage models/mapobjects/light/light_m4.tga
	q3map_surfacelight 5000
	surfaceparm nomarks
	{
		map models/mapobjects/light/light_m4.tga
		rgbGen vertex
	}
	{
		map textures/lights/light_m4.blend.tga
		blendfunc GL_ONE GL_ONE
		rgbGen identity
	}
}

models/mapobjects/light/pendant10k
{
	qer_editorimage models/mapobjects/light/pendant_sd.tga
		{
		map models/mapobjects/light/pendant_sd.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	q3map_lightimage models/mapobjects/light/pendant_sd.tga
	q3map_surfacelight 10000
	surfaceparm nomarks
}

models/mapobjects/light/sconce
{
	qer_alphafunc gequal 0.5
	qer_trans 0.5
	cull disable
	surfaceparm nomarks
     	{
		map models/mapobjects/light/sconce.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/light/sconce2
{
	q3map_lightimage models/mapobjects/light/sconce2.tga
	q3map_surfacelight 5000
	surfaceparm nomarks
	{
		map models/mapobjects/light/sconce2.tga
		rgbGen identity
	}
}

models/mapobjects/light/sd_sconce
{
	qer_alphafunc gequal 0.5
	cull disable
	surfaceparm nomarks
     	{
		map models/mapobjects/light/sd_sconce.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/locomotive_sd/loco1_sd
{
	cull disable
		{
		map models/mapobjects/locomotive_sd/loco1_sd
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/locomotive_sd/loco2_sd
{
	cull disable
		{
		map models/mapobjects/locomotive_sd/loco2_sd
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/locomotive_sd/wheel_sd
{
	qer_alphafunc gequal 0.5
	cull disable
	surfaceparm nomarks
     	{
		map models/mapobjects/locomotive_sd/wheel_sd.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/logs_sd/snow
{
	qer_editorimage textures/snow_sd/snow_var01.tga
		{
		map textures/snow_sd/snow_var01.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/miltary_trim/bags1_s_wils
{
	qer_editorimage models/mapobjects/miltary_trim/bags1_s.tga
		{
		map models/mapobjects/miltary_trim/bags1_s.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/miltary_trim/metal_m05
{
	cull disable
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
     	{
		map models/mapobjects/miltary_trim/metal_m05.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/miltary_trim/metal_m05_wils
{
	qer_editorimage models/mapobjects/miltary_trim/metal_m05.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
     	{
		map models/mapobjects/miltary_trim/metal_m05.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		//rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/pak75_sd/pak75-a
{
	qer_alphafunc gequal 0.5
	cull none
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
     	{
		map models/mapobjects/pak75_sd/pak75-a.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants/bushes1
{
	cull twosided
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/plants/bushes1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants/bushes3
{
	cull twosided
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/plants/bushes3.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

//**********************************************************************
// Plants 
//**********************************************************************

models/mapobjects/plants_sd/bush_desert1
{
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/bush_desert1.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
	nopicmip
     	{
		map models/mapobjects/plants_sd/bush_desert1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/bush_desert2
{ 
     qer_alphafunc greater 0.5 
     qer_editorimage models/mapobjects/plants_sd/bush_desert2.tga 
     cull disable 
     q3map_bounceScale 0.25
     surfaceparm alphashadow 
     surfaceparm nomarks 
     surfaceparm pointlight 
     surfaceparm trans 
     nopicmip 
     	{
		map models/mapobjects/plants_sd/bush_desert2.tga 
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}


models/mapobjects/plants_sd/bush_snow1
{
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/bush_snow1.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
	nopicmip
     	{
		map models/mapobjects/plants_sd/bush_snow1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/bush_snow2
{
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/bush_snow2.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
	nopicmip
     	{
		map models/mapobjects/plants_sd/bush_snow2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}


models/mapobjects/plants_sd/catail1
{
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/catail1.tga
	cull disable
	nopicmip
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
	nopicmip
     	{
		map models/mapobjects/plants_sd/catail1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/catail2
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/catail2.tga
	cull disable
	nopicmip
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
	deformVertexes wave 15 sin 0 1 0 0.25
     	{
		map models/mapobjects/plants_sd/catail2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/catail3
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/catail3.tga
	cull disable
	nopicmip
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
	deformVertexes wave 15 sin 0 1 0 0.25
     	{
		map models/mapobjects/plants_sd/catail3.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/deadbranch1
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/deadbranch1.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/deadbranch1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/deadbranch2
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/deadbranch2.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/deadbranch2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/deadbranch3
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/deadbranch3.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/deadbranch3.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/deadbranch1_damp
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/deadbranch1_damp.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/deadbranch1_damp.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/deadbranch2_damp
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/deadbranch2_damp.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/deadbranch2_damp.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/deadbranch3_damp
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/deadbranch3_damp.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/deadbranch3_damp.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/shrub_green1
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/shrub_green1.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/shrub_green1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}


models/mapobjects/plants_sd/shrub_green2
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/shrub_green2.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/shrub_green2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}



models/mapobjects/plants_sd/leaf1
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/leaf1.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/leaf1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/leaf2
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/leaf2.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/leaf2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}


models/mapobjects/plants_sd/leaf3
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/leaf3.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/leaf3.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}


models/mapobjects/plants_sd/grass_dry1
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/grass_dry1.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/grass_dry1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/grass_dry2
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/grass_dry2.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/grass_dry2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/grass_dry3
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/grass_dry3.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/grass_dry3.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/grass_green1
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/grass_green1.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/grass_green1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/grass_green2
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/grass_green2.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/grass_green2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/grass_green3
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/grass_green3.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/grass_green3.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/mil1
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/mil1.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/mil1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/mil2
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/mil2.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/mil2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/plants_sd/grassfoliage1
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/plants_sd/grassfoliage1.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/plants_sd/grassfoliage1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree_temperate_sd/trunk_temperate
{
	surfaceparm nomarks
		{
		map models/mapobjects/tree_temperate_sd/trunk_temperate.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tree_temperate_sd/trunk_cut
{

	surfaceparm nomarks
		{
		map models/mapobjects/tree_temperate_sd/trunk_cut.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}


models/mapobjects/tree_temperate_sd/leaves_temperate1
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/tree_temperate_sd/leaves_temperate1.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/tree_temperate_sd/leaves_temperate1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree_temperate_sd/leaves_temperate2
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/tree_temperate_sd/leaves_temperate2.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/tree_temperate_sd/leaves_temperate2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree_temperate_sd/leaves_temperate3
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/tree_temperate_sd/leaves_temperate3.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/tree_temperate_sd/leaves_temperate3.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}


models/mapobjects/tree_temperate_sd/leaves_temperate1_s
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/tree_temperate_sd/leaves_temperate1_s.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/tree_temperate_sd/leaves_temperate1_s.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree_temperate_sd/leaves_temperate2_s
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/tree_temperate_sd/leaves_temperate2_s.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/tree_temperate_sd/leaves_temperate2_s.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree_temperate_sd/leaves_temperate3_s
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/tree_temperate_sd/leaves_temperate3_s.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/tree_temperate_sd/leaves_temperate3_s.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree_temperate_sd/floor_leaf1
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/tree_temperate_sd/floor_leaf1.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/tree_temperate_sd/floor_leaf1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}


//**********************************************************************
models/mapobjects/portable_radar_sd/portable_radar_sd
{
	cull disable
	surfaceparm nomarks
	{
	         map textures/effects/envmap_slate.tga
	         //tcmod scale 2 2
	         rgbGen lightingdiffuse
	         tcGen environment
	}
	{
	         map models/mapobjects/portable_radar_sd/portable_radar_sd.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	
	
}

models/mapobjects/portable_radar_sd/portable_radar_t_sd
{
	qer_alphafunc gequal 0.5
	cull disable
	surfaceparm nomarks
     	{
		map models/mapobjects/portable_radar_sd/portable_radar_t_sd.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/props_sd/basket
{
	surfaceparm nomarks
		{
		map models/mapobjects/props_sd/basket.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/props_sd/basketsand
{
	qer_editorimage textures/props_sd/basketsand.tga
		{
		map textures/props_sd/basketsand.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/props_sd/sandlevel
{
	qer_editorimage textures/desert_sd/sand_disturb_desert.tga
		{
		map textures/desert_sd/sand_disturb_desert.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/props_sd/lid
{
	surfaceparm nomarks
		{
		map models/mapobjects/props_sd/lid.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/props_sd/bunk_sd1
{
	qer_editorimage textures/chat/bedlinen_c02.tga
	surfaceparm nomarks
		{
		map textures/chat/bedlinen_c02.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/props_sd/bunk_sd2
{
	qer_editorimage models/mapobjects/furniture/wood1.tga
	surfaceparm nomarks
		{
		map models/mapobjects/furniture/wood1.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/props_sd/bunk_sd3
{
	qer_editorimage textures/chat/bedlinenpillow_c02.tga
	surfaceparm nomarks
		{
		map textures/chat/bedlinenpillow_c02.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/props_sd/drape_rug
{
	qer_editorimage textures/egypt_props_sd/siwa_carpet2.tga
	cull twosided
	surfaceparm nomarks
		{
		map textures/egypt_props_sd/siwa_carpet2.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/props_sd/drape_wood
{
	qer_editorimage textures/egypt_door_sd/siwa_door_neutral.tga
	surfaceparm nomarks
		{
		map textures/egypt_door_sd/siwa_door_neutral.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/props_sd/vase
{
	surfaceparm nomarks
		{
		map models/mapobjects/props_sd/vase.tga
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

// radio sign
models/mapobjects/radios_sd/sign
{
     	{
		map models/mapobjects/radios_sd/sign.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

//**********************************************************************
// Rocks
//**********************************************************************

models/mapobjects/rocks_sd/rock_desert
{
	qer_editorimage models/mapobjects/rocks_sd/rock_desert.tga
		{
		map models/mapobjects/rocks_sd/rock_desert.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/rocks_sd/rock_desert_small
{
	qer_editorimage models/mapobjects/rocks_sd/rock_desert_small.tga
		{
		map models/mapobjects/rocks_sd/rock_desert_small.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/rocks_sd/rock_desert_big
{
	qer_editorimage models/mapobjects/rocks_sd/rock_desert_big.tga
		{
		map models/mapobjects/rocks_sd/rock_desert_big.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/rocks_sd/rock_snow
{
	qer_editorimage models/mapobjects/rocks_sd/rock_snow.tga
		{
		map models/mapobjects/rocks_sd/rock_snow.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	//q3map_clipModel
}

models/mapobjects/rocks_sd/rock_snow_small
{
	qer_editorimage models/mapobjects/rocks_sd/rock_snow_small.tga
		{
		map models/mapobjects/rocks_sd/rock_snow_small.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/rocks_sd/rock_snow_big
{
	qer_editorimage models/mapobjects/rocks_sd/rock_snow_big.tga
		{
		map models/mapobjects/rocks_sd/rock_snow_big.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	//q3map_clipModel
}

models/mapobjects/rocks_sd/rock_temperate
{
	//q3map_clipModel
	qer_editorimage models/mapobjects/rocks_sd/rock_temperate.tga
		{
		map models/mapobjects/rocks_sd/rock_temperate.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/rocks_sd/rock_temperate_small
{
	//q3map_clipModel
	qer_editorimage models/mapobjects/rocks_sd/rock_temperate_small.tga
		{
		map models/mapobjects/rocks_sd/rock_temperate_small.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/rocks_sd/rock_temperate_big
{
	//q3map_clipModel
	qer_editorimage models/mapobjects/rocks_sd/rock_temperate_big.tga
		{
		map models/mapobjects/rocks_sd/rock_temperate_big.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/rocks_sd/rock_temperate2
{
	//q3map_clipModel
	qer_editorimage models/mapobjects/rocks_sd/rock_temperate2.tga
		{
		map models/mapobjects/rocks_sd/rock_temperate2.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/rocks_sd/rock_temperate2_small
{
	//q3map_clipModel
	qer_editorimage models/mapobjects/rocks_sd/rock_temperate2_small.tga
		{
		map models/mapobjects/rocks_sd/rock_temperate2_small.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/rocks_sd/rock_temperate2_big
{
	//q3map_clipModel
	qer_editorimage models/mapobjects/rocks_sd/rock_temperate2_big.tga
		{
		map models/mapobjects/rocks_sd/rock_temperate2_big.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/rocks_sd/rock_tunnelsiwa
{
	qer_editorimage models/mapobjects/rocks_sd/rock_tunnelsiwa.tga
		{
		map models/mapobjects/rocks_sd/rock_tunnelsiwa.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/rocks_sd/rock_tunnelsiwa_small
{
	qer_editorimage models/mapobjects/rocks_sd/rock_tunnelsiwa_small.tga
		{
		map models/mapobjects/rocks_sd/rock_tunnelsiwa_small.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

//**********************************************************************
// remapped snow rocks in fueldump
//**********************************************************************

models/mapobjects/props_sd/snowrock_clip
{
	qer_editorimage models/mapobjects/props_sd/snowrock_clip.tga
		{
		map models/mapobjects/rocks_sd/rock_snow_big.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	//q3map_clipModel
}

//**********************************************************************

models/mapobjects/seawall_rocks/rocks
{
	qer_editorimage textures/temperate_sd/rock_grayvar.tga
	q3map_forcemeta
	q3map_lightmapSampleOffset 8.0
	q3map_nonplanar
	//q3map_clipModel
		{
		map textures/temperate_sd/rock_grayvar.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
	surfaceparm pointlight
	q3map_shadeangle 180
 
}

models/mapobjects/siwa_tunnels_sd/siwa_tunnel
{
	qer_editorimage textures/desert_sd/rock_edged_smooth.tga
	//q3map_clipModel
	q3map_forcemeta
	q3map_lightmapSampleOffset 8.0
	q3map_nonplanar
	q3map_normalimage models/mapobjects/siwa_tunnels_sd/siwa_nm.tga
	q3map_shadeangle 180
	surfaceparm pointlight
		{
		map textures/desert_sd/rock_edged_smooth.tga
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/siwa_tunnels_sd/siwa_tunneliaclip
{
	qer_editorimage textures/common/clipmonster.tga
	q3map_forcemeta
    	//q3map_clipModel
    	surfaceparm nodraw
    	surfaceparm nomarks
    	surfaceparm nonsolid
    	surfaceparm monsterclip
}

models/mapobjects/skel/skel
{
	cull disable
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
     	{
		map models/mapobjects/skel/skel.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/skel/skel2
{
	cull disable
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
     	{
		map models/mapobjects/skel/skel2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/supplystands/frame
{
	{
		map textures/effects/envmap_slate.tga
		rgbGen lightingDiffuse
		tcGen environment
	}
	{
		map models/mapobjects/supplystands/frame.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/supplystands/metal_shelves
{
	{
		map textures/effects/envmap_slate.tga
		rgbGen lightingDiffuse
		tcGen environment
	}
	{
		map models/mapobjects/supplystands/metal_shelves.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/bits_backward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		map models/mapobjects/tanks_sd/tracks.tga
		alphaFunc GE128
		tcMod scroll 0 -2
		//rgbgen lightingDiffuse
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/bits_backward_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		map models/mapobjects/tanks_sd/tracks_a.tga
		alphaFunc GE128
		tcMod scroll 0 -2
	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/bits_forward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		map models/mapobjects/tanks_sd/tracks.tga
		alphaFunc GE128
		tcMod scroll 0 2
	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/bits_forward_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		map models/mapobjects/tanks_sd/tracks_a.tga
		alphaFunc GE128
		tcMod scroll 0 2
	//rgbgen lightingDiffuse
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}


models/mapobjects/tanks_sd/bits_forward_oasis
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		map models/mapobjects/tanks_sd/tracks.tga
		alphaFunc GE128
		tcMod scroll 0 1.5
	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/bits_l
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		map models/mapobjects/tanks_sd/tracks.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/bits_l_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		map models/mapobjects/tanks_sd/tracks_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/bits_r
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		map models/mapobjects/tanks_sd/tracks.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/bits_r_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		map models/mapobjects/tanks_sd/tracks_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/bits_static
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		map models/mapobjects/tanks_sd/tracks.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/bits_static_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		map models/mapobjects/tanks_sd/tracks_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/churchill_flat_oasis
{
	{
		map models/mapobjects/tanks_sd/churchill_flat_oasis.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/explosive
{
	qer_editorimage models/mapobjects/tanks_sd/explosive.tga
	surfaceparm metalsteps
	{
		map models/mapobjects/tanks_sd/explosive.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_cogs_alt_backward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		tcMod rotate 75
	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_cogs_alt_forward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		tcMod rotate -75
	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_cogs_left
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel_a.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_cogs_left_s
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel_a.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		rgbGen vertex
	}
}

models/mapobjects/tanks_sd/jag_cogs_right
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel_a.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_cogs_right_s
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel_a.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		rgbGen vertex
	}
}

models/mapobjects/tanks_sd/jag_cogs_snow_alt_backward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a_s.tga
		alphaFunc GE128
		tcMod rotate 75
	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_cogs_snow_alt_forward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a_s.tga
		alphaFunc GE128
		tcMod rotate -75
	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_cogs_snow_left
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel_a_s.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a_s.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_cogs_snow_right
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel_a_s.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a_s.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_tracks_alt_backward
{
	{
		map models/mapobjects/tanks_sd/tracks_b.tga
		tcMod scroll -1 0
	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_tracks_alt_forward
{
	{
		map models/mapobjects/tanks_sd/tracks_b.tga
		tcMod scroll 1 0
	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_tracks_left
{
	qer_editorimage models/mapobjects/tanks_sd/tracks_b.tga
	{
		map models/mapobjects/tanks_sd/tracks_b.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_tracks_left_s
{
	qer_editorimage models/mapobjects/tanks_sd/tracks_b.tga
	{
		map models/mapobjects/tanks_sd/tracks_b.tga
		rgbGen vertex
	}
}

models/mapobjects/tanks_sd/jag_tracks_right
{
	qer_editorimage models/mapobjects/tanks_sd/tracks_b.tga
	{
		map models/mapobjects/tanks_sd/tracks_b.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_tracks_right_s
{
	qer_editorimage models/mapobjects/tanks_sd/tracks_b.tga
	{
		map models/mapobjects/tanks_sd/tracks_b.tga
		rgbGen vertex
	}
}

models/mapobjects/tanks_sd/jag_wheels_alt_backward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a.tga
		alphaFunc GE128
		tcMod rotate -75
	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_wheels_alt_forward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a.tga
		alphaFunc GE128
		tcMod rotate -75
	//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_wheels_left
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel2_a.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_wheels_left_s
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel2_a.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a.tga
		alphaFunc GE128
		rgbGen vertex
	}
}

models/mapobjects/tanks_sd/jag_wheels_right
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel2_a.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jag_wheels_right_s
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel2_a.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a.tga
		alphaFunc GE128
		rgbGen vertex
	}
}

models/mapobjects/tanks_sd/jag_wheels_snow_alt_backward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a_s.tga
		alphaFunc GE128
		tcMod rotate -75
	//rgbgen lightingDiffuse 
		rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag_wheels_snow_alt_forward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a_s.tga
		alphaFunc GE128
		tcMod rotate -75
	//rgbgen lightingDiffuse 
		rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag_wheels_snow_left
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel2_a_s.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a_s.tga
		alphaFunc GE128
	//rgbgen lightingDiffuse 
		rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag_wheels_snow_right
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel2_a_s.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a_s.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
		rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jagdpanther
{
	qer_editorimage models/mapobjects/tanks_sd/jagdpanther.tga
	{
		map models/mapobjects/tanks_sd/jagdpanther.tga
		//rgbgen lightingDiffuse 
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jagdpanther_additions_des_s
{
	qer_editorimage models/mapobjects/tanks_sd/jagdpanther_additions_desert.tga
	{
		map models/mapobjects/tanks_sd/jagdpanther_additions_desert.tga
		rgbGen vertex
	}
}

models/mapobjects/tanks_sd/jagdpanther_additions_desert
{
	{
		map models/mapobjects/tanks_sd/jagdpanther_additions_desert.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jagdpanther_additions_temperate
{
	{
		map models/mapobjects/tanks_sd/jagdpanther_additions_temperate.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jagdpanther_additions_snow
{
	{
		map models/mapobjects/tanks_sd/jagdpanther_additions_snow.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jagdpanther_full
{
	qer_editorimage models/mapobjects/tanks_sd/jagdpanther_full.tga
	{
		map models/mapobjects/tanks_sd/jagdpanther_full.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

// Added by Rich May 10 2003
models/mapobjects/tanks_sd/mg42turret_2
{
	{
		map models/mapobjects/tanks_sd/mg42turret_2.tga
		//rgbgen lightingDiffuse 
	}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/tanks_sd/jagdpanther_full_temperate
{
	qer_editorimage models/mapobjects/tanks_sd/jagdpanther_full_temperate.tga
	{
		map models/mapobjects/tanks_sd/jagdpanther_full_temperate.tga
		//rgbgen lightingDiffuse 
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jagdpanther_full_s
{
	qer_editorimage models/mapobjects/tanks_sd/jagdpanther_full.tga
	{
		map models/mapobjects/tanks_sd/jagdpanther_full.tga
		rgbGen vertex
	}
}

models/mapobjects/tanks_sd/jag2_cogs_alt_backward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		tcMod rotate 75
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag2_cogs_alt_forward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		tcMod rotate -75
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag2_cogs_left
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel_a.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag2_cogs_right
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel_a.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag2_tracks_alt_backward
{
	{
		map models/mapobjects/tanks_sd/tracks_b.tga
		tcMod scroll -1 0
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag2_tracks_alt_forward
{
	{
		map models/mapobjects/tanks_sd/tracks_b.tga
		tcMod scroll 1 0
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag2_tracks_left
{
	qer_editorimage models/mapobjects/tanks_sd/tracks_b.tga
	{
		map models/mapobjects/tanks_sd/tracks_b.tga
		//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag2_tracks_right
{
	qer_editorimage models/mapobjects/tanks_sd/tracks_b.tga
	{
		map models/mapobjects/tanks_sd/tracks_b.tga
		//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag2_wheels_alt_backward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_b.tga
		alphaFunc GE128
		tcMod rotate -75
	//rgbgen lightingDiffuse
				rgbGen vertex 
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag2_wheels_alt_forward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_b.tga
		alphaFunc GE128
		tcMod rotate -75
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag2_wheels_left
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel_b.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_b.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/jag2_wheels_right
{
	qer_alphafunc gequal 0.5
	qer_editorimage models/mapobjects/tanks_sd/wheel_b.tga
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_b.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel_backward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel.tga
		alphaFunc GE128
		tcMod rotate -60
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel_backward_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		tcMod rotate -60
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel_forward
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel.tga
		alphaFunc GE128
		tcMod rotate 60
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel_forward_oasis
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel.tga
		alphaFunc GE128
		tcMod rotate 90
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel_forward_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		tcMod rotate 60
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel_l
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel.tga
		alphaFunc GE128
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel_l_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse
				rgbGen vertex 
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel_r
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel_r_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel2_backward_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a.tga
		alphaFunc GE128
		tcMod rotate -60
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel2_forward_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a.tga
		alphaFunc GE128
		tcMod rotate 60
	//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel2_l_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse
				rgbGen vertex 
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/tanks_sd/wheel2_r_a
{
	qer_alphafunc gequal 0.5
	cull disable
	{
		clampmap models/mapobjects/tanks_sd/wheel2_a.tga
		alphaFunc GE128
		//rgbgen lightingDiffuse 
				rgbGen vertex
	}
//		{
//			map $lightmap
//			blendFunc filter
//			rgbGen identity
//		}
}

models/mapobjects/torture/glass
{
	{
		map models/mapobjects/test3/c_water2.tga
		blendFunc GL_DST_COLOR GL_ONE
		rgbgen identity
		tcmod scroll .05 .05
	}
	{
		map models/mapobjects/torture/glass.tga
		blendfunc blend
		rgbGen vertex
	}
}

models/mapobjects/tree/branch_slp1
{
	cull twosided
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/tree/branch_slp1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree/branch_slp2
{
	cull twosided
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/tree/branch_slp2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree/branch8
{
	cull twosided
	deformVertexes wave 194 sin 0 1 0 .4
	deformVertexes wave 194 sin 0 2 0 .1
	deformVertexes wave 30 sin 0 1 0 .3
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/tree/branch8.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree_desert_sd/palm_leaf1
{
	qer_alphafunc gequal 0.5
	cull twosided
	deformVertexes wave 15 sin 0 1 0 0.25
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/tree_desert_sd/palm_leaf1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree_desert_sd/palm_leaves
{
	qer_alphafunc gequal 0.5
	cull twosided
	deformVertexes wave 194 sin 0 1 0 .4
	deformVertexes wave 194 sin 0 2 0 .1
	deformVertexes wave 30 sin 0 1 0 .3
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/tree_desert_sd/palm_leaves.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree_desert_sd/palm_leaves2
{
	cull twosided
	deformVertexes wave 194 sin 0 1 0 .4
	deformVertexes wave 194 sin 0 2 0 .1
	deformVertexes wave 30 sin 0 1 0 .3
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/tree_desert_sd/palm_leaves2.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/tree_desert_sd/floorpalmleaf
{
	qer_editorimage models/mapobjects/tree_desert_sd/palm_leaf1.tga
	qer_alphafunc gequal 0.5
	cull twosided
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/tree_desert_sd/palm_leaf1.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/trees_sd/bush_s
{
	qer_alphafunc gequal 0.5
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm nonsolid
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/trees_sd/bush_s.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/trees_sd/winterbranch01
{
	qer_alphafunc gequal 0.5
	cull twosided
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/trees_sd/winterbranch01.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/trees_sd/wintertrunk01
{
	qer_alphafunc gequal 0.5
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm trans
	nopicmip
     	{
		map models/mapobjects/trees_sd/wintertrunk01.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/vehicles/sherman_s
{
	{
	map models/mapobjects/vehicles/sherman_s.tga
	blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	rgbGen identity
	}
	{
	map $lightmap
	rgbGen identity
	blendFunc GL_DST_COLOR GL_ZERO
	}
}

models/mapobjects/vehicles/truck_shadow
{
	{
	map models/mapobjects/vehicles/truck_shadow.tga
	blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/vehicles/truckside4bloop
{
	qer_editorimage models/mapobjects/vehicles/truckside4.tga
	cull twosided
     	{
		map models/mapobjects/vehicles/truckside4.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

// Lamp - Thomas C - 21 - 02 - 2003
//-----------------------------------------------------------
models/mapobjects/xlab_props/light_1
{
	qer_editorimage models/mapobjects/xlab_props/light.tga
      surfaceparm nomarks
      surfaceparm alphashadow
 	surfaceparm nolightmap
		{
		map models/mapobjects/xlab_props/light.tga
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/xlab_props/light_1_oasis
{
	qer_editorimage models/mapobjects/xlab_props/light.tga
      surfaceparm nomarks
      surfaceparm alphashadow
 	surfaceparm nolightmap
	surfaceparm trans
	{
		map models/mapobjects/xlab_props/light.tga
		rgbGen identity
	}
}

models/mapobjects/vehicles/wagon/wag_whl
{
	cull twosided
     	{
		map models/mapobjects/vehicles/wagon/wag_whl.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}


models/mapobjects/xp_chandelier/md_chand_arm
{
	cull none
     	{
		map models/mapobjects/xp_chandelier/md_chand_arm.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/xp_dino_skel/dino_alpha_bones
{
	cull none
     	{
		map models/mapobjects/xp_dino_skel/dino_alpha_bones.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/xp_dino_skel/dino_alpha_skull
{
	cull none
     	{
		map models/mapobjects/xp_dino_skel/dino_alpha_skull.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/xp_dino_skel/dino_bone
{
	cull none
     	{
		map models/mapobjects/xp_dino_skel/dino_bone.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/xp_sarcophagus/dsarcophagus
{
	{
		map textures/effects/tinfx.jpg
		rgbGen vertex
		tcGen environment
	}
	{
		map models/mapobjects/xp_sarcophagus/dsarcophagus.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

models/mapobjects/xp_sdkfz222/sdkfz222_3
{
	cull none
     	{
		map models/mapobjects/xp_sdkfz222/sdkfz222_3.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/xp_sdkfz222/sdkfz222_3gray
{
	cull none
     	{
		map models/mapobjects/xp_sdkfz222/sdkfz222_3gray.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}

models/mapobjects/xp_sdkfz222/sdkfz222_3gray_d
{
	cull none
     	{
		map models/mapobjects/xp_sdkfz222/sdkfz222_3gray_d.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		//rgbGen identity
		rgbGen vertex
		}
//		{
//		map $lightmap
//		rgbGen identity
//		blendFunc GL_DST_COLOR GL_ZERO
//		depthFunc equal
//		}
}
