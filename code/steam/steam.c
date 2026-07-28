#include "steam.h"

#include "../steamshim/steamshim_child.h"

#include <stdint.h>
#include <stdio.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#ifdef STEAM

static uint64_t s_steamCurrentLobby = 0;
static uint64_t s_steamCurrentLobbyOwner = 0;
static int s_steamHostLeft = 0;

static uint64_t s_localSteamID = 0;
static char s_localPersonaName[64] = "";

// Small cache of other players' persona names, keyed by steamID; populated from
// SHIMEVENT_FRIEND_NAME replies to steamRequestFriendName(). Plenty for a 4-player lobby.
#define STEAM_FRIENDNAME_CACHE_MAX 16
typedef struct {
	uint64_t steamID;
	char name[64];
} steamFriendNameEntry_t;
static steamFriendNameEntry_t s_friendNameCache[STEAM_FRIENDNAME_CACHE_MAX];
static int s_friendNameCacheCount = 0;

static void steamCacheFriendName(uint64_t steamID, const char *name)
{
	int i;

	for (i = 0; i < s_friendNameCacheCount; i++) {
		if (s_friendNameCache[i].steamID == steamID) {
			Q_strncpyz(s_friendNameCache[i].name, name, sizeof(s_friendNameCache[i].name));
			return;
		}
	}

	if (s_friendNameCacheCount >= STEAM_FRIENDNAME_CACHE_MAX) {
		// Cache full (shouldn't happen with MAX_COOP_PLAYERS this small) - overwrite oldest.
		memmove(&s_friendNameCache[0], &s_friendNameCache[1], sizeof(s_friendNameCache[0]) * (STEAM_FRIENDNAME_CACHE_MAX - 1));
		s_friendNameCacheCount = STEAM_FRIENDNAME_CACHE_MAX - 1;
	}

	s_friendNameCache[s_friendNameCacheCount].steamID = steamID;
	Q_strncpyz(s_friendNameCache[s_friendNameCacheCount].name, name, sizeof(s_friendNameCache[s_friendNameCacheCount].name));
	s_friendNameCacheCount++;
}

// Steam lobby list cache for the server browser; filled in from SHIMEVENT_LOBBY_LIST events, polled via steamLobbyListDirty().
#define STEAM_LOBBYLIST_MAX 128

static steamLobbyInfo_t s_lobbyList[STEAM_LOBBYLIST_MAX];
static int s_lobbyListCount = 0;
static int s_lobbyListDirty = 0;

// Splits "name\x01map\x01members\x01maxMembers\x01gametype" from steamshim_parent.cpp into a steamLobbyInfo_t.
static void steamLobbyListParseEntry(const char *text, steamLobbyInfo_t *out)
{
	char buf[256];
	char *fields[5];
	int fieldCount = 0;
	char *p, *start;

	memset(out, 0, sizeof(*out));

	Q_strncpyz(buf, text, sizeof(buf));

	start = buf;
	for (p = buf; *p && fieldCount < 4; p++) {
		if (*p == '\x01') {
			*p = '\0';
			fields[fieldCount++] = start;
			start = p + 1;
		}
	}
	fields[fieldCount++] = start;
	while (fieldCount < 5) {
		fields[fieldCount++] = "";
	}

	Q_strncpyz(out->name, fields[0], sizeof(out->name));
	Q_strncpyz(out->map, fields[1], sizeof(out->map));
	out->members = atoi(fields[2]);
	out->maxMembers = atoi(fields[3]);
	out->gameType = atoi(fields[4]);
}

int steamLobbyListCount(void)
{
	return s_lobbyListCount;
}

const steamLobbyInfo_t *steamLobbyListGet(int index)
{
	if (index < 0 || index >= s_lobbyListCount) {
		return NULL;
	}
	return &s_lobbyList[index];
}

int steamLobbyListDirty(void)
{
	return s_lobbyListDirty;
}

