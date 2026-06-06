
#include <stdint.h>

int steamInit(void);

void steamRun(void);

void steamSetAchievement(const char*);

void steamResetStats(const int bAlsoAchievements);

int steamAlive(void);

void steamSetRichPresence(const char* key, const char* value);

void steamLobbyCreate(int maxPlayers);
void steamLobbyList(void);
void steamLobbyJoin(uint64_t lobbyID);
void steamLobbyLeave(void);
void steamLobbySetData(const char *key, const char *value);
void steamLobbyInvite(void);
uint64_t steamLobbyCurrent(void);