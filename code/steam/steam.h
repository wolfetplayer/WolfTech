
#include <stdint.h>

int steamInit(void);

void steamRun(void);

void steamSetAchievement(const char*);

void steamResetStats(const int bAlsoAchievements);

int steamAlive(void);

void steamSetRichPresence(const char* key, const char* value);

/*
===============
Steam lobbies
===============
*/
void steamLobbyCreate(int maxPlayers);
void steamLobbyList(void);
void steamLobbyJoin(uint64_t lobbyID);
void steamLobbyLeave(void);
void steamLobbySetData(const char *key, const char *value);

/*
Invite friends to current lobby.
For now this does not need lobbyID on game side.
*/
void steamLobbyInvite(void);

/*
Returns latest lobby id known by game wrapper.
This is updated when lobby create/join event arrives.
*/
uint64_t steamLobbyCurrent(void);

// Steam lobby list (server browser)
typedef struct {
	uint64_t lobbyID;
	char name[128];
	char map[64];
	int members;
	int maxMembers;
	int gameType;
} steamLobbyInfo_t;

/* Number of lobbies currently cached from the last steamLobbyList() request. */
int steamLobbyListCount(void);

/* Returns the cached lobby at index, or NULL if out of range. */
const steamLobbyInfo_t *steamLobbyListGet(int index);

/* True if the cache changed (new results or list finished) since the last steamLobbyListClearDirty(). */
int steamLobbyListDirty(void);

/* Acknowledge the current cache contents; clears the dirty flag. */
void steamLobbyListClearDirty(void);

/*
Returns the steamID of the current lobby's owner, or 0 if unknown.
The shim automatically starts listening (if this is our own steamID) or
connecting out via Steam P2P (otherwise) as soon as this is known, so
game code doesn't need to call steamNetListen()/steamNetConnect() itself
for the common join-a-lobby case.
*/
uint64_t steamLobbyOwner(void);

/* Polls whether the current lobby's host has left/disconnected; returns 1 once (clearing the flag), else 0. */
int steamCheckHostLeft(void);

/*
===============
Player identity (pre-game lobby: SteamID + persona name for player slots)
===============
*/

/* Fire-and-forget; call once at startup. Answered (synchronously, from the shim's
   point of view) via SHIMEVENT_LOCAL_IDENTITY, then available via the two getters below. */
void steamRequestLocalIdentity(void);
uint64_t steamLocalSteamID(void);
const char *steamLocalPersonaName(void);

/* Requests another player's persona name by steamID; poll steamGetCachedFriendName()
   afterwards. Resolves from Steam's local cache, already populated for lobby members. */
void steamRequestFriendName(uint64_t steamID);
/* Returns the cached name for steamID, or "" if steamRequestFriendName() hasn't resolved yet. */
const char *steamGetCachedFriendName(uint64_t steamID);

/*
===============
Steam P2P net transport (used by NA_STEAM_P2P in qcommon/net_ip.c)
===============
*/

/* Host: start accepting P2P connections. */
void steamNetListen(void);

/* Client: connect out to a host's steamID (e.g. the lobby owner). */
void steamNetConnect(uint64_t steamID);

/* Send a packet to a connected peer. Returns 1 if queued, 0 otherwise
   (peer not connected, packet too big, or shim not alive). */
int steamNetSend(uint64_t steamID, const void *data, int len);

/* Close a specific peer connection, or 0 to close all of them. */
void steamNetClose(uint64_t steamID);

/*
Pulls one queued inbound packet, if any.
outSteamID receives the sender, buf receives up to maxLen bytes, and the
return value is the packet length, or 0 if no packet is pending.
*/
int steamNetPollPacket(uint64_t *outSteamID, void *buf, int maxLen);

/*
Pulls one queued connect/disconnect event, if any.
outSteamID receives the peer, outConnected is set to 1 for a new
connection or 0 for a disconnect. Returns 1 if an event was pulled.
*/
int steamNetPollConnEvent(uint64_t *outSteamID, int *outConnected);