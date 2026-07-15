models/mapobjects/hd_vanilla/shield
{
	    qer_editorimage models/mapobjects/hd_vanilla/shield.jpg
		{
		map models/mapobjects/hd_vanilla/shield.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/hd_vanilla/metal
{
	    qer_editorimage models/mapobjects/hd_vanilla/metal.jpg
		{
		map models/mapobjects/hd_vanilla/metal.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/hd_vanilla/light
{
    //off white KP light
    q3map_lightimage models/mapobjects/hd_vanilla/light.jpg
    qer_editorimage models/mapobjects/hd_vanilla/light.jpg
    q3map_surfacelight 5000
    surfaceparm nomarks
    {
        map $lightmap
        rgbGen identity
    }
    {
        map models/mapobjects/hd_vanilla/light.jpg
        blendFunc GL_DST_COLOR GL_ZERO
        rgbGen identity
    }
    {
        map models/mapobjects/hd_vanilla/light.jpg
        blendFunc GL_ONE GL_ONE
    }
}

models/mapobjects/hd_vanilla/metal_lantern
{
	    qer_editorimage models/mapobjects/hd_vanilla/metal_lantern.jpg
		{
		map models/mapobjects/hd_vanilla/metal_lantern.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/hd_vanilla/light_lantern
{
    //off white KP light
    q3map_lightimage models/mapobjects/hd_vanilla/light_lantern.jpg
    qer_editorimage models/mapobjects/hd_vanilla/light_lantern.jpg
    q3map_surfacelight 5000
    surfaceparm nomarks
    {
        map $lightmap
        rgbGen identity
    }
    {
        map models/mapobjects/hd_vanilla/light_lantern.jpg
        blendFunc GL_DST_COLOR GL_ZERO
        rgbGen identity
    }
    {
        map models/mapobjects/hd_vanilla/light_lantern.jpg
        blendFunc GL_ONE GL_ONE
    }
}
