#ifndef _INCL_STEAMSHIM_CHILD_H_
#define _INCL_STEAMSHIM_CHILD_H_

#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum STEAMSHIM_EventType
{
	SHIMEVENT_BYE,
	SHIMEVENT_STATSRECEIVED,
	SHIMEVENT_STATSSTORED,
	SHIMEVENT_SETACHIEVEMENT,
	SHIMEVENT_GETACHIEVEMENT,
	SHIMEVENT_RESETSTATS,
	SHIMEVENT_SETSTATI,
	SHIMEVENT_GETSTATI,
	SHIMEVENT_SETSTATF,
	SHIMEVENT_GETSTATF,
	SHIMEVENT_APPRESTARTED,
	SHIMEVENT_SETRICHPRESENCE,

	SHIMEVENT_LOBBY_CREATED,
	SHIMEVENT_LOBBY_LIST,
	SHIMEVENT_LOBBY_JOINED,
	SHIMEVENT_LOBBY_DATA,
	SHIMEVENT_LOBBY_CHAT,
	SHIMEVENT_LOBBY_INVITE,
	SHIMEVENT_LOBBY_OWNER,	/* uvalue = owning steamID of the current lobby */
	SHIMEVENT_LOBBY_HOSTLEFT,	/* uvalue = steamID of the host that just left/disconnected */

	/* Steam P2P net transport (per-frame game traffic). */
	SHIMEVENT_NET_CONNECTED,
	SHIMEVENT_NET_DISCONNECTED,
	SHIMEVENT_NET_DATA,
} STEAMSHIM_EventType;

/* must match qcommon's MAX_PACKETLEN (net_chan.c). Raise both together. */
#define STEAMSHIM_MAX_NET_PACKET 2048

    /* not all of these fields make sense in a given event. */
typedef struct STEAMSHIM_Event
{
	STEAMSHIM_EventType type;
	int okay;
	int ivalue;
	float fvalue;
	uint64_t uvalue;	/* also holds the peer steamID for NET_* events */
	unsigned long long epochsecs;
	char name[256];

	unsigned char data[STEAMSHIM_MAX_NET_PACKET];	/* raw payload for SHIMEVENT_NET_DATA */
	int datalen;
} STEAMSHIM_Event;

int STEAMSHIM_init(void);
void STEAMSHIM_deinit(void);
int STEAMSHIM_alive(void);
const STEAMSHIM_Event *STEAMSHIM_pump(void);

void STEAMSHIM_requestStats(void);
void STEAMSHIM_storeStats(void);
void STEAMSHIM_setAchievement(const char *name, const int enable);
void STEAMSHIM_getAchievement(const char *name);
void STEAMSHIM_resetStats(const int bAlsoAchievements);
void STEAMSHIM_setStatI(const char *name, const int val);
void STEAMSHIM_getStatI(const char *name);
void STEAMSHIM_setStatF(const char *name, const float val);
void STEAMSHIM_getStatF(const char *name);
void STEAMSHIM_restartIfNecessary(unsigned int unOwnAppID);
void STEAMSHIM_setRichPresence(const char *key, const char *value);

void STEAMSHIM_lobbyCreate(int maxPlayers);
void STEAMSHIM_lobbyList(void);
void STEAMSHIM_lobbyJoin(uint64_t lobbyID);
void STEAMSHIM_lobbyLeave(void);
void STEAMSHIM_lobbySetData(const char *key, const char *value);
void STEAMSHIM_lobbyInvite(uint64_t lobbyID);

/* Steam P2P net transport, used by NA_STEAM_P2P in qcommon/net_ip.c.
   The host can have multiple simultaneous peers, so send/close are
   addressed by steamID; passing 0 to STEAMSHIM_netClose closes all of
   them (e.g. on shutdown). */
void STEAMSHIM_netListen(void);
void STEAMSHIM_netConnect(uint64_t steamID);
int STEAMSHIM_netSend(uint64_t steamID, const void *data, int len);
void STEAMSHIM_netClose(uint64_t steamID);

#ifdef __cplusplus
}
#endif

#endif  /* include-once blocker */

/* end of steamshim_child.h ... */