void steamLobbyListClearDirty(void)
{
	s_lobbyListDirty = 0;
}

/*
===============
Steam P2P net transport queues

steamRun() drains every pending shim event once per client frame (and,
once wired up, once per server frame too), but net_ip.c wants to pull
packets one at a time from its own NET_GetPacket loop. These small ring
buffers bridge that gap. Overflow just drops the oldest entry, same as
a UDP socket buffer overflowing.
===============
*/

#define STEAMNET_PACKETQUEUE_SIZE 64

typedef struct {
	uint64_t steamID;
	int len;
	unsigned char data[STEAMSHIM_MAX_NET_PACKET];
} SteamNetQueuedPacket;

static SteamNetQueuedPacket s_netPacketQueue[STEAMNET_PACKETQUEUE_SIZE];
static int s_netPacketHead = 0;   /* next slot to read */
static int s_netPacketCount = 0;  /* number of queued packets */

#define STEAMNET_CONNQUEUE_SIZE 16

typedef struct {
	uint64_t steamID;
	int connected;  /* 1 = connected, 0 = disconnected */
} SteamNetConnEvent;

static SteamNetConnEvent s_netConnQueue[STEAMNET_CONNQUEUE_SIZE];
static int s_netConnHead = 0;
static int s_netConnCount = 0;

static void steamNetQueuePacket(uint64_t steamID, const unsigned char *data, int len)
{
	int tail;

	if (len < 0) {
		return;
	}

	if (len > STEAMSHIM_MAX_NET_PACKET) {
		len = STEAMSHIM_MAX_NET_PACKET;
	}

	if (s_netPacketCount == STEAMNET_PACKETQUEUE_SIZE) {
		/* queue full: drop the oldest packet to make room, same as a
		   full socket buffer would. */
		s_netPacketHead = (s_netPacketHead + 1) % STEAMNET_PACKETQUEUE_SIZE;
		s_netPacketCount--;
	}

	tail = (s_netPacketHead + s_netPacketCount) % STEAMNET_PACKETQUEUE_SIZE;
	s_netPacketQueue[tail].steamID = steamID;
	s_netPacketQueue[tail].len = len;
	memcpy(s_netPacketQueue[tail].data, data, len);
	s_netPacketCount++;
}

static void steamNetQueueConnEvent(uint64_t steamID, int connected)
{
	int tail;

	if (s_netConnCount == STEAMNET_CONNQUEUE_SIZE) {
		s_netConnHead = (s_netConnHead + 1) % STEAMNET_CONNQUEUE_SIZE;
		s_netConnCount--;
	}

	tail = (s_netConnHead + s_netConnCount) % STEAMNET_CONNQUEUE_SIZE;
	s_netConnQueue[tail].steamID = steamID;
	s_netConnQueue[tail].connected = connected;
	s_netConnCount++;
}

void steamNetListen(void)
{
	STEAMSHIM_netListen();
}

void steamNetConnect(uint64_t steamID)
{
	STEAMSHIM_netConnect(steamID);
}

int steamNetSend(uint64_t steamID, const void *data, int len)
{
	return STEAMSHIM_netSend(steamID, data, len);
}

void steamNetClose(uint64_t steamID)
{
	STEAMSHIM_netClose(steamID);
}

int steamNetPollPacket(uint64_t *outSteamID, void *buf, int maxLen)
{
	SteamNetQueuedPacket *pkt;
	int len;

	if (s_netPacketCount == 0) {
		return 0;
	}

	pkt = &s_netPacketQueue[s_netPacketHead];
	len = pkt->len;
	if (len > maxLen) {
		len = maxLen;
	}

	if (outSteamID) {
		*outSteamID = pkt->steamID;
	}
	memcpy(buf, pkt->data, len);

	s_netPacketHead = (s_netPacketHead + 1) % STEAMNET_PACKETQUEUE_SIZE;
	s_netPacketCount--;

	return len;
}

