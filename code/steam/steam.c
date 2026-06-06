#include "steam.h"

#include "../steamshim/steamshim_child.h"

#include <stdint.h>

#ifdef STEAM

void steamRun(void)
{
	STEAMSHIM_pump();
	
	return;
}

int steamInit(void)
{
	if (!STEAMSHIM_init())
	{
		return 0;
	}

	STEAMSHIM_requestStats();

	return 1;
}

void steamSetAchievement(const char* id)
{
	STEAMSHIM_setAchievement(id, 1);

	STEAMSHIM_storeStats();

	return;
}

void steamResetStats(const int bAlsoAchievements)
{
	STEAMSHIM_resetStats(bAlsoAchievements);

	return;
}

int steamAlive()
{
	return STEAMSHIM_alive();
}

void steamSetRichPresence(const char* key, const char* value)
{
	STEAMSHIM_setRichPresence(key, value);
}

void steamLobbyCreate(int maxPlayers)
{
	STEAMSHIM_lobbyCreate(maxPlayers);
}

void steamLobbyList(void)
{
	STEAMSHIM_lobbyList();
}

void steamLobbyJoin(uint64_t lobbyID)
{
	STEAMSHIM_lobbyJoin((uint64_t)lobbyID);
}

void steamLobbyLeave(void)
{
	STEAMSHIM_lobbyLeave();
}

void steamLobbySetData(const char *key, const char *value)
{
	STEAMSHIM_lobbySetData(key, value);
}

#else

void steamRun(void)
{
	return;
}

int steamInit(void)
{
	return 0;
}

void steamSetAchievement(const char* id)
{
	return;
}

int steamAlive()
{
	return 1;
}

void steamSetRichPresence(const char* key, const char* value)
{
	return;
}

void steamResetStats(const int bAlsoAchievements)
{
	return;
}


void steamLobbyCreate(int maxPlayers)
{
	(void)maxPlayers;
}

void steamLobbyList(void)
{
}

void steamLobbyJoin(uint64_t lobbyID)
{
	(void)lobbyID;
}

void steamLobbyLeave(void)
{
}

void steamLobbySetData(const char *key, const char *value)
{
	(void)key;
	(void)value;
}

#endif