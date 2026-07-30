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

	SHIMEVENT_LOCAL_IDENTITY,	/* uvalue = local user's steamID, name = local persona name */
	SHIMEVENT_FRIEND_NAME,	/* uvalue = the steamID that was asked about, name = their persona name */

	/* Steam P2P net transport (per-frame game traffic). */
	SHIMEVENT_NET_CONNECTED,
	SHIMEVENT_NET_DISCONNECTED,
	SHIMEVENT_NET_DATA,

	/* Pre-game lobby roster: one per current lobby member (uvalue = their steamID,
	   name = persona name), pushed unprompted whenever membership changes, terminated
	   by a uvalue==0 "DONE" entry - same convention as SHIMEVENT_LOBBY_LIST. Appended
	   here (rather than grouped with the other LOBBY_* events above) so existing event
	   values don't shift. */
	SHIMEVENT_LOBBY_MEMBER,

	/* Pre-game lobby text chat: uvalue = sender's steamID, name = message text.
	   Pushed unprompted whenever another lobby member's message arrives. */
	SHIMEVENT_LOBBY_CHATMSG,

	SHIMEVENT_FRIEND_AVATAR,	/* uvalue = the steamID asked about, avatarRGBA/avatarLen = raw 32x32 RGBA pixels (avatarLen==0 if !okay) */
} STEAMSHIM_EventType;

/* must match qcommon's MAX_PACKETLEN (net_chan.c). Raise both together. */
#define STEAMSHIM_MAX_NET_PACKET 2048

/* Steam's "small" friend avatar is a fixed 32x32 RGBA image. */
#define STEAMSHIM_AVATAR_DIM 32
#define STEAMSHIM_AVATAR_RGBA_SIZE (STEAMSHIM_AVATAR_DIM * STEAMSHIM_AVATAR_DIM * 4)

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

	unsigned char avatarRGBA[STEAMSHIM_AVATAR_RGBA_SIZE];	/* raw payload for SHIMEVENT_FRIEND_AVATAR */
	int avatarLen;
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

void STEAMSHIM_lobbyCreate(int maxPlayers, int lobbyType);
void STEAMSHIM_lobbyList(void);
void STEAMSHIM_lobbyJoin(uint64_t lobbyID);
void STEAMSHIM_lobbyLeave(void);
void STEAMSHIM_lobbySetData(const char *key, const char *value);
void STEAMSHIM_lobbyInvite(uint64_t lobbyID);

/* Pre-game lobby text chat: sends a message to every member of the current lobby
   (including ourselves, echoed back via SHIMEVENT_LOBBY_CHATMSG same as everyone else). */
void STEAMSHIM_lobbySendChat(const char *text);

/* Local user's own steamID + persona name; answered synchronously (both are already known by the time initSteamworks() returns). */
void STEAMSHIM_getLocalIdentity(void);
/* Another player's persona name by steamID; answered from Steam's cache, which is already populated for anyone in our current lobby. */
void STEAMSHIM_getFriendName(uint64_t steamID);
/* Another player's small (32x32) avatar by steamID; answered via SHIMEVENT_FRIEND_AVATAR. */
void STEAMSHIM_getFriendAvatar(uint64_t steamID);

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