int steamNetPollConnEvent(uint64_t *outSteamID, int *outConnected)
{
	SteamNetConnEvent *ev;

	if (s_netConnCount == 0) {
		return 0;
	}

	ev = &s_netConnQueue[s_netConnHead];
	if (outSteamID) {
		*outSteamID = ev->steamID;
	}
	if (outConnected) {
		*outConnected = ev->connected;
	}

	s_netConnHead = (s_netConnHead + 1) % STEAMNET_CONNQUEUE_SIZE;
	s_netConnCount--;

	return 1;
}

static void steamHandleEvent(const STEAMSHIM_Event *ev)
{
	if (!ev) {
		return;
	}

	switch (ev->type)
	{
	case SHIMEVENT_NET_CONNECTED:
		steamNetQueueConnEvent(ev->uvalue, 1);
		break;

	case SHIMEVENT_NET_DISCONNECTED:
		steamNetQueueConnEvent(ev->uvalue, 0);
		break;

	case SHIMEVENT_NET_DATA:
		steamNetQueuePacket(ev->uvalue, ev->data, ev->datalen);
		break;

	case SHIMEVENT_LOBBY_OWNER:
		s_steamCurrentLobbyOwner = ev->uvalue;
		printf("Steam lobby owner: %llu\n", (unsigned long long)s_steamCurrentLobbyOwner);
		break;

	case SHIMEVENT_LOBBY_HOSTLEFT:
		s_steamHostLeft = 1;
		printf("Steam lobby host %llu has left/disconnected\n", (unsigned long long)ev->uvalue);
		break;

	case SHIMEVENT_LOCAL_IDENTITY:
		if (ev->okay) {
			s_localSteamID = ev->uvalue;
			Q_strncpyz(s_localPersonaName, ev->name, sizeof(s_localPersonaName));
			printf("Steam local identity: %llu '%s'\n", (unsigned long long)s_localSteamID, s_localPersonaName);
		}
		break;

	case SHIMEVENT_FRIEND_NAME:
		if (ev->okay) {
			steamCacheFriendName(ev->uvalue, ev->name);
			printf("Steam friend name: %llu '%s'\n", (unsigned long long)ev->uvalue, ev->name);
		}
		break;

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
		// ev->uvalue == 0 means list finished; ev->name is "name\x01map\x01members\x01maxMembers\x01gametype".
		if (ev->okay) {
			if (ev->uvalue == 0) {
				printf("Steam lobby list finished\n");
				s_lobbyListDirty = 1;
			} else if (s_lobbyListCount < STEAM_LOBBYLIST_MAX) {
				steamLobbyInfo_t *entry = &s_lobbyList[s_lobbyListCount++];
				steamLobbyListParseEntry(ev->name, entry);
				entry->lobbyID = ev->uvalue;
				printf("Steam lobby found: %llu '%s'\n",
					(unsigned long long)ev->uvalue,
					entry->name);
				s_lobbyListDirty = 1;
			}
		} else {
			printf("Steam lobby list failed\n");
			s_lobbyListDirty = 1;
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

static const char *SteamEventName(STEAMSHIM_EventType type)
{
	switch (type)
	{
	case SHIMEVENT_LOBBY_CREATED:     return "LOBBY_CREATED";
	case SHIMEVENT_LOBBY_LIST:        return "LOBBY_LIST";
	case SHIMEVENT_LOBBY_JOINED:      return "LOBBY_JOINED";
	case SHIMEVENT_LOBBY_DATA:        return "LOBBY_DATA";
	case SHIMEVENT_LOBBY_OWNER:       return "LOBBY_OWNER";
	case SHIMEVENT_LOBBY_HOSTLEFT:    return "LOBBY_HOSTLEFT";
	case SHIMEVENT_LOCAL_IDENTITY:    return "LOCAL_IDENTITY";
	case SHIMEVENT_FRIEND_NAME:       return "FRIEND_NAME";
	case SHIMEVENT_NET_CONNECTED:     return "NET_CONNECTED";
	case SHIMEVENT_NET_DISCONNECTED:  return "NET_DISCONNECTED";
	case SHIMEVENT_NET_DATA:          return "NET_DATA";
	default:                          return "UNKNOWN";
	}
}

void steamRun(void)
{
	const STEAMSHIM_Event *ev;

	while ((ev = STEAMSHIM_pump()) != NULL) {
		/* NET_DATA fires every packet, every frame - too noisy to log. */
		if (ev->type != SHIMEVENT_NET_DATA) {
			Com_Printf(
				"STEAMRUN: %s okay=%d lobby=%llu name='%s'\n",
				SteamEventName(ev->type),
				ev->okay,
				(unsigned long long)ev->uvalue,
				ev->name
			);
		}

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
	int alive = STEAMSHIM_alive();

	return alive;
}
void steamSetRichPresence(const char *key, const char *value)
{
	STEAMSHIM_setRichPresence(key, value);
}

void steamLobbyCreate(int maxPlayers)
{
	Com_Printf("steamLobbyCreate: maxPlayers=%d\n", maxPlayers);
	STEAMSHIM_lobbyCreate(maxPlayers);
}

void steamLobbyList(void)
{
	s_lobbyListCount = 0;
	s_lobbyListDirty = 0;
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
	s_steamCurrentLobbyOwner = 0;
	s_steamHostLeft = 0;
	steamNetClose(0);
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

uint64_t steamLobbyOwner(void)
{
	return s_steamCurrentLobbyOwner;
}

int steamCheckHostLeft(void)
{
	if (!s_steamHostLeft) {
		return 0;
	}
	s_steamHostLeft = 0;
	return 1;
}

void steamRequestLocalIdentity(void)
{
	STEAMSHIM_getLocalIdentity();
}

uint64_t steamLocalSteamID(void)
{
	return s_localSteamID;
}

const char *steamLocalPersonaName(void)
{
	return s_localPersonaName;
}

void steamRequestFriendName(uint64_t steamID)
{
	STEAMSHIM_getFriendName(steamID);
}

const char *steamGetCachedFriendName(uint64_t steamID)
{
	int i;

	for (i = 0; i < s_friendNameCacheCount; i++) {
		if (s_friendNameCache[i].steamID == steamID) {
			return s_friendNameCache[i].name;
		}
	}
	return "";
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

int steamLobbyListCount(void)
{
	return 0;
}

const steamLobbyInfo_t *steamLobbyListGet(int index)
{
	(void)index;
	return NULL;
}

int steamLobbyListDirty(void)
{
	return 0;
}

void steamLobbyListClearDirty(void)
{
}

uint64_t steamLobbyOwner(void)
{
	return 0;
}

int steamCheckHostLeft(void)
{
	return 0;
}

void steamRequestLocalIdentity(void)
{
}

uint64_t steamLocalSteamID(void)
{
	return 0;
}

const char *steamLocalPersonaName(void)
{
	return "";
}

void steamRequestFriendName(uint64_t steamID)
{
	(void)steamID;
}

const char *steamGetCachedFriendName(uint64_t steamID)
{
	(void)steamID;
	return "";
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

void steamNetListen(void)
{
}

void steamNetConnect(uint64_t steamID)
{
	(void)steamID;
}

int steamNetSend(uint64_t steamID, const void *data, int len)
{
	(void)steamID;
	(void)data;
	(void)len;
	return 0;
}

void steamNetClose(uint64_t steamID)
{
	(void)steamID;
}

int steamNetPollPacket(uint64_t *outSteamID, void *buf, int maxLen)
{
	(void)outSteamID;
	(void)buf;
	(void)maxLen;
	return 0;
}

int steamNetPollConnEvent(uint64_t *outSteamID, int *outConnected)
{
	(void)outSteamID;
	(void)outConnected;
	return 0;
}

#endif