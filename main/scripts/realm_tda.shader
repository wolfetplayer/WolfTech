//**************
// SKYBOX
//**************

textures/realm_tda/skybox
{
	qer_editorimage textures/realm_tda/ed_skybox.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_sun 0.05355 0.53440 0.87905 80 60 90
//	q3map_surfacelight 120
//	q3map_lightsuvdivide 256
	skyparms textures/realm_tda/env/zsky 512 -
}


//*************
// flare
//*************
textures/realm_tda/flare
{
	deformVertexes autoSprite

	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight

	cull none

	qer_editorimage gfx/misc/flare5.tga
	{
		map gfx/misc/flare5.tga
		blendFunc add
		rgbGen const ( 0.082 0.255 0.651 )
	}
}

//*************
// teleport
//*************

textures/realm_tda/telep
{ 
	cull none
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	q3map_forcesunlight
	sort 8
	{	        
		clampmap textures/realm_tda/telep.tga
		blendFunc add
		tcmod stretch triangle 1 0.05 0 13
		tcmod rotate 327
		rgbGen identitylighting
	}
	{
		clampmap textures/realm_tda/telep2.tga
		blendFunc add
//		tcmod stretch triangle 1 0.05 0.5 666
		tcmod rotate -211
		rgbGen identitylighting
	}
}

//************
// snake finger
//************
textures/realm_tda/snake_finger
{
	q3map_forcesunlight
	surfaceparm trans
//	surfaceparm lightfilter
	surfaceparm nonsolid
	surfaceparm nolightmap
//	cull none
	{
		map textures/realm_tda/snake_finger.tga
		rgbGen vertex
		tcMod Rotate 180
	}
}

//************
// beams
//************

