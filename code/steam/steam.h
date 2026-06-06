
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