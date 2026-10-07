textures/wt_skies/sky_castle_night
{
	nocompress
	qer_editorimage textures/skies/sky_8.tga
	q3map_lightimage textures/skies/n_blue2.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_globaltexture
	q3map_lightsubdivide 256 
    q3map_sun 0.274632 0.274632 0.39 10 35 45
    q3map_surfacelight 10
	skyparms full 200 -
	sunshader sun
	
	{
		map textures/wt_skies/night_sky.jpg
		tcMod scale 16.0 16.0
		depthWrite
	}

	//{
	//	map textures/wt_skies/clouds_1.tga
		//blendfunc blend
		//tcMod scroll 0.001 0.00
		//tcMod scale 2 1
	//}

}
