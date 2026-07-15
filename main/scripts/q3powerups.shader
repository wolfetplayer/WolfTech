hasteSmokePuff			// drops behind player's feet when speeded
{
	cull none
	entityMergable		// allow all the sprites to be merged together
	{
		map gfx/misc/smokepuff3.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen		vertex
		alphaGen	vertex
	}
}

models/powerups/instant/quad
{

	{
		map textures/effects/envmapblue.tga
		blendfunc GL_ONE GL_ZERO
		tcGen environment
                rgbGen identity
	}
}
models/powerups/instant/enviro
{

	{
		map textures/effects/envmapgold.tga
		blendfunc GL_ONE GL_ZERO
		tcGen environment
                rgbGen identity
	}
}

models/powerups/instant/haste
{
	{
		map textures/effects/envmapyel.tga
		blendfunc GL_ONE GL_ZERO
		tcGen environment
                rgbGen identity
	}
}
models/powerups/instant/invis
{
	{
		map textures/effects/tinfx2c.tga
		blendfunc GL_ONE GL_ONE
		tcGen environment
	}
}

powerups/quadWeapon
{
	deformVertexes wave 100 sin 0.5 0 0 0
	{
		map textures/effects/quadmap2.tga
		blendfunc GL_ONE GL_ONE
		tcGen environment
                tcmod rotate 30
                tcmod scroll 1 .1
	}
}

powerups/vampire
{
	deformVertexes wave 100 sin 0.5 0 0 0
	{
		map textures/effects/quadmap2_red.tga
		blendfunc GL_ONE GL_ONE
		tcGen environment
                tcmod rotate 30
                tcmod scroll 1 .1
	}
}


models/powerups/health/yellow
{
	
	{
		map textures/effects/envmapyel.tga
                tcGen environment
		//blendfunc GL_ONE GL_ONE
		
	}
        
}
models/powerups/health/yellow_sphere
{
	{
		map textures/effects/tinfx2b.tga
		tcGen environment
		blendfunc GL_ONE GL_ONE
	}
}

models/powerups/survival/sphere_gold
{
	deformVertexes wave 100 sin 0.5 0 0 0
	{
		map models/powerups/survival/sphere_gold.jpg
		blendfunc GL_ONE GL_ONE
		tcGen environment
                tcmod rotate 30
                tcmod scroll 1 .1
	}
}

models/powerups/survival/sphere_red
{
	deformVertexes wave 100 sin 0.5 0 0 0
	{
		map models/powerups/survival/sphere_red.jpg
		blendfunc GL_ONE GL_ONE
		tcGen environment
                tcmod rotate 30
                tcmod scroll 1 .1
	}
}

models/powerups/survival/sphere_green
{
	deformVertexes wave 100 sin 0.5 0 0 0
	{
		map models/powerups/survival/sphere_green.jpg
		blendfunc GL_ONE GL_ONE
		tcGen environment
                tcmod rotate 30
                tcmod scroll 1 .1
	}
}

models/powerups/survival/sphere_blue
{
	deformVertexes wave 100 sin 0.5 0 0 0
	{
		map models/powerups/survival/sphere_blue.jpg
		blendfunc GL_ONE GL_ONE
		tcGen environment
                tcmod rotate 30
                tcmod scroll 1 .1
	}
}