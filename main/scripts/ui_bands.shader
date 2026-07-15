_backimage2
{
	nopicmip
	nomipmaps
	{
		map ui/assets/_backimage2.tga
		blendfunc blend
	}
}

_backimage3
{
	nopicmip
	nomipmaps
	{
		map ui/assets/_backimage3.tga
		blendfunc blend
	}
}

_backimage4
{
	nopicmip
	nomipmaps
	{
		map ui/assets/_backimage4.tga
		blendfunc blend
	}
}

_menu_load
{
	nopicmip
	nomipmaps
	{
		map ui/assets/_menu_load.tga
		blendfunc blend
	}
}

_menu_select
{
	nopicmip
	nomipmaps
	{
		map ui/assets/_menu_select.tga
		blendfunc blend
	}
}

_menu_start
{
	nopicmip
	nomipmaps
	{
		map ui/assets/_menu_start.tga
		blendfunc blend
	}
}



_bands
{
	nopicmip
	nomipmaps
	{
		map ui/assets/_bands.tga
		blendfunc blend
	//	tcmod stretch sin 1 .2 0 1
		tcmod scroll -0.03 0 
		rgbgen wave sawtooth 2 .7 0 .5
	}

	{
		map ui/assets/_bandsa.tga
		blendfunc blend
	//	tcmod stretch sin 1 .1 0 1 
	//	tcmod turb  1 .01 0 .1
		tcmod scroll 0.05 0

	}
}

_band_logo
{
	nopicmip
	nomipmaps
	{
		map ui/assets/_band_logo.tga
		blendfunc blend
	//	tcmod stretch sin 1 .2 0 1
		tcmod scroll -0.03 0 
		rgbgen wave sawtooth 2 .7 0 .5
	}

	{
		map ui/assets/_band_logo.tga
		blendfunc blend
	//	tcmod stretch sin 1 .1 0 1 
	//	tcmod turb  1 .01 0 .1
		tcmod scroll 0.03 0

	}
}

_band2
{
	nopicmip
	nomipmaps
	{
		map ui/assets/_band2.tga
		blendfunc blend
		rgbgen wave sawtooth 2 .5 0 .5
	}
}