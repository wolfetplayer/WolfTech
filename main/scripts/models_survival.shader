models/perk_machines/indicator_enabled
{
    //off white KP light
    q3map_lightimage models/perk_machines/indicator_enabled.tga
    qer_editorimage models/perk_machines/indicator_enabled.tga
    q3map_surfacelight 5000
    surfaceparm nomarks
    {
        map $lightmap
        rgbGen identity
    }
    {
        map models/perk_machines/indicator_enabled.tga
        blendFunc GL_DST_COLOR GL_ZERO
        rgbGen identity
    }
    {
        map models/perk_machines/indicator_enabled.tga
        blendFunc GL_ONE GL_ONE
    }
}


models/perk_machines/glass
{
	
	{ 
		map models/perk_machines/glass.tga
		blendFunc GL_dst_color GL_one
		rgbgen identity
		tcmod scale 1 1
	}


	{
		map models/perk_machines/glass.tga
		blendfunc blend
		tcmod scale 1 1
		rgbGen vertex
	}
	
}


models/perk_machines/red/liquid_red
		
	{
		surfaceparm trans
		surfaceparm nonsolid
		surfaceparm water


		{ 
			map models/perk_machines/red/liquid_red.tga
			blendFunc GL_dst_color GL_one
			rgbgen identity
			tcmod scale .5 .5
			tcmod scroll .05 .05
		}
	
		{ 
			map models/perk_machines/red/liquid_red.tga
			blendFunc GL_dst_color gl_one
			rgbgen identity
			tcmod scale -1 -1
			tcmod scroll .05 .05
		}	
		{
			map $lightmap
			blendFunc GL_dst_color GL_zero
			rgbgen identity		
		}
	
	
	}
	
models/perk_machines/green/liquid_green
		
	{
		surfaceparm trans
		surfaceparm nonsolid
		surfaceparm water


		{ 
			map models/perk_machines/green/liquid_green.tga
			blendFunc GL_dst_color GL_one
			rgbgen identity
			tcmod scale .5 .5
			tcmod scroll .05 .05
		}
	
		{ 
			map models/perk_machines/green/liquid_green.tga
			blendFunc GL_dst_color gl_one
			rgbgen identity
			tcmod scale -1 -1
			tcmod scroll .05 .05
		}	
		{
			map $lightmap
			blendFunc GL_dst_color GL_zero
			rgbgen identity		
		}
	
	
	}
	
models/perk_machines/yellow/liquid_yellow
		
	{
		surfaceparm trans
		surfaceparm nonsolid
		surfaceparm water


		{ 
			map models/perk_machines/yellow/liquid_yellow.tga
			blendFunc GL_dst_color GL_one
			rgbgen identity
			tcmod scale .5 .5
			tcmod scroll .05 .05
		}
	
		{ 
			map models/perk_machines/yellow/liquid_yellow.tga
			blendFunc GL_dst_color gl_one
			rgbgen identity
			tcmod scale -1 -1
			tcmod scroll .05 .05
		}	
		{
			map $lightmap
			blendFunc GL_dst_color GL_zero
			rgbgen identity		
		}
	
	
	}
	
models/perk_machines/orange/liquid_orange
		
	{
		surfaceparm trans
		surfaceparm nonsolid
		surfaceparm water


		{ 
			map models/perk_machines/orange/liquid_orange.tga
			blendFunc GL_dst_color GL_one
			rgbgen identity
			tcmod scale .5 .5
			tcmod scroll .05 .05
		}
	
		{ 
			map models/perk_machines/orange/liquid_orange.tga
			blendFunc GL_dst_color gl_one
			rgbgen identity
			tcmod scale -1 -1
			tcmod scroll .05 .05
		}	
		{
			map $lightmap
			blendFunc GL_dst_color GL_zero
			rgbgen identity		
		}
	
	
	}
	
