
textures/survival_misc/sphere_red
{
	deformVertexes wave .1 sin 0.5 0 0 0
	{
		map textures/survival_misc/sphere_red.jpg
		blendfunc GL_ONE GL_ONE
		tcGen environment
		qer_trans 0.3
	    surfaceparm trans
	    surfaceparm nonsolid
	    surfaceparm nolightmap
	    cull none
                tcmod rotate 1
                tcmod scroll 1 .1
	}
}

textures/sfx/wpn_upgrade_red

    {
        map textures/sfx/wpn_upgrade_red.tga 
        blendFunc GL_SRC_ALPHA GL_ONE 
        rgbGen const ( 0.25 0.25 0.25 )
        tcGen environment 
        tcMod scroll 0.025 -0.07625
    }
	
textures/village/villwin_c15_fixed
{
    qer_editorimage textures/village/villwin_c15.tga
    q3map_surfacelight 300

    // 1) OPAQUE base (this is the key!)
    {
        map textures/village/villwin_c15.tga
        rgbGen identity
    }

    // 2) Lightmap multiply, but only where base already drew
    {
        map $lightmap
        blendFunc GL_DST_COLOR GL_ZERO
        depthFunc equal
        rgbGen identity
    }

    // 3) Extra glow/reflection/etc, also depthFunc equal
    {
        map textures/village/villwin_c15.blend.tga
        blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
        depthFunc equal
        rgbGen wave sin 0.75 0 0 0
    }
}

textures/village/villwin_c18_fixed
{
	qer_editorimage textures/village/villwin_c18.tga
	surfaceparm nomarks
	q3map_surfacelight 200
	q3map_lightsubdivide 128

	// 1) OPAQUE base first (critical for correct fog/depth behavior)
	{
		map textures/village/villwin_c18.tga
		rgbGen identity
	}

	// 2) Apply lightmap as a "filter" only where base already drew
	{
		map $lightmap
		blendFunc GL_DST_COLOR GL_ZERO
		depthFunc equal
		rgbGen identity
	}

	// 3) Glow/detail pass, also constrained to the already-drawn pixels
	//    (use a softer additive to avoid white cards in fog)
	{
		map textures/village/villwin_c18.blend.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		depthFunc equal
		rgbGen identity
		// Optional: slightly reduce intensity if still too hot:
		// rgbGen wave sin 0.75 0 0 0
	}
}

textures/barn/grass_dm01_fixed
{
	surfaceparm grasssteps
	q3map_lightmapsamplesize 32
	q3map_foliage models/foliage/grassfoliage1.md3 0.6 16 0.025 0
	q3map_foliage models/foliage/grassfoliage2.md3 0.6 16 0.025 0
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/terrain/grass_dm01.tga
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

sprites/emp_ring1
{
    cull disable
    polygonOffset
    {
        clampmap sprites/emp_ring1.jpg
        blendFunc GL_SRC_ALPHA GL_ONE
        rgbGen vertex
        alphaGen vertex
        tcMod rotate 90
    }
}

sprites/emp_spark
{
    cull disable
    {
        clampmap sprites/emp_spark.jpg
        blendFunc GL_SRC_ALPHA GL_ONE
        rgbGen vertex
        alphaGen vertex
    }
}

textures/sfx/wilsflame1
{
	qer_editorimage textures/sfx/flame1.tga
	q3map_surfacelight 1482
	cull none
	nofog
	surfaceparm nomarks
	surfaceparm nonsolid
	surfaceparm pointlight
	surfaceparm trans
	{
		animMap 10 textures/sfx/flame1.tga textures/sfx/flame2.tga textures/sfx/flame3.tga textures/sfx/flame4.tga textures/sfx/flame5.tga textures/sfx/flame6.tga textures/sfx/flame7.tga textures/sfx/flame8.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave inverseSawtooth 0 1 0 10
	}
	{
		animMap 10 textures/sfx/flame2.tga textures/sfx/flame3.tga textures/sfx/flame4.tga textures/sfx/flame5.tga textures/sfx/flame6.tga textures/sfx/flame7.tga textures/sfx/flame8.tga textures/sfx/flame1.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave sawtooth 0 1 0 10
	}
	{
		map textures/sfx/flameball.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin .6 .2 0 .6
	}
}

models/multiplayer/mine_marker/allied_marker
{
	cull disable
	nomipmaps
	nopicmip
	{
		map models/multiplayer/mine_marker/allied_marker.tga
		rgbGen lightingdiffuse
	}
}