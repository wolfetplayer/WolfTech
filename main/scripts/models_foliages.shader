models/foliages/cliff01
{
	qer_editorImage models/foliages/cliff01.jpg
	{
		map $lightmap
		rgbGen identity
	}
		q3map_styleMarker
	{
		map models/foliages/cliff01.jpg
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}


models/foliages/rock01
{
	qer_editorImage models/foliages/rock01.jpg
	{
		map $lightmap
		rgbGen identity
	}
		q3map_styleMarker
	{
		map models/foliages/rock01.jpg
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

models/foliages/rock02
{
	qer_editorImage models/foliages/rock02.jpg
	{
		map $lightmap
		rgbGen identity
	}
		q3map_styleMarker
	{
		map models/foliages/rock02.jpg
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

models/foliages/rock03
{
	qer_editorImage models/foliages/rock03.jpg
	{
		map $lightmap
		rgbGen identity
	}
		q3map_styleMarker
	{
		map models/foliages/rock03.jpg
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

models/foliages/rock04
{
	qer_editorImage models/foliages/rock04.jpg
	{
		map $lightmap
		rgbGen identity
	}
		q3map_styleMarker
	{
		map models/foliages/rock04.jpg
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

models/foliages/rock05
{
	qer_editorImage models/foliages/rock05.jpg
	{
		map $lightmap
		rgbGen identity
	}
		q3map_styleMarker
	{
		map models/foliages/rock05.jpg
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

models/foliages/branch_leaves
{
	qer_alphafunc greater 0.5
	surfaceparm alphashadow
	cull disable
	sort seethrough
	nopicmip
	surfaceparm trans
	surfaceparm nomarks
	deformVertexes wave 15 sin 0 0.5 0 0.25
	{
		map models/foliages/branch_leaves.tga
		alphaFunc GE128
		rgbGen exactVertex
		alphaGen vertex
	}
}

models/foliages/branch_leaves_big
{
	qer_alphafunc greater 0.5
	surfaceparm alphashadow
	cull disable
	sort seethrough
	nopicmip
	surfaceparm trans
	surfaceparm nomarks
	deformVertexes wave 15 sin 0 0.75 0 0.25
	{
		map models/foliages/branch_leaves_big.tga
		alphaFunc GE128
		rgbGen exactVertex
		alphaGen vertex
	}
}

models/foliages/leaves01
{
	qer_alphafunc greater 0.5
	surfaceparm alphashadow
	cull disable
	sort seethrough
	nopicmip
	surfaceparm trans
	surfaceparm nomarks
	deformVertexes wave 15 sin 0 0.25 0 0.25
	{
		map models/foliages/leaves01.tga
		alphaFunc GE128
		rgbGen exactVertex
		alphaGen vertex
	}
}

models/foliages/leaves02
{
	qer_alphafunc greater 0.5
	surfaceparm alphashadow
	cull disable
	sort seethrough
	nopicmip
	surfaceparm trans
	surfaceparm nomarks
	deformVertexes wave 15 sin 0 0.25 0 0.25
	{
		map models/foliages/leaves02.tga
		alphaFunc GE128
		rgbGen exactVertex
		alphaGen vertex
	}
}

models/foliages/branch_small02
{
	qer_editorImage models/foliages/branch_small.tga
	qer_alphafunc greater 0.5
	cull disable
	sort seethrough
	nopicmip
	surfaceparm trans
	surfaceparm nomarks
	distanceCull 3200 4000 0.5
	{
		map models/foliages/branch_small.tga
		alphaFunc GE128
		rgbGen exactVertex
		alphaGen vertex
	}
}

models/foliages/bark_small
{
	qer_editorImage models/foliages/bark_small.jpg
	{
		map $lightmap
		rgbGen identity
	}
		q3map_styleMarker
	{
		map models/foliages/bark_small.jpg
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

models/foliages/bark_big
{
	qer_editorImage models/foliages/bark_big.jpg
	{
		map $lightmap
		rgbGen identity
	}
		q3map_styleMarker
	{
		map models/foliages/bark_big.jpg
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}


models/foliages/foliage01
{
	qer_alphafunc greater 0.5
	cull disable
	sort seethrough
	nopicmip
	surfaceparm pointlight
	surfaceparm trans
	surfaceparm nomarks
	distanceCull 1800 3600 0.5
	deformVertexes wave 15 sin 0 1 0 0.25
	{
		map models/foliages/foliage01.tga
		alphaFunc GE128
		rgbGen exactVertex
		alphaGen vertex
	}
}

models/foliages/foliage03
{
	qer_alphafunc greater 0.5
	cull disable
	sort seethrough
	nopicmip
	surfaceparm pointlight
	surfaceparm trans
	surfaceparm nomarks
	distanceCull 1800 3600 0.5
	deformVertexes wave 15 sin 0 1 0 0.25
	{
		map models/foliages/foliage03.tga
		alphaFunc GE128
		rgbGen exactVertex
		alphaGen vertex
	}
}

models/foliages/foliage04
{
	qer_alphafunc greater 0.5
	cull disable
	sort seethrough
	nopicmip
	surfaceparm pointlight
	surfaceparm trans
	surfaceparm nomarks
	distanceCull 1800 3600 0.5
	deformVertexes wave 15 sin 0 1 0 0.25
	{
		map models/foliages/foliage04.tga
		alphaFunc GE128
		rgbGen exactVertex
		alphaGen vertex
	}
}

models/foliages/foliage05
{
	qer_alphafunc greater 0.5
	cull disable
	sort seethrough
	nopicmip
	surfaceparm pointlight
	surfaceparm trans
	surfaceparm nomarks
	distanceCull 1800 3600 0.5
	deformVertexes wave 15 sin 0 1 0 0.25
	{
		map models/foliages/foliage05.tga
		alphaFunc GE128
		rgbGen exactVertex
		alphaGen vertex
	}
}

models/foliages/foliage06
{
	qer_alphafunc greater 0.5
	cull disable
	sort seethrough
	nopicmip
	surfaceparm pointlight
	surfaceparm trans
	surfaceparm nomarks
	distanceCull 1800 3600 0.5
	deformVertexes wave 15 sin 0 1 0 0.25
	{
		map models/foliages/foliage06.tga
		alphaFunc GE128
		rgbGen exactVertex
		alphaGen vertex
	}
}
