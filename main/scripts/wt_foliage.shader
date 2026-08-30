models/mapobjects/wt_foliage/trunk
{
	surfaceparm nomarks
		{
		map models/mapobjects/wt_foliage/trunk.jpg
			rgbGen vertex
		}
		{
			map $lightmap
			blendFunc filter
			rgbGen identity
		}
}

models/mapobjects/wt_foliage/leaves_01
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/wt_foliage/leaves_01.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/wt_foliage/leaves_01.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		rgbGen vertex
		}
}

models/mapobjects/wt_foliage/leaves_02
{
	nopicmip
	qer_alphafunc greater 0.5
	qer_editorimage models/mapobjects/wt_foliage/leaves_02.tga
	cull disable
	surfaceparm alphashadow
	surfaceparm trans
	surfaceparm nomarks
     	{
		map models/mapobjects/wt_foliage/leaves_02.tga
		blendfunc GL_ONE GL_ZERO
		alphaFunc GE128
		depthWrite
		rgbGen vertex
		}
}