models/mapobjects/wt_crates/ammo_box
{
	    qer_editorimage models/mapobjects/wt_crates/ammo_box.jpg
		{
		map models/mapobjects/wt_crates/ammo_box.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/wt_crates/cargo_crate
{
	    qer_editorimage models/mapobjects/wt_crates/cargo_crate.jpg
		{
		map models/mapobjects/wt_crates/cargo_crate.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/wt_furniture/filling_cabinet
{
	    qer_editorimage models/mapobjects/wt_furniture/filling_cabinet.jpg
		{
		map models/mapobjects/wt_furniture/filling_cabinet.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/wt_props/fuel_oil_drum
{
	    qer_editorimage models/mapobjects/wt_props/fuel_oil_drum.jpg
		{
		map models/mapobjects/wt_props/fuel_oil_drum.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/wt_props/furnace
{
	    qer_editorimage models/mapobjects/wt_props/furnace.jpg
		{
		map models/mapobjects/wt_props/furnace.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/wt_props/hedgehog
{
	    qer_editorimage models/mapobjects/wt_props/hedgehog.jpg
		{
		map models/mapobjects/wt_props/hedgehog.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/wt_lights/Lampshade
{
	    qer_editorimage models/mapobjects/wt_lights/Lampshade.jpg
		{
		map models/mapobjects/wt_lights/Lampshade.jpg
		rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/light/Lamp
{
	surfaceparm nomarks
	q3map_lightimage models/mapobjects/light/Lamp.jpg
	q3map_surfacelight 7000
	q3map_lightrgb 0.80 0.72 0.60
	
	{
		map models/mapobjects/light/Lamp.jpg
		blendFunc GL_ONE GL_ZERO
		rgbGen vertex
	}
	{
		map models/mapobjects/light/Lamp.jpg
		blendfunc GL_ONE GL_ONE
	}
}