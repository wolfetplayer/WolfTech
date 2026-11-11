// through-wall teammate outline (cg_drawTeamOutline) - mask+outline pair, sort order below must stay as-is

models/misc/teammate_xray_mask
{
	sort 15.0
	{
		map *white
		blendFunc GL_ZERO GL_ONE     // invisible
		depthwrite
		depthfunc always             // not nodepthtest - that also skips the depth write
	}
}

models/misc/teammate_xray_outline
{
	// default culling: only the inflated mesh's near surface should count here
	sort 15.1
	{
		map *white
		rgbGen const ( 0.25 1.0 0.3 )
		alphaGen const 0.45   // subtle rim, not a solid block
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		depthfunc greater
	}
}

// through-wall revive-needed icon - same stock icon as the voice-chat medic call
sprites/voiceMedic_xray
{
	sort 15.1
	{
		map sprites/voiceMedic
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		nodepthtest
	}
}
