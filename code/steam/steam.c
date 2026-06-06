#include "steam.h"

#include "../steamshim/steamshim_child.h"

#include <stdint.h>
#include <stdio.h>

#ifdef STEAM

static uint64_t s_steamCurrentLobby = 0;

static void steamHandleEvent(const STEAMSHIM_Event *ev)
{
	if (!ev) {
		return;
	}

	switch (ev->type)
	{
	case SHIMEVENT_LOBBY_CREATED:
		if (ev->okay) {
			s_steamCurrentLobby = ev->uvalue;
			printf("Steam lobby created: %llu\n", (unsigned long long)s_steamCurrentLobby);
		} else {
			printf("Steam lobby create failed\n");
		}
		break;

	case SHIMEVENT_LOBBY_JOINED:
		if (ev->okay) {
			s_steamCurrentLobby = ev->uvalue;
			printf("Steam lobby joined: %llu connect='%s'\n",
				(unsigned long long)s_steamCurrentLobby,
				ev->name);
		} else {
			printf("Steam lobby join failed\n");
		}
		break;

	case SHIMEVENT_LOBBY_LIST:
		/*
		ev->uvalue == 0 means list finished.
		ev->name contains lobby name/map/connect/etc depending on parent.
		*/
		if (ev->okay) {
			if (ev->uvalue == 0) {
				printf("Steam lobby list finished\n");
			} else {
				printf("Steam lobby found: %llu '%s'\n",
					(unsigned long long)ev->uvalue,
					ev->name);
			}
		} else {
			printf("Steam lobby list failed\n");
		}
		break;

	case SHIMEVENT_LOBBY_DATA:
		printf("Steam lobby data updated: %llu '%s'\n",
			(unsigned long long)ev->uvalue,
			ev->name);
		break;

	default:
		break;
	}
}

void steamRun(void)
{
	const STEAMSHIM_Event *ev;

	/*
	Pump can return one event per call.
	Loop a few times so multiple pending lobby-list events are consumed.
	*/
	while ((ev = STEAMSHIM_pump()) != NULL) {
		steamHandleEvent(ev);
	}
}

int steamInit(void)
{
	if (!STEAMSHIM_init()) {
		return 0;
	}

	STEAMSHIM_requestStats();

	return 1;
}

void steamSetAchievement(const char *id)
{
	STEAMSHIM_setAchievement(id, 1);
	STEAMSHIM_storeStats();
}

void steamResetStats(const int bAlsoAchievements)
{
	STEAMSHIM_resetStats(bAlsoAchievements);
}

int steamAlive(void)
{
	return STEAMSHIM_alive();
}

void steamSetRichPresence(const char *key, const char *value)
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
	STEAMSHIM_lobbyJoin(lobbyID);
}

void steamLobbyLeave(void)
{
	STEAMSHIM_lobbyLeave();
	s_steamCurrentLobby = 0;
}

void steamLobbySetData(const char *key, const char *value)
{
	STEAMSHIM_lobbySetData(key, value);
}

void steamLobbyInvite(void)
{
	if (s_steamCurrentLobby != 0) {
		STEAMSHIM_lobbyInvite(s_steamCurrentLobby);
	}
}

uint64_t steamLobbyCurrent(void)
{
	return s_steamCurrentLobby;
}

#else

static uint64_t s_steamCurrentLobby = 0;

void steamRun(void)
{
}

int steamInit(void)
{
	return 0;
}

void steamSetAchievement(const char *id)
{
	(void)id;
}

void steamResetStats(const int bAlsoAchievements)
{
	(void)bAlsoAchievements;
}

int steamAlive(void)
{
	return 1;
}

void steamSetRichPresence(const char *key, const char *value)
{
	(void)key;
	(void)value;
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
	s_steamCurrentLobby = 0;
}

void steamLobbySetData(const char *key, const char *value)
{
	(void)key;
	(void)value;
}

void steamLobbyInvite(void)
{
}

uint64_t steamLobbyCurrent(void)
{
	return s_steamCurrentLobby;
}

#endif