textures/realm_tda/beam_red4
{
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda/beam_red4.tga
		tcMod Scroll .4 .1
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda/beam_red4_static
{
	qer_editorimage textures/realm_tda/beam_red4.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda/beam_red4.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda/floor_special
{
	qer_editorimage sprites/inferno_red.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none
	q3map_tcGen ivector ( 256 0 0 ) ( 0 256 0 )
	{
		map sprites/inferno_red.tga
		//map sprites/lightningGlow.tga
		tcMod Turb 1 .5 0 0.1
		tcGen environment
		blendFunc GL_ONE GL_ONE
		//rgbGen identityLighting
		//rgbGen const ( 0.25 0.25 0.25 )
		rgbGen identity
        }
}

textures/realm_tda/screw
{
	qer_editorimage textures/sfx/electricslime.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	//q3map_forcesunlight
	cull none
	{
		map textures/sfx/electricslime.tga
		tcMod Turb .5 .4 0 .6
		blendFunc GL_SRC_ALPHA GL_ONE
		//rgbGen vertex
		rgbgen identityLighting
		alphagen vertex
        }
}

textures/realm_tda/dqn_tongue
{
    qer_editorimage textures/realm_tda/dqn_tongue.tga
    qer_trans 0.35

    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    surfaceparm nomarks

    cull none
    tessSize 32

    // very subtle surface movement
    deformVertexes wave 1 sin 0 0.3 0 0.4

    // base layer
    {
        map textures/realm_tda/dqn_tongue.tga
        blendFunc GL_SRC_ALPHA GL_ONE
        rgbGen identity
        tcMod scroll 0.01 0.005
    }

    // soft energy motion layer
    {
        map textures/realm_tda/dqn_tongue.tga
        blendFunc GL_ONE GL_ONE
        tcMod scroll -0.008 0.004
        rgbGen wave sin 0.4 0.15 0 2
    }
}

//***************
// solids
//***************

textures/realm_tda/trim_shiney
{
	surfaceparm nolightmap
	{
		map textures/realm_tda/trim_env.tga
		tcGen environment
		rgbGen const ( 0.05355 0.53440 0.87905 )
	}   
	{
		map textures/realm_tda/trim_shiney.tga
		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		rgbGen vertex
	} 
}

textures/realm_tda/snake_floor
{
	surfaceparm nonsolid
	surfaceparm playerclip
	{
		map textures/castle_floor/floor_c03.tga
		rgbGen identity
	}
	{
		map $lightmap
		rgbGen identity
		blendFunc filter
	}
}

textures/realm_tda/title
{
	qer_trans 1.0
	
	surfaceparm nolightmap
	surfaceparm trans
	surfaceparm nonsolid
	
	nopicmip
	nomipmaps
	{
		map textures/realm_tda/title.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen identityLighting
	}
}

textures/realm_tda/dark_relic
{
	qer_trans 0.3
	
	qer_editorimage textures/liquids/water_blood_50.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm playerclip
	surfaceparm monsterclip
	//surfaceparm nolightmap
		cull disable
		deformVertexes wave 64 sin .5 .2 0 .5	
		{ 
			map textures/liquids/water_blood_50.tga
			blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
			rgbgen vertex
			tcmod scale .3 .5
			tcmod stretch 0 1 .01 0 .25 1
			tcmod scroll .050 .050
		}	

		{ 
			tcmod scale .4 .5
		}
	
		{
			map $lightmap
			blendFunc GL_dst_color GL_zero
			rgbgen identity		
		}

}

//
//
//  realm_tda_1
//
//

//**************
// SKYBOX
//**************

textures/realm_tda_1/skybox
{
	qer_editorimage textures/realm_tda_1/ed_skybox.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_sun 0.05355 0.53440 0.87905 80 60 90
//	q3map_surfacelight 120
//	q3map_lightsuvdivide 256
	skyparms textures/realm_tda_1/env/zsky 512 -
}


//*************
// flare
//*************
textures/realm_tda_1/flare
{
	deformVertexes autoSprite

	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight

	cull none

	qer_editorimage gfx/misc/flare5.tga
	{
		map gfx/misc/flare5.tga
		blendFunc add
		rgbGen const ( 0.082 0.255 0.651 )
	}
}

//*************
// teleport
//*************

textures/realm_tda_1/telep
{ 
	cull none
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	q3map_forcesunlight
	sort 8
	{	        
		clampmap textures/realm_tda_1/telep.tga
		blendFunc add
		tcmod stretch triangle 1 0.05 0 13
		tcmod rotate 327
		rgbGen identitylighting
	}
	{
		clampmap textures/realm_tda_1/telep2.tga
		blendFunc add
//		tcmod stretch triangle 1 0.05 0.5 666
		tcmod rotate -211
		rgbGen identitylighting
	}
}

//************
// snake finger
//************
textures/realm_tda_1/snake_finger
{
	q3map_forcesunlight
	surfaceparm trans
//	surfaceparm lightfilter
	surfaceparm nonsolid
	surfaceparm nolightmap
//	cull none
	{
		map textures/realm_tda_1/snake_finger.tga
		rgbGen vertex
		tcMod Rotate 180
	}
}

//************
// beams
//************

textures/realm_tda_1/beam_red4
{
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_1/beam_red4.tga
		tcMod Scroll .4 .1
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_1/beam_red4_static
{
	qer_editorimage textures/realm_tda_1/beam_red4.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_1/beam_red4.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_1/floor_special
{
	qer_editorimage sprites/inferno_red.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none
	q3map_tcGen ivector ( 256 0 0 ) ( 0 256 0 )
	{
		map sprites/inferno_red.tga
		//map sprites/lightningGlow.tga
		tcMod Turb 1 .5 0 0.1
		tcGen environment
		blendFunc GL_ONE GL_ONE
		//rgbGen identityLighting
		//rgbGen const ( 0.25 0.25 0.25 )
		rgbGen identity
        }
}

textures/realm_tda_1/screw
{
	qer_editorimage textures/sfx/electricslime.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	//q3map_forcesunlight
	cull none
	{
		map textures/sfx/electricslime.tga
		tcMod Turb .5 .4 0 .6
		blendFunc GL_SRC_ALPHA GL_ONE
		//rgbGen vertex
		rgbgen identityLighting
		alphagen vertex
        }
}
//***************
// snake tongue
//***************
textures/realm_tda_1/dqn_tongue
{
	qer_editorimage textures/realm_tda_1/dqn_tongue.tga
	qer_trans 0.3
	surfaceparm trans
	tessSize 16
	deformVertexes wave 194 sin 0 2 0 2
//	deformVertexes normal .5 .1
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none
	{
		map textures/realm_tda_1/dqn_tongue.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
        }
}

//***************
// solids
//***************

textures/realm_tda_1/trim_shiney
{
	surfaceparm nolightmap
	{
		map textures/realm_tda_1/trim_env.tga
		tcGen environment
		rgbGen const ( 0.05355 0.53440 0.87905 )
	}   
	{
		map textures/realm_tda_1/trim_shiney.tga
		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		rgbGen vertex
	} 
}

textures/realm_tda_1/snake_floor
{
	surfaceparm nonsolid
	surfaceparm playerclip
	{
		map textures/castle_floor/floor_c03.tga
		rgbGen identity
	}
	{
		map $lightmap
		rgbGen identity
		blendFunc filter
	}
}

//
//
//  realm_tda_2
//
//

//**************
// SKYBOX
//**************

textures/realm_tda_2/skybox
{
	qer_editorimage textures/realm_tda_2/ed_skybox.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_sun 0.05355 0.53440 0.87905 80 60 90
//	q3map_surfacelight 120
//	q3map_lightsuvdivide 256
	skyparms textures/realm_tda_2/env/zsky 512 -
}


//*************
// flare
//*************
textures/realm_tda_2/flare
{
	deformVertexes autoSprite

	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight

	cull none

	qer_editorimage gfx/misc/flare5.tga
	{
		map gfx/misc/flare5.tga
		blendFunc add
		rgbGen const ( 0.082 0.255 0.651 )
	}
}

//*************
// teleport
//*************

textures/realm_tda_2/telep
{ 
	cull none
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	q3map_forcesunlight
	sort 8
	{	        
		clampmap textures/realm_tda_2/telep.tga
		blendFunc add
		tcmod stretch triangle 1 0.05 0 13
		tcmod rotate 327
		rgbGen identitylighting
	}
	{
		clampmap textures/realm_tda_2/telep2.tga
		blendFunc add
//		tcmod stretch triangle 1 0.05 0.5 666
		tcmod rotate -211
		rgbGen identitylighting
	}
}

//************
// snake finger
//************
textures/realm_tda_2/snake_finger
{
	q3map_forcesunlight
	surfaceparm trans
//	surfaceparm lightfilter
	surfaceparm nonsolid
	surfaceparm nolightmap
//	cull none
	{
		map textures/realm_tda_2/snake_finger.tga
		rgbGen vertex
		tcMod Rotate 180
	}
}

//************
// beams
//************

textures/realm_tda_2/beam_red4
{
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_2/beam_red4.tga
		tcMod Scroll .4 .1
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_2/beam_red4_static
{
	qer_editorimage textures/realm_tda_2/beam_red4.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_2/beam_red4.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_2/floor_special
{
	qer_editorimage sprites/inferno_red.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm playerclip
	surfaceparm monsterclip
	surfaceparm nolightmap
	cull none
	q3map_tcGen ivector ( 256 0 0 ) ( 0 256 0 )
	{
		map sprites/inferno_red.tga
		//map sprites/lightningGlow.tga
		tcMod Turb 1 .5 0 0.1
		tcGen environment
		blendFunc GL_ONE GL_ONE
		//rgbGen identityLighting
		//rgbGen const ( 0.25 0.25 0.25 )
		rgbGen identity
        }
}

textures/realm_tda_2/screw
{
	qer_editorimage textures/sfx/electricslime.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	//q3map_forcesunlight
	cull none
	{
		map textures/sfx/electricslime.tga
		tcMod Turb .5 .4 0 .6
		blendFunc GL_SRC_ALPHA GL_ONE
		//rgbGen vertex
		rgbgen identityLighting
		alphagen vertex
        }
}
//***************
// snake tongue
//***************
textures/realm_tda_2/dqn_tongue
{
	qer_editorimage textures/realm_tda_2/dqn_tongue.tga
	qer_trans 0.3
	//surfaceparm trans
	//tessSize 16
	//deformVertexes wave 1 sin 0 1 0 1
	//surfaceparm nonsolid
	//surfaceparm nolightmap
	//cull none
	{
		map textures/realm_tda_2/dqn_tongue.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
        }
}

//***************
// solids
//***************

textures/realm_tda_2/trim_shiney
{
	surfaceparm nolightmap
	{
		map textures/realm_tda_2/trim_env.tga
		tcGen environment
		rgbGen const ( 0.05355 0.53440 0.87905 )
	}   
	{
		map textures/realm_tda_2/trim_shiney.tga
		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		rgbGen vertex
	} 
}

textures/realm_tda_2/snake_floor
{
	surfaceparm nonsolid
	surfaceparm playerclip
	{
		map textures/castle_floor/floor_c03.tga
		rgbGen identity
	}
	{
		map $lightmap
		rgbGen identity
		blendFunc filter
	}
}

//
//
//  realm_tda_3
//
//

//**************
// SKYBOX
//**************

textures/realm_tda_3/skybox
{
	qer_editorimage textures/realm_tda_3/ed_skybox.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_sun 0.05355 0.53440 0.87905 80 60 90
//	q3map_surfacelight 120
//	q3map_lightsuvdivide 256
	skyparms textures/realm_tda_3/env/zsky 512 -
}


//*************
// flare
//*************
textures/realm_tda_3/flare
{
	deformVertexes autoSprite

	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight

	cull none

	qer_editorimage gfx/misc/flare5.tga
	{
		map gfx/misc/flare5.tga
		blendFunc add
		rgbGen const ( 0.082 0.255 0.651 )
	}
}

//*************
// teleport
//*************

textures/realm_tda_3/telep
{ 
	cull none
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	q3map_forcesunlight
	sort 8
	{	        
		clampmap textures/realm_tda_3/telep.tga
		blendFunc add
		tcmod stretch triangle 1 0.05 0 13
		tcmod rotate 327
		rgbGen identitylighting
	}
	{
		clampmap textures/realm_tda_3/telep2.tga
		blendFunc add
//		tcmod stretch triangle 1 0.05 0.5 666
		tcmod rotate -211
		rgbGen identitylighting
	}
}

//************
// snake finger
//************
textures/realm_tda_3/snake_finger
{
	q3map_forcesunlight
	surfaceparm trans
//	surfaceparm lightfilter
	surfaceparm nonsolid
	surfaceparm nolightmap
//	cull none
	{
		map textures/realm_tda_3/snake_finger.tga
		rgbGen vertex
		tcMod Rotate 180
	}
}

//************
// beams
//************

textures/realm_tda_3/beam_red4
{
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_3/beam_red4.tga
		tcMod Scroll .4 .1
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_3/beam_red4_static
{
	qer_editorimage textures/realm_tda_3/beam_red4.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_3/beam_red4.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_3/floor_special
{
	qer_editorimage sprites/inferno_red.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm playerclip
	surfaceparm monsterclip
	surfaceparm nolightmap
	cull none
	q3map_tcGen ivector ( 256 0 0 ) ( 0 256 0 )
	{
		map sprites/inferno_red.tga
		//map sprites/lightningGlow.tga
		tcMod Turb 1 .5 0 0.1
		tcGen environment
		blendFunc GL_ONE GL_ONE
		//rgbGen identityLighting
		//rgbGen const ( 0.25 0.25 0.25 )
		rgbGen identity
        }
}

textures/realm_tda_3/screw
{
	qer_editorimage textures/sfx/electricslime.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	//q3map_forcesunlight
	cull none
	{
		map textures/sfx/electricslime.tga
		tcMod Turb .5 .4 0 .6
		blendFunc GL_SRC_ALPHA GL_ONE
		//rgbGen vertex
		rgbgen identityLighting
		alphagen vertex
        }
}
//***************
// snake tongue
//***************
textures/realm_tda_3/dqn_tongue
{
	qer_editorimage textures/realm_tda_3/dqn_tongue.tga
	qer_trans 0.3
	surfaceparm trans
	tessSize 16
	deformVertexes wave 194 sin 0 2 0 2
//	deformVertexes normal .5 .1
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none
	{
		map textures/realm_tda_3/dqn_tongue.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
        }
}

//***************
// solids
//***************

textures/realm_tda_3/trim_shiney
{
	surfaceparm nolightmap
	{
		map textures/realm_tda_3/trim_env.tga
		tcGen environment
		rgbGen const ( 0.05355 0.53440 0.87905 )
	}   
	{
		map textures/realm_tda_3/trim_shiney.tga
		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		rgbGen vertex
	} 
}

textures/realm_tda_3/snake_floor
{
	surfaceparm nonsolid
	surfaceparm playerclip
	{
		map textures/castle_floor/floor_c03.tga
		rgbGen identity
	}
	{
		map $lightmap
		rgbGen identity
		blendFunc filter
	}
}

//
//
//  realm_tda_4
//
//

//**************
// SKYBOX
//**************

textures/realm_tda_4/skybox
{
	qer_editorimage textures/realm_tda_4/ed_skybox.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_sun 0.05355 0.53440 0.87905 80 60 90
//	q3map_surfacelight 120
//	q3map_lightsuvdivide 256
	skyparms textures/realm_tda_4/env/zsky 512 -
}


//*************
// flare
//*************
textures/realm_tda_4/flare
{
	deformVertexes autoSprite

	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight

	cull none

	qer_editorimage gfx/misc/flare5.tga
	{
		map gfx/misc/flare5.tga
		blendFunc add
		rgbGen const ( 0.082 0.255 0.651 )
	}
}

//*************
// teleport
//*************

textures/realm_tda_4/telep
{ 
	cull none
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	q3map_forcesunlight
	sort 8
	{	        
		clampmap textures/realm_tda_4/telep.tga
		blendFunc add
		tcmod stretch triangle 1 0.05 0 13
		tcmod rotate 327
		rgbGen identitylighting
	}
	{
		clampmap textures/realm_tda_4/telep2.tga
		blendFunc add
//		tcmod stretch triangle 1 0.05 0.5 666
		tcmod rotate -211
		rgbGen identitylighting
	}
}

//************
// snake finger
//************
textures/realm_tda_4/snake_finger
{
	q3map_forcesunlight
	surfaceparm trans
//	surfaceparm lightfilter
	surfaceparm nonsolid
	surfaceparm nolightmap
//	cull none
	{
		map textures/realm_tda_4/snake_finger.tga
		rgbGen vertex
		tcMod Rotate 180
	}
}

//************
// beams
//************

textures/realm_tda_4/beam_red4
{
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_4/beam_red4.tga
		tcMod Scroll .4 .1
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_4/beam_red4_static
{
	qer_editorimage textures/realm_tda_4/beam_red4.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_4/beam_red4.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_4/floor_special
{
	qer_editorimage sprites/inferno_red.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm playerclip
	surfaceparm monsterclip
	surfaceparm nolightmap
	cull none
	q3map_tcGen ivector ( 256 0 0 ) ( 0 256 0 )
	{
		map sprites/inferno_red.tga
		//map sprites/lightningGlow.tga
		tcMod Turb 1 .5 0 0.1
		tcGen environment
		blendFunc GL_ONE GL_ONE
		//rgbGen identityLighting
		//rgbGen const ( 0.25 0.25 0.25 )
		rgbGen identity
        }
}

textures/realm_tda_4/screw
{
	qer_editorimage textures/sfx/electricslime.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	//q3map_forcesunlight
	cull none
	{
		map textures/sfx/electricslime.tga
		tcMod Turb .5 .4 0 .6
		blendFunc GL_SRC_ALPHA GL_ONE
		//rgbGen vertex
		rgbgen identityLighting
		alphagen vertex
        }
}
//***************
// snake tongue
//***************
textures/realm_tda_4/dqn_tongue
{
	qer_editorimage textures/realm_tda_4/dqn_tongue.tga
	qer_trans 0.3
	surfaceparm trans
	tessSize 16
	deformVertexes wave 194 sin 0 2 0 2
//	deformVertexes normal .5 .1
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none
	{
		map textures/realm_tda_4/dqn_tongue.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
        }
}

//***************
// solids
//***************

textures/realm_tda_4/trim_shiney
{
	surfaceparm nolightmap
	{
		map textures/realm_tda_4/trim_env.tga
		tcGen environment
		rgbGen const ( 0.05355 0.53440 0.87905 )
	}   
	{
		map textures/realm_tda_4/trim_shiney.tga
		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		rgbGen vertex
	} 
}

textures/realm_tda_4/snake_floor
{
	surfaceparm nonsolid
	surfaceparm playerclip
	{
		map textures/castle_floor/floor_c03.tga
		rgbGen identity
	}
	{
		map $lightmap
		rgbGen identity
		blendFunc filter
	}
}

//
//
//  realm_tda_5
//
//

//**************
// SKYBOX
//**************

textures/realm_tda_5/skybox
{
	qer_editorimage textures/realm_tda_5/ed_skybox.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_sun 0.05355 0.53440 0.87905 80 60 90
//	q3map_surfacelight 120
//	q3map_lightsuvdivide 256
	skyparms textures/realm_tda_5/env/zsky 512 -
}


//*************
// flare
//*************
textures/realm_tda_5/flare
{
	deformVertexes autoSprite

	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight

	cull none

	qer_editorimage gfx/misc/flare5.tga
	{
		map gfx/misc/flare5.tga
		blendFunc add
		rgbGen const ( 0.082 0.255 0.651 )
	}
}

//*************
// teleport
//*************

textures/realm_tda_5/telep
{ 
	cull none
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	q3map_forcesunlight
	sort 8
	{	        
		clampmap textures/realm_tda_5/telep.tga
		blendFunc add
		tcmod stretch triangle 1 0.05 0 13
		tcmod rotate 327
		rgbGen identitylighting
	}
	{
		clampmap textures/realm_tda_5/telep2.tga
		blendFunc add
//		tcmod stretch triangle 1 0.05 0.5 666
		tcmod rotate -211
		rgbGen identitylighting
	}
}

//************
// snake finger
//************
textures/realm_tda_5/snake_finger
{
	q3map_forcesunlight
	surfaceparm trans
//	surfaceparm lightfilter
	surfaceparm nonsolid
	surfaceparm nolightmap
//	cull none
	{
		map textures/realm_tda_5/snake_finger.tga
		rgbGen vertex
		tcMod Rotate 180
	}
}

//************
// beams
//************

textures/realm_tda_5/beam_red4
{
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_5/beam_red4.tga
		tcMod Scroll .4 .1
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_5/beam_red4_static
{
	qer_editorimage textures/realm_tda_5/beam_red4.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_5/beam_red4.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_5/floor_special
{
	qer_editorimage sprites/inferno_red.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm playerclip
	surfaceparm monsterclip
	surfaceparm nolightmap
	cull none
	q3map_tcGen ivector ( 256 0 0 ) ( 0 256 0 )
	{
		map sprites/inferno_red.tga
		//map sprites/lightningGlow.tga
		tcMod Turb 1 .5 0 0.1
		tcGen environment
		blendFunc GL_ONE GL_ONE
		//rgbGen identityLighting
		//rgbGen const ( 0.25 0.25 0.25 )
		rgbGen identity
        }
}

textures/realm_tda_5/screw
{
	qer_editorimage textures/sfx/electricslime.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	//q3map_forcesunlight
	cull none
	{
		map textures/sfx/electricslime.tga
		tcMod Turb .5 .4 0 .6
		blendFunc GL_SRC_ALPHA GL_ONE
		//rgbGen vertex
		rgbgen identityLighting
		alphagen vertex
        }
}
//***************
// snake tongue
//***************
textures/realm_tda_5/dqn_tongue
{
	qer_editorimage textures/realm_tda_5/dqn_tongue.tga
	qer_trans 0.3
	surfaceparm trans
	tessSize 16
	deformVertexes wave 194 sin 0 2 0 2
//	deformVertexes normal .5 .1
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none
	{
		map textures/realm_tda_5/dqn_tongue.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
        }
}

//***************
// solids
//***************

textures/realm_tda_5/trim_shiney
{
	surfaceparm nolightmap
	{
		map textures/realm_tda_5/trim_env.tga
		tcGen environment
		rgbGen const ( 0.05355 0.53440 0.87905 )
	}   
	{
		map textures/realm_tda_5/trim_shiney.tga
		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		rgbGen vertex
	} 
}

textures/realm_tda_5/snake_floor
{
	surfaceparm nonsolid
	surfaceparm playerclip
	{
		map textures/castle_floor/floor_c03.tga
		rgbGen identity
	}
	{
		map $lightmap
		rgbGen identity
		blendFunc filter
	}
}

//
//
//  realm_tda_6
//
//

//**************
// SKYBOX
//**************

textures/realm_tda_6/skybox
{
	qer_editorimage textures/realm_tda_6/ed_skybox.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_sun 0.05355 0.53440 0.87905 80 60 90
//	q3map_surfacelight 120
//	q3map_lightsuvdivide 256
	skyparms textures/realm_tda_6/env/zsky 512 -
}


//*************
// flare
//*************
textures/realm_tda_6/flare
{
	deformVertexes autoSprite

	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight

	cull none

	qer_editorimage gfx/misc/flare5.tga
	{
		map gfx/misc/flare5.tga
		blendFunc add
		rgbGen const ( 0.082 0.255 0.651 )
	}
}

//*************
// teleport
//*************

textures/realm_tda_6/telep
{ 
	cull none
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	q3map_forcesunlight
	sort 8
	{	        
		clampmap textures/realm_tda_6/telep.tga
		blendFunc add
		tcmod stretch triangle 1 0.05 0 13
		tcmod rotate 327
		rgbGen identitylighting
	}
	{
		clampmap textures/realm_tda_6/telep2.tga
		blendFunc add
//		tcmod stretch triangle 1 0.05 0.5 666
		tcmod rotate -211
		rgbGen identitylighting
	}
}

//************
// snake finger
//************
textures/realm_tda_6/snake_finger
{
	q3map_forcesunlight
	surfaceparm trans
//	surfaceparm lightfilter
	surfaceparm nonsolid
	surfaceparm nolightmap
//	cull none
	{
		map textures/realm_tda_6/snake_finger.tga
		rgbGen vertex
		tcMod Rotate 180
	}
}

//************
// beams
//************

textures/realm_tda_6/beam_red4
{
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_6/beam_red4.tga
		tcMod Scroll .4 .1
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_6/beam_red4_static
{
	qer_editorimage textures/realm_tda_6/beam_red4.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_6/beam_red4.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_6/floor_special
{
	qer_editorimage sprites/inferno_red.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm playerclip
	surfaceparm monsterclip
	surfaceparm nolightmap
	cull none
	q3map_tcGen ivector ( 256 0 0 ) ( 0 256 0 )
	{
		map sprites/inferno_red.tga
		//map sprites/lightningGlow.tga
		tcMod Turb 1 .5 0 0.1
		tcGen environment
		blendFunc GL_ONE GL_ONE
		//rgbGen identityLighting
		//rgbGen const ( 0.25 0.25 0.25 )
		rgbGen identity
        }
}

textures/realm_tda_6/screw
{
	qer_editorimage textures/sfx/electricslime.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	//q3map_forcesunlight
	cull none
	{
		map textures/sfx/electricslime.tga
		tcMod Turb .5 .4 0 .6
		blendFunc GL_SRC_ALPHA GL_ONE
		//rgbGen vertex
		rgbgen identityLighting
		alphagen vertex
        }
}
//***************
// snake tongue
//***************
textures/realm_tda_6/dqn_tongue
{
	qer_editorimage textures/realm_tda_6/dqn_tongue.tga
	qer_trans 0.3
	surfaceparm trans
	tessSize 16
	deformVertexes wave 194 sin 0 2 0 2
//	deformVertexes normal .5 .1
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none
	{
		map textures/realm_tda_6/dqn_tongue.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
        }
}

//***************
// solids
//***************

textures/realm_tda_6/trim_shiney
{
	surfaceparm nolightmap
	{
		map textures/realm_tda_6/trim_env.tga
		tcGen environment
		rgbGen const ( 0.05355 0.53440 0.87905 )
	}   
	{
		map textures/realm_tda_6/trim_shiney.tga
		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		rgbGen vertex
	} 
}

textures/realm_tda_6/snake_floor
{
	surfaceparm nonsolid
	surfaceparm playerclip
	{
		map textures/castle_floor/floor_c03.tga
		rgbGen identity
	}
	{
		map $lightmap
		rgbGen identity
		blendFunc filter
	}
}

//
//
//  realm_tda_7
//
//

//**************
// SKYBOX
//**************

textures/realm_tda_7/skybox
{
	qer_editorimage textures/realm_tda_7/ed_skybox.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_sun 0.05355 0.53440 0.87905 80 60 90
//	q3map_surfacelight 120
//	q3map_lightsuvdivide 256
	skyparms textures/realm_tda_7/env/zsky 512 -
}


//*************
// flare
//*************
textures/realm_tda_7/flare
{
	deformVertexes autoSprite

	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight

	cull none

	qer_editorimage gfx/misc/flare5.tga
	{
		map gfx/misc/flare5.tga
		blendFunc add
		rgbGen const ( 0.082 0.255 0.651 )
	}
}

//*************
// teleport
//*************

textures/realm_tda_7/telep
{ 
	cull none
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	q3map_forcesunlight
	sort 8
	{	        
		clampmap textures/realm_tda_7/telep.tga
		blendFunc add
		tcmod stretch triangle 1 0.05 0 13
		tcmod rotate 327
		rgbGen identitylighting
	}
	{
		clampmap textures/realm_tda_7/telep2.tga
		blendFunc add
//		tcmod stretch triangle 1 0.05 0.5 666
		tcmod rotate -211
		rgbGen identitylighting
	}
}

//************
// snake finger
//************
textures/realm_tda_7/snake_finger
{
	q3map_forcesunlight
	surfaceparm trans
//	surfaceparm lightfilter
	surfaceparm nonsolid
	surfaceparm nolightmap
//	cull none
	{
		map textures/realm_tda_7/snake_finger.tga
		rgbGen vertex
		tcMod Rotate 180
	}
}

//************
// beams
//************

textures/realm_tda_7/beam_red4
{
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_7/beam_red4.tga
		tcMod Scroll .4 .1
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_7/beam_red4_static
{
	qer_editorimage textures/realm_tda_7/beam_red4.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_7/beam_red4.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_7/floor_special
{
	qer_editorimage sprites/inferno_red.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm playerclip
	surfaceparm monsterclip
	surfaceparm nolightmap
	cull none
	q3map_tcGen ivector ( 256 0 0 ) ( 0 256 0 )
	{
		map sprites/inferno_red.tga
		//map sprites/lightningGlow.tga
		tcMod Turb 1 .5 0 0.1
		tcGen environment
		blendFunc GL_ONE GL_ONE
		//rgbGen identityLighting
		//rgbGen const ( 0.25 0.25 0.25 )
		rgbGen identity
        }
}

textures/realm_tda_7/screw
{
	qer_editorimage textures/sfx/electricslime.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	//q3map_forcesunlight
	cull none
	{
		map textures/sfx/electricslime.tga
		tcMod Turb .5 .4 0 .6
		blendFunc GL_SRC_ALPHA GL_ONE
		//rgbGen vertex
		rgbgen identityLighting
		alphagen vertex
        }
}
//***************
// snake tongue
//***************
textures/realm_tda_7/dqn_tongue
{
	qer_editorimage textures/realm_tda_7/dqn_tongue.tga
	qer_trans 0.3
	surfaceparm trans
	tessSize 16
	//deformVertexes wave 194 sin 0 2 0 2
//	deformVertexes normal .5 .1
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none
	{
		map textures/realm_tda_7/dqn_tongue.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
        }
}

//***************
// solids
//***************

textures/realm_tda_7/trim_shiney
{
	surfaceparm nolightmap
	{
		map textures/realm_tda_7/trim_env.tga
		tcGen environment
		rgbGen const ( 0.05355 0.53440 0.87905 )
	}   
	{
		map textures/realm_tda_7/trim_shiney.tga
		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		rgbGen vertex
	} 
}

textures/realm_tda_7/snake_floor
{
	surfaceparm nonsolid
	surfaceparm playerclip
	{
		map textures/castle_floor/floor_c03.tga
		rgbGen identity
	}
	{
		map $lightmap
		rgbGen identity
		blendFunc filter
	}
}

//
//
//  realm_tda_8
//
//

//**************
// SKYBOX
//**************

textures/realm_tda_8/skybox
{
	qer_editorimage textures/realm_tda_8/ed_skybox.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_sun 0.05355 0.53440 0.87905 80 60 90
//	q3map_surfacelight 120
//	q3map_lightsuvdivide 256
	skyparms textures/realm_tda_8/env/zsky 512 -
}


//*************
// flare
//*************
textures/realm_tda_8/flare
{
	deformVertexes autoSprite

	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight

	cull none

	qer_editorimage gfx/misc/flare5.tga
	{
		map gfx/misc/flare5.tga
		blendFunc add
		rgbGen const ( 0.082 0.255 0.651 )
	}
}

//*************
// teleport
//*************

textures/realm_tda_8/telep
{ 
	cull none
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	q3map_forcesunlight
	sort 8
	{	        
		clampmap textures/realm_tda_8/telep.tga
		blendFunc add
		tcmod stretch triangle 1 0.05 0 13
		tcmod rotate 327
		rgbGen identitylighting
	}
	{
		clampmap textures/realm_tda_8/telep2.tga
		blendFunc add
//		tcmod stretch triangle 1 0.05 0.5 666
		tcmod rotate -211
		rgbGen identitylighting
	}
}

//************
// snake finger
//************
textures/realm_tda_8/snake_finger
{
	q3map_forcesunlight
	surfaceparm trans
//	surfaceparm lightfilter
	surfaceparm nonsolid
	surfaceparm nolightmap
//	cull none
	{
		map textures/realm_tda_8/snake_finger.tga
		rgbGen vertex
		tcMod Rotate 180
	}
}

//************
// beams
//************

textures/realm_tda_8/beam_red4
{
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_8/beam_red4.tga
		tcMod Scroll .4 .1
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_8/beam_red4_static
{
	qer_editorimage textures/realm_tda_8/beam_red4.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_8/beam_red4.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_8/floor_special
{
	qer_editorimage sprites/inferno_red.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm playerclip
	surfaceparm monsterclip
	surfaceparm nolightmap
	cull none
	q3map_tcGen ivector ( 256 0 0 ) ( 0 256 0 )
	{
		map sprites/inferno_red.tga
		//map sprites/lightningGlow.tga
		tcMod Turb 1 .5 0 0.1
		tcGen environment
		blendFunc GL_ONE GL_ONE
		//rgbGen identityLighting
		//rgbGen const ( 0.25 0.25 0.25 )
		rgbGen identity
        }
}

textures/realm_tda_8/screw
{
	qer_editorimage textures/sfx/electricslime.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	//q3map_forcesunlight
	cull none
	{
		map textures/sfx/electricslime.tga
		tcMod Turb .5 .4 0 .6
		blendFunc GL_SRC_ALPHA GL_ONE
		//rgbGen vertex
		rgbgen identityLighting
		alphagen vertex
        }
}
//***************
// snake tongue
//***************
textures/realm_tda_8/dqn_tongue
{
	qer_editorimage textures/realm_tda_8/dqn_tongue.tga
	qer_trans 0.3
	surfaceparm trans
	tessSize 16
	deformVertexes wave 194 sin 0 2 0 2
//	deformVertexes normal .5 .1
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none
	{
		map textures/realm_tda_8/dqn_tongue.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
        }
}

//***************
// solids
//***************

textures/realm_tda_8/trim_shiney
{
	surfaceparm nolightmap
	{
		map textures/realm_tda_8/trim_env.tga
		tcGen environment
		rgbGen const ( 0.05355 0.53440 0.87905 )
	}   
	{
		map textures/realm_tda_8/trim_shiney.tga
		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		rgbGen vertex
	} 
}

textures/realm_tda_8/snake_floor
{
	surfaceparm nonsolid
	surfaceparm playerclip
	{
		map textures/castle_floor/floor_c03.tga
		rgbGen identity
	}
	{
		map $lightmap
		rgbGen identity
		blendFunc filter
	}
}

//
//
//  realm_tda_9
//
//

//**************
// SKYBOX
//**************

textures/realm_tda_9/skybox
{
	qer_editorimage textures/realm_tda_9/ed_skybox.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_sun 0.05355 0.53440 0.87905 80 60 90
//	q3map_surfacelight 120
//	q3map_lightsuvdivide 256
	skyparms textures/realm_tda_9/env/zsky 512 -
}


//*************
// flare
//*************
textures/realm_tda_9/flare
{
	deformVertexes autoSprite

	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight

	cull none

	qer_editorimage gfx/misc/flare5.tga
	{
		map gfx/misc/flare5.tga
		blendFunc add
		rgbGen const ( 0.082 0.255 0.651 )
	}
}

//*************
// teleport
//*************

textures/realm_tda_9/telep
{ 
	cull none
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	q3map_forcesunlight
	sort 8
	{	        
		clampmap textures/realm_tda_9/telep.tga
		blendFunc add
		tcmod stretch triangle 1 0.05 0 13
		tcmod rotate 327
		rgbGen identitylighting
	}
	{
		clampmap textures/realm_tda_9/telep2.tga
		blendFunc add
//		tcmod stretch triangle 1 0.05 0.5 666
		tcmod rotate -211
		rgbGen identitylighting
	}
}

//************
// snake finger
//************
textures/realm_tda_9/snake_finger
{
	q3map_forcesunlight
	surfaceparm trans
//	surfaceparm lightfilter
	surfaceparm nonsolid
	surfaceparm nolightmap
//	cull none
	{
		map textures/realm_tda_9/snake_finger.tga
		rgbGen vertex
		tcMod Rotate 180
	}
}

//************
// beams
//************

textures/realm_tda_9/beam_red4
{
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_9/beam_red4.tga
		tcMod Scroll .4 .1
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_9/beam_red4_static
{
	qer_editorimage textures/realm_tda_9/beam_red4.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_9/beam_red4.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_9/floor_special
{
	qer_editorimage sprites/inferno_red.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm playerclip
	surfaceparm monsterclip
	surfaceparm nolightmap
	cull none
	q3map_tcGen ivector ( 256 0 0 ) ( 0 256 0 )
	{
		map sprites/inferno_red.tga
		//map sprites/lightningGlow.tga
		tcMod Turb 1 .5 0 0.1
		tcGen environment
		blendFunc GL_ONE GL_ONE
		//rgbGen identityLighting
		//rgbGen const ( 0.25 0.25 0.25 )
		rgbGen identity
        }
}

textures/realm_tda_9/screw
{
	qer_editorimage textures/sfx/electricslime.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	//q3map_forcesunlight
	cull none
	{
		map textures/sfx/electricslime.tga
		tcMod Turb .5 .4 0 .6
		blendFunc GL_SRC_ALPHA GL_ONE
		//rgbGen vertex
		rgbgen identityLighting
		alphagen vertex
        }
}
//***************
// snake tongue
//***************
textures/realm_tda_9/dqn_tongue
{
	qer_editorimage textures/realm_tda_9/dqn_tongue.tga
	qer_trans 0.3
	surfaceparm trans
	tessSize 16
	deformVertexes wave 194 sin 0 2 0 2
//	deformVertexes normal .5 .1
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none
	{
		map textures/realm_tda_9/dqn_tongue.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
        }
}

//***************
// solids
//***************

textures/realm_tda_9/trim_shiney
{
	surfaceparm nolightmap
	{
		map textures/realm_tda_9/trim_env.tga
		tcGen environment
		rgbGen const ( 0.05355 0.53440 0.87905 )
	}   
	{
		map textures/realm_tda_9/trim_shiney.tga
		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		rgbGen vertex
	} 
}

textures/realm_tda_9/snake_floor
{
	surfaceparm nonsolid
	surfaceparm playerclip
	{
		map textures/castle_floor/floor_c03.tga
		rgbGen identity
	}
	{
		map $lightmap
		rgbGen identity
		blendFunc filter
	}
}

//
//
//  realm_tda_10
//
//

//**************
// SKYBOX
//**************

textures/realm_tda_10/skybox
{
	qer_editorimage textures/realm_tda_10/ed_skybox.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_sun 0.05355 0.53440 0.87905 80 60 90
//	q3map_surfacelight 120
//	q3map_lightsuvdivide 256
	skyparms textures/realm_tda_10/env/zsky 512 -
}


//*************
// flare
//*************
textures/realm_tda_10/flare
{
	deformVertexes autoSprite

	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight

	cull none

	qer_editorimage gfx/misc/flare5.tga
	{
		map gfx/misc/flare5.tga
		blendFunc add
		rgbGen const ( 0.082 0.255 0.651 )
	}
}

//*************
// teleport
//*************

textures/realm_tda_10/telep
{ 
	cull none
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	q3map_forcesunlight
	sort 8
	{	        
		clampmap textures/realm_tda_10/telep.tga
		blendFunc add
		tcmod stretch triangle 1 0.05 0 13
		tcmod rotate 327
		rgbGen identitylighting
	}
	{
		clampmap textures/realm_tda_10/telep2.tga
		blendFunc add
//		tcmod stretch triangle 1 0.05 0.5 666
		tcmod rotate -211
		rgbGen identitylighting
	}
}

//************
// snake finger
//************
textures/realm_tda_10/snake_finger
{
	q3map_forcesunlight
	surfaceparm trans
//	surfaceparm lightfilter
	surfaceparm nonsolid
	surfaceparm nolightmap
//	cull none
	{
		map textures/realm_tda_10/snake_finger.tga
		rgbGen vertex
		tcMod Rotate 180
	}
}

//************
// beams
//************

textures/realm_tda_10/beam_red4
{
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_10/beam_red4.tga
		tcMod Scroll .4 .1
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_10/beam_red4_static
{
	qer_editorimage textures/realm_tda_10/beam_red4.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm nonsolid
	//surfaceparm playerclip
	surfaceparm nodamage
	surfaceparm nosteps
	cull none
	q3map_forcesunlight
	{
		map textures/realm_tda_10/beam_red4.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

textures/realm_tda_10/floor_special
{
	qer_editorimage sprites/inferno_red.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nodamage
	surfaceparm nonsolid
	surfaceparm playerclip
	surfaceparm monsterclip
	surfaceparm nolightmap
	cull none
	q3map_tcGen ivector ( 256 0 0 ) ( 0 256 0 )
	{
		map sprites/inferno_red.tga
		//map sprites/lightningGlow.tga
		tcMod Turb 1 .5 0 0.1
		tcGen environment
		blendFunc GL_ONE GL_ONE
		//rgbGen identityLighting
		//rgbGen const ( 0.25 0.25 0.25 )
		rgbGen identity
        }
}

textures/realm_tda_10/screw
{
	qer_editorimage textures/sfx/electricslime.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	//q3map_forcesunlight
	cull none
	{
		map textures/sfx/electricslime.tga
		tcMod Turb .5 .4 0 .6
		blendFunc GL_SRC_ALPHA GL_ONE
		//rgbGen vertex
		rgbgen identityLighting
		alphagen vertex
        }
}
//***************
// snake tongue
//***************
textures/realm_tda_10/dqn_tongue
{
	qer_editorimage textures/realm_tda_10/dqn_tongue.tga
	qer_trans 0.3
	surfaceparm trans
	tessSize 16
	deformVertexes wave 194 sin 0 2 0 2
//	deformVertexes normal .5 .1
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none
	{
		map textures/realm_tda_10/dqn_tongue.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
        }
}

//***************
// solids
//***************

textures/realm_tda_10/trim_shiney
{
	surfaceparm nolightmap
	{
		map textures/realm_tda_10/trim_env.tga
		tcGen environment
		rgbGen const ( 0.05355 0.53440 0.87905 )
	}   
	{
		map textures/realm_tda_10/trim_shiney.tga
		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		rgbGen vertex
	} 
}

textures/realm_tda_10/snake_floor
{
	surfaceparm nonsolid
	surfaceparm playerclip
	{
		map textures/castle_floor/floor_c03.tga
		rgbGen identity
	}
	{
		map $lightmap
		rgbGen identity
		blendFunc filter
	}
}