models/perk_machines/blue/liquid_blue
		
	{
		surfaceparm trans
		surfaceparm nonsolid
		surfaceparm water


		{ 
			map models/perk_machines/blue/liquid_blue.tga
			blendFunc GL_dst_color GL_one
			rgbgen identity
			tcmod scale .5 .5
			tcmod scroll .05 .05
		}
	
		{ 
			map models/perk_machines/blue/liquid_blue.tga
			blendFunc GL_dst_color gl_one
			rgbgen identity
			tcmod scale -1 -1
			tcmod scroll .05 .05
		}	
		{
			map $lightmap
			blendFunc GL_dst_color GL_zero
			rgbgen identity		
		}
	
	
	}
	
models/perk_machines/purple/liquid_purple
		
	{
		surfaceparm trans
		surfaceparm nonsolid
		surfaceparm water


		{ 
			map models/perk_machines/purple/liquid_purple.tga
			blendFunc GL_dst_color GL_one
			rgbgen identity
			tcmod scale .5 .5
			tcmod scroll .05 .05
		}
	
		{ 
			map models/perk_machines/purple/liquid_purple.tga
			blendFunc GL_dst_color gl_one
			rgbgen identity
			tcmod scale -1 -1
			tcmod scroll .05 .05
		}	
		{
			map $lightmap
			blendFunc GL_dst_color GL_zero
			rgbgen identity		
		}
	
	
	}
	
	
models/random_box/blue_light
		
	{
		surfaceparm trans
		surfaceparm nonsolid
		surfaceparm water


		{ 
			map models/random_box/blue_light.tga
			blendFunc GL_dst_color GL_one
			rgbgen identity
			tcmod scale .5 .5
			tcmod scroll .05 .05
		}
	
		{ 
			map models/random_box/blue_light.tga
			blendFunc GL_dst_color gl_one
			rgbgen identity
			tcmod scale -1 -1
			tcmod scroll .05 .05
		}	
		{
			map $lightmap
			blendFunc GL_dst_color GL_zero
			rgbgen identity		
		}
	
	
	}
	
models/random_box/main
{
	    qer_editorimage models/random_box/main.jpg
		{
		map models/random_box/main.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/upgrade_table/coil
{
	    qer_editorimage models/mapobjects/upgrade_table/coil.jpg
		{
		map models/mapobjects/upgrade_table/coil.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/upgrade_table/hacksaw
{
	    qer_editorimage models/mapobjects/upgrade_table/hacksaw.jpg
		{
		map models/mapobjects/upgrade_table/hacksaw.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/upgrade_table/hammer
{
	    qer_editorimage models/mapobjects/upgrade_table/hammer.jpg
		{
		map models/mapobjects/upgrade_table/hammer.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/upgrade_table/pilers
{
	    qer_editorimage models/mapobjects/upgrade_table/pilers.jpg
		{
		map models/mapobjects/upgrade_table/pilers.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/upgrade_table/screwdriver
{
	    qer_editorimage models/mapobjects/upgrade_table/screwdriver.jpg
		{
		map models/mapobjects/upgrade_table/screwdriver.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/upgrade_table/soldering_iron
{
	    qer_editorimage models/mapobjects/upgrade_table/soldering_iron.jpg
		{
		map models/mapobjects/upgrade_table/soldering_iron.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/upgrade_table/toolbox
{
	    qer_editorimage models/mapobjects/upgrade_table/toolbox.jpg
		{
		map models/mapobjects/upgrade_table/toolbox.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/upgrade_table/vice
{
	    qer_editorimage models/mapobjects/upgrade_table/vice.jpg
		{
		map models/mapobjects/upgrade_table/vice.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/upgrade_table/voltmeter
{
	    qer_editorimage models/mapobjects/upgrade_table/voltmeter.jpg
		{
		map models/mapobjects/upgrade_table/voltmeter.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/upgrade_table/workbench
{
	    qer_editorimage models/mapobjects/upgrade_table/workbench.jpg
		{
		map models/mapobjects/upgrade_table/workbench.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/upgrade_table/saw
{
	    qer_editorimage models/mapobjects/upgrade_table/saw.jpg
		{
		map models/mapobjects/upgrade_table/saw.jpg
		rgbGen vertex
		alphaFunc GE128
        depthWrite
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}