speakerScript
{
	// Thunderclaps for the "thunderstuff" script entity in sv_river_outpost.coop.script,
	// same setup as radar's thunderspeaker01-05. Non-looped + global, so origin doesn't
	// matter -- the sound plays at full volume from every player's own position with no
	// attenuation, it's just here because the format requires the key.

	speakerDef
	{
		noise "sound/world/thunder_01.wav"
		origin 0 0 0
		targetname "thunderspeaker01"
		looped "no"
		broadcast "global"
		volume 255
	}

	speakerDef
	{
		noise "sound/world/thunder_02.wav"
		origin 0 0 0
		targetname "thunderspeaker02"
		looped "no"
		broadcast "global"
		volume 255
	}

	speakerDef
	{
		noise "sound/world/thunder_03.wav"
		origin 0 0 0
		targetname "thunderspeaker03"
		looped "no"
		broadcast "global"
		volume 255
	}

	speakerDef
	{
		noise "sound/world/thunder_04.wav"
		origin 0 0 0
		targetname "thunderspeaker04"
		looped "no"
		broadcast "global"
		volume 255
	}

	speakerDef
	{
		noise "sound/world/thunder_05.wav"
		origin 0 0 0
		targetname "thunderspeaker05"
		looped "no"
		broadcast "global"
		volume 255
	}
}
