textures/wt_skies/sky_castle_night
{
	nocompress
	qer_editorimage textures/skies/sky_8.tga
	q3map_lightimage textures/skies/n_blue2.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_globaltexture
	q3map_lightsubdivide 128 
    q3map_sun 0.274632 0.274632 0.39 5 35 45
    q3map_surfacelight 5
	skyparms textures/wt_skies/sky_castle_night_box 2048 -
	sunshader sun

	{
		map textures/wt_skies/clouds_1.tga
		blendFunc blend
		rgbGen identityLighting
		tcMod scroll 0.0015 0.0008
	}
}
