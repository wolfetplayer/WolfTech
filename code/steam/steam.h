
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
Pre-game lobby: member roster + started flag

Nobody connects to a game server at all until the host clicks "Start The
Game" - until then, players sit in the front-end "pregame" UI, which is fed
by these two things instead of any in-game player list.
===============
*/

/* Number of players currently in the lobby (host included), from the last roster push. */
int steamLobbyMemberCount(void);

/* SteamID/persona name of the member at index, or 0/"" if out of range. */
uint64_t steamLobbyMemberSteamID(int index);
const char *steamLobbyMemberName(int index);

/* Persona name of the lobby's owner ("Lobby Leader"), resolved from the roster
   above; "" if not (yet) known. Works whether we're the owner or a guest. */
const char *steamLobbyOwnerName(void);

/* True if the roster changed (join/leave) since the last steamLobbyMembersClearDirty(). */
int steamLobbyMembersDirty(void);
void steamLobbyMembersClearDirty(void);

/* True once the host has set the lobby's "started" flag (see steamLobbySetData),
   i.e. it's time for guests to auto-connect and load the map alongside the host. */
int steamLobbyStarted(void);

/* The lobby's currently chosen map/gametype (mirrored from lobby data), for the lobby
   info panel - works for guests too, without needing their own selection to be set.
   steamLobbyGameType() returns -1 if not known yet. */
const char *steamLobbyMapName(void);
int steamLobbyGameType(void);

/* Sends a text message to every member of the current lobby (ourselves included -
   Steam echoes it back the same as everyone else's, so the log stays in one place). */
void steamLobbySendChatMsg(const char *text);

/* Number of received lobby chat lines cached (oldest first). */
int steamLobbyChatCount(void);

/* Sender's display name / message text for the line at index, or "" if out of range. */
const char *steamLobbyChatSenderName(int index);
const char *steamLobbyChatText(int index);

/* True if a new chat line arrived since the last steamLobbyChatClearDirty(). */
int steamLobbyChatDirty(void);
void steamLobbyChatClearDirty(void);